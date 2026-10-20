; TalkBOX 1.0.0 — Inno Setup script
; Built by scripts/package.ps1; the version comes from CMakeLists.txt.
;
; Installs per-user by default (no UAC prompt): the app keeps everything in
; %LOCALAPPDATA%\TalkBOX, so a non-elevated install is consistent with the
; data model. Elevation is still available via the dialog for all users.

#define AppName "TalkBOX"
#define AppVersion GetEnv("TALKBOX_VERSION")
#define AppPublisher "TalkBOX"
#define AppExe "TalkBOX.exe"
; One brace is the Inno literal escape, so the effective AppId is
; {7E0F5B5C-3B1E-4F1B-9E3A-2B4C6D8E5A10}
#define AppId "{{7E0F5B5C-3B1E-4F1B-9E3A-2B4C6D8E5A10}"

[Setup]
AppId={#AppId}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\TalkBOX
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
; Per-user install: no elevation and no admin rights needed.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\dist
OutputBaseFilename=TalkBOX-{#AppVersion}-setup
SetupIconFile=..\assets\talkbox.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
; Keep the existing user data when upgrading; uninstalling never deletes
; %LOCALAPPDATA%\TalkBOX (the user's snippet library lives there).
CloseApplications=yes
RestartApplications=no
ChangesAssociations=no

[Languages]
Name: "chinesesimplified"; MessagesFile: "languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\dist\TalkBOX\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; The Qt deployment files are ours; the data directory is never touched.
Type: filesandordirs; Name: "{app}"

