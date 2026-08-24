# Account Armory text editing guide

This file shows where the app's visible words are currently defined. Text is
not centralized yet, so use this guide to find the correct screen or feature.

## How text is written

- In `.xaml` files, edit quoted values such as `Text="..."`, `Content="..."`,
  `PlaceholderText="..."`, and `AutomationProperties.Name="..."`.
- In `.cpp` files, most visible text is a wide string such as `L"Add account"`.
- Keep `AutomationProperties.Name`, `AutomationProperties.HelpText`, and tooltips
  accurate when changing the visible label of a control. Screen readers use them.
- Rebuild the app after changing text in either XAML or C++.

## Main window and navigation

### `MainWindow.xaml`

Edit this file for static text that exists as soon as the main window loads:

- Window title and the `ACCOUNT ARMORY` brand label
- Launcher Vault and Credential Vault navigation labels
- Account/Vault Actions and Utilities menu labels
- Import, export, add, theme, password-generator, and auto-lock menu entries
- Search placeholder defaults
- Empty-state title and description defaults
- Lock-screen message and Unlock button
- Status-bar defaults and accessibility labels
- The theme names shown in both the compact menu and right-side selector

Useful searches inside this file:

```text
Text="
Content="
PlaceholderText="
AutomationProperties.Name="
AutomationProperties.HelpText="
ToolTipService.ToolTip="
```

### `MainWindow.xaml.cpp`

Edit this file for text that changes while the app is running:

- Launcher Vault versus Credential Vault headings and subtitles
- Workspace-specific search placeholders
- Add/import/export and `Remove shown` wording for each vault
- Empty-state text for each vault
- `All launchers` and `All categories`
- Account/credential counts in the status bar
- Lock, unlock, migration, startup-theme, and auto-lock status messages

The workspace wording is grouped in `MainWindow::switchWorkspace`. Status-bar
account counts are built in `MainWindow::refreshAccounts`.

## Account and credential cards

### `Components/AccountCard.cpp`

Edit this file for text shown on each record card and its menus:

- `SERVICE`, `IN-GAME NAME`, and `EMAIL` labels
- `CREDENTIALS`, `RECORD`, and `ACCOUNT` buttons
- Copy username/email/password actions
- Details, export, and remove actions
- Clipboard, single/bulk removal, and card error messages

## Popups and tool windows

Each popup keeps its labels, placeholders, validation messages, button text,
and completion messages in its own file:

| Popup or feature | File |
| --- | --- |
| Add launcher account | `Dialogs/AddAccountDialog.cpp` |
| View/edit launcher account | `Dialogs/AccountDetailsDialog.cpp` |
| Add/view/edit general credential | `Dialogs/CredentialDialog.cpp` |
| Bulk-import browser password CSV into credential cards | `Dialogs/BrowserCsvImportDialog.cpp` |
| Encrypted import/export backup | `Dialogs/BackupDialog.cpp` |
| Password generator | `Dialogs/PasswordGeneratorDialog.cpp` |
| Auto-lock settings | `Dialogs/AutoLockDialog.cpp` |
| Custom theme editor and preview | `Dialogs/ThemeEditorDialog.cpp` |

Common popup searches:

```text
dialog.Title
PrimaryButtonText
CloseButtonText
.Header(
.PlaceholderText(
.Text(
.Content(
```

The browser importer automatically recognizes common Chrome, Edge, Firefox,
Safari, and Chromium-style column names. Header aliases and comma, semicolon,
or tab format detection are defined in `Services/BrowserCsvImportService.cpp`.

`Components/ModelessToolWindow.cpp` controls how shared popup buttons are laid
out and displayed. It normally should not be edited just to rename a popup.

## Lists and catalog names

### `Services/LauncherCatalog.h`

Contains the launcher names displayed by the launcher selector and account
cards. If an existing launcher is renamed, keep its old name accepted in
`launcherFromName` so previously saved records can still load.

### `Services/EmailProviderCatalog.h`

Contains email-provider names and their associated websites. These values are
used by the add/edit account dialogs and are stored with account records.

### `Services/CredentialCategoryCatalog.h`

Contains the default Credential Vault categories such as Finance, School, and
Work. User-created categories are stored in account data and will still appear.

### Theme names

Built-in theme names currently appear in two places:

- `MainWindow.xaml` — the names visible in the theme controls
- `Themes/ThemeService.cpp` — the built-in theme definitions

Keep the names and their order synchronized. The numeric theme tags in XAML
must continue to match the definition order in `ThemeService.cpp`.

The custom-theme editor's color-token labels and live-preview sample text are
in `Dialogs/ThemeEditorDialog.cpp`.

## Status, validation, and error text

These files contain additional messages users may see:

| Message type | File |
| --- | --- |
| Windows identity-verification prompts and results | `Services/UserVerificationService.cpp` |
| Browser CSV format/parser errors | `Services/BrowserCsvImportService.cpp` |
| Backup-format and encryption errors | `Services/PortableBackupService.cpp` |
| Account-storage load/save errors | `Services/AccountStorageService.cpp` |
| DPAPI password display label | `Services/CredentialService.cpp` |

Some low-level service strings are technical errors or file-format values, not
ordinary UI copy. Check how a string is used before changing it.

## App name and Windows package text

The app name appears in several places:

- `MainWindow.xaml` — window title, visible brand, lock screen, and accessibility
- `MainWindow.xaml.cpp` — lock/unlock and status messages
- `app.manifest` — unpackaged executable description
- `Package.appxmanifest` — packaged app display name, publisher display name,
  and description

Search the whole project for both `Account Armory` and `AccountVault` before a
full rename. `AccountVault` is also the C++/WinRT namespace and project name, so
changing every occurrence is not a simple text-only rename.

## Do not casually rename these strings

The following are identifiers or persisted data rather than visible wording:

- JSON field names in `Services/AccountStorageService.cpp`
- Backup format, JSON fields, cipher names, and KDF names in
  `Services/PortableBackupService.cpp`
- DPAPI entropy/purpose strings in `Services/CredentialService.cpp`
- Settings keys such as `StartupThemeIndex` and `AutoLockTimeoutSeconds` in
  `MainWindow.xaml.cpp`
- XAML resource keys such as `AppAccentBrush` and `AppTextBrush`
- Enum names, namespaces, generated class names, and event-handler names

Changing those can break saved accounts, backups, settings, encryption access,
or generated XAML bindings.

## Fast whole-project search

In Visual Studio, use **Edit > Find and Replace > Find in Files** and search for
the exact text currently shown in the app. Limit the search to the AccountVault
project. For a broader audit, search for:

```text
Text="
Content="
PlaceholderText="
AutomationProperties.
L"
```

The `L"` search also finds internal identifiers, so use the warnings above
before editing every result.
