#pragma once

#include "../Models/Launcher.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

namespace account_vault::services
{
    struct LauncherDefinition
    {
        models::Launcher value;
        std::wstring_view displayName;
    };

    inline constexpr std::array<LauncherDefinition, 8> LauncherCatalog{
        LauncherDefinition{ models::Launcher::Steam, L"Steam" },
        LauncherDefinition{ models::Launcher::RiotClient, L"Riot Client" },
        LauncherDefinition{ models::Launcher::Epic, L"Epic" },
        LauncherDefinition{ models::Launcher::BattleNet, L"Battle.net" },
        LauncherDefinition{ models::Launcher::EaApp, L"EA App" },
        LauncherDefinition{ models::Launcher::UbisoftConnect, L"Ubisoft Connect" },
        LauncherDefinition{
            models::Launcher::RockstarGamesLauncher,
            L"Rockstar Games Launcher" },
        LauncherDefinition{ models::Launcher::Other, L"Other" },
    };

    [[nodiscard]] inline std::wstring_view launcherDisplayName(
        models::Launcher launcher) noexcept
    {
        for (auto const& definition : LauncherCatalog)
        {
            if (definition.value == launcher)
            {
                return definition.displayName;
            }
        }

        return L"Other";
    }

    [[nodiscard]] inline std::optional<models::Launcher> launcherFromName(
        std::wstring_view name) noexcept
    {
        // Schema versions 1-3 stored Riot Client records as "Riot".
        if (name == L"Riot")
        {
            return models::Launcher::RiotClient;
        }

        for (auto const& definition : LauncherCatalog)
        {
            if (definition.displayName == name)
            {
                return definition.value;
            }
        }

        return std::nullopt;
    }

    [[nodiscard]] inline int findLauncherIndex(
        models::Launcher launcher) noexcept
    {
        for (std::size_t index{}; index < LauncherCatalog.size(); ++index)
        {
            if (LauncherCatalog[index].value == launcher)
            {
                return static_cast<int>(index);
            }
        }

        return static_cast<int>(LauncherCatalog.size()) - 1;
    }
}
