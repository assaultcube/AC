!include "MUI.nsh"
!include Sections.nsh

# Uses $0
Function openLinkNewWindow
  Push $3 
  Push $2
  Push $1
  Push $0
  ReadRegStr $0 HKCR "http\shell\open\command" ""
# Get browser path
    DetailPrint $0
  StrCpy $2 '"'
  StrCpy $1 $0 1
  StrCmp $1 $2 +2 # if path is not enclosed in " look for space as final char
    StrCpy $2 ' '
  StrCpy $3 1
  loop:
    StrCpy $1 $0 1 $3
    DetailPrint $1
    StrCmp $1 $2 found
    StrCmp $1 "" found
    IntOp $3 $3 + 1
    Goto loop
 
  found:
    StrCpy $1 $0 $3
    StrCmp $2 " " +2
      StrCpy $1 '$1"'
 
  Pop $0
  Exec '$1 $0'
  Pop $1
  Pop $2
  Pop $3
FunctionEnd

# Uses $0
Function un.openLinkNewWindow
  Push $3 
  Push $2
  Push $1
  Push $0
  ReadRegStr $0 HKCR "http\shell\open\command" ""
# Get browser path
    DetailPrint $0
  StrCpy $2 '"'
  StrCpy $1 $0 1
  StrCmp $1 $2 +2 # if path is not enclosed in " look for space as final char
    StrCpy $2 ' '
  StrCpy $3 1
  loop:
    StrCpy $1 $0 1 $3
    DetailPrint $1
    StrCmp $1 $2 found
    StrCmp $1 "" found
    IntOp $3 $3 + 1
    Goto loop
 
  found:
    StrCpy $1 $0 $3
    StrCmp $2 " " +2
      StrCpy $1 '$1"'
 
  Pop $0
  Exec '$1 $0'
  Pop $1
  Pop $2
  Pop $3
FunctionEnd


; CONFIGURATION

AutoCloseWindow True
SetCompressor /SOLID lzma

!define CURPATH "R:\projects\ActionCube\ac\source\vcpp\buildEnv" ; CHANGE ME
!define AC_VERSION "v1.0"
!define AC_SHORTNAME "AssaultCube"
!define AC_FULLNAME "AssaultCube v1.0"
!define AC_FULLNAMESAVE "AssaultCube_v1.0"

; general

Name "AssaultCube"
OutFile "AssaultCube_${AC_VERSION}.exe"
InstallDir "$PROGRAMFILES\${AC_FULLNAMESAVE}"
InstallDirRegKey HKCU "Software\${AC_FULLNAMESAVE}" ""
RequestExecutionLevel admin  ; require admin in vista

; Interface Configuration

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "${CURPATH}\header.bmp" ; optional

XPStyle on
Icon "${CURPATH}\icon.ico"
UninstallIcon "${CURPATH}\icon.ico"
!define MUI_ICON "${CURPATH}\icon.ico"
!define MUI_UNICON "${CURPATH}\icon.ico"


; Pages

!insertmacro MUI_PAGE_LICENSE "${CURPATH}\License.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
  

; Languages
 
!insertmacro MUI_LANGUAGE "English"


; Installer Sections

Section "AssaultCube v1.0" AC

  SectionIn RO

  SetOutPath "$INSTDIR"
  
  File /r ac\*.*
  
  WriteRegStr HKCU "Software\${AC_FULLNAMESAVE}" "" $INSTDIR
  
  ; Create uninstaller
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AC_FULLNAMESAVE}" "DisplayName" "${AC_FULLNAME}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AC_FULLNAMESAVE}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AC_FULLNAMESAVE}" "DisplayIcon" '"$INSTDIR\icon.ico"'

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AC_FULLNAMESAVE}" "HelpLink" "http://assault.cubers.net/help.html"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AC_FULLNAMESAVE}" "URLInfoAbout" "http://assault.cubers.net"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AC_FULLNAMESAVE}" "URLUpdateInfo" "http://assault.cubers.net/download.html"

  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AC_FULLNAMESAVE}" "DisplayVersion" "v1.0"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AC_FULLNAMESAVE}" "VersionMajor" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AC_FULLNAMESAVE}" "VersionMinor" 0

  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AC_FULLNAMESAVE}" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AC_FULLNAMESAVE}" "NoRepair" 1

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  StrCpy $0 "http://assault.cubers.net/releasenotes/v1.0/"
  Call openLinkNewWindow

SectionEnd

Section "Visual C++ redistributable runtime" VCPP

  SectionIn RO
  ExecWait '"$INSTDIR\bin\vcredist_x86.exe"'
  
SectionEnd

Section "OpenAL 1.1 redistributable" OAL

  SectionIn RO
  ExecWait '"$INSTDIR\bin\oalinst.exe -s"'

SectionEnd

Section "Multiuser Support (recommended)" MULTIUSER

  ; configures the .bat file to store configs in the appdata directory

  FileOpen $9 "$INSTDIR\AssaultCube.bat" w
  FileWrite $9 "bin_win32\ac_client.exe --home=$\"%appdata%\${AC_FULLNAMESAVE}$\" --init %1 %2 %3 %4 %5$\r$\n"
  FileWrite $9 "pause$\r$\n"
  FileClose $9

  ; link to it
  
  CreateShortCut "$INSTDIR\User Data.lnk" "%appdata%\${AC_FULLNAMESAVE}" "" "" 0

SectionEnd

Section "Start Menu Shortcuts" PROGSHORTCUTS

  SetShellVarContext all

  CreateDirectory "$SMPROGRAMS\${AC_SHORTNAME}"
  CreateShortCut "$SMPROGRAMS\${AC_SHORTNAME}\${AC_SHORTNAME}.lnk" "$INSTDIR\AssaultCube.bat" "" "$INSTDIR\icon.ico" 0 SW_SHOWMINIMIZED
  CreateShortCut "$SMPROGRAMS\${AC_SHORTNAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\${AC_SHORTNAME}\README.lnk" "$INSTDIR\README.html" "" "" 0

  ; create link to user settings dir if the multiuser-setting is selected

  SectionGetFlags ${Multiuser} $0
  IntOp $0 $0 & ${SF_SELECTED}
  IntCmp $0 ${SF_SELECTED} CreateUserSettingsShortCut SkipCreateUserSettingsShortCut

  CreateUserSettingsShortCut:
  CreateShortCut "$SMPROGRAMS\AssaultCube\AssaultCube User Data.lnk" "%appdata%\${AC_FULLNAMESAVE}" "" "" 0
  SkipCreateUserSettingsShortCut:
  
SectionEnd

Section "Desktop Shortcuts" DESKSHORTCUTS

  SetShellVarContext all

  CreateShortCut "$DESKTOP\${AC_SHORTNAME}.lnk" "$INSTDIR\AssaultCube.bat" "" "$INSTDIR\icon.ico" 0 SW_SHOWMINIMIZED
  
SectionEnd

Section "Uninstall"
  
  SetShellVarContext all
  
  ; delete reg keys
  
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AC_FULLNAMESAVE}"
  DeleteRegKey HKLM "SOFTWARE\${AC_FULLNAMESAVE}"
  DeleteRegKey /ifempty HKCU "SOFTWARE\${AC_FULLNAMESAVE}"

  ; delete directory
  
  RMDir /r "$INSTDIR"

  ; delete shortcuts

  RMDir /r "$SMPROGRAMS\${AC_SHORTNAME}"
  Delete "$DESKTOP\AssaultCube.lnk"
  
  StrCpy $0 "http://assault.cubers.net/uninstallnotes/v1.0/"
  Call un.openLinkNewWindow  

SectionEnd


; set descriptions

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN

  !insertmacro MUI_DESCRIPTION_TEXT ${AC} "Installs the required AssaultCube core files"
  !insertmacro MUI_DESCRIPTION_TEXT ${VCPP} "Installs the runtime to make AssaultCube run on your computer"
  !insertmacro MUI_DESCRIPTION_TEXT ${OAL} "Installs a sound library for 3D audio"
  !insertmacro MUI_DESCRIPTION_TEXT ${MULTIUSER} "Stores configuration and downloaded maps in %APPDATA%"
  !insertmacro MUI_DESCRIPTION_TEXT ${PROGSHORTCUTS} "Creates shortcuts in your Startmenu"
  !insertmacro MUI_DESCRIPTION_TEXT ${DESKSHORTCUTS} "Creates shortcuts on your Desktop"

!insertmacro MUI_FUNCTION_DESCRIPTION_END


