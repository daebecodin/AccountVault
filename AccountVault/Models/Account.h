#pragma once

#include <cstdint>
#include <string>

namespace account_vault::models
{
    using RecordId = std::uint64_t;

    struct Account
    {
        RecordId recordId{};
        std::wstring launcher;
        std::wstring launcherUsername;
        std::wstring launcherPassword;
        std::wstring emailAddress;
        std::wstring emailProviderWebsite;
        std::wstring emailPassword;
    };
}
