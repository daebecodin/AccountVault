#include "pch.h"
// Account Armory automatic-lock compile fix v28.7.3.
#include "MainWindow.xaml.h"
#include "Security/SensitiveData.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.Windows.System.Power.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Storage.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Input;
using namespace Microsoft::Windows::System::Power;
using namespace Windows::ApplicationModel::DataTransfer;
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

        // Drive the shell from the measured layout width. Full rails require
        // at least 1440 effective pixels; narrower windows use the centered
        // workspace and its compact account-actions dropdown.
        const auto shellWeak{ get_weak() };
        AdaptiveHost().SizeChanged(
            [shellWeak](
                IInspectable const&,
                SizeChangedEventArgs const& args)
            {
                if (const auto self{ shellWeak.get() })
                {
                    self->updateWindowDimensions(
                        args.NewSize().Width,
                        args.NewSize().Height);
                    self->updateShellLayout(args.NewSize().Width);
                }
            });
        AdaptiveHost().Loaded(
            [shellWeak](
                IInspectable const&,
                RoutedEventArgs const&)
            {
                if (const auto self{ shellWeak.get() })
                {
                    const double width{ self->AdaptiveHost().ActualWidth() };
                    const double height{ self->AdaptiveHost().ActualHeight() };
                    self->updateWindowDimensions(width, height);
                    self->updateShellLayout(width);
                }
            });

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
                    auto launcherPassword{
                        m_credentials.legacyLauncherPassword(account.recordId) };
                    auto emailPassword{
                        m_credentials.legacyEmailPassword(account.recordId) };
                    auto wipeLauncherPassword{
                        account_vault::security::wipeOnExit(
                            launcherPassword) };
                    auto wipeEmailPassword{
                        account_vault::security::wipeOnExit(emailPassword) };

                    if (!launcherPassword || !emailPassword)
                    {
                        migrationSucceeded = false;
                        break;
                    }

                    const auto protectedLauncherPassword{
                        m_credentials.protectPassword(*launcherPassword) };
                    const auto protectedEmailPassword{
                        m_credentials.protectPassword(*emailPassword) };
                    account_vault::security::wipe(launcherPassword);
                    account_vault::security::wipe(emailPassword);

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

        // Storage and UI must begin from the same filtered account set before
        // any incremental create/edit/remove operation calculates an index.
        if (loadResult.succeeded)
        {
            refreshAccounts();
        }

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

        initializeAutoLock();
    }
    MainWindow::~MainWindow()
    {
        try
        {
            if (m_autoLockTimer)
            {
                m_autoLockTimer.Stop();
            }
        }
        catch (...)
        {
        }

        // The delay coroutine cannot run after process exit. Apply the same
        // sequence guard used by auto-lock so newer user clipboard content is
        // never erased.
        try
        {
            if (m_accountClipboardSequence != 0 &&
                ::GetClipboardSequenceNumber() == m_accountClipboardSequence)
            {
                Clipboard::Clear();
            }
            m_accountClipboardSequence = 0;
        }
        catch (...)
        {
            m_accountClipboardSequence = 0;
        }

        try
        {
            if (m_appWindow && m_appWindowChangedToken)
            {
                m_appWindow.Changed(m_appWindowChangedToken);
            }
        }
        catch (...)
        {
        }

        try
        {
            if (m_suspendStatusChangedToken)
            {
                PowerManager::SystemSuspendStatusChanged(
                    m_suspendStatusChangedToken);
            }
        }
        catch (...)
        {
        }
    }

    void MainWindow::applyDefaultWindowSize() noexcept
    {
        try
        {
            if (!m_appWindow)
            {
                return;
            }

            const auto displayArea{
                Microsoft::UI::Windowing::DisplayArea::GetFromWindowId(
                    m_appWindow.Id(),
                    Microsoft::UI::Windowing::DisplayAreaFallback::Primary) };

            if (!displayArea)
            {
                return;
            }

            const auto workArea{ displayArea.WorkArea() };
            if (workArea.Width <= 0 || workArea.Height <= 0)
            {
                return;
            }

            const double aspectRatio{
                static_cast<double>(workArea.Width) /
                static_cast<double>(workArea.Height) };

            const bool isUltrawide{
                workArea.Width >= UltrawideStartupWidth &&
                aspectRatio >= UltrawideAspectRatioThreshold };

            const std::int32_t preferredWidth{
                isUltrawide ? UltrawideStartupWidth : RegularStartupWidth };
            const std::int32_t preferredHeight{
                isUltrawide ? UltrawideStartupHeight : RegularStartupHeight };

            const std::int32_t availableWidth{
                std::max<std::int32_t>(
                    1,
                    workArea.Width - (2 * StartupEdgeMargin)) };
            const std::int32_t availableHeight{
                std::max<std::int32_t>(
                    1,
                    workArea.Height - (2 * StartupEdgeMargin)) };

            const std::int32_t width{
                std::min<std::int32_t>(preferredWidth, availableWidth) };
            const std::int32_t height{
                std::min<std::int32_t>(preferredHeight, availableHeight) };

            const std::int32_t x{
                workArea.X + ((workArea.Width - width) / 2) };
            const std::int32_t y{
                workArea.Y + ((workArea.Height - height) / 2) };

            m_appWindow.MoveAndResize(
                Windows::Graphics::RectInt32{
                    x,
                    y,
                    width,
                    height },
                    displayArea);
        }
        catch (...)
        {
            // Keep the platform-selected bounds if display discovery fails.
        }
    }

    void MainWindow::updateWindowDimensions(
        double width,
        double height) noexcept
    {
        try
        {
            const auto displayedWidth{
                static_cast<std::int32_t>(width + 0.5) };
            const auto displayedHeight{
                static_cast<std::int32_t>(height + 0.5) };

            if (displayedWidth == m_displayedWindowWidth &&
                displayedHeight == m_displayedWindowHeight)
            {
                return;
            }

            std::wstring text{ std::to_wstring(displayedWidth) };
            text += L" x ";
            text += std::to_wstring(displayedHeight);

            WindowSizeText().Text(hstring{ text });
            m_displayedWindowWidth = displayedWidth;
            m_displayedWindowHeight = displayedHeight;
        }
        catch (...)
        {
        }
    }

    void MainWindow::updateShellLayout(double width) noexcept
    {
        ShellLayout nextLayout{ ShellLayout::Compact };
        if (width >= WideShellMinWidth)
        {
            nextLayout = ShellLayout::Wide;
        }
        else if (width >= CenterOnlyShellMinWidth)
        {
            nextLayout = ShellLayout::CenterOnly;
        }

        if (nextLayout == m_shellLayout)
        {
            return;
        }

        try
        {
            // Keep the visual-state group deterministic: one active trigger,
            // with no competing MinWindowWidth matches.
            WideShellTrigger().IsActive(false);
            CenterOnlyShellTrigger().IsActive(false);
            CompactShellTrigger().IsActive(false);

            switch (nextLayout)
            {
            case ShellLayout::Wide:
                WideShellTrigger().IsActive(true);
                break;
            case ShellLayout::CenterOnly:
                CenterOnlyShellTrigger().IsActive(true);
                break;
            case ShellLayout::Compact:
                CompactShellTrigger().IsActive(true);
                break;
            default:
                return;
            }

            const bool showRails{ nextLayout == ShellLayout::Wide };

            if (showRails)
            {
                CompactNavigation().IsPaneOpen(false);
            }

            CompactNavigation().CompactPaneLength(
                showRails ? 0.0 : 48.0);

            CompactNavigation().IsPaneToggleButtonVisible(!showRails);

            const double shellMargin{
                nextLayout == ShellLayout::Wide ? 24.0 :
                nextLayout == ShellLayout::Compact ? 12.0 : 18.0
            };

            ShellFrame().Margin(
                Thickness{
                    shellMargin,
                    shellMargin,
                    shellMargin,
                    shellMargin });

            RootGrid().Padding(Thickness{ 0.0, 0.0, 0.0, 0.0 });

            LeftRail().Visibility(
                showRails ? Visibility::Visible : Visibility::Collapsed);

            RightRail().Visibility(
                showRails ? Visibility::Visible : Visibility::Collapsed);

            AccountActionsButton().Visibility(
                showRails ? Visibility::Collapsed : Visibility::Visible);

            LeftRailColumn().Width(
                GridLengthHelper::FromPixels(
                    showRails ? 220.0 : 0.0));

            RightRailColumn().Width(
                GridLengthHelper::FromPixels(
                    showRails ? 240.0 : 0.0));

            RootGrid().ColumnSpacing(
                showRails ? 20.0 : 0.0);

            m_shellLayout = nextLayout;
        }
        catch (...)
        {
            // Permit a later SizeChanged/Loaded event to retry initialization.
            m_shellLayout = ShellLayout::Unknown;
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
    void MainWindow::UnlockButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        unlockApplication();
    }
    void MainWindow::initializeAutoLock()
    {
        try
        {
            const auto weak{ get_weak() };
            const auto dispatcher{ DispatcherQueue() };

            m_autoLockTimer = dispatcher.CreateTimer();
            m_autoLockTimer.Interval(std::chrono::seconds{ 1 });
            m_autoLockTimer.IsRepeating(true);
            m_autoLockTimer.Tick(
                [weak](
                    Microsoft::UI::Dispatching::DispatcherQueueTimer const&,
                    IInspectable const&)
                {
                    if (const auto self{ weak.get() })
                    {
                        self->updateAutoLockStatus();
                    }
                });

            const auto recordPointerActivity = [weak](
                IInspectable const&,
                PointerRoutedEventArgs const&)
                {
                    if (const auto self{ weak.get() })
                    {
                        self->noteUserActivity();
                    }
                };

            RootGrid().AddHandler(
                UIElement::PointerMovedEvent(),
                box_value(PointerEventHandler{ recordPointerActivity }),
                true);
            RootGrid().AddHandler(
                UIElement::PointerPressedEvent(),
                box_value(PointerEventHandler{ recordPointerActivity }),
                true);
            RootGrid().AddHandler(
                UIElement::PointerWheelChangedEvent(),
                box_value(PointerEventHandler{ recordPointerActivity }),
                true);

            RootGrid().AddHandler(
                UIElement::KeyDownEvent(),
                box_value(KeyEventHandler{
                    [weak](IInspectable const&, KeyRoutedEventArgs const&)
                    {
                        if (const auto self{ weak.get() })
                        {
                            self->noteUserActivity();
                        }
                    } }),
                true);

            m_appWindow = this->AppWindow();
            applyDefaultWindowSize();
            m_appWindowChangedToken = m_appWindow.Changed(
                [weak, dispatcher](
                    Microsoft::UI::Windowing::AppWindow const&,
                    Microsoft::UI::Windowing::AppWindowChangedEventArgs const& args)
                {
                    try
                    {
                        if (!args.DidPresenterChange())
                        {
                            return;
                        }

                        static_cast<void>(dispatcher.TryEnqueue([weak]()
                            {
                                try
                                {
                                    if (const auto self{ weak.get() })
                                    {
                                        const auto presenter{ self->m_appWindow
                                            .Presenter()
                                            .try_as<Microsoft::UI::Windowing::OverlappedPresenter>() };
                                        if (presenter &&
                                            presenter.State() ==
                                            Microsoft::UI::Windowing::OverlappedPresenterState::Minimized)
                                        {
                                            self->lockApplication(
                                                L"the window was minimized");
                                        }
                                    }
                                }
                                catch (...)
                                {
                                }
                            }));
                    }
                    catch (...)
                    {
                    }
                });

            m_suspendStatusChangedToken =
                PowerManager::SystemSuspendStatusChanged(
                    [weak, dispatcher](
                        IInspectable const&,
                        IInspectable const&)
                    {
                        try
                        {
                            if (PowerManager::SystemSuspendStatus() !=
                                SystemSuspendStatus::Entering)
                            {
                                return;
                            }

                            static_cast<void>(dispatcher.TryEnqueue([weak]()
                                {
                                    if (const auto self{ weak.get() })
                                    {
                                        self->lockApplication(
                                            L"Windows entered suspend");
                                    }
                                }));
                        }
                        catch (...)
                        {
                        }
                    });

            m_autoLockDeadline = std::chrono::steady_clock::now() +
                std::chrono::seconds{ AutoLockTimeoutSeconds };
            m_autoLockTimer.Start();
            updateAutoLockStatus();
        }
        catch (...)
        {
            try
            {
                AutoLockStatusText().Text(L"AUTO-LOCK ERROR");
                StatusText().Text(
                    L"Automatic locking could not be initialized");
            }
            catch (...)
            {
            }
        }
    }
    void MainWindow::noteUserActivity() noexcept
    {
        try
        {
            if (m_isLocked)
            {
                return;
            }

            // Only move the deadline here. The one-second timer owns text
            // updates so high-frequency pointer movement stays inexpensive.
            m_autoLockDeadline = std::chrono::steady_clock::now() +
                std::chrono::seconds{ AutoLockTimeoutSeconds };
        }
        catch (...)
        {
        }
    }
    void MainWindow::updateAutoLockStatus() noexcept
    {
        try
        {
            if (m_isLocked)
            {
                AutoLockStatusText().Text(L"LOCKED");
                return;
            }

            const auto now{ std::chrono::steady_clock::now() };
            if (now >= m_autoLockDeadline)
            {
                lockApplication(L"five minutes of inactivity elapsed");
                return;
            }

            const auto remainingMilliseconds{
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    m_autoLockDeadline - now).count() };
            const auto totalSeconds{
                static_cast<int>((remainingMilliseconds + 999) / 1000) };
            const int minutes{ totalSeconds / 60 };
            const int seconds{ totalSeconds % 60 };

            std::wstring status{ L"AUTO-LOCK " };
            status += std::to_wstring(minutes);
            status += L":";
            if (seconds < 10)
            {
                status += L"0";
            }
            status += std::to_wstring(seconds);
            AutoLockStatusText().Text(hstring{ status });
        }
        catch (...)
        {
            try
            {
                AutoLockStatusText().Text(L"AUTO-LOCK ERROR");
            }
            catch (...)
            {
            }
        }
    }
    void MainWindow::lockApplication(std::wstring_view reason) noexcept
    {
        // Every lock request invalidates an unlock already awaiting Windows
        // Hello, even when the overlay was already visible.
        ++m_lockGeneration;
        if (m_isLocked)
        {
            return;
        }

        m_isLocked = true;
        m_unlockInProgress = false;

        try
        {
            CompactNavigation().IsPaneOpen(false);
        }
        catch (...)
        {
        }

        try
        {
            if (m_autoLockTimer)
            {
                m_autoLockTimer.Stop();
            }
        }
        catch (...)
        {
        }

        // Hiding every in-place dialog resumes its owning coroutine. Details
        // cleanup then stops reveal timers and clears revealed plaintext.
        try
        {
            const auto children{ RootGrid().Children() };
            std::vector<ContentDialog> openDialogs;
            for (std::uint32_t index{}; index < children.Size(); ++index)
            {
                if (const auto dialog{
                        children.GetAt(index).try_as<ContentDialog>() })
                {
                    openDialogs.push_back(dialog);
                }
            }

            // Hide from a snapshot because each dialog coroutine may remove
            // itself from RootGrid when ShowAsync completes.
            for (auto const& dialog : openDialogs)
            {
                try
                {
                    dialog.Hide();
                }
                catch (...)
                {
                }
            }
        }
        catch (...)
        {
        }

        // Clear only the latest account value this app copied. A changed
        // sequence means the user copied newer content, which is preserved.
        try
        {
            if (m_accountClipboardSequence != 0 &&
                ::GetClipboardSequenceNumber() == m_accountClipboardSequence)
            {
                Clipboard::Clear();
            }
            m_accountClipboardSequence = 0;
        }
        catch (...)
        {
            m_accountClipboardSequence = 0;
        }

        try
        {
            LockOverlay().Visibility(Visibility::Visible);
            UnlockButton().IsEnabled(true);
            UnlockButton().Content(box_value(L"Unlock"));
            AutoLockStatusText().Text(L"LOCKED");

            std::wstring status{ L"Account Armory locked: " };
            status += reason;
            StatusText().Text(hstring{ status });
            UnlockButton().Focus(FocusState::Programmatic);
        }
        catch (...)
        {
        }
    }
    fire_and_forget MainWindow::unlockApplication()
    {
        Button unlockButton{ nullptr };

        try
        {
            auto lifetime{ get_strong() };
            if (!m_isLocked || m_unlockInProgress)
            {
                co_return;
            }

            m_unlockInProgress = true;
            const std::uint64_t unlockGeneration{ m_lockGeneration };
            unlockButton = UnlockButton();
            unlockButton.IsEnabled(false);
            unlockButton.Content(box_value(L"Verifying..."));

            const bool verified{ co_await verifyUser(
                L"Verify your identity to unlock Account Armory") };
            if (verified &&
                m_isLocked &&
                m_lockGeneration == unlockGeneration)
            {
                m_isLocked = false;
                LockOverlay().Visibility(Visibility::Collapsed);
                m_autoLockDeadline = std::chrono::steady_clock::now() +
                    std::chrono::seconds{ AutoLockTimeoutSeconds };
                m_autoLockTimer.Start();
                updateAutoLockStatus();
                StatusText().Text(L"Account Armory unlocked");
            }
        }
        catch (...)
        {
            try
            {
                StatusText().Text(L"Account Armory could not be unlocked");
            }
            catch (...)
            {
            }
        }

        m_unlockInProgress = false;
        try
        {
            if (unlockButton)
            {
                unlockButton.IsEnabled(true);
                unlockButton.Content(box_value(L"Unlock"));
            }
        }
        catch (...)
        {
        }
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
