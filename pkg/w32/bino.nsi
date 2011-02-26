; Copyright (C) 2011 Martin Lambers
;
; Copying and distribution of this file, with or without modification, are
; permitted in any medium without royalty provided the copyright notice and this
; notice are preserved. This file is offered as-is, without any warranty.

!include "MUI.nsh"

; The name of the installer
Name "bino ${PACKAGE_VERSION}"

; The file to write
OutFile "bino-${PACKAGE_VERSION}-w32.exe"

; The default installation directory
InstallDir "$PROGRAMFILES\bino-${PACKAGE_VERSION}"

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\bino-${PACKAGE_VERSION}" "Install_Dir"

;SetCompressor lzma
ShowInstDetails show

Var MUI_TEMP
Var STARTMENU_FOLDER

;Interface Settings

  !define MUI_ABORTWARNING
  !define MUI_ICON bino_logo.ico
  ;!define MUI_UNICON appicon.ico
 
; Pages

  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "COPYING"
  !insertmacro MUI_PAGE_LICENSE "notes.txt"
  !insertmacro MUI_PAGE_DIRECTORY
  ;Start Menu Folder Page Configuration
  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU" 
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\bino-${PACKAGE_VERSION}"
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
  !insertmacro MUI_PAGE_STARTMENU Application $STARTMENU_FOLDER
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

;Languages
 
  !insertmacro MUI_LANGUAGE "English"

Section "Bino Program" SecTools

  SetOutPath $INSTDIR\bin
  FILE bino.exe
  SetOutPath $INSTDIR\doc
  FILE bino.html
  FILE multi-display-vrlab.jpg
  FILE multi-display-rotated.jpg
  FILE gamma-pattern-tb.png
  FILE crosstalk-pattern-tb.png
  
SectionEnd

Var MYTMP

# Last section is a hidden one.
Section
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; Write the installation path into the registry
  WriteRegStr HKLM "Software\bino-${PACKAGE_VERSION}" "Install_Dir" "$INSTDIR"

  # Windows Add/Remove Programs support
  StrCpy $MYTMP "Software\Microsoft\Windows\CurrentVersion\Uninstall\bino-${PACKAGE_VERSION}"
  WriteRegStr       HKLM $MYTMP "DisplayName"     "bino ${PACKAGE_VERSION}"
  WriteRegExpandStr HKLM $MYTMP "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegExpandStr HKLM $MYTMP "InstallLocation" "$INSTDIR"
  WriteRegStr       HKLM $MYTMP "DisplayVersion"  "${PACKAGE_VERSION}"
  WriteRegStr       HKLM $MYTMP "Publisher"       "The Bino developers"
  WriteRegStr       HKLM $MYTMP "URLInfoAbout"    "http://bino.nongnu.org/"
  WriteRegStr       HKLM $MYTMP "HelpLink"        "http://bino.nongnu.org/"
  WriteRegStr       HKLM $MYTMP "URLUpdateInfo"   "http://bino.nongnu.org/"
  WriteRegDWORD     HKLM $MYTMP "NoModify"        "1"
  WriteRegDWORD     HKLM $MYTMP "NoRepair"        "1"

!insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    
;Create shortcuts
  CreateDirectory "$SMPROGRAMS\$STARTMENU_FOLDER"
  CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Bino.lnk" "$INSTDIR\bin\bino.exe"
  CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Manual.lnk" "$INSTDIR\doc\bino.html"
  WriteINIStr "$SMPROGRAMS\$STARTMENU_FOLDER\Website.url" "InternetShortcut" "URL" "http://bino.nongnu.org/"
  CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
!insertmacro MUI_STARTMENU_WRITE_END

SectionEnd

;Descriptions

  LangString DESC_SecTools ${LANG_ENGLISH} "Bino Program"

  !insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecTools}     $(DESC_SecTools)
  !insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------

; Uninstaller

Section "Uninstall"
  
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\bino-${PACKAGE_VERSION}"
  DeleteRegKey HKLM "Software\bino-${PACKAGE_VERSION}"
  ; Remove files
  Delete $INSTDIR\bin\bino.exe
  RMDir $INSTDIR\bin
  Delete $INSTDIR\doc\bino.html
  Delete $INSTDIR\doc\multi-display-vrlab.jpg
  Delete $INSTDIR\doc\multi-display-rotated.jpg
  Delete $INSTDIR\doc\gamma-pattern-tb.png
  Delete $INSTDIR\doc\crosstalk-pattern-tb.png
  RMDir $INSTDIR\doc
  ; Remove uninstaller
  Delete $INSTDIR\uninstall.exe
  ; Remove shortcuts, if any
  !insertmacro MUI_STARTMENU_GETFOLDER Application $MUI_TEMP
  Delete "$SMPROGRAMS\$MUI_TEMP\Bino.lnk"
  Delete "$SMPROGRAMS\$MUI_TEMP\Manual.lnk"
  Delete "$SMPROGRAMS\$MUI_TEMP\Website.url"
  Delete "$SMPROGRAMS\$MUI_TEMP\Uninstall.lnk"
  ;Delete empty start menu parent diretories
  StrCpy $MUI_TEMP "$SMPROGRAMS\$MUI_TEMP"
  startMenuDeleteLoop:
    ClearErrors
    RMDir $MUI_TEMP
    GetFullPathName $MUI_TEMP "$MUI_TEMP\.."
    IfErrors startMenuDeleteLoopDone
    StrCmp $MUI_TEMP $SMPROGRAMS startMenuDeleteLoopDone startMenuDeleteLoop
  startMenuDeleteLoopDone:
  DeleteRegKey /ifempty HKCU "Software\bino-${PACKAGE_VERSION}"
  ; Remove directories used
  RMDir "$INSTDIR"

SectionEnd
