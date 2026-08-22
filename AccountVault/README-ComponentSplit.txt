AccountVault v20 - Email Provider Dropdowns
===========================================

This update reorganizes the existing MainWindow implementation without
changing the app's behavior or introducing additional WinRT runtime classes.

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

Components\AccountCard.cpp
  Account-card construction, card actions, clipboard operations, and removal.

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
  In-memory account storage, search, update, and removal.

Services\EmailProviderCatalog.h
  Shared provider names and destinations used by Add and Details dialogs.

Installation
------------

1. Close the running AccountVault application.
2. Copy these files and folders into the existing AccountVault project folder.
3. Replace AccountVault.vcxproj and AccountVault.vcxproj.filters.
4. In Visual Studio, reload the project if prompted.
5. Build > Clean Solution, then Build > Rebuild Solution.

Important invariant
-------------------

MainWindow remains the sole WinRT/XAML runtime class. Its member functions may
be defined in different .cpp translation units as long as every declaration
remains in MainWindow.xaml.h and every .cpp file is registered in the project.

This is the low-risk first boundary. Later, individual dialog translation
units can be promoted to independent classes without performing every
architectural change at once.
