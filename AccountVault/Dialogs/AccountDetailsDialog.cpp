#include "pch.h"
#include "../MainWindow.xaml.h"
#include "../Security/SensitiveData.h"
#include "../Services/EmailProviderCatalog.h"
#include "../Services/LauncherCatalog.h"

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.UI.Text.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace winrt;
using namespace Microsoft::UI::Dispatching;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::Foundation;

namespace
{
    constexpr int PasswordRevealSeconds{ 30 };
    constexpr auto CountdownTickInterval{ std::chrono::seconds{ 1 } };

    template <typename T>
    void completeDeferral(T const& deferral) noexcept
    {
        try
        {
            if (deferral)
            {
                deferral.Complete();
            }
        }
        catch (...)
        {
        }
    }

    void hideRevealedPassword(
        TextBox const& passwordText,
        Button const& revealButton,
        TextBlock const& countdownText,
        wchar_t const* revealLabel) noexcept
    {
        try
        {
            passwordText.Text(L"");
            passwordText.Visibility(Visibility::Collapsed);
            countdownText.Text(L"");
            countdownText.Visibility(Visibility::Collapsed);
            revealButton.Content(box_value(revealLabel));
        }
        catch (...)
        {
        }
    }

    void showRevealCountdown(
        TextBlock const& countdownText,
        int secondsRemaining) noexcept
    {
        try
        {
            std::wstring text{ L"Hides in " };
            text += std::to_wstring(secondsRemaining);
            text += L"s";
            countdownText.Text(hstring{ text });
            countdownText.Visibility(Visibility::Visible);
        }
        catch (...)
        {
        }
    }
}

namespace winrt::AccountVault::implementation
{
    fire_and_forget MainWindow::showAccountDetailsDialog(RecordId id)
    {
        bool editing{ false };
        bool saved{ false };
        account_vault::ui::ModelessToolWindow dialog{ nullptr };
        bool dialogAttached{ false };
        std::shared_ptr<bool> dialogOpen;
        DispatcherQueueTimer launcherRevealTimer{ nullptr };
        DispatcherQueueTimer emailRevealTimer{ nullptr };

        try
        {
        auto lifetime{ get_strong() };
        const Account* account{ m_repository.find(id) };
        if (!account)
        {
            StatusText().Text(L"That account no longer exists");
            co_return;
        }

        dialog = account_vault::ui::ModelessToolWindow{ L"Account details", 900, 520 };
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
        launcher.HorizontalAlignment(HorizontalAlignment::Stretch);
        launcher.IsEnabled(false);
        for (auto const& definition :
             account_vault::services::LauncherCatalog)
        {
            ComboBoxItem item;
            item.Content(box_value(hstring{ definition.displayName }));
            item.Tag(box_value(
                static_cast<std::uint32_t>(definition.value)));
            launcher.Items().Append(item);
        }
        launcher.SelectedIndex(
            account_vault::services::findLauncherIndex(account->launcher));

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

        TextBlock launcherRevealCountdown;
        launcherRevealCountdown.FontSize(11);
        launcherRevealCountdown.Opacity(0.8);
        launcherRevealCountdown.Visibility(Visibility::Collapsed);

        launcherRevealPanel.Children().Append(revealLauncherPassword);
        launcherRevealPanel.Children().Append(revealedLauncherPassword);
        launcherRevealPanel.Children().Append(launcherRevealCountdown);

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
        emailProvider.HorizontalAlignment(HorizontalAlignment::Stretch);
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
        providerLinkLabel.Text(L"Email provider");

        HyperlinkButton providerLink;
        std::wstring providerLinkText{ providerDisplayName };
        providerLinkText += L" — Open provider website";
        providerLink.Content(box_value(hstring{ providerLinkText }));
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

        TextBlock emailRevealCountdown;
        emailRevealCountdown.FontSize(11);
        emailRevealCountdown.Opacity(0.8);
        emailRevealCountdown.Visibility(Visibility::Collapsed);

        emailRevealPanel.Children().Append(revealEmailPassword);
        emailRevealPanel.Children().Append(revealedEmailPassword);
        emailRevealPanel.Children().Append(emailRevealCountdown);

        dialogOpen = std::make_shared<bool>(true);
        const auto launcherSecondsRemaining{
            std::make_shared<int>(PasswordRevealSeconds) };
        const auto emailSecondsRemaining{
            std::make_shared<int>(PasswordRevealSeconds) };

        launcherRevealTimer = DispatcherQueue().CreateTimer();
        launcherRevealTimer.Interval(CountdownTickInterval);
        launcherRevealTimer.IsRepeating(true);

        const auto weakLauncherPassword{
            make_weak(revealedLauncherPassword) };
        const auto weakLauncherButton{
            make_weak(revealLauncherPassword) };
        const auto weakLauncherCountdown{
            make_weak(launcherRevealCountdown) };
        launcherRevealTimer.Tick(
            [weakLauncherPassword,
             weakLauncherButton,
             weakLauncherCountdown,
             launcherSecondsRemaining](
                DispatcherQueueTimer const& timer,
                IInspectable const&)
            {
                try
                {
                    const auto passwordText{ weakLauncherPassword.get() };
                    const auto revealButton{ weakLauncherButton.get() };
                    const auto countdownText{ weakLauncherCountdown.get() };
                    if (!passwordText || !revealButton || !countdownText)
                    {
                        timer.Stop();
                        return;
                    }

                    --*launcherSecondsRemaining;
                    if (*launcherSecondsRemaining <= 0)
                    {
                        timer.Stop();
                        hideRevealedPassword(
                            passwordText,
                            revealButton,
                            countdownText,
                            L"Reveal launcher password");
                        return;
                    }

                    showRevealCountdown(
                        countdownText,
                        *launcherSecondsRemaining);
                }
                catch (...)
                {
                    try
                    {
                        timer.Stop();
                        const auto passwordText{
                            weakLauncherPassword.get() };
                        const auto revealButton{
                            weakLauncherButton.get() };
                        const auto countdownText{
                            weakLauncherCountdown.get() };
                        if (passwordText && revealButton && countdownText)
                        {
                            hideRevealedPassword(
                                passwordText,
                                revealButton,
                                countdownText,
                                L"Reveal launcher password");
                        }
                    }
                    catch (...)
                    {
                    }
                }
            });

        emailRevealTimer = DispatcherQueue().CreateTimer();
        emailRevealTimer.Interval(CountdownTickInterval);
        emailRevealTimer.IsRepeating(true);

        const auto weakEmailPassword{ make_weak(revealedEmailPassword) };
        const auto weakEmailButton{ make_weak(revealEmailPassword) };
        const auto weakEmailCountdown{ make_weak(emailRevealCountdown) };
        emailRevealTimer.Tick(
            [weakEmailPassword,
             weakEmailButton,
             weakEmailCountdown,
             emailSecondsRemaining](
                DispatcherQueueTimer const& timer,
                IInspectable const&)
            {
                try
                {
                    const auto passwordText{ weakEmailPassword.get() };
                    const auto revealButton{ weakEmailButton.get() };
                    const auto countdownText{ weakEmailCountdown.get() };
                    if (!passwordText || !revealButton || !countdownText)
                    {
                        timer.Stop();
                        return;
                    }

                    --*emailSecondsRemaining;
                    if (*emailSecondsRemaining <= 0)
                    {
                        timer.Stop();
                        hideRevealedPassword(
                            passwordText,
                            revealButton,
                            countdownText,
                            L"Reveal email password");
                        return;
                    }

                    showRevealCountdown(
                        countdownText,
                        *emailSecondsRemaining);
                }
                catch (...)
                {
                    try
                    {
                        timer.Stop();
                        const auto passwordText{ weakEmailPassword.get() };
                        const auto revealButton{ weakEmailButton.get() };
                        const auto countdownText{ weakEmailCountdown.get() };
                        if (passwordText && revealButton && countdownText)
                        {
                            hideRevealedPassword(
                                passwordText,
                                revealButton,
                                countdownText,
                                L"Reveal email password");
                        }
                    }
                    catch (...)
                    {
                    }
                }
            });

        ToolTipService::SetToolTip(
            revealLauncherPassword,
            box_value(L"Automatically hides after 30 seconds"));
        ToolTipService::SetToolTip(
            revealEmailPassword,
            box_value(L"Automatically hides after 30 seconds"));

        TextBlock validation;
        validation.Visibility(Visibility::Collapsed);
        SolidColorBrush validationBrush;
        validationBrush.Color(color(248, 81, 73));
        validation.Foreground(validationBrush);

        revealLauncherPassword.Click(
            [this,
             id,
             dialogOpen,
             launcherRevealTimer,
             launcherSecondsRemaining,
             revealedPassword = revealedLauncherPassword,
             countdownText = launcherRevealCountdown,
             validationText = validation](
                IInspectable const& sender,
                RoutedEventArgs const&)
                -> fire_and_forget
            {
                Button revealButton{ nullptr };

                try
                {
                    auto lifetime{ get_strong() };
                    revealButton = sender.as<Button>();
                    if (revealedPassword.Visibility() == Visibility::Visible)
                    {
                        launcherRevealTimer.Stop();
                        hideRevealedPassword(
                            revealedPassword,
                            revealButton,
                            countdownText,
                            L"Reveal launcher password");
                        co_return;
                    }

                    revealButton.IsEnabled(false);
                    const bool verified{ co_await verifyUser(
                        L"Verify your identity to reveal the launcher password") };
                    revealButton.IsEnabled(true);

                    if (!verified || !*dialogOpen)
                    {
                        co_return;
                    }

                    const Account* current{ m_repository.find(id) };
                    auto password{ !current
                        ? std::nullopt
                        : current->protectedLauncherPassword.empty()
                            ? m_credentials.legacyLauncherPassword(id)
                            : m_credentials.unprotectPassword(
                                current->protectedLauncherPassword) };
                    auto wipePassword{
                        account_vault::security::wipeOnExit(password) };

                    if (!password)
                    {
                        validationText.Text(
                            L"The launcher password could not be decrypted.");
                        validationText.Visibility(Visibility::Visible);
                        co_return;
                    }

                    validationText.Visibility(Visibility::Collapsed);
                    revealedPassword.Text(hstring{ *password });
                    account_vault::security::wipe(password);
                    revealedPassword.Visibility(Visibility::Visible);
                    revealButton.Content(box_value(L"Hide password"));
                    *launcherSecondsRemaining = PasswordRevealSeconds;
                    showRevealCountdown(
                        countdownText,
                        *launcherSecondsRemaining);
                    launcherRevealTimer.Stop();
                    launcherRevealTimer.Start();
                }
                catch (...)
                {
                    try
                    {
                        launcherRevealTimer.Stop();
                    }
                    catch (...)
                    {
                    }
                    hideRevealedPassword(
                        revealedPassword,
                        revealButton,
                        countdownText,
                        L"Reveal launcher password");

                    try
                    {
                        if (revealButton)
                        {
                            revealButton.IsEnabled(true);
                        }
                        validationText.Text(
                            L"The launcher password could not be revealed.");
                        validationText.Visibility(Visibility::Visible);
                    }
                    catch (...)
                    {
                    }
                }
            });

        revealEmailPassword.Click(
            [this,
             id,
             dialogOpen,
             emailRevealTimer,
             emailSecondsRemaining,
             revealedPassword = revealedEmailPassword,
             countdownText = emailRevealCountdown,
             validationText = validation](
                IInspectable const& sender,
                RoutedEventArgs const&)
                -> fire_and_forget
            {
                Button revealButton{ nullptr };

                try
                {
                    auto lifetime{ get_strong() };
                    revealButton = sender.as<Button>();
                    if (revealedPassword.Visibility() == Visibility::Visible)
                    {
                        emailRevealTimer.Stop();
                        hideRevealedPassword(
                            revealedPassword,
                            revealButton,
                            countdownText,
                            L"Reveal email password");
                        co_return;
                    }

                    revealButton.IsEnabled(false);
                    const bool verified{ co_await verifyUser(
                        L"Verify your identity to reveal the email password") };
                    revealButton.IsEnabled(true);

                    if (!verified || !*dialogOpen)
                    {
                        co_return;
                    }

                    const Account* current{ m_repository.find(id) };
                    auto password{ !current
                        ? std::nullopt
                        : current->protectedEmailPassword.empty()
                            ? m_credentials.legacyEmailPassword(id)
                            : m_credentials.unprotectPassword(
                                current->protectedEmailPassword) };
                    auto wipePassword{
                        account_vault::security::wipeOnExit(password) };

                    if (!password)
                    {
                        validationText.Text(
                            L"The email password could not be decrypted.");
                        validationText.Visibility(Visibility::Visible);
                        co_return;
                    }

                    validationText.Visibility(Visibility::Collapsed);
                    revealedPassword.Text(hstring{ *password });
                    account_vault::security::wipe(password);
                    revealedPassword.Visibility(Visibility::Visible);
                    revealButton.Content(box_value(L"Hide password"));
                    *emailSecondsRemaining = PasswordRevealSeconds;
                    showRevealCountdown(
                        countdownText,
                        *emailSecondsRemaining);
                    emailRevealTimer.Stop();
                    emailRevealTimer.Start();
                }
                catch (...)
                {
                    try
                    {
                        emailRevealTimer.Stop();
                    }
                    catch (...)
                    {
                    }
                    hideRevealedPassword(
                        revealedPassword,
                        revealButton,
                        countdownText,
                        L"Reveal email password");

                    try
                    {
                        if (revealButton)
                        {
                            revealButton.IsEnabled(true);
                        }
                        validationText.Text(
                            L"The email password could not be revealed.");
                        validationText.Visibility(Visibility::Visible);
                    }
                    catch (...)
                    {
                    }
                }
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

        dialog.PrimaryButtonClick(
            [&, this](
                account_vault::ui::ModelessToolWindow const& sender,
                account_vault::ui::ModelessButtonClickEventArgs const& args)
                -> fire_and_forget
            {
                account_vault::ui::ModelessDeferral deferral{ nullptr };

                try
                {
                    auto lifetime{ get_strong() };
                    const auto clickArgs{ args };
                    const auto activeDialog{ sender };
                    const auto validationText{ validation };
                    const auto dispatcher{ DispatcherQueue() };
                    deferral = clickArgs.GetDeferral();
                    bool backgroundResult{ false };

                    try
                    {
                    if (!editing)
                    {
                        clickArgs.Cancel(true);
                        editing = true;
                        launcherRevealTimer.Stop();
                        emailRevealTimer.Stop();
                        launcher.IsEnabled(true);
                        launcherUsername.IsReadOnly(false);
                        hideRevealedPassword(
                            revealedLauncherPassword,
                            revealLauncherPassword,
                            launcherRevealCountdown,
                            L"Reveal launcher password");
                        launcherRevealPanel.Visibility(Visibility::Collapsed);
                        launcherPassword.Visibility(Visibility::Visible);
                        launcherPassword.IsEnabled(true);
                        providerLinkPanel.Visibility(Visibility::Collapsed);
                        emailProvider.Visibility(Visibility::Visible);
                        emailProvider.IsEnabled(true);
                        emailAddress.IsReadOnly(false);
                        hideRevealedPassword(
                            revealedEmailPassword,
                            revealEmailPassword,
                            emailRevealCountdown,
                            L"Reveal email password");
                        emailRevealPanel.Visibility(Visibility::Collapsed);
                        emailPassword.Visibility(Visibility::Visible);
                        emailPassword.IsEnabled(true);
                        activeDialog.PrimaryButtonText(L"Save changes");
                        launcherUsername.Focus(FocusState::Programmatic);
                        completeDeferral(deferral);
                        co_return;
                    }

                    if (launcher.SelectedIndex() < 0 ||
                        launcherUsername.Text().empty() ||
                        emailProvider.SelectedIndex() < 0 ||
                        emailAddress.Text().empty())
                    {
                        clickArgs.Cancel(true);
                        validationText.Text(
                            L"All launcher and email fields are required.");
                        validationText.Visibility(Visibility::Visible);
                        completeDeferral(deferral);
                        co_return;
                    }

                const auto launcherItem = launcher.SelectedItem().as<ComboBoxItem>();
                const Launcher launcherValue{ static_cast<Launcher>(
                    unbox_value<std::uint32_t>(launcherItem.Tag())) };

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
                auto wipeLauncherPassword{
                    account_vault::security::wipeOnExit(
                        newLauncherPassword) };

                std::optional<std::wstring> newEmailPassword;
                if (!emailPassword.Password().empty())
                {
                    newEmailPassword.emplace(emailPassword.Password().c_str());
                }
                auto wipeEmailPassword{
                    account_vault::security::wipeOnExit(newEmailPassword) };

                launcherPassword.Password(L"");
                emailPassword.Password(L"");

                const std::wstring launcherUsernameValue{
                    launcherUsername.Text().c_str() };
                const std::wstring emailAddressValue{
                    emailAddress.Text().c_str() };
                const std::wstring providerNameValue{ providerName.c_str() };
                const std::wstring providerWebsiteValue{
                    providerWebsite.c_str() };

                activeDialog.IsPrimaryButtonEnabled(false);
                activeDialog.PrimaryButtonText(L"Saving...");

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
                    account_vault::security::wipe(newLauncherPassword);
                    account_vault::security::wipe(newEmailPassword);
                    }
                    catch (...)
                    {
                        backgroundResult = false;
                    }

                    bool foregroundReady{ true };
                    try
                    {
                        co_await wil::resume_foreground(dispatcher);
                    }
                    catch (...)
                    {
                        foregroundReady = false;
                    }

                    if (!foregroundReady)
                    {
                        completeDeferral(deferral);
                        co_return;
                    }

                    try
                    {
                        activeDialog.IsPrimaryButtonEnabled(true);
                        activeDialog.PrimaryButtonText(L"Save changes");

                        if (!backgroundResult)
                        {
                            clickArgs.Cancel(true);
                            validationText.Text(L"The account could not be updated.");
                            validationText.Visibility(Visibility::Visible);
                        }
                        else
                        {
                            saved = true;
                        }
                    }
                    catch (...)
                    {
                        try
                        {
                            clickArgs.Cancel(true);
                            validationText.Text(L"The account could not be updated.");
                            validationText.Visibility(Visibility::Visible);
                        }
                        catch (...)
                        {
                        }
                    }

                    completeDeferral(deferral);
                }
                catch (...)
                {
                    try
                    {
                        args.Cancel(true);
                        validation.Text(L"The account could not be updated.");
                        validation.Visibility(Visibility::Visible);
                    }
                    catch (...)
                    {
                    }
                    completeDeferral(deferral);
                }
            });

        attachDialogToShell(dialog);
        dialogAttached = true;

        co_await dialog.ShowAsync(ContentDialogPlacement::InPlace);

        *dialogOpen = false;
        launcherRevealTimer.Stop();
        emailRevealTimer.Stop();
        hideRevealedPassword(
            revealedLauncherPassword,
            revealLauncherPassword,
            launcherRevealCountdown,
            L"Reveal launcher password");
        hideRevealedPassword(
            revealedEmailPassword,
            revealEmailPassword,
            emailRevealCountdown,
            L"Reveal email password");
        launcherPassword.Password(L"");
        emailPassword.Password(L"");

        detachModelessWindow(dialog);
        dialogAttached = false;

        if (saved)
        {
            refreshAccountCard(id);
            StatusText().Text(L"Account details saved securely");
        }
        }
        catch (...)
        {
            try
            {
                if (dialogOpen)
                {
                    *dialogOpen = false;
                }
            }
            catch (...)
            {
            }

            try
            {
                if (launcherRevealTimer)
                {
                    launcherRevealTimer.Stop();
                }
            }
            catch (...)
            {
            }

            try
            {
                if (emailRevealTimer)
                {
                    emailRevealTimer.Stop();
                }
            }
            catch (...)
            {
            }

            try
            {
                if (dialogAttached && dialog)
                {
                    dialog.Hide();
                    detachModelessWindow(dialog);
                }
            }
            catch (...)
            {
            }

            try
            {
                if (saved)
                {
                    refreshAccounts();
                    StatusText().Text(
                        L"Account updated; the account view was refreshed");
                }
                else
                {
                    StatusText().Text(
                        L"The Account Details dialog encountered a UI error");
                }
            }
            catch (...)
            {
            }
        }
    }
}
