#include "pch.h"
#include "BrowserCsvImportService.h"
#include "../Security/SensitiveData.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    constexpr std::size_t MaximumColumnCount{ 64 };
    constexpr std::size_t MaximumFieldCharacters{ 32768 };

    using CsvRow = std::vector<std::wstring>;
    using CsvTable = std::vector<CsvRow>;

    void wipeTable(CsvTable& table) noexcept
    {
        for (auto& row : table)
        {
            for (auto& field : row)
            {
                account_vault::security::wipe(field);
            }
            row.clear();
        }
        table.clear();
    }

    struct TableWiper
    {
        CsvTable& table;
        ~TableWiper()
        {
            wipeTable(table);
        }
    };

    [[nodiscard]] std::wstring trimCopy(std::wstring_view value)
    {
        while (!value.empty() && std::iswspace(value.front()))
        {
            value.remove_prefix(1);
        }
        while (!value.empty() && std::iswspace(value.back()))
        {
            value.remove_suffix(1);
        }
        return std::wstring{ value };
    }

    [[nodiscard]] std::wstring normalizeHeader(std::wstring_view value)
    {
        if (!value.empty() && value.front() == 0xFEFF)
        {
            value.remove_prefix(1);
        }

        std::wstring normalized;
        normalized.reserve(value.size());
        for (wchar_t character : value)
        {
            if (std::iswspace(character) || character == L'_' ||
                character == L'-')
            {
                continue;
            }
            normalized.push_back(
                static_cast<wchar_t>(std::towlower(character)));
        }
        return normalized;
    }

    [[nodiscard]] bool parseTable(
        std::wstring_view text,
        CsvTable& rows,
        std::wstring& error)
    {
        CsvRow row;
        std::wstring field;
        bool quoted{};

        const auto pushField = [&]() -> bool
        {
            if (field.size() > MaximumFieldCharacters ||
                row.size() >= MaximumColumnCount)
            {
                error = L"The browser CSV contains an oversized row or field.";
                return false;
            }
            row.push_back(std::move(field));
            field.clear();
            return true;
        };

        const auto pushRow = [&]() -> bool
        {
            if (!pushField())
            {
                return false;
            }

            const bool hasValue{ std::ranges::any_of(
                row,
                [](std::wstring const& value)
                {
                    return !value.empty();
                }) };
            if (hasValue)
            {
                if (rows.size() >
                    account_vault::services::
                        BrowserCsvImportService::MaximumCredentialCount)
                {
                    error = L"The browser CSV contains too many login rows.";
                    return false;
                }
                rows.push_back(std::move(row));
            }
            row.clear();
            return true;
        };

        for (std::size_t index{}; index < text.size(); ++index)
        {
            const wchar_t character{ text[index] };
            if (quoted)
            {
                if (character == L'"')
                {
                    if (index + 1 < text.size() && text[index + 1] == L'"')
                    {
                        field.push_back(L'"');
                        ++index;
                    }
                    else
                    {
                        quoted = false;
                    }
                }
                else
                {
                    field.push_back(character);
                }
                continue;
            }

            if (character == L'"' && field.empty())
            {
                quoted = true;
            }
            else if (character == L',')
            {
                if (!pushField())
                {
                    return false;
                }
            }
            else if (character == L'\r' || character == L'\n')
            {
                if (character == L'\r' && index + 1 < text.size() &&
                    text[index + 1] == L'\n')
                {
                    ++index;
                }
                if (!pushRow())
                {
                    return false;
                }
            }
            else
            {
                field.push_back(character);
            }

            if (field.size() > MaximumFieldCharacters)
            {
                error = L"The browser CSV contains an oversized field.";
                return false;
            }
        }

        if (quoted)
        {
            error = L"The browser CSV contains an unterminated quoted value.";
            return false;
        }

        if (!field.empty() || !row.empty())
        {
            return pushRow();
        }
        return true;
    }

    [[nodiscard]] std::optional<std::size_t> findColumn(
        CsvRow const& headers,
        std::initializer_list<std::wstring_view> aliases)
    {
        for (std::size_t index{}; index < headers.size(); ++index)
        {
            const std::wstring normalized{ normalizeHeader(headers[index]) };
            for (std::wstring_view alias : aliases)
            {
                if (normalized == alias)
                {
                    return index;
                }
            }
        }
        return std::nullopt;
    }
}

namespace account_vault::services
{
    BrowserCsvParseResult BrowserCsvImportService::parse(
        std::wstring_view csvText)
    {
        BrowserCsvParseResult result;
        CsvTable rows;
        TableWiper wipeRows{ rows };

        if (csvText.empty())
        {
            result.error = L"The browser CSV is empty.";
            return result;
        }
        if (!parseTable(csvText, rows, result.error))
        {
            return result;
        }
        if (rows.size() < 2)
        {
            result.error = L"The browser CSV does not contain any login rows.";
            return result;
        }

        const CsvRow& headers{ rows.front() };
        const auto urlColumn{ findColumn(
            headers,
            { L"url", L"origin", L"website", L"hostname", L"loginuri" }) };
        const auto usernameColumn{ findColumn(
            headers,
            { L"username", L"user", L"login", L"loginusername" }) };
        const auto passwordColumn{ findColumn(
            headers,
            { L"password", L"pass", L"loginpassword" }) };
        const auto nameColumn{ findColumn(
            headers,
            { L"name", L"title", L"service", L"site" }) };

        if (!urlColumn || !usernameColumn || !passwordColumn)
        {
            result.error =
                L"The browser CSV must contain url, username, and password columns.";
            return result;
        }

        const std::size_t requiredColumn{
            (std::max)({ *urlColumn, *usernameColumn, *passwordColumn }) };
        result.credentials.reserve((std::min)(
            rows.size() - 1,
            MaximumCredentialCount));

        for (std::size_t rowIndex{ 1 }; rowIndex < rows.size(); ++rowIndex)
        {
            const CsvRow& row{ rows[rowIndex] };
            if (row.size() <= requiredColumn)
            {
                ++result.skippedRows;
                continue;
            }

            BrowserCsvCredential credential;
            if (nameColumn && *nameColumn < row.size())
            {
                credential.name = trimCopy(row[*nameColumn]);
            }
            credential.url = trimCopy(row[*urlColumn]);
            credential.username = trimCopy(row[*usernameColumn]);
            credential.password = row[*passwordColumn];

            if (credential.url.empty() || credential.username.empty() ||
                credential.password.empty())
            {
                security::wipe(credential.name);
                security::wipe(credential.url);
                security::wipe(credential.username);
                security::wipe(credential.password);
                ++result.skippedRows;
                continue;
            }

            result.credentials.push_back(std::move(credential));
            if (result.credentials.size() >= MaximumCredentialCount)
            {
                break;
            }
        }

        if (result.credentials.empty())
        {
            result.error = L"No complete browser logins were found in the CSV.";
        }
        return result;
    }

    void BrowserCsvImportService::wipe(
        std::vector<BrowserCsvCredential>& credentials) noexcept
    {
        for (auto& credential : credentials)
        {
            security::wipe(credential.name);
            security::wipe(credential.url);
            security::wipe(credential.username);
            security::wipe(credential.password);
        }
        credentials.clear();
    }
}
