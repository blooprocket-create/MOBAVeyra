; Veyra Setup (ADR-022 §2): installs the Veyra launcher for the current Windows user, with no
; administrator prompt, then opens it; the launcher installs the game. Launcher/Package.ps1 compiles
; this with NSIS 3's makensis and passes:
;
;   VERSION       the launcher's version, such as 0.1.0
;   VERSION_QUAD  the same as four numbers, such as 0.1.0.0, for Windows' file properties
;   PAYLOAD       a folder holding veyra-launcher.exe and VeyraLauncher.json
;   ART           a folder holding welcome.bmp and header.bmp, which veyra-setup-art draws
;   ICON          the launcher's icon, which Launcher/app/build.rs draws
;   OUTFILE       where Setup goes
;
; /S installs with no pages, for the launcher's self-updater (ADR-005 L2, ADR-022 §11), which starts
; Setup with /S /RELAUNCH and closes: Setup waits for the launcher's file to be free, replaces it, then
; opens the new launcher. The uninstaller removes only Setup's own files; it asks the launcher to remove
; the game (`--uninstall-game`), which deletes only what the launcher installed. A silent uninstall
; keeps the game.

Unicode true
ManifestDPIAware true
SetCompressor /SOLID lzma
RequestExecutionLevel user

!ifndef VERSION | VERSION_QUAD | PAYLOAD | ART | ICON | OUTFILE
  !error "Compile Setup with Launcher/Package.ps1, which defines VERSION, VERSION_QUAD, PAYLOAD, ART, ICON and OUTFILE."
!endif

!define PRODUCT "Veyra"
!define PUBLISHER "Wayfinder Studios"
!define LAUNCHER_EXE "veyra-launcher.exe"
!define CONFIG_FILE "VeyraLauncher.json"
!define UNINSTALLER "Uninstall Veyra.exe"
!define SHORTCUT "Veyra.lnk"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Veyra"
; The launcher's web view keeps its data under its Tauri identifier (Launcher/app/tauri.conf.json).
!define LAUNCHER_DATA "$LOCALAPPDATA\com.wayfinder.veyra.launcher"
; The WebView2 runtime's registration, machine-wide and per user. Setup is a 32-bit program, so the
; machine-wide key is read through the 32-bit view Microsoft documents it in.
!define WEBVIEW2_KEY "SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"
!define WEBVIEW2_USER_KEY "Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}"
!define WEBVIEW2_PAGE "https://developer.microsoft.com/microsoft-edge/webview2/"

; How long Setup waits for a launcher that is closing to let go of its file, as it does when it
; updates itself: this many tries, this many milliseconds apart (30 seconds in all).
!define LAUNCHER_WAIT_TRIES 120
!define LAUNCHER_WAIT_MS 250

; The launcher's palette (Launcher/ui/launcher.css): ink behind, its text on it.
!define INK "07090E"
!define TEXT "ECEEF2"

Name "${PRODUCT}"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\${PRODUCT}"
; Running Setup again upgrades the launcher where it already is.
InstallDirRegKey HKCU "${UNINSTALL_KEY}" "InstallLocation"
BrandingText "${PRODUCT} ${VERSION}"

VIProductVersion "${VERSION_QUAD}"
VIFileVersion "${VERSION_QUAD}"
VIAddVersionKey "ProductName" "${PRODUCT}"
VIAddVersionKey "CompanyName" "${PUBLISHER}"
VIAddVersionKey "FileDescription" "${PRODUCT} Setup"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "LegalCopyright" "© 2026 ${PUBLISHER}"

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"

!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_BGCOLOR "${INK}"
!define MUI_TEXTCOLOR "${TEXT}"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${ART}\welcome.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "${ART}\welcome.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "${ART}\header.bmp"
!define MUI_HEADERIMAGE_UNBITMAP "${ART}\header.bmp"
!define MUI_ABORTWARNING

!define MUI_WELCOMEPAGE_TITLE "Welcome to Veyra"
!define MUI_WELCOMEPAGE_TEXT "Setup installs the Veyra launcher for you on this computer. You don't need to be an administrator.$\r$\n$\r$\nThe launcher then installs the game, keeps it up to date and signs you in.$\r$\n$\r$\nClick Next to continue."
!insertmacro MUI_PAGE_WELCOME

!define MUI_DIRECTORYPAGE_TEXT_TOP "Setup will install the Veyra launcher in this folder. The launcher asks where to put the game itself."
!insertmacro MUI_PAGE_DIRECTORY

!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_TITLE "The launcher is ready"
!define MUI_FINISHPAGE_TEXT "Open Veyra to install the game and play."
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Open Veyra now"
!define MUI_FINISHPAGE_RUN_FUNCTION OpenLauncher
!define MUI_FINISHPAGE_SHOWREADME
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Add a desktop shortcut"
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION AddDesktopShortcut
!define MUI_PAGE_CUSTOMFUNCTION_SHOW ShowFinishPage
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Function .onInit
  SetShellVarContext current
  ; The launcher's window needs WebView2, which Windows 11 includes.
  ReadRegStr $0 HKLM "${WEBVIEW2_KEY}" "pv"
  ${If} $0 == ""
  ${OrIf} $0 == "0.0.0.0"
    ReadRegStr $0 HKCU "${WEBVIEW2_USER_KEY}" "pv"
  ${EndIf}
  ${If} $0 == ""
  ${OrIf} $0 == "0.0.0.0"
    MessageBox MB_YESNO|MB_ICONINFORMATION "The Veyra launcher needs Microsoft's WebView2 Runtime, which this computer does not have yet.$\r$\n$\r$\nOpen Microsoft's download page? Setup carries on either way." /SD IDNO IDNO +2
      ExecShell "open" "${WEBVIEW2_PAGE}"
  ${EndIf}
FunctionEnd

Section "Veyra launcher"
  SetOutPath "$INSTDIR"
  SetOverwrite on
  ; A launcher updating itself has just started Setup and is closing: wait until its file can be
  ; written, which it cannot while the launcher runs.
  ${If} ${FileExists} "$INSTDIR\${LAUNCHER_EXE}"
    StrCpy $1 0
    ${Do}
      ClearErrors
      FileOpen $0 "$INSTDIR\${LAUNCHER_EXE}" a
      ${IfNot} ${Errors}
        FileClose $0
        ${Break}
      ${EndIf}
      IntOp $1 $1 + 1
      ${If} $1 >= ${LAUNCHER_WAIT_TRIES}
        MessageBox MB_OK|MB_ICONEXCLAMATION "The Veyra launcher is still open. Close it, then run Setup again." /SD IDOK
        Abort
      ${EndIf}
      Sleep ${LAUNCHER_WAIT_MS}
    ${Loop}
  ${EndIf}
  File "${PAYLOAD}\${LAUNCHER_EXE}"
  File "${PAYLOAD}\${CONFIG_FILE}"
  WriteUninstaller "$INSTDIR\${UNINSTALLER}"
  CreateShortcut "$SMPROGRAMS\${SHORTCUT}" "$INSTDIR\${LAUNCHER_EXE}"

  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "${PRODUCT}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\${LAUNCHER_EXE}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\${UNINSTALLER}"'
  WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\${UNINSTALLER}" /S'
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Function OpenLauncher
  SetOutPath "$INSTDIR"
  Exec '"$INSTDIR\${LAUNCHER_EXE}"'
FunctionEnd

Function .onInstSuccess
  ; The launcher that updated itself (/S /RELAUNCH) opens again, now the new one. With pages, the
  ; finish page offers to open it instead.
  ${If} ${Silent}
    ${GetParameters} $0
    ClearErrors
    ${GetOptions} $0 "/RELAUNCH" $1
    ${IfNot} ${Errors}
      Call OpenLauncher
    ${EndIf}
  ${EndIf}
FunctionEnd

Function AddDesktopShortcut
  CreateShortcut "$DESKTOP\${SHORTCUT}" "$INSTDIR\${LAUNCHER_EXE}"
FunctionEnd

Function ShowFinishPage
  ; Themed check boxes ignore their text colour, which would leave their labels dark on the dark
  ; page; without a theme they take the palette.
  System::Call 'uxtheme::SetWindowTheme(p $mui.FinishPage.Run, w " ", w " ")'
  System::Call 'uxtheme::SetWindowTheme(p $mui.FinishPage.ShowReadme, w " ", w " ")'
  SetCtlColors $mui.FinishPage.Run "${TEXT}" "${INK}"
  SetCtlColors $mui.FinishPage.ShowReadme "${TEXT}" "${INK}"
FunctionEnd

Function un.onInit
  SetShellVarContext current
FunctionEnd

Section "Uninstall"
  ; The game first, while the launcher that installed it is still here.
  ${If} ${FileExists} "$INSTDIR\${LAUNCHER_EXE}"
    StrCpy $0 "keep"
    MessageBox MB_YESNO|MB_ICONQUESTION "Remove Veyra's game files as well?$\r$\n$\r$\nChoose No to keep them, for example to install the launcher again later." /SD IDNO IDNO +2
      StrCpy $0 "remove"
    ${If} $0 == "remove"
      DetailPrint "Removing Veyra's game files"
      ExecWait '"$INSTDIR\${LAUNCHER_EXE}" --uninstall-game' $1
      ${If} $1 != 0
        MessageBox MB_OK|MB_ICONEXCLAMATION "Some of Veyra's game files could not be removed. Close Veyra if it is running, then remove the rest from the game's folder." /SD IDOK
      ${EndIf}
    ${EndIf}
  ${EndIf}

  ; Only Setup's own files: the folder goes only if nothing else is left in it.
  Delete "$INSTDIR\${LAUNCHER_EXE}"
  Delete "$INSTDIR\${CONFIG_FILE}"
  Delete "$INSTDIR\${UNINSTALLER}"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\${SHORTCUT}"
  Delete "$DESKTOP\${SHORTCUT}"
  RMDir /r "${LAUNCHER_DATA}"
  DeleteRegKey HKCU "${UNINSTALL_KEY}"
SectionEnd
