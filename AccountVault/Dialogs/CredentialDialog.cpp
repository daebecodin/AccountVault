#include "pch.h"
#include "../MainWindow.xaml.h"
#include "../Security/SensitiveData.h"
#include "../Services/CredentialCategoryCatalog.h"

#include <winrt/Windows.UI.Text.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

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

    [[nodiscard]] std::wstring categoryValue(ComboBox const& category)
    {
        std::wstring value{ category.Text().c_str() };
        if (!value.empty() || category.SelectedIndex() < 0)
        {
            return value;
        }

        const auto item{ category.SelectedItem().try_as<ComboBoxItem>() };
        return item
            ? std::wstring{ unbox_value<hstring>(item.Content()).c_str() }
            : std::wstring{};
    }

    void addCategory(ComboBox const& category, std::wstring_view value)
    {
        ComboBoxItem item;
        item.Content(box_value(hstring{ value }));
        category.Items().Append(item);
    }

}

namespace winrt::AccountVault::implementation
{
    fire_and_forget MainWindow::showAddCredentialDialog()
    {
        std::optional<RecordId> addedId;
        account_vault::ui::ModelessToolWindow dialog{ nullptr };
        bool dialogAttached{ false };

        try
        {
            auto lifetime{ get_strong() };
            if (activateModelessWindow(ModelessWindowKind::AddCredential))
            {
                co_return;
            }
            dialog = account_vault::ui::ModelessToolWindow{ L"Credential", 680, 760 };
            dialog.XamlRoot(Content().XamlRoot());
            dialog.Title(box_value(L"Add credential"));
            dialog.PrimaryButtonText(L"Add");
            dialog.CloseButtonText(L"Cancel");
            dialog.DefaultButton(ContentDialogButton::Primary);
            dialog.MaxWidth(960);
            dialog.HorizontalAlignment(HorizontalAlignment::Center);
            dialog.VerticalAlignment(VerticalAlignment::Center);

            const auto accentBrush{ Application::Current().Resources()
                .Lookup(box_value(L"AppAccentBrush")).as<Brush>() };
            const auto borderBrush{ Application::Current().Resources()
                .Lookup(box_value(L"AppBorderBrush")).as<Brush>() };

            StackPanel fields;
            fields.Spacing(16);

            Grid sections;
            sections.ColumnSpacing(20);
            for (int column{}; column < 3; ++column)
            {
                ColumnDefinition definition;
                definition.Width(
                    column == 1
                        ? GridLengthHelper::Auto()
                        : GridLengthHelper::FromValueAndType(
                            1,
                            GridUnitType::Star));
                sections.ColumnDefinitions().Append(definition);
            }

            StackPanel identityFields;
            identityFields.Spacing(12);
            TextBlock identityHeading;
            identityHeading.Text(L"SERVICE IDENTITY");
            identityHeading.FontFamily(FontFamily{ L"Cascadia Mono" });
            identityHeading.FontWeight(
                Windows::UI::Text::FontWeights::SemiBold());
            identityHeading.Foreground(accentBrush);

            ComboBox category;
            category.Header(box_value(L"Category"));
            category.PlaceholderText(L"Choose or type a category");
            category.IsEditable(true);
            category.IsTextSearchEnabled(true);
            for (auto const value :
                 account_vault::services::DefaultCredentialCategories)
            {
                addCategory(category, value);
            }
            for (auto const& value : m_repository.credentialCategories())
            {
                if (!account_vault::services::isDefaultCredentialCategory(value))
                {
                    addCategory(category, value);
                }
            }

            TextBlock categoryHint;
            categoryHint.Text(
                L"Choose a default or type a custom category.");
            categoryHint.FontSize(11);
            categoryHint.TextWrapping(TextWrapping::Wrap);
            categoryHint.Foreground(Application::Current().Resources()
                .Lookup(box_value(L"AppMutedTextBrush")).as<Brush>());

            TextBox serviceName;
            serviceName.Header(box_value(L"Service name"));
            serviceName.PlaceholderText(L"Bank, school portal, store, or website");

            TextBox username;
            username.Header(box_value(L"Username (optional)"));
            username.PlaceholderText(L"Username or account ID");

            TextBox email;
            email.Header(box_value(L"Email address (optional)"));
            email.PlaceholderText(L"name@example.com");

            TextBox website;
            website.Header(box_value(L"Website (optional)"));
            website.PlaceholderText(L"https://example.com");

            identityFields.Children().Append(identityHeading);
            identityFields.Children().Append(category);
            identityFields.Children().Append(categoryHint);
            identityFields.Children().Append(serviceName);
            identityFields.Children().Append(username);
            identityFields.Children().Append(email);
            identityFields.Children().Append(website);

            Border separator;
            separator.Width(1);
            separator.Margin(Thickness{ 0, 4, 0, 4 });
            separator.VerticalAlignment(VerticalAlignment::Stretch);
            separator.Background(borderBrush);

            StackPanel securityFields;
            securityFields.Spacing(12);
            TextBlock securityHeading;
            securityHeading.Text(L"SECURITY & RECOVERY");
            securityHeading.FontFamily(FontFamily{ L"Cascadia Mono" });
            securityHeading.FontWeight(
                Windows::UI::Text::FontWeights::SemiBold());
            securityHeading.Foreground(accentBrush);

            PasswordBox password;
            password.Header(box_value(L"Password"));
            password.PlaceholderText(L"Service password");

            TextBox recoveryEmail;
            recoveryEmail.Header(box_value(L"Recovery email (optional)"));
            recoveryEmail.PlaceholderText(L"recovery@example.com");

            PasswordBox recoveryPassword;
            recoveryPassword.Header(box_value(
                L"Recovery email password (optional)"));
            recoveryPassword.PlaceholderText(L"Recovery email password");

            TextBox notes;
            notes.Header(box_value(L"Notes (optional)"));
            notes.PlaceholderText(L"Account number hint, sign-in notes, or recovery details");
            notes.AcceptsReturn(true);
            notes.TextWrapping(TextWrapping::Wrap);
            notes.MinHeight(96);

            securityFields.Children().Append(securityHeading);
            securityFields.Children().Append(password);
            securityFields.Children().Append(recoveryEmail);
            securityFields.Children().Append(recoveryPassword);
            securityFields.Children().Append(notes);

            Grid::SetColumn(identityFields, 0);
            Grid::SetColumn(separator, 1);
            Grid::SetColumn(securityFields, 2);
            sections.Children().Append(identityFields);
            sections.Children().Append(separator);
            sections.Children().Append(securityFields);

            TextBlock validation;
            validation.Text(
                L"Service name, category, password, and a username or email are required.");
            validation.Visibility(Visibility::Collapsed);
            SolidColorBrush validationBrush;
            validationBrush.Color(color(248, 81, 73));
            validation.Foreground(validationBrush);

            fields.Children().Append(sections);
            fields.Children().Append(validation);

            ScrollViewer scroller;
            scroller.MaxHeight(620);
            scroller.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
            scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
            scroller.HorizontalContentAlignment(HorizontalAlignment::Stretch);
            scroller.Content(fields);
            dialog.Content(scroller);

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

                        const std::wstring categoryText{
                            categoryValue(category) };
                        if (serviceName.Text().empty() ||
                            categoryText.empty() ||
                            (username.Text().empty() && email.Text().empty()) ||
                            password.Password().empty())
                        {
                            clickArgs.Cancel(true);
                            validationText.Text(
                                L"Service name, category, password, and a username or email are required.");
                            validationText.Visibility(Visibility::Visible);
                            completeDeferral(deferral);
                            co_return;
                        }
                        if (!recoveryPassword.Password().empty() &&
                            recoveryEmail.Text().empty())
                        {
                            clickArgs.Cancel(true);
                            validationText.Text(
                                L"Enter a recovery email before its password.");
                            validationText.Visibility(Visibility::Visible);
                            completeDeferral(deferral);
                            co_return;
                        }

                        const std::wstring serviceValue{
                            serviceName.Text().c_str() };
                        const std::wstring usernameValue{ username.Text().c_str() };
                        const std::wstring emailValue{ email.Text().c_str() };
                        const std::wstring websiteValue{ website.Text().c_str() };
                        const std::wstring recoveryEmailValue{
                            recoveryEmail.Text().c_str() };
                        const std::wstring notesValue{ notes.Text().c_str() };
                        std::wstring passwordValue{ password.Password().c_str() };
                        std::wstring recoveryPasswordValue{
                            recoveryPassword.Password().c_str() };
                        auto wipePassword{
                            account_vault::security::wipeOnExit(passwordValue) };
                        auto wipeRecoveryPassword{
                            account_vault::security::wipeOnExit(
                                recoveryPasswordValue) };

                        password.Password(L"");
                        recoveryPassword.Password(L"");
                        activeDialog.IsPrimaryButtonEnabled(false);
                        activeDialog.PrimaryButtonText(L"Saving...");

                        std::optional<RecordId> result;
                        try
                        {
                            co_await resume_background();
                            result = addCredential(
                                serviceValue,
                                categoryText,
                                usernameValue,
                                emailValue,
                                passwordValue,
                                websiteValue,
                                recoveryEmailValue,
                                recoveryPasswordValue,
                                notesValue);
                            account_vault::security::wipe(passwordValue);
                            account_vault::security::wipe(
                                recoveryPasswordValue);
                        }
                        catch (...)
                        {
                            result = std::nullopt;
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

                        activeDialog.IsPrimaryButtonEnabled(true);
                        activeDialog.PrimaryButtonText(L"Add");
                        if (!result)
                        {
                            clickArgs.Cancel(true);
                            validationText.Text(
                                L"The credential could not be saved securely. Please try again.");
                            validationText.Visibility(Visibility::Visible);
                        }
                        else
                        {
                            addedId = result;
                        }
                        completeDeferral(deferral);
                    }
                    catch (...)
                    {
                        try
                        {
                            args.Cancel(true);
                            validation.Text(
                                L"The credential could not be saved securely. Please try again.");
                            validation.Visibility(Visibility::Visible);
                        }
                        catch (...)
                        {
                        }
                        completeDeferral(deferral);
                    }
                });

            attachDialogToShell(dialog, ModelessWindowKind::AddCredential);
            dialogAttached = true;
            co_await dialog.ShowAsync(ContentDialogPlacement::InPlace);
            password.Password(L"");
            recoveryPassword.Password(L"");
            dialog.Hide();
            detachModelessWindow(dialog, ModelessWindowKind::AddCredential);
            dialogAttached = false;

            if (addedId)
            {
                rebuildRecordFilter();
                refreshAccounts();
                StatusText().Text(L"Credential saved securely");
            }
        }
        catch (...)
        {
            if (dialogAttached && dialog)
            {
                dialog.Hide();
                detachModelessWindow(dialog, ModelessWindowKind::AddCredential);
            }
            try
            {
                if (addedId)
                {
                    rebuildRecordFilter();
                    refreshAccounts();
                    StatusText().Text(
                        L"Credential saved; the vault view was refreshed");
                }
                else
                {
                    StatusText().Text(
                        L"The Add Credential dialog encountered a UI error");
                }
            }
            catch (...)
            {
            }
        }
    }

    fire_and_forget MainWindow::showCredentialDetailsDialog(RecordId id)
    {
        bool editing{ false };
        bool saved{ false };
        account_vault::ui::ModelessToolWindow dialog{ nullptr };
        bool dialogAttached{ false };

        try
        {
            auto lifetime{ get_strong() };
            if (activateModelessWindow(ModelessWindowKind::CredentialDetails))
            {
                co_return;
            }
            const Account* account{ m_repository.find(id) };
            if (!account || account->kind != AccountKind::Credential)
            {
                StatusText().Text(L"That credential no longer exists");
                co_return;
            }

            const bool hadRecoveryPassword{
                !account->protectedRecoveryEmailPassword.empty() };
            dialog = account_vault::ui::ModelessToolWindow{ L"Credential", 680, 760 };
            dialog.XamlRoot(Content().XamlRoot());
            dialog.Title(box_value(L"Credential details"));
            dialog.PrimaryButtonText(L"Edit");
            dialog.CloseButtonText(L"Close");
            dialog.MaxWidth(960);
            dialog.HorizontalAlignment(HorizontalAlignment::Center);
            dialog.VerticalAlignment(VerticalAlignment::Center);

            const auto accentBrush{ Application::Current().Resources()
                .Lookup(box_value(L"AppAccentBrush")).as<Brush>() };

            StackPanel fields;
            fields.Spacing(12);
            TextBlock heading;
            heading.Text(L"GENERAL CREDENTIAL");
            heading.FontFamily(FontFamily{ L"Cascadia Mono" });
            heading.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
            heading.Foreground(accentBrush);

            ComboBox category;
            category.Header(box_value(L"Category"));
            category.IsEditable(true);
            category.Text(account->category);
            category.IsEnabled(false);
            for (auto const value :
                 account_vault::services::DefaultCredentialCategories)
            {
                addCategory(category, value);
            }
            for (auto const& value : m_repository.credentialCategories())
            {
                if (!account_vault::services::isDefaultCredentialCategory(value))
                {
                    addCategory(category, value);
                }
            }

            TextBox serviceName;
            serviceName.Header(box_value(L"Service name"));
            serviceName.Text(account->serviceName);
            serviceName.IsReadOnly(true);

            TextBox username;
            username.Header(box_value(L"Username (optional)"));
            username.Text(account->username);
            username.IsReadOnly(true);

            TextBox email;
            email.Header(box_value(L"Email address (optional)"));
            email.Text(account->emailAddress);
            email.IsReadOnly(true);

            TextBox website;
            website.Header(box_value(L"Website (optional)"));
            website.Text(account->website);
            website.IsReadOnly(true);

            TextBox recoveryEmail;
            recoveryEmail.Header(box_value(L"Recovery email (optional)"));
            recoveryEmail.Text(account->recoveryEmail);
            recoveryEmail.IsReadOnly(true);

            TextBox notes;
            notes.Header(box_value(L"Notes (optional)"));
            notes.Text(account->notes);
            notes.AcceptsReturn(true);
            notes.TextWrapping(TextWrapping::Wrap);
            notes.MinHeight(96);
            notes.IsReadOnly(true);

            PasswordBox password;
            password.Header(box_value(L"New password (optional)"));
            password.PlaceholderText(L"Leave blank to keep the stored password");
            password.Visibility(Visibility::Collapsed);

            PasswordBox recoveryPassword;
            recoveryPassword.Header(box_value(
                L"New recovery email password (optional)"));
            recoveryPassword.PlaceholderText(
                L"Leave blank to keep the stored password");
            recoveryPassword.Visibility(Visibility::Collapsed);

            TextBlock passwordStatus;
            passwordStatus.Text(
                hadRecoveryPassword
                    ? L"Passwords are stored securely. Use the card's Credentials menu to copy them after verification."
                    : L"The primary password is stored securely; no recovery email password is stored.");
            passwordStatus.TextWrapping(TextWrapping::Wrap);
            passwordStatus.Foreground(Application::Current().Resources()
                .Lookup(box_value(L"AppMutedTextBrush")).as<Brush>());

            TextBlock validation;
            validation.Visibility(Visibility::Collapsed);
            SolidColorBrush validationBrush;
            validationBrush.Color(color(248, 81, 73));
            validation.Foreground(validationBrush);

            fields.Children().Append(heading);
            fields.Children().Append(category);
            fields.Children().Append(serviceName);
            fields.Children().Append(username);
            fields.Children().Append(email);
            fields.Children().Append(website);
            fields.Children().Append(recoveryEmail);
            fields.Children().Append(notes);
            fields.Children().Append(passwordStatus);
            fields.Children().Append(password);
            fields.Children().Append(recoveryPassword);
            fields.Children().Append(validation);

            ScrollViewer scroller;
            scroller.MaxHeight(640);
            scroller.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
            scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
            scroller.Content(fields);
            dialog.Content(scroller);

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
                        if (!editing)
                        {
                            args.Cancel(true);
                            editing = true;
                            category.IsEnabled(true);
                            serviceName.IsReadOnly(false);
                            username.IsReadOnly(false);
                            email.IsReadOnly(false);
                            website.IsReadOnly(false);
                            recoveryEmail.IsReadOnly(false);
                            notes.IsReadOnly(false);
                            password.Visibility(Visibility::Visible);
                            recoveryPassword.Visibility(Visibility::Visible);
                            sender.PrimaryButtonText(L"Save");
                            co_return;
                        }

                        const auto clickArgs{ args };
                        const auto activeDialog{ sender };
                        const auto dispatcher{ DispatcherQueue() };
                        deferral = clickArgs.GetDeferral();
                        const std::wstring categoryText{
                            categoryValue(category) };
                        if (serviceName.Text().empty() ||
                            categoryText.empty() ||
                            (username.Text().empty() && email.Text().empty()))
                        {
                            clickArgs.Cancel(true);
                            validation.Text(
                                L"Service name, category, and a username or email are required.");
                            validation.Visibility(Visibility::Visible);
                            completeDeferral(deferral);
                            co_return;
                        }
                        if (!recoveryPassword.Password().empty() &&
                            recoveryEmail.Text().empty())
                        {
                            clickArgs.Cancel(true);
                            validation.Text(
                                L"Enter a recovery email before its password.");
                            validation.Visibility(Visibility::Visible);
                            completeDeferral(deferral);
                            co_return;
                        }

                        std::optional<std::wstring> passwordValue;
                        if (!password.Password().empty())
                        {
                            passwordValue = password.Password().c_str();
                        }
                        std::optional<std::wstring> recoveryPasswordValue;
                        if (!recoveryPassword.Password().empty())
                        {
                            recoveryPasswordValue =
                                recoveryPassword.Password().c_str();
                        }
                        else if (recoveryEmail.Text().empty() &&
                            hadRecoveryPassword)
                        {
                            recoveryPasswordValue = std::wstring{};
                        }
                        auto wipePassword{
                            account_vault::security::wipeOnExit(passwordValue) };
                        auto wipeRecoveryPassword{
                            account_vault::security::wipeOnExit(
                                recoveryPasswordValue) };
                        password.Password(L"");
                        recoveryPassword.Password(L"");
                        activeDialog.IsPrimaryButtonEnabled(false);
                        activeDialog.PrimaryButtonText(L"Saving...");

                        const std::wstring serviceValue{
                            serviceName.Text().c_str() };
                        const std::wstring usernameValue{ username.Text().c_str() };
                        const std::wstring emailValue{ email.Text().c_str() };
                        const std::wstring websiteValue{ website.Text().c_str() };
                        const std::wstring recoveryEmailValue{
                            recoveryEmail.Text().c_str() };
                        const std::wstring notesValue{ notes.Text().c_str() };

                        bool result{ false };
                        try
                        {
                            co_await resume_background();
                            result = updateCredential(
                                id,
                                serviceValue,
                                categoryText,
                                usernameValue,
                                emailValue,
                                std::move(passwordValue),
                                websiteValue,
                                recoveryEmailValue,
                                std::move(recoveryPasswordValue),
                                notesValue);
                        }
                        catch (...)
                        {
                            result = false;
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

                        activeDialog.IsPrimaryButtonEnabled(true);
                        activeDialog.PrimaryButtonText(L"Save");
                        if (!result)
                        {
                            clickArgs.Cancel(true);
                            validation.Text(
                                L"The credential changes could not be saved securely.");
                            validation.Visibility(Visibility::Visible);
                        }
                        else
                        {
                            saved = true;
                        }
                        completeDeferral(deferral);
                    }
                    catch (...)
                    {
                        try
                        {
                            args.Cancel(true);
                            validation.Text(
                                L"The credential changes could not be saved securely.");
                            validation.Visibility(Visibility::Visible);
                        }
                        catch (...)
                        {
                        }
                        completeDeferral(deferral);
                    }
                });

            attachDialogToShell(dialog, ModelessWindowKind::CredentialDetails);
            dialogAttached = true;
            co_await dialog.ShowAsync(ContentDialogPlacement::InPlace);
            password.Password(L"");
            recoveryPassword.Password(L"");
            dialog.Hide();
            detachModelessWindow(dialog, ModelessWindowKind::CredentialDetails);
            dialogAttached = false;

            if (saved)
            {
                rebuildRecordFilter();
                refreshAccounts();
                StatusText().Text(L"Credential changes saved securely");
            }
        }
        catch (...)
        {
            if (dialogAttached && dialog)
            {
                dialog.Hide();
                detachModelessWindow(dialog, ModelessWindowKind::CredentialDetails);
            }
            try
            {
                if (saved)
                {
                    rebuildRecordFilter();
                    refreshAccounts();
                    StatusText().Text(
                        L"Credential changes saved; the vault view was refreshed");
                }
                else
                {
                    StatusText().Text(
                        L"The Credential Details dialog encountered a UI error");
                }
            }
            catch (...)
            {
            }
        }
    }
}
