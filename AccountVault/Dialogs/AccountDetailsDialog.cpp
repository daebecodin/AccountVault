#include "pch.h"
#include "../MainWindow.xaml.h"
#include "../Services/EmailProviderCatalog.h"

#include <winrt/Windows.UI.Text.h>

#include <optional>
#include <string>
#include <utility>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::Foundation;

namespace winrt::AccountVault::implementation
{
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

        PasswordBox launcherPassword;
        launcherPassword.Header(box_value(L"New launcher password (optional)"));
        launcherPassword.PlaceholderText(L"Stored securely; leave blank to keep it");
        launcherPassword.IsEnabled(false);
        launcherPassword.Visibility(Visibility::Collapsed);

        StackPanel launcherRevealPanel;
        launcherRevealPanel.Spacing(8);

        Button revealLauncherPassword;
        revealLauncherPassword.Content(box_value(L"Reveal launcher password"));
        revealLauncherPassword.HorizontalAlignment(HorizontalAlignment::Left);

        TextBox revealedLauncherPassword;
        revealedLauncherPassword.Header(box_value(L"Launcher password"));
        revealedLauncherPassword.IsReadOnly(true);
        revealedLauncherPassword.Visibility(Visibility::Collapsed);

        launcherRevealPanel.Children().Append(revealLauncherPassword);
        launcherRevealPanel.Children().Append(revealedLauncherPassword);

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

        ComboBox emailProvider;
        emailProvider.Header(box_value(L"Email provider"));
        emailProvider.IsEnabled(false);
        emailProvider.Visibility(Visibility::Collapsed);
        for (auto const& provider : account_vault::services::EmailProviders)
        {
            ComboBoxItem item;
            item.Content(box_value(hstring{ provider.name }));
            item.Tag(box_value(hstring{ provider.website }));
            emailProvider.Items().Append(item);
        }

        const int providerIndex =
            account_vault::services::findEmailProviderIndex(
                account->emailProvider,
                account->emailProviderWebsite);

        std::wstring providerDisplayName{ account->emailProvider };
        if (providerIndex >= 0)
        {
            emailProvider.SelectedIndex(providerIndex);
            if (providerDisplayName.empty())
            {
                providerDisplayName =
                    account_vault::services::EmailProviders[providerIndex].name;
            }
        }
        else
        {
            providerDisplayName = providerDisplayName.empty()
                ? account->emailProviderWebsite
                : providerDisplayName;

            ComboBoxItem currentProvider;
            currentProvider.Content(box_value(hstring{ providerDisplayName }));
            currentProvider.Tag(
                box_value(hstring{ account->emailProviderWebsite }));
            emailProvider.Items().Append(currentProvider);
            emailProvider.SelectedIndex(
                static_cast<int>(emailProvider.Items().Size()) - 1);
        }

        StackPanel providerLinkPanel;
        providerLinkPanel.Spacing(4);

        TextBlock providerLinkLabel;
        providerLinkLabel.Text(L"Provider website");

        HyperlinkButton providerLink;
        providerLink.Content(box_value(hstring{ providerDisplayName }));
        providerLink.NavigateUri(
            Uri{ hstring{ account->emailProviderWebsite } });
        providerLink.Padding(Thickness{ 0 });
        providerLink.HorizontalAlignment(HorizontalAlignment::Left);

        providerLinkPanel.Children().Append(providerLinkLabel);
        providerLinkPanel.Children().Append(providerLink);

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

        PasswordBox emailPassword;
        emailPassword.Header(box_value(L"New email password (optional)"));
        emailPassword.PlaceholderText(L"Stored securely; leave blank to keep it");
        emailPassword.IsEnabled(false);
        emailPassword.Visibility(Visibility::Collapsed);

        StackPanel emailRevealPanel;
        emailRevealPanel.Spacing(8);

        Button revealEmailPassword;
        revealEmailPassword.Content(box_value(L"Reveal email password"));
        revealEmailPassword.HorizontalAlignment(HorizontalAlignment::Left);

        TextBox revealedEmailPassword;
        revealedEmailPassword.Header(box_value(L"Email password"));
        revealedEmailPassword.IsReadOnly(true);
        revealedEmailPassword.Visibility(Visibility::Collapsed);

        emailRevealPanel.Children().Append(revealEmailPassword);
        emailRevealPanel.Children().Append(revealedEmailPassword);

        TextBlock validation;
        validation.Visibility(Visibility::Collapsed);
        SolidColorBrush validationBrush;
        validationBrush.Color(color(248, 81, 73));
        validation.Foreground(validationBrush);

        revealLauncherPassword.Click(
            [&, this](IInspectable const&, RoutedEventArgs const&)
                -> fire_and_forget
            {
                if (revealedLauncherPassword.Visibility() ==
                    Visibility::Visible)
                {
                    revealedLauncherPassword.Text(L"");
                    revealedLauncherPassword.Visibility(Visibility::Collapsed);
                    revealLauncherPassword.Content(
                        box_value(L"Reveal launcher password"));
                    co_return;
                }

                if (!(co_await verifyUser(
                        L"Verify your identity to reveal the launcher password")))
                {
                    co_return;
                }

                const Account* current{ m_repository.find(id) };
                const auto password{ !current
                    ? std::nullopt
                    : current->protectedLauncherPassword.empty()
                        ? m_credentials.legacyLauncherPassword(id)
                        : m_credentials.unprotectPassword(
                            current->protectedLauncherPassword) };

                if (!password)
                {
                    validation.Text(
                        L"The launcher password could not be decrypted.");
                    validation.Visibility(Visibility::Visible);
                    co_return;
                }

                revealedLauncherPassword.Text(hstring{ *password });
                revealedLauncherPassword.Visibility(Visibility::Visible);
                revealLauncherPassword.Content(box_value(L"Hide password"));
            });

        revealEmailPassword.Click(
            [&, this](IInspectable const&, RoutedEventArgs const&)
                -> fire_and_forget
            {
                if (revealedEmailPassword.Visibility() == Visibility::Visible)
                {
                    revealedEmailPassword.Text(L"");
                    revealedEmailPassword.Visibility(Visibility::Collapsed);
                    revealEmailPassword.Content(
                        box_value(L"Reveal email password"));
                    co_return;
                }

                if (!(co_await verifyUser(
                        L"Verify your identity to reveal the email password")))
                {
                    co_return;
                }

                const Account* current{ m_repository.find(id) };
                const auto password{ !current
                    ? std::nullopt
                    : current->protectedEmailPassword.empty()
                        ? m_credentials.legacyEmailPassword(id)
                        : m_credentials.unprotectPassword(
                            current->protectedEmailPassword) };

                if (!password)
                {
                    validation.Text(
                        L"The email password could not be decrypted.");
                    validation.Visibility(Visibility::Visible);
                    co_return;
                }

                revealedEmailPassword.Text(hstring{ *password });
                revealedEmailPassword.Visibility(Visibility::Visible);
                revealEmailPassword.Content(box_value(L"Hide password"));
            });

        launcherFields.Children().Append(launcherHeading);
        launcherFields.Children().Append(launcher);
        launcherFields.Children().Append(launcherUsername);
        launcherFields.Children().Append(launcherRevealPanel);
        launcherFields.Children().Append(launcherPassword);
        launcherFields.Children().Append(linkedEmail);

        emailFields.Children().Append(emailHeading);
        emailFields.Children().Append(providerLinkPanel);
        emailFields.Children().Append(emailProvider);
        emailFields.Children().Append(emailAddress);
        emailFields.Children().Append(emailRevealPanel);
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
            [&, this](
                ContentDialog const& sender,
                ContentDialogButtonClickEventArgs const& args)
                -> fire_and_forget
            {
                if (!editing)
                {
                    args.Cancel(true);
                    editing = true;
                    launcher.IsEnabled(true);
                    launcherUsername.IsReadOnly(false);
                    revealedLauncherPassword.Text(L"");
                    revealedLauncherPassword.Visibility(Visibility::Collapsed);
                    launcherRevealPanel.Visibility(Visibility::Collapsed);
                    launcherPassword.Visibility(Visibility::Visible);
                    launcherPassword.IsEnabled(true);
                    providerLinkPanel.Visibility(Visibility::Collapsed);
                    emailProvider.Visibility(Visibility::Visible);
                    emailProvider.IsEnabled(true);
                    emailAddress.IsReadOnly(false);
                    revealedEmailPassword.Text(L"");
                    revealedEmailPassword.Visibility(Visibility::Collapsed);
                    emailRevealPanel.Visibility(Visibility::Collapsed);
                    emailPassword.Visibility(Visibility::Visible);
                    emailPassword.IsEnabled(true);
                    sender.PrimaryButtonText(L"Save changes");
                    launcherUsername.Focus(FocusState::Programmatic);
                    co_return;
                }

                if (launcher.SelectedIndex() < 0 ||
                    launcherUsername.Text().empty() ||
                    emailProvider.SelectedIndex() < 0 ||
                    emailAddress.Text().empty())
                {
                    args.Cancel(true);
                    validation.Text(L"All launcher and email fields are required.");
                    validation.Visibility(Visibility::Visible);
                    co_return;
                }

                const auto launcherItem = launcher.SelectedItem().as<ComboBoxItem>();
                const hstring launcherName =
                    unbox_value<hstring>(launcherItem.Content());

                const auto providerItem =
                    emailProvider.SelectedItem().as<ComboBoxItem>();
                const hstring providerName =
                    unbox_value<hstring>(providerItem.Content());
                const hstring providerWebsite =
                    unbox_value<hstring>(providerItem.Tag());

                std::optional<std::wstring> newLauncherPassword;
                if (!launcherPassword.Password().empty())
                {
                    newLauncherPassword.emplace(
                        launcherPassword.Password().c_str());
                }

                std::optional<std::wstring> newEmailPassword;
                if (!emailPassword.Password().empty())
                {
                    newEmailPassword.emplace(emailPassword.Password().c_str());
                }

                const std::wstring launcherValue{ launcherName.c_str() };
                const std::wstring launcherUsernameValue{
                    launcherUsername.Text().c_str() };
                const std::wstring emailAddressValue{
                    emailAddress.Text().c_str() };
                const std::wstring providerNameValue{ providerName.c_str() };
                const std::wstring providerWebsiteValue{
                    providerWebsite.c_str() };

                const auto clickArgs{ args };
                const auto activeDialog{ sender };
                const auto validationText{ validation };
                const auto dispatcher{ DispatcherQueue() };
                const auto deferral{ clickArgs.GetDeferral() };

                activeDialog.IsPrimaryButtonEnabled(false);
                activeDialog.PrimaryButtonText(L"Saving...");

                bool backgroundResult{ false };
                try
                {
                    co_await resume_background();
                    backgroundResult = updateAccount(
                        id,
                        launcherValue,
                        launcherUsernameValue,
                        std::move(newLauncherPassword),
                        emailAddressValue,
                        providerNameValue,
                        providerWebsiteValue,
                        std::move(newEmailPassword));
                }
                catch (...)
                {
                    backgroundResult = false;
                }

                co_await wil::resume_foreground(dispatcher);
                saved = backgroundResult;
                activeDialog.IsPrimaryButtonEnabled(true);
                activeDialog.PrimaryButtonText(L"Save changes");

                if (!saved)
                {
                    clickArgs.Cancel(true);
                    validationText.Text(L"The account could not be updated.");
                    validationText.Visibility(Visibility::Visible);
                }

                deferral.Complete();
            });

        Grid::SetRowSpan(dialog, 4);
        RootGrid().Children().Append(dialog);

        co_await dialog.ShowAsync(ContentDialogPlacement::InPlace);

        revealedLauncherPassword.Text(L"");
        revealedEmailPassword.Text(L"");

        auto rootChildren{ RootGrid().Children() };
        std::uint32_t dialogIndex{};
        if (rootChildren.IndexOf(dialog, dialogIndex))
        {
            rootChildren.RemoveAt(dialogIndex);
        }

        if (saved)
        {
            refreshAccountCard(id);
            StatusText().Text(L"Account details saved securely");
        }
    }
}
