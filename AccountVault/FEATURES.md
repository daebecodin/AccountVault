# Account Armory — Feature Tracker

Updated: 2026-08-23  
Code baseline: v28 + v28.1 + v28.2 + v28.3  
App name: **Account Armory**  
Internal name: `AccountVault`

## Status

- `[x]` Working
- `[~]` Working, but incomplete
- `[ ]` Not implemented

## Rules that must stay true

- Buttons identify accounts with `RecordId`, not list position.
- Search/filter never changes stored data.
- JSON never contains plaintext passwords.
- Password copy/reveal requires Windows verification.
- Failed saves restore the old repository state.
- XAML controls are changed only on the UI thread.
- Every dialog deferral is completed.
- No exception escapes `fire_and_forget`.
- Clipboard auto-clear never deletes newer clipboard content.
- Theme order stays synchronized across XAML, header, and C++.

## Feature overview

| Status | Feature | Key behavior | Main file |
|---|---|---|---|
| `[x]` | Create account | Validates, encrypts, saves, adds one card | `Dialogs/AddAccountDialog.cpp` |
| `[x]` | View details | Read-only account view and provider link | `Dialogs/AccountDetailsDialog.cpp` |
| `[x]` | Edit account | Blank password keeps old password | `Dialogs/AccountDetailsDialog.cpp` |
| `[~]` | Remove account | Saves removal; no confirmation/undo | `Components/AccountCard.cpp` |
| `[x]` | JSON storage | Temp-file write then file replacement | `Services/AccountStorageService.cpp` |
| `[x]` | DPAPI | Protects passwords before JSON storage | `Services/CredentialService.cpp` |
| `[x]` | Windows verification | Required per password copy/reveal | `Services/UserVerificationService.cpp` |
| `[x]` | Clipboard clear | Clears after 30 seconds if unchanged | `Components/AccountCard.cpp` |
| `[x]` | Search/filter | Case-insensitive search + launcher filter | `Services/AccountRepository.h` |
| `[x]` | Incremental cards | CRUD refreshes only the affected card | `Components/AccountCard.cpp` |
| `[x]` | Email providers | Shared six-provider dropdown | `Services/EmailProviderCatalog.h` |
| `[x]` | Built-in themes | Ten selectable palettes | `Themes/ThemeService.cpp` |
| `[x]` | Startup theme | Right-click to set/remove default | `MainWindow.xaml.cpp` |
| `[~]` | Custom themes | Editor works; themes are session-only | `Dialogs/ThemeEditorDialog.cpp` |
| `[ ]` | App auto-lock | Lock on inactivity/minimize/suspend | — |
| `[ ]` | Export/import | Portable password-encrypted backup | — |

## Account record

```text
RecordId
Launcher
Launcher username
Protected launcher password
Email address
Email provider name
Email provider website
Protected email password
```

ID rules:

- Type: `std::uint64_t`
- Invalid: `0` and `UINT64_MAX`
- IDs must be unique.
- `nextRecordId` must be greater than every saved ID.
- Duplicate usernames/emails are currently allowed.

Source: `Models/Account.h`, `Services/AccountRepository.h`

## CRUD rules

### Create `[x]`

Required: launcher, username, launcher password, provider, email, email password.

```text
Validate -> encrypt -> repository add -> JSON save -> card add
Save fails -> remove new repository record
```

### Edit `[x]`

Required: launcher, username, provider, email.

```text
Blank password -> keep stored password
New password -> encrypt and replace that password
Save fails -> restore complete old account
```

### Remove `[~]`

```text
Repository remove -> JSON save -> card remove
Save fails -> restore repository snapshot
```

Missing:

- [ ] Confirmation or Undo
- [ ] Cleanup for leftover legacy `PasswordVault` entries

## Storage `[x]`

| Item | Value |
|---|---|
| Final file | `%LocalFolder%/accounts.json` |
| Temporary file | `%LocalFolder%/accounts.json.tmp` |
| Current schema | `2.0` |
| Legacy schema | `1.0` |

Save:

```text
Write temporary file -> flush/check -> replace final file
```

Load rejects:

- Empty or invalid JSON
- Unsupported schema
- Missing/wrong fields
- Invalid or duplicate IDs
- Invalid `nextRecordId`

Limits:

- Rewrites the complete JSON file per save.
- No corrupted-file backup or repair.
- No portable backup yet.

## Password security

### DPAPI `[x]`

- `CryptProtectData` / `CryptUnprotectData`
- Base64 ciphertext stored in JSON
- Tied to the current Windows user
- DPAPI plaintext buffer zeroed before free

Still exposed temporarily in:

- `std::wstring`
- `PasswordBox`
- Coroutine frames
- Clipboard readers before auto-clear

### Windows verification `[x]`

Required for:

- Copy launcher password
- Copy email password
- Reveal launcher password
- Reveal email password

Rule: verification applies to one action; no session is cached.

### Clipboard `[x]`

- History disabled
- Roaming disabled
- Clears after 30 seconds
- Clears only when sequence number is unchanged
- Busy/failed clipboard access does not crash the app

Limits: closing the app stops the timer; another app may read the value first.

## Search and cards `[x]`

Searches:

- Launcher
- Username
- Email
- Provider name
- Provider website

Filters: All, Steam, Riot, Epic, Other.

Performance:

- CRUD updates one card.
- Search/filter rebuilds all visible cards.
- Stale card insertion index falls back to append.

Button groups:

```text
CREDENTIALS                 ACCOUNT
Copy username  Copy email   Details
Copy launcher  Copy email   Remove
PW             PW
```

## Email providers `[x]`

| Provider | Website |
|---|---|
| Gmail | `https://mail.google.com/` |
| Yahoo | `https://mail.yahoo.com/` |
| Outlook | `https://outlook.live.com/mail/` |
| MSN | `https://outlook.live.com/mail/` |
| Inbox.lv | `https://www.inbox.lv/` |
| ZSTHost | `https://mail.zsthost.com/?_task=login` |

Rules:

- Add/Edit share one catalog.
- Name and URL are stored separately.
- Outlook and MSN remain separate choices.

## Themes

### Built-in `[x]`

Order:

1. Catppuccin Mocha
2. Tokyo Night
3. Dracula
4. Ayu Mirage
5. Dainty Dark
6. GitHub Dark
7. Atom One Dark
8. Houston
9. Night Owl
10. Matcha

```text
MainWindow.xaml order
== BuiltInThemeCount
== ThemeService.cpp switch order
```

### Startup theme `[x]`

- Right-click selected built-in theme.
- **Set default** saves `StartupThemeIndex`.
- **Remove default** works only on the saved default.
- Fallback: Ayu Mirage.
- Custom themes cannot be defaults.

### Custom themes `[~]`

Working: six colors, HEX/RGB input, live preview, named themes.

Missing:

- [ ] Persistence across runs
- [ ] Rename/delete
- [ ] Duplicate/whitespace-name validation

## Async rules `[~]`

Working:

- Add/Edit save off the UI thread.
- UI work resumes through the dispatcher.
- Add/Edit deferrals are completed.
- Saved data survives card-refresh failure.

Missing:

- [ ] Catch every `fire_and_forget` exception.
- [ ] Choose one repository threading policy.
- [ ] Add useful diagnostics to recovery catches.

Threading choice still required:

```text
Option A: repository access only on UI thread
Option B: protect repository with synchronization/snapshots
```

## Branding `[x]`

- Product name: Account Armory
- Internal name: `AccountVault`
- Icon: transparent knight helmet and key

Internal rename requires coordinated XAML, IDL, namespace, project, generated
type, and package changes.

## Next work

### High priority

- [ ] Catch all `fire_and_forget` exceptions.
- [ ] Add password reveal timeout.
- [ ] Decide repository threading policy.
- [ ] Add storage/rollback tests.

### Security

- [ ] Lock on minimize, suspend, and inactivity.
- [ ] Clear plaintext strings where practical.
- [ ] Review crash dumps and logs.
- [ ] Encrypted export/import.

### Reliability/UI

- [ ] Corrupted-file recovery.
- [ ] Remove confirmation/Undo.
- [ ] Prevent ID overflow.
- [ ] Trim/validate input and maximum lengths.
- [ ] Persist custom themes.
- [ ] Accessibility and keyboard review.

### Release

- [ ] Privacy policy and threat model.
- [ ] MSIX signing/versioning.
- [ ] Clean-machine install test.
- [ ] Store assets/listing.

## Test checklist

### Accounts/storage

- [ ] Create survives restart.
- [ ] Metadata edit keeps both passwords.
- [ ] Changing one password keeps the other.
- [ ] Remove survives restart.
- [ ] Invalid JSON disables writes.
- [ ] Failed create/edit/remove restores correct state.

### Security

- [ ] Password copy/reveal requires verification.
- [ ] Canceling verification exposes nothing.
- [ ] Clipboard clears after 30 seconds.
- [ ] New clipboard content is preserved.
- [ ] JSON contains no plaintext password.

### Themes

- [ ] Default survives restart.
- [ ] Remove default falls back to Ayu Mirage.
- [ ] Context-menu options enable correctly.
- [ ] Ten names match ten palettes.

## Update template

```text
Date:
Feature:
Status: [x] / [~] / [ ]
Files changed:
Rule changed:
Test result:
Remaining issue:
```

## Change log

### 2026-08-23

- Replaced the long invariant report with this editable feature tracker.
- Baseline includes DPAPI, Windows verification, incremental card updates,
  clipboard auto-clear, startup themes, and CRUD stability patches.
