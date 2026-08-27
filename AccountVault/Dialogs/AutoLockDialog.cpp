#include "pch.h"
#include "../MainWindow.xaml.h"

#include <winrt/Microsoft.UI.Xaml.Automation.h>

#include <array>
#include <chrono>
#include <string>
#include <string_view>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Automation;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::Foundation;

namespace
{
    struct AutoLockOption
    {
        std::wstring_view label;
        int seconds;
    };

    constexpr std::array AutoLockOptions{
        AutoLockOption{ L"30 seconds", 30 },
        AutoLockOption{ L"1 minute", 60 },
        AutoLockOption{ L"1 minute 30 seconds", 90 },
        AutoLockOption{ L"5 minutes", 5 * 60 },
        AutoLockOption{ L"15 minutes", 15 * 60 } };
}

namespace winrt::AccountVault::implementation
{
    fire_and_forget MainWindow::showAutoLockDialog()
    {
        account_vault::ui::ModelessToolWindow dialog{ nullptr };
        bool dialogAttached{ false };

        try
        {
            auto lifetime{ get_strong() };

            if (activateModelessWindow(ModelessWindowKind::AutoLockSettings))
            {
                co_return;
            }

            dialog = account_vault::ui::ModelessToolWindow{
                L"Auto-lock",
                520,
                390 };
            dialog.XamlRoot(Content().XamlRoot());
            dialog.Title(box_value(L"Auto-lock"));
            dialog.PrimaryButtonText(L"Save timer");
            dialog.CloseButtonText(L"Cancel");
            dialog.DefaultButton(ContentDialogButton::Primary);
            dialog.MaxWidth(520);

            const auto mutedBrush{ Application::Current().Resources()
                .Lookup(box_value(L"AppMutedTextBrush")).as<Brush>() };

            StackPanel content;
            content.Spacing(18);

            TextBlock introduction;
            introduction.Text(
                L"Choose how long Account Armory waits after activity before locking the vault.");
            introduction.TextWrapping(TextWrapping::Wrap);
            introduction.Foreground(mutedBrush);

            ComboBox timer;
            timer.Header(box_value(L"Lock after"));
            timer.HorizontalAlignment(HorizontalAlignment::Stretch);

            int selectedIndex{};
            for (std::size_t index{}; index < AutoLockOptions.size(); ++index)
            {
                const auto& option{ AutoLockOptions[index] };
                ComboBoxItem item;
                item.Content(box_value(hstring{ option.label }));
                item.Tag(box_value(option.seconds));
                timer.Items().Append(item);
                if (option.seconds == m_autoLockTimeoutSeconds)
                {
                    selectedIndex = static_cast<int>(index);
                }
            }
            timer.SelectedIndex(selectedIndex);

            TextBlock behaviorNote;
            behaviorNote.Text(
                L"The vault still locks immediately when the app is minimized or Windows suspends.");
            behaviorNote.TextWrapping(TextWrapping::Wrap);
            behaviorNote.Foreground(mutedBrush);

            Button lockNow;
            lockNow.Content(box_value(L"Lock vault now"));
            lockNow.HorizontalAlignment(HorizontalAlignment::Stretch);
            AutomationProperties::SetName(
                lockNow,
                L"Lock the vault immediately");

            bool lockNowRequested{ false };
            lockNow.Click(
                [&](IInspectable const&, RoutedEventArgs const&)
                {
                    lockNowRequested = true;
                    dialog.Close(ContentDialogResult::None);
                });

            content.Children().Append(introduction);
            content.Children().Append(timer);
            content.Children().Append(behaviorNote);
            content.Children().Append(lockNow);
            dialog.Content(content);

            attachDialogToShell(dialog, ModelessWindowKind::AutoLockSettings);
            dialogAttached = true;

            const ContentDialogResult result{
                co_await dialog.ShowAsync(ContentDialogPlacement::InPlace) };

            detachModelessWindow(dialog, ModelessWindowKind::AutoLockSettings);
            dialogAttached = false;

            if (lockNowRequested)
            {
                lockApplication(L"locked from the Auto-lock utility");
                co_return;
            }

            if (result != ContentDialogResult::Primary)
            {
                co_return;
            }

            const auto selectedItem{
                timer.SelectedItem().as<ComboBoxItem>() };
            const int timeoutSeconds{ unbox_value<int>(selectedItem.Tag()) };
            setAutoLockTimeout(timeoutSeconds);

            std::wstring status{ L"Auto-lock timer set to " };
            status += unbox_value<hstring>(selectedItem.Content()).c_str();
            StatusText().Text(hstring{ status });
        }
        catch (...)
        {
            try
            {
                if (dialogAttached && dialog)
                {
                    dialog.Hide();
                    detachModelessWindow(dialog, ModelessWindowKind::AutoLockSettings);
                }
            }
            catch (...)
            {
            }

            try
            {
                StatusText().Text(
                    L"The Auto-lock utility encountered an unexpected error");
            }
            catch (...)
            {
            }
        }
    }
}
