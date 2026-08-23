#pragma once

#include "MainWindow.g.h"
#include "Models/ThemeDefinition.h"
#include "Services/AccountRepository.h"
#include "Services/AccountStorageService.h"
#include "Services/CredentialService.h"

#include <winrt/Microsoft.UI.Xaml.Input.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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

        void ThemeOption_RightTapped(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args);

        void CustomizeColorsButton_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        using Account = account_vault::models::Account;
        using RecordId = account_vault::models::RecordId;

        using ThemeDefinition = account_vault::models::ThemeDefinition;

        static constexpr int BuiltInThemeCount{ 10 };

        account_vault::services::AccountRepository m_repository;
        account_vault::services::AccountStorageService m_accountStorage;
        account_vault::services::CredentialService m_credentials;
        std::vector<ThemeDefinition> m_customThemes;
        bool m_windowReady{ false };
        bool m_storageReady{ true };

        void refreshAccounts();
        void appendAccountCard(
            Account const& account,
            std::optional<std::uint32_t> index = std::nullopt);
        void refreshAccountCard(RecordId id);
        void copyToClipboard(std::wstring const& value, std::wstring_view label);
        void removeAccount(RecordId id);

        [[nodiscard]] std::optional<RecordId> addAccount(
            std::wstring launcher,
            std::wstring launcherUsername,
            std::wstring launcherPassword,
            std::wstring emailAddress,
            std::wstring emailProvider,
            std::wstring emailProviderWebsite,
            std::wstring emailPassword);

        [[nodiscard]] bool updateAccount(
            RecordId id,
            std::wstring launcher,
            std::wstring launcherUsername,
            std::optional<std::wstring> launcherPassword,
            std::wstring emailAddress,
            std::wstring emailProvider,
            std::wstring emailProviderWebsite,
            std::optional<std::wstring> emailPassword);

        [[nodiscard]] bool persistAccounts(std::wstring& error) const;
        void applyPreset(int selectedIndex);
        void applyTheme(ThemeDefinition const& theme);

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
        winrt::fire_and_forget showAccountDetailsDialog(RecordId id);
        winrt::fire_and_forget showColorDialog();
    };
}

namespace winrt::AccountVault::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
