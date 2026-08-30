; Firestorm viewer installer (Inno Setup 7)
; Replaces the legacy NSIS installer for the VulkanStorm build.
;
; This library is free software; you can redistribute it and/or
; modify it under the terms of the GNU Lesser General Public
; License as published by the Free Software Foundation;
; version 2.1 of the License only.
;
; This script is preprocessed by viewer_manifest.py before ISCC is invoked.
; Tokens of the form %%NAME%% are substituted by the manifest.

#define AppName       "%%APP_NAME%%"
#define AppNameOneWord "%%APP_NAME_ONEWORD%%"
#define AppVersion    "%%VERSION%%"
#define AppExe        "%%FINAL_EXE%%"
#define InstallerFile "%%INSTALLER_FILE%%"
#define InstallerOut "%%INSTALLER_OUT%%"
#define SourceDir     "%%SOURCE_DIR%%"
#define LicenseFile   "%%LICENSE_FILE%%"
#define SetupIcon     "%%SETUP_ICON%%"
#define AppURL        "%%DL_URL%%"

[Setup]
; Stable per-product upgrade GUID. Keep this constant so upgrades/reinstalls
; detect and upgrade prior installs instead of side-by-side installing.
AppId={{7A4C2E91-3F5B-4D6A-9C1E-5E8F2A6B9D04}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=The Phoenix Firestorm Project
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\{#AppNameOneWord}
DefaultGroupName={#AppName}
; Require 64-bit Windows
ArchitecturesAllowed=x64
; Show the LGPL-2.1 license agreement before anything else
LicenseFile={#LicenseFile}
; Offer "Launch Firestorm" on the finish page (see [Run] below)
; OutputBaseFilename must NOT carry .exe (ISCC appends it). The manifest
; passes InstallerOut = <base>_Setup so the result is <base>_Setup.exe.
OutputBaseFilename={#InstallerOut}
OutputDir={#SourceDir}
SetupIconFile={#SetupIcon}
UninstallDisplayIcon={app}\{#AppExe}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
; Register an uninstaller entry
Uninstallable=yes
CreateUninstallRegKey=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Files]
; Ship the entire staged viewer tree (everything viewer_manifest.py copied).
; Exclude generated installer/script artifacts so the installer never packs itself.
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "Phoenix-*.exe,firestorm_setup.iss,firestorm_setup_tmp.nsi"

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
; "Launch Firestorm" checkbox on the successful-completion page.
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName} now"; Flags: nowait postinstall skipifsilent
