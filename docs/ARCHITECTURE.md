# Architecture

Account Armory is a WinUI 3 C++/WinRT application. `MainWindow` remains the
single XAML runtime class; its implementation is split across focused C++
translation units.

| Directory or file | Responsibility |
| --- | --- |
| `AccountVault/MainWindow.*` | Window shell, responsive layout, navigation, locking, and shared handlers |
| `AccountVault/Components/` | Account cards and reusable popup-window infrastructure |
| `AccountVault/Dialogs/` | Add, edit, backup, CSV import, theme, password, and auto-lock flows |
| `AccountVault/Models/` | Account, launcher, and theme data types |
| `AccountVault/Services/` | Repository, storage, encryption, backup, catalogs, and verification |
| `AccountVault/Security/` | Sensitive-data wiping helpers |
| `AccountVault/Themes/` | Built-in theme definitions and application |
| `AccountVault/Assets/` | Icons and Windows package artwork |

New code should be placed with the feature it owns. Build files, manifests,
`App.*`, `MainWindow.*`, precompiled headers, and native resources stay in the
project root because Visual Studio and WinUI tooling expect them there.

