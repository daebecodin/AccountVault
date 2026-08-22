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
        std::wstring launcherAccountId;
        std::wstring email;
        std::wstring password;
    };
}
