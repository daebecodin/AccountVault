#include "pch.h"
#include "../MainWindow.xaml.h"

#include <microsoft.ui.xaml.window.h>
#include <userconsentverifierinterop.h>
#include <winrt/Windows.Security.Credentials.UI.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Security::Credentials::UI;

namespace winrt::AccountVault::implementation
{
    IAsyncOperation<bool> MainWindow::verifyUser(hstring const& message)
    {
        try
        {
            auto lifetime{ get_strong() };
            const auto availability{
                co_await UserConsentVerifier::CheckAvailabilityAsync() };
            if (availability != UserConsentVerifierAvailability::Available)
            {
                StatusText().Text(
                    L"Windows Hello is unavailable or not configured");
                co_return false;
            }

            HWND windowHandle{};
            check_hresult(
                m_inner.as<::IWindowNative>()->get_WindowHandle(
                    &windowHandle));

            const auto interop{ get_activation_factory<
                UserConsentVerifier,
                ::IUserConsentVerifierInterop>() };

            const auto result{ co_await capture<
                IAsyncOperation<UserConsentVerificationResult>>(
                    interop,
                    &::IUserConsentVerifierInterop::
                        RequestVerificationForWindowAsync,
                    windowHandle,
                    reinterpret_cast<HSTRING>(get_abi(message))) };

            if (result == UserConsentVerificationResult::Verified)
            {
                co_return true;
            }

            if (result == UserConsentVerificationResult::Canceled)
            {
                StatusText().Text(L"Verification canceled");
            }
            else if (result == UserConsentVerificationResult::RetriesExhausted)
            {
                StatusText().Text(L"Verification attempts exhausted");
            }
            else
            {
                StatusText().Text(L"Windows could not verify this user");
            }
        }
        catch (...)
        {
            try
            {
                StatusText().Text(
                    L"Windows Hello verification is unavailable on this system");
            }
            catch (...)
            {
            }
        }

        co_return false;
    }
}
