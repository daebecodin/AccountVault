#pragma once

#include "../Models/Account.h"

#include <filesystem>
#include <string>
#include <vector>

namespace account_vault::services
{
    // This seam is used only by deterministic storage tests. Production code
    // always uses None.
    enum class StorageFailurePoint
    {
        None,
        OpenTemporaryFile,
        WriteTemporaryFile,
        ReplaceFinalFile,
    };

    struct AccountLoadResult
    {
        bool succeeded{ true };
        bool fileFound{ false };
        bool credentialMigrationRequired{ false };
        bool recoveredTemporaryFile{ false };
        bool quarantinedCorruptFile{ false };
        models::RecordId nextRecordId{ 1 };
        std::vector<models::Account> accounts;
        std::wstring recoveryFilePath;
        std::wstring warning;
        std::wstring error;
    };

    class AccountStorageService
    {
    public:
        // Production storage: Windows ApplicationData LocalFolder.
        AccountStorageService();

        // Test storage: an isolated directory plus optional fault injection.
        explicit AccountStorageService(
            std::filesystem::path storageDirectory,
            StorageFailurePoint failurePoint = StorageFailurePoint::None);

        [[nodiscard]] AccountLoadResult load() const;

        [[nodiscard]] bool save(
            std::vector<models::Account> const& accounts,
            models::RecordId nextRecordId,
            std::wstring& error) const;

    private:
        std::filesystem::path m_storageDirectory;
        StorageFailurePoint m_failurePoint{ StorageFailurePoint::None };
        bool m_isApplicationStorage{ false };
    };
}
