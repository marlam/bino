; Copyright (C) 2011 Martin Lambers
;
; Copying and distribution of this file, with or without modification, are
; permitted in any medium without royalty provided the copyright notice and this
; notice are preserved. This file is offered as-is, without any warranty.

!include "MUI.nsh"

; The name of the installer
Name "Bino ${PACKAGE_VERSION}"

; The file to write
OutFile "bino-${PACKAGE_VERSION}-w32.exe"

; Require Administrator privileges
RequestExecutionLevel admin

; The default installation directory
InstallDir "$PROGRAMFILES\Bino ${PACKAGE_VERSION}"

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\Bino ${PACKAGE_VERSION}" "Install_Dir"

SetCompressor lzma
ShowInstDetails show

Var MUI_TEMP
Var STARTMENU_FOLDER

; Interface Settings
  !define MUI_ABORTWARNING
  !define MUI_ICON bino_logo.ico
  ;!define MUI_UNICON appicon.ico
 
; Installer Pages
  !insertmacro MUI_PAGE_WELCOME
  !insertmacro MUI_PAGE_LICENSE "COPYING"
  !insertmacro MUI_PAGE_LICENSE "notes.txt"
  !insertmacro MUI_PAGE_DIRECTORY
  ;Start Menu Folder Page Configuration
  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU" 
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Bino ${PACKAGE_VERSION}"
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
  !insertmacro MUI_PAGE_STARTMENU Application $STARTMENU_FOLDER
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

; Languages: native first, others sorted by the language code (cs, de, ...)
  !insertmacro MUI_LANGUAGE "English"
;  !insertmacro MUI_LANGUAGE "Czech"
  !insertmacro MUI_LANGUAGE "German"
  !insertmacro MUI_LANGUAGE "French"
  !insertmacro MUI_LANGUAGE "Polish"
  !insertmacro MUI_LANGUAGE "Russian"

; Program file installation
Section "Bino Program" SecTools
  SetOutPath $INSTDIR\bin
  FILE bino.exe
;  SetOutPath $INSTDIR\locale\cs\LC_MESSAGES
;  FILE cs\LC_MESSAGES\bino.mo
  SetOutPath $INSTDIR\locale\de\LC_MESSAGES
  FILE de\LC_MESSAGES\bino.mo
  SetOutPath $INSTDIR\locale\fr\LC_MESSAGES
  FILE fr\LC_MESSAGES\bino.mo
  SetOutPath $INSTDIR\locale\pl\LC_MESSAGES
  FILE pl\LC_MESSAGES\bino.mo
  SetOutPath $INSTDIR\locale\ru\LC_MESSAGES
  FILE ru\LC_MESSAGES\bino.mo
  SetOutPath $INSTDIR\doc
  FILE bino.html
  FILE multi-display-vrlab.jpg
  FILE multi-display-rotated.jpg
  FILE gamma-pattern-tb.png
  FILE crosstalk-pattern-tb.png
SectionEnd

; Last section is a hidden one.
Var MYTMP
Section
  SetShellVarContext all
  WriteUninstaller "$INSTDIR\uninstall.exe"
  ; Write the installation path into the registry
  WriteRegStr HKLM "Software\Bino ${PACKAGE_VERSION}" "Install_Dir" "$INSTDIR"
  ; Windows Add/Remove Programs support
  StrCpy $MYTMP "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bino ${PACKAGE_VERSION}"
  WriteRegStr       HKLM $MYTMP "DisplayName"     "Bino ${PACKAGE_VERSION}"
  WriteRegExpandStr HKLM $MYTMP "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegExpandStr HKLM $MYTMP "InstallLocation" "$INSTDIR"
  WriteRegStr       HKLM $MYTMP "DisplayVersion"  "${PACKAGE_VERSION}"
  WriteRegStr       HKLM $MYTMP "Publisher"       "The Bino developers"
  WriteRegStr       HKLM $MYTMP "URLInfoAbout"    "http://bino3d.org/"
  WriteRegStr       HKLM $MYTMP "HelpLink"        "http://bino3d.org/"
  WriteRegStr       HKLM $MYTMP "URLUpdateInfo"   "http://bino3d.org/"
  WriteRegDWORD     HKLM $MYTMP "NoModify"        "1"
  WriteRegDWORD     HKLM $MYTMP "NoRepair"        "1"
  ; Start menu
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
  CreateDirectory "$SMPROGRAMS\$STARTMENU_FOLDER"
  CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Bino.lnk" "$INSTDIR\bin\bino.exe"
  CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Manual.lnk" "$INSTDIR\doc\bino.html"
  WriteINIStr "$SMPROGRAMS\$STARTMENU_FOLDER\Website.url" "InternetShortcut" "URL" "http://bino3d.org/"
  CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  !insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

; Uninstaller
Section "Uninstall"
  SetShellVarContext all
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bino ${PACKAGE_VERSION}"
  DeleteRegKey HKLM "Software\Bino ${PACKAGE_VERSION}"
  DeleteRegKey /ifempty HKCU "Software\Bino ${PACKAGE_VERSION}"
  ; Remove start menu entries
  !insertmacro MUI_STARTMENU_GETFOLDER Application $MUI_TEMP
  RMDir /r "$SMPROGRAMS\$MUI_TEMP"
  ; Remove files
  RMDir /r $INSTDIR
SectionEnd
