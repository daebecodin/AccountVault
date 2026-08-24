#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace account_vault::services
{
    struct EmailProvider
    {
        std::wstring_view name;
        std::wstring_view website;
    };

    inline constexpr std::array<EmailProvider, 6> EmailProviders{
        EmailProvider{ L"Gmail", L"https://mail.google.com/" },
        EmailProvider{ L"Yahoo", L"https://mail.yahoo.com/" },
        EmailProvider{ L"Outlook", L"https://outlook.live.com/mail/" },
        EmailProvider{ L"MSN", L"https://outlook.live.com/mail/" },
        EmailProvider{ L"Inbox.lv", L"https://www.inbox.lv/" },
        EmailProvider{ L"ZSTHost", L"https://mail.zsthost.com/?_task=login" },
    };

    [[nodiscard]] inline int findEmailProviderIndex(
        std::wstring_view name,
        std::wstring_view website) noexcept
    {
        for (std::size_t index = 0; index < EmailProviders.size(); ++index)
        {
            if (EmailProviders[index].name == name)
            {
                return static_cast<int>(index);
            }
        }

        for (std::size_t index = 0; index < EmailProviders.size(); ++index)
        {
            if (EmailProviders[index].website == website)
            {
                return static_cast<int>(index);
            }
        }

        return -1;
    }
}
