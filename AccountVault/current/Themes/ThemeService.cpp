#include "pch.h"
#include "../MainWindow.xaml.h"

#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

#include <string>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::UI;

namespace winrt::AccountVault::implementation
{
    void MainWindow::applyWindowChromeTheme() noexcept
    {
        try
        {
            if (m_appWindow &&
                Microsoft::UI::Windowing::AppWindowTitleBar::
                    IsCustomizationSupported())
            {
                const auto boxed = [](Color value)
                {
                    return box_value(value)
                        .as<Windows::Foundation::IReference<Color>>();
                };

                const auto background{ boxed(brushColor(L"AppBackgroundBrush")) };
                const auto surfaceAlt{ boxed(brushColor(L"AppSurfaceAltBrush")) };
                const auto accent{ boxed(brushColor(L"AppAccentBrush")) };
                const auto text{ boxed(brushColor(L"AppTextBrush")) };
                const auto muted{ boxed(brushColor(L"AppMutedTextBrush")) };

                const auto titleBar{ m_appWindow.TitleBar() };
                titleBar.BackgroundColor(background);
                titleBar.ForegroundColor(text);
                titleBar.ButtonBackgroundColor(background);
                titleBar.ButtonForegroundColor(text);
                titleBar.ButtonHoverBackgroundColor(surfaceAlt);
                titleBar.ButtonHoverForegroundColor(accent);
                titleBar.ButtonPressedBackgroundColor(accent);
                titleBar.ButtonPressedForegroundColor(background);
                titleBar.InactiveBackgroundColor(background);
                titleBar.InactiveForegroundColor(muted);
                titleBar.ButtonInactiveBackgroundColor(background);
                titleBar.ButtonInactiveForegroundColor(muted);
            }

            for (auto const& window : m_modelessWindows)
            {
                window.RefreshTheme();
            }
        }
        catch (...)
        {
        }
    }

    void MainWindow::applyPreset(int selectedIndex)
    {
        ThemeDefinition theme{};

        switch (selectedIndex)
        {
        case 0: // Catppuccin Mocha
            theme = ThemeDefinition{
                L"Catppuccin Mocha",
                color(30, 30, 46),
                color(24, 24, 37),
                color(49, 50, 68),
                color(203, 166, 247),
                color(205, 214, 244),
                color(166, 173, 200) };
            break;

        case 1: // Tokyo Night
            theme = ThemeDefinition{
                L"Tokyo Night",
                color(26, 27, 38),
                color(36, 40, 59),
                color(65, 72, 104),
                color(122, 162, 247),
                color(192, 202, 245),
                color(86, 95, 137) };
            break;

        case 2: // Dracula
            theme = ThemeDefinition{
                L"Dracula",
                color(40, 42, 54),
                color(52, 55, 70),
                color(68, 71, 90),
                color(189, 147, 249),
                color(248, 248, 242),
                color(98, 114, 164) };
            break;

        case 3: // Ayu Mirage
            theme = ThemeDefinition{
                L"Ayu Mirage",
                color(31, 36, 48),
                color(36, 41, 54),
                color(50, 56, 68),
                color(255, 173, 102),
                color(204, 202, 194),
                color(112, 122, 140) };
            break;

        case 4: // Dainty Dark
            theme = ThemeDefinition{
                L"Dainty Dark",
                color(18, 24, 34),
                color(24, 32, 44),
                color(35, 45, 60),
                color(92, 207, 230),
                color(215, 218, 224),
                color(127, 140, 152) };
            break;

        case 5: // GitHub Dark Default
            theme = ThemeDefinition{
                L"GitHub Dark",
                color(13, 17, 23),
                color(22, 27, 34),
                color(33, 38, 45),
                color(88, 166, 255),
                color(201, 209, 217),
                color(139, 148, 158) };
            break;

        case 6: // Atom One Dark
            theme = ThemeDefinition{
                L"Atom One Dark",
                color(40, 44, 52),
                color(33, 37, 43),
                color(44, 49, 60),
                color(97, 175, 239),
                color(171, 178, 191),
                color(92, 99, 112) };
            break;

        case 7: // Houston
            theme = ThemeDefinition{
                L"Houston",
                color(13, 17, 23),
                color(19, 26, 36),
                color(28, 38, 51),
                color(34, 211, 238),
                color(225, 232, 240),
                color(107, 114, 128) };
            break;

        case 8: // Night Owl
            theme = ThemeDefinition{
                L"Night Owl",
                color(1, 22, 39),
                color(1, 17, 29),
                color(11, 41, 66),
                color(130, 170, 255),
                color(214, 222, 235),
                color(99, 119, 119) };
            break;

        case 9: // Matcha
            theme = ThemeDefinition{
                L"Matcha",
                color(39, 49, 54),
                color(47, 59, 63),
                color(58, 71, 75),
                color(126, 176, 138),
                color(209, 222, 211),
                color(126, 164, 176) };
            break;

        default:
        {
            if (selectedIndex < BuiltInThemeCount)
            {
                return;
            }

            const auto customIndex = static_cast<std::size_t>(
                selectedIndex - BuiltInThemeCount);
            if (customIndex >= m_customThemes.size())
            {
                return;
            }

            theme = m_customThemes[customIndex];
            break;
        }
        }

        applyTheme(theme);
    }
    void MainWindow::applyTheme(ThemeDefinition const& theme)
    {
        const auto blend = [](
            Color foreground,
            Color background,
            std::uint8_t foregroundWeight)
        {
            const auto channel = [foregroundWeight](
                std::uint8_t foregroundValue,
                std::uint8_t backgroundValue)
            {
                const unsigned inverseWeight{ 255U - foregroundWeight };
                return static_cast<std::uint8_t>(
                    (static_cast<unsigned>(foregroundValue) * foregroundWeight +
                     static_cast<unsigned>(backgroundValue) * inverseWeight) /
                    255U);
            };

            return ColorHelper::FromArgb(
                255,
                channel(foreground.R, background.R),
                channel(foreground.G, background.G),
                channel(foreground.B, background.B));
        };

        Color accentLow{ theme.accent };
        accentLow.A = 32;
        Color accentMedium{ theme.accent };
        accentMedium.A = 64;

        setBrushColor(L"AppBackgroundBrush", theme.background);
        setBrushColor(L"AppSurfaceBrush", theme.surface);
        setBrushColor(L"AppSurfaceAltBrush", theme.surfaceAlt);
        setBrushColor(L"AppAccentBrush", theme.accent);
        setBrushColor(L"AppAccentLowBrush", accentLow);
        setBrushColor(L"AppAccentMediumBrush", accentMedium);
        setBrushColor(
            L"AppAccentHoverBrush",
            blend(theme.accent, theme.background, 224));
        setBrushColor(
            L"AppAccentPressedBrush",
            blend(theme.accent, theme.background, 190));
        setBrushColor(L"AppTextBrush", theme.text);
        setBrushColor(L"AppMutedTextBrush", theme.mutedText);

        // Primary commands share one theme-aware state palette. Keeping these
        // brushes mutable preserves WinUI's native hover transition while the
        // selected accent changes at runtime.
        setBrushColor(L"AccentButtonBackground", theme.accent);
        setBrushColor(
            L"AccentButtonBackgroundPointerOver",
            blend(theme.accent, theme.background, 224));
        setBrushColor(
            L"AccentButtonBackgroundPressed",
            blend(theme.accent, theme.background, 190));
        setBrushColor(L"AccentButtonBorderBrush", theme.accent);
        setBrushColor(
            L"AccentButtonBorderBrushPointerOver",
            blend(theme.accent, theme.background, 224));
        setBrushColor(
            L"AccentButtonBorderBrushPressed",
            blend(theme.accent, theme.background, 190));
        setBrushColor(L"AccentButtonForeground", theme.background);
        setBrushColor(L"AccentButtonForegroundPointerOver", theme.background);
        setBrushColor(L"AccentButtonForegroundPressed", theme.background);
        applyWindowChromeTheme();
    }
    void MainWindow::setBrushColor(
        std::wstring_view resourceName,
        Color value)
    {
        auto brush = Application::Current()
            .Resources()
            .Lookup(box_value(hstring{ resourceName }))
            .as<SolidColorBrush>();
        brush.Color(value);
    }
    Color MainWindow::brushColor(std::wstring_view resourceName) const
    {
        return Application::Current()
            .Resources()
            .Lookup(box_value(hstring{ resourceName }))
            .as<SolidColorBrush>()
            .Color();
    }
    Color MainWindow::color(
        std::uint8_t red,
        std::uint8_t green,
        std::uint8_t blue)
    {
        return ColorHelper::FromArgb(255, red, green, blue);
    }
}
