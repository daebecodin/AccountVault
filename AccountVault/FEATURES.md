# Account Armory — Feature Tracker

Updated: 2026-08-23  
Code baseline: v28 + v28.1 + v28.2 + v28.3 + v28.4 + v28.5 + v28.6 + v28.7.3 + v29  
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
- Invalid repository state is rejected before any storage file is changed.
- Corrupt JSON is preserved under a recovery filename before it is disabled.
- An unsupported future schema is never renamed or overwritten.
- XAML controls are changed only on the UI thread.
- Every dialog deferral is completed.
- No exception escapes `fire_and_forget`.
- Clipboard auto-clear never deletes newer clipboard content.
- Locking hides account UI and closes open dialogs.
- A verification result cannot copy a password after the app locks.
- Normal status messages never move or replace the auto-lock countdown.
- Theme order stays synchronized across XAML, header, and C++.

## Feature overview

| Status | Feature | Key behavior | Main file |
|---|---|---|---|
| `[x]` | Create account | Validates, encrypts, saves, adds one card | `Dialogs/AddAccountDialog.cpp` |
| `[x]` | View details | Read-only account view and provider link | `Dialogs/AccountDetailsDialog.cpp` |
| `[x]` | Edit account | Blank password keeps old password | `Dialogs/AccountDetailsDialog.cpp` |
| `[~]` | Remove account | Saves removal; no confirmation/undo | `Components/AccountCard.cpp` |
| `[x]` | JSON storage | Temp-file write then file replacement | `Services/AccountStorageService.cpp` |
| `[x]` | Storage recovery | Quarantine corrupt JSON; recover valid temp saves | `Services/AccountStorageService.cpp` |
| `[x]` | Storage tests | Debug-only deterministic 14-case suite | `Services/AccountStorageService.cpp` |
| `[x]` | DPAPI | Protects passwords before JSON storage | `Services/CredentialService.cpp` |
| `[x]` | Windows verification | Required per password copy/reveal | `Services/UserVerificationService.cpp` |
| `[x]` | Coroutine safety | No exception escapes `fire_and_forget` | Dialog/card source files |
| `[x]` | Reveal timeout | Shows a live countdown; hides at zero | `Dialogs/AccountDetailsDialog.cpp` |
| `[x]` | Clipboard clear | Clears after 30 seconds if unchanged | `Components/AccountCard.cpp` |
| `[x]` | Search/filter | Case-insensitive search + launcher filter | `Services/AccountRepository.h` |
| `[x]` | Incremental cards | CRUD refreshes only the affected card | `Components/AccountCard.cpp` |
| `[x]` | Email providers | Shared six-provider dropdown | `Services/EmailProviderCatalog.h` |
| `[x]` | Built-in themes | Ten selectable palettes | `Themes/ThemeService.cpp` |
| `[x]` | Startup theme | Right-click to set/remove default | `MainWindow.xaml.cpp` |
| `[~]` | Custom themes | Editor works; themes are session-only | `Dialogs/ThemeEditorDialog.cpp` |
| `[x]` | App auto-lock | Five-minute idle timer; lock on minimize/suspend | `MainWindow.xaml.cpp` |
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
Validate state -> write temp -> flush stream -> flush file -> replace final
```

Recovery:

```text
Valid final + stale temp -> keep final, delete temp
Missing final + valid temp -> promote temp
Corrupt final + valid temp -> quarantine final, promote temp
Corrupt final only -> quarantine, disable writes, restart into empty vault
Unsupported schema -> leave file untouched, disable writes
```

Load rejects:

- Empty or invalid JSON
- Unsupported schema
- Missing/wrong fields
- Invalid or duplicate IDs
- Invalid `nextRecordId`

Limits:

- Rewrites the complete JSON file per save.
- Maximum JSON size: 16 MiB.
- Maximum account count: 100,000.
- No portable backup yet.

### Developer storage tests `[x]`

Debug builds can run 14 deterministic tests in isolated `%TEMP%` folders.

```text
Visual Studio project properties
-> Debugging
-> Environment
-> ACCOUNT_ARMORY_RUN_STORAGE_TESTS=1
```

Run **Debug x64** and open **View -> Output -> Debug**. A copy is written to:

```text
%TEMP%\AccountArmoryStorageTests\latest-report.txt
```

Covered: create/edit/remove persistence, password preservation, schema 1
migration signal, corrupt JSON, future schema, duplicate/zero/next IDs, stale
temp recovery, corrupt-final recovery, failed replacement rollback, invalid
save-state rejection, and ID exhaustion.

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

### Password reveal timeout `[x]`

- Each revealed password has its own 30-second countdown timer.
- A local `Hides in 30s` countdown updates once per second.
- Launcher and email countdowns run independently.
- Manual Hide stops the timer and clears the text immediately.
- Entering edit mode stops both timers and clears both revealed values.
- Closing the dialog stops both timers and clears both revealed values.
- A verification result cannot reveal text after the dialog has closed.
- Reveal handlers catch errors so exceptions do not escape their coroutines.

### Clipboard `[x]`

- History disabled
- Roaming disabled
- Clears after 30 seconds
- Clears only when sequence number is unchanged
- Busy/failed clipboard access does not crash the app

Limits: closing the app stops the timer; another app may read the value first.

### Automatic app lock `[x]`

- Live `AUTO-LOCK M:SS` countdown updates once per second.
- Pointer, click, wheel, and keyboard input reset the five-minute deadline.
- Minimizing the window locks immediately.
- Windows system suspend requests an immediate lock.
- The lock overlay hides account content but leaves the status row visible.
- Open dialogs close when locking, clearing revealed values through cleanup.
- Unlock requires Windows verification.
- A new minimize/suspend lock request invalidates an unlock still awaiting
  Windows verification.
- Locking clears Account Armory's current copied account value only when it
  has not been replaced by newer clipboard content.
- A password-copy verification that finishes after locking exposes nothing.

Status layout:

```text
[normal status message................] [AUTO-LOCK M:SS]
 flexible left column                    fixed right column
```

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
- All eleven `fire_and_forget` bodies have an outer catch boundary.
- Copy/reveal buttons are re-enabled after coroutine failure.
- Add/Edit handler setup failures still complete their deferrals.
- Add/Details/Theme failures remove an attached orphan dialog.
- Details failures stop reveal timers and mark the dialog closed.
- Verification converts unexpected failures to `false`.

Missing:

- [ ] Choose one repository threading policy.
- [ ] Add useful diagnostics to recovery catches.
- [ ] Audit synchronous WinUI event callbacks separately.

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

- [ ] Decide repository threading policy.
- [ ] Audit synchronous WinUI event callbacks.

### Security

- [ ] Clear plaintext strings where practical.
- [ ] Review crash dumps and logs.
- [ ] Encrypted export/import.

### Reliability/UI

- [ ] Remove confirmation/Undo.
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

- [x] Create survives reload. (automated)
- [x] Metadata edit keeps both passwords. (automated)
- [ ] Changing one password keeps the other.
- [x] Remove survives reload. (automated)
- [x] Invalid JSON is quarantined and disables writes for that run. (automated)
- [x] Failed file replacement preserves the previous final file. (automated)
- [ ] Failed create/edit/remove restores correct UI state. (manual)

### Security

- [ ] Password copy/reveal requires verification.
- [ ] Canceling verification exposes nothing.
- [ ] Revealed launcher password hides after 30 seconds.
- [ ] Revealed email password hides after 30 seconds.
- [ ] Both countdown labels update once per second.
- [ ] Manual Hide and dialog close clear revealed text.
- [ ] Clipboard clears after 30 seconds.
- [ ] New clipboard content is preserved.
- [ ] Countdown stays fixed at the far right when normal status text changes.
- [ ] Pointer/keyboard input resets the countdown to five minutes.
- [ ] No input locks the app after five minutes.
- [ ] Minimize locks immediately; restoring shows the lock overlay.
- [ ] Sleep/resume returns to the lock overlay.
- [ ] Canceling unlock keeps the app locked.
- [ ] Successful Windows verification unlocks and resets the timer.
- [ ] Minimizing during unlock verification keeps the app locked.
- [ ] Locking closes open dialogs and hides revealed passwords.
- [ ] JSON contains no plaintext password.

### Async failure recovery

- [ ] Failed password copy re-enables its button and does not crash.
- [ ] Failed reveal clears the text, stops its timer, and re-enables its button.
- [ ] Add/Edit setup failure leaves no incomplete dialog deferral.
- [ ] Add/Details/Theme failure leaves no orphan dialog in `RootGrid`.
- [ ] Verification failure returns `false` and exposes no password.

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

- Added v29 storage reliability: state validation, durable temp-file flush,
  interrupted-save recovery, corrupt-file quarantine, future-schema
  preservation, ID overflow prevention, complete create rollback, and a
  debug-only 14-case storage test suite.
- Added v28.7.3 C++ name-lookup fix: windowing types in the AppWindow
  callback are fully qualified so they cannot collide with `MainWindow::AppWindow()`.
- Added v28.7.2 build diagnosis: the project copy of `MainWindow.xaml`
  contained only lines 96–220 of the complete file. Restoring the full XAML
  document fixes the multiple-root markup error.
- Added v28.7.1 XAML-root hotfix: `MainWindow.xaml` now begins directly with
  its single `<Window>` root for stricter markup-compiler parsing.
- Added v28.7 five-minute automatic app lock, immediate minimize/suspend lock,
  Windows-verification unlock, and fixed-right live status countdown.
- Added v28.6 fire-and-forget safety audit and recovery boundaries.
- Added orphan-dialog cleanup, reveal-timer cleanup, and no-throw verification.
- Added v28.5 live countdown labels beside revealed passwords.
- Added v28.4 password reveal timeout and dialog-close cleanup.
- Added exception boundaries around both password reveal handlers.
- Replaced the long invariant report with this editable feature tracker.
- Baseline includes DPAPI, Windows verification, incremental card updates,
  clipboard auto-clear, startup themes, and CRUD stability patches.
