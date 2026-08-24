#pragma once

#include "Launcher.h"

#include <cstdint>
#include <string>

namespace account_vault::models
{
    using RecordId = std::uint64_t;

    enum class AccountKind : std::uint8_t
    {
        Launcher,
        Credential
    };

    struct Account
    {
        RecordId recordId{};
        AccountKind kind{ AccountKind::Launcher };

        // Launcher-account fields. The enum is converted to its stable display
        // name only at storage and portable-backup boundaries.
        Launcher launcher{ Launcher::Other };
        std::wstring launcherUsername;
        std::wstring emailAddress;
        std::wstring emailProvider;
        std::wstring emailProviderWebsite;
        std::wstring protectedLauncherPassword;
        std::wstring protectedEmailPassword;

        // General credential fields. Category is the user-facing grouping
        // (Finance, School, Work, or a custom value); serviceName identifies
        // the actual site or service inside that category.
        std::wstring serviceName;
        std::wstring category;
        std::wstring username;
        std::wstring website;
        std::wstring recoveryEmail;
        std::wstring notes;
        std::wstring protectedPassword;
        std::wstring protectedRecoveryEmailPassword;
    };
}
