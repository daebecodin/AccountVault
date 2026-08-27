#include "pch.h"
// Account Armory automatic-lock compile fix v28.7.3.
#include "MainWindow.xaml.h"
#include "Security/SensitiveData.h"
#include "Services/CredentialCategoryCatalog.h"
#include "Services/LauncherCatalog.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <microsoft.ui.xaml.window.h>
#include <commctrl.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.Windows.System.Power.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Storage.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#pragma comment(lib, "Comctl32.lib")

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Automation;
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
    constexpr wchar_t AutoLockTimeoutSettingKey[]{ L"AutoLockTimeoutSeconds" };
    constexpr wchar_t GettingStartedVersionSettingKey[]{
        L"GettingStartedTourVersion" };
}

namespace winrt::AccountVault::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        // Modeless tool-window coroutines intentionally keep MainWindow alive
        // while they are open. Close those windows as soon as the shell closes
        // so each coroutine can run its plaintext cleanup and release its
        // strong reference; waiting for the destructor would be too late.
        Closed([this](IInspectable const&, WindowEventArgs const&)
        {
            m_windowReady = false;
            closeModelessWindows();
        });

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
                    self->showGettingStartedIfNeeded();
                }
            });

        GettingStartedTip().ActionButtonClick(
            [shellWeak](TeachingTip const&, IInspectable const&)
            {
                if (const auto self{ shellWeak.get() })
                {
                    const int nextStep{ self->m_gettingStartedStep + 1 };
                    if (nextStep >= self->GettingStartedStepCount)
                    {
                        self->finishGettingStartedTour(true);
                    }
                    else
                    {
                        self->showGettingStartedStep(nextStep);
                    }
                }
            });

        GettingStartedTip().CloseButtonClick(
            [shellWeak](TeachingTip const&, IInspectable const&)
            {
                if (const auto self{ shellWeak.get() })
                {
                    self->finishGettingStartedTour(true);
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

        applyPreset(startupThemeIndex);
        switchWorkspace(WorkspaceSection::LauncherAccounts);
        m_windowReady = true;

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
        closeModelessWindows();

        if (m_windowHandle && ::IsWindow(m_windowHandle))
        {
            ::RemoveWindowSubclass(
                m_windowHandle,
                &MainWindow::windowSubclassProc,
                reinterpret_cast<UINT_PTR>(this));
        }
        m_windowHandle = nullptr;

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

    void MainWindow::installWindowSizeConstraints() noexcept
    {
        try
        {
            HWND windowHandle{};
            check_hresult(
                m_inner.as<::IWindowNative>()->get_WindowHandle(&windowHandle));

            if (!windowHandle)
            {
                return;
            }

            if (::SetWindowSubclass(
                    windowHandle,
                    &MainWindow::windowSubclassProc,
                    reinterpret_cast<UINT_PTR>(this),
                    reinterpret_cast<DWORD_PTR>(this)))
            {
                m_windowHandle = windowHandle;
            }
        }
        catch (...)
        {
            // Retain normal Windows sizing behavior if subclassing is unavailable.
        }
    }

    void MainWindow::applyWindowSizeConstraints(
        MINMAXINFO& limits) const noexcept
    {
        if (!m_windowHandle)
        {
            return;
        }

        const UINT dpi{ ::GetDpiForWindow(m_windowHandle) };
        const DWORD style{
            static_cast<DWORD>(::GetWindowLongPtrW(m_windowHandle, GWL_STYLE)) };
        const DWORD extendedStyle{
            static_cast<DWORD>(::GetWindowLongPtrW(m_windowHandle, GWL_EXSTYLE)) };

        const auto outerSizeForClient =
            [dpi, style, extendedStyle](std::int32_t width, std::int32_t height)
            {
                RECT bounds{
                    0,
                    0,
                    ::MulDiv(width, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI),
                    ::MulDiv(height, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI) };

                static_cast<void>(::AdjustWindowRectExForDpi(
                    &bounds,
                    style,
                    FALSE,
                    extendedStyle,
                    dpi));

                return SIZE{
                    bounds.right - bounds.left,
                    bounds.bottom - bounds.top };
            };

        const SIZE minimum{
            outerSizeForClient(MinimumClientWidth, MinimumClientHeight) };
        const SIZE maximum{
            outerSizeForClient(MaximumClientWidth, MaximumClientHeight) };

        // A logical 945-DIP minimum is taller than the available work area on
        // a 1920x1080 display at 150% scaling. Never advertise a track size
        // larger than the current monitor's work area or Windows can create a
        // clipped window whose lower controls cannot be reached.
        RECT workArea{};
        bool hasWorkArea{ false };
        const HMONITOR monitor{
            ::MonitorFromWindow(m_windowHandle, MONITOR_DEFAULTTONEAREST) };
        if (monitor)
        {
            MONITORINFO monitorInfo{ sizeof(MONITORINFO) };
            if (::GetMonitorInfoW(monitor, &monitorInfo))
            {
                workArea = monitorInfo.rcWork;
                hasWorkArea = true;
            }
        }

        const LONG availableWidth{ hasWorkArea
            ? (std::max)(1L, workArea.right - workArea.left)
            : maximum.cx };
        const LONG availableHeight{ hasWorkArea
            ? (std::max)(1L, workArea.bottom - workArea.top)
            : maximum.cy };
        const LONG minimumWidth{
            (std::min)(static_cast<LONG>(minimum.cx), availableWidth) };
        const LONG minimumHeight{
            (std::min)(static_cast<LONG>(minimum.cy), availableHeight) };
        const LONG maximumWidth{
            (std::max)(
                minimumWidth,
                (std::min)(
                    static_cast<LONG>(maximum.cx),
                    availableWidth)) };
        const LONG maximumHeight{
            (std::max)(
                minimumHeight,
                (std::min)(
                    static_cast<LONG>(maximum.cy),
                    availableHeight)) };

        limits.ptMinTrackSize.x = minimumWidth;
        limits.ptMinTrackSize.y = minimumHeight;
        limits.ptMaxTrackSize.x = maximumWidth;
        limits.ptMaxTrackSize.y = maximumHeight;
        limits.ptMaxSize.x = (std::min)(limits.ptMaxSize.x, maximumWidth);
        limits.ptMaxSize.y = (std::min)(limits.ptMaxSize.y, maximumHeight);
    }

    LRESULT CALLBACK MainWindow::windowSubclassProc(
        HWND windowHandle,
        UINT message,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR subclassId,
        DWORD_PTR referenceData) noexcept
    {
        auto* self{ reinterpret_cast<MainWindow*>(referenceData) };

        if (self && message == WM_GETMINMAXINFO)
        {
            const LRESULT result{
                ::DefSubclassProc(windowHandle, message, wParam, lParam) };
            self->applyWindowSizeConstraints(
                *reinterpret_cast<MINMAXINFO*>(lParam));
            return result;
        }

        if (self && message == WM_NCDESTROY)
        {
            ::RemoveWindowSubclass(
                windowHandle,
                &MainWindow::windowSubclassProc,
                subclassId);
            self->m_windowHandle = nullptr;
        }

        return ::DefSubclassProc(windowHandle, message, wParam, lParam);
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
                (std::max)(
                    1,
                    workArea.Width - (2 * StartupEdgeMargin)) };
            const std::int32_t availableHeight{
                (std::max)(
                    1,
                    workArea.Height - (2 * StartupEdgeMargin)) };

            const std::int32_t width{
                (std::min)(
                    preferredWidth,
                    availableWidth) };
            const std::int32_t height{
                (std::min)(
                    preferredHeight,
                    availableHeight) };

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

            UtilitiesButton().Visibility(
                showRails ? Visibility::Collapsed : Visibility::Visible);

            TopAddAccountButton().Visibility(
                showRails ? Visibility::Visible : Visibility::Collapsed);

            TopMoreActionsButton().Visibility(
                showRails ? Visibility::Visible : Visibility::Collapsed);

            // Keep this action visible at every window size. Its local XAML
            // values otherwise win over the responsive-state setters.
            TopRemoveVisibleButton().Visibility(Visibility::Visible);
            Grid::SetRow(
                TopRemoveVisibleButton(),
                nextLayout == ShellLayout::Compact ? 1 : 0);
            Grid::SetColumn(
                TopRemoveVisibleButton(),
                nextLayout == ShellLayout::Wide
                    ? 4
                    : nextLayout == ShellLayout::CenterOnly ? 2 : 1);
            Grid::SetColumnSpan(TopRemoveVisibleButton(), 1);

            LeftRailColumn().Width(
                GridLengthHelper::FromPixels(
                    showRails ? 220.0 : 0.0));

            RightRailColumn().Width(
                GridLengthHelper::FromPixels(
                    showRails ? 240.0 : 0.0));

            RootGrid().ColumnSpacing(
                showRails ? 20.0 : 0.0);

            m_shellLayout = nextLayout;

            if (m_gettingStartedActive && m_gettingStartedStep >= 0)
            {
                showGettingStartedStep(m_gettingStartedStep);
            }
        }
        catch (...)
        {
            // Permit a later SizeChanged/Loaded event to retry initialization.
            m_shellLayout = ShellLayout::Unknown;
        }
    }

    void MainWindow::attachDialogToShell(ContentDialog const& dialog)
    {
        const Grid root{ RootGrid() };

        // ContentDialog and its generated action buttons normally consume the
        // operating-system control palette. Override those tokens locally so
        // every add/details dialog follows Account Armory's active theme while
        // retaining WinUI's native pointer-over/pressed transitions.
        const auto applicationResources{ Application::Current().Resources() };
        const auto dialogResources{ dialog.Resources() };
        const auto aliasBrush = [&applicationResources, &dialogResources](
            wchar_t const* targetKey,
            wchar_t const* sourceKey)
        {
            dialogResources.Insert(
                box_value(targetKey),
                applicationResources.Lookup(box_value(sourceKey)));
        };

        aliasBrush(L"ContentDialogBackground", L"AppSurfaceBrush");
        aliasBrush(L"ContentDialogForeground", L"AppTextBrush");
        aliasBrush(L"ContentDialogBorderBrush", L"AppBorderBrush");
        aliasBrush(L"ContentDialogSeparatorBorderBrush", L"AppBorderBrush");

        aliasBrush(L"AccentButtonBackground", L"AppAccentBrush");
        aliasBrush(L"AccentButtonBackgroundPointerOver", L"AppAccentHoverBrush");
        aliasBrush(L"AccentButtonBackgroundPressed", L"AppAccentPressedBrush");
        aliasBrush(L"AccentButtonBorderBrush", L"AppAccentBrush");
        aliasBrush(L"AccentButtonBorderBrushPointerOver", L"AppAccentHoverBrush");
        aliasBrush(L"AccentButtonBorderBrushPressed", L"AppAccentPressedBrush");
        aliasBrush(L"AccentButtonForeground", L"AppBackgroundBrush");
        aliasBrush(L"AccentButtonForegroundPointerOver", L"AppBackgroundBrush");
        aliasBrush(L"AccentButtonForegroundPressed", L"AppBackgroundBrush");

        aliasBrush(L"ButtonBackground", L"AppSurfaceAltBrush");
        aliasBrush(L"ButtonBackgroundPointerOver", L"AppAccentLowBrush");
        aliasBrush(L"ButtonBackgroundPressed", L"AppAccentMediumBrush");
        aliasBrush(L"ButtonBorderBrush", L"AppBorderBrush");
        aliasBrush(L"ButtonBorderBrushPointerOver", L"AppAccentBrush");
        aliasBrush(L"ButtonBorderBrushPressed", L"AppAccentBrush");
        aliasBrush(L"ButtonForeground", L"AppTextBrush");
        aliasBrush(L"ButtonForegroundPointerOver", L"AppTextBrush");
        aliasBrush(L"ButtonForegroundPressed", L"AppTextBrush");

        // Current WinUI templates consume these semantic tokens for text,
        // password, combo-box, and dialog surfaces.
        aliasBrush(L"ControlFillColorDefaultBrush", L"AppSurfaceAltBrush");
        aliasBrush(L"ControlFillColorSecondaryBrush", L"AppAccentLowBrush");
        aliasBrush(L"ControlFillColorTertiaryBrush", L"AppAccentMediumBrush");
        aliasBrush(L"ControlStrokeColorDefaultBrush", L"AppBorderBrush");
        aliasBrush(L"ControlStrokeColorSecondaryBrush", L"AppAccentBrush");
        aliasBrush(L"TextFillColorPrimaryBrush", L"AppTextBrush");
        aliasBrush(L"TextFillColorSecondaryBrush", L"AppMutedTextBrush");
        aliasBrush(L"AccentFillColorDefaultBrush", L"AppAccentBrush");
        aliasBrush(L"AccentFillColorSecondaryBrush", L"AppAccentHoverBrush");
        aliasBrush(L"AccentFillColorTertiaryBrush", L"AppAccentPressedBrush");

        // In-place ContentDialogs participate in their parent's Grid layout.
        // Span the live shell definition counts instead of preserving a magic
        // row/column count from an earlier RootGrid shape.
        Grid::SetRow(dialog, 0);
        Grid::SetColumn(dialog, 0);
        Grid::SetRowSpan(
            dialog,
            static_cast<std::int32_t>(root.RowDefinitions().Size()));
        Grid::SetColumnSpan(
            dialog,
            static_cast<std::int32_t>(root.ColumnDefinitions().Size()));

        // ContentDialogs are reserved for security and destructive flows.
        // Disable every ordinary tool window for the duration so the prompt
        // remains a true app-wide modal boundary.
        setModelessWindowsInteraction(false);
        try
        {
            root.Children().Append(dialog);
        }
        catch (...)
        {
            setModelessWindowsInteraction(true);
            throw;
        }
    }

    void MainWindow::detachDialogFromShell(
        ContentDialog const& dialog) noexcept
    {
        try
        {
            const auto children{ RootGrid().Children() };
            std::uint32_t index{};
            if (children.IndexOf(dialog, index))
            {
                children.RemoveAt(index);
            }
        }
        catch (...)
        {
        }

        setModelessWindowsInteraction(true);
    }

    void MainWindow::attachDialogToShell(
        account_vault::ui::ModelessToolWindow const& window)
    {
        if (window)
        {
            HWND ownerWindow{};
            if (SUCCEEDED(
                    m_inner.as<::IWindowNative>()->get_WindowHandle(
                        &ownerWindow)))
            {
                window.OwnerWindowHandle(
                    reinterpret_cast<std::intptr_t>(ownerWindow));
            }
            m_modelessWindows.push_back(window);
        }
    }

    void MainWindow::detachModelessWindow(
        account_vault::ui::ModelessToolWindow const& window) noexcept
    {
        try
        {
            const auto id{ window.Id() };
            m_modelessWindows.erase(
                std::remove_if(
                    m_modelessWindows.begin(),
                    m_modelessWindows.end(),
                    [id](account_vault::ui::ModelessToolWindow const& candidate)
                    {
                        return candidate.Id() == id;
                    }),
                m_modelessWindows.end());
        }
        catch (...)
        {
        }
    }

    void MainWindow::closeModelessWindows() noexcept
    {
        try
        {
            const auto windows{ m_modelessWindows };
            for (auto const& window : windows)
            {
                window.Close();
            }
        }
        catch (...)
        {
        }
    }

    void MainWindow::setModelessWindowsInteraction(bool enabled) noexcept
    {
        try
        {
            for (auto const& window : m_modelessWindows)
            {
                window.IsInteractionEnabled(enabled);
            }
        }
        catch (...)
        {
        }
    }

    void MainWindow::AddAccountButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        if (m_workspaceSection == WorkspaceSection::CredentialVault)
        {
            showAddCredentialDialog();
        }
        else
        {
            showAddAccountDialog();
        }
    }
    void MainWindow::RemoveVisibleButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        showRemoveVisibleConfirmation();
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
    void MainWindow::RecordFilter_SelectionChanged(
        IInspectable const&,
        SelectionChangedEventArgs const&)
    {
        if (m_windowReady && !m_updatingRecordFilter)
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
    void MainWindow::CompactThemeMenuItem_Click(
        IInspectable const& sender,
        RoutedEventArgs const&)
    {
        try
        {
            const auto item{ sender.as<MenuFlyoutItem>() };
            const auto tag{ unbox_value<hstring>(item.Tag()) };
            if (tag.size() != 1 || tag[0] < L'0' || tag[0] > L'9')
            {
                return;
            }

            const int selectedIndex{ static_cast<int>(tag[0] - L'0') };
            if (ThemePicker().SelectedIndex() == selectedIndex)
            {
                applyPreset(selectedIndex);
            }
            else
            {
                ThemePicker().SelectedIndex(selectedIndex);
            }
        }
        catch (...)
        {
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
    void MainWindow::PasswordGeneratorButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        showPasswordGenerator();
    }
    void MainWindow::AutoLockButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        showAutoLockDialog();
    }
    void MainWindow::HelpButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        startGettingStartedTour();
    }
    void MainWindow::UnlockButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        unlockApplication();
    }
    void MainWindow::showGettingStartedIfNeeded() noexcept
    {
        if (m_gettingStartedChecked)
        {
            return;
        }

        m_gettingStartedChecked = true;

        try
        {
            const auto values{
                ApplicationData::Current().LocalSettings().Values() };
            if (values.HasKey(GettingStartedVersionSettingKey))
            {
                const auto completedVersion{ unbox_value<std::int32_t>(
                    values.Lookup(GettingStartedVersionSettingKey)) };
                if (completedVersion >= GettingStartedTourVersion)
                {
                    return;
                }
            }
        }
        catch (...)
        {
            // A settings read should never keep the rest of the app from
            // loading. If package settings are unavailable, show the tour for
            // this session and simply skip persistence.
        }

        try
        {
            startGettingStartedTour();
        }
        catch (...)
        {
        }
    }
    void MainWindow::startGettingStartedTour()
    {
        if (m_isLocked)
        {
            return;
        }

        m_gettingStartedActive = true;
        showGettingStartedStep(0);
    }
    void MainWindow::showGettingStartedStep(int step)
    {
        if (!m_gettingStartedActive ||
            step < 0 ||
            step >= GettingStartedStepCount)
        {
            return;
        }

        const bool wideLayout{ m_shellLayout == ShellLayout::Wide };
        const auto tip{ GettingStartedTip() };

        m_gettingStartedStep = step;
        tip.ActionButtonContent(box_value(
            step + 1 == GettingStartedStepCount ? L"Done" : L"Next"));
        tip.CloseButtonContent(box_value(
            step + 1 == GettingStartedStepCount ? L"Close" : L"Skip"));

        switch (step)
        {
        case 0:
            tip.Title(L"Welcome to Account Armory");
            tip.Subtitle(
                L"Launcher Vault keeps linked launcher and email logins. "
                L"Credential Vault keeps website and app credentials. "
                L"Switch between them from Navigation.");
            tip.Target(HeaderTitle());
            tip.PreferredPlacement(TeachingTipPlacementMode::Bottom);
            break;
        case 1:
            tip.Title(L"Add or import records");
            tip.Subtitle(
                L"Add records individually, or open Credential Vault and "
                L"choose Import browser CSV for Chrome, Edge, Firefox, and "
                L"compatible browser exports.");
            tip.Target(
                wideLayout
                    ? TopAddAccountButton().as<FrameworkElement>()
                    : AccountActionsButton().as<FrameworkElement>());
            tip.PreferredPlacement(TeachingTipPlacementMode::Bottom);
            break;
        case 2:
            tip.Title(L"Back up carefully");
            tip.Subtitle(
                L"Use Import / Export for encrypted vault backups. Keep the "
                L"recovery password safe: a forgotten recovery password "
                L"cannot be reset or recovered.");
            tip.Target(
                wideLayout
                    ? TopMoreActionsButton().as<FrameworkElement>()
                    : AccountActionsButton().as<FrameworkElement>());
            tip.PreferredPlacement(TeachingTipPlacementMode::Bottom);
            break;
        case 3:
            tip.Title(L"Security and utilities");
            tip.Subtitle(
                L"Passwords stay on this Windows account and are protected "
                L"with Windows DPAPI. Account Armory can auto-lock and uses "
                L"Windows Hello to unlock. Utilities also includes the "
                L"password generator, themes, and this walkthrough. "
                L"Account Armory 1.0.");
            tip.Target(
                wideLayout
                    ? HelpUtilityButton().as<FrameworkElement>()
                    : UtilitiesButton().as<FrameworkElement>());
            tip.PreferredPlacement(
                wideLayout
                    ? TeachingTipPlacementMode::Left
                    : TeachingTipPlacementMode::Bottom);
            break;
        default:
            return;
        }

        if (!tip.IsOpen())
        {
            tip.IsOpen(true);
        }
    }
    void MainWindow::finishGettingStartedTour(
        bool rememberCompletion) noexcept
    {
        try
        {
            GettingStartedTip().IsOpen(false);
        }
        catch (...)
        {
        }

        m_gettingStartedActive = false;
        m_gettingStartedStep = -1;

        if (!rememberCompletion)
        {
            return;
        }

        try
        {
            ApplicationData::Current()
                .LocalSettings()
                .Values()
                .Insert(
                    GettingStartedVersionSettingKey,
                    box_value(static_cast<std::int32_t>(
                        GettingStartedTourVersion)));
        }
        catch (...)
        {
        }
    }
    void MainWindow::initializeAutoLock()
    {
        try
        {
            const auto weak{ get_weak() };
            const auto dispatcher{ DispatcherQueue() };

            try
            {
                const auto values{ ApplicationData::Current()
                    .LocalSettings()
                    .Values() };
                if (values.HasKey(AutoLockTimeoutSettingKey))
                {
                    const auto savedTimeout{ unbox_value<std::int32_t>(
                        values.Lookup(AutoLockTimeoutSettingKey)) };
                    if (savedTimeout >= 30 && savedTimeout <= 15 * 60)
                    {
                        m_autoLockTimeoutSeconds = savedTimeout;
                    }
                }
            }
            catch (...)
            {
                m_autoLockTimeoutSeconds = DefaultAutoLockTimeoutSeconds;
            }

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
            installWindowSizeConstraints();
            applyWindowChromeTheme();
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
                std::chrono::seconds{ m_autoLockTimeoutSeconds };
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
                std::chrono::seconds{ m_autoLockTimeoutSeconds };
        }
        catch (...)
        {
        }
    }
    void MainWindow::setAutoLockTimeout(int timeoutSeconds) noexcept
    {
        if (timeoutSeconds <= 0)
        {
            return;
        }

        m_autoLockTimeoutSeconds = timeoutSeconds;

        try
        {
            ApplicationData::Current()
                .LocalSettings()
                .Values()
                .Insert(
                    AutoLockTimeoutSettingKey,
                    box_value(static_cast<std::int32_t>(timeoutSeconds)));
        }
        catch (...)
        {
            // The active session still uses the selected timer if settings
            // storage is temporarily unavailable.
        }

        if (m_isLocked)
        {
            return;
        }

        try
        {
            m_autoLockDeadline = std::chrono::steady_clock::now() +
                std::chrono::seconds{ m_autoLockTimeoutSeconds };
            if (m_autoLockTimer)
            {
                m_autoLockTimer.Start();
            }
            updateAutoLockStatus();
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
                lockApplication(L"the inactivity timer elapsed");
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
    void MainWindow::setLockedInteractionState(bool locked) noexcept
    {
        const bool enabled{ !locked };

        try
        {
            LeftRail().IsHitTestVisible(enabled);
            RightRail().IsHitTestVisible(enabled);
            HeaderGrid().IsHitTestVisible(enabled);
            ToolbarGrid().IsHitTestVisible(enabled);
            WorkspaceRecordsContainer().IsHitTestVisible(enabled);
            StatusContainer().IsHitTestVisible(enabled);

            LauncherWorkspaceButton().IsEnabled(enabled);
            CredentialWorkspaceButton().IsEnabled(enabled);
            LauncherNavigationItem().IsEnabled(enabled);
            CredentialNavigationItem().IsEnabled(enabled);
            AccountActionsButton().IsEnabled(enabled);
            UtilitiesButton().IsEnabled(enabled);
            SearchBox().IsEnabled(enabled);
            RecordFilter().IsEnabled(enabled);
            TopAddAccountButton().IsEnabled(enabled);
            TopMoreActionsButton().IsEnabled(enabled);
            TopRemoveVisibleButton().IsEnabled(
                enabled && AccountsList().Items().Size() != 0 &&
                !m_removeVisibleInProgress);
            AccountsList().IsEnabled(enabled);
            ThemePicker().IsEnabled(enabled);
            CustomizeColorsButton().IsEnabled(enabled);
            PasswordGeneratorUtilityButton().IsEnabled(enabled);
            AutoLockUtilityButton().IsEnabled(enabled);
            HelpUtilityButton().IsEnabled(enabled);

            if (locked)
            {
                CompactNavigation().IsPaneOpen(false);
            }
            CompactNavigation().IsPaneToggleButtonVisible(
                enabled && m_shellLayout != ShellLayout::Wide);
        }
        catch (...)
        {
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

        // Modeless account/tool windows may own password controls or revealed
        // values. Closing them first resumes their cleanup coroutines before
        // the lock overlay is presented.
        finishGettingStartedTour(false);
        closeModelessWindows();

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
            setLockedInteractionState(true);
            LockOverlay().Visibility(Visibility::Visible);
            UnlockButton().IsEnabled(true);
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

            const bool verified{ co_await verifyUser(
                L"Verify your identity to unlock Account Armory") };
            if (verified &&
                m_isLocked &&
                m_lockGeneration == unlockGeneration)
            {
                m_isLocked = false;
                LockOverlay().Visibility(Visibility::Collapsed);
                setLockedInteractionState(false);
                m_autoLockDeadline = std::chrono::steady_clock::now() +
                    std::chrono::seconds{ m_autoLockTimeoutSeconds };
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
            }
        }
        catch (...)
        {
        }
    }
    void MainWindow::WorkspaceNavigation_SelectionChanged(
        NavigationView const&,
        NavigationViewSelectionChangedEventArgs const& args)
    {
        if (!m_windowReady ||
            m_updatingWorkspaceNavigation ||
            args.IsSettingsSelected())
        {
            return;
        }

        const auto item{ args.SelectedItem().try_as<NavigationViewItem>() };
        if (!item || !item.Tag())
        {
            return;
        }

        const hstring tag{ unbox_value<hstring>(item.Tag()) };
        switchWorkspace(
            tag == L"credential"
                ? WorkspaceSection::CredentialVault
                : WorkspaceSection::LauncherAccounts);

        if (m_shellLayout != ShellLayout::Wide)
        {
            CompactNavigation().IsPaneOpen(false);
        }
    }

    void MainWindow::LauncherWorkspaceButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        switchWorkspace(WorkspaceSection::LauncherAccounts);
    }

    void MainWindow::CredentialWorkspaceButton_Click(
        IInspectable const&,
        RoutedEventArgs const&)
    {
        switchWorkspace(WorkspaceSection::CredentialVault);
    }

    void MainWindow::switchWorkspace(WorkspaceSection section)
    {
        m_workspaceSection = section;
        const bool credential{
            section == WorkspaceSection::CredentialVault };

        m_updatingWorkspaceNavigation = true;
        LauncherWorkspaceButton().IsChecked(!credential);
        CredentialWorkspaceButton().IsChecked(credential);
        CompactNavigation().SelectedItem(
            credential
                ? CredentialNavigationItem()
                : LauncherNavigationItem());
        m_updatingWorkspaceNavigation = false;

        HeaderTitle().Text(
            credential ? L"Credential Vault" : L"Launcher Vault");
        HeaderSubtitle().Text(
            credential
                ? L"Store sign-ins for services, websites, school, work, and everyday accounts."
                : L"Manage linked launcher and email credentials in one record.");

        SearchBox().Text(L"");
        SearchBox().PlaceholderText(
            credential
                ? L"Search service, category, username, email, website, or notes"
                : L"Search launcher, username, email, or provider website");
        AutomationProperties::SetName(
            SearchBox(),
            credential
                ? L"Search Credential Vault"
                : L"Search Launcher Vault");
        AutomationProperties::SetHelpText(
            SearchBox(),
            SearchBox().PlaceholderText());

        CompactAddMenuItem().Text(
            credential ? L"Add credential" : L"Add account");
        CompactImportOneMenuItem().Text(
            credential ? L"Import one credential..." : L"Import one account...");
        CompactImportAllMenuItem().Text(
            credential ? L"Import vault..." : L"Import all accounts...");
        CompactImportBrowserCsvMenuItem().Visibility(
            credential ? Visibility::Visible : Visibility::Collapsed);
        CompactExportAllMenuItem().Text(
            credential ? L"Export vault..." : L"Export all accounts...");
        AccountActionsButton().Content(box_value(
            credential ? L"ACTIONS" : L"ACTIONS"));
        AutomationProperties::SetName(
            AccountActionsButton(),
            credential ? L"Credential Vault actions" : L"Account actions");
        TopAddAccountButton().Content(box_value(
            credential ? L"Add credential" : L"Add account"));
        AutomationProperties::SetName(
            TopAddAccountButton(),
            credential ? L"Add credential" : L"Add account");
        TopImportOneMenuItem().Text(
            credential ? L"Import one credential..." : L"Import one account...");
        TopImportAllMenuItem().Text(
            credential ? L"Import vault..." : L"Import all accounts...");
        TopImportBrowserCsvMenuItem().Visibility(
            credential ? Visibility::Visible : Visibility::Collapsed);
        TopExportAllMenuItem().Text(
            credential ? L"Export vault..." : L"Export all accounts...");
        AutomationProperties::SetName(
            TopRemoveVisibleButton(),
            credential
                ? L"Remove shown credentials"
                : L"Remove shown accounts");

        EmptyStateTitle().Text(
            credential ? L"No credentials found" : L"No accounts found");
        EmptyStateSubtitle().Text(
            credential
                ? L"Add a credential or change your search."
                : L"Add an account or change your search.");
        AutomationProperties::SetName(
            AccountsList(),
            credential ? L"Stored credentials" : L"Accounts");
        AutomationProperties::SetHelpText(
            AccountsList(),
            credential
                ? L"General service and website credentials"
                : L"Stored launcher and email accounts");

        rebuildRecordFilter();
        if (m_windowReady)
        {
            refreshAccounts();
        }
    }

    void MainWindow::rebuildRecordFilter()
    {
        m_updatingRecordFilter = true;
        const auto items{ RecordFilter().Items() };
        items.Clear();

        const auto appendItem = [&items](
            std::wstring_view text,
            std::optional<Launcher> launcher = std::nullopt)
        {
            ComboBoxItem item;
            item.Content(box_value(hstring{ text }));
            if (launcher)
            {
                item.Tag(box_value(static_cast<std::uint32_t>(*launcher)));
            }
            items.Append(item);
        };

        if (m_workspaceSection == WorkspaceSection::LauncherAccounts)
        {
            AutomationProperties::SetName(RecordFilter(), L"Launcher filter");
            appendItem(L"All launchers");
            for (auto const& launcher :
                 account_vault::services::LauncherCatalog)
            {
                appendItem(launcher.displayName, launcher.value);
            }
        }
        else
        {
            AutomationProperties::SetName(RecordFilter(), L"Category filter");
            appendItem(L"All categories");
            std::vector<std::wstring> categories;
            categories.reserve(
                account_vault::services::DefaultCredentialCategories.size());
            for (auto const category :
                 account_vault::services::DefaultCredentialCategories)
            {
                categories.emplace_back(category);
            }

            for (auto const& saved : m_repository.credentialCategories())
            {
                if (std::ranges::find(categories, saved) == categories.end())
                {
                    categories.push_back(saved);
                }
            }
            for (auto const& category : categories)
            {
                appendItem(category);
            }
        }

        RecordFilter().SelectedIndex(0);
        m_updatingRecordFilter = false;
    }

    std::vector<MainWindow::Account const*> MainWindow::visibleAccounts()
    {
        const std::wstring query{ SearchBox().Text().c_str() };
        if (m_workspaceSection == WorkspaceSection::CredentialVault)
        {
            return m_repository.searchCredentials(
                query,
                selectedCategoryFilter());
        }

        return m_repository.search(query, selectedLauncherFilter());
    }

    void MainWindow::refreshAccounts()
    {
        AccountsList().Items().Clear();
        const auto matches{ visibleAccounts() };

        for (Account const* account : matches)
        {
            appendAccountCard(*account);
        }

        EmptyState().Visibility(
            matches.empty() ? Visibility::Visible : Visibility::Collapsed);
        TopRemoveVisibleButton().IsEnabled(
            !matches.empty() && m_storageReady && !m_isLocked &&
            !m_removeVisibleInProgress);

        std::wstring status{ std::to_wstring(matches.size()) };
        if (m_workspaceSection == WorkspaceSection::CredentialVault)
        {
            status += matches.size() == 1
                ? L" credential shown"
                : L" credentials shown";
        }
        else
        {
            status += matches.size() == 1
                ? L" account shown"
                : L" accounts shown";
        }
        status += L"  |  passwords protected locally with Windows DPAPI";
        StatusText().Text(hstring{ status });
    }
    std::wstring MainWindow::selectedCategoryFilter()
    {
        const int selectedIndex{ RecordFilter().SelectedIndex() };

        if (selectedIndex <= 0)
        {
            return {};
        }

        const auto item = RecordFilter()
            .SelectedItem()
            .as<ComboBoxItem>();
        return unbox_value<hstring>(item.Content()).c_str();
    }

    std::optional<MainWindow::Launcher> MainWindow::selectedLauncherFilter()
    {
        if (RecordFilter().SelectedIndex() <= 0)
        {
            return std::nullopt;
        }

        const auto item{ RecordFilter().SelectedItem().as<ComboBoxItem>() };
        if (!item.Tag())
        {
            return std::nullopt;
        }

        return static_cast<Launcher>(
            unbox_value<std::uint32_t>(item.Tag()));
    }
}
