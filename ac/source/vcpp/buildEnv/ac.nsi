!include "MUI.nsh"


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


; CONFIGURATION

AutoCloseWindow True
SetCompressor /SOLID lzma

!define CURPATH "R:\projects\ActionCube\ac\source\vcpp\buildEnv" ; CHANGE ME
!define AC_VERSION "v1.0"

Name "AssaultCube"
OutFile "AssaultCube_${AC_VERSION}.exe"
InstallDir "$PROGRAMFILES\AssaultCube"
InstallDirRegKey HKCU "Software\AssaultCube" ""


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

Section "AssaultCube (required)" SecDummy

  SectionIn RO

  SetOutPath "$INSTDIR"
  
  File /r ac\*.*
  
  WriteRegStr HKCU "Software\AssaultCube" "" $INSTDIR
  
  ; Create uninstaller
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AssaultCube" "DisplayName" "AssaultCube ${AC_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AssaultCube" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AssaultCube" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AssaultCube" "NoRepair" 1

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  StrCpy $0 "http://assault.cubers.net/releasenotes/v1.0/"
  Call openLinkNewWindow

SectionEnd

Section "Visual C++ redistributable runtime"

  SectionIn RO
  ExecWait '"$INSTDIR\bin\vcredist_x86.exe"'
  
SectionEnd

Section "OpenAL 1.1 redistributable"

  SectionIn RO
  ExecWait '"$INSTDIR\bin\oalinst.exe -s"'

SectionEnd

Section /o "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\AssaultCube"
  CreateShortCut "$SMPROGRAMS\AssaultCube\AssaultCube.lnk" "$INSTDIR\AssaultCube.bat" "" "$INSTDIR\icon.ico" 0 SW_SHOWMINIMIZED
  CreateShortCut "$SMPROGRAMS\AssaultCube\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\AssaultCube\README.lnk" "$INSTDIR\README.html" "" "" 0
  
SectionEnd

Section "Uninstall"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AssaultCube"
  DeleteRegKey HKLM SOFTWARE\AssaultCube

  RMDir /r "$SMPROGRAMS\AssaultCube"
  RMDir /r "$INSTDIR"

  DeleteRegKey /ifempty HKCU "Software\AssaultCube"

SectionEnd
