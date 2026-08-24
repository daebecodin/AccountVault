# Changelog

Notable Account Armory changes are recorded here.

## Unreleased

### Changed

- Reorganized project documentation under `docs/`.
- Replaced the WinUI template readme with a project-specific README.
- Expanded `.gitignore` for Visual Studio, WinUI, NuGet, packaging, temporary,
  and sensitive export files.
- Removed the obsolete tracked `AccountVault/current` project duplicate.

### Added

- Added a first-run TeachingTip walkthrough covering vault navigation, adding
  and importing records, encrypted backups, recovery-password safety, DPAPI,
  and security utilities.
- Added a reusable **Getting started** entry to both compact and wide utility
  layouts, with completion remembered in local app settings.
- Added concise tooltips to the primary navigation and compact action controls.

## 2026-08-24 — Personal alpha

### Added

- Launcher Vault and general Credential Vault workspaces.
- Launcher choices for Steam, Riot Client, Epic, Battle.net, EA App, Ubisoft
  Connect, Rockstar Games Launcher, and Other.
- Credential categories, search, filtering, add/edit/details, per-record remove,
  and confirmed **Remove shown** bulk deletion.
- Browser CSV import for common Chrome, Edge, Firefox, Safari, and Chromium
  export layouts, including comma, semicolon, and tab delimiters.
- Password generator and ten built-in themes.
- Theme color editor with live preview.
- Configurable auto-lock controls and a dedicated lock screen.

### Security and reliability

- DPAPI protection for locally stored passwords.
- Windows Hello verification for password copy, reveal, unlock, backup export,
  and authenticated backup import.
- AES-256-GCM `.aabackup` files with password-based key derivation.
- Backup-password validation before Windows Hello and a maximum of three retry
  attempts per import.
- Atomic account storage, temporary-file recovery, corrupt-file quarantine,
  rollback on failed saves, and fresh IDs for imported records.
- Clipboard history/roaming disabled with a 30-second conditional auto-clear.
- Password reveal countdowns and secure cleanup on early-return/error paths.

### UI

- Responsive wide, center-only, and compact layouts.
- Navigation and utility rails, compact action menus, status bar, and window
  size indicators.
- Restyled account actions, modeless popup windows, and improved popup spacing.
- Account Armory icon and user-facing branding.

### Fixed

- Provider link text encoding in account details.
- Popup footer alignment and outer-container borders.
- Locked-state interaction so only the unlock action remains usable.
- Save-time behavior in auto-lock settings.
- Browser CSV mapping into Credential Vault cards.
- Remove-shown visibility at every supported window size.

### Testing

- Manual alpha checks passed for create/edit/remove, restart persistence,
  encrypted export/import, canceled pickers, wrong backup passwords, modified
  backups, and three-attempt backup unlock behavior.

### Known limitations

- Custom themes are session-only.
- A signed MSIX installer and clean-machine installation test are pending.
- Microsoft account linking, cloud sync, and automatic game discovery/launching
  are not implemented.
