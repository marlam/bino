; Copyright (C) 2011, 2012
; Martin Lambers <marlam@marlam.de>
;
; Copying and distribution of this file, with or without modification, are
; permitted in any medium without royalty provided the copyright notice and this
; notice are preserved. This file is offered as-is, without any warranty.

!include "MUI.nsh"

; The name of the installer
Name "Bino"

; The file to write
OutFile "bino-${PACKAGE_VERSION}-w64.exe"

; Require Administrator privileges
RequestExecutionLevel admin

; The default installation directory
InstallDir "$PROGRAMFILES\Bino"

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\Bino" "Install_Dir"

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
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Bino"
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
  !insertmacro MUI_PAGE_STARTMENU Application $STARTMENU_FOLDER
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

; Languages: native first, others sorted by the language code (cs, de, ...)
  !insertmacro MUI_LANGUAGE "English"
  !insertmacro MUI_LANGUAGE "Bulgarian"
  !insertmacro MUI_LANGUAGE "Czech"
  !insertmacro MUI_LANGUAGE "German"
  !insertmacro MUI_LANGUAGE "French"
;  !insertmacro MUI_LANGUAGE "Polish"
  !insertmacro MUI_LANGUAGE "Russian"
  !insertmacro MUI_LANGUAGE "SimpChinese"

; Program file installation
Section "Bino Program" SecTools
  SetOutPath $INSTDIR\bin
  FILE bino.exe
  SetOutPath $INSTDIR\locale\bg\LC_MESSAGES
  FILE bg\LC_MESSAGES\bino.mo
  SetOutPath $INSTDIR\locale\cs\LC_MESSAGES
  FILE cs\LC_MESSAGES\bino.mo
  SetOutPath $INSTDIR\locale\de\LC_MESSAGES
  FILE de\LC_MESSAGES\bino.mo
  SetOutPath $INSTDIR\locale\fr\LC_MESSAGES
  FILE fr\LC_MESSAGES\bino.mo
;  SetOutPath $INSTDIR\locale\pl\LC_MESSAGES
;  FILE pl\LC_MESSAGES\bino.mo
  SetOutPath $INSTDIR\locale\ru\LC_MESSAGES
  FILE ru\LC_MESSAGES\bino.mo
  SetOutPath $INSTDIR\locale\zh_cn\LC_MESSAGES
  FILE zh_cn\LC_MESSAGES\bino.mo
  SetOutPath $INSTDIR\doc
  FILE doc\*.*
SectionEnd

; Last section is a hidden one.
Var MYTMP
Section
  SetShellVarContext all
  WriteUninstaller "$INSTDIR\uninstall.exe"
  ; Write the installation path into the registry
  WriteRegStr HKLM "Software\Bino" "Install_Dir" "$INSTDIR"
  ; Windows Add/Remove Programs support
  StrCpy $MYTMP "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bino"
  WriteRegStr       HKLM $MYTMP "DisplayName"     "Bino"
  WriteRegExpandStr HKLM $MYTMP "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegExpandStr HKLM $MYTMP "InstallLocation" "$INSTDIR"
  WriteRegStr       HKLM $MYTMP "DisplayVersion"  "${PACKAGE_VERSION}"
  WriteRegStr       HKLM $MYTMP "Publisher"       "The Bino developers"
  WriteRegStr       HKLM $MYTMP "URLInfoAbout"    "https://bino3d.org/"
  WriteRegStr       HKLM $MYTMP "HelpLink"        "https://bino3d.org/"
  WriteRegStr       HKLM $MYTMP "URLUpdateInfo"   "https://bino3d.org/"
  WriteRegDWORD     HKLM $MYTMP "NoModify"        "1"
  WriteRegDWORD     HKLM $MYTMP "NoRepair"        "1"
  ; Start menu
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
  CreateDirectory "$SMPROGRAMS\$STARTMENU_FOLDER"
  CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Bino.lnk" "$INSTDIR\bin\bino.exe"
  CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Manual.lnk" "$INSTDIR\doc\bino.html"
  WriteINIStr "$SMPROGRAMS\$STARTMENU_FOLDER\Website.url" "InternetShortcut" "URL" "https://bino3d.org/"
  CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  !insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

; Uninstaller
Section "Uninstall"
  SetShellVarContext all
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Bino"
  DeleteRegKey HKLM "Software\Bino"
  DeleteRegKey /ifempty HKCU "Software\Bino"
  ; Remove start menu entries
  !insertmacro MUI_STARTMENU_GETFOLDER Application $MUI_TEMP
  RMDir /r "$SMPROGRAMS\$MUI_TEMP"
  ; Remove files
  RMDir /r $INSTDIR
SectionEnd
