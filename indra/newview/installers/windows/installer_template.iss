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
AppPublisher=The Vulkanstorm Project
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\{#AppNameOneWord}
DefaultGroupName={#AppName}
; Require 64-bit Windows
ArchitecturesAllowed=x64
; Show the LGPL-2.1 license agreement before anything else
LicenseFile={#LicenseFile}
; Offer "Launch Vulkanstorm" on the finish page (see [Run] below)
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
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*_Setup.exe,firestorm_setup.iss,firestorm_setup_tmp.nsi"

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
; "Launch Firestorm" checkbox on the successful-completion page.
Filename: "{app}\{#AppExe}"; Description: "Launch {#AppName} now"; Flags: nowait postinstall skipifsilent
[Registry]
; URL protocol handlers (secondlife://, hypergrid, hop://).
; The URL parameter is passed last; the viewer ignores subsequent params to
; avoid parameter-injection attacks (MAIT-8305 semantics preserved).
Root: HKCR; Subkey: "secondlife"; ValueType: string; ValueName: ""; ValueData: "URL:Second Life"; Flags: uninsdeletekey
Root: HKCR; Subkey: "secondlife"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""
Root: HKCR; Subkey: "secondlife\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"""
Root: HKCR; Subkey: "secondlife\shell\open"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "{#AppName}"
Root: HKCR; Subkey: "secondlife\shell\open\command"; ValueType: expandsz; ValueName: ""; ValueData: """{app}\{#AppExe}"" -url ""%1"""
Root: HKCR; Subkey: "x-grid-location-info"; ValueType: string; ValueName: ""; ValueData: "URL:Hypergrid"; Flags: uninsdeletekey
Root: HKCR; Subkey: "x-grid-location-info"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""
Root: HKCR; Subkey: "x-grid-location-info\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"""
Root: HKCR; Subkey: "x-grid-location-info\shell\open"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "{#AppName}"
Root: HKCR; Subkey: "x-grid-location-info\shell\open\command"; ValueType: expandsz; ValueName: ""; ValueData: """{app}\{#AppExe}"" -url ""%1"""
Root: HKCR; Subkey: "x-grid-info"; ValueType: string; ValueName: ""; ValueData: "URL:Hypergrid"; Flags: uninsdeletekey
Root: HKCR; Subkey: "x-grid-info"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""
Root: HKCR; Subkey: "x-grid-info\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"""
Root: HKCR; Subkey: "x-grid-info\shell\open"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "{#AppName}"
Root: HKCR; Subkey: "x-grid-info\shell\open\command"; ValueType: expandsz; ValueName: ""; ValueData: """{app}\{#AppExe}"" -url ""%1"""
; hop:// is registered only for OpenSim builds (see ISOPENSIM in the NSIS path);
; the Inno template always registers it for parity with OS builds.
Root: HKCR; Subkey: "hop"; ValueType: string; ValueName: ""; ValueData: "URL:Hypergrid"; Flags: uninsdeletekey
Root: HKCR; Subkey: "hop"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""
Root: HKCR; Subkey: "hop\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExe}"""
Root: HKCR; Subkey: "hop\shell\open"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "{#AppName}"
Root: HKCR; Subkey: "hop\shell\open\command"; ValueType: expandsz; ValueName: ""; ValueData: """{app}\{#AppExe}"" -url ""%1"""