# Account Armory

Account Armory is a local Windows credential organizer for game-launcher
accounts and general website credentials. Passwords are protected with Windows
DPAPI, sensitive actions require Windows Hello, and the vault locks after
inactivity.

## Main features

- Separate Launcher Vault and Credential Vault workspaces
- Search, filters, editing, and safe bulk removal of shown records
- Local DPAPI password protection and Windows Hello verification
- Configurable automatic locking with a 90-second default
- Encrypted `.aabackup` import and export
- Browser-password CSV import for Chrome, Edge, Firefox, Safari, and compatible
  formats
- Password generator, built-in themes, and custom color themes

## Installation

### Testers

A signed installer is not available yet. Friends and family should wait for a
signed MSIX release instead of copying a Debug build. Every Windows user gets a
separate, empty local vault after installation.

### Build from source

1. Install Visual Studio 2022 with C++ and Windows app development tools.
2. Clone the repository.
3. Open `AccountVault.slnx`.
4. Allow Visual Studio to restore the NuGet packages.
5. Select **Debug**, **x64**, then choose **Build > Rebuild Solution**.
6. Start the app from Visual Studio.

## Project status

Account Armory is ready for personal alpha testing. Core CRUD, locking,
encrypted backup, browser CSV import, cancellation, wrong-password, modified
backup, and restart scenarios have been manually tested. Custom themes are not
persisted yet, and signing/clean-machine installation are still pending.

## Documentation

- [Features](docs/FEATURES.md)
- [Changelog](CHANGELOG.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Storage and security](docs/STORAGE.md)
- [Packaging](docs/PACKAGING.md)
- [Text editing guide](docs/TEXT_EDITING_GUIDE.md)

