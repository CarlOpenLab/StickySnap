; StickySnap Windows 安装脚本（Qt portable 版）
; CI 用法：iscc /DAppVersion=x.y.z packaging\windows\StickySnap.iss
; 期望 working directory 为仓库根目录，Qt 部署产物位于 build-qt\deploy\

#ifndef AppVersion
#define AppVersion "0.0.1"
#endif

[Setup]
AppId={{7C2B6A90-4E1D-4E8F-9A3B-2F6FED000001}
AppName=StickySnap
AppVersion={#AppVersion}
AppPublisher=CarlOpenLab
AppPublisherURL=https://carlopenlab.github.io/StickySnap/
DefaultDirName={localappdata}\Programs\StickySnap
DefaultGroupName=StickySnap
; 按用户安装（无需管理员权限），贴图工具不需要全机器部署。
PrivilegesRequired=lowest
SetupIconFile=res\app.ico
UninstallDisplayIcon={app}\StickySnap.exe
OutputDir=build-qt
OutputBaseFilename=StickySnap-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式(&D)"; GroupDescription: "附加任务："
Name: "autostart"; Description: "开机自动运行 StickySnap(&A)"; GroupDescription: "附加任务："

[Files]
Source: "build-qt\deploy\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\StickySnap"; Filename: "{app}\StickySnap.exe"
Name: "{group}\卸载 StickySnap"; Filename: "{uninstallexe}"
Name: "{userdesktop}\StickySnap"; Filename: "{app}\StickySnap.exe"; Tasks: desktopicon

[Registry]
; 勾选「开机自启」任务时写当前用户 Run 键（与设置对话框同一项，覆盖式同步）。
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
  ValueType: string; ValueName: "StickySnap"; ValueData: """{app}\StickySnap.exe"""; \
  Tasks: autostart; Flags: uninsdeletevalue

[Run]
Filename: "{app}\StickySnap.exe"; Description: "立即运行 StickySnap(&R)"; \
  Flags: nowait postinstall skipifsilent

[UninstallDelete]
; 用户数据（config.ini / index.txt / pins）保留，不做破坏性清理。
