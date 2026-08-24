#pragma once

#include <array>
#include <string_view>

namespace account_vault::services
{
    inline constexpr std::array<std::wstring_view, 8>
        DefaultCredentialCategories{
            L"Finance",
            L"School",
            L"Work",
            L"Shopping",
            L"Social",
            L"Entertainment",
            L"Utilities",
            L"Other",
        };

    [[nodiscard]] inline bool isDefaultCredentialCategory(
        std::wstring_view value) noexcept
    {
        for (auto const category : DefaultCredentialCategories)
        {
            if (category == value)
            {
                return true;
            }
        }

        return false;
    }
}
