#pragma once

#include <winrt/Windows.UI.h>

#include <string>

namespace account_vault::models
{
    struct ThemeDefinition
    {
        std::wstring name;
        winrt::Windows::UI::Color background;
        winrt::Windows::UI::Color surface;
        winrt::Windows::UI::Color surfaceAlt;
        winrt::Windows::UI::Color accent;
        winrt::Windows::UI::Color text;
        winrt::Windows::UI::Color mutedText;
    };
}
