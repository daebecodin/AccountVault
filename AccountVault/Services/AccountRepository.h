#pragma once

#include "../Models/Account.h"

#include <algorithm>
#include <cwctype>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace account_vault::services
{
    class AccountRepository
    {
    public:
        using Account = models::Account;
        using RecordId = models::RecordId;

        [[nodiscard]] RecordId add(
            std::wstring launcher,
            std::wstring launcherUsername,
            std::wstring emailAddress,
            std::wstring emailProvider,
            std::wstring emailProviderWebsite,
            std::wstring protectedLauncherPassword,
            std::wstring protectedEmailPassword)
        {
            const RecordId id{ m_nextId++ };

            m_accounts.push_back(Account{
                .recordId = id,
                .launcher = std::move(launcher),
                .launcherUsername = std::move(launcherUsername),
                .emailAddress = std::move(emailAddress),
                .emailProvider = std::move(emailProvider),
                .emailProviderWebsite = std::move(emailProviderWebsite),
                .protectedLauncherPassword =
                    std::move(protectedLauncherPassword),
                .protectedEmailPassword =
                    std::move(protectedEmailPassword),
            });

            return id;
        }

        [[nodiscard]] bool remove(RecordId id)
        {
            const auto account = std::ranges::find(
                m_accounts,
                id,
                &Account::recordId);

            if (account == m_accounts.end())
            {
                return false;
            }

            m_accounts.erase(account);
            return true;
        }

        [[nodiscard]] bool update(
            RecordId id,
            std::wstring launcher,
            std::wstring launcherUsername,
            std::wstring emailAddress,
            std::wstring emailProvider,
            std::wstring emailProviderWebsite,
            std::optional<std::wstring> protectedLauncherPassword = std::nullopt,
            std::optional<std::wstring> protectedEmailPassword = std::nullopt)
        {
            const auto account = std::ranges::find(
                m_accounts,
                id,
                &Account::recordId);

            if (account == m_accounts.end())
            {
                return false;
            }

            account->launcher = std::move(launcher);
            account->launcherUsername = std::move(launcherUsername);
            account->emailAddress = std::move(emailAddress);
            account->emailProvider = std::move(emailProvider);
            account->emailProviderWebsite = std::move(emailProviderWebsite);
            if (protectedLauncherPassword)
            {
                account->protectedLauncherPassword =
                    std::move(*protectedLauncherPassword);
            }
            if (protectedEmailPassword)
            {
                account->protectedEmailPassword =
                    std::move(*protectedEmailPassword);
            }
            return true;
        }

        [[nodiscard]] bool updateProtectedPasswords(
            RecordId id,
            std::wstring protectedLauncherPassword,
            std::wstring protectedEmailPassword)
        {
            const auto account = std::ranges::find(
                m_accounts,
                id,
                &Account::recordId);

            if (account == m_accounts.end())
            {
                return false;
            }

            account->protectedLauncherPassword =
                std::move(protectedLauncherPassword);
            account->protectedEmailPassword =
                std::move(protectedEmailPassword);
            return true;
        }

        void replaceAll(
            std::vector<Account> accounts,
            RecordId nextId)
        {
            m_accounts = std::move(accounts);
            m_nextId = nextId;
        }

        [[nodiscard]] std::vector<Account> const& accounts() const noexcept
        {
            return m_accounts;
        }

        [[nodiscard]] RecordId nextId() const noexcept
        {
            return m_nextId;
        }

        [[nodiscard]] Account const* find(RecordId id) const noexcept
        {
            const auto account = std::ranges::find(
                m_accounts,
                id,
                &Account::recordId);

            return account == m_accounts.end() ? nullptr : &*account;
        }

        [[nodiscard]] std::vector<Account const*> search(
            std::wstring_view query,
            std::wstring_view launcher) const
        {
            const std::wstring loweredQuery{ toLower(query) };
            std::vector<Account const*> matches;
            matches.reserve(m_accounts.size());

            for (Account const& account : m_accounts)
            {
                const bool launcherMatches =
                    launcher.empty() || account.launcher == launcher;

                const bool queryMatches =
                    loweredQuery.empty() ||
                    containsIgnoreCase(account.launcher, loweredQuery) ||
                    containsIgnoreCase(account.launcherUsername, loweredQuery) ||
                    containsIgnoreCase(account.emailAddress, loweredQuery) ||
                    containsIgnoreCase(account.emailProvider, loweredQuery) ||
                    containsIgnoreCase(account.emailProviderWebsite, loweredQuery);

                if (launcherMatches && queryMatches)
                {
                    matches.push_back(&account);
                }
            }

            return matches;
        }

        [[nodiscard]] std::size_t size() const noexcept
        {
            return m_accounts.size();
        }

    private:
        [[nodiscard]] static std::wstring toLower(std::wstring_view value)
        {
            std::wstring result{ value };

            std::ranges::transform(
                result,
                result.begin(),
                [](wchar_t character)
                {
                    return static_cast<wchar_t>(std::towlower(character));
                });

            return result;
        }

        [[nodiscard]] static bool containsIgnoreCase(
            std::wstring_view value,
            std::wstring const& loweredQuery)
        {
            return toLower(value).find(loweredQuery) != std::wstring::npos;
        }

        std::vector<Account> m_accounts;
        RecordId m_nextId{ 1 };
    };
}
