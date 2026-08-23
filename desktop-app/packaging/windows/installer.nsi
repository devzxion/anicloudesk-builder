Unicode true
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "StrFunc.nsh"
${StrLoc}

!define PRODUCT_NAME "AniCloud"
!define PRODUCT_VERSION "4.1.1"
!define PRODUCT_ID "ink.anicloud.desktop"

Name "${PRODUCT_NAME}"
OutFile "${OUTPUT_FILE}"
InstallDir "$PROGRAMFILES64\AniCloud"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

!define MUI_ICON "${ICON_FILE}"
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function EnsureAniCloudClosed
ensure_anicloud_closed_retry:
  ; In-app updates ask the running process to quit before this installer gets
  ; here. Also handle manually launched installers while the tray process is
  ; active, then verify that Windows actually released the executable.
  Sleep 700
  nsExec::ExecToStack '"$SYSDIR\tasklist.exe" /FI "IMAGENAME eq AniCloud.exe" /FO CSV /NH'
  Pop $0
  Pop $1
  ${StrLoc} $2 $1 '"AniCloud.exe"' ">"
  StrCmp $2 "" ensure_anicloud_closed_done

  ; Never use /T here: an older AniCloud release may have launched this
  ; installer as its child, and tree termination would kill the installer too.
  nsExec::ExecToStack '"$SYSDIR\taskkill.exe" /IM AniCloud.exe /F'
  Pop $0
  Pop $1
  Sleep 1200
  nsExec::ExecToStack '"$SYSDIR\tasklist.exe" /FI "IMAGENAME eq AniCloud.exe" /FO CSV /NH'
  Pop $0
  Pop $1
  ${StrLoc} $2 $1 '"AniCloud.exe"' ">"
  StrCmp $2 "" ensure_anicloud_closed_done

  IfSilent ensure_anicloud_closed_abort
  MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION \
    "AniCloud is still running in the background and could not be closed.$\r$\n$\r$\nClose AniCloud from Task Manager or its tray icon, then choose Retry." \
    IDRETRY ensure_anicloud_closed_retry
ensure_anicloud_closed_abort:
  SetErrorLevel 5
  Abort
ensure_anicloud_closed_done:
FunctionEnd

Function .onInit
  Call EnsureAniCloudClosed
FunctionEnd

Section "AniCloud" SEC_MAIN
  SetShellVarContext all
  ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_ID}" "UninstallString"
  StrCmp $0 "" check_default_install
  ExecWait '$0 /S'
  Goto legacy_removed
check_default_install:
  IfFileExists "$INSTDIR\Uninstall AniCloud.exe" 0 legacy_removed
  ExecWait '"$INSTDIR\Uninstall AniCloud.exe" /S _?=$INSTDIR'
legacy_removed:
  ; Clear only the old program directory. Electron user data and downloaded
  ; files live outside Program Files and are intentionally never touched.
  RMDir /r "$INSTDIR"
  SetOutPath "$INSTDIR"
  File /r "${STAGE_DIR}\*"
  WriteUninstaller "$INSTDIR\Uninstall AniCloud.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_ID}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_ID}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_ID}" "UninstallString" '"$INSTDIR\Uninstall AniCloud.exe"'
  WriteRegStr HKCR "anicloud" "" "URL:AniCloud Protocol"
  WriteRegStr HKCR "anicloud" "URL Protocol" ""
  WriteRegStr HKCR "anicloud\shell\open\command" "" '"$INSTDIR\AniCloud.exe" "%1"'
  ; Startup is privacy- and security-sensitive. Clear the legacy automatic
  ; registration; users can explicitly opt in from Profile after installation.
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "AniCloud"
  CreateDirectory "$SMPROGRAMS\AniCloud"
  CreateShortcut "$SMPROGRAMS\AniCloud\AniCloud.lnk" "$INSTDIR\AniCloud.exe" "" "$INSTDIR\AniCloud.exe"
  CreateShortcut "$DESKTOP\AniCloud.lnk" "$INSTDIR\AniCloud.exe" "" "$INSTDIR\AniCloud.exe"
SectionEnd

Section "Uninstall"
  SetShellVarContext all
  Delete "$DESKTOP\AniCloud.lnk"
  Delete "$SMPROGRAMS\AniCloud\AniCloud.lnk"
  RMDir "$SMPROGRAMS\AniCloud"
  DeleteRegKey HKCR "anicloud"
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "AniCloud"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_ID}"
  RMDir /r "$INSTDIR"
SectionEnd
