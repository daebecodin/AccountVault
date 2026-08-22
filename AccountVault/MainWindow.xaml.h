#pragma once

#include "MainWindow.g.h"
#include "Services/AccountRepository.h"

namespace winrt::AccountVault::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();

        void AddAccountButton_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void SearchBox_TextChanged(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& args);

        void LauncherFilter_SelectionChanged(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

        void ThemePicker_SelectionChanged(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

        void CustomizeColorsButton_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        using Account = account_vault::models::Account;
        using RecordId = account_vault::models::RecordId;

        account_vault::services::AccountRepository m_repository;
        bool m_windowReady{ false };

        void refreshAccounts();
        void appendAccountCard(Account const& account);
        void copyToClipboard(std::wstring const& value, std::wstring_view label);
        void removeAccount(RecordId id);
        void applyPreset(int selectedIndex);

        void setBrushColor(
            std::wstring_view resourceName,
            Windows::UI::Color color);

        [[nodiscard]] Windows::UI::Color brushColor(
            std::wstring_view resourceName) const;

        [[nodiscard]] static Windows::UI::Color color(
            std::uint8_t red,
            std::uint8_t green,
            std::uint8_t blue);

        [[nodiscard]] std::wstring selectedLauncher();
        [[nodiscard]] RecordId recordIdFrom(
            Microsoft::UI::Xaml::Controls::Button const& button) const;

        winrt::fire_and_forget showAddAccountDialog();
        winrt::fire_and_forget showColorDialog();
    };
}

namespace winrt::AccountVault::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
