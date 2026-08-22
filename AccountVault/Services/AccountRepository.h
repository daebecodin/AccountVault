#pragma once

#include "../Models/Account.h"

#include <algorithm>
#include <cwctype>
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
            std::wstring launcherAccountId,
            std::wstring email,
            std::wstring password)
        {
            const RecordId id{ m_nextId++ };

            m_accounts.push_back(Account{
                .recordId = id,
                .launcher = std::move(launcher),
                .launcherAccountId = std::move(launcherAccountId),
                .email = std::move(email),
                .password = std::move(password),
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
                    containsIgnoreCase(account.launcherAccountId, loweredQuery) ||
                    containsIgnoreCase(account.email, loweredQuery);

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
