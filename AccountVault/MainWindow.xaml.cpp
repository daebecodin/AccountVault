#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Windows.Storage.h>

#include <string>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Input;
using namespace Windows::Foundation;
using namespace Windows::Storage;

namespace
{
    constexpr int DefaultStartupThemeIndex{ 3 }; // Ayu Mirage
    constexpr wchar_t StartupThemeSettingKey[]{ L"StartupThemeIndex" };
}

namespace winrt::AccountVault::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        MenuFlyout defaultMenu;

        MenuFlyoutItem setDefault;
        setDefault.Text(L"Set default");
        setDefault.Click(
            [this](
                IInspectable const&,
                RoutedEventArgs const&)
            {
                const int themeIndex{ ThemePicker().SelectedIndex() };
                if (themeIndex < 0 || themeIndex >= BuiltInThemeCount)
                {
                    return;
                }

                const auto themeItem{ ThemePicker()
                    .SelectedItem()
                    .as<ComboBoxItem>() };
                const hstring themeName{
                    unbox_value<hstring>(themeItem.Content()) };

                ApplicationData::Current()
                    .LocalSettings()
                    .Values()
                    .Insert(
                        StartupThemeSettingKey,
                        box_value(static_cast<std::int32_t>(themeIndex)));

                std::wstring status{ themeName.c_str() };
                status += L" set as the startup theme";
                StatusText().Text(hstring{ status });
            });

        MenuFlyoutItem removeDefault;
        removeDefault.Text(L"Remove default");
        removeDefault.Click(
            [this](
                IInspectable const&,
                RoutedEventArgs const&)
            {
                const auto themeItem{ ThemePicker()
                    .SelectedItem()
                    .as<ComboBoxItem>() };
                const hstring themeName{
                    unbox_value<hstring>(themeItem.Content()) };

                ApplicationData::Current()
                    .LocalSettings()
                    .Values()
                    .Remove(StartupThemeSettingKey);

                std::wstring status{ themeName.c_str() };
                status += L" removed as the startup theme; Ayu Mirage is the fallback";
                StatusText().Text(hstring{ status });
            });

        defaultMenu.Opening(
            [this, setDefault, removeDefault](
                IInspectable const&,
                IInspectable const&)
            {
                const int selectedIndex{ ThemePicker().SelectedIndex() };
                const bool hasBuiltInSelection{
                    selectedIndex >= 0 && selectedIndex < BuiltInThemeCount };

                bool isSavedDefault{ false };
                const auto settingsValues{ ApplicationData::Current()
                    .LocalSettings()
                    .Values() };

                if (hasBuiltInSelection &&
                    settingsValues.HasKey(StartupThemeSettingKey))
                {
                    const int storedIndex{ unbox_value<std::int32_t>(
                        settingsValues.Lookup(StartupThemeSettingKey)) };
                    isSavedDefault = storedIndex == selectedIndex;
                }

                setDefault.IsEnabled(hasBuiltInSelection);
                removeDefault.IsEnabled(isSavedDefault);
            });

        defaultMenu.Items().Append(setDefault);
        defaultMenu.Items().Append(removeDefault);

        ThemePicker().AddHandler(
            UIElement::PointerPressedEvent(),
            box_value(PointerEventHandler{
                [this, defaultMenu](
                    IInspectable const&,
                    PointerRoutedEventArgs const& pointerArgs)
                {
                    const auto pointerPoint{
                        pointerArgs.GetCurrentPoint(ThemePicker()) };
                    if (!pointerPoint.Properties().IsRightButtonPressed() ||
                        ThemePicker().IsDropDownOpen() ||
                        ThemePicker().SelectedIndex() < 0)
                    {
                        return;
                    }

                    pointerArgs.Handled(true);
                    DispatcherQueue().TryEnqueue(
                        [this, defaultMenu]()
                        {
                            defaultMenu.ShowAt(ThemePicker());
                        });
                } }),
            true);

        int startupThemeIndex{ DefaultStartupThemeIndex };
        const auto settingsValues{
            ApplicationData::Current().LocalSettings().Values() };

        if (settingsValues.HasKey(StartupThemeSettingKey))
        {
            const int storedIndex{ unbox_value<std::int32_t>(
                settingsValues.Lookup(StartupThemeSettingKey)) };

            if (storedIndex >= 0 && storedIndex < BuiltInThemeCount)
            {
                startupThemeIndex = storedIndex;
            }
        }

        ThemePicker().SelectedIndex(startupThemeIndex);

        // Temporary data makes the first UI run immediately useful.
        // Delete these calls when you are ready to begin with an empty vault.
        static_cast<void>(m_repository.add(
            L"Steam",
            L"night_shift",
            L"demo-launcher-password",
            L"night@example.com",
            L"Gmail",
            L"https://mail.google.com/",
            L"demo-email-password"));
        static_cast<void>(m_repository.add(
            L"Riot",
            L"pixelpilot#NA1",
            L"demo-launcher-password",
            L"pilot@example.com",
            L"Outlook",
            L"https://outlook.live.com/mail/",
            L"demo-email-password"));
        static_cast<void>(m_repository.add(
            L"Epic",
            L"orbit_runner",
            L"demo-launcher-password",
            L"orbit@example.com",
            L"Yahoo",
            L"https://mail.yahoo.com/",
            L"demo-email-password"));

        m_windowReady = true;
        applyPreset(startupThemeIndex);
    }
    void MainWindow::AddAccountButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        showAddAccountDialog();
    }
    void MainWindow::SearchBox_TextChanged(
        IInspectable const&,
        TextChangedEventArgs const&)
    {
        if (m_windowReady)
        {
            refreshAccounts();
        }
    }
    void MainWindow::LauncherFilter_SelectionChanged(
        IInspectable const&,
        SelectionChangedEventArgs const&)
    {
        if (m_windowReady)
        {
            refreshAccounts();
        }
    }
    void MainWindow::ThemePicker_SelectionChanged(
        IInspectable const&,
        SelectionChangedEventArgs const&)
    {
        if (m_windowReady)
        {
            applyPreset(ThemePicker().SelectedIndex());
        }
    }
    void MainWindow::ThemeOption_RightTapped(
        IInspectable const&,
        RightTappedRoutedEventArgs const& args)
    {
        // PointerPressed owns right-click handling so the ComboBox template
        // cannot consume the event before the menu opens.
        args.Handled(true);
    }
    void MainWindow::CustomizeColorsButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        showColorDialog();
    }
    void MainWindow::refreshAccounts()
    {
        AccountsList().Items().Clear();

        const std::wstring query{ SearchBox().Text().c_str() };
        const std::wstring launcher{ selectedLauncher() };
        const auto matches{ m_repository.search(query, launcher) };

        for (Account const* account : matches)
        {
            appendAccountCard(*account);
        }

        EmptyState().Visibility(
            matches.empty() ? Visibility::Visible : Visibility::Collapsed);

        std::wstring status{ std::to_wstring(matches.size()) };
        status += matches.size() == 1 ? L" account shown" : L" accounts shown";
        status += L"  |  in-memory demo";
        StatusText().Text(hstring{ status });
    }
    std::wstring MainWindow::selectedLauncher()
    {
        const int selectedIndex{ LauncherFilter().SelectedIndex() };

        if (selectedIndex <= 0)
        {
            return {};
        }

        const auto item = LauncherFilter()
            .SelectedItem()
            .as<ComboBoxItem>();
        return unbox_value<hstring>(item.Content()).c_str();
    }
}
