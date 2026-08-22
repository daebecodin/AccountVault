#include "pch.h"
#include "../MainWindow.xaml.h"

#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.UI.Text.h>

#include <string>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::ApplicationModel::DataTransfer;
using namespace Windows::Foundation;

namespace winrt::AccountVault::implementation
{
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
    MainWindow::RecordId MainWindow::recordIdFrom(
        Button const& button) const
    {
        return unbox_value<RecordId>(button.Tag());
    }
}
