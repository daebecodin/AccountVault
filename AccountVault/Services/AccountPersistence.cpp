#include "pch.h"
#include "../MainWindow.xaml.h"

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
}
