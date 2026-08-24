#include "pch.h"
#include "../MainWindow.xaml.h"

#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Text.h>

#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::Foundation;
using namespace Windows::UI;

namespace winrt::AccountVault::implementation
{
    fire_and_forget MainWindow::showColorDialog()
    {
        account_vault::ui::ModelessToolWindow dialog{ nullptr };
        bool dialogAttached{ false };

        try
        {
        auto lifetime{ get_strong() };

        // Theme editor layout revision v11: fit the popup to its 920-DIP editor
        // grid and remove the unused outer bands around the content.

        dialog = account_vault::ui::ModelessToolWindow{ L"Customize colors", 1040, 700 };
        dialog.XamlRoot(Content().XamlRoot());
        dialog.Title(box_value(L"Create theme"));
        dialog.PrimaryButtonText(L"Save theme");
        dialog.CloseButtonText(L"Cancel");
        dialog.DefaultButton(ContentDialogButton::Primary);
        dialog.HorizontalAlignment(HorizontalAlignment::Center);
        dialog.VerticalAlignment(VerticalAlignment::Center);

        // WinUI's default ContentDialog width is intentionally narrow. This
        // editor uses a wider local template limit without changing other dialogs.
        dialog.Resources().Insert(
            box_value(L"ContentDialogMaxWidth"),
            box_value(1040.0));
        dialog.Resources().Insert(
            box_value(L"ContentDialogMinWidth"),
            box_value(980.0));

        ThemeDefinition draft{
            L"",
            brushColor(L"AppBackgroundBrush"),
            brushColor(L"AppSurfaceBrush"),
            brushColor(L"AppSurfaceAltBrush"),
            brushColor(L"AppAccentBrush"),
            brushColor(L"AppTextBrush"),
            brushColor(L"AppMutedTextBrush") };

        StackPanel dialogContent;
        dialogContent.Spacing(16);

        TextBox themeName;
        themeName.Header(box_value(L"Theme name"));
        themeName.PlaceholderText(L"My custom theme");

        TextBlock validation;
        validation.Text(L"Enter a name for the theme.");
        validation.Visibility(Visibility::Collapsed);
        SolidColorBrush validationBrush;
        validationBrush.Color(color(248, 81, 73));
        validation.Foreground(validationBrush);

        const std::array<std::wstring_view, 6> tokenNames{
            L"Background",
            L"Account cards",
            L"Alternate surface",
            L"Accent",
            L"Primary text",
            L"Muted text" };

        const auto colorAt = [&](int index) -> Color
        {
            switch (index)
            {
            case 0: return draft.background;
            case 1: return draft.surface;
            case 2: return draft.surfaceAlt;
            case 3: return draft.accent;
            case 4: return draft.text;
            case 5: return draft.mutedText;
            default: return draft.background;
            }
        };

        const auto setColorAt = [&](int index, Color value)
        {
            switch (index)
            {
            case 0: draft.background = value; break;
            case 1: draft.surface = value; break;
            case 2: draft.surfaceAlt = value; break;
            case 3: draft.accent = value; break;
            case 4: draft.text = value; break;
            case 5: draft.mutedText = value; break;
            default: break;
            }
        };

        const auto colorHex = [](Color value)
        {
            constexpr wchar_t digits[] = L"0123456789ABCDEF";
            std::wstring result{ L"#000000" };
            result[1] = digits[(value.R >> 4) & 0x0F];
            result[2] = digits[value.R & 0x0F];
            result[3] = digits[(value.G >> 4) & 0x0F];
            result[4] = digits[value.G & 0x0F];
            result[5] = digits[(value.B >> 4) & 0x0F];
            result[6] = digits[value.B & 0x0F];
            return hstring{ result };
        };

        const auto tryParseHex = [](hstring const& text, Color& parsed)
        {
            const std::wstring_view value{ text.c_str(), text.size() };
            if (value.size() != 7 || value[0] != L'#')
            {
                return false;
            }

            const auto digitValue = [](wchar_t digit) -> int
            {
                if (digit >= L'0' && digit <= L'9')
                {
                    return digit - L'0';
                }
                if (digit >= L'A' && digit <= L'F')
                {
                    return digit - L'A' + 10;
                }
                if (digit >= L'a' && digit <= L'f')
                {
                    return digit - L'a' + 10;
                }
                return -1;
            };

            std::array<int, 6> digits{};
            for (std::size_t index = 0; index < digits.size(); ++index)
            {
                digits[index] = digitValue(value[index + 1]);
                if (digits[index] < 0)
                {
                    return false;
                }
            }

            parsed = ColorHelper::FromArgb(
                255,
                static_cast<std::uint8_t>(digits[0] * 16 + digits[1]),
                static_cast<std::uint8_t>(digits[2] * 16 + digits[3]),
                static_cast<std::uint8_t>(digits[4] * 16 + digits[5]));
            return true;
        };

        const auto makeSolidBrush = [](Color value)
        {
            SolidColorBrush brush;
            brush.Color(value);
            return brush;
        };

        SolidColorBrush previewBackgroundBrush{ makeSolidBrush(draft.background) };
        SolidColorBrush previewSurfaceBrush{ makeSolidBrush(draft.surface) };
        SolidColorBrush previewSurfaceAltBrush{ makeSolidBrush(draft.surfaceAlt) };
        SolidColorBrush previewAccentBrush{ makeSolidBrush(draft.accent) };
        SolidColorBrush previewTextBrush{ makeSolidBrush(draft.text) };
        SolidColorBrush previewMutedTextBrush{ makeSolidBrush(draft.mutedText) };

        const auto updatePreview = [&]()
        {
            previewBackgroundBrush.Color(draft.background);
            previewSurfaceBrush.Color(draft.surface);
            previewSurfaceAltBrush.Color(draft.surfaceAlt);
            previewAccentBrush.Color(draft.accent);
            previewTextBrush.Color(draft.text);
            previewMutedTextBrush.Color(draft.mutedText);
        };

        Grid editor;
        editor.Width(920);
        editor.HorizontalAlignment(HorizontalAlignment::Center);
        editor.ColumnSpacing(24);

        ColumnDefinition tokenColumn;
        tokenColumn.Width(GridLengthHelper::FromPixels(200));
        editor.ColumnDefinitions().Append(tokenColumn);

        ColumnDefinition pickerColumn;
        pickerColumn.Width(
            GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
        editor.ColumnDefinitions().Append(pickerColumn);

        ColumnDefinition previewColumn;
        previewColumn.Width(GridLengthHelper::FromPixels(320));
        editor.ColumnDefinitions().Append(previewColumn);

        StackPanel tokenPanel;
        tokenPanel.Spacing(6);

        TextBlock tokenHeading;
        tokenHeading.Text(L"THEME COLORS");
        tokenHeading.FontFamily(FontFamily{ L"Cascadia Mono" });
        tokenHeading.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        tokenHeading.Foreground(previewAccentBrush);
        tokenPanel.Children().Append(tokenHeading);

        ColorPicker picker;
        picker.Color(draft.background);
        picker.MaxHeight(345);
        picker.IsAlphaEnabled(false);
        picker.IsColorChannelTextInputVisible(false);
        picker.IsHexInputVisible(false);
        picker.IsColorSpectrumVisible(true);
        picker.IsColorSliderVisible(true);

        TextBox hexInput;
        hexInput.Header(box_value(L"HEX"));
        hexInput.Text(colorHex(draft.background));

        NumberBox redInput;
        redInput.Header(box_value(L"R"));
        redInput.Minimum(0);
        redInput.Maximum(255);
        redInput.SmallChange(1);
        redInput.Value(draft.background.R);

        NumberBox greenInput;
        greenInput.Header(box_value(L"G"));
        greenInput.Minimum(0);
        greenInput.Maximum(255);
        greenInput.SmallChange(1);
        greenInput.Value(draft.background.G);

        NumberBox blueInput;
        blueInput.Header(box_value(L"B"));
        blueInput.Minimum(0);
        blueInput.Maximum(255);
        blueInput.SmallChange(1);
        blueInput.Value(draft.background.B);

        Grid colorInputs;
        colorInputs.ColumnSpacing(8);

        ColumnDefinition hexInputColumn;
        hexInputColumn.Width(GridLengthHelper::FromPixels(112));
        colorInputs.ColumnDefinitions().Append(hexInputColumn);

        for (int column = 0; column < 3; ++column)
        {
            ColumnDefinition channelColumn;
            channelColumn.Width(
                GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
            colorInputs.ColumnDefinitions().Append(channelColumn);
        }

        Grid::SetColumn(hexInput, 0);
        Grid::SetColumn(redInput, 1);
        Grid::SetColumn(greenInput, 2);
        Grid::SetColumn(blueInput, 3);
        colorInputs.Children().Append(hexInput);
        colorInputs.Children().Append(redInput);
        colorInputs.Children().Append(greenInput);
        colorInputs.Children().Append(blueInput);

        int selectedToken{ 0 };
        bool synchronizingPicker{ false };
        bool synchronizingInputs{ false };

        const auto updateInputs = [&](Color value)
        {
            synchronizingInputs = true;
            hexInput.Text(colorHex(value));
            redInput.Value(value.R);
            greenInput.Value(value.G);
            blueInput.Value(value.B);
            synchronizingInputs = false;
        };

        const auto commitRgbInputs = [&]()
        {
            if (synchronizingInputs ||
                std::isnan(redInput.Value()) ||
                std::isnan(greenInput.Value()) ||
                std::isnan(blueInput.Value()))
            {
                return;
            }

            picker.Color(ColorHelper::FromArgb(
                255,
                static_cast<std::uint8_t>(redInput.Value()),
                static_cast<std::uint8_t>(greenInput.Value()),
                static_cast<std::uint8_t>(blueInput.Value())));
        };

        redInput.ValueChanged(
            [&](NumberBox const&, NumberBoxValueChangedEventArgs const&)
            {
                commitRgbInputs();
            });
        greenInput.ValueChanged(
            [&](NumberBox const&, NumberBoxValueChangedEventArgs const&)
            {
                commitRgbInputs();
            });
        blueInput.ValueChanged(
            [&](NumberBox const&, NumberBoxValueChangedEventArgs const&)
            {
                commitRgbInputs();
            });

        hexInput.TextChanged(
            [&](IInspectable const&, TextChangedEventArgs const&)
            {
                if (synchronizingInputs)
                {
                    return;
                }

                Color parsed{};
                if (tryParseHex(hexInput.Text(), parsed))
                {
                    picker.Color(parsed);
                }
            });

        TextBlock selectedTokenHeading;
        selectedTokenHeading.Text(hstring{ tokenNames[0] });
        selectedTokenHeading.FontSize(18);
        selectedTokenHeading.FontWeight(
            Windows::UI::Text::FontWeights::SemiBold());

        TextBlock pickerHelp;
        pickerHelp.Text(L"Changes update the preview immediately.");
        pickerHelp.Foreground(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppMutedTextBrush"))
                .as<Brush>());

        StackPanel pickerPanel;
        pickerPanel.Spacing(10);
        pickerPanel.Children().Append(selectedTokenHeading);
        pickerPanel.Children().Append(pickerHelp);
        pickerPanel.Children().Append(picker);
        pickerPanel.Children().Append(colorInputs);

        StackPanel previewPanel;
        previewPanel.Spacing(10);
        previewPanel.HorizontalAlignment(HorizontalAlignment::Stretch);

        TextBlock previewHeading;
        previewHeading.Text(L"LIVE PREVIEW");
        previewHeading.FontFamily(FontFamily{ L"Cascadia Mono" });
        previewHeading.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        previewHeading.Foreground(previewAccentBrush);
        previewHeading.Margin(Thickness{ 0, 0, 0, 2 });

        Border previewFrame;
        // Keep the preview large enough to read without turning the unused
        // lower portion of the modal into an empty canvas.
        previewFrame.MinHeight(300);
        previewFrame.HorizontalAlignment(HorizontalAlignment::Stretch);
        previewFrame.Padding(Thickness{ 0 });
        previewFrame.CornerRadius(CornerRadius{ 8, 8, 8, 8 });
        previewFrame.Background(previewBackgroundBrush);
        previewFrame.BorderBrush(
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppBorderBrush"))
                .as<Brush>());
        previewFrame.BorderThickness(Thickness{ 1 });

        Border previewContentContainer;
        previewContentContainer.HorizontalAlignment(
            HorizontalAlignment::Stretch);
        previewContentContainer.VerticalAlignment(
            VerticalAlignment::Center);
        previewContentContainer.Margin(Thickness{ 18, 14, 18, 14 });

        StackPanel previewBody;
        previewBody.HorizontalAlignment(HorizontalAlignment::Stretch);
        previewBody.Spacing(14);

        TextBlock previewTitle;
        previewTitle.Text(L"Launcher accounts");
        previewTitle.FontSize(20);
        previewTitle.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        previewTitle.Foreground(previewTextBrush);
        previewTitle.Margin(Thickness{ 0, 0, 0, 2 });

        TextBlock previewSubtitle;
        previewSubtitle.Text(L"A preview of the active palette.");
        previewSubtitle.TextWrapping(TextWrapping::Wrap);
        previewSubtitle.Foreground(previewMutedTextBrush);
        previewSubtitle.Margin(Thickness{ 0 });

        Border previewCard;
        previewCard.HorizontalAlignment(HorizontalAlignment::Stretch);
        previewCard.Padding(Thickness{ 0, 16, 0, 16 });
        previewCard.Margin(Thickness{ 0, 16, 0, 16 });
        previewCard.CornerRadius(CornerRadius{ 6, 6, 6, 6 });
        previewCard.Background(previewSurfaceBrush);
        previewCard.BorderBrush(previewSurfaceAltBrush);
        previewCard.BorderThickness(Thickness{ 1 });

        StackPanel previewCardBody;
        previewCardBody.Width(160);
        previewCardBody.HorizontalAlignment(HorizontalAlignment::Center);
        previewCardBody.Margin(Thickness{ 0 });
        previewCardBody.Spacing(10);

        Border previewBadge;
        previewBadge.Padding(Thickness{ 9, 5, 9, 5 });
        previewBadge.CornerRadius(CornerRadius{ 5, 5, 5, 5 });
        previewBadge.HorizontalAlignment(HorizontalAlignment::Center);
        previewBadge.Margin(Thickness{ 0, 0, 0, 2 });
        previewBadge.Background(previewSurfaceAltBrush);

        TextBlock previewBadgeText;
        previewBadgeText.Text(L"Steam");
        previewBadgeText.FontFamily(FontFamily{ L"Cascadia Mono" });
        previewBadgeText.FontWeight(
            Windows::UI::Text::FontWeights::SemiBold());
        previewBadgeText.Foreground(previewAccentBrush);
        previewBadge.Child(previewBadgeText);

        TextBlock previewUsername;
        previewUsername.Text(L"night_shift");
        previewUsername.HorizontalAlignment(HorizontalAlignment::Center);
        previewUsername.TextAlignment(TextAlignment::Center);
        previewUsername.Foreground(previewTextBrush);

        TextBlock previewEmail;
        previewEmail.Text(L"night@example.com");
        previewEmail.HorizontalAlignment(HorizontalAlignment::Center);
        previewEmail.TextAlignment(TextAlignment::Center);
        previewEmail.Foreground(previewMutedTextBrush);

        Border previewAction;
        previewAction.Width(160);
        previewAction.Padding(Thickness{ 10, 7, 10, 7 });
        previewAction.HorizontalAlignment(HorizontalAlignment::Center);
        previewAction.Margin(Thickness{ 0, 6, 0, 0 });
        previewAction.CornerRadius(CornerRadius{ 5, 5, 5, 5 });
        previewAction.Background(previewSurfaceAltBrush);

        TextBlock previewActionText;
        previewActionText.Text(L"Copy password");
        previewActionText.HorizontalAlignment(HorizontalAlignment::Center);
        previewActionText.TextAlignment(TextAlignment::Center);
        previewActionText.Foreground(previewAccentBrush);
        previewAction.Child(previewActionText);

        previewCardBody.Children().Append(previewBadge);
        previewCardBody.Children().Append(previewUsername);
        previewCardBody.Children().Append(previewEmail);
        previewCardBody.Children().Append(previewAction);
        previewCard.Child(previewCardBody);

        previewBody.Children().Append(previewTitle);
        previewBody.Children().Append(previewSubtitle);
        previewBody.Children().Append(previewCard);
        previewContentContainer.Child(previewBody);
        previewFrame.Child(previewContentContainer);

        previewPanel.Children().Append(previewHeading);
        previewPanel.Children().Append(previewFrame);

        std::vector<Button> tokenButtons;
        std::vector<SolidColorBrush> tokenSwatchBrushes;
        std::vector<TextBlock> tokenHexLabels;
        tokenButtons.reserve(tokenNames.size());
        tokenSwatchBrushes.reserve(tokenNames.size());
        tokenHexLabels.reserve(tokenNames.size());

        const Brush normalTokenBrush =
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppSurfaceBrush"))
                .as<Brush>();
        const Brush selectedTokenBrush =
            Application::Current()
                .Resources()
                .Lookup(box_value(L"AppSurfaceAltBrush"))
                .as<Brush>();

        for (int index = 0; index < static_cast<int>(tokenNames.size()); ++index)
        {
            Button tokenButton;
            tokenButton.MinHeight(56);
            tokenButton.Padding(Thickness{ 10, 6, 10, 6 });
            tokenButton.HorizontalAlignment(HorizontalAlignment::Stretch);
            tokenButton.HorizontalContentAlignment(HorizontalAlignment::Stretch);
            tokenButton.Background(
                index == 0 ? selectedTokenBrush : normalTokenBrush);

            Grid tokenContent;
            tokenContent.ColumnSpacing(8);

            RowDefinition labelRow;
            labelRow.Height(GridLengthHelper::Auto());
            tokenContent.RowDefinitions().Append(labelRow);

            RowDefinition hexRow;
            hexRow.Height(GridLengthHelper::Auto());
            tokenContent.RowDefinitions().Append(hexRow);

            ColumnDefinition swatchColumn;
            swatchColumn.Width(GridLengthHelper::FromPixels(22));
            tokenContent.ColumnDefinitions().Append(swatchColumn);

            ColumnDefinition labelColumn;
            labelColumn.Width(
                GridLengthHelper::FromValueAndType(1, GridUnitType::Star));
            tokenContent.ColumnDefinitions().Append(labelColumn);

            SolidColorBrush swatchBrush{ makeSolidBrush(colorAt(index)) };

            Border swatch;
            swatch.Width(20);
            swatch.Height(20);
            swatch.CornerRadius(CornerRadius{ 4, 4, 4, 4 });
            swatch.Background(swatchBrush);

            TextBlock tokenLabel;
            tokenLabel.Text(hstring{ tokenNames[index] });
            tokenLabel.VerticalAlignment(VerticalAlignment::Center);

            TextBlock hexLabel;
            hexLabel.Text(colorHex(colorAt(index)));
            hexLabel.FontFamily(FontFamily{ L"Cascadia Mono" });
            hexLabel.FontSize(11);
            hexLabel.Foreground(
                Application::Current()
                    .Resources()
                    .Lookup(box_value(L"AppMutedTextBrush"))
                    .as<Brush>());

            Grid::SetColumn(swatch, 0);
            Grid::SetRowSpan(swatch, 2);
            Grid::SetColumn(tokenLabel, 1);
            Grid::SetRow(hexLabel, 1);
            Grid::SetColumn(hexLabel, 1);
            tokenContent.Children().Append(swatch);
            tokenContent.Children().Append(tokenLabel);
            tokenContent.Children().Append(hexLabel);
            tokenButton.Content(tokenContent);

            tokenButton.Click(
                [&, index](IInspectable const&, RoutedEventArgs const&)
                {
                    selectedToken = index;
                    selectedTokenHeading.Text(hstring{ tokenNames[index] });

                    for (int buttonIndex = 0;
                        buttonIndex < static_cast<int>(tokenButtons.size());
                        ++buttonIndex)
                    {
                        tokenButtons[buttonIndex].Background(
                            buttonIndex == index
                                ? selectedTokenBrush
                                : normalTokenBrush);
                    }

                    synchronizingPicker = true;
                    const Color selectedColor{ colorAt(index) };
                    picker.Color(selectedColor);
                    updateInputs(selectedColor);
                    synchronizingPicker = false;
                });

            tokenButtons.push_back(tokenButton);
            tokenSwatchBrushes.push_back(swatchBrush);
            tokenHexLabels.push_back(hexLabel);
            tokenPanel.Children().Append(tokenButton);
        }

        picker.ColorChanged(
            [&](ColorPicker const&, ColorChangedEventArgs const& args)
            {
                if (synchronizingPicker)
                {
                    return;
                }

                setColorAt(selectedToken, args.NewColor());
                tokenSwatchBrushes[selectedToken].Color(args.NewColor());
                tokenHexLabels[selectedToken].Text(colorHex(args.NewColor()));
                updateInputs(args.NewColor());
                updatePreview();
            });

        Grid::SetColumn(tokenPanel, 0);
        Grid::SetColumn(pickerPanel, 1);
        Grid::SetColumn(previewPanel, 2);
        editor.Children().Append(tokenPanel);
        editor.Children().Append(pickerPanel);
        editor.Children().Append(previewPanel);

        dialogContent.Children().Append(themeName);
        dialogContent.Children().Append(validation);
        dialogContent.Children().Append(editor);
        dialog.Content(dialogContent);

        dialog.PrimaryButtonClick(
            [&](account_vault::ui::ModelessToolWindow const&, account_vault::ui::ModelessButtonClickEventArgs const& args)
            {
                if (themeName.Text().empty())
                {
                    args.Cancel(true);
                    validation.Visibility(Visibility::Visible);
                    themeName.Focus(FocusState::Programmatic);
                }
            });

        attachDialogToShell(dialog);
        dialogAttached = true;

        const ContentDialogResult result =
            co_await dialog.ShowAsync(ContentDialogPlacement::InPlace);

        detachModelessWindow(dialog);
        dialogAttached = false;

        if (result != ContentDialogResult::Primary)
        {
            co_return;
        }

        draft.name = themeName.Text().c_str();
        m_customThemes.push_back(draft);

        ComboBoxItem item;
        item.Content(box_value(hstring{ draft.name }));
        ThemePicker().Items().Append(item);
        ThemePicker().SelectedIndex(
            BuiltInThemeCount +
            static_cast<int>(m_customThemes.size()) - 1);

        std::wstring status{ L"Theme created: " };
        status += draft.name;
        status += L" (session only)";
        StatusText().Text(hstring{ status });
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
            }
            catch (...)
            {
            }

            try
            {
                StatusText().Text(
                    L"The theme editor encountered an unexpected error");
            }
            catch (...)
            {
            }
        }
    }
}
