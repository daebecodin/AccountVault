# Text editing guide

Visible text is currently defined close to the screen or feature that owns it.

| Area | File |
| --- | --- |
| Main shell, navigation, lock screen, static menus | `AccountVault/MainWindow.xaml` |
| Dynamic workspace, filter, lock, and status text | `AccountVault/MainWindow.xaml.cpp` |
| Account cards and record actions | `AccountVault/Components/AccountCard.cpp` |
| Add launcher account | `AccountVault/Dialogs/AddAccountDialog.cpp` |
| Launcher account details | `AccountVault/Dialogs/AccountDetailsDialog.cpp` |
| Credential add/details | `AccountVault/Dialogs/CredentialDialog.cpp` |
| Backup import/export | `AccountVault/Dialogs/BackupDialog.cpp` |
| Browser CSV import | `AccountVault/Dialogs/BrowserCsvImportDialog.cpp` |
| Password generator | `AccountVault/Dialogs/PasswordGeneratorDialog.cpp` |
| Auto-lock settings | `AccountVault/Dialogs/AutoLockDialog.cpp` |
| Theme editor | `AccountVault/Dialogs/ThemeEditorDialog.cpp` |
| Launcher names | `AccountVault/Services/LauncherCatalog.h` |
| Email providers | `AccountVault/Services/EmailProviderCatalog.h` |
| Credential categories | `AccountVault/Services/CredentialCategoryCatalog.h` |
| Built-in themes | `AccountVault/MainWindow.xaml` and `AccountVault/Themes/ThemeService.cpp` |

In XAML, search for `Text=`, `Content=`, `PlaceholderText=`, and
`AutomationProperties.`. In C++, visible strings normally begin with `L"`.
Keep accessibility names, help text, and tooltips synchronized with visible
labels.

Do not rename persisted JSON fields, backup fields, encryption-purpose strings,
settings keys, resource keys, namespaces, generated class names, or XAML event
handlers unless the related migration/build changes are also made.

