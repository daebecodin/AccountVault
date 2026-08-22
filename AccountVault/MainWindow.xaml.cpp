#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <string>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Windows::Foundation;

namespace winrt::AccountVault::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        // Temporary data makes the first UI run immediately useful.
        // Delete these calls when you are ready to begin with an empty vault.
        static_cast<void>(m_repository.add(
            L"Steam",
            L"night_shift",
            L"demo-launcher-password",
            L"night@example.com",
            L"Gmail",
            L"https://mail.google.com/",
            L"demo-email-password"));
        static_cast<void>(m_repository.add(
            L"Riot",
            L"pixelpilot#NA1",
            L"demo-launcher-password",
            L"pilot@example.com",
            L"Outlook",
            L"https://outlook.live.com/mail/",
            L"demo-email-password"));
        static_cast<void>(m_repository.add(
            L"Epic",
            L"orbit_runner",
            L"demo-launcher-password",
            L"orbit@example.com",
            L"Yahoo",
            L"https://mail.yahoo.com/",
            L"demo-email-password"));

        m_windowReady = true;
        refreshAccounts();
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
        status += L"  |  in-memory milestone";
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
