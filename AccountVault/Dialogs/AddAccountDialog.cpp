#include "pch.h"
#include "../MainWindow.xaml.h"
#include "../Security/SensitiveData.h"
#include "../Services/EmailProviderCatalog.h"

#include <winrt/Windows.UI.Text.h>

#include <optional>
#include <string>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::Foundation;

namespace
{
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
}

namespace winrt::AccountVault::implementation
{
    fire_and_forget MainWindow::showAddAccountDialog()
    {
        std::optional<RecordId> addedId;
        ContentDialog dialog{ nullptr };
        bool dialogAttached{ false };

        try
        {
            auto lifetime{ get_strong() };
            dialog = ContentDialog{};
            dialog.XamlRoot(Content().XamlRoot());
            dialog.Title(box_value(L"Add account"));
            dialog.PrimaryButtonText(L"Add");
            dialog.CloseButtonText(L"Cancel");
            dialog.DefaultButton(ContentDialogButton::Primary);
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

            TextBlock linkedEmail;
            linkedEmail.Text(L"Linked email: ");
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
            emailProvider.PlaceholderText(L"Choose an email provider");
            for (auto const& provider : account_vault::services::EmailProviders)
            {
                ComboBoxItem item;
                item.Content(box_value(hstring{ provider.name }));
                item.Tag(box_value(hstring{ provider.website }));
                emailProvider.Items().Append(item);
            }

            TextBox emailAddress;
            emailAddress.Header(box_value(L"Email address (shared with launcher)"));
            emailAddress.PlaceholderText(L"name@example.com");
            emailAddress.TextChanged(
                [&linkedEmail](IInspectable const& sender, TextChangedEventArgs const&)
                {
                    const auto textBox{ sender.as<TextBox>() };
                    std::wstring mirrorText{ L"Linked email: " };
                    mirrorText += textBox.Text().c_str();
                    linkedEmail.Text(hstring{ mirrorText });
                });

            PasswordBox emailPassword;
            emailPassword.Header(box_value(L"Email password"));
            emailPassword.PlaceholderText(L"Email password");

            TextBlock validation;
            validation.Text(L"All launcher and email fields are required.");
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
            emailFields.Children().Append(emailProvider);
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

            ScrollViewer addAccountScroller;
            addAccountScroller.MaxHeight(560);
            addAccountScroller.HorizontalScrollBarVisibility(
                ScrollBarVisibility::Disabled);
            addAccountScroller.VerticalScrollBarVisibility(
                ScrollBarVisibility::Auto);
            addAccountScroller.HorizontalContentAlignment(
                HorizontalAlignment::Stretch);
            addAccountScroller.Content(fields);
            dialog.Content(addAccountScroller);

            dialog.PrimaryButtonClick(
                [&, this](
                    ContentDialog const& sender,
                    ContentDialogButtonClickEventArgs const& args)
                -> fire_and_forget
                {
                    ContentDialogButtonClickDeferral deferral{ nullptr };

                    try
                    {
                        auto lifetime{ get_strong() };
                        const auto clickArgs{ args };
                        const auto activeDialog{ sender };
                        const auto validationText{ validation };
                        const auto dispatcher{ DispatcherQueue() };
                        deferral = clickArgs.GetDeferral();
                        std::optional<RecordId> backgroundResult;

                        try
                        {
                            if (launcher.SelectedIndex() < 0 ||
                                launcherUsername.Text().empty() ||
                                launcherPassword.Password().empty() ||
                                emailProvider.SelectedIndex() < 0 ||
                                emailAddress.Text().empty() ||
                                emailPassword.Password().empty())
                            {
                                clickArgs.Cancel(true);
                                validationText.Visibility(Visibility::Visible);
                                completeDeferral(deferral);
                                co_return;
                            }

                            const auto launcherItem =
                                launcher.SelectedItem().as<ComboBoxItem>();
                            const hstring launcherName =
                                unbox_value<hstring>(launcherItem.Content());

                            const auto providerItem =
                                emailProvider.SelectedItem().as<ComboBoxItem>();
                            const hstring providerName =
                                unbox_value<hstring>(providerItem.Content());
                            const hstring providerWebsite =
                                unbox_value<hstring>(providerItem.Tag());

                            const std::wstring launcherValue{ launcherName.c_str() };
                            const std::wstring launcherUsernameValue{
                                launcherUsername.Text().c_str() };
                            std::wstring launcherPasswordValue{
                                launcherPassword.Password().c_str() };
                            auto wipeLauncherPassword{
                                account_vault::security::wipeOnExit(
                                    launcherPasswordValue) };
                            const std::wstring emailAddressValue{
                                emailAddress.Text().c_str() };
                            const std::wstring providerNameValue{ providerName.c_str() };
                            const std::wstring providerWebsiteValue{
                                providerWebsite.c_str() };
                            std::wstring emailPasswordValue{
                                emailPassword.Password().c_str() };
                            auto wipeEmailPassword{
                                account_vault::security::wipeOnExit(
                                    emailPasswordValue) };

                            // PasswordBox stores its own immutable WinRT string. Clear
                            // it before leaving the UI thread; the guarded C++ copies
                            // are now the only plaintext values owned by this handler.
                            launcherPassword.Password(L"");
                            emailPassword.Password(L"");

                            activeDialog.IsPrimaryButtonEnabled(false);
                            activeDialog.PrimaryButtonText(L"Saving...");

                            co_await resume_background();
                            backgroundResult = addAccount(
                                launcherValue,
                                launcherUsernameValue,
                                launcherPasswordValue,
                                emailAddressValue,
                                providerNameValue,
                                providerWebsiteValue,
                                emailPasswordValue);
                            account_vault::security::wipe(launcherPasswordValue);
                            account_vault::security::wipe(emailPasswordValue);
                        }
                        catch (...)
                        {
                            backgroundResult = std::nullopt;
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
                            activeDialog.PrimaryButtonText(L"Add");

                            if (!backgroundResult)
                            {
                                clickArgs.Cancel(true);
                                validationText.Text(
                                    L"The account could not be saved securely. Please try again.");
                                validationText.Visibility(Visibility::Visible);
                            }
                            else
                            {
                                addedId = backgroundResult;
                            }
                        }
                        catch (...)
                        {
                            try
                            {
                                clickArgs.Cancel(true);
                                validationText.Text(
                                    L"The account could not be saved securely. Please try again.");
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
                            validation.Text(
                                L"The account could not be saved securely. Please try again.");
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

            launcherPassword.Password(L"");
            emailPassword.Password(L"");

            auto rootChildren{ RootGrid().Children() };
            std::uint32_t dialogIndex{};
            if (rootChildren.IndexOf(dialog, dialogIndex))
            {
                rootChildren.RemoveAt(dialogIndex);
            }
            dialogAttached = false;

            if (addedId)
            {
                refreshAccountCard(*addedId);
                StatusText().Text(L"Account saved securely");
            }
        }
        catch (...)
        {
            try
            {
                if (dialogAttached && dialog)
                {
                    auto rootChildren{ RootGrid().Children() };
                    std::uint32_t dialogIndex{};
                    if (rootChildren.IndexOf(dialog, dialogIndex))
                    {
                        rootChildren.RemoveAt(dialogIndex);
                    }
                }
            }
            catch (...)
            {
            }

            try
            {
                if (addedId)
                {
                    refreshAccounts();
                    StatusText().Text(
                        L"Account saved; the account view was refreshed");
                }
                else
                {
                    StatusText().Text(
                        L"The Add Account dialog encountered a UI error");
                }
            }
            catch (...)
            {
            }
        }
    }
}
