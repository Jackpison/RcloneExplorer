; Rclone Explorer setup for Windows, built by the Update workflow with Inno Setup 6.
;   ISCC.exe /DMyVersion=5.4.2 /DSource=<folder with Rclone Explorer.exe> /O<out dir> installer.iss
; Installs for the current user only (no administrator rights), into
; %LOCALAPPDATA%\Programs\Rclone Explorer. Without "Rclone Explorer.ini" the
; app runs in normal mode: settings live in %APPDATA%, so updates never touch
; them, and an uninstall leaves them in place.

#ifndef MyVersion
  #error Pass /DMyVersion=x.y.z
#endif
#ifndef Source
  #error Pass /DSource=<folder>
#endif

[Setup]
AppId={{36FF6A8B-39E2-49C1-8A42-DE716F7CB67D}
AppName=Rclone Explorer
AppVersion={#MyVersion}
AppVerName=Rclone Explorer {#MyVersion}
AppPublisher=Jackpison
AppPublisherURL=https://jackpison.github.io/RcloneExplorer/
AppSupportURL=https://github.com/Jackpison/RcloneExplorer/issues
AppUpdatesURL=https://github.com/Jackpison/RcloneExplorer/releases
VersionInfoVersion={#MyVersion}
DefaultDirName={autopf}\Rclone Explorer
DefaultGroupName=Rclone Explorer
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
LicenseFile={#Source}\docs\LICENSE
SetupIconFile=..\..\src\icon.ico
UninstallDisplayIcon={app}\Rclone Explorer.exe
UninstallDisplayName=Rclone Explorer
OutputBaseFilename=RcloneExplorer-{#MyVersion}-windows-x64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
; an update closes the running app (the app quits by itself first)
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; everything from the portable folder except its ini (which would switch on
; portable mode)
Source: "{#Source}\*"; DestDir: "{app}"; Excludes: "Rclone Explorer.ini"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Rclone Explorer"; Filename: "{app}\Rclone Explorer.exe"
Name: "{autodesktop}\Rclone Explorer"; Filename: "{app}\Rclone Explorer.exe"; Tasks: desktopicon

[Run]
; also after a silent update from inside the app, so it comes back by itself
Filename: "{app}\Rclone Explorer.exe"; Description: "{cm:LaunchProgram,Rclone Explorer}"; Flags: nowait postinstall; Check: Launch

[Code]
// "/NOLAUNCH" on the command line: do not start the app afterwards (the
// release workflow's test install)
function Launch: Boolean;
begin
  Result := Pos('/NOLAUNCH', UpperCase(GetCmdTail)) = 0;
end;

[UninstallDelete]
; files an in-place update left behind
Type: files; Name: "{app}\*.old"
Type: files; Name: "{app}\update-leftovers.txt"
