# Storage and security

## Local vault

Account data is stored as `accounts.json` in
`Windows.Storage.ApplicationData.Current.LocalFolder`. The current schema is
version 3. Password fields contain DPAPI-protected ciphertext rather than
plaintext.

Saving writes a complete candidate state to `accounts.json.tmp`, flushes it,
and replaces the final file. Failed saves keep the previous repository state.
Startup recovery can promote a valid temporary file and quarantine corrupt
JSON without overwriting an unsupported future schema.

Local app data is not compiled into the executable or installer. Every Windows
user receives separate application storage.

## Password handling

- `CryptProtectData` and `CryptUnprotectData` bind local passwords to the
  current Windows user.
- Windows Hello is required before password copy/reveal and vault unlock.
- Clipboard history and roaming are disabled for copied credentials.
- Unchanged copied passwords are cleared after 30 seconds.
- Temporary plaintext values are wiped where the APIs allow it.

## Portable backups

`.aabackup` files use AES-256-GCM with a password-derived key. They can move
records between computers because the imported plaintext is re-protected with
DPAPI only after the backup is authenticated and Windows Hello succeeds.

Wrong passwords and modified files share one error message, receive at most
three attempts per import flow, and never partially update the vault.

