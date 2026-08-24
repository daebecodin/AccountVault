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
#include <winrt/Windows.UI.Text.h>

#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <limits>
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
    using BrowserCredential =
        account_vault::services::BrowserCsvCredential;
    using Account = account_vault::models::Account;
    using AccountKind = account_vault::models::AccountKind;

    constexpr std::wstring_view DefaultImportCategory{ L"Browser Import" };
    constexpr std::size_t MaximumVaultRecordCount{ 100000 };

    struct BrowserCredentialWiper
    {
        std::vector<BrowserCredential>& values;

        ~BrowserCredentialWiper()
        {
            account_vault::services::BrowserCsvImportService::wipe(values);
        }
    };

    [[nodiscard]] std::wstring trimCopy(std::wstring_view value)
    {
        while (!value.empty() && std::iswspace(value.front()))
        {
            value.remove_prefix(1);
        }
        while (!value.empty() && std::iswspace(value.back()))
        {
            value.remove_suffix(1);
        }
        return std::wstring{ value };
    }

    [[nodiscard]] std::wstring lowerCopy(std::wstring_view value)
    {
        std::wstring lowered{ value };
        std::ranges::transform(
            lowered,
            lowered.begin(),
            [](wchar_t character)
            {
                return static_cast<wchar_t>(std::towlower(character));
            });
        return lowered;
    }

    [[nodiscard]] std::wstring normalizedUrl(std::wstring_view value)
    {
        std::wstring normalized{ lowerCopy(trimCopy(value)) };
        while (normalized.size() > 1 && normalized.back() == L'/')
        {
            normalized.pop_back();
        }
        return normalized;
    }

    [[nodiscard]] bool looksLikeEmail(std::wstring_view value) noexcept
    {
        const std::size_t at{ value.find(L'@') };
        return at != std::wstring_view::npos && at != 0 &&
            at + 1 < value.size();
    }

    [[nodiscard]] std::wstring serviceNameFor(
        BrowserCredential const& credential)
    {
        if (!credential.name.empty())
        {
            return credential.name;
        }

        std::wstring_view remainder{ credential.url };
        const std::size_t schemeEnd{ remainder.find(L"://") };
        if (schemeEnd != std::wstring_view::npos)
        {
            remainder.remove_prefix(schemeEnd + 3);
        }

        const std::size_t pathStart{ remainder.find_first_of(L"/?#") };
        std::wstring_view host{ remainder.substr(0, pathStart) };
        const std::size_t userInfoEnd{ host.rfind(L'@') };
        if (userInfoEnd != std::wstring_view::npos)
        {
            host.remove_prefix(userInfoEnd + 1);
        }
        const std::size_t portStart{ host.rfind(L':') };
        if (portStart != std::wstring_view::npos &&
            host.find(L':') == portStart)
        {
            host = host.substr(0, portStart);
        }
        if (host.starts_with(L"www."))
        {
            host.remove_prefix(4);
        }

        std::wstring serviceName{ trimCopy(host) };
        if (serviceName.empty())
        {
            serviceName = L"Imported login";
        }
        return serviceName;
    }

    [[nodiscard]] bool matchesBrowserCredential(
        Account const& account,
        BrowserCredential const& credential)
    {
        if (account.kind != AccountKind::Credential ||
            normalizedUrl(account.website) != normalizedUrl(credential.url))
        {
            return false;
        }

        const std::wstring importedUser{ lowerCopy(credential.username) };
        if (importedUser.empty())
        {
            return account.username.empty() && account.emailAddress.empty();
        }
        return lowerCopy(account.username) == importedUser ||
            (!account.emailAddress.empty() &&
                lowerCopy(account.emailAddress) == importedUser);
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
        bool interactionDisabled{};

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

            hstring fileText{ co_await FileIO::ReadTextAsync(
                file,
                UnicodeEncoding::Utf8) };
            if (m_isLocked || m_lockGeneration != operationGeneration)
            {
                fileText = hstring{};
                co_return;
            }

            std::wstring csvText{ fileText.c_str(), fileText.size() };
            fileText = hstring{};
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
                L"Import browser passwords",
                700,
                470 };
            dialog.XamlRoot(Content().XamlRoot());
            dialog.Title(box_value(L"Import browser passwords"));
            dialog.PrimaryButtonText(L"Import all logins");
            dialog.CloseButtonText(L"Cancel");
            dialog.DefaultButton(ContentDialogButton::Primary);
            dialog.MaxWidth(700);

            const auto resources{ Application::Current().Resources() };
            const auto mutedBrush{ resources
                .Lookup(box_value(L"AppMutedTextBrush")).as<Brush>() };
            const auto accentBrush{ resources
                .Lookup(box_value(L"AppAccentBrush")).as<Brush>() };

            StackPanel content;
            content.Spacing(16);

            std::wstring summary{ L"Loaded " };
            summary += std::to_wstring(parsed.credentials.size());
            summary += parsed.credentials.size() == 1
                ? L" complete browser login."
                : L" complete browser logins.";
            if (parsed.skippedRows != 0)
            {
                summary += L" ";
                summary += std::to_wstring(parsed.skippedRows);
                summary += L" incomplete row(s) will be skipped.";
            }

            TextBlock introduction;
            introduction.Text(hstring{ summary });
            introduction.TextWrapping(TextWrapping::Wrap);

            TextBlock formatSupport;
            formatSupport.Text(
                L"Chrome, Edge, Firefox, Safari, and compatible browser CSV formats are detected automatically.");
            formatSupport.TextWrapping(TextWrapping::Wrap);
            formatSupport.Foreground(mutedBrush);

            TextBlock mappingHeading;
            mappingHeading.Text(L"CARD MAPPING");
            mappingHeading.FontFamily(FontFamily{ L"Cascadia Mono" });
            mappingHeading.FontWeight(
                Windows::UI::Text::FontWeights::SemiBold());
            mappingHeading.Foreground(accentBrush);

            TextBlock mapping;
            mapping.Text(
                L"URL  →  Website and card name\n"
                L"Username  →  Username and email when present\n"
                L"Password  →  Protected primary password\n"
                L"Matching URL + username  →  Update existing card");
            mapping.TextWrapping(TextWrapping::Wrap);
            mapping.Foreground(mutedBrush);

            TextBox category;
            category.Header(box_value(L"Category for new cards"));
            category.Text(hstring{ DefaultImportCategory });
            category.PlaceholderText(L"Browser Import");

            TextBlock validation;
            validation.Text(L"Enter a category for newly created cards.");
            validation.Foreground(accentBrush);
            validation.Visibility(Visibility::Collapsed);

            TextBlock warning;
            warning.Text(
                L"This imports every complete row. Browser CSV exports contain plaintext passwords, so delete the CSV after confirming the cards were saved.");
            warning.TextWrapping(TextWrapping::Wrap);
            warning.Foreground(accentBrush);

            content.Children().Append(introduction);
            content.Children().Append(formatSupport);
            content.Children().Append(mappingHeading);
            content.Children().Append(mapping);
            content.Children().Append(category);
            content.Children().Append(validation);
            content.Children().Append(warning);
            dialog.Content(content);

            dialog.PrimaryButtonClick(
                [&](account_vault::ui::ModelessToolWindow const&,
                    account_vault::ui::ModelessButtonClickEventArgs const& args)
                {
                    const bool valid{ !trimCopy(category.Text().c_str()).empty() };
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

            const std::wstring importCategory{
                trimCopy(category.Text().c_str()) };
            StatusText().Text(L"Protecting browser passwords for import...");
            setLockedInteractionState(true);
            setModelessWindowsInteraction(false);
            interactionDisabled = true;

            apartment_context uiThread;
            std::vector<std::wstring> protectedPasswords;
            std::wstring preparationError;
            protectedPasswords.reserve(parsed.credentials.size());

            co_await resume_background();
            try
            {
                account_vault::services::CredentialService credentials;
                for (auto const& credential : parsed.credentials)
                {
                    const auto protectedPassword{
                        credentials.protectPassword(credential.password) };
                    if (!protectedPassword)
                    {
                        preparationError =
                            L"A browser password could not be protected locally.";
                        protectedPasswords.clear();
                        break;
                    }
                    protectedPasswords.push_back(*protectedPassword);
                }
            }
            catch (...)
            {
                preparationError =
                    L"The browser passwords could not be prepared for import.";
                protectedPasswords.clear();
            }
            co_await uiThread;

            if (!preparationError.empty() || m_isLocked ||
                m_lockGeneration != operationGeneration)
            {
                setLockedInteractionState(m_isLocked);
                setModelessWindowsInteraction(!m_isLocked);
                interactionDisabled = false;
                if (!m_isLocked && !preparationError.empty())
                {
                    StatusText().Text(hstring{ preparationError });
                }
                co_return;
            }

            std::vector<Account> candidate{ m_repository.accounts() };
            RecordId nextId{ m_repository.nextId() };
            const RecordId maximumId{
                (std::numeric_limits<RecordId>::max)() };
            std::size_t createdCount{};
            std::size_t updatedCount{};

            for (std::size_t index{};
                 index < parsed.credentials.size();
                 ++index)
            {
                const auto& imported{ parsed.credentials[index] };
                const auto match{ std::ranges::find_if(
                    candidate,
                    [&](Account const& account)
                    {
                        return matchesBrowserCredential(account, imported);
                    }) };

                if (match != candidate.end())
                {
                    if (match->serviceName.empty())
                    {
                        match->serviceName = serviceNameFor(imported);
                    }
                    match->username = imported.username;
                    if (looksLikeEmail(imported.username))
                    {
                        match->emailAddress = imported.username;
                    }
                    match->website = imported.url;
                    match->protectedPassword = protectedPasswords[index];
                    ++updatedCount;
                    continue;
                }

                if (nextId == 0 || nextId == maximumId ||
                    candidate.size() >= MaximumVaultRecordCount)
                {
                    preparationError =
                        L"Importing this CSV would exceed the vault record limit.";
                    break;
                }

                candidate.push_back(Account{
                    .recordId = nextId++,
                    .kind = AccountKind::Credential,
                    .emailAddress = looksLikeEmail(imported.username)
                        ? imported.username
                        : std::wstring{},
                    .serviceName = serviceNameFor(imported),
                    .category = importCategory,
                    .username = imported.username,
                    .website = imported.url,
                    .notes = L"Imported from browser CSV",
                    .protectedPassword = protectedPasswords[index],
                });
                ++createdCount;
            }

            if (!preparationError.empty())
            {
                setLockedInteractionState(m_isLocked);
                setModelessWindowsInteraction(!m_isLocked);
                interactionDisabled = false;
                StatusText().Text(hstring{ preparationError });
                co_return;
            }

            std::wstring saveError;
            bool saved{};
            co_await resume_background();
            try
            {
                saved = m_accountStorage.save(candidate, nextId, saveError);
            }
            catch (...)
            {
                saved = false;
            }
            co_await uiThread;

            if (!saved)
            {
                setLockedInteractionState(m_isLocked);
                setModelessWindowsInteraction(!m_isLocked);
                interactionDisabled = false;
                if (!m_isLocked)
                {
                    StatusText().Text(
                        L"The browser CSV could not be saved; nothing changed");
                }
                co_return;
            }

            // The atomic disk save succeeded, so synchronize the in-memory
            // repository even if the inactivity timer locked the app while
            // the save was completing.
            m_repository.replaceAll(std::move(candidate), nextId);
            rebuildRecordFilter();
            refreshAccounts();
            setLockedInteractionState(m_isLocked);
            setModelessWindowsInteraction(!m_isLocked);
            interactionDisabled = false;

            if (!m_isLocked)
            {
                std::wstring status{ L"Browser CSV imported: " };
                status += std::to_wstring(createdCount);
                status += L" card(s) created, ";
                status += std::to_wstring(updatedCount);
                status += L" matching card(s) updated. Delete the plaintext CSV when finished.";
                StatusText().Text(hstring{ status });
                noteUserActivity();
            }
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
                if (interactionDisabled)
                {
                    setLockedInteractionState(m_isLocked);
                    setModelessWindowsInteraction(!m_isLocked);
                }
                StatusText().Text(
                    L"The browser CSV import encountered an unexpected error");
            }
            catch (...)
            {
            }
        }
    }
}
