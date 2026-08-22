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

        co_await dialog.ShowAsync();

        if (saved)
        {
            refreshAccounts();
            StatusText().Text(L"Account details updated");
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
