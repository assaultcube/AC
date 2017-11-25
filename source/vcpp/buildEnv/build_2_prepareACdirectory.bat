@echo off
set ACDIR=AC
set ACDIRTESTING=AC_testing

echo make sure, that you compiled new binaries in %ACDIRTESTING% folder, tested them and generated fresh config\mapmodelattributes.cfg file (/loadallmapmodels)
pause

rem copy binaries
copy /Y %ACDIRTESTING%\bin_win32\ac_client.exe %ACDIR%\bin_win32\
copy /Y %ACDIRTESTING%\bin_win32\ac_server.exe %ACDIR%\bin_win32\

rem copy mapmodelattributes.cfg
copy /Y %ACDIRTESTING%\profile\config\mapmodelattributes.cfg %ACDIR%\config\

rem copy shadow files
set ACDIRMODELS_SHADOWS=%ACDIRTESTING%\profile\packages\models
copy /Y %ACDIRMODELS_SHADOWS%\misc\gib01\shadows.dat %ACDIR%\packages\models\misc\gib01\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\misc\gib02\shadows.dat %ACDIR%\packages\models\misc\gib02\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\misc\gib03\shadows.dat %ACDIR%\packages\models\misc\gib03\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\pickups\akimbo\shadows.dat %ACDIR%\packages\models\pickups\akimbo\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\pickups\ammobox\shadows.dat %ACDIR%\packages\models\pickups\ammobox\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\pickups\health\shadows.dat %ACDIR%\packages\models\pickups\health\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\pickups\helmet\shadows.dat %ACDIR%\packages\models\pickups\helmet\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\pickups\kevlar\shadows.dat %ACDIR%\packages\models\pickups\kevlar\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\pickups\nade\shadows.dat %ACDIR%\packages\models\pickups\nade\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\pickups\pistolclips\shadows.dat %ACDIR%\packages\models\pickups\pistolclips\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\playermodels\shadows.dat %ACDIR%\packages\models\playermodels\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\weapons\assault\world\shadows.dat %ACDIR%\packages\models\weapons\assault\world\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\weapons\carbine\world\shadows.dat %ACDIR%\packages\models\weapons\carbine\world\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\weapons\grenade\static\shadows.dat %ACDIR%\packages\models\weapons\grenade\static\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\weapons\grenade\world\shadows.dat %ACDIR%\packages\models\weapons\grenade\world\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\weapons\knife\world\shadows.dat %ACDIR%\packages\models\weapons\knife\world\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\weapons\pistol\world\shadows.dat %ACDIR%\packages\models\weapons\pistol\world\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\weapons\shotgun\world\shadows.dat %ACDIR%\packages\models\weapons\shotgun\world\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\weapons\sniper\world\shadows.dat %ACDIR%\packages\models\weapons\sniper\world\shadows.dat
copy /Y %ACDIRMODELS_SHADOWS%\weapons\subgun\world\shadows.dat %ACDIR%\packages\models\weapons\subgun\world\shadows.dat

rem delete stuff related to git and GitHub
rmdir /S /Q %ACDIR%\.git
del %ACDIR%\.gitattributes
del %ACDIR%\.travis.yml
for /r %ACDIR% %%i in (*) do if "%%~nxi"==".gitignore" del "%%i"

rem create config template
7z a -tzip -aoa %ACDIR%\config\configtemplates.zip %ACDIR%\config\autoexec.cfg %ACDIR%\config\favourites.cfg %ACDIR%\config\pcksources.cfg

rem remove source files (those are available in the source pkg)
rmdir /S /Q %ACDIR%\source

rem delete config and logs
del %ACDIR%\config\autoexec.cfg
del %ACDIR%\config\favourites.cfg
del %ACDIR%\config\pcksources.cfg
del %ACDIR%\config\init*.cfg
del %ACDIR%\config\saved*.cfg
del %ACDIR%\config\servervita*.cfg
del %ACDIR%\config\servers.cfg
del %ACDIR%\config\history
del %ACDIR%\clientlog*.txt

rem delete map files
del %ACDIR%\packages\maps\*.cgz
del %ACDIR%\packages\maps\*.cfg
del %ACDIR%\packages\maps\servermaps\incoming\*.cgz
del %ACDIR%\packages\maps\servermaps\incoming\*.cfg

rem purge screenshots
del /Q %ACDIR%\screenshots\*

rem purge demo directory, but leave tutorial demo, just in case of future use
for %%i in (%ACDIR%\demos\*) do if not "%%~nxi"=="tutorial_demo.dmo" del /Q "%%i"

rem remove linux stuff
for /r %ACDIR% %%i in (*.sh) do del "%%i"
rmdir /S /Q %ACDIR%\bin_unix

pause
