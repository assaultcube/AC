Name "Cube"

OutFile "cube_2005_08_22_setup.exe"

InstallDir $PROGRAMFILES\Cube

InstallDirRegKey HKLM "Software\Cube" "Install_Dir"

SetCompressor /SOLID lzma
XPStyle on

Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "Cube (required)"

  SectionIn RO
  
  SetOutPath $INSTDIR
  
  File /r "..\cube\*.*"
  
  WriteRegStr HKLM SOFTWARE\Cube "Install_Dir" "$INSTDIR"
  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Cube" "DisplayName" "Cube"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Cube" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Cube" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Cube" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
  
SectionEnd

Section "Start Menu Shortcuts"

  CreateDirectory "$SMPROGRAMS\Cube"
  CreateShortCut "$SMPROGRAMS\Cube\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\Cube\Cube.lnk" "$INSTDIR\cube.bat" "" "$INSTDIR\cube.bat" 0
  
SectionEnd

Section "Uninstall"
  
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Cube"
  DeleteRegKey HKLM SOFTWARE\Cube

  RMDir /r "$SMPROGRAMS\Cube"
  RMDir /r "$INSTDIR"

SectionEnd