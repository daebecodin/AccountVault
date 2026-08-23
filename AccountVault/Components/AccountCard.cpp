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

        Grid actions;
        actions.Width(460);
        actions.ColumnSpacing(12);
        actions.VerticalAlignment(VerticalAlignment::Center);

        ColumnDefinition copyGroupColumn;
        copyGroupColumn.Width(
            GridLengthHelper::FromValueAndType(2, GridUnitType::Star));
        actions.ColumnDefinitions().Append(copyGroupColumn);

        ColumnDefinition accountGroupColumn;
        accountGroupColumn.Width(
            GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        actions.ColumnDefinitions().Append(accountGroupColumn);

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
        copyLauncherPassword.Click([this](
            IInspectable const& sender,
            RoutedEventArgs const&) -> fire_and_forget
            {
                Button button{ nullptr };
                try
                {
                    auto lifetime{ get_strong() };
                    button = sender.as<Button>();
                    button.IsEnabled(false);
                    const RecordId id{ recordIdFrom(button) };

                    const bool verified{ co_await verifyUser(
                        L"Verify your identity to copy the launcher password") };
                    button.IsEnabled(true);
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
                        if (button)
                        {
                            button.IsEnabled(true);
                        }
                        StatusText().Text(
                            L"Launcher password copy could not be completed");
                    }
                    catch (...)
                    {
                    }
                }
            });

        Button copyEmailPassword{ makeButton(L"Copy email PW") };
        copyEmailPassword.Click([this](
            IInspectable const& sender,
            RoutedEventArgs const&) -> fire_and_forget
            {
                Button button{ nullptr };
                try
                {
                    auto lifetime{ get_strong() };
                    button = sender.as<Button>();
                    button.IsEnabled(false);
                    const RecordId id{ recordIdFrom(button) };

                    const bool verified{ co_await verifyUser(
                        L"Verify your identity to copy the email password") };
                    button.IsEnabled(true);
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
                        if (button)
                        {
                            button.IsEnabled(true);
                        }
                        StatusText().Text(
                            L"Email password copy could not be completed");
                    }
                    catch (...)
                    {
                    }
                }
            });

        Button remove{ makeButton(L"Remove") };
        remove.Click([this](IInspectable const& sender, RoutedEventArgs const&)
            {
                const auto button{ sender.as<Button>() };
                removeAccount(recordIdFrom(button));
            });

        Grid copyActions;
        copyActions.ColumnSpacing(8);
        copyActions.RowSpacing(8);

        for (int column = 0; column < 2; ++column)
        {
            ColumnDefinition definition;
            definition.Width(
                GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
            copyActions.ColumnDefinitions().Append(definition);
        }

        for (int row = 0; row < 2; ++row)
        {
            RowDefinition definition;
            definition.Height(GridLengthHelper::Auto());
            copyActions.RowDefinitions().Append(definition);
        }

        Grid::SetColumn(copyUsername, 0);
        Grid::SetColumn(copyEmail, 1);
        Grid::SetRow(copyLauncherPassword, 1);
        Grid::SetColumn(copyLauncherPassword, 0);
        Grid::SetRow(copyEmailPassword, 1);
        Grid::SetColumn(copyEmailPassword, 1);

        copyActions.Children().Append(copyUsername);
        copyActions.Children().Append(copyEmail);
        copyActions.Children().Append(copyLauncherPassword);
        copyActions.Children().Append(copyEmailPassword);

        Grid accountActions;
        accountActions.RowSpacing(8);

        for (int row = 0; row < 2; ++row)
        {
            RowDefinition definition;
            definition.Height(GridLengthHelper::Auto());
            accountActions.RowDefinitions().Append(definition);
        }

        Grid::SetRow(remove, 1);
        accountActions.Children().Append(details);
        accountActions.Children().Append(remove);

        const auto makeGroupLabel = [mutedTextBrush](hstring const& label)
            {
                TextBlock text;
                text.Text(label);
                text.FontSize(11);
                text.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
                text.Foreground(mutedTextBrush);
                return text;
            };

        StackPanel copyGroupContent;
        copyGroupContent.Spacing(8);
        copyGroupContent.Children().Append(
            makeGroupLabel(L"CREDENTIALS"));
        copyGroupContent.Children().Append(copyActions);

        Border copyGroup;
        copyGroup.Padding(Thickness{ 10, 9, 10, 10 });
        copyGroup.CornerRadius(CornerRadius{ 8, 8, 8, 8 });
        copyGroup.Background(surfaceAltBrush);
        copyGroup.Child(copyGroupContent);

        StackPanel accountGroupContent;
        accountGroupContent.Spacing(8);
        accountGroupContent.Children().Append(
            makeGroupLabel(L"ACCOUNT"));
        accountGroupContent.Children().Append(accountActions);

        Border accountGroup;
        accountGroup.Padding(Thickness{ 10, 9, 10, 10 });
        accountGroup.CornerRadius(CornerRadius{ 8, 8, 8, 8 });
        accountGroup.Background(surfaceAltBrush);
        accountGroup.Child(accountGroupContent);

        Grid::SetColumn(copyGroup, 0);
        Grid::SetColumn(accountGroup, 1);

        actions.Children().Append(copyGroup);
        actions.Children().Append(accountGroup);

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
