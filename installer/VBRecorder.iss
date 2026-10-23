; VB Recorder - installer, for Inno Setup 7.
;
; One installer for 64-bit and 32-bit Windows: it puts in the build that
; matches the system. It is made from the two release folders (what is inside
; VBRecorder-x64.zip and VBRecorder-x86.zip):
;
;   ISCC /DX64=<x64 folder> /DX86=<x86 folder> installer\VBRecorder.iss
;
; /DOutputDir=<folder> picks where VBRecorder-Setup.exe goes (default: dist).
;
; It installs for the current user, with no administrator rights, into
; C:\VBRecorder by default. The program can then update itself there as the
; portable copy does, and keeps its "Installed apps" version up to date
; (src/install.cpp). The AppId below must stay the same as VB_APP_ID there.

#ifndef X64
  #error Pass the 64-bit folder with /DX64=...
#endif
#ifndef X86
  #error Pass the 32-bit folder with /DX86=...
#endif
#ifndef OutputDir
  #define OutputDir "..\dist"
#endif
#ifndef AppVersion
  ; "1.4.5.0" from the executable becomes "1.4.5", as the program shows it.
  #define FullVersion GetVersionNumbersString(AddBackslash(X64) + "VBRecorder.exe")
  #if FullVersion == ""
    #error VBRecorder.exe was not found in the 64-bit folder
  #endif
  #define AppVersion Copy(FullVersion, 1, RPos(".", FullVersion) - 1)
#endif

[Setup]
AppId={{97ED0908-C5B3-45CF-9B21-19663A845A5E}
AppName=VB Recorder
AppVersion={#AppVersion}
AppPublisher=Vitor Bruski
AppPublisherURL=https://github.com/vrivrixd/vbrecorder
AppSupportURL=https://github.com/vrivrixd/vbrecorder/issues
AppUpdatesURL=https://github.com/vrivrixd/vbrecorder/releases
AppCopyright=© 2026 Vitor Bruski
VersionInfoVersion={#AppVersion}
VersionInfoCompany=Vitor Bruski
VersionInfoProductName=VB Recorder
VersionInfoDescription=VB Recorder Setup
; Without the version in the name: the program updates the version shown.
UninstallDisplayName=VB Recorder
UninstallDisplayIcon={app}\VBRecorder.exe
DefaultDirName=C:\VBRecorder
; A portable copy may already be in C:\VBRecorder; installing over it is fine.
DirExistsWarning=no
; The Start menu folder is always "VB Recorder"; a task decides whether it is
; made at all.
DefaultGroupName=VB Recorder
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
; The program's own single-instance mutex: Setup asks to close it first.
AppMutex=VBRecorder_Mutex
WizardStyle=modern
; The welcome page, with only Next and Cancel, which Inno Setup hides by default.
DisableWelcomePage=no
ShowLanguageDialog=yes
OutputDir={#OutputDir}
OutputBaseFilename=VBRecorder-Setup
Compression=lzma2/max
SolidCompression=yes

; The languages of the program, and only those. English comes first: it is
; used when the system's language is none of them, as in the program. The
; Brazilian Portuguese one is our own: the one Inno Setup ships is too informal.
[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ar"; MessagesFile: "compiler:Languages\Arabic.isl"
Name: "bg"; MessagesFile: "compiler:Languages\Bulgarian.isl"
Name: "bn"; MessagesFile: "Languages\Bengali.isl"
Name: "cs"; MessagesFile: "compiler:Languages\Czech.isl"
Name: "da"; MessagesFile: "compiler:Languages\Danish.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "eo"; MessagesFile: "Languages\Esperanto.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "fi"; MessagesFile: "compiler:Languages\Finnish.isl"
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "gl"; MessagesFile: "Languages\Galician.isl"
Name: "hi"; MessagesFile: "Languages\Hindi.isl"
Name: "id"; MessagesFile: "Languages\Indonesian.isl"
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "ja"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "ko"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "pl"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "pt_BR"; MessagesFile: "Languages\BrazilianPortuguese.isl"
Name: "pt_PT"; MessagesFile: "compiler:Languages\Portuguese.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "sk"; MessagesFile: "compiler:Languages\Slovak.isl"
Name: "sl"; MessagesFile: "compiler:Languages\Slovenian.isl"
Name: "th"; MessagesFile: "compiler:Languages\Thai.isl"
Name: "tr"; MessagesFile: "compiler:Languages\Turkish.isl"
Name: "vi"; MessagesFile: "Languages\Vietnamese.isl"
Name: "zh_CN"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "zh_TW"; MessagesFile: "compiler:Languages\ChineseTraditional.isl"

[CustomMessages]
en.StartMenuIcons=Create Start Menu shortcuts
ar.StartMenuIcons=إنشاء اختصارات في قائمة ابدأ
bg.StartMenuIcons=Създаване на преки пътища в менюто „Старт“
bn.StartMenuIcons=স্টার্ট মেনুতে শর্টকাট তৈরি করুন
cs.StartMenuIcons=Vytvořit zástupce v nabídce Start
da.StartMenuIcons=Opret genveje i menuen Start
de.StartMenuIcons=Verknüpfungen im Startmenü erstellen
eo.StartMenuIcons=Krei ŝparvojojn en la Startmenuo
es.StartMenuIcons=Crear accesos directos en el menú Inicio
fi.StartMenuIcons=Luo pikakuvakkeet Käynnistä-valikkoon
fr.StartMenuIcons=Créer des raccourcis dans le menu Démarrer
gl.StartMenuIcons=Crear atallos no menú Inicio
hi.StartMenuIcons=स्टार्ट मेनू में शॉर्टकट बनाएँ
id.StartMenuIcons=Buat pintasan di menu Start
it.StartMenuIcons=Crea collegamenti nel menu Start
ja.StartMenuIcons=スタート メニューにショートカットを作成する
ko.StartMenuIcons=시작 메뉴에 바로 가기 만들기
pl.StartMenuIcons=Utwórz skróty w menu Start
pt_BR.StartMenuIcons=Criar atalhos no menu Iniciar
pt_PT.StartMenuIcons=Criar atalhos no menu Iniciar
ru.StartMenuIcons=Создать ярлыки в меню «Пуск»
sk.StartMenuIcons=Vytvoriť odkazy v ponuke Štart
sl.StartMenuIcons=Ustvari bližnjice v meniju Start
th.StartMenuIcons=สร้างทางลัดในเมนูเริ่ม
tr.StartMenuIcons=Başlat menüsünde kısayollar oluştur
vi.StartMenuIcons=Tạo lối tắt trong menu Start
zh_CN.StartMenuIcons=在“开始”菜单中创建快捷方式
zh_TW.StartMenuIcons=在 [開始] 功能表中建立捷徑

[Tasks]
Name: "startmenu"; Description: "{cm:StartMenuIcons}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#X64}\VBRecorder.exe"; DestDir: "{app}"; Check: Is64BitInstallMode; Flags: ignoreversion
Source: "{#X64}\lib\*.dll"; DestDir: "{app}\lib"; Check: Is64BitInstallMode; Flags: ignoreversion
Source: "{#X86}\VBRecorder.exe"; DestDir: "{app}"; Check: not Is64BitInstallMode; Flags: ignoreversion
Source: "{#X86}\lib\*.dll"; DestDir: "{app}\lib"; Check: not Is64BitInstallMode; Flags: ignoreversion
Source: "{#X64}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\VB Recorder"; Filename: "{app}\VBRecorder.exe"; WorkingDir: "{app}"; Tasks: startmenu
Name: "{group}\{cm:UninstallProgram,VB Recorder}"; Filename: "{uninstallexe}"; Tasks: startmenu
Name: "{autodesktop}\VB Recorder"; Filename: "{app}\VBRecorder.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\VBRecorder.exe"; WorkingDir: "{app}"; Description: "{cm:LaunchProgram,VB Recorder}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; What the program's own updates leave or add: files set aside, and DLLs that
; a later version may bring. The settings (VBRecorder.ini) and the
; recordings stay.
Type: files; Name: "{app}\*.old"
Type: filesandordirs; Name: "{app}\lib"

[Code]
const
  RunKey = 'Software\Microsoft\Windows\CurrentVersion\Run';
  ApprovedKey = 'Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run';
  RunValue = 'VB Recorder';

{ Without administrator rights, a folder such as Program Files cannot be
  written to. Better to say so on the folder page, where another can be
  picked, than to fail halfway through the copy. A folder made only for the
  test is removed again. }
function CanWriteTo(Dir: String): Boolean;
var
  Existed: Boolean;
  Test: String;
begin
  Existed := DirExists(Dir);
  Result := Existed or ForceDirectories(Dir);
  if Result then
  begin
    Test := AddBackslash(Dir) + 'vbrecorder-write-test.tmp';
    Result := SaveStringToFile(Test, '', False);
    DeleteFile(Test);
  end;
  if not Existed then
    RemoveDir(Dir);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectDir) and not CanWriteTo(WizardDirValue) then
  begin
    MsgBox(FmtMessage(SetupMessage(msgErrorCreatingDir), [WizardDirValue]), mbError, MB_OK);
    Result := False;
  end;
end;

{ "Start with Windows" is turned on in the program, not here. On uninstall,
  the entry goes away, but only if it starts this copy: a portable one
  elsewhere keeps its own. }
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Command: String;
begin
  if CurUninstallStep <> usUninstall then
    Exit;
  if RegQueryStringValue(HKCU, RunKey, RunValue, Command) and
     (Pos(Lowercase(AddBackslash(ExpandConstant('{app}'))), Lowercase(Command)) > 0) then
  begin
    RegDeleteValue(HKCU, RunKey, RunValue);
    RegDeleteValue(HKCU, ApprovedKey, RunValue);
  end;
end;
