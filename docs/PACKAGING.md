# Packaging

Account Armory does not yet have a production-signed installer. Do not send a
Debug output folder to testers as the final installation experience.

Before sharing broadly:

1. Build a Release x64 MSIX package.
2. Set the user-facing manifest name and description to Account Armory while
   preserving the internal package identity and entry point.
3. Sign the package with a certificate trusted by the test computer.
4. Install it on a clean Windows machine.
5. Verify first launch, Windows Hello, add/edit/remove, restart persistence,
   auto-lock, encrypted backup, uninstall, and upgrade behavior.

Until signing is complete, development and personal testing should run from
Visual Studio using `AccountVault.slnx`.

