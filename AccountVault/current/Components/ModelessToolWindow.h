#pragma once

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace account_vault::ui
{
    struct ModelessToolWindowState;
    struct ModelessClickState;

    class ModelessDeferral
    {
    public:
        ModelessDeferral(std::nullptr_t = nullptr) noexcept;
        explicit ModelessDeferral(
            std::shared_ptr<ModelessClickState> const& clickState);

        explicit operator bool() const noexcept;
        void Complete() const noexcept;

    private:
        struct CompletionState;
        std::shared_ptr<CompletionState> m_completion;
    };

    class ModelessButtonClickEventArgs
    {
    public:
        explicit ModelessButtonClickEventArgs(
            std::shared_ptr<ModelessClickState> clickState) noexcept;

        void Cancel(bool value) const noexcept;
        [[nodiscard]] bool Cancel() const noexcept;
        [[nodiscard]] ModelessDeferral GetDeferral() const;

    private:
        std::shared_ptr<ModelessClickState> m_state;
    };

    // A small value-type facade around a shared secondary WinUI Window. It
    // deliberately mirrors the subset of ContentDialog used by Account Armory
    // so existing validation and plaintext-cleanup flows remain easy to audit.
    class ModelessToolWindow
    {
    public:
        ModelessToolWindow(std::nullptr_t = nullptr) noexcept;
        ModelessToolWindow(
            winrt::hstring const& title,
            std::int32_t clientWidth,
            std::int32_t clientHeight);

        explicit operator bool() const noexcept;
        [[nodiscard]] std::uint64_t Id() const noexcept;

        void XamlRoot(winrt::Microsoft::UI::Xaml::XamlRoot const&) const noexcept;
        void Title(winrt::Windows::Foundation::IInspectable const& value) const;
        void PrimaryButtonText(winrt::hstring const& value) const;
        void CloseButtonText(winrt::hstring const& value) const;
        void DefaultButton(
            winrt::Microsoft::UI::Xaml::Controls::ContentDialogButton) const noexcept;
        void MaxWidth(double value) const noexcept;
        void HorizontalAlignment(
            winrt::Microsoft::UI::Xaml::HorizontalAlignment) const noexcept;
        void VerticalAlignment(
            winrt::Microsoft::UI::Xaml::VerticalAlignment) const noexcept;
        void Content(winrt::Microsoft::UI::Xaml::UIElement const& value) const;
        void IsPrimaryButtonEnabled(bool value) const noexcept;
        void IsInteractionEnabled(bool value) const noexcept;
        void OwnerWindowHandle(std::intptr_t value) const noexcept;

        [[nodiscard]] winrt::Microsoft::UI::Xaml::ResourceDictionary Resources() const;

        using PrimaryClickHandler = std::function<void(
            ModelessToolWindow const&,
            ModelessButtonClickEventArgs const&)>;

        void PrimaryButtonClick(PrimaryClickHandler handler) const;

        [[nodiscard]] winrt::Windows::Foundation::IAsyncOperation<
            winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult>
            ShowAsync(
                winrt::Microsoft::UI::Xaml::Controls::ContentDialogPlacement placement) const;

        void Activate() const noexcept;
        void RefreshTheme() const noexcept;
        void Hide() const noexcept;
        void Close(
            winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult result =
                winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult::None) const noexcept;

        explicit ModelessToolWindow(
            std::shared_ptr<ModelessToolWindowState> state) noexcept;

    private:
        std::shared_ptr<ModelessToolWindowState> m_state;

        friend struct ModelessToolWindowState;
    };
}
