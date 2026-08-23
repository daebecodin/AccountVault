#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Windows.Storage.h>

#include <string>
#include <vector>

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

        const auto loadResult{ m_accountStorage.load() };
        std::wstring storageStatus;
        m_storageReady = loadResult.succeeded;
        if (loadResult.succeeded)
        {
            m_repository.replaceAll(
                loadResult.accounts,
                loadResult.nextRecordId);

            if (loadResult.credentialMigrationRequired)
            {
                bool migrationSucceeded{ true };
                std::vector<RecordId> migratedIds;
                migratedIds.reserve(loadResult.accounts.size());

                for (Account const& account : loadResult.accounts)
                {
                    const auto launcherPassword{
                        m_credentials.legacyLauncherPassword(account.recordId) };
                    const auto emailPassword{
                        m_credentials.legacyEmailPassword(account.recordId) };

                    if (!launcherPassword || !emailPassword)
                    {
                        migrationSucceeded = false;
                        break;
                    }

                    const auto protectedLauncherPassword{
                        m_credentials.protectPassword(*launcherPassword) };
                    const auto protectedEmailPassword{
                        m_credentials.protectPassword(*emailPassword) };

                    if (!protectedLauncherPassword ||
                        !protectedEmailPassword ||
                        !m_repository.updateProtectedPasswords(
                            account.recordId,
                            *protectedLauncherPassword,
                            *protectedEmailPassword))
                    {
                        migrationSucceeded = false;
                        break;
                    }

                    migratedIds.push_back(account.recordId);
                }

                std::wstring migrationError;
                if (migrationSucceeded && persistAccounts(migrationError))
                {
                    for (RecordId id : migratedIds)
                    {
                        static_cast<void>(
                            m_credentials.removeLegacyAccountSecrets(id));
                    }
                    storageStatus =
                        L"Credentials migrated to fast DPAPI-protected storage";
                }
                else
                {
                    m_repository.replaceAll(
                        loadResult.accounts,
                        loadResult.nextRecordId);
                    m_storageReady = false;
                    storageStatus =
                        L"Credential migration could not be completed; storage changes are disabled";
                }
            }
        }

        m_windowReady = true;
        applyPreset(startupThemeIndex);

        if (!loadResult.succeeded)
        {
            std::wstring status{ L"Account data could not be loaded: " };
            status += loadResult.error;
            StatusText().Text(hstring{ status });
        }
        else if (!storageStatus.empty())
        {
            StatusText().Text(hstring{ storageStatus });
        }
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
        status += L"  |  passwords protected locally with Windows DPAPI";
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
