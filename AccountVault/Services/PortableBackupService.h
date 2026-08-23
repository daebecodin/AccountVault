#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace account_vault::services
{
    struct PortableAccount
    {
        std::wstring kind{ L"launcher" };
        std::wstring launcher;
        std::wstring launcherUsername;
        std::wstring launcherPassword;
        std::wstring emailAddress;
        std::wstring emailProvider;
        std::wstring emailProviderWebsite;
        std::wstring emailPassword;
        std::wstring serviceName;
        std::wstring category;
        std::wstring username;
        std::wstring website;
        std::wstring recoveryEmail;
        std::wstring notes;
        std::wstring password;
        std::wstring recoveryEmailPassword;
    };

    struct BackupReadResult
    {
        bool succeeded{};
        std::vector<PortableAccount> accounts;
        std::wstring error;
    };

    class PortableBackupService
    {
    public:
        // Production constructor: PBKDF2-HMAC-SHA-256 with 600,000 rounds.
        PortableBackupService();

        // Deterministic test seam. Production code should use the default.
        explicit PortableBackupService(std::uint64_t iterations);

        [[nodiscard]] bool encrypt(
            std::vector<PortableAccount> const& accounts,
            std::wstring_view password,
            std::string& encryptedJson,
            std::wstring& error) const noexcept;

        [[nodiscard]] BackupReadResult decrypt(
            std::string const& encryptedJson,
            std::wstring_view password) const noexcept;

    private:
        std::uint64_t m_iterations;
    };
}
