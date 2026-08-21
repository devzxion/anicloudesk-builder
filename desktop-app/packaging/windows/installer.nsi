Unicode true
!include "MUI2.nsh"
!include "FileFunc.nsh"

!define PRODUCT_NAME "AniCloud"
!define PRODUCT_VERSION "4.0.2"
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
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "AniCloud" '"$INSTDIR\AniCloud.exe" --background'
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
