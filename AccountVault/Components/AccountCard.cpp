#include "pch.h"
#include "../MainWindow.xaml.h"
#include "../Security/SensitiveData.h"

#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.UI.Text.h>

#include <chrono>
#include <limits>
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
    constexpr double CompactAccountCardWidth{ 760.0 };

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
        const bool credential{ account.kind == AccountKind::Credential };
        const auto resources{ Application::Current().Resources() };
        const auto surfaceBrush{
            resources.Lookup(box_value(L"AppSurfaceBrush")).as<Brush>() };
        const auto surfaceAltBrush{
            resources.Lookup(box_value(L"AppSurfaceAltBrush")).as<Brush>() };
        const auto borderBrush{
            resources.Lookup(box_value(L"AppBorderBrush")).as<Brush>() };
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
        card.ColumnSpacing(22);

        // Grid has no CornerRadius. The Border owns the visual surface while
        // the Grid continues to own the responsive card layout and events.
        Border cardSurface;
        cardSurface.Tag(box_value(account.recordId));
        cardSurface.Margin(Thickness{ 0, 0, 0, 12 });
        cardSurface.Background(surfaceBrush);
        cardSurface.BorderBrush(borderBrush);
        cardSurface.BorderThickness(Thickness{ 1, 1, 1, 1 });
        cardSurface.CornerRadius(CornerRadius{ 8, 8, 8, 8 });
        cardSurface.Child(card);

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

        // Empty Auto rows collapse while the card is wide. They become the
        // vertical slots used by the compact layout below.
        for (int row{}; row < 4; ++row)
        {
            RowDefinition definition;
            definition.Height(GridLengthHelper::Auto());
            card.RowDefinitions().Append(definition);
        }

        Border launcherBadge;
        launcherBadge.Padding(Thickness{ 12, 7, 12, 7 });
        launcherBadge.CornerRadius(CornerRadius{ 6, 6, 6, 6 });
        launcherBadge.HorizontalAlignment(HorizontalAlignment::Left);
        launcherBadge.VerticalAlignment(VerticalAlignment::Center);
        launcherBadge.Background(surfaceAltBrush);

        TextBlock launcherText;
        launcherText.Text(
            credential ? account.category : account.launcher);
        launcherText.FontFamily(FontFamily{ L"Cascadia Mono" });
        launcherText.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        launcherText.Foreground(accentBrush);
        launcherBadge.Child(launcherText);

        StackPanel idPanel;
        idPanel.Spacing(5);
        idPanel.VerticalAlignment(VerticalAlignment::Center);
        TextBlock idLabel;
        idLabel.Text(credential ? L"SERVICE" : L"IN-GAME NAME");
        idLabel.FontSize(11);
        idLabel.Foreground(mutedTextBrush);
        TextBlock idValue;
        idValue.Text(
            credential ? account.serviceName : account.launcherUsername);
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
                const std::wstring& value{
                    account->kind == AccountKind::Credential
                        ? account->username
                        : account->launcherUsername };
                if (value.empty())
                {
                    StatusText().Text(L"No username is stored for this credential");
                    return;
                }
                copyToClipboard(
                    value,
                    account->kind == AccountKind::Credential
                        ? L"Username"
                        : L"Launcher username");
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

        MenuFlyoutItem copyRecoveryEmail{
            makeMenuItem(L"Copy recovery email") };
        copyRecoveryEmail.Click([this, idFromMenuItem](
            IInspectable const& sender,
            RoutedEventArgs const&)
        {
            const Account* account{ m_repository.find(
                idFromMenuItem(sender)) };
            if (!account || account->recoveryEmail.empty())
            {
                StatusText().Text(L"No recovery email is stored for this credential");
                return;
            }
            copyToClipboard(account->recoveryEmail, L"Recovery email");
        });

        MenuFlyoutItem copyLauncherPassword{
            makeMenuItem(
                credential ? L"Copy password" : L"Copy launcher password") };
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

                const Account* pendingAccount{ m_repository.find(id) };
                const bool generic{
                    pendingAccount &&
                    pendingAccount->kind == AccountKind::Credential };
                const bool verified{ co_await verifyUser(
                    generic
                        ? L"Verify your identity to copy the password"
                        : L"Verify your identity to copy the launcher password") };
                item.IsEnabled(true);
                if (!verified || m_isLocked)
                {
                    co_return;
                }

                const Account* account{ m_repository.find(id) };
                auto password{ !account
                    ? std::nullopt
                    : account->kind == AccountKind::Credential
                        ? m_credentials.unprotectPassword(
                            account->protectedPassword)
                        : account->protectedLauncherPassword.empty()
                            ? m_credentials.legacyLauncherPassword(id)
                            : m_credentials.unprotectPassword(
                                account->protectedLauncherPassword) };
                auto wipePassword{
                    account_vault::security::wipeOnExit(password) };
                if (password)
                {
                    copyToClipboard(
                        *password,
                        account && account->kind == AccountKind::Credential
                            ? L"Password"
                            : L"Launcher password");
                    account_vault::security::wipe(password);
                }
                else
                {
                    StatusText().Text(L"Password could not be retrieved");
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
                        L"Password copy could not be completed");
                }
                catch (...)
                {
                }
            }
        });

        MenuFlyoutItem copyEmailPassword{
            makeMenuItem(
                credential
                    ? L"Copy recovery email password"
                    : L"Copy email password") };
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

                const Account* pendingAccount{ m_repository.find(id) };
                const bool generic{
                    pendingAccount &&
                    pendingAccount->kind == AccountKind::Credential };
                const bool verified{ co_await verifyUser(
                    generic
                        ? L"Verify your identity to copy the recovery email password"
                        : L"Verify your identity to copy the email password") };
                item.IsEnabled(true);
                if (!verified || m_isLocked)
                {
                    co_return;
                }

                const Account* account{ m_repository.find(id) };
                auto password{ !account
                    ? std::nullopt
                    : account->kind == AccountKind::Credential
                        ? account->protectedRecoveryEmailPassword.empty()
                            ? std::nullopt
                            : m_credentials.unprotectPassword(
                                account->protectedRecoveryEmailPassword)
                        : account->protectedEmailPassword.empty()
                            ? m_credentials.legacyEmailPassword(id)
                            : m_credentials.unprotectPassword(
                                account->protectedEmailPassword) };
                auto wipePassword{
                    account_vault::security::wipeOnExit(password) };
                if (password)
                {
                    copyToClipboard(
                        *password,
                        account && account->kind == AccountKind::Credential
                            ? L"Recovery email password"
                            : L"Email password");
                    account_vault::security::wipe(password);
                }
                else
                {
                    StatusText().Text(
                        generic
                            ? L"No recovery email password is stored"
                            : L"Email password could not be retrieved");
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
        if (credential)
        {
            credentialFlyout.Items().Append(copyRecoveryEmail);
        }
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
            const RecordId id{ idFromMenuItem(sender) };
            const Account* account{ m_repository.find(id) };
            if (account && account->kind == AccountKind::Credential)
            {
                showCredentialDetailsDialog(id);
            }
            else
            {
                showAccountDetailsDialog(id);
            }
        });

        MenuFlyoutItem exportAccount{
            makeMenuItem(
                credential
                    ? L"Export this credential..."
                    : L"Export this account...") };
        exportAccount.Click([this, idFromMenuItem](
            IInspectable const& sender,
            RoutedEventArgs const&)
        {
            showExportBackup(idFromMenuItem(sender));
        });

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

        DropDownButton accountActions{
            makeMenuButton(credential ? L"RECORD" : L"ACCOUNT") };
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

        // Account cards are created in C++, so they cannot use the page's
        // declarative AdaptiveTriggers. Give each card its own width-based
        // invariant: below the breakpoint every field occupies one row and
        // no fixed-width action block can force horizontal clipping.
        const auto weakCard{ make_weak(card) };
        const auto weakLauncherBadge{ make_weak(launcherBadge) };
        const auto weakIdPanel{ make_weak(idPanel) };
        const auto weakEmailPanel{ make_weak(emailPanel) };
        const auto weakActions{ make_weak(actions) };

        card.SizeChanged(
            [weakCard,
             weakLauncherBadge,
             weakIdPanel,
             weakEmailPanel,
             weakActions](
                IInspectable const&,
                SizeChangedEventArgs const& args)
            {
                const auto currentCard{ weakCard.get() };
                const auto currentLauncherBadge{ weakLauncherBadge.get() };
                const auto currentIdPanel{ weakIdPanel.get() };
                const auto currentEmailPanel{ weakEmailPanel.get() };
                const auto currentActions{ weakActions.get() };

                if (!currentCard ||
                    !currentLauncherBadge ||
                    !currentIdPanel ||
                    !currentEmailPanel ||
                    !currentActions)
                {
                    return;
                }

                const bool compact{
                    args.NewSize().Width < CompactAccountCardWidth };

                if (compact)
                {
                    currentCard.MinHeight(0);
                    currentCard.ColumnSpacing(0);
                    currentCard.RowSpacing(14);

                    currentCard.ColumnDefinitions().GetAt(0).Width(
                        GridLengthHelper::FromValueAndType(
                            1,
                            GridUnitType::Star));
                    currentCard.ColumnDefinitions().GetAt(1).Width(
                        GridLengthHelper::FromPixels(0));
                    currentCard.ColumnDefinitions().GetAt(2).Width(
                        GridLengthHelper::FromPixels(0));
                    currentCard.ColumnDefinitions().GetAt(3).Width(
                        GridLengthHelper::FromPixels(0));

                    Grid::SetRow(currentLauncherBadge, 0);
                    Grid::SetColumn(currentLauncherBadge, 0);
                    Grid::SetRow(currentIdPanel, 1);
                    Grid::SetColumn(currentIdPanel, 0);
                    Grid::SetRow(currentEmailPanel, 2);
                    Grid::SetColumn(currentEmailPanel, 0);
                    Grid::SetRow(currentActions, 3);
                    Grid::SetColumn(currentActions, 0);

                    currentActions.Width(
                        std::numeric_limits<double>::quiet_NaN());
                    currentActions.HorizontalAlignment(
                        HorizontalAlignment::Stretch);
                    return;
                }

                currentCard.MinHeight(94);
                currentCard.ColumnSpacing(22);
                currentCard.RowSpacing(0);

                currentCard.ColumnDefinitions().GetAt(0).Width(
                    GridLengthHelper::FromPixels(112));
                currentCard.ColumnDefinitions().GetAt(1).Width(
                    GridLengthHelper::FromValueAndType(
                        1,
                        GridUnitType::Star));
                currentCard.ColumnDefinitions().GetAt(2).Width(
                    GridLengthHelper::FromValueAndType(
                        1,
                        GridUnitType::Star));
                currentCard.ColumnDefinitions().GetAt(3).Width(
                    GridLengthHelper::Auto());

                Grid::SetRow(currentLauncherBadge, 0);
                Grid::SetColumn(currentLauncherBadge, 0);
                Grid::SetRow(currentIdPanel, 0);
                Grid::SetColumn(currentIdPanel, 1);
                Grid::SetRow(currentEmailPanel, 0);
                Grid::SetColumn(currentEmailPanel, 2);
                Grid::SetRow(currentActions, 0);
                Grid::SetColumn(currentActions, 3);

                currentActions.Width(300);
                currentActions.HorizontalAlignment(
                    HorizontalAlignment::Stretch);
            });

        auto items{ AccountsList().Items() };
        if (index && *index < items.Size())
        {
            items.InsertAt(*index, cardSurface);
        }
        else
        {
            // Append is correct both for a new last item and as a safe fallback
            // if the visible list was temporarily out of sync with the model.
            items.Append(cardSurface);
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

        const auto matches{ visibleAccounts() };

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

        const Account* accountToRemove{ m_repository.find(id) };
        if (!accountToRemove)
        {
            StatusText().Text(L"That account no longer exists");
            return;
        }
        const bool removedCredential{
            accountToRemove->kind == AccountKind::Credential };

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
        if (removedCredential)
        {
            rebuildRecordFilter();
            refreshAccounts();
        }
        else
        {
            refreshAccountCard(id);
        }
        StatusText().Text(
            removedCredential
                ? L"Credential removed"
                : L"Account and credentials removed");
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
