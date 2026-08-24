# Account Armory features

Updated: 2026-08-24  
Status: personal alpha

## Vaults and records

| Feature | Status | Notes |
| --- | --- | --- |
| Launcher Vault | Complete | Linked launcher and email credentials |
| Credential Vault | Complete | General website and service credentials |
| Add, view, and edit | Complete | Blank password fields preserve existing passwords |
| Search and filtering | Complete | Workspace-specific search and launcher/category filters |
| Remove one record | Complete | Persisted immediately |
| Remove shown records | Complete | Uses the current search/filter and requires confirmation |
| Restart persistence | Tested | Empty and populated vault states survive restart |

Supported launcher labels: Steam, Riot Client, Epic, Battle.net, EA App,
Ubisoft Connect, Rockstar Games Launcher, and Other.

## Security

| Feature | Status | Notes |
| --- | --- | --- |
| Local password protection | Complete | Windows DPAPI, tied to the Windows user |
| Windows Hello | Complete | Gates unlock, copy/reveal, and backup operations |
| Automatic lock | Complete | Configurable; defaults to 90 seconds |
| Clipboard cleanup | Complete | Clears after 30 seconds if content is unchanged |
| Reveal timeout | Complete | Revealed passwords hide after 30 seconds |
| Plaintext cleanup | Complete | Sensitive temporary C++ values are wiped where practical |
| App-wide locked state | Complete | Ordinary controls are disabled until unlock |

## Import and export

| Feature | Status | Notes |
| --- | --- | --- |
| Encrypted full-vault backup | Complete | `.aabackup`, AES-256-GCM |
| Single-record backup | Complete | Export/import one record |
| Transactional import | Complete | Repository changes only after the full save succeeds |
| Wrong-password handling | Complete | Up to three attempts before import ends |
| Modified-backup rejection | Complete | Authenticated decryption imports nothing on failure |
| Browser CSV import | Complete | Credential Vault only |

Browser import recognizes common URL, username, password, email, title, and
service headers. It accepts comma-, semicolon-, and tab-separated exports from
Chrome, Edge, Firefox, Safari, and compatible browsers. Extra columns and
different column orders are ignored.

## Interface and utilities

| Feature | Status | Notes |
| --- | --- | --- |
| Responsive layouts | Complete | Wide, center-only, and compact |
| Password generator | Complete | Local cryptographic random source |
| Built-in themes | Complete | Ten palettes |
| Custom color editor | Partial | Works during the current session only |
| Auto-lock utility | Complete | Lock now and configure timeout |
| Accessibility metadata | Complete | Names, help text, access keys, and live status |

## Reliability rules

- Passwords are never stored as plaintext in `accounts.json`.
- Search and filtering never modify stored data.
- Failed saves do not replace the in-memory repository.
- Storage uses a temporary file and final-file replacement.
- Corrupt JSON is quarantined instead of overwritten.
- Unsupported future schemas are left untouched.
- Imported records receive fresh local IDs.
- Wrong passwords, modified backups, cancellation, and failed saves import zero
  records.
- Locking invalidates pending password-copy/reveal and unlock operations.

## Testing status

Passed manually with fake data:

- Create, edit, remove, remove shown, and restart persistence
- Encrypted export and import
- Canceling file pickers and password prompts
- Wrong backup password and three-attempt retry limit
- Modified encrypted backup rejection
- Browser CSV import and credential-card mapping
- Auto-lock and Windows Hello unlock

## Remaining work

- Persist custom themes across restarts.
- Produce and sign an MSIX installer.
- Test installation and upgrades on a clean Windows computer.
- Add broader automated UI/input-validation tests.
- Optional future features: Microsoft account linking, cloud sync, and game
  discovery/launching.

