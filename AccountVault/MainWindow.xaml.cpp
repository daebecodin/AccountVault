#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.UI.Text.h>

#include <string>
#include <utility>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::ApplicationModel::DataTransfer;
using namespace Windows::UI;
using namespace Windows::Foundation;

namespace winrt::AccountVault::implementation
{
    MainWindow::MainWindow()
    {
        InitializeComponent();

        // Temporary data makes the first UI run immediately useful.
        // Delete these four calls when you are ready to begin with an empty vault.
        static_cast<void>(m_repository.add(
            L"Steam", L"night_shift", L"night@example.com", L"demo-password"));
        static_cast<void>(m_repository.add(
            L"Riot", L"pixelpilot#NA1", L"pilot@example.com", L"demo-password"));
        static_cast<void>(m_repository.add(
            L"Epic", L"orbit_runner", L"orbit@example.com", L"demo-password"));

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

    void MainWindow::appendAccountCard(Account const& account)
    {
        Grid card;
        card.Padding(Thickness{ 16 });
        card.Margin(Thickness{ 0, 0, 0, 10 });
        card.ColumnSpacing(14);
        card.Background(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppSurfaceBrush"))
                .as<Brush>());

        card.ColumnDefinitions().Append(ColumnDefinition{});
        card.ColumnDefinitions().Append(ColumnDefinition{});
        card.ColumnDefinitions().Append(ColumnDefinition{});
        card.ColumnDefinitions().Append(ColumnDefinition{});

        card.ColumnDefinitions().GetAt(0).Width(
            GridLengthHelper::FromPixels(120));
        card.ColumnDefinitions().GetAt(1).Width(
            GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        card.ColumnDefinitions().GetAt(2).Width(
            GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        card.ColumnDefinitions().GetAt(3).Width(GridLengthHelper::Auto());

        Border launcherBadge;
        launcherBadge.Padding(Thickness{ 10, 6, 10, 6 });
        launcherBadge.CornerRadius(CornerRadius{ 6, 6, 6, 6 });
        launcherBadge.HorizontalAlignment(HorizontalAlignment::Left);
        launcherBadge.VerticalAlignment(VerticalAlignment::Center);
        launcherBadge.Background(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppSurfaceAltBrush"))
                .as<Brush>());

        TextBlock launcherText;
        launcherText.Text(account.launcher);
        launcherText.FontFamily(FontFamily{ L"Cascadia Mono" });
        launcherText.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        launcherText.Foreground(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppAccentBrush"))
                .as<Brush>());
        launcherBadge.Child(launcherText);

        StackPanel idPanel;
        idPanel.Spacing(3);
        TextBlock idLabel;
        idLabel.Text(L"ACCOUNT ID");
        idLabel.FontSize(11);
        idLabel.Foreground(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppMutedTextBrush"))
                .as<Brush>());
        TextBlock idValue;
        idValue.Text(account.launcherAccountId);
        idValue.Foreground(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppTextBrush"))
                .as<Brush>());
        idPanel.Children().Append(idLabel);
        idPanel.Children().Append(idValue);

        StackPanel emailPanel;
        emailPanel.Spacing(3);
        TextBlock emailLabel;
        emailLabel.Text(L"EMAIL");
        emailLabel.FontSize(11);
        emailLabel.Foreground(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppMutedTextBrush"))
                .as<Brush>());
        TextBlock emailValue;
        emailValue.Text(account.email);
        emailValue.Foreground(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppTextBrush"))
                .as<Brush>());
        emailPanel.Children().Append(emailLabel);
        emailPanel.Children().Append(emailValue);

        StackPanel actions;
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(8);

        const auto makeButton = [&](hstring const& label)
        {
            Button button;
            button.Content(box_value(label));
            button.Tag(box_value(account.recordId));
            button.Padding(Thickness{ 11, 5, 11, 5 });
            button.CornerRadius(CornerRadius{ 6, 6, 6, 6 });
            return button;
        };

        Button copyId{ makeButton(L"Copy ID") };
        copyId.Click([this](IInspectable const& sender, RoutedEventArgs const&)
        {
            const auto button{ sender.as<Button>() };
            const Account* account{ m_repository.find(recordIdFrom(button)) };
            if (account)
            {
                copyToClipboard(account->launcherAccountId, L"Account ID");
            }
        });

        Button copyEmail{ makeButton(L"Copy email") };
        copyEmail.Click([this](IInspectable const& sender, RoutedEventArgs const&)
        {
            const auto button{ sender.as<Button>() };
            const Account* account{ m_repository.find(recordIdFrom(button)) };
            if (account)
            {
                copyToClipboard(account->email, L"Email");
            }
        });

        Button copyPassword{ makeButton(L"Copy password") };
        copyPassword.Click([this](IInspectable const& sender, RoutedEventArgs const&)
        {
            const auto button{ sender.as<Button>() };
            const Account* account{ m_repository.find(recordIdFrom(button)) };
            if (account)
            {
                copyToClipboard(account->password, L"Password");
            }
        });

        Button remove{ makeButton(L"Remove") };
        remove.Click([this](IInspectable const& sender, RoutedEventArgs const&)
        {
            const auto button{ sender.as<Button>() };
            removeAccount(recordIdFrom(button));
        });

        actions.Children().Append(copyId);
        actions.Children().Append(copyEmail);
        actions.Children().Append(copyPassword);
        actions.Children().Append(remove);

        Grid::SetColumn(launcherBadge, 0);
        Grid::SetColumn(idPanel, 1);
        Grid::SetColumn(emailPanel, 2);
        Grid::SetColumn(actions, 3);

        card.Children().Append(launcherBadge);
        card.Children().Append(idPanel);
        card.Children().Append(emailPanel);
        card.Children().Append(actions);

        AccountsList().Items().Append(card);
    }

    void MainWindow::copyToClipboard(
        std::wstring const& value,
        std::wstring_view label)
    {
        DataPackage package;
        package.SetText(hstring{ value });
        Clipboard::SetContent(package);
        std::wstring status{ label };
        status += L" copied to the clipboard";
        StatusText().Text(hstring{ status });
    }

    void MainWindow::removeAccount(RecordId id)
    {
        if (m_repository.remove(id))
        {
            refreshAccounts();
            StatusText().Text(L"Account removed from memory");
        }
    }

    void MainWindow::applyPreset(int selectedIndex)
    {
        switch (selectedIndex)
        {
        case 0: // Console Dark
            setBrushColor(L"AppBackgroundBrush", color(12, 15, 13));
            setBrushColor(L"AppSurfaceBrush", color(21, 26, 23));
            setBrushColor(L"AppSurfaceAltBrush", color(28, 36, 31));
            setBrushColor(L"AppAccentBrush", color(86, 211, 100));
            setBrushColor(L"AppTextBrush", color(230, 237, 243));
            setBrushColor(L"AppMutedTextBrush", color(139, 148, 158));
            break;

        case 1: // Midnight
            setBrushColor(L"AppBackgroundBrush", color(8, 12, 24));
            setBrushColor(L"AppSurfaceBrush", color(18, 25, 47));
            setBrushColor(L"AppSurfaceAltBrush", color(27, 37, 65));
            setBrushColor(L"AppAccentBrush", color(124, 156, 255));
            setBrushColor(L"AppTextBrush", color(242, 245, 255));
            setBrushColor(L"AppMutedTextBrush", color(151, 162, 190));
            break;

        case 2: // Light
            setBrushColor(L"AppBackgroundBrush", color(245, 247, 250));
            setBrushColor(L"AppSurfaceBrush", color(255, 255, 255));
            setBrushColor(L"AppSurfaceAltBrush", color(232, 237, 244));
            setBrushColor(L"AppAccentBrush", color(0, 103, 192));
            setBrushColor(L"AppTextBrush", color(25, 30, 38));
            setBrushColor(L"AppMutedTextBrush", color(90, 100, 115));
            break;

        default:
            return;
        }

        refreshAccounts();
    }

    void MainWindow::setBrushColor(
        std::wstring_view resourceName,
        Color value)
    {
        auto brush = Application::Current()
            .Resources()
            .Lookup(box_value(hstring{ resourceName }))
            .as<SolidColorBrush>();
        brush.Color(value);
    }

    Color MainWindow::brushColor(std::wstring_view resourceName) const
    {
        return Application::Current()
            .Resources()
            .Lookup(box_value(hstring{ resourceName }))
            .as<SolidColorBrush>()
            .Color();
    }

    Color MainWindow::color(
        std::uint8_t red,
        std::uint8_t green,
        std::uint8_t blue)
    {
        return ColorHelper::FromArgb(255, red, green, blue);
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

    MainWindow::RecordId MainWindow::recordIdFrom(
        Button const& button) const
    {
        return unbox_value<RecordId>(button.Tag());
    }

    fire_and_forget MainWindow::showAddAccountDialog()
    {
        auto lifetime{ get_strong() };

        ContentDialog dialog;
        dialog.XamlRoot(Content().XamlRoot());
        dialog.Title(box_value(L"Add launcher account"));
        dialog.PrimaryButtonText(L"Add");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(ContentDialogButton::Primary);

        StackPanel fields;
        fields.Spacing(12);

        ComboBox launcher;
        launcher.Header(box_value(L"Launcher"));
        launcher.PlaceholderText(L"Choose a launcher");
        for (auto const* name : { L"Steam", L"Riot", L"Epic", L"Other" })
        {
            ComboBoxItem item;
            item.Content(box_value(name));
            launcher.Items().Append(item);
        }

        TextBox accountId;
        accountId.Header(box_value(L"Launcher account ID"));
        accountId.PlaceholderText(L"Username, ID, or Riot ID");

        TextBox email;
        email.Header(box_value(L"Account email"));
        email.PlaceholderText(L"name@example.com");

        PasswordBox password;
        password.Header(box_value(L"Account / launcher password"));
        password.PlaceholderText(L"Password");

        fields.Children().Append(launcher);
        fields.Children().Append(accountId);
        fields.Children().Append(email);
        fields.Children().Append(password);
        dialog.Content(fields);

        if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
        {
            co_return;
        }

        if (launcher.SelectedIndex() < 0 ||
            accountId.Text().empty() ||
            email.Text().empty() ||
            password.Password().empty())
        {
            StatusText().Text(L"All four account fields are required");
            co_return;
        }

        const auto launcherItem = launcher.SelectedItem().as<ComboBoxItem>();
        const hstring launcherName =
            unbox_value<hstring>(launcherItem.Content());

        static_cast<void>(m_repository.add(
            launcherName.c_str(),
            accountId.Text().c_str(),
            email.Text().c_str(),
            password.Password().c_str()));

        refreshAccounts();
        StatusText().Text(L"Account added to memory");
    }

    fire_and_forget MainWindow::showColorDialog()
    {
        auto lifetime{ get_strong() };

        ContentDialog dialog;
        dialog.XamlRoot(Content().XamlRoot());
        dialog.Title(box_value(L"Custom color scheme"));
        dialog.PrimaryButtonText(L"Apply colors");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(ContentDialogButton::Primary);

        StackPanel fields;
        fields.Spacing(16);

        const auto makePicker = [&](hstring const& label, Color initial)
        {
            StackPanel group;
            group.Spacing(6);

            TextBlock title;
            title.Text(label);
            title.FontWeight(Windows::UI::Text::FontWeights::SemiBold());

            ColorPicker picker;
            picker.Color(initial);
            picker.IsAlphaEnabled(false);
            picker.IsColorChannelTextInputVisible(true);
            picker.IsHexInputVisible(true);
            picker.IsColorSpectrumVisible(true);
            picker.IsColorSliderVisible(true);

            group.Children().Append(title);
            group.Children().Append(picker);
            return std::pair{ group, picker };
        };

        auto [backgroundGroup, backgroundPicker] =
            makePicker(L"Background", brushColor(L"AppBackgroundBrush"));
        auto [surfaceGroup, surfacePicker] =
            makePicker(L"Account cards", brushColor(L"AppSurfaceBrush"));
        auto [accentGroup, accentPicker] =
            makePicker(L"Accent", brushColor(L"AppAccentBrush"));
        auto [textGroup, textPicker] =
            makePicker(L"Primary text", brushColor(L"AppTextBrush"));

        fields.Children().Append(backgroundGroup);
        fields.Children().Append(surfaceGroup);
        fields.Children().Append(accentGroup);
        fields.Children().Append(textGroup);

        ScrollViewer scroller;
        scroller.MaxHeight(520);
        scroller.Content(fields);
        dialog.Content(scroller);

        if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
        {
            co_return;
        }

        setBrushColor(L"AppBackgroundBrush", backgroundPicker.Color());
        setBrushColor(L"AppSurfaceBrush", surfacePicker.Color());
        setBrushColor(L"AppAccentBrush", accentPicker.Color());
        setBrushColor(L"AppTextBrush", textPicker.Color());

        ThemePicker().SelectedIndex(3);
        refreshAccounts();
        StatusText().Text(L"Custom color scheme applied for this session");
    }
}
