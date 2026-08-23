#include "pch.h"
#include "../MainWindow.xaml.h"

#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.UI.Text.h>

#include <chrono>
#include <string>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::ApplicationModel::DataTransfer;
using namespace Windows::Foundation;

namespace
{
    constexpr auto ClipboardClearDelay{ std::chrono::seconds{ 30 } };

    winrt::fire_and_forget clearClipboardAfterDelay(
        DWORD expectedSequence,
        Microsoft::UI::Dispatching::DispatcherQueue dispatcher)
    {
        try
        {
            co_await winrt::resume_after(ClipboardClearDelay);

            dispatcher.TryEnqueue([expectedSequence]()
                {
                    // A changed sequence means the user or another app copied
                    // something newer. Never erase that newer clipboard value.
                    if (expectedSequence != 0 &&
                        ::GetClipboardSequenceNumber() == expectedSequence)
                    {
                        try
                        {
                            Clipboard::Clear();
                        }
                        catch (...)
                        {
                            // Clipboard access can be temporarily unavailable.
                            // Auto-clear failure must never terminate the app.
                        }
                    }
                });
        }
        catch (...)
        {
            // The timer is a best-effort security feature. A failure must not
            // affect the copy action or the UI thread.
        }
    }
}

namespace winrt::AccountVault::implementation
{
    void MainWindow::appendAccountCard(
        Account const& account,
        std::optional<std::uint32_t> index)
    {
        const auto resources{ Application::Current().Resources() };
        const auto surfaceBrush{
            resources.Lookup(box_value(L"AppSurfaceBrush")).as<Brush>() };
        const auto surfaceAltBrush{
            resources.Lookup(box_value(L"AppSurfaceAltBrush")).as<Brush>() };
        const auto accentBrush{
            resources.Lookup(box_value(L"AppAccentBrush")).as<Brush>() };
        const auto textBrush{
            resources.Lookup(box_value(L"AppTextBrush")).as<Brush>() };
        const auto mutedTextBrush{
            resources.Lookup(box_value(L"AppMutedTextBrush")).as<Brush>() };

        Grid card;
        card.Tag(box_value(account.recordId));
        card.MinHeight(94);
        card.Padding(Thickness{ 18, 14, 18, 14 });
        card.Margin(Thickness{ 0, 0, 0, 12 });
        card.ColumnSpacing(22);
        card.Background(surfaceBrush);

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
        launcherBadge.Background(surfaceAltBrush);

        TextBlock launcherText;
        launcherText.Text(account.launcher);
        launcherText.FontFamily(FontFamily{ L"Cascadia Mono" });
        launcherText.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        launcherText.Foreground(accentBrush);
        launcherBadge.Child(launcherText);

        StackPanel idPanel;
        idPanel.Spacing(5);
        idPanel.VerticalAlignment(VerticalAlignment::Center);
        TextBlock idLabel;
        idLabel.Text(L"IN-GAME NAME");
        idLabel.FontSize(11);
        idLabel.Foreground(mutedTextBrush);
        TextBlock idValue;
        idValue.Text(account.launcherUsername);
        idValue.FontSize(15);
        idValue.Foreground(textBrush);
        idPanel.Children().Append(idLabel);
        idPanel.Children().Append(idValue);

        StackPanel emailPanel;
        emailPanel.Spacing(5);
        emailPanel.VerticalAlignment(VerticalAlignment::Center);
        TextBlock emailLabel;
        emailLabel.Text(L"EMAIL");
        emailLabel.FontSize(11);
        emailLabel.Foreground(mutedTextBrush);
        TextBlock emailValue;
        emailValue.Text(account.emailAddress);
        emailValue.FontSize(15);
        emailValue.Foreground(textBrush);
        emailPanel.Children().Append(emailLabel);
        emailPanel.Children().Append(emailValue);

        // v30: keep cards compact. Each section exposes one DropDownButton;
        // the section actions are created as MenuFlyoutItems below.
        Grid actions;
        actions.Width(300);
        actions.ColumnSpacing(10);
        actions.VerticalAlignment(VerticalAlignment::Center);

        for (int column = 0; column < 2; ++column)
        {
            ColumnDefinition definition;
            definition.Width(
                GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
            actions.ColumnDefinitions().Append(definition);
        }

        const RecordId cardId{ account.recordId };

        const auto makeMenuButton = [](hstring const& label)
            {
                DropDownButton button;
                button.Content(box_value(label));
                button.Height(40);
                button.Padding(Thickness{ 12, 0, 12, 0 });
                button.CornerRadius(CornerRadius{ 6, 6, 6, 6 });
                button.HorizontalAlignment(HorizontalAlignment::Stretch);
                button.HorizontalContentAlignment(HorizontalAlignment::Center);
                return button;
            };

        const auto makeMenuItem = [cardId](hstring const& label)
            {
                MenuFlyoutItem item;
                item.Text(label);
                item.Tag(box_value(cardId));
                return item;
            };

        const auto idFromMenuItem = [](IInspectable const& sender)
            {
                return unbox_value<RecordId>(
                    sender.as<MenuFlyoutItem>().Tag());
            };

        MenuFlyoutItem copyUsername{ makeMenuItem(L"Copy username") };
        copyUsername.Click([this, idFromMenuItem](
            IInspectable const& sender,
            RoutedEventArgs const&)
            {
                const Account* account{ m_repository.find(
                    idFromMenuItem(sender)) };
                if (account)
                {
                    copyToClipboard(
                        account->launcherUsername,
                        L"Launcher username");
                }
            });

        MenuFlyoutItem copyEmail{ makeMenuItem(L"Copy email") };
        copyEmail.Click([this, idFromMenuItem](
            IInspectable const& sender,
            RoutedEventArgs const&)
            {
                const Account* account{ m_repository.find(
                    idFromMenuItem(sender)) };
                if (account)
                {
                    copyToClipboard(account->emailAddress, L"Email address");
                }
            });

        MenuFlyoutItem copyLauncherPassword{
            makeMenuItem(L"Copy launcher password") };
        copyLauncherPassword.Click([this](
            IInspectable const& sender,
            RoutedEventArgs const&) -> fire_and_forget
            {
                MenuFlyoutItem item{ nullptr };
                try
                {
                    auto lifetime{ get_strong() };
                    item = sender.as<MenuFlyoutItem>();
                    item.IsEnabled(false);
                    const RecordId id{ unbox_value<RecordId>(item.Tag()) };

                    const bool verified{ co_await verifyUser(
                        L"Verify your identity to copy the launcher password") };
                    item.IsEnabled(true);
                    if (!verified || m_isLocked)
                    {
                        co_return;
                    }

                    const Account* account{ m_repository.find(id) };
                    const auto password{ !account
                        ? std::nullopt
                        : account->protectedLauncherPassword.empty()
                            ? m_credentials.legacyLauncherPassword(id)
                            : m_credentials.unprotectPassword(
                                account->protectedLauncherPassword) };
                    if (password)
                    {
                        copyToClipboard(*password, L"Launcher password");
                    }
                    else
                    {
                        StatusText().Text(
                            L"Launcher password could not be retrieved");
                    }
                }
                catch (...)
                {
                    try
                    {
                        if (item)
                        {
                            item.IsEnabled(true);
                        }
                        StatusText().Text(
                            L"Launcher password copy could not be completed");
                    }
                    catch (...)
                    {
                    }
                }
            });

        MenuFlyoutItem copyEmailPassword{
            makeMenuItem(L"Copy email password") };
        copyEmailPassword.Click([this](
            IInspectable const& sender,
            RoutedEventArgs const&) -> fire_and_forget
            {
                MenuFlyoutItem item{ nullptr };
                try
                {
                    auto lifetime{ get_strong() };
                    item = sender.as<MenuFlyoutItem>();
                    item.IsEnabled(false);
                    const RecordId id{ unbox_value<RecordId>(item.Tag()) };

                    const bool verified{ co_await verifyUser(
                        L"Verify your identity to copy the email password") };
                    item.IsEnabled(true);
                    if (!verified || m_isLocked)
                    {
                        co_return;
                    }

                    const Account* account{ m_repository.find(id) };
                    const auto password{ !account
                        ? std::nullopt
                        : account->protectedEmailPassword.empty()
                            ? m_credentials.legacyEmailPassword(id)
                            : m_credentials.unprotectPassword(
                                account->protectedEmailPassword) };
                    if (password)
                    {
                        copyToClipboard(*password, L"Email password");
                    }
                    else
                    {
                        StatusText().Text(
                            L"Email password could not be retrieved");
                    }
                }
                catch (...)
                {
                    try
                    {
                        if (item)
                        {
                            item.IsEnabled(true);
                        }
                        StatusText().Text(
                            L"Email password copy could not be completed");
                    }
                    catch (...)
                    {
                    }
                }
            });

        MenuFlyout credentialFlyout;
        credentialFlyout.Items().Append(copyUsername);
        credentialFlyout.Items().Append(copyEmail);
        credentialFlyout.Items().Append(MenuFlyoutSeparator{});
        credentialFlyout.Items().Append(copyLauncherPassword);
        credentialFlyout.Items().Append(copyEmailPassword);

        DropDownButton credentialActions{
            makeMenuButton(L"CREDENTIALS") };
        credentialActions.Flyout(credentialFlyout);

        MenuFlyoutItem details{ makeMenuItem(L"Details") };
        details.Click([this, idFromMenuItem](
            IInspectable const& sender,
            RoutedEventArgs const&)
            {
                showAccountDetailsDialog(idFromMenuItem(sender));
            });

        // UI reservation for the encrypted portable-backup feature. It is
        // intentionally disabled until the export service is implemented.
        MenuFlyoutItem exportAccount{
            makeMenuItem(L"Export this account...") };
        exportAccount.IsEnabled(false);

        MenuFlyoutItem remove{ makeMenuItem(L"Remove") };
        remove.Click([this, idFromMenuItem](
            IInspectable const& sender,
            RoutedEventArgs const&)
            {
                removeAccount(idFromMenuItem(sender));
            });

        MenuFlyout accountFlyout;
        accountFlyout.Items().Append(details);
        accountFlyout.Items().Append(exportAccount);
        accountFlyout.Items().Append(MenuFlyoutSeparator{});
        accountFlyout.Items().Append(remove);

        DropDownButton accountActions{ makeMenuButton(L"ACCOUNT") };
        accountActions.Flyout(accountFlyout);

        Grid::SetColumn(credentialActions, 0);
        Grid::SetColumn(accountActions, 1);
        actions.Children().Append(credentialActions);
        actions.Children().Append(accountActions);

        Grid::SetColumn(launcherBadge, 0);
        Grid::SetColumn(idPanel, 1);
        Grid::SetColumn(emailPanel, 2);
        Grid::SetColumn(actions, 3);

        card.Children().Append(launcherBadge);
        card.Children().Append(idPanel);
        card.Children().Append(emailPanel);
        card.Children().Append(actions);

        auto items{ AccountsList().Items() };
        if (index && *index < items.Size())
        {
            items.InsertAt(*index, card);
        }
        else
        {
            // Append is correct both for a new last item and as a safe fallback
            // if the visible list was temporarily out of sync with the model.
            items.Append(card);
        }
    }
    void MainWindow::refreshAccountCard(RecordId id)
    {
        auto items{ AccountsList().Items() };

        for (std::uint32_t index = 0; index < items.Size(); ++index)
        {
            const auto element{ items.GetAt(index).try_as<FrameworkElement>() };
            if (element && element.Tag() &&
                unbox_value<RecordId>(element.Tag()) == id)
            {
                items.RemoveAt(index);
                break;
            }
        }

        const std::wstring query{ SearchBox().Text().c_str() };
        const std::wstring launcher{ selectedLauncher() };
        const auto matches{ m_repository.search(query, launcher) };

        for (std::uint32_t index = 0;
            index < static_cast<std::uint32_t>(matches.size());
            ++index)
        {
            if (matches[index]->recordId == id)
            {
                appendAccountCard(*matches[index], index);
                break;
            }
        }

        EmptyState().Visibility(
            items.Size() == 0 ? Visibility::Visible : Visibility::Collapsed);
    }
    void MainWindow::copyToClipboard(
        std::wstring const& value,
        std::wstring_view label)
    {
        DataPackage package;
        package.SetText(hstring{ value });

        ClipboardContentOptions options;
        options.IsAllowedInHistory(false);
        options.IsRoamable(false);

        if (!Clipboard::SetContentWithOptions(package, options))
        {
            StatusText().Text(L"The clipboard is busy; try copying again");
            return;
        }

        const DWORD clipboardSequence{ ::GetClipboardSequenceNumber() };
        m_accountClipboardSequence = clipboardSequence;
        clearClipboardAfterDelay(clipboardSequence, DispatcherQueue());

        std::wstring status{ label };
        status += L" copied; clipboard auto-clears in 30 seconds";
        StatusText().Text(hstring{ status });
    }
    void MainWindow::removeAccount(RecordId id)
    {
        bool removalPersisted{ false };

        try
        {
            if (!m_storageReady)
            {
                StatusText().Text(
                    L"Account storage is unavailable; no data was changed");
                return;
            }

            if (!m_repository.find(id))
            {
                StatusText().Text(L"That account no longer exists");
                return;
            }

            const auto oldAccounts{ m_repository.accounts() };
            const RecordId oldNextId{ m_repository.nextId() };
            if (!m_repository.remove(id))
            {
                StatusText().Text(L"The account could not be removed");
                return;
            }

            std::wstring error;
            if (!persistAccounts(error))
            {
                m_repository.replaceAll(oldAccounts, oldNextId);
                StatusText().Text(L"The account removal could not be saved");
                return;
            }

            removalPersisted = true;
            refreshAccountCard(id);
            StatusText().Text(L"Account and credentials removed");
        }
        catch (...)
        {
            try
            {
                if (removalPersisted)
                {
                    refreshAccounts();
                    StatusText().Text(
                        L"Account removed; the account view was refreshed");
                }
                else
                {
                    StatusText().Text(
                        L"The account could not be removed safely");
                }
            }
            catch (...)
            {
            }
        }
    }
    MainWindow::RecordId MainWindow::recordIdFrom(
        Button const& button) const
    {
        return unbox_value<RecordId>(button.Tag());
    }
}
