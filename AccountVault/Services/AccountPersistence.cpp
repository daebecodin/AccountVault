#include "pch.h"
#include "../MainWindow.xaml.h"
#include "../Security/SensitiveData.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace winrt::AccountVault::implementation
{
    bool MainWindow::persistAccounts(std::wstring& error) const
    {
        if (!m_storageReady)
        {
            error = L"Account storage is unavailable.";
            return false;
        }

        return m_accountStorage.save(
            m_repository.accounts(),
            m_repository.nextId(),
            error);
    }

    std::optional<MainWindow::RecordId> MainWindow::addAccount(
        std::wstring launcher,
        std::wstring launcherUsername,
        std::wstring launcherPassword,
        std::wstring emailAddress,
        std::wstring emailProvider,
        std::wstring emailProviderWebsite,
        std::wstring emailPassword)
    {
        auto wipeLauncherPassword{
            account_vault::security::wipeOnExit(launcherPassword) };
        auto wipeEmailPassword{
            account_vault::security::wipeOnExit(emailPassword) };

        if (!m_storageReady)
        {
            return std::nullopt;
        }

        const auto protectedLauncherPassword{
            m_credentials.protectPassword(launcherPassword) };
        const auto protectedEmailPassword{
            m_credentials.protectPassword(emailPassword) };
        if (!protectedLauncherPassword || !protectedEmailPassword)
        {
            return std::nullopt;
        }

        // A failed create must restore both the account list and nextRecordId.
        // Removing only the new record would leave a permanent ID gap.
        const auto oldAccounts{ m_repository.accounts() };
        const RecordId oldNextId{ m_repository.nextId() };

        const RecordId id{ m_repository.add(
            std::move(launcher),
            std::move(launcherUsername),
            std::move(emailAddress),
            std::move(emailProvider),
            std::move(emailProviderWebsite),
            *protectedLauncherPassword,
            *protectedEmailPassword) };

        std::wstring error;
        if (!persistAccounts(error))
        {
            m_repository.replaceAll(oldAccounts, oldNextId);
            return std::nullopt;
        }

        return id;
    }

    bool MainWindow::updateAccount(
        RecordId id,
        std::wstring launcher,
        std::wstring launcherUsername,
        std::optional<std::wstring> launcherPassword,
        std::wstring emailAddress,
        std::wstring emailProvider,
        std::wstring emailProviderWebsite,
        std::optional<std::wstring> emailPassword)
    {
        auto wipeLauncherPassword{
            account_vault::security::wipeOnExit(launcherPassword) };
        auto wipeEmailPassword{
            account_vault::security::wipeOnExit(emailPassword) };

        if (!m_storageReady)
        {
            return false;
        }

        const Account* current{ m_repository.find(id) };
        if (!current)
        {
            return false;
        }

        const Account oldAccount{ *current };
        std::optional<std::wstring> protectedLauncherPassword;
        if (launcherPassword)
        {
            protectedLauncherPassword =
                m_credentials.protectPassword(*launcherPassword);
            if (!protectedLauncherPassword)
            {
                return false;
            }
        }

        std::optional<std::wstring> protectedEmailPassword;
        if (emailPassword)
        {
            protectedEmailPassword =
                m_credentials.protectPassword(*emailPassword);
            if (!protectedEmailPassword)
            {
                return false;
            }
        }

        if (!m_repository.update(
                id,
                std::move(launcher),
                std::move(launcherUsername),
                std::move(emailAddress),
                std::move(emailProvider),
                std::move(emailProviderWebsite),
                std::move(protectedLauncherPassword),
                std::move(protectedEmailPassword)))
        {
            return false;
        }

        std::wstring error;
        if (!persistAccounts(error))
        {
            static_cast<void>(m_repository.update(
                id,
                oldAccount.launcher,
                oldAccount.launcherUsername,
                oldAccount.emailAddress,
                oldAccount.emailProvider,
                oldAccount.emailProviderWebsite,
                oldAccount.protectedLauncherPassword,
                oldAccount.protectedEmailPassword));
            return false;
        }

        return true;
    }

    std::optional<MainWindow::RecordId> MainWindow::addCredential(
        std::wstring serviceName,
        std::wstring category,
        std::wstring username,
        std::wstring emailAddress,
        std::wstring password,
        std::wstring website,
        std::wstring recoveryEmail,
        std::wstring recoveryEmailPassword,
        std::wstring notes)
    {
        auto wipePassword{ account_vault::security::wipeOnExit(password) };
        auto wipeRecoveryPassword{
            account_vault::security::wipeOnExit(recoveryEmailPassword) };

        if (!m_storageReady)
        {
            return std::nullopt;
        }

        const auto protectedPassword{ m_credentials.protectPassword(password) };
        if (!protectedPassword)
        {
            return std::nullopt;
        }

        std::wstring protectedRecoveryPassword;
        if (!recoveryEmailPassword.empty())
        {
            const auto value{
                m_credentials.protectPassword(recoveryEmailPassword) };
            if (!value)
            {
                return std::nullopt;
            }
            protectedRecoveryPassword = *value;
        }

        const auto oldAccounts{ m_repository.accounts() };
        const RecordId oldNextId{ m_repository.nextId() };
        const RecordId id{ m_repository.addCredential(
            std::move(serviceName),
            std::move(category),
            std::move(username),
            std::move(emailAddress),
            std::move(website),
            std::move(recoveryEmail),
            std::move(notes),
            *protectedPassword,
            std::move(protectedRecoveryPassword)) };

        std::wstring error;
        if (!persistAccounts(error))
        {
            m_repository.replaceAll(oldAccounts, oldNextId);
            return std::nullopt;
        }
        return id;
    }

    bool MainWindow::updateCredential(
        RecordId id,
        std::wstring serviceName,
        std::wstring category,
        std::wstring username,
        std::wstring emailAddress,
        std::optional<std::wstring> password,
        std::wstring website,
        std::wstring recoveryEmail,
        std::optional<std::wstring> recoveryEmailPassword,
        std::wstring notes)
    {
        auto wipePassword{ account_vault::security::wipeOnExit(password) };
        auto wipeRecoveryPassword{
            account_vault::security::wipeOnExit(recoveryEmailPassword) };

        if (!m_storageReady)
        {
            return false;
        }

        const Account* current{ m_repository.find(id) };
        if (!current || current->kind != AccountKind::Credential)
        {
            return false;
        }

        const Account oldAccount{ *current };
        std::optional<std::wstring> protectedPassword;
        if (password)
        {
            protectedPassword = m_credentials.protectPassword(*password);
            if (!protectedPassword)
            {
                return false;
            }
        }

        std::optional<std::wstring> protectedRecoveryPassword;
        if (recoveryEmailPassword)
        {
            if (recoveryEmailPassword->empty())
            {
                protectedRecoveryPassword = std::wstring{};
            }
            else
            {
                protectedRecoveryPassword =
                    m_credentials.protectPassword(*recoveryEmailPassword);
                if (!protectedRecoveryPassword)
                {
                    return false;
                }
            }
        }

        if (!m_repository.updateCredential(
                id,
                std::move(serviceName),
                std::move(category),
                std::move(username),
                std::move(emailAddress),
                std::move(website),
                std::move(recoveryEmail),
                std::move(notes),
                std::move(protectedPassword),
                std::move(protectedRecoveryPassword)))
        {
            return false;
        }

        std::wstring error;
        if (!persistAccounts(error))
        {
            static_cast<void>(m_repository.updateCredential(
                id,
                oldAccount.serviceName,
                oldAccount.category,
                oldAccount.username,
                oldAccount.emailAddress,
                oldAccount.website,
                oldAccount.recoveryEmail,
                oldAccount.notes,
                oldAccount.protectedPassword,
                oldAccount.protectedRecoveryEmailPassword));
            return false;
        }
        return true;
    }
}
