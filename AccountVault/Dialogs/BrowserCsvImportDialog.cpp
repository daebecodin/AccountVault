#include "pch.h"
#include "../MainWindow.xaml.h"
#include "../Security/SensitiveData.h"
#include "../Services/BrowserCsvImportService.h"

#include <microsoft.ui.xaml.window.h>
#include <shobjidl.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.FileProperties.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.Streams.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Windows::Storage::Streams;

namespace
{
    struct BrowserCredentialWiper
    {
        std::vector<account_vault::services::BrowserCsvCredential>& values;

        ~BrowserCredentialWiper()
        {
            account_vault::services::BrowserCsvImportService::wipe(values);
        }
    };

    [[nodiscard]] bool looksLikeEmail(std::wstring_view value) noexcept
    {
        const std::size_t at{ value.find(L'@') };
        return at != std::wstring_view::npos && at != 0 &&
            at + 1 < value.size();
    }
}

namespace winrt::AccountVault::implementation
{
    void MainWindow::ImportBrowserCsvButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_workspaceSection != WorkspaceSection::CredentialVault)
        {
            StatusText().Text(
                L"Browser CSV import is available in the Credential Vault");
            return;
        }
        showBrowserCsvImport();
    }

    fire_and_forget MainWindow::showBrowserCsvImport()
    {
        account_vault::ui::ModelessToolWindow dialog{ nullptr };
        bool dialogAttached{};

        try
        {
            auto lifetime{ get_strong() };

            if (m_browserCsvImportWindow)
            {
                m_browserCsvImportWindow.Activate();
                co_return;
            }
            if (!m_storageReady || m_isLocked)
            {
                StatusText().Text(
                    L"Unlock Account Armory and verify storage before importing browser data");
                co_return;
            }
            if (m_workspaceSection != WorkspaceSection::CredentialVault)
            {
                StatusText().Text(
                    L"Open the Credential Vault to import browser logins");
                co_return;
            }
            const bool hasCredentialRecord{ std::ranges::any_of(
                m_repository.accounts(),
                [](Account const& account)
                {
                    return account.kind == AccountKind::Credential;
                }) };
            if (!hasCredentialRecord)
            {
                StatusText().Text(
                    L"Add a Credential Vault record before mapping a browser login");
                co_return;
            }

            noteUserActivity();
            const std::uint64_t operationGeneration{ m_lockGeneration };

            FileOpenPicker picker;
            picker.SuggestedStartLocation(PickerLocationId::DocumentsLibrary);
            picker.FileTypeFilter().Append(L".csv");

            HWND windowHandle{};
            check_hresult(
                m_inner.as<::IWindowNative>()->get_WindowHandle(
                    &windowHandle));
            const auto initializeWithWindow{
                picker.as<::IInitializeWithWindow>() };
            check_hresult(initializeWithWindow->Initialize(windowHandle));

            const StorageFile file{ co_await picker.PickSingleFileAsync() };
            if (!file || m_isLocked ||
                m_lockGeneration != operationGeneration)
            {
                if (!m_isLocked)
                {
                    StatusText().Text(L"Browser CSV import canceled");
                }
                co_return;
            }

            const auto properties{ co_await file.GetBasicPropertiesAsync() };
            if (properties.Size() == 0 ||
                properties.Size() > account_vault::services::
                    BrowserCsvImportService::MaximumCsvBytes)
            {
                StatusText().Text(L"The browser CSV file size is invalid");
                co_return;
            }

            const hstring fileText{ co_await FileIO::ReadTextAsync(
                file,
                UnicodeEncoding::Utf8) };
            if (m_isLocked || m_lockGeneration != operationGeneration)
            {
                co_return;
            }

            std::wstring csvText{ fileText.c_str(), fileText.size() };
            auto wipeCsvText{ account_vault::security::wipeOnExit(csvText) };
            auto parsed{
                account_vault::services::BrowserCsvImportService::parse(
                    csvText) };
            BrowserCredentialWiper wipeCredentials{ parsed.credentials };
            account_vault::security::wipe(csvText);

            if (!parsed.succeeded())
            {
                StatusText().Text(hstring{ parsed.error });
                co_return;
            }

            dialog = account_vault::ui::ModelessToolWindow{
                L"Import browser CSV",
                760,
                610 };
            dialog.XamlRoot(Content().XamlRoot());
            dialog.Title(box_value(L"Map browser login to credential"));
            dialog.PrimaryButtonText(L"Import to credential");
            dialog.CloseButtonText(L"Cancel");
            dialog.DefaultButton(ContentDialogButton::Primary);
            dialog.MaxWidth(760);

            const auto resources{ Application::Current().Resources() };
            const auto mutedBrush{ resources
                .Lookup(box_value(L"AppMutedTextBrush")).as<Brush>() };
            const auto accentBrush{ resources
                .Lookup(box_value(L"AppAccentBrush")).as<Brush>() };

            StackPanel content;
            content.Spacing(16);

            TextBlock introduction;
            introduction.Text(
                L"Choose one browser login and map it to an existing Credential Vault record.");
            introduction.TextWrapping(TextWrapping::Wrap);
            introduction.Foreground(mutedBrush);

            ComboBox loginPicker;
            loginPicker.Header(box_value(L"Browser login"));
            loginPicker.HorizontalAlignment(HorizontalAlignment::Stretch);
            for (std::size_t index{}; index < parsed.credentials.size(); ++index)
            {
                const auto& credential{ parsed.credentials[index] };
                std::wstring label{
                    credential.name.empty() ? credential.url : credential.name };
                label += L"  —  ";
                label += credential.username;

                ComboBoxItem item;
                item.Content(box_value(hstring{ label }));
                item.Tag(box_value(static_cast<std::uint32_t>(index)));
                loginPicker.Items().Append(item);
                account_vault::security::wipe(label);
            }
            loginPicker.SelectedIndex(0);

            TextBlock loginDetails;
            loginDetails.TextWrapping(TextWrapping::Wrap);
            loginDetails.Foreground(mutedBrush);

            const auto updateLoginDetails = [&]()
            {
                const int index{ loginPicker.SelectedIndex() };
                if (index < 0 ||
                    static_cast<std::size_t>(index) >= parsed.credentials.size())
                {
                    loginDetails.Text(L"");
                    return;
                }

                const auto& credential{ parsed.credentials[index] };
                std::wstring details{ credential.url };
                details += L"\nUsername: ";
                details += credential.username;
                loginDetails.Text(hstring{ details });
                account_vault::security::wipe(details);
            };
            loginPicker.SelectionChanged(
                [&](IInspectable const&, SelectionChangedEventArgs const&)
                {
                    updateLoginDetails();
                });
            updateLoginDetails();

            ComboBox accountPicker;
            accountPicker.Header(box_value(L"Credential record"));
            accountPicker.PlaceholderText(L"Choose a Credential Vault record");
            accountPicker.HorizontalAlignment(HorizontalAlignment::Stretch);
            for (auto const& account : m_repository.accounts())
            {
                if (account.kind != AccountKind::Credential)
                {
                    continue;
                }

                std::wstring label{ account.serviceName.empty()
                    ? account.category
                    : account.serviceName };
                label += L"  —  ";
                label += account.username.empty()
                    ? account.emailAddress
                    : account.username;

                ComboBoxItem item;
                item.Content(box_value(hstring{ label }));
                item.Tag(box_value(account.recordId));
                accountPicker.Items().Append(item);
            }
            ComboBox destinationPicker;
            destinationPicker.Header(box_value(L"Credential slot"));
            destinationPicker.PlaceholderText(L"Choose a credential slot");
            destinationPicker.HorizontalAlignment(HorizontalAlignment::Stretch);

            const auto updateDestinations = [&]()
            {
                destinationPicker.Items().Clear();
                if (accountPicker.SelectedIndex() < 0)
                {
                    return;
                }

                const auto selectedAccountItem{
                    accountPicker.SelectedItem().as<ComboBoxItem>() };
                const RecordId id{
                    unbox_value<RecordId>(selectedAccountItem.Tag()) };
                const Account* account{ m_repository.find(id) };
                if (!account || account->kind != AccountKind::Credential)
                {
                    return;
                }

                ComboBoxItem primary;
                ComboBoxItem secondary;
                primary.Content(box_value(L"Primary sign-in"));
                secondary.Content(box_value(L"Recovery email sign-in"));
                primary.Tag(box_value(0));
                secondary.Tag(box_value(1));
                destinationPicker.Items().Append(primary);
                destinationPicker.Items().Append(secondary);
            };
            accountPicker.SelectionChanged(
                [&](IInspectable const&, SelectionChangedEventArgs const&)
                {
                    updateDestinations();
                });
            updateDestinations();

            TextBlock validation;
            validation.Text(L"Select a browser login, Credential Vault record, and credential slot.");
            validation.Foreground(accentBrush);
            validation.Visibility(Visibility::Collapsed);

            TextBlock warning;
            warning.Text(
                L"Importing replaces the selected slot's saved sign-in details. Browser CSV exports contain plaintext passwords, so delete the CSV after you finish importing it.");
            warning.TextWrapping(TextWrapping::Wrap);
            warning.Foreground(accentBrush);

            content.Children().Append(introduction);
            if (parsed.skippedRows != 0)
            {
                std::wstring skipped{ std::to_wstring(parsed.skippedRows) };
                skipped += L" incomplete CSV row(s) were skipped.";
                TextBlock skippedNotice;
                skippedNotice.Text(hstring{ skipped });
                skippedNotice.Foreground(mutedBrush);
                content.Children().Append(skippedNotice);
            }

            content.Children().Append(loginPicker);
            content.Children().Append(loginDetails);
            content.Children().Append(accountPicker);
            content.Children().Append(destinationPicker);
            content.Children().Append(validation);
            content.Children().Append(warning);
            dialog.Content(content);

            dialog.PrimaryButtonClick(
                [&](account_vault::ui::ModelessToolWindow const&,
                    account_vault::ui::ModelessButtonClickEventArgs const& args)
                {
                    const bool valid{
                        loginPicker.SelectedIndex() >= 0 &&
                        accountPicker.SelectedIndex() >= 0 &&
                        destinationPicker.SelectedIndex() >= 0 };
                    validation.Visibility(
                        valid ? Visibility::Collapsed : Visibility::Visible);
                    args.Cancel(!valid);
                });

            attachDialogToShell(dialog);
            dialogAttached = true;
            m_browserCsvImportWindow = dialog;

            const ContentDialogResult result{
                co_await dialog.ShowAsync(ContentDialogPlacement::InPlace) };

            detachModelessWindow(dialog);
            dialogAttached = false;
            m_browserCsvImportWindow = nullptr;

            if (result != ContentDialogResult::Primary || m_isLocked ||
                m_lockGeneration != operationGeneration)
            {
                co_return;
            }

            const std::size_t loginIndex{
                static_cast<std::size_t>(loginPicker.SelectedIndex()) };
            const auto accountItem{
                accountPicker.SelectedItem().as<ComboBoxItem>() };
            const RecordId accountId{
                unbox_value<RecordId>(accountItem.Tag()) };
            const int destination{ destinationPicker.SelectedIndex() };
            const Account* account{ m_repository.find(accountId) };
            if (!account || account->kind != AccountKind::Credential ||
                loginIndex >= parsed.credentials.size())
            {
                StatusText().Text(
                    L"The selected Credential Vault record is no longer available");
                co_return;
            }

            const Account current{ *account };
            const auto& imported{ parsed.credentials[loginIndex] };
            bool updated{};
            if (destination == 0)
            {
                updated = updateCredential(
                    accountId,
                    imported.name.empty()
                        ? current.serviceName
                        : imported.name,
                    current.category,
                    imported.username,
                    looksLikeEmail(imported.username)
                        ? imported.username
                        : current.emailAddress,
                    std::optional<std::wstring>{ imported.password },
                    imported.url,
                    current.recoveryEmail,
                    std::nullopt,
                    current.notes);
            }
            else
            {
                updated = updateCredential(
                    accountId,
                    current.serviceName,
                    current.category,
                    current.username,
                    current.emailAddress,
                    std::nullopt,
                    current.website,
                    imported.username,
                    std::optional<std::wstring>{ imported.password },
                    current.notes);
            }

            if (!updated)
            {
                StatusText().Text(
                    L"The browser login could not be protected and saved");
                co_return;
            }

            refreshAccountCard(accountId);
            std::wstring status{ L"Browser login imported into " };
            status += unbox_value<hstring>(accountItem.Content()).c_str();
            status += L". Delete the plaintext CSV when finished.";
            StatusText().Text(hstring{ status });
        }
        catch (...)
        {
            try
            {
                if (dialogAttached && dialog)
                {
                    dialog.Hide();
                    detachModelessWindow(dialog);
                }
                m_browserCsvImportWindow = nullptr;
            }
            catch (...)
            {
            }

            try
            {
                StatusText().Text(
                    L"The browser CSV import encountered an unexpected error");
            }
            catch (...)
            {
            }
        }
    }
}
