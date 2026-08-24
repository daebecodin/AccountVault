#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace account_vault::services
{
    struct BrowserCsvCredential
    {
        std::wstring name;
        std::wstring url;
        std::wstring username;
        std::wstring password;
    };

    struct BrowserCsvParseResult
    {
        std::vector<BrowserCsvCredential> credentials;
        std::size_t skippedRows{};
        std::wstring error;

        [[nodiscard]] bool succeeded() const noexcept
        {
            return error.empty();
        }
    };

    class BrowserCsvImportService
    {
    public:
        static constexpr std::uint64_t MaximumCsvBytes{
            16U * 1024U * 1024U };
        static constexpr std::size_t MaximumCredentialCount{ 3000 };

        [[nodiscard]] static BrowserCsvParseResult parse(
            std::wstring_view csvText);

        static void wipe(
            std::vector<BrowserCsvCredential>& credentials) noexcept;
    };
}
