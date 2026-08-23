#pragma once

#include "../Models/Account.h"

#include <string>
#include <vector>

namespace account_vault::services
{
    struct AccountLoadResult
    {
        bool succeeded{ true };
        bool fileFound{ false };
        bool credentialMigrationRequired{ false };
        models::RecordId nextRecordId{ 1 };
        std::vector<models::Account> accounts;
        std::wstring error;
    };

    class AccountStorageService
    {
    public:
        [[nodiscard]] AccountLoadResult load() const;

        [[nodiscard]] bool save(
            std::vector<models::Account> const& accounts,
            models::RecordId nextRecordId,
            std::wstring& error) const;
    };
}
