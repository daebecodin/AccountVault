Account Armory - DPAPI-protected credential storage
===================================================

Storage layout
--------------

accounts.json
  Stored in Windows.Storage.ApplicationData.Current.LocalFolder.
  Contains schema version 2, the next record ID, account metadata, and two
  Base64-encoded DPAPI ciphertext blobs per account. Passwords are never stored
  as plaintext.

Windows DPAPI
  CryptProtectData protects each password for the current Windows user.
  CryptUnprotectData is called only when a password is copied. The ciphertext
  normally cannot be decrypted by another Windows user or on another computer.

Runtime invariant
-----------------

Account owns metadata and ciphertext only. Windows Hello verification is
required immediately before DPAPI decrypts a password for reveal or copy.
Plaintext exists only while the user enters it, while it is displayed after
verification, or while clipboard content is being prepared.

Persistence behavior
--------------------

- Add and edit protect only passwords that changed.
- Each mutation serializes one in-memory snapshot to accounts.json.tmp.
- MoveFileExW atomically replaces accounts.json without forced write-through.
- A failed file replacement restores the previous in-memory account state.
- Schema version 1 is migrated once from Windows Credential Locker to DPAPI.
- Legacy Credential Locker entries are removed only after schema version 2 is
  saved successfully.
- If migration fails, existing metadata remains readable and storage mutations
  are disabled so the legacy file is not overwritten.

Manual verification
-------------------

1. Back up the current accounts.json file.
2. Clean and rebuild the solution.
3. Start the app and confirm the migration status appears once.
4. Restart and confirm every account still appears.
5. Copy both passwords from an existing migrated account.
6. Add an account and confirm saving completes quickly.
7. Edit metadata without entering passwords and confirm both passwords remain.
8. Change one password and confirm only that password changes.
9. Remove an account, restart, and confirm it remains removed.
10. Confirm reveal and password-copy actions require Windows Hello.

Security work still required before public release
--------------------------------------------------

- Clear password clipboard contents after a short timeout when still unchanged.
- Add automated persistence, corruption, rollback, and migration tests.
- Perform a security review and publish an explicit threat model/privacy policy.
