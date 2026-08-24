#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "resource.h"

#include <microsoft.ui.xaml.window.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace
{
    void setNativeWindowIcons(Window const& window)
    {
        HWND windowHandle{};
        auto windowNative{ window.as<::IWindowNative>() };
        check_hresult(windowNative->get_WindowHandle(&windowHandle));

        const HINSTANCE moduleHandle{ GetModuleHandleW(nullptr) };

        const auto loadIconAtSystemSize = [moduleHandle](int width, int height)
        {
            return static_cast<HICON>(LoadImageW(
                moduleHandle,
                MAKEINTRESOURCEW(IDI_ACCOUNT_ARMORY),
                IMAGE_ICON,
                width,
                height,
                LR_DEFAULTCOLOR | LR_SHARED));
        };

        if (const HICON smallIcon{ loadIconAtSystemSize(
            GetSystemMetrics(SM_CXSMICON),
            GetSystemMetrics(SM_CYSMICON)) })
        {
            static_cast<void>(SendMessageW(
                windowHandle,
                WM_SETICON,
                ICON_SMALL,
                reinterpret_cast<LPARAM>(smallIcon)));
        }

        if (const HICON largeIcon{ loadIconAtSystemSize(
            GetSystemMetrics(SM_CXICON),
            GetSystemMetrics(SM_CYICON)) })
        {
            static_cast<void>(SendMessageW(
                windowHandle,
                WM_SETICON,
                ICON_BIG,
                reinterpret_cast<LPARAM>(largeIcon)));
        }
    }
}

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace winrt::AccountVault::implementation
{
    /// <summary>
    /// Initializes the singleton application object.  This is the first line of authored code
    /// executed, and as such is the logical equivalent of main() or WinMain().
    /// </summary>
    App::App()
    {
        // Xaml objects should not call InitializeComponent during construction.
        // See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent

#if defined _DEBUG && !defined DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION
        UnhandledException([](IInspectable const&, UnhandledExceptionEventArgs const& e)
        {
            if (IsDebuggerPresent())
            {
                auto errorMessage = e.Message();
                __debugbreak();
            }
        });
#endif
    }

    /// <summary>
    /// Invoked when the application is launched.
    /// </summary>
    /// <param name="e">Details about the launch request and process.</param>
    void App::OnLaunched([[maybe_unused]] LaunchActivatedEventArgs const& e)
    {
        window = make<MainWindow>();
        window.Activate();
        setNativeWindowIcons(window);
    }
}
