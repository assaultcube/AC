IF EXIST "%programfiles%\NSIS\makensis.exe" (
	"%programfiles%\NSIS\makensis.exe" ac.nsi
) ELSE (
	"%programfiles(x86)%\NSIS\makensis.exe" ac.nsi
)
pause
