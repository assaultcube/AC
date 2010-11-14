set exportDir=ac

rem purge old directory
del /S /Q %exportDir%

rem call build processes
call build_1_exportSVN.bat
call build_2_prepareACdirectory.bat
call build_3_createInstaller.bat

pause
