#pragma once

#include <cstdint>

namespace account_vault::models
{
    enum class Launcher : std::uint8_t
    {
        Steam,
        RiotClient,
        Epic,
        BattleNet,
        EaApp,
        UbisoftConnect,
        RockstarGamesLauncher,
        Other,
    };
}
