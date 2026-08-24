#pragma once

#include "MainWindow.g.h"
#include "Components/ModelessToolWindow.h"
#include "Models/ThemeDefinition.h"
#include "Services/AccountRepository.h"
#include "Services/AccountStorageService.h"
#include "Services/CredentialService.h"
#include "Services/PortableBackupService.h"

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>

#include <chrono>
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
        ~MainWindow();

        void AddAccountButton_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void ImportOneAccountButton_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void ImportAllAccountsButton_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void ExportAllAccountsButton_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void SearchBox_TextChanged(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::TextChangedEventArgs const& args);

        void RecordFilter_SelectionChanged(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

        void ThemePicker_SelectionChanged(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& args);

        void CompactThemeMenuItem_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void ThemeOption_RightTapped(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& args);

        void CustomizeColorsButton_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void PasswordGeneratorButton_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void UnlockButton_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

    private:
        using Account = account_vault::models::Account;
        using AccountKind = account_vault::models::AccountKind;
        using RecordId = account_vault::models::RecordId;
        using PortableAccount = account_vault::services::PortableAccount;
        using ThemeDefinition = account_vault::models::ThemeDefinition;

        enum class ShellLayout
        {
            Unknown,
            Compact,
            CenterOnly,
            Wide
        };

        enum class WorkspaceSection
        {
            LauncherAccounts,
            CredentialVault
        };

        static constexpr int BuiltInThemeCount{ 10 };
        static constexpr int AutoLockTimeoutSeconds{ 5 * 60 };
        static constexpr std::int32_t StartupEdgeMargin{ 48 };
        static constexpr std::int32_t RegularStartupWidth{ 1440 };
        static constexpr std::int32_t RegularStartupHeight{ 984 };
        static constexpr std::int32_t UltrawideStartupWidth{ 2300 };
        static constexpr std::int32_t UltrawideStartupHeight{ 984 };
        static constexpr std::int32_t MinimumClientWidth{ 1120 };
        static constexpr std::int32_t MinimumClientHeight{ 945 };
        static constexpr std::int32_t MaximumClientWidth{ 2315 };
        static constexpr std::int32_t MaximumClientHeight{ 945 };
        static constexpr double UltrawideAspectRatioThreshold{ 2.0 };
        static constexpr double CenterOnlyShellMinWidth{ 760.0 };
        static constexpr double WideShellMinWidth{ 1440.0 };

        account_vault::services::AccountRepository m_repository;
        account_vault::services::AccountStorageService m_accountStorage;
        account_vault::services::CredentialService m_credentials;
        account_vault::services::PortableBackupService m_backupService;
        std::vector<ThemeDefinition> m_customThemes;
        std::vector<account_vault::ui::ModelessToolWindow> m_modelessWindows;
        account_vault::ui::ModelessToolWindow m_passwordGeneratorWindow{ nullptr };
        bool m_windowReady{ false };
        bool m_storageReady{ true };
        bool m_isLocked{ false };
        bool m_unlockInProgress{ false };
        bool m_backupOperationInProgress{ false };
        bool m_updatingWorkspaceNavigation{ false };
        bool m_updatingRecordFilter{ false };
        ShellLayout m_shellLayout{ ShellLayout::Unknown };
        WorkspaceSection m_workspaceSection{
            WorkspaceSection::LauncherAccounts };
        std::int32_t m_displayedWindowWidth{ -1 };
        std::int32_t m_displayedWindowHeight{ -1 };
        std::uint64_t m_lockGeneration{};
        std::uint32_t m_accountClipboardSequence{};
        std::chrono::steady_clock::time_point m_autoLockDeadline{};
        Microsoft::UI::Dispatching::DispatcherQueueTimer m_autoLockTimer{ nullptr };
        Microsoft::UI::Windowing::AppWindow m_appWindow{ nullptr };
        HWND m_windowHandle{};
        winrt::event_token m_appWindowChangedToken{};
        winrt::event_token m_suspendStatusChangedToken{};

        void refreshAccounts();
        void appendAccountCard(
            Account const& account,
            std::optional<std::uint32_t> index = std::nullopt);
        void refreshAccountCard(RecordId id);
        void switchWorkspace(WorkspaceSection section);
        void rebuildRecordFilter();
        [[nodiscard]] std::vector<Account const*> visibleAccounts();
        void copyToClipboard(std::wstring const& value, std::wstring_view label);
        Windows::Foundation::IAsyncOperation<bool> verifyUser(
            winrt::hstring const& message);
        void removeAccount(RecordId id);
        void applyDefaultWindowSize() noexcept;
        void installWindowSizeConstraints() noexcept;
        void applyWindowSizeConstraints(MINMAXINFO& limits) const noexcept;
        static LRESULT CALLBACK windowSubclassProc(
            HWND windowHandle,
            UINT message,
            WPARAM wParam,
            LPARAM lParam,
            UINT_PTR subclassId,
            DWORD_PTR referenceData) noexcept;
        void applyWindowChromeTheme() noexcept;
        void updateWindowDimensions(double width, double height) noexcept;
        void updateShellLayout(double width) noexcept;
        void initializeAutoLock();
        void noteUserActivity() noexcept;
        void updateAutoLockStatus() noexcept;
        void lockApplication(std::wstring_view reason) noexcept;
        winrt::fire_and_forget unlockApplication();

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

        [[nodiscard]] std::optional<RecordId> addCredential(
            std::wstring serviceName,
            std::wstring category,
            std::wstring username,
            std::wstring emailAddress,
            std::wstring password,
            std::wstring website,
            std::wstring recoveryEmail,
            std::wstring recoveryEmailPassword,
            std::wstring notes);

        [[nodiscard]] bool updateCredential(
            RecordId id,
            std::wstring serviceName,
            std::wstring category,
            std::wstring username,
            std::wstring emailAddress,
            std::optional<std::wstring> password,
            std::wstring website,
            std::wstring recoveryEmail,
            std::optional<std::wstring> recoveryEmailPassword,
            std::wstring notes);

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

        [[nodiscard]] std::wstring selectedFilter();
        [[nodiscard]] RecordId recordIdFrom(
            Microsoft::UI::Xaml::Controls::Button const& button) const;

        void attachDialogToShell(
            Microsoft::UI::Xaml::Controls::ContentDialog const& dialog);
        void detachDialogFromShell(
            Microsoft::UI::Xaml::Controls::ContentDialog const& dialog) noexcept;
        void attachDialogToShell(
            account_vault::ui::ModelessToolWindow const& window);
        void detachModelessWindow(
            account_vault::ui::ModelessToolWindow const& window) noexcept;
        void setModelessWindowsInteraction(bool enabled) noexcept;
        void closeModelessWindows() noexcept;

        [[nodiscard]] bool buildPortableAccounts(
            std::optional<RecordId> onlyRecord,
            std::vector<PortableAccount>& accounts,
            std::wstring& error) const;

        Windows::Foundation::IAsyncOperation<winrt::hstring>
            requestBackupPassword(bool confirmPassword);

        winrt::fire_and_forget showExportBackup(
            std::optional<RecordId> onlyRecord);
        winrt::fire_and_forget showImportBackup(bool requireSingleAccount);
        winrt::fire_and_forget showAddAccountDialog();
        winrt::fire_and_forget showAccountDetailsDialog(RecordId id);
        winrt::fire_and_forget showAddCredentialDialog();
        winrt::fire_and_forget showCredentialDetailsDialog(RecordId id);
        winrt::fire_and_forget showColorDialog();
        winrt::fire_and_forget showPasswordGenerator();

    public:
        void WorkspaceNavigation_SelectionChanged(
            Microsoft::UI::Xaml::Controls::NavigationView const& sender,
            Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& args);

        void LauncherWorkspaceButton_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);

        void CredentialWorkspaceButton_Click(
            Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
    };
}

namespace winrt::AccountVault::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
