#pragma once

#include "ThemeDefinition.h"

#include <cstdint>

namespace account_vault::models
{
    using CustomThemeId = std::uint64_t;

    struct CustomTheme
    {
        CustomThemeId id{};
        ThemeDefinition definition;
    };
}
