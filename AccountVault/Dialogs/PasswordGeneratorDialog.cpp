#include "pch.h"
#include "../MainWindow.xaml.h"
#include "../Security/SensitiveData.h"

#include <bcrypt.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Text.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#pragma comment(lib, "Bcrypt.lib")

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Automation;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::Foundation;

namespace
{
    [[nodiscard]] bool isChecked(CheckBox const& checkBox) noexcept
    {
        const auto value{ checkBox.IsChecked() };
        return value && value.Value();
    }

    [[nodiscard]] std::uint32_t secureRandomIndex(std::uint32_t upperBound)
    {
        if (upperBound == 0)
        {
            throw std::invalid_argument{ "The random range cannot be empty." };
        }

        // A 32-bit source has 2^32 possible values. Discard the short tail so
        // every remainder occurs the same number of times.
        constexpr std::uint64_t SourceRange{
            static_cast<std::uint64_t>(
                (std::numeric_limits<std::uint32_t>::max)()) + 1ULL };
        const std::uint64_t acceptedRange{
            SourceRange - (SourceRange % upperBound) };

        std::uint32_t value{};
        do
        {
            const NTSTATUS status{ ::BCryptGenRandom(
                nullptr,
                reinterpret_cast<PUCHAR>(&value),
                static_cast<ULONG>(sizeof(value)),
                BCRYPT_USE_SYSTEM_PREFERRED_RNG) };
            if (status < 0)
            {
                throw std::runtime_error{
                    "Windows could not generate secure random data." };
            }
        }
        while (static_cast<std::uint64_t>(value) >= acceptedRange);

        return value % upperBound;
    }

    [[nodiscard]] wchar_t randomCharacter(std::wstring_view characters)
    {
        return characters[secureRandomIndex(
            static_cast<std::uint32_t>(characters.size()))];
    }

    void secureShuffle(std::wstring& value)
    {
        for (std::size_t remaining{ value.size() };
             remaining > 1;
             --remaining)
        {
            const std::size_t swapIndex{ secureRandomIndex(
                static_cast<std::uint32_t>(remaining)) };
            std::swap(value[remaining - 1], value[swapIndex]);
        }
    }
}

namespace winrt::AccountVault::implementation
{
    fire_and_forget MainWindow::showPasswordGenerator()
    {
        account_vault::ui::ModelessToolWindow dialog{ nullptr };
        bool dialogAttached{ false };

        try
        {
            auto lifetime{ get_strong() };

            if (m_passwordGeneratorWindow)
            {
                m_passwordGeneratorWindow.Activate();
                co_return;
            }

            dialog = account_vault::ui::ModelessToolWindow{
                L"Password generator",
                720,
                600 };
            dialog.XamlRoot(Content().XamlRoot());
            dialog.Title(box_value(L"Password generator"));
            dialog.PrimaryButtonText(L"");
            dialog.CloseButtonText(L"Close");
            dialog.MaxWidth(720);

            const auto accentBrush{ Application::Current().Resources()
                .Lookup(box_value(L"AppAccentBrush")).as<Brush>() };
            const auto mutedBrush{ Application::Current().Resources()
                .Lookup(box_value(L"AppMutedTextBrush")).as<Brush>() };
            const auto borderBrush{ Application::Current().Resources()
                .Lookup(box_value(L"AppBorderBrush")).as<Brush>() };

            StackPanel content;
            content.Spacing(16);

            TextBlock introduction;
            introduction.Text(
                L"Generate a password locally with Windows' cryptographic random source. Nothing is saved automatically.");
            introduction.TextWrapping(TextWrapping::Wrap);
            introduction.Foreground(mutedBrush);

            TextBox generatedPassword;
            generatedPassword.Header(box_value(L"Generated password"));
            generatedPassword.IsReadOnly(true);
            generatedPassword.FontFamily(FontFamily{ L"Cascadia Mono" });
            generatedPassword.FontSize(17);
            generatedPassword.HorizontalContentAlignment(
                HorizontalAlignment::Stretch);
            AutomationProperties::SetName(
                generatedPassword,
                L"Generated password");

            Grid actions;
            actions.ColumnSpacing(12);
            for (int column{}; column < 2; ++column)
            {
                ColumnDefinition definition;
                definition.Width(GridLengthHelper::FromValueAndType(
                    1,
                    GridUnitType::Star));
                actions.ColumnDefinitions().Append(definition);
            }

            Button generateButton;
            generateButton.Content(box_value(L"Generate"));
            generateButton.HorizontalAlignment(HorizontalAlignment::Stretch);
            generateButton.Style(Application::Current().Resources()
                .Lookup(box_value(L"PrimaryActionButtonStyle")).as<Style>());
            AutomationProperties::SetName(
                generateButton,
                L"Generate a new password");

            Button copyButton;
            copyButton.Content(box_value(L"Copy"));
            copyButton.HorizontalAlignment(HorizontalAlignment::Stretch);
            Grid::SetColumn(copyButton, 1);
            AutomationProperties::SetName(
                copyButton,
                L"Copy generated password");

            actions.Children().Append(generateButton);
            actions.Children().Append(copyButton);

            Border divider;
            divider.Height(1);
            divider.Background(borderBrush);

            Grid optionsHeader;
            optionsHeader.ColumnSpacing(16);
            optionsHeader.ColumnDefinitions().Append(ColumnDefinition{});
            optionsHeader.ColumnDefinitions().GetAt(0).Width(
                GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
            optionsHeader.ColumnDefinitions().Append(ColumnDefinition{});
            optionsHeader.ColumnDefinitions().GetAt(1).Width(
                GridLengthHelper::Auto());

            TextBlock optionsHeading;
            optionsHeading.Text(L"GENERATOR OPTIONS");
            optionsHeading.FontFamily(FontFamily{ L"Cascadia Mono" });
            optionsHeading.FontWeight(
                Windows::UI::Text::FontWeights::SemiBold());
            optionsHeading.Foreground(accentBrush);
            optionsHeading.VerticalAlignment(VerticalAlignment::Center);

            NumberBox length;
            length.Header(box_value(L"Length"));
            length.Minimum(8);
            length.Maximum(128);
            length.SmallChange(1);
            length.LargeChange(8);
            length.Value(20);
            length.Width(130);
            length.SpinButtonPlacementMode(
                NumberBoxSpinButtonPlacementMode::Compact);
            Grid::SetColumn(length, 1);
            AutomationProperties::SetName(length, L"Password length");

            optionsHeader.Children().Append(optionsHeading);
            optionsHeader.Children().Append(length);

            Grid characterOptions;
            characterOptions.ColumnSpacing(24);
            characterOptions.RowSpacing(10);
            for (int column{}; column < 2; ++column)
            {
                ColumnDefinition definition;
                definition.Width(GridLengthHelper::FromValueAndType(
                    1,
                    GridUnitType::Star));
                characterOptions.ColumnDefinitions().Append(definition);
            }
            characterOptions.RowDefinitions().Append(RowDefinition{});
            characterOptions.RowDefinitions().GetAt(0).Height(
                GridLengthHelper::Auto());
            characterOptions.RowDefinitions().Append(RowDefinition{});
            characterOptions.RowDefinitions().GetAt(1).Height(
                GridLengthHelper::Auto());
            characterOptions.RowDefinitions().Append(RowDefinition{});
            characterOptions.RowDefinitions().GetAt(2).Height(
                GridLengthHelper::Auto());

            CheckBox lowercase;
            lowercase.Content(box_value(L"Lowercase letters"));
            lowercase.IsChecked(true);

            CheckBox uppercase;
            uppercase.Content(box_value(L"Uppercase letters"));
            uppercase.IsChecked(true);
            Grid::SetColumn(uppercase, 1);

            CheckBox numbers;
            numbers.Content(box_value(L"Numbers"));
            numbers.IsChecked(true);
            Grid::SetRow(numbers, 1);

            CheckBox symbols;
            symbols.Content(box_value(L"Symbols"));
            symbols.IsChecked(true);
            Grid::SetRow(symbols, 1);
            Grid::SetColumn(symbols, 1);

            CheckBox avoidAmbiguous;
            avoidAmbiguous.Content(
                box_value(L"Avoid ambiguous characters (I, l, O, 0, 1)"));
            avoidAmbiguous.IsChecked(true);
            Grid::SetRow(avoidAmbiguous, 2);
            Grid::SetColumnSpan(avoidAmbiguous, 2);

            characterOptions.Children().Append(lowercase);
            characterOptions.Children().Append(uppercase);
            characterOptions.Children().Append(numbers);
            characterOptions.Children().Append(symbols);
            characterOptions.Children().Append(avoidAmbiguous);

            TextBlock validation;
            validation.Text(L"Select at least one character set.");
            validation.Foreground(accentBrush);
            validation.Visibility(Visibility::Collapsed);

            ProgressBar strength;
            strength.Minimum(0);
            strength.Maximum(128);
            strength.Height(6);

            TextBlock strengthLabel;
            strengthLabel.FontFamily(FontFamily{ L"Cascadia Mono" });
            strengthLabel.Foreground(mutedBrush);

            content.Children().Append(introduction);
            content.Children().Append(generatedPassword);
            content.Children().Append(actions);
            content.Children().Append(divider);
            content.Children().Append(optionsHeader);
            content.Children().Append(characterOptions);
            content.Children().Append(validation);
            content.Children().Append(strength);
            content.Children().Append(strengthLabel);

            const auto generate = [&]() -> bool
            {
                try
                {
                    const bool omitAmbiguous{ isChecked(avoidAmbiguous) };

                    const std::wstring lowerCharacters{
                        omitAmbiguous
                            ? L"abcdefghijkmnopqrstuvwxyz"
                            : L"abcdefghijklmnopqrstuvwxyz" };
                    const std::wstring upperCharacters{
                        omitAmbiguous
                            ? L"ABCDEFGHJKLMNPQRSTUVWXYZ"
                            : L"ABCDEFGHIJKLMNOPQRSTUVWXYZ" };
                    const std::wstring numberCharacters{
                        omitAmbiguous ? L"23456789" : L"0123456789" };
                    const std::wstring symbolCharacters{
                        L"!@#$%^&*()-_=+[]{};:,.?/" };

                    std::vector<std::wstring const*> selectedSets;
                    if (isChecked(lowercase))
                    {
                        selectedSets.push_back(&lowerCharacters);
                    }
                    if (isChecked(uppercase))
                    {
                        selectedSets.push_back(&upperCharacters);
                    }
                    if (isChecked(numbers))
                    {
                        selectedSets.push_back(&numberCharacters);
                    }
                    if (isChecked(symbols))
                    {
                        selectedSets.push_back(&symbolCharacters);
                    }

                    if (selectedSets.empty())
                    {
                        validation.Visibility(Visibility::Visible);
                        generatedPassword.Text(L"");
                        strength.Value(0);
                        strengthLabel.Text(L"No character set selected");
                        return false;
                    }

                    validation.Visibility(Visibility::Collapsed);
                    const std::size_t requestedLength{
                        static_cast<std::size_t>(std::clamp(
                            std::isfinite(length.Value())
                                ? std::round(length.Value())
                                : 20.0,
                            8.0,
                            128.0)) };
                    length.Value(static_cast<double>(requestedLength));

                    std::wstring pool;
                    for (auto const* set : selectedSets)
                    {
                        pool += *set;
                    }

                    std::wstring password;
                    password.reserve(requestedLength);
                    auto wipePassword{
                        account_vault::security::wipeOnExit(password) };

                    // Include every requested class at least once, fill from
                    // the combined pool, then securely shuffle positions.
                    for (auto const* set : selectedSets)
                    {
                        password.push_back(randomCharacter(*set));
                    }
                    while (password.size() < requestedLength)
                    {
                        password.push_back(randomCharacter(pool));
                    }
                    secureShuffle(password);

                    generatedPassword.Text(hstring{ password });

                    const double entropyBits{
                        static_cast<double>(requestedLength) *
                        std::log2(static_cast<double>(pool.size())) };
                    strength.Value((std::min)(entropyBits, 128.0));

                    std::wstring rating;
                    if (entropyBits < 60.0)
                    {
                        rating = L"FAIR";
                    }
                    else if (entropyBits < 80.0)
                    {
                        rating = L"STRONG";
                    }
                    else
                    {
                        rating = L"EXCELLENT";
                    }

                    std::wstring strengthText{ rating };
                    strengthText += L"  |  approximately ";
                    strengthText += std::to_wstring(
                        static_cast<unsigned>(entropyBits + 0.5));
                    strengthText += L" bits";
                    strengthLabel.Text(hstring{ strengthText });
                    return true;
                }
                catch (...)
                {
                    generatedPassword.Text(L"");
                    validation.Text(
                        L"Windows could not generate a password. Try again.");
                    validation.Visibility(Visibility::Visible);
                    strength.Value(0);
                    strengthLabel.Text(L"Generation failed");
                    return false;
                }
            };

            generateButton.Click(
                [&](IInspectable const&, RoutedEventArgs const&)
                {
                    static_cast<void>(generate());
                });

            copyButton.Click(
                [&](IInspectable const&, RoutedEventArgs const&)
                {
                    if (generatedPassword.Text().empty() && !generate())
                    {
                        return;
                    }

                    std::wstring password{ generatedPassword.Text().c_str() };
                    auto wipePassword{
                        account_vault::security::wipeOnExit(password) };
                    copyToClipboard(password, L"Generated password");
                });

            length.ValueChanged(
                [&](NumberBox const&, NumberBoxValueChangedEventArgs const&)
                {
                    if (!generatedPassword.Text().empty())
                    {
                        static_cast<void>(generate());
                    }
                });

            dialog.Content(content);
            static_cast<void>(generate());

            attachDialogToShell(dialog);
            dialogAttached = true;
            m_passwordGeneratorWindow = dialog;

            co_await dialog.ShowAsync(ContentDialogPlacement::InPlace);

            // XAML owns an immutable text buffer while the password is shown;
            // release it as soon as the utility window closes.
            generatedPassword.Text(L"");
            detachModelessWindow(dialog);
            dialogAttached = false;
            m_passwordGeneratorWindow = nullptr;
        }
        catch (...)
        {
            try
            {
                if (dialogAttached && dialog)
                {
                    dialog.Hide();
                    detachModelessWindow(dialog);
                }
                m_passwordGeneratorWindow = nullptr;
            }
            catch (...)
            {
            }

            try
            {
                StatusText().Text(
                    L"The password generator encountered an unexpected error");
            }
            catch (...)
            {
            }
        }
    }
}
