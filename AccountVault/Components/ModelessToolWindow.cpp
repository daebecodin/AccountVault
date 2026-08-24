#include "pch.h"
#include "ModelessToolWindow.h"
#include "../resource.h"

#include <microsoft.ui.xaml.window.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Text.h>

#include <algorithm>
#include <atomic>
#include <utility>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::Foundation;
using namespace Windows::Graphics;

namespace account_vault::ui
{
    struct ModelessToolWindowState
    {
        Microsoft::UI::Xaml::Window window{ nullptr };
        Grid root{ nullptr };
        Border card{ nullptr };
        TextBlock title{ nullptr };
        ContentPresenter presenter{ nullptr };
        Button primaryButton{ nullptr };
        Button closeButton{ nullptr };
        Microsoft::UI::Dispatching::DispatcherQueue dispatcher{ nullptr };
        winrt::handle closedEvent;
        ContentDialogResult result{ ContentDialogResult::None };
        std::function<void(
            ModelessToolWindow const&,
            ModelessButtonClickEventArgs const&)> primaryHandler;
        std::int32_t clientWidth{};
        std::int32_t clientHeight{};
        std::uint64_t id{};
        std::atomic_size_t activeDeferrals{};
        HWND windowHandle{};
        HWND ownerWindowHandle{};
        std::atomic_bool closed{};
        bool activated{};

        ~ModelessToolWindowState()
        {
            try
            {
                if (window && !closed)
                {
                    window.Close();
                }
            }
            catch (...)
            {
            }
        }
    };

    struct ModelessClickState
    {
        std::weak_ptr<ModelessToolWindowState> owner;
        std::atomic_size_t outstandingDeferrals{};
        std::atomic_bool handlerReturned{};
        std::atomic_bool canceled{};
    };

    struct ModelessDeferral::CompletionState
    {
        std::shared_ptr<ModelessClickState> click;
        std::atomic_bool completed{};
    };

    namespace
    {
        std::atomic_uint64_t NextWindowId{ 1 };

        Brush appBrush(wchar_t const* key)
        {
            return Application::Current()
                .Resources()
                .Lookup(box_value(key))
                .as<Brush>();
        }

        Windows::UI::Color appColor(wchar_t const* key)
        {
            return Application::Current()
                .Resources()
                .Lookup(box_value(key))
                .as<SolidColorBrush>()
                .Color();
        }

        Windows::Foundation::IReference<Windows::UI::Color> boxedColor(
            Windows::UI::Color value)
        {
            return box_value(value)
                .as<Windows::Foundation::IReference<Windows::UI::Color>>();
        }

        void setNativeWindowIcons(HWND windowHandle) noexcept
        {
            if (!windowHandle)
            {
                return;
            }

            const HINSTANCE moduleHandle{ ::GetModuleHandleW(nullptr) };
            const auto loadIconAtSystemSize = [moduleHandle](int width, int height)
            {
                return static_cast<HICON>(::LoadImageW(
                    moduleHandle,
                    MAKEINTRESOURCEW(IDI_ACCOUNT_ARMORY),
                    IMAGE_ICON,
                    width,
                    height,
                    LR_DEFAULTCOLOR | LR_SHARED));
            };

            if (const HICON smallIcon{ loadIconAtSystemSize(
                ::GetSystemMetrics(SM_CXSMICON),
                ::GetSystemMetrics(SM_CYSMICON)) })
            {
                static_cast<void>(::SendMessageW(
                    windowHandle,
                    WM_SETICON,
                    ICON_SMALL,
                    reinterpret_cast<LPARAM>(smallIcon)));
            }

            if (const HICON largeIcon{ loadIconAtSystemSize(
                ::GetSystemMetrics(SM_CXICON),
                ::GetSystemMetrics(SM_CYICON)) })
            {
                static_cast<void>(::SendMessageW(
                    windowHandle,
                    WM_SETICON,
                    ICON_BIG,
                    reinterpret_cast<LPARAM>(largeIcon)));
            }
        }

        void applyTitleBarTheme(
            std::shared_ptr<ModelessToolWindowState> const& state) noexcept
        {
            try
            {
                if (!state || !state->window || state->closed ||
                    !Microsoft::UI::Windowing::AppWindowTitleBar::
                        IsCustomizationSupported())
                {
                    return;
                }

                const auto background{ boxedColor(appColor(L"AppBackgroundBrush")) };
                const auto surfaceAlt{ boxedColor(appColor(L"AppSurfaceAltBrush")) };
                const auto accent{ boxedColor(appColor(L"AppAccentBrush")) };
                const auto text{ boxedColor(appColor(L"AppTextBrush")) };
                const auto muted{ boxedColor(appColor(L"AppMutedTextBrush")) };

                const auto titleBar{ state->window.AppWindow().TitleBar() };
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
            catch (...)
            {
            }
        }

        void finishPrimaryClick(
            std::shared_ptr<ModelessClickState> const& click) noexcept
        {
            if (!click || !click->handlerReturned ||
                click->outstandingDeferrals != 0 || click->canceled)
            {
                return;
            }

            if (const auto owner{ click->owner.lock() })
            {
                if (owner->dispatcher && !owner->dispatcher.HasThreadAccess())
                {
                    static_cast<void>(owner->dispatcher.TryEnqueue(
                        [click]() noexcept
                        {
                            finishPrimaryClick(click);
                        }));
                    return;
                }
                ModelessToolWindow{ owner }.Close(ContentDialogResult::Primary);
            }
        }

        void signalClosedWhenIdle(
            std::shared_ptr<ModelessToolWindowState> const& state) noexcept
        {
            if (state && state->closed && state->activeDeferrals == 0)
            {
                ::SetEvent(state->closedEvent.get());
            }
        }

        void aliasControlBrushes(ResourceDictionary const& resources)
        {
            const auto applicationResources{ Application::Current().Resources() };
            const auto alias = [&resources, &applicationResources](
                wchar_t const* target,
                wchar_t const* source)
            {
                resources.Insert(
                    box_value(target),
                    applicationResources.Lookup(box_value(source)));
            };

            alias(L"ControlFillColorDefaultBrush", L"AppSurfaceAltBrush");
            alias(L"ControlFillColorSecondaryBrush", L"AppAccentLowBrush");
            alias(L"ControlFillColorTertiaryBrush", L"AppAccentMediumBrush");
            alias(L"ControlStrokeColorDefaultBrush", L"AppBorderBrush");
            alias(L"ControlStrokeColorSecondaryBrush", L"AppAccentBrush");
            alias(L"TextFillColorPrimaryBrush", L"AppTextBrush");
            alias(L"TextFillColorSecondaryBrush", L"AppMutedTextBrush");
            alias(L"AccentFillColorDefaultBrush", L"AppAccentBrush");
            alias(L"AccentFillColorSecondaryBrush", L"AppAccentHoverBrush");
            alias(L"AccentFillColorTertiaryBrush", L"AppAccentPressedBrush");
            alias(L"ButtonBackground", L"AppSurfaceAltBrush");
            alias(L"ButtonBackgroundPointerOver", L"AppAccentLowBrush");
            alias(L"ButtonBackgroundPressed", L"AppAccentMediumBrush");
            alias(L"ButtonBorderBrush", L"AppBorderBrush");
            alias(L"ButtonBorderBrushPointerOver", L"AppAccentBrush");
            alias(L"ButtonBorderBrushPressed", L"AppAccentBrush");
            alias(L"ButtonForeground", L"AppTextBrush");
            alias(L"ButtonForegroundPointerOver", L"AppTextBrush");
            alias(L"ButtonForegroundPressed", L"AppTextBrush");
            alias(L"AccentButtonBackground", L"AppAccentBrush");
            alias(L"AccentButtonBackgroundPointerOver", L"AppAccentHoverBrush");
            alias(L"AccentButtonBackgroundPressed", L"AppAccentPressedBrush");
            alias(L"AccentButtonBorderBrush", L"AppAccentBrush");
            alias(L"AccentButtonBorderBrushPointerOver", L"AppAccentHoverBrush");
            alias(L"AccentButtonBorderBrushPressed", L"AppAccentPressedBrush");
            alias(L"AccentButtonForeground", L"AppBackgroundBrush");
            alias(L"AccentButtonForegroundPointerOver", L"AppBackgroundBrush");
            alias(L"AccentButtonForegroundPressed", L"AppBackgroundBrush");
        }
    }

    ModelessDeferral::ModelessDeferral(std::nullptr_t) noexcept
    {
    }

    ModelessDeferral::ModelessDeferral(
        std::shared_ptr<ModelessClickState> const& clickState)
        : m_completion(std::make_shared<CompletionState>())
    {
        m_completion->click = clickState;
        if (clickState)
        {
            ++clickState->outstandingDeferrals;
            if (const auto owner{ clickState->owner.lock() })
            {
                ++owner->activeDeferrals;
            }
        }
    }

    ModelessDeferral::operator bool() const noexcept
    {
        return static_cast<bool>(m_completion);
    }

    void ModelessDeferral::Complete() const noexcept
    {
        if (!m_completion || m_completion->completed.exchange(true))
        {
            return;
        }

        const auto click{ m_completion->click };
        if (click && click->outstandingDeferrals != 0)
        {
            --click->outstandingDeferrals;
        }
        if (click)
        {
            if (const auto owner{ click->owner.lock() })
            {
                if (owner->activeDeferrals != 0)
                {
                    --owner->activeDeferrals;
                }
                signalClosedWhenIdle(owner);
            }
        }
        finishPrimaryClick(click);
    }

    ModelessButtonClickEventArgs::ModelessButtonClickEventArgs(
        std::shared_ptr<ModelessClickState> clickState) noexcept
        : m_state(std::move(clickState))
    {
    }

    void ModelessButtonClickEventArgs::Cancel(bool value) const noexcept
    {
        if (m_state)
        {
            m_state->canceled = value;
        }
    }

    bool ModelessButtonClickEventArgs::Cancel() const noexcept
    {
        return m_state && m_state->canceled;
    }

    ModelessDeferral ModelessButtonClickEventArgs::GetDeferral() const
    {
        return ModelessDeferral{ m_state };
    }

    ModelessToolWindow::ModelessToolWindow(std::nullptr_t) noexcept
    {
    }

    ModelessToolWindow::ModelessToolWindow(
        std::shared_ptr<ModelessToolWindowState> state) noexcept
        : m_state(std::move(state))
    {
    }

    ModelessToolWindow::ModelessToolWindow(
        hstring const& windowTitle,
        std::int32_t clientWidth,
        std::int32_t clientHeight)
        : m_state(std::make_shared<ModelessToolWindowState>())
    {
        m_state->id = NextWindowId.fetch_add(1);
        m_state->clientWidth = clientWidth;
        m_state->clientHeight = clientHeight;
        m_state->closedEvent.attach(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
        if (!m_state->closedEvent)
        {
            throw_last_error();
        }

        m_state->window = Microsoft::UI::Xaml::Window{};
        m_state->window.Title(windowTitle);

        Grid shell;
        shell.Background(appBrush(L"AppBackgroundBrush"));
        shell.Padding(Thickness{ 16 });
        aliasControlBrushes(shell.Resources());

        Border card;
        card.Background(appBrush(L"AppSurfaceBrush"));
        card.BorderBrush(appBrush(L"AppBorderBrush"));
        card.BorderThickness(Thickness{ 1 });
        card.CornerRadius(CornerRadius{ 10 });
        card.HorizontalAlignment(HorizontalAlignment::Stretch);
        card.VerticalAlignment(VerticalAlignment::Stretch);

        Grid layout;
        layout.RowDefinitions().Append(RowDefinition{});
        layout.RowDefinitions().GetAt(0).Height(GridLengthHelper::Auto());
        layout.RowDefinitions().Append(RowDefinition{});
        layout.RowDefinitions().GetAt(1).Height(
            GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        layout.RowDefinitions().Append(RowDefinition{});
        layout.RowDefinitions().GetAt(2).Height(GridLengthHelper::Auto());

        TextBlock title;
        title.Text(windowTitle);
        title.FontSize(20);
        title.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        title.Foreground(appBrush(L"AppTextBrush"));
        title.Margin(Thickness{ 24, 20, 24, 12 });

        ContentPresenter presenter;
        presenter.Margin(Thickness{ 24, 0, 24, 20 });
        presenter.HorizontalContentAlignment(HorizontalAlignment::Stretch);
        presenter.VerticalContentAlignment(VerticalAlignment::Stretch);

        Border footer;
        footer.Background(appBrush(L"AppBackgroundBrush"));
        footer.BorderBrush(appBrush(L"AppBorderBrush"));
        footer.BorderThickness(Thickness{ 0, 1, 0, 0 });
        footer.Padding(Thickness{ 24, 16, 24, 16 });

        Grid footerLayout;
        footerLayout.ColumnSpacing(8);
        footerLayout.ColumnDefinitions().Append(ColumnDefinition{});
        footerLayout.ColumnDefinitions().GetAt(0).Width(
            GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        footerLayout.ColumnDefinitions().Append(ColumnDefinition{});
        footerLayout.ColumnDefinitions().GetAt(1).Width(
            GridLengthHelper::FromValueAndType(1, GridUnitType::Star));

        Button primary;
        primary.HorizontalAlignment(HorizontalAlignment::Stretch);
        try
        {
            primary.Style(Application::Current()
                .Resources()
                .Lookup(box_value(L"PrimaryActionButtonStyle"))
                .as<Style>());
        }
        catch (...)
        {
        }

        Button close;
        close.HorizontalAlignment(HorizontalAlignment::Stretch);
        Grid::SetColumn(close, 1);

        footerLayout.Children().Append(primary);
        footerLayout.Children().Append(close);
        footer.Child(footerLayout);

        Grid::SetRow(title, 0);
        Grid::SetRow(presenter, 1);
        Grid::SetRow(footer, 2);
        layout.Children().Append(title);
        layout.Children().Append(presenter);
        layout.Children().Append(footer);
        card.Child(layout);
        shell.Children().Append(card);

        m_state->root = shell;
        m_state->dispatcher = shell.DispatcherQueue();
        m_state->card = card;
        m_state->title = title;
        m_state->presenter = presenter;
        m_state->primaryButton = primary;
        m_state->closeButton = close;
        m_state->window.Content(shell);

        const std::weak_ptr<ModelessToolWindowState> weakState{ m_state };
        primary.Click([weakState](IInspectable const&, RoutedEventArgs const&)
        {
            const auto state{ weakState.lock() };
            if (!state || state->closed || !state->primaryHandler)
            {
                return;
            }

            auto click = std::make_shared<ModelessClickState>();
            click->owner = state;
            ModelessToolWindow sender{ state };
            ModelessButtonClickEventArgs args{ click };
            try
            {
                state->primaryHandler(sender, args);
            }
            catch (...)
            {
                click->canceled = true;
            }
            click->handlerReturned = true;
            finishPrimaryClick(click);
        });

        close.Click([weakState](IInspectable const&, RoutedEventArgs const&)
        {
            if (const auto state{ weakState.lock() })
            {
                ModelessToolWindow{ state }.Close();
            }
        });

        m_state->window.Closed(
            [weakState](IInspectable const&, WindowEventArgs const&)
            {
                if (const auto state{ weakState.lock() })
                {
                    if (!state->closed)
                    {
                        state->closed = true;
                    }
                    signalClosedWhenIdle(state);
                }
            });
    }

    ModelessToolWindow::operator bool() const noexcept
    {
        return static_cast<bool>(m_state) && !m_state->closed;
    }

    std::uint64_t ModelessToolWindow::Id() const noexcept
    {
        return m_state ? m_state->id : 0;
    }

    void ModelessToolWindow::XamlRoot(Microsoft::UI::Xaml::XamlRoot const&) const noexcept
    {
    }

    void ModelessToolWindow::Title(IInspectable const& value) const
    {
        if (!m_state)
        {
            return;
        }
        const hstring text{ unbox_value<hstring>(value) };
        m_state->title.Text(text);
        m_state->window.Title(text);
    }

    void ModelessToolWindow::PrimaryButtonText(hstring const& value) const
    {
        if (m_state)
        {
            m_state->primaryButton.Content(box_value(value));
            m_state->primaryButton.Visibility(
                value.empty() ? Visibility::Collapsed : Visibility::Visible);
            Grid::SetColumn(
                m_state->closeButton,
                value.empty() ? 0 : 1);
            Grid::SetColumnSpan(
                m_state->closeButton,
                value.empty() ? 2 : 1);
        }
    }

    void ModelessToolWindow::CloseButtonText(hstring const& value) const
    {
        if (m_state)
        {
            m_state->closeButton.Content(box_value(value));
            m_state->closeButton.Visibility(
                value.empty() ? Visibility::Collapsed : Visibility::Visible);
        }
    }

    void ModelessToolWindow::DefaultButton(ContentDialogButton) const noexcept
    {
    }

    void ModelessToolWindow::MaxWidth(double value) const noexcept
    {
        try
        {
            if (m_state)
            {
                m_state->card.MaxWidth(value);
            }
        }
        catch (...)
        {
        }
    }

    void ModelessToolWindow::HorizontalAlignment(
        Microsoft::UI::Xaml::HorizontalAlignment) const noexcept
    {
    }

    void ModelessToolWindow::VerticalAlignment(
        Microsoft::UI::Xaml::VerticalAlignment) const noexcept
    {
    }

    void ModelessToolWindow::Content(UIElement const& value) const
    {
        if (m_state)
        {
            m_state->presenter.Content(value);
        }
    }

    void ModelessToolWindow::IsPrimaryButtonEnabled(bool value) const noexcept
    {
        try
        {
            if (m_state)
            {
                m_state->primaryButton.IsEnabled(value);
            }
        }
        catch (...)
        {
        }
    }

    void ModelessToolWindow::IsInteractionEnabled(bool value) const noexcept
    {
        try
        {
            if (!m_state || m_state->closed)
            {
                return;
            }

            m_state->root.IsHitTestVisible(value);
            if (m_state->windowHandle)
            {
                ::EnableWindow(m_state->windowHandle, value ? TRUE : FALSE);
            }
        }
        catch (...)
        {
        }
    }

    void ModelessToolWindow::OwnerWindowHandle(std::intptr_t value) const noexcept
    {
        if (m_state)
        {
            m_state->ownerWindowHandle = reinterpret_cast<HWND>(value);
        }
    }

    ResourceDictionary ModelessToolWindow::Resources() const
    {
        return m_state ? m_state->root.Resources() : ResourceDictionary{ nullptr };
    }

    void ModelessToolWindow::PrimaryButtonClick(
        PrimaryClickHandler handler) const
    {
        if (m_state)
        {
            m_state->primaryHandler = std::move(handler);
        }
    }

    IAsyncOperation<ContentDialogResult> ModelessToolWindow::ShowAsync(
        ContentDialogPlacement) const
    {
        const auto state{ m_state };
        if (!state)
        {
            co_return ContentDialogResult::None;
        }

        apartment_context uiThread;
        Activate();
        co_await resume_on_signal(state->closedEvent.get());
        co_await uiThread;
        co_return state->result;
    }

    void ModelessToolWindow::Activate() const noexcept
    {
        try
        {
            if (!m_state || m_state->closed)
            {
                return;
            }

            m_state->window.Activate();
            if (!m_state->activated)
            {
                m_state->activated = true;
                check_hresult(
                    m_state->window.as<::IWindowNative>()->get_WindowHandle(
                        &m_state->windowHandle));
                setNativeWindowIcons(m_state->windowHandle);
                applyTitleBarTheme(m_state);
                if (m_state->ownerWindowHandle)
                {
                    ::SetWindowLongPtrW(
                        m_state->windowHandle,
                        GWLP_HWNDPARENT,
                        reinterpret_cast<LONG_PTR>(m_state->ownerWindowHandle));
                }
                const UINT dpi{ ::GetDpiForWindow(m_state->windowHandle) };
                int pixelWidth{ ::MulDiv(
                    m_state->clientWidth,
                    dpi == 0 ? USER_DEFAULT_SCREEN_DPI : dpi,
                    USER_DEFAULT_SCREEN_DPI) };
                int pixelHeight{ ::MulDiv(
                    m_state->clientHeight,
                    dpi == 0 ? USER_DEFAULT_SCREEN_DPI : dpi,
                    USER_DEFAULT_SCREEN_DPI) };

                MONITORINFO monitorInfo{ sizeof(MONITORINFO) };
                const HMONITOR monitor{ ::MonitorFromWindow(
                    m_state->ownerWindowHandle
                        ? m_state->ownerWindowHandle
                        : m_state->windowHandle,
                    MONITOR_DEFAULTTONEAREST) };
                const bool hasMonitorInfo{
                    monitor && ::GetMonitorInfoW(monitor, &monitorInfo) };
                if (hasMonitorInfo)
                {
                    const int workWidth{
                        monitorInfo.rcWork.right - monitorInfo.rcWork.left };
                    const int workHeight{
                        monitorInfo.rcWork.bottom - monitorInfo.rcWork.top };
                    pixelWidth = (std::min)(pixelWidth, (std::max)(480, workWidth - 64));
                    pixelHeight = (std::min)(pixelHeight, (std::max)(420, workHeight - 64));
                }
                m_state->window.AppWindow().ResizeClient(
                    SizeInt32{ pixelWidth, pixelHeight });

                RECT ownerRect{};
                RECT windowRect{};
                if (m_state->ownerWindowHandle &&
                    ::GetWindowRect(m_state->ownerWindowHandle, &ownerRect) &&
                    ::GetWindowRect(m_state->windowHandle, &windowRect))
                {
                    const LONG windowWidth{ windowRect.right - windowRect.left };
                    const LONG windowHeight{ windowRect.bottom - windowRect.top };
                    const LONG ownerWidth{ ownerRect.right - ownerRect.left };
                    const LONG ownerHeight{ ownerRect.bottom - ownerRect.top };
                    LONG left{
                        ownerRect.left + (ownerWidth - windowWidth) / 2 };
                    LONG top{
                        ownerRect.top + (ownerHeight - windowHeight) / 2 };
                    if (hasMonitorInfo)
                    {
                        left = std::clamp(
                            left,
                            monitorInfo.rcWork.left,
                            (std::max)(
                                monitorInfo.rcWork.left,
                                monitorInfo.rcWork.right - windowWidth));
                        top = std::clamp(
                            top,
                            monitorInfo.rcWork.top,
                            (std::max)(
                                monitorInfo.rcWork.top,
                                monitorInfo.rcWork.bottom - windowHeight));
                    }
                    ::SetWindowPos(
                        m_state->windowHandle,
                        nullptr,
                        left,
                        top,
                        0,
                        0,
                        SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER);
                }
            }
            else
            {
                applyTitleBarTheme(m_state);
            }
        }
        catch (...)
        {
        }
    }

    void ModelessToolWindow::RefreshTheme() const noexcept
    {
        applyTitleBarTheme(m_state);
    }

    void ModelessToolWindow::Hide() const noexcept
    {
        Close();
    }

    void ModelessToolWindow::Close(ContentDialogResult result) const noexcept
    {
        try
        {
            if (!m_state || m_state->closed)
            {
                return;
            }
            m_state->result = result;
            m_state->window.Close();
        }
        catch (...)
        {
            if (m_state && !m_state->closed)
            {
                m_state->closed = true;
                signalClosedWhenIdle(m_state);
            }
        }
    }
}
