#ifndef MyAppVersion
  #error Define MyAppVersion from CMakeLists.txt.
#endif

#define MyAppName "媒体小工具"
#define MyAppDirName "VideoTools"
#define MyAppExe "MediaTool.exe"
#define MyAppId "{{7C2A6E14-9B35-4F28-A1D6-5E0B8C4F2A91}"

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=MediaTool
DefaultDirName={localappdata}\Programs\{#MyAppDirName}
DisableWelcomePage=yes
DisableDirPage=no
DefaultGroupName={#MyAppDirName}
DisableProgramGroupPage=yes
DisableReadyPage=yes
DisableFinishedPage=yes
OutputDir=..\artifacts
OutputBaseFilename=MediaTool-Setup-{#MyAppVersion}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=lowest
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExe}

[Files]
Source: "..\publish\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExe}"
