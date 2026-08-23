#pragma once

#include "../Models/Account.h"

#include <optional>
#include <string>

namespace account_vault::services
{
    class CredentialService
    {
    public:
        using RecordId = models::RecordId;

        [[nodiscard]] std::optional<std::wstring> protectPassword(
            std::wstring const& password) const;

        [[nodiscard]] std::optional<std::wstring> unprotectPassword(
            std::wstring const& protectedPassword) const;

        [[nodiscard]] std::optional<std::wstring> legacyLauncherPassword(
            RecordId id) const;

        [[nodiscard]] std::optional<std::wstring> legacyEmailPassword(
            RecordId id) const;

        [[nodiscard]] bool removeLegacyAccountSecrets(RecordId id) const;
    };
}
