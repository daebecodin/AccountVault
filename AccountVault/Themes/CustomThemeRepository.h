#pragma once

#include "../Models/CustomTheme.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace account_vault::themes
{
    struct CustomThemeLoadResult
    {
        bool succeeded{ true };
        bool fileFound{ false };
        std::wstring error;
    };

    class CustomThemeRepository
    {
    public:
        CustomThemeRepository();

        explicit CustomThemeRepository(
            std::filesystem::path storageDirectory);

        [[nodiscard]] CustomThemeLoadResult load();

        [[nodiscard]] std::vector<models::CustomTheme> const& themes() const
            noexcept;

        [[nodiscard]] models::CustomTheme const* find(
            models::CustomThemeId id) const noexcept;

        [[nodiscard]] std::optional<models::CustomThemeId> create(
            models::ThemeDefinition definition,
            std::wstring& error);

        [[nodiscard]] bool update(
            models::CustomThemeId id,
            models::ThemeDefinition definition,
            std::wstring& error);

        [[nodiscard]] bool remove(
            models::CustomThemeId id,
            std::wstring& error);

    private:
        [[nodiscard]] bool saveSnapshot(
            std::vector<models::CustomTheme> const& themes,
            models::CustomThemeId nextThemeId,
            std::wstring& error) const;

        std::filesystem::path m_storageDirectory;
        std::vector<models::CustomTheme> m_themes;
        models::CustomThemeId m_nextThemeId{ 1 };
        bool m_storageReady{ true };
    };
}
