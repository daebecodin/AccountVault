#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.UI.Text.h>

#include <array>
#include <cmath>
#include <string>
#include <utility>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::ApplicationModel::DataTransfer;
using namespace Windows::Foundation;
using namespace Windows::UI;

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
            L"https://mail.google.com",
            L"demo-email-password"));
        static_cast<void>(m_repository.add(
            L"Riot",
            L"pixelpilot#NA1",
            L"demo-launcher-password",
            L"pilot@example.com",
            L"https://outlook.live.com",
            L"demo-email-password"));
        static_cast<void>(m_repository.add(
            L"Epic",
            L"orbit_runner",
            L"demo-launcher-password",
            L"orbit@example.com",
            L"https://mail.yahoo.com",
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

    void MainWindow::appendAccountCard(Account const& account)
    {
        Grid card;
        card.MinHeight(94);
        card.Padding(Thickness{ 18, 14, 18, 14 });
        card.Margin(Thickness{ 0, 0, 0, 12 });
        card.ColumnSpacing(22);
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
            GridLengthHelper::FromPixels(112));
        card.ColumnDefinitions().GetAt(1).Width(
            GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        card.ColumnDefinitions().GetAt(2).Width(
            GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        card.ColumnDefinitions().GetAt(3).Width(GridLengthHelper::Auto());

        Border launcherBadge;
        launcherBadge.Padding(Thickness{ 12, 7, 12, 7 });
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
        idPanel.Spacing(5);
        idPanel.VerticalAlignment(VerticalAlignment::Center);
        TextBlock idLabel;
        idLabel.Text(L"LAUNCHER USERNAME");
        idLabel.FontSize(11);
        idLabel.Foreground(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppMutedTextBrush"))
                .as<Brush>());
        TextBlock idValue;
        idValue.Text(account.launcherUsername);
        idValue.FontSize(15);
        idValue.Foreground(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppTextBrush"))
                .as<Brush>());
        idPanel.Children().Append(idLabel);
        idPanel.Children().Append(idValue);

        StackPanel emailPanel;
        emailPanel.Spacing(5);
        emailPanel.VerticalAlignment(VerticalAlignment::Center);
        TextBlock emailLabel;
        emailLabel.Text(L"EMAIL");
        emailLabel.FontSize(11);
        emailLabel.Foreground(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppMutedTextBrush"))
                .as<Brush>());
        TextBlock emailValue;
        emailValue.Text(account.emailAddress);
        emailValue.FontSize(15);
        emailValue.Foreground(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppTextBrush"))
                .as<Brush>());
        emailPanel.Children().Append(emailLabel);
        emailPanel.Children().Append(emailValue);

        Grid actions;
        actions.Width(440);
        actions.ColumnSpacing(8);
        actions.RowSpacing(8);
        actions.VerticalAlignment(VerticalAlignment::Center);

        for (int column = 0; column < 3; ++column)
        {
            ColumnDefinition definition;
            definition.Width(
                GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
            actions.ColumnDefinitions().Append(definition);
        }

        RowDefinition topActionRow;
        topActionRow.Height(GridLengthHelper::Auto());
        actions.RowDefinitions().Append(topActionRow);

        RowDefinition bottomActionRow;
        bottomActionRow.Height(GridLengthHelper::Auto());
        actions.RowDefinitions().Append(bottomActionRow);

        const auto makeButton = [&](hstring const& label)
        {
            Button button;
            button.Content(box_value(label));
            button.Tag(box_value(account.recordId));
            button.Height(34);
            button.Padding(Thickness{ 10, 0, 10, 0 });
            button.CornerRadius(CornerRadius{ 6, 6, 6, 6 });
            button.HorizontalAlignment(HorizontalAlignment::Stretch);
            button.HorizontalContentAlignment(HorizontalAlignment::Center);
            return button;
        };

        Button details{ makeButton(L"Details") };
        details.Click([this](IInspectable const& sender, RoutedEventArgs const&)
        {
            const auto button{ sender.as<Button>() };
            showAccountDetailsDialog(recordIdFrom(button));
        });

        Button copyUsername{ makeButton(L"Copy username") };
        copyUsername.Click([this](IInspectable const& sender, RoutedEventArgs const&)
        {
            const auto button{ sender.as<Button>() };
            const Account* account{ m_repository.find(recordIdFrom(button)) };
            if (account)
            {
                copyToClipboard(account->launcherUsername, L"Launcher username");
            }
        });

        Button copyEmail{ makeButton(L"Copy email") };
        copyEmail.Click([this](IInspectable const& sender, RoutedEventArgs const&)
        {
            const auto button{ sender.as<Button>() };
            const Account* account{ m_repository.find(recordIdFrom(button)) };
            if (account)
            {
                copyToClipboard(account->emailAddress, L"Email address");
            }
        });

        Button copyLauncherPassword{ makeButton(L"Copy launcher PW") };
        copyLauncherPassword.Click([this](IInspectable const& sender, RoutedEventArgs const&)
        {
            const auto button{ sender.as<Button>() };
            const Account* account{ m_repository.find(recordIdFrom(button)) };
            if (account)
            {
                copyToClipboard(account->launcherPassword, L"Launcher password");
            }
        });

        Button copyEmailPassword{ makeButton(L"Copy email PW") };
        copyEmailPassword.Click([this](IInspectable const& sender, RoutedEventArgs const&)
        {
            const auto button{ sender.as<Button>() };
            const Account* account{ m_repository.find(recordIdFrom(button)) };
            if (account)
            {
                copyToClipboard(account->emailPassword, L"Email password");
            }
        });

        Button remove{ makeButton(L"Remove") };
        remove.Click([this](IInspectable const& sender, RoutedEventArgs const&)
        {
            const auto button{ sender.as<Button>() };
            removeAccount(recordIdFrom(button));
        });

        Grid::SetColumn(details, 0);
        Grid::SetColumn(copyUsername, 1);
        Grid::SetColumn(copyEmail, 2);
        Grid::SetRow(copyLauncherPassword, 1);
        Grid::SetColumn(copyLauncherPassword, 0);
        Grid::SetRow(copyEmailPassword, 1);
        Grid::SetColumn(copyEmailPassword, 1);
        Grid::SetRow(remove, 1);
        Grid::SetColumn(remove, 2);

        actions.Children().Append(details);
        actions.Children().Append(copyUsername);
        actions.Children().Append(copyEmail);
        actions.Children().Append(copyLauncherPassword);
        actions.Children().Append(copyEmailPassword);
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

    fire_and_forget MainWindow::showAccountDetailsDialog(RecordId id)
    {
        auto lifetime{ get_strong() };

        const Account* account{ m_repository.find(id) };
        if (!account)
        {
            StatusText().Text(L"That account no longer exists");
            co_return;
        }

        ContentDialog dialog;
        dialog.XamlRoot(Content().XamlRoot());
        dialog.Title(box_value(L"Account details"));
        dialog.PrimaryButtonText(L"Edit");
        dialog.CloseButtonText(L"Close");
        dialog.MaxWidth(900);
        dialog.HorizontalAlignment(HorizontalAlignment::Center);
        dialog.VerticalAlignment(VerticalAlignment::Center);

        StackPanel fields;
        fields.Spacing(16);

        Grid sections;
        sections.ColumnSpacing(20);

        ColumnDefinition launcherColumn;
        launcherColumn.Width(
            GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        sections.ColumnDefinitions().Append(launcherColumn);

        ColumnDefinition separatorColumn;
        separatorColumn.Width(GridLengthHelper::Auto());
        sections.ColumnDefinitions().Append(separatorColumn);

        ColumnDefinition emailColumn;
        emailColumn.Width(
            GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        sections.ColumnDefinitions().Append(emailColumn);

        StackPanel launcherFields;
        launcherFields.Spacing(12);

        StackPanel emailFields;
        emailFields.Spacing(12);

        TextBlock launcherHeading;
        launcherHeading.Text(L"LAUNCHER ACCOUNT");
        launcherHeading.FontFamily(FontFamily{ L"Cascadia Mono" });
        launcherHeading.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        launcherHeading.Foreground(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppAccentBrush"))
                .as<Brush>());

        ComboBox launcher;
        launcher.Header(box_value(L"Launcher"));
        launcher.IsEnabled(false);
        for (auto const* name : { L"Steam", L"Riot", L"Epic", L"Other" })
        {
            ComboBoxItem item;
            item.Content(box_value(name));
            launcher.Items().Append(item);
        }

        if (account->launcher == L"Steam")
        {
            launcher.SelectedIndex(0);
        }
        else if (account->launcher == L"Riot")
        {
            launcher.SelectedIndex(1);
        }
        else if (account->launcher == L"Epic")
        {
            launcher.SelectedIndex(2);
        }
        else
        {
            launcher.SelectedIndex(3);
        }

        TextBox launcherUsername;
        launcherUsername.Header(box_value(L"Launcher username / account ID"));
        launcherUsername.Text(account->launcherUsername);
        launcherUsername.IsReadOnly(true);

        TextBox launcherPassword;
        launcherPassword.Header(box_value(L"Launcher password"));
        launcherPassword.Text(account->launcherPassword);
        launcherPassword.IsReadOnly(true);

        TextBlock linkedEmail;
        std::wstring linkedEmailText{ L"Linked email: " };
        linkedEmailText += account->emailAddress;
        linkedEmail.Text(hstring{ linkedEmailText });
        linkedEmail.Foreground(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppMutedTextBrush"))
                .as<Brush>());

        Border separator;
        separator.Width(1);
        separator.Margin(Thickness{ 0, 4, 0, 4 });
        separator.VerticalAlignment(VerticalAlignment::Stretch);
        separator.Background(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppBorderBrush"))
                .as<Brush>());

        TextBlock emailHeading;
        emailHeading.Text(L"EMAIL ACCOUNT");
        emailHeading.FontFamily(FontFamily{ L"Cascadia Mono" });
        emailHeading.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        emailHeading.Foreground(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppAccentBrush"))
                .as<Brush>());

        TextBox emailProviderWebsite;
        emailProviderWebsite.Header(box_value(L"Provider website"));
        emailProviderWebsite.Text(account->emailProviderWebsite);
        emailProviderWebsite.IsReadOnly(true);

        TextBox emailAddress;
        emailAddress.Header(box_value(L"Email address (shared with launcher)"));
        emailAddress.Text(account->emailAddress);
        emailAddress.IsReadOnly(true);
        emailAddress.TextChanged(
            [&linkedEmail](IInspectable const& sender, TextChangedEventArgs const&)
            {
                const auto textBox{ sender.as<TextBox>() };
                std::wstring mirrorText{ L"Linked email: " };
                mirrorText += textBox.Text().c_str();
                linkedEmail.Text(hstring{ mirrorText });
            });

        TextBox emailPassword;
        emailPassword.Header(box_value(L"Email password"));
        emailPassword.Text(account->emailPassword);
        emailPassword.IsReadOnly(true);

        TextBlock validation;
        validation.Visibility(Visibility::Collapsed);
        SolidColorBrush validationBrush;
        validationBrush.Color(color(248, 81, 73));
        validation.Foreground(validationBrush);

        launcherFields.Children().Append(launcherHeading);
        launcherFields.Children().Append(launcher);
        launcherFields.Children().Append(launcherUsername);
        launcherFields.Children().Append(launcherPassword);
        launcherFields.Children().Append(linkedEmail);

        emailFields.Children().Append(emailHeading);
        emailFields.Children().Append(emailProviderWebsite);
        emailFields.Children().Append(emailAddress);
        emailFields.Children().Append(emailPassword);

        Grid::SetColumn(launcherFields, 0);
        Grid::SetColumn(separator, 1);
        Grid::SetColumn(emailFields, 2);

        sections.Children().Append(launcherFields);
        sections.Children().Append(separator);
        sections.Children().Append(emailFields);

        fields.Children().Append(sections);
        fields.Children().Append(validation);

        ScrollViewer detailsScroller;
        detailsScroller.MaxHeight(560);
        detailsScroller.HorizontalScrollBarVisibility(
            ScrollBarVisibility::Disabled);
        detailsScroller.VerticalScrollBarVisibility(
            ScrollBarVisibility::Auto);
        detailsScroller.HorizontalContentAlignment(
            HorizontalAlignment::Stretch);
        detailsScroller.Content(fields);
        dialog.Content(detailsScroller);

        bool editing{ false };
        bool saved{ false };

        dialog.PrimaryButtonClick(
            [&, this](ContentDialog const& sender, ContentDialogButtonClickEventArgs const& args)
            {
                if (!editing)
                {
                    args.Cancel(true);
                    editing = true;
                    launcher.IsEnabled(true);
                    launcherUsername.IsReadOnly(false);
                    launcherPassword.IsReadOnly(false);
                    emailProviderWebsite.IsReadOnly(false);
                    emailAddress.IsReadOnly(false);
                    emailPassword.IsReadOnly(false);
                    sender.PrimaryButtonText(L"Save changes");
                    launcherUsername.Focus(FocusState::Programmatic);
                    return;
                }

                if (launcher.SelectedIndex() < 0 ||
                    launcherUsername.Text().empty() ||
                    launcherPassword.Text().empty() ||
                    emailProviderWebsite.Text().empty() ||
                    emailAddress.Text().empty() ||
                    emailPassword.Text().empty())
                {
                    args.Cancel(true);
                    validation.Text(L"All launcher and email fields are required.");
                    validation.Visibility(Visibility::Visible);
                    return;
                }

                const auto launcherItem = launcher.SelectedItem().as<ComboBoxItem>();
                const hstring launcherName =
                    unbox_value<hstring>(launcherItem.Content());

                saved = m_repository.update(
                    id,
                    launcherName.c_str(),
                    launcherUsername.Text().c_str(),
                    launcherPassword.Text().c_str(),
                    emailAddress.Text().c_str(),
                    emailProviderWebsite.Text().c_str(),
                    emailPassword.Text().c_str());

                if (!saved)
                {
                    args.Cancel(true);
                    validation.Text(L"The account could not be updated.");
                    validation.Visibility(Visibility::Visible);
                }
            });

        Grid::SetRowSpan(dialog, 4);
        RootGrid().Children().Append(dialog);

        co_await dialog.ShowAsync(ContentDialogPlacement::InPlace);

        auto rootChildren{ RootGrid().Children() };
        std::uint32_t dialogIndex{};
        if (rootChildren.IndexOf(dialog, dialogIndex))
        {
            rootChildren.RemoveAt(dialogIndex);
        }

        if (saved)
        {
            refreshAccounts();
            StatusText().Text(L"Account details updated");
        }
    }

    void MainWindow::applyPreset(int selectedIndex)
    {
        ThemeDefinition theme{};

        switch (selectedIndex)
        {
        case 0: // Catppuccin Mocha
            theme = ThemeDefinition{
                L"Catppuccin Mocha",
                color(30, 30, 46),
                color(24, 24, 37),
                color(49, 50, 68),
                color(203, 166, 247),
                color(205, 214, 244),
                color(166, 173, 200) };
            break;

        case 1: // Tokyo Night
            theme = ThemeDefinition{
                L"Tokyo Night",
                color(26, 27, 38),
                color(36, 40, 59),
                color(65, 72, 104),
                color(122, 162, 247),
                color(192, 202, 245),
                color(86, 95, 137) };
            break;

        case 2: // Dracula
            theme = ThemeDefinition{
                L"Dracula",
                color(40, 42, 54),
                color(52, 55, 70),
                color(68, 71, 90),
                color(189, 147, 249),
                color(248, 248, 242),
                color(98, 114, 164) };
            break;

        case 3: // Ayu Mirage
            theme = ThemeDefinition{
                L"Ayu Mirage",
                color(31, 36, 48),
                color(36, 41, 54),
                color(50, 56, 68),
                color(255, 173, 102),
                color(204, 202, 194),
                color(112, 122, 140) };
            break;

        case 4: // Dainty Dark
            theme = ThemeDefinition{
                L"Dainty Dark",
                color(18, 24, 34),
                color(24, 32, 44),
                color(35, 45, 60),
                color(92, 207, 230),
                color(215, 218, 224),
                color(127, 140, 152) };
            break;

        case 5: // GitHub Dark Default
            theme = ThemeDefinition{
                L"GitHub Dark",
                color(13, 17, 23),
                color(22, 27, 34),
                color(33, 38, 45),
                color(88, 166, 255),
                color(201, 209, 217),
                color(139, 148, 158) };
            break;

        case 6: // Atom One Dark
            theme = ThemeDefinition{
                L"Atom One Dark",
                color(40, 44, 52),
                color(33, 37, 43),
                color(44, 49, 60),
                color(97, 175, 239),
                color(171, 178, 191),
                color(92, 99, 112) };
            break;

        case 7: // Houston
            theme = ThemeDefinition{
                L"Houston",
                color(13, 17, 23),
                color(19, 26, 36),
                color(28, 38, 51),
                color(34, 211, 238),
                color(225, 232, 240),
                color(107, 114, 128) };
            break;

        case 8: // Night Owl
            theme = ThemeDefinition{
                L"Night Owl",
                color(1, 22, 39),
                color(1, 17, 29),
                color(11, 41, 66),
                color(130, 170, 255),
                color(214, 222, 235),
                color(99, 119, 119) };
            break;

        case 9: // Matcha
            theme = ThemeDefinition{
                L"Matcha",
                color(39, 49, 54),
                color(47, 59, 63),
                color(58, 71, 75),
                color(126, 176, 138),
                color(209, 222, 211),
                color(126, 164, 176) };
            break;

        default:
        {
            if (selectedIndex < BuiltInThemeCount)
            {
                return;
            }

            const auto customIndex = static_cast<std::size_t>(
                selectedIndex - BuiltInThemeCount);
            if (customIndex >= m_customThemes.size())
            {
                return;
            }

            theme = m_customThemes[customIndex];
            break;
        }
        }

        applyTheme(theme);
        refreshAccounts();
    }

    void MainWindow::applyTheme(ThemeDefinition const& theme)
    {
        setBrushColor(L"AppBackgroundBrush", theme.background);
        setBrushColor(L"AppSurfaceBrush", theme.surface);
        setBrushColor(L"AppSurfaceAltBrush", theme.surfaceAlt);
        setBrushColor(L"AppAccentBrush", theme.accent);
        setBrushColor(L"AppTextBrush", theme.text);
        setBrushColor(L"AppMutedTextBrush", theme.mutedText);
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
        dialog.Title(box_value(L"Add account"));
        dialog.PrimaryButtonText(L"Add");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(ContentDialogButton::Primary);

        StackPanel fields;
        fields.Spacing(12);

        TextBlock launcherHeading;
        launcherHeading.Text(L"LAUNCHER ACCOUNT");
        launcherHeading.FontFamily(FontFamily{ L"Cascadia Mono" });
        launcherHeading.FontWeight(Windows::UI::Text::FontWeights::SemiBold());

        ComboBox launcher;
        launcher.Header(box_value(L"Launcher"));
        launcher.PlaceholderText(L"Choose a launcher");
        for (auto const* name : { L"Steam", L"Riot", L"Epic", L"Other" })
        {
            ComboBoxItem item;
            item.Content(box_value(name));
            launcher.Items().Append(item);
        }

        TextBox launcherUsername;
        launcherUsername.Header(box_value(L"Launcher username / account ID"));
        launcherUsername.PlaceholderText(L"Username, ID, or Riot ID");

        PasswordBox launcherPassword;
        launcherPassword.Header(box_value(L"Launcher password"));
        launcherPassword.PlaceholderText(L"Launcher password");

        TextBlock emailHeading;
        emailHeading.Text(L"EMAIL ACCOUNT");
        emailHeading.FontFamily(FontFamily{ L"Cascadia Mono" });
        emailHeading.FontWeight(Windows::UI::Text::FontWeights::SemiBold());

        TextBox emailProviderWebsite;
        emailProviderWebsite.Header(box_value(L"Provider website"));
        emailProviderWebsite.PlaceholderText(L"https://mail.google.com");

        TextBox emailAddress;
        emailAddress.Header(box_value(L"Email address (shared with launcher)"));
        emailAddress.PlaceholderText(L"name@example.com");

        PasswordBox emailPassword;
        emailPassword.Header(box_value(L"Email password"));
        emailPassword.PlaceholderText(L"Email password");

        fields.Children().Append(launcherHeading);
        fields.Children().Append(launcher);
        fields.Children().Append(launcherUsername);
        fields.Children().Append(launcherPassword);
        fields.Children().Append(emailHeading);
        fields.Children().Append(emailProviderWebsite);
        fields.Children().Append(emailAddress);
        fields.Children().Append(emailPassword);
        dialog.Content(fields);

        if (co_await dialog.ShowAsync() != ContentDialogResult::Primary)
        {
            co_return;
        }

        if (launcher.SelectedIndex() < 0 ||
            launcherUsername.Text().empty() ||
            launcherPassword.Password().empty() ||
            emailProviderWebsite.Text().empty() ||
            emailAddress.Text().empty() ||
            emailPassword.Password().empty())
        {
            StatusText().Text(L"All launcher and email fields are required");
            co_return;
        }

        const auto launcherItem = launcher.SelectedItem().as<ComboBoxItem>();
        const hstring launcherName =
            unbox_value<hstring>(launcherItem.Content());

        static_cast<void>(m_repository.add(
            launcherName.c_str(),
            launcherUsername.Text().c_str(),
            launcherPassword.Password().c_str(),
            emailAddress.Text().c_str(),
            emailProviderWebsite.Text().c_str(),
            emailPassword.Password().c_str()));

        refreshAccounts();
        StatusText().Text(L"Account added to memory");
    }

    fire_and_forget MainWindow::showColorDialog()
    {
        auto lifetime{ get_strong() };

        // Theme editor layout revision v10: compact token cards and one
        // horizontal HEX/R/G/B input row below the color spectrum.

        ContentDialog dialog;
        dialog.XamlRoot(Content().XamlRoot());
        dialog.Title(box_value(L"Create theme"));
        dialog.PrimaryButtonText(L"Save theme");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(ContentDialogButton::Primary);
        dialog.HorizontalAlignment(HorizontalAlignment::Center);
        dialog.VerticalAlignment(VerticalAlignment::Center);

        // WinUI's default ContentDialog width is intentionally narrow. This
        // editor uses a wider local template limit without changing other dialogs.
        dialog.Resources().Insert(
            box_value(L"ContentDialogMaxWidth"),
            box_value(1040.0));
        dialog.Resources().Insert(
            box_value(L"ContentDialogMinWidth"),
            box_value(980.0));

        ThemeDefinition draft{
            L"",
            brushColor(L"AppBackgroundBrush"),
            brushColor(L"AppSurfaceBrush"),
            brushColor(L"AppSurfaceAltBrush"),
            brushColor(L"AppAccentBrush"),
            brushColor(L"AppTextBrush"),
            brushColor(L"AppMutedTextBrush") };

        StackPanel dialogContent;
        dialogContent.Spacing(16);

        TextBox themeName;
        themeName.Header(box_value(L"Theme name"));
        themeName.PlaceholderText(L"My custom theme");

        TextBlock validation;
        validation.Text(L"Enter a name for the theme.");
        validation.Visibility(Visibility::Collapsed);
        SolidColorBrush validationBrush;
        validationBrush.Color(color(248, 81, 73));
        validation.Foreground(validationBrush);

        const std::array<std::wstring_view, 6> tokenNames{
            L"Background",
            L"Account cards",
            L"Alternate surface",
            L"Accent",
            L"Primary text",
            L"Muted text" };

        const auto colorAt = [&](int index) -> Color
        {
            switch (index)
            {
            case 0: return draft.background;
            case 1: return draft.surface;
            case 2: return draft.surfaceAlt;
            case 3: return draft.accent;
            case 4: return draft.text;
            case 5: return draft.mutedText;
            default: return draft.background;
            }
        };

        const auto setColorAt = [&](int index, Color value)
        {
            switch (index)
            {
            case 0: draft.background = value; break;
            case 1: draft.surface = value; break;
            case 2: draft.surfaceAlt = value; break;
            case 3: draft.accent = value; break;
            case 4: draft.text = value; break;
            case 5: draft.mutedText = value; break;
            default: break;
            }
        };

        const auto colorHex = [](Color value)
        {
            constexpr wchar_t digits[] = L"0123456789ABCDEF";
            std::wstring result{ L"#000000" };
            result[1] = digits[(value.R >> 4) & 0x0F];
            result[2] = digits[value.R & 0x0F];
            result[3] = digits[(value.G >> 4) & 0x0F];
            result[4] = digits[value.G & 0x0F];
            result[5] = digits[(value.B >> 4) & 0x0F];
            result[6] = digits[value.B & 0x0F];
            return hstring{ result };
        };

        const auto tryParseHex = [](hstring const& text, Color& parsed)
        {
            const std::wstring_view value{ text.c_str(), text.size() };
            if (value.size() != 7 || value[0] != L'#')
            {
                return false;
            }

            const auto digitValue = [](wchar_t digit) -> int
            {
                if (digit >= L'0' && digit <= L'9')
                {
                    return digit - L'0';
                }
                if (digit >= L'A' && digit <= L'F')
                {
                    return digit - L'A' + 10;
                }
                if (digit >= L'a' && digit <= L'f')
                {
                    return digit - L'a' + 10;
                }
                return -1;
            };

            std::array<int, 6> digits{};
            for (std::size_t index = 0; index < digits.size(); ++index)
            {
                digits[index] = digitValue(value[index + 1]);
                if (digits[index] < 0)
                {
                    return false;
                }
            }

            parsed = ColorHelper::FromArgb(
                255,
                static_cast<std::uint8_t>(digits[0] * 16 + digits[1]),
                static_cast<std::uint8_t>(digits[2] * 16 + digits[3]),
                static_cast<std::uint8_t>(digits[4] * 16 + digits[5]));
            return true;
        };

        const auto makeSolidBrush = [](Color value)
        {
            SolidColorBrush brush;
            brush.Color(value);
            return brush;
        };

        SolidColorBrush previewBackgroundBrush{ makeSolidBrush(draft.background) };
        SolidColorBrush previewSurfaceBrush{ makeSolidBrush(draft.surface) };
        SolidColorBrush previewSurfaceAltBrush{ makeSolidBrush(draft.surfaceAlt) };
        SolidColorBrush previewAccentBrush{ makeSolidBrush(draft.accent) };
        SolidColorBrush previewTextBrush{ makeSolidBrush(draft.text) };
        SolidColorBrush previewMutedTextBrush{ makeSolidBrush(draft.mutedText) };

        const auto updatePreview = [&]()
        {
            previewBackgroundBrush.Color(draft.background);
            previewSurfaceBrush.Color(draft.surface);
            previewSurfaceAltBrush.Color(draft.surfaceAlt);
            previewAccentBrush.Color(draft.accent);
            previewTextBrush.Color(draft.text);
            previewMutedTextBrush.Color(draft.mutedText);
        };

        Grid editor;
        editor.Width(920);
        editor.ColumnSpacing(24);

        ColumnDefinition tokenColumn;
        tokenColumn.Width(GridLengthHelper::FromPixels(220));
        editor.ColumnDefinitions().Append(tokenColumn);

        ColumnDefinition pickerColumn;
        pickerColumn.Width(
            GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        editor.ColumnDefinitions().Append(pickerColumn);

        ColumnDefinition previewColumn;
        previewColumn.Width(GridLengthHelper::FromPixels(280));
        editor.ColumnDefinitions().Append(previewColumn);

        StackPanel tokenPanel;
        tokenPanel.Spacing(6);

        TextBlock tokenHeading;
        tokenHeading.Text(L"THEME COLORS");
        tokenHeading.FontFamily(FontFamily{ L"Cascadia Mono" });
        tokenHeading.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        tokenHeading.Foreground(previewAccentBrush);
        tokenPanel.Children().Append(tokenHeading);

        ColorPicker picker;
        picker.Color(draft.background);
        picker.MaxHeight(345);
        picker.IsAlphaEnabled(false);
        picker.IsColorChannelTextInputVisible(false);
        picker.IsHexInputVisible(false);
        picker.IsColorSpectrumVisible(true);
        picker.IsColorSliderVisible(true);

        TextBox hexInput;
        hexInput.Header(box_value(L"HEX"));
        hexInput.Text(colorHex(draft.background));

        NumberBox redInput;
        redInput.Header(box_value(L"R"));
        redInput.Minimum(0);
        redInput.Maximum(255);
        redInput.SmallChange(1);
        redInput.Value(draft.background.R);

        NumberBox greenInput;
        greenInput.Header(box_value(L"G"));
        greenInput.Minimum(0);
        greenInput.Maximum(255);
        greenInput.SmallChange(1);
        greenInput.Value(draft.background.G);

        NumberBox blueInput;
        blueInput.Header(box_value(L"B"));
        blueInput.Minimum(0);
        blueInput.Maximum(255);
        blueInput.SmallChange(1);
        blueInput.Value(draft.background.B);

        Grid colorInputs;
        colorInputs.ColumnSpacing(8);

        ColumnDefinition hexInputColumn;
        hexInputColumn.Width(GridLengthHelper::FromPixels(112));
        colorInputs.ColumnDefinitions().Append(hexInputColumn);

        for (int column = 0; column < 3; ++column)
        {
            ColumnDefinition channelColumn;
            channelColumn.Width(
                GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
            colorInputs.ColumnDefinitions().Append(channelColumn);
        }

        Grid::SetColumn(hexInput, 0);
        Grid::SetColumn(redInput, 1);
        Grid::SetColumn(greenInput, 2);
        Grid::SetColumn(blueInput, 3);
        colorInputs.Children().Append(hexInput);
        colorInputs.Children().Append(redInput);
        colorInputs.Children().Append(greenInput);
        colorInputs.Children().Append(blueInput);

        int selectedToken{ 0 };
        bool synchronizingPicker{ false };
        bool synchronizingInputs{ false };

        const auto updateInputs = [&](Color value)
        {
            synchronizingInputs = true;
            hexInput.Text(colorHex(value));
            redInput.Value(value.R);
            greenInput.Value(value.G);
            blueInput.Value(value.B);
            synchronizingInputs = false;
        };

        const auto commitRgbInputs = [&]()
        {
            if (synchronizingInputs ||
                std::isnan(redInput.Value()) ||
                std::isnan(greenInput.Value()) ||
                std::isnan(blueInput.Value()))
            {
                return;
            }

            picker.Color(ColorHelper::FromArgb(
                255,
                static_cast<std::uint8_t>(redInput.Value()),
                static_cast<std::uint8_t>(greenInput.Value()),
                static_cast<std::uint8_t>(blueInput.Value())));
        };

        redInput.ValueChanged(
            [&](NumberBox const&, NumberBoxValueChangedEventArgs const&)
            {
                commitRgbInputs();
            });
        greenInput.ValueChanged(
            [&](NumberBox const&, NumberBoxValueChangedEventArgs const&)
            {
                commitRgbInputs();
            });
        blueInput.ValueChanged(
            [&](NumberBox const&, NumberBoxValueChangedEventArgs const&)
            {
                commitRgbInputs();
            });

        hexInput.TextChanged(
            [&](IInspectable const&, TextChangedEventArgs const&)
            {
                if (synchronizingInputs)
                {
                    return;
                }

                Color parsed{};
                if (tryParseHex(hexInput.Text(), parsed))
                {
                    picker.Color(parsed);
                }
            });

        TextBlock selectedTokenHeading;
        selectedTokenHeading.Text(hstring{ tokenNames[0] });
        selectedTokenHeading.FontSize(18);
        selectedTokenHeading.FontWeight(
            Windows::UI::Text::FontWeights::SemiBold());

        TextBlock pickerHelp;
        pickerHelp.Text(L"Changes update the preview immediately.");
        pickerHelp.Foreground(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppMutedTextBrush"))
                .as<Brush>());

        StackPanel pickerPanel;
        pickerPanel.Spacing(10);
        pickerPanel.Children().Append(selectedTokenHeading);
        pickerPanel.Children().Append(pickerHelp);
        pickerPanel.Children().Append(picker);
        pickerPanel.Children().Append(colorInputs);

        StackPanel previewPanel;
        previewPanel.Spacing(10);

        TextBlock previewHeading;
        previewHeading.Text(L"LIVE PREVIEW");
        previewHeading.FontFamily(FontFamily{ L"Cascadia Mono" });
        previewHeading.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        previewHeading.Foreground(previewAccentBrush);

        Border previewFrame;
        // Keep the preview's outer box, inner content box, and card box
        // explicit so the card cannot consume or clip the frame padding.
        previewFrame.Width(280);
        previewFrame.MinHeight(260);
        previewFrame.Padding(Thickness{ 16 });
        previewFrame.CornerRadius(CornerRadius{ 8, 8, 8, 8 });
        previewFrame.Background(previewBackgroundBrush);
        previewFrame.BorderBrush(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppBorderBrush"))
                .as<Brush>());
        previewFrame.BorderThickness(Thickness{ 1 });

        StackPanel previewBody;
        previewBody.Width(246);
        previewBody.Spacing(12);

        TextBlock previewTitle;
        previewTitle.Text(L"Launcher accounts");
        previewTitle.FontSize(20);
        previewTitle.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        previewTitle.Foreground(previewTextBrush);

        TextBlock previewSubtitle;
        previewSubtitle.Text(L"A preview of the active palette.");
        previewSubtitle.TextWrapping(TextWrapping::Wrap);
        previewSubtitle.Foreground(previewMutedTextBrush);

        Border previewCard;
        previewCard.Width(246);
        previewCard.Padding(Thickness{ 12 });
        previewCard.CornerRadius(CornerRadius{ 6, 6, 6, 6 });
        previewCard.Background(previewSurfaceBrush);
        previewCard.BorderBrush(previewSurfaceAltBrush);
        previewCard.BorderThickness(Thickness{ 1 });

        StackPanel previewCardBody;
        previewCardBody.Width(220);
        previewCardBody.Spacing(8);

        Border previewBadge;
        previewBadge.Padding(Thickness{ 9, 5, 9, 5 });
        previewBadge.CornerRadius(CornerRadius{ 5, 5, 5, 5 });
        previewBadge.HorizontalAlignment(HorizontalAlignment::Left);
        previewBadge.Background(previewSurfaceAltBrush);

        TextBlock previewBadgeText;
        previewBadgeText.Text(L"Steam");
        previewBadgeText.FontFamily(FontFamily{ L"Cascadia Mono" });
        previewBadgeText.FontWeight(
            Windows::UI::Text::FontWeights::SemiBold());
        previewBadgeText.Foreground(previewAccentBrush);
        previewBadge.Child(previewBadgeText);

        TextBlock previewUsername;
        previewUsername.Text(L"night_shift");
        previewUsername.Foreground(previewTextBrush);

        TextBlock previewEmail;
        previewEmail.Text(L"night@example.com");
        previewEmail.Foreground(previewMutedTextBrush);

        Border previewAction;
        previewAction.Padding(Thickness{ 10, 7, 10, 7 });
        previewAction.HorizontalAlignment(HorizontalAlignment::Stretch);
        previewAction.CornerRadius(CornerRadius{ 5, 5, 5, 5 });
        previewAction.Background(previewSurfaceAltBrush);

        TextBlock previewActionText;
        previewActionText.Text(L"Copy password");
        previewActionText.HorizontalAlignment(HorizontalAlignment::Center);
        previewActionText.Foreground(previewAccentBrush);
        previewAction.Child(previewActionText);

        previewCardBody.Children().Append(previewBadge);
        previewCardBody.Children().Append(previewUsername);
        previewCardBody.Children().Append(previewEmail);
        previewCardBody.Children().Append(previewAction);
        previewCard.Child(previewCardBody);

        previewBody.Children().Append(previewTitle);
        previewBody.Children().Append(previewSubtitle);
        previewBody.Children().Append(previewCard);
        previewFrame.Child(previewBody);

        previewPanel.Children().Append(previewHeading);
        previewPanel.Children().Append(previewFrame);

        std::vector<Button> tokenButtons;
        std::vector<SolidColorBrush> tokenSwatchBrushes;
        std::vector<TextBlock> tokenHexLabels;
        tokenButtons.reserve(tokenNames.size());
        tokenSwatchBrushes.reserve(tokenNames.size());
        tokenHexLabels.reserve(tokenNames.size());

        const Brush normalTokenBrush =
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppSurfaceBrush"))
                .as<Brush>();
        const Brush selectedTokenBrush =
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppSurfaceAltBrush"))
                .as<Brush>();

        for (int index = 0; index < static_cast<int>(tokenNames.size()); ++index)
        {
            Button tokenButton;
            tokenButton.MinHeight(56);
            tokenButton.Padding(Thickness{ 10, 6, 10, 6 });
            tokenButton.HorizontalAlignment(HorizontalAlignment::Stretch);
            tokenButton.HorizontalContentAlignment(HorizontalAlignment::Stretch);
            tokenButton.Background(
                index == 0 ? selectedTokenBrush : normalTokenBrush);

            Grid tokenContent;
            tokenContent.ColumnSpacing(8);

            RowDefinition labelRow;
            labelRow.Height(GridLengthHelper::Auto());
            tokenContent.RowDefinitions().Append(labelRow);

            RowDefinition hexRow;
            hexRow.Height(GridLengthHelper::Auto());
            tokenContent.RowDefinitions().Append(hexRow);

            ColumnDefinition swatchColumn;
            swatchColumn.Width(GridLengthHelper::FromPixels(22));
            tokenContent.ColumnDefinitions().Append(swatchColumn);

            ColumnDefinition labelColumn;
            labelColumn.Width(
                GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
            tokenContent.ColumnDefinitions().Append(labelColumn);

            SolidColorBrush swatchBrush{ makeSolidBrush(colorAt(index)) };

            Border swatch;
            swatch.Width(20);
            swatch.Height(20);
            swatch.CornerRadius(CornerRadius{ 4, 4, 4, 4 });
            swatch.Background(swatchBrush);

            TextBlock tokenLabel;
            tokenLabel.Text(hstring{ tokenNames[index] });
            tokenLabel.VerticalAlignment(VerticalAlignment::Center);

            TextBlock hexLabel;
            hexLabel.Text(colorHex(colorAt(index)));
            hexLabel.FontFamily(FontFamily{ L"Cascadia Mono" });
            hexLabel.FontSize(11);
            hexLabel.Foreground(
                Application::Current()
                    .Resources()
                    .Lookup(box_value(L"AppMutedTextBrush"))
                    .as<Brush>());

            Grid::SetColumn(swatch, 0);
            Grid::SetRowSpan(swatch, 2);
            Grid::SetColumn(tokenLabel, 1);
            Grid::SetRow(hexLabel, 1);
            Grid::SetColumn(hexLabel, 1);
            tokenContent.Children().Append(swatch);
            tokenContent.Children().Append(tokenLabel);
            tokenContent.Children().Append(hexLabel);
            tokenButton.Content(tokenContent);

            tokenButton.Click(
                [&, index](IInspectable const&, RoutedEventArgs const&)
                {
                    selectedToken = index;
                    selectedTokenHeading.Text(hstring{ tokenNames[index] });

                    for (int buttonIndex = 0;
                        buttonIndex < static_cast<int>(tokenButtons.size());
                        ++buttonIndex)
                    {
                        tokenButtons[buttonIndex].Background(
                            buttonIndex == index
                                ? selectedTokenBrush
                                : normalTokenBrush);
                    }

                    synchronizingPicker = true;
                    const Color selectedColor{ colorAt(index) };
                    picker.Color(selectedColor);
                    updateInputs(selectedColor);
                    synchronizingPicker = false;
                });

            tokenButtons.push_back(tokenButton);
            tokenSwatchBrushes.push_back(swatchBrush);
            tokenHexLabels.push_back(hexLabel);
            tokenPanel.Children().Append(tokenButton);
        }

        picker.ColorChanged(
            [&](ColorPicker const&, ColorChangedEventArgs const& args)
            {
                if (synchronizingPicker)
                {
                    return;
                }

                setColorAt(selectedToken, args.NewColor());
                tokenSwatchBrushes[selectedToken].Color(args.NewColor());
                tokenHexLabels[selectedToken].Text(colorHex(args.NewColor()));
                updateInputs(args.NewColor());
                updatePreview();
            });

        Grid::SetColumn(tokenPanel, 0);
        Grid::SetColumn(pickerPanel, 1);
        Grid::SetColumn(previewPanel, 2);
        editor.Children().Append(tokenPanel);
        editor.Children().Append(pickerPanel);
        editor.Children().Append(previewPanel);

        dialogContent.Children().Append(themeName);
        dialogContent.Children().Append(validation);
        dialogContent.Children().Append(editor);
        dialog.Content(dialogContent);

        dialog.PrimaryButtonClick(
            [&](ContentDialog const&, ContentDialogButtonClickEventArgs const& args)
            {
                if (themeName.Text().empty())
                {
                    args.Cancel(true);
                    validation.Visibility(Visibility::Visible);
                    themeName.Focus(FocusState::Programmatic);
                }
            });

        Grid::SetRowSpan(dialog, 4);
        RootGrid().Children().Append(dialog);

        const ContentDialogResult result =
            co_await dialog.ShowAsync(ContentDialogPlacement::InPlace);

        auto rootChildren{ RootGrid().Children() };
        std::uint32_t dialogIndex{};
        if (rootChildren.IndexOf(dialog, dialogIndex))
        {
            rootChildren.RemoveAt(dialogIndex);
        }

        if (result != ContentDialogResult::Primary)
        {
            co_return;
        }

        draft.name = themeName.Text().c_str();
        m_customThemes.push_back(draft);

        ComboBoxItem item;
        item.Content(box_value(hstring{ draft.name }));
        ThemePicker().Items().Append(item);
        ThemePicker().SelectedIndex(
            BuiltInThemeCount +
            static_cast<int>(m_customThemes.size()) - 1);

        std::wstring status{ L"Theme created: " };
        status += draft.name;
        status += L" (session only)";
        StatusText().Text(hstring{ status });
    }
}
