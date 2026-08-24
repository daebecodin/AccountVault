#include "pch.h"
#include "../MainWindow.xaml.h"
#include "../Security/SensitiveData.h"

#include <microsoft.ui.xaml.window.h>
#include <shobjidl.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.FileProperties.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.Streams.h>

#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Windows::Storage::Streams;

namespace
{
    constexpr std::uint64_t MaximumBackupBytes{ 64U * 1024U * 1024U };
    constexpr std::size_t MaximumAccountCount{ 100000 };
    constexpr std::size_t MinimumNewPasswordCharacters{ 12 };
    constexpr std::size_t MaximumPasswordCharacters{ 256 };

    struct ScopeExit
    {
        std::function<void()> action;

        ~ScopeExit()
        {
            if (action)
            {
                action();
            }
        }
    };

    void secureWipe(std::wstring& value) noexcept
    {
        account_vault::security::wipe(value);
    }

    void secureWipe(
        account_vault::services::PortableAccount& account) noexcept
    {
        account_vault::security::wipe(account.kind);
        account_vault::security::wipe(account.launcher);
        account_vault::security::wipe(account.launcherUsername);
        account_vault::security::wipe(account.launcherPassword);
        account_vault::security::wipe(account.emailAddress);
        account_vault::security::wipe(account.emailProvider);
        account_vault::security::wipe(account.emailProviderWebsite);
        account_vault::security::wipe(account.emailPassword);
        account_vault::security::wipe(account.serviceName);
        account_vault::security::wipe(account.category);
        account_vault::security::wipe(account.username);
        account_vault::security::wipe(account.website);
        account_vault::security::wipe(account.recoveryEmail);
        account_vault::security::wipe(account.notes);
        account_vault::security::wipe(account.password);
        account_vault::security::wipe(account.recoveryEmailPassword);
    }

    void secureWipe(
        std::vector<account_vault::services::PortableAccount>& accounts)
        noexcept
    {
        for (auto& account : accounts)
        {
            secureWipe(account);
        }
        accounts.clear();
    }

}

namespace winrt::AccountVault::implementation
{
    void MainWindow::ImportOneAccountButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        showImportBackup(true);
    }

    void MainWindow::ImportAllAccountsButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        showImportBackup(false);
    }

    void MainWindow::ExportAllAccountsButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        showExportBackup(std::nullopt);
    }

    bool MainWindow::buildPortableAccounts(
        std::optional<RecordId> onlyRecord,
        std::vector<PortableAccount>& accounts,
        std::wstring& error) const
    {
        accounts.clear();

        const auto appendAccount = [this, &accounts, &error](
            Account const& account)
        {
            if (account.kind == AccountKind::Credential)
            {
                auto password{ m_credentials.unprotectPassword(
                    account.protectedPassword) };
                std::optional<std::wstring> recoveryPassword{
                    std::wstring{} };
                if (!account.protectedRecoveryEmailPassword.empty())
                {
                    recoveryPassword = m_credentials.unprotectPassword(
                        account.protectedRecoveryEmailPassword);
                }
                auto wipePassword{
                    account_vault::security::wipeOnExit(password) };
                auto wipeRecoveryPassword{
                    account_vault::security::wipeOnExit(recoveryPassword) };

                if (!password || !recoveryPassword)
                {
                    error = L"One or more credential passwords could not be decrypted.";
                    return false;
                }

                accounts.push_back(PortableAccount{
                    .kind = L"credential",
                    .emailAddress = account.emailAddress,
                    .serviceName = account.serviceName,
                    .category = account.category,
                    .username = account.username,
                    .website = account.website,
                    .recoveryEmail = account.recoveryEmail,
                    .notes = account.notes,
                    .password = std::move(*password),
                    .recoveryEmailPassword =
                        std::move(*recoveryPassword),
                });
                return true;
            }

            auto launcherPassword{
                account.protectedLauncherPassword.empty()
                    ? m_credentials.legacyLauncherPassword(account.recordId)
                    : m_credentials.unprotectPassword(
                        account.protectedLauncherPassword) };
            auto emailPassword{
                account.protectedEmailPassword.empty()
                    ? m_credentials.legacyEmailPassword(account.recordId)
                    : m_credentials.unprotectPassword(
                        account.protectedEmailPassword) };
            auto wipeLauncherPassword{
                account_vault::security::wipeOnExit(launcherPassword) };
            auto wipeEmailPassword{
                account_vault::security::wipeOnExit(emailPassword) };

            if (!launcherPassword || !emailPassword)
            {
                if (launcherPassword)
                {
                    secureWipe(*launcherPassword);
                }
                if (emailPassword)
                {
                    secureWipe(*emailPassword);
                }
                error = L"One or more account passwords could not be decrypted.";
                return false;
            }

            accounts.push_back(PortableAccount{
                .kind = L"launcher",
                .launcher = account.launcher,
                .launcherUsername = account.launcherUsername,
                .launcherPassword = std::move(*launcherPassword),
                .emailAddress = account.emailAddress,
                .emailProvider = account.emailProvider,
                .emailProviderWebsite = account.emailProviderWebsite,
                .emailPassword = std::move(*emailPassword),
            });
            return true;
        };

        try
        {
            if (onlyRecord)
            {
                const Account* account{ m_repository.find(*onlyRecord) };
                if (!account)
                {
                    error = L"That account no longer exists.";
                    return false;
                }
                if (!appendAccount(*account))
                {
                    secureWipe(accounts);
                    return false;
                }
            }
            else
            {
                if (m_repository.accounts().empty())
                {
                    error = L"There are no vault records to export.";
                    return false;
                }

                accounts.reserve(m_repository.accounts().size());
                for (auto const& account : m_repository.accounts())
                {
                    if (!appendAccount(account))
                    {
                        secureWipe(accounts);
                        return false;
                    }
                }
            }

            error.clear();
            return true;
        }
        catch (...)
        {
            secureWipe(accounts);
            error = L"The account data could not be prepared for export.";
            return false;
        }
    }

    IAsyncOperation<hstring> MainWindow::requestBackupPassword(
        bool confirmPassword)
    {
        ContentDialog dialog;
        bool appended{};
        PasswordBox password;
        PasswordBox confirmation;

        try
        {
            auto lifetime{ get_strong() };

            dialog.Title(box_value(confirmPassword
                ? L"Protect encrypted backup"
                : L"Unlock encrypted backup"));
            dialog.PrimaryButtonText(confirmPassword ? L"Continue" : L"Unlock");
            dialog.CloseButtonText(L"Cancel");
            dialog.DefaultButton(ContentDialogButton::Primary);

            StackPanel content;
            content.Spacing(10);

            TextBlock explanation;
            explanation.Text(confirmPassword
                ? L"Create a backup password with at least 12 characters. "
                  L"It cannot be recovered if forgotten."
                : L"Enter the password used when this backup was exported.");
            explanation.TextWrapping(TextWrapping::Wrap);
            explanation.Foreground(
                Application::Current()
                    .Resources()
                    .Lookup(box_value(L"AppMutedTextBrush"))
                    .as<Microsoft::UI::Xaml::Media::Brush>());
            content.Children().Append(explanation);

            password.Header(box_value(L"Backup password"));
            password.PasswordRevealMode(PasswordRevealMode::Peek);
            password.MaxLength(static_cast<int>(MaximumPasswordCharacters));
            content.Children().Append(password);

            if (confirmPassword)
            {
                confirmation.Header(box_value(L"Confirm backup password"));
                confirmation.PasswordRevealMode(PasswordRevealMode::Peek);
                confirmation.MaxLength(
                    static_cast<int>(MaximumPasswordCharacters));
                content.Children().Append(confirmation);
            }

            TextBlock validation;
            validation.Visibility(Visibility::Collapsed);
            validation.TextWrapping(TextWrapping::Wrap);
            Microsoft::UI::Xaml::Media::SolidColorBrush validationBrush;
            validationBrush.Color(color(248, 81, 73));
            validation.Foreground(validationBrush);
            content.Children().Append(validation);

            dialog.Content(content);
            dialog.PrimaryButtonClick(
                [password, confirmation, validation, confirmPassword](
                    ContentDialog const&,
                    ContentDialogButtonClickEventArgs const& args)
                {
                    std::wstring value{ password.Password().c_str() };
                    const std::size_t minimumLength{ confirmPassword
                        ? MinimumNewPasswordCharacters
                        : 1U };

                    if (value.size() < minimumLength ||
                        value.size() > MaximumPasswordCharacters)
                    {
                        validation.Text(confirmPassword
                            ? L"Use 12 to 256 characters."
                            : L"Enter the backup password.");
                        validation.Visibility(Visibility::Visible);
                        args.Cancel(true);
                        secureWipe(value);
                        return;
                    }

                    if (confirmPassword &&
                        password.Password() != confirmation.Password())
                    {
                        validation.Text(L"The passwords do not match.");
                        validation.Visibility(Visibility::Visible);
                        args.Cancel(true);
                    }
                    secureWipe(value);
                });

            attachDialogToShell(dialog);
            appended = true;

            const ContentDialogResult result{
                co_await dialog.ShowAsync(ContentDialogPlacement::InPlace) };

            std::wstring selectedPassword;
            auto wipeSelectedPassword{
                account_vault::security::wipeOnExit(selectedPassword) };
            if (result == ContentDialogResult::Primary)
            {
                selectedPassword = password.Password().c_str();
            }
            password.Password(L"");
            confirmation.Password(L"");
            detachDialogFromShell(dialog);
            appended = false;

            if (result != ContentDialogResult::Primary)
            {
                secureWipe(selectedPassword);
                co_return hstring{};
            }

            hstring returned{ selectedPassword };
            secureWipe(selectedPassword);
            co_return returned;
        }
        catch (...)
        {
            try
            {
                password.Password(L"");
                confirmation.Password(L"");
                if (appended)
                {
                    detachDialogFromShell(dialog);
                }
            }
            catch (...)
            {
            }
            co_return hstring{};
        }
    }

    fire_and_forget MainWindow::showExportBackup(
        std::optional<RecordId> onlyRecord)
    {
        try
        {
            auto lifetime{ get_strong() };
            if (m_backupOperationInProgress)
            {
                StatusText().Text(L"Another backup operation is already running");
                co_return;
            }
            if (!m_storageReady || m_isLocked)
            {
                StatusText().Text(
                    L"Unlock Account Armory and verify storage before exporting");
                co_return;
            }

            m_backupOperationInProgress = true;
            AccountActionsButton().IsEnabled(false);
            TopAddAccountButton().IsEnabled(false);
            TopMoreActionsButton().IsEnabled(false);
            AccountsList().IsEnabled(false);
            ScopeExit cleanup{ [this]()
            {
                try
                {
                    m_backupOperationInProgress = false;
                    AccountActionsButton().IsEnabled(true);
                    TopAddAccountButton().IsEnabled(true);
                    TopMoreActionsButton().IsEnabled(true);
                    AccountsList().IsEnabled(true);
                }
                catch (...)
                {
                }
            } };

            noteUserActivity();
            const std::uint64_t operationGeneration{ m_lockGeneration };
            const bool verified{ co_await verifyUser(
                L"Verify your identity to export account passwords") };
            if (!verified || m_isLocked ||
                m_lockGeneration != operationGeneration)
            {
                co_return;
            }

            FileSavePicker picker;
            picker.SuggestedStartLocation(PickerLocationId::DocumentsLibrary);
            picker.SuggestedFileName(onlyRecord
                ? L"AccountArmory-account"
                : L"AccountArmory-all-accounts");
            auto extensions{ single_threaded_vector<hstring>() };
            extensions.Append(L".aabackup");
            picker.FileTypeChoices().Insert(
                L"Account Armory encrypted backup",
                extensions);

            HWND windowHandle{};
            check_hresult(
                m_inner.as<::IWindowNative>()->get_WindowHandle(
                    &windowHandle));
            const auto initializeWithWindow{
                picker.as<::IInitializeWithWindow>() };
            check_hresult(initializeWithWindow->Initialize(windowHandle));

            const StorageFile file{ co_await picker.PickSaveFileAsync() };
            if (!file || m_isLocked ||
                m_lockGeneration != operationGeneration)
            {
                if (!m_isLocked)
                {
                    StatusText().Text(L"Backup export canceled");
                }
                co_return;
            }

            hstring passwordValue{
                co_await requestBackupPassword(true) };
            if (passwordValue.empty() || m_isLocked ||
                m_lockGeneration != operationGeneration)
            {
                co_return;
            }

            std::wstring backupPassword{ passwordValue.c_str() };
            passwordValue = hstring{};
            auto wipeBackupPassword{
                account_vault::security::wipeOnExit(backupPassword) };
            std::vector<PortableAccount> portableAccounts;
            std::wstring preparationError;
            if (!buildPortableAccounts(
                    onlyRecord,
                    portableAccounts,
                    preparationError))
            {
                secureWipe(backupPassword);
                StatusText().Text(hstring{ preparationError });
                co_return;
            }

            apartment_context uiThread;
            std::string encryptedJson;
            std::wstring encryptionError;
            co_await resume_background();
            const bool encrypted{ m_backupService.encrypt(
                portableAccounts,
                backupPassword,
                encryptedJson,
                encryptionError) };
            secureWipe(portableAccounts);
            secureWipe(backupPassword);
            co_await uiThread;

            if (!encrypted)
            {
                StatusText().Text(hstring{ encryptionError });
                co_return;
            }
            if (m_isLocked || m_lockGeneration != operationGeneration)
            {
                co_return;
            }

            co_await FileIO::WriteTextAsync(
                file,
                to_hstring(encryptedJson),
                UnicodeEncoding::Utf8);
            encryptedJson.clear();

            if (!m_isLocked)
            {
                StatusText().Text(onlyRecord
                    ? L"Encrypted account backup exported"
                    : L"Encrypted backup of all accounts exported");
                noteUserActivity();
            }
        }
        catch (...)
        {
            try
            {
                StatusText().Text(L"The encrypted backup could not be exported");
            }
            catch (...)
            {
            }
        }
    }

    fire_and_forget MainWindow::showImportBackup(bool requireSingleAccount)
    {
        try
        {
            auto lifetime{ get_strong() };
            if (m_backupOperationInProgress)
            {
                StatusText().Text(L"Another backup operation is already running");
                co_return;
            }
            if (!m_storageReady || m_isLocked)
            {
                StatusText().Text(
                    L"Unlock Account Armory and verify storage before importing");
                co_return;
            }

            m_backupOperationInProgress = true;
            AccountActionsButton().IsEnabled(false);
            TopAddAccountButton().IsEnabled(false);
            TopMoreActionsButton().IsEnabled(false);
            AccountsList().IsEnabled(false);
            ScopeExit cleanup{ [this]()
            {
                try
                {
                    m_backupOperationInProgress = false;
                    AccountActionsButton().IsEnabled(true);
                    TopAddAccountButton().IsEnabled(true);
                    TopMoreActionsButton().IsEnabled(true);
                    AccountsList().IsEnabled(true);
                }
                catch (...)
                {
                }
            } };

            noteUserActivity();
            const std::uint64_t operationGeneration{ m_lockGeneration };

            FileOpenPicker picker;
            picker.SuggestedStartLocation(PickerLocationId::DocumentsLibrary);
            picker.FileTypeFilter().Append(L".aabackup");

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
                    StatusText().Text(L"Backup import canceled");
                }
                co_return;
            }

            const auto properties{ co_await file.GetBasicPropertiesAsync() };
            if (properties.Size() == 0 ||
                properties.Size() > MaximumBackupBytes)
            {
                StatusText().Text(L"The backup file size is invalid");
                co_return;
            }

            const hstring fileText{ co_await FileIO::ReadTextAsync(
                file,
                UnicodeEncoding::Utf8) };
            if (m_isLocked || m_lockGeneration != operationGeneration)
            {
                co_return;
            }

            hstring passwordValue{
                co_await requestBackupPassword(false) };
            if (passwordValue.empty() || m_isLocked ||
                m_lockGeneration != operationGeneration)
            {
                co_return;
            }

            std::wstring backupPassword{ passwordValue.c_str() };
            passwordValue = hstring{};
            auto wipeBackupPassword{
                account_vault::security::wipeOnExit(backupPassword) };
            const bool verified{ co_await verifyUser(
                L"Verify your identity to import account passwords") };
            if (!verified || m_isLocked ||
                m_lockGeneration != operationGeneration)
            {
                secureWipe(backupPassword);
                co_return;
            }

            const std::string encryptedJson{ to_string(fileText) };
            apartment_context uiThread;
            co_await resume_background();

            auto readResult{ m_backupService.decrypt(
                encryptedJson,
                backupPassword) };
            secureWipe(backupPassword);

            std::vector<Account> protectedImports;
            std::wstring importError;
            if (readResult.succeeded &&
                requireSingleAccount &&
                readResult.accounts.size() != 1)
            {
                importError =
                    L"This backup does not contain exactly one record.";
            }
            else if (readResult.succeeded)
            {
                protectedImports.reserve(readResult.accounts.size());
                account_vault::services::CredentialService credentials;
                for (auto& portable : readResult.accounts)
                {
                    if (portable.kind == L"credential")
                    {
                        if (portable.serviceName.empty() ||
                            portable.category.empty() ||
                            portable.emailAddress.empty() ||
                            portable.password.empty())
                        {
                            importError =
                                L"An imported credential is missing a required field.";
                            protectedImports.clear();
                            break;
                        }
                        const auto protectedPassword{
                            credentials.protectPassword(portable.password) };
                        std::wstring protectedRecoveryPassword;
                        if (!portable.recoveryEmailPassword.empty())
                        {
                            const auto value{ credentials.protectPassword(
                                portable.recoveryEmailPassword) };
                            if (!value)
                            {
                                importError =
                                    L"An imported recovery password could not be protected locally.";
                                protectedImports.clear();
                                break;
                            }
                            protectedRecoveryPassword = *value;
                        }
                        if (!protectedPassword)
                        {
                            importError =
                                L"An imported password could not be protected locally.";
                            protectedImports.clear();
                            break;
                        }

                        protectedImports.push_back(Account{
                            .recordId = 0,
                            .kind = AccountKind::Credential,
                            .emailAddress = std::move(portable.emailAddress),
                            .serviceName = std::move(portable.serviceName),
                            .category = std::move(portable.category),
                            .username = std::move(portable.username),
                            .website = std::move(portable.website),
                            .recoveryEmail = std::move(portable.recoveryEmail),
                            .notes = std::move(portable.notes),
                            .protectedPassword = *protectedPassword,
                            .protectedRecoveryEmailPassword =
                                std::move(protectedRecoveryPassword),
                        });
                        continue;
                    }

                    const auto launcherPassword{
                        credentials.protectPassword(
                            portable.launcherPassword) };
                    const auto emailPassword{
                        credentials.protectPassword(portable.emailPassword) };
                    if (!launcherPassword || !emailPassword)
                    {
                        importError =
                            L"An imported password could not be protected locally.";
                        protectedImports.clear();
                        break;
                    }

                    protectedImports.push_back(Account{
                        .recordId = 0,
                        .launcher = std::move(portable.launcher),
                        .launcherUsername =
                            std::move(portable.launcherUsername),
                        .emailAddress = std::move(portable.emailAddress),
                        .emailProvider = std::move(portable.emailProvider),
                        .emailProviderWebsite =
                            std::move(portable.emailProviderWebsite),
                        .protectedLauncherPassword = *launcherPassword,
                        .protectedEmailPassword = *emailPassword,
                    });
                }
            }
            secureWipe(readResult.accounts);
            co_await uiThread;

            if (!readResult.succeeded)
            {
                StatusText().Text(hstring{ readResult.error });
                co_return;
            }
            if (!importError.empty())
            {
                StatusText().Text(hstring{ importError });
                co_return;
            }
            if (m_isLocked || m_lockGeneration != operationGeneration)
            {
                co_return;
            }
            if (protectedImports.empty() ||
                protectedImports.size() >
                    MaximumAccountCount - m_repository.accounts().size())
            {
                StatusText().Text(
                    L"Importing this backup would exceed the vault record limit");
                co_return;
            }

            std::vector<Account> candidate{ m_repository.accounts() };
            RecordId nextId{ m_repository.nextId() };
            const RecordId maximumId{
                (std::numeric_limits<RecordId>::max)() };
            if (nextId == 0 || nextId >= maximumId - 1 ||
                protectedImports.size() >
                    static_cast<std::size_t>(maximumId - 1 - nextId))
            {
                StatusText().Text(L"The account record ID space is exhausted");
                co_return;
            }

            candidate.reserve(candidate.size() + protectedImports.size());
            for (auto& account : protectedImports)
            {
                account.recordId = nextId++;
                candidate.push_back(std::move(account));
            }
            const std::size_t importedCount{ protectedImports.size() };

            std::wstring saveError;
            co_await resume_background();
            const bool saved{ m_accountStorage.save(
                candidate,
                nextId,
                saveError) };
            co_await uiThread;

            if (!saved)
            {
                StatusText().Text(
                    L"The imported accounts could not be saved; nothing changed");
                co_return;
            }

            // Disk is now authoritative. Synchronize the repository even if
            // the app locked while the background save was completing.
            m_repository.replaceAll(std::move(candidate), nextId);
            rebuildRecordFilter();
            refreshAccounts();
            if (!m_isLocked)
            {
                std::wstring status{ L"Imported " };
                status += std::to_wstring(importedCount);
                status += importedCount == 1 ? L" record" : L" records";
                StatusText().Text(hstring{ status });
                noteUserActivity();
            }
        }
        catch (...)
        {
            try
            {
                StatusText().Text(
                    L"The encrypted backup could not be imported; nothing changed");
            }
            catch (...)
            {
            }
        }
    }
}
