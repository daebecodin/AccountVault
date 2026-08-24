Account Armory v24 - Window and Taskbar Icon Fix
=================================================

This update reorganizes the existing MainWindow implementation without
changing the app's behavior or introducing additional WinRT runtime classes.

App icon assets
---------------

The knight-and-key artwork is preserved as a true RGBA image in
Assets\AppIconSource.png. All pixels outside the helmet and key have zero alpha.
Transparent Windows-sized derivatives replace the project templates for the
app list, taskbar, Store logo, lock screen, wide tile, and splash screen. The
complete 16-through-256 target-size set is included for both light and dark
Windows shell themes. The taskbar variants crop the source transparency before
scaling, so the artwork occupies the available icon canvas more fully.

Assets\AccountArmory.ico contains 16, 24, 32, 48, 64, 128, and 256 pixel
frames. AccountArmory.rc embeds the ICO directly into AccountVault.exe. After
the window is activated, App.xaml.cpp loads the embedded small and large icon
frames and sends WM_SETICON to the WinUI window. This avoids any dependency on
an external ICO deployment path and prevents the generic file icon fallback.

Product-facing branding reads "Account Armory" in the window title and main
heading. The internal AccountVault C++/WinRT namespace, project name, and binary
name remain intentionally unchanged. Renaming them would require coordinated
changes to the solution, package manifest, generated XAML/WinRT metadata,
deployment identity, and existing build paths without improving the visible
branding.

Windows can cache installed package icons. After copying the files, clean and
rebuild the solution, uninstall the previously deployed package if its cached
taskbar icon remains, then redeploy from Visual Studio.

Email provider behavior
-----------------------

Add Account and Edit Account Details now share one provider catalog containing
Gmail, Yahoo, Outlook, MSN, Inbox.lv, and ZSTHost. Account Details displays the
selected provider as a clickable link in view mode and changes it to a dropdown
when editing.

Source responsibilities
-----------------------

MainWindow.xaml.cpp
  Window construction, UI event handlers, filtering, and account-list refresh.

App.xaml.cpp
  App launch, window activation, and native title-bar/taskbar icon assignment.

AccountArmory.rc and resource.h
  Native multi-resolution icon resource compiled into the executable.

Components\AccountCard.cpp
  Account-card construction, clipboard operations, removal, and two labeled
  action groups: a 2x2 Copy Credentials grid plus Details/Remove under
  Account Actions.

Dialogs\AddAccountDialog.cpp
  Horizontal Add Account dialog, field validation, and repository insertion.

Dialogs\AccountDetailsDialog.cpp
  Horizontal details dialog, editing, validation, and repository updates.

Dialogs\ThemeEditorDialog.cpp
  Custom-theme editor, color inputs, and live preview.

Themes\ThemeService.cpp
  Built-in theme definitions and application-resource color updates.

Models\Account.h
  Account data model.

Models\ThemeDefinition.h
  Theme palette data model shared by theme-related components.

Services\AccountRepository.h
  In-memory metadata storage, search, update, and removal.

Services\AccountStorageService.h/.cpp
  Versioned JSON metadata loading and atomic local-data writes.

Services\CredentialService.h/.cpp
  Launcher and email passwords protected locally with Windows DPAPI.

Services\AccountPersistence.cpp
  Coordinates repository, metadata-file, and credential operations.

Services\EmailProviderCatalog.h
  Shared provider names and destinations used by Add and Details dialogs.

Installation
------------

1. Close the running AccountVault application.
2. Copy these files and folders into the existing AccountVault project folder.
3. Replace AccountVault.vcxproj and AccountVault.vcxproj.filters.
4. In Package.appxmanifest, change both application-facing DisplayName values
   from "AccountVault" or "Account Vault" to "Account Armory". Do not change
   the package Identity Name or the Executable/EntryPoint namespace values.
5. In Visual Studio, reload the project if prompted.
6. Build > Clean Solution, then Build > Rebuild Solution.

Important invariant
-------------------

MainWindow remains the sole WinRT/XAML runtime class. Its member functions may
be defined in different .cpp translation units as long as every declaration
remains in MainWindow.xaml.h and every .cpp file is registered in the project.

This is the low-risk first boundary. Later, individual dialog translation
units can be promoted to independent classes without performing every
architectural change at once.
