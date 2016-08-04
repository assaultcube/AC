@echo off
set acdir=ac
set workingacdir=..\..\..\

rem delete stuff related to git and GitHub
rmdir /S /Q %acdir%\.git
del %acdir%\.gitattributes
del %acdir%\.travis.yml
for /r %acdir% %%i in (*) do if "%%~nxi"==".gitignore" del "%%i"

rem delete config and logs
del %acdir%\config\init*.cfg
del %acdir%\config\saved*.cfg
del %acdir%\config\servervita*.cfg
del %acdir%\config\servers.cfg
del %acdir%\config\history
del %acdir%\clientlog*.txt

rem create config template
7z a -tzip -aoa %acdir%\config\configtemplates.zip .\%acdir%\config\autoexec.cfg .\%acdir%\config\favourites.cfg .\%acdir%\config\pcksources.cfg 
del %acdir%\config\autoexec.cfg
del %acdir%\config\favourites.cfg
del %acdir%\config\pcksources.cfg

rem delete map files
del %acdir%\packages\maps\*.cgz
del %acdir%\packages\maps\*.cfg
del %acdir%\packages\maps\servermaps\incoming\*.cgz
del %acdir%\packages\maps\servermaps\incoming\*.cfg

rem purge screenshots
del /Q %acdir%\screenshots\*

rem purge demo directory, but leave tutorial demo, just in case of future use
for %%i in (%acdir%\demos\*) do if not "%%~nxi"=="tutorial_demo.dmo" del /Q "%%i"

rem remove linux stuff
del %acdir%\*.sh
del %acdir%\config\*.sh
rmdir /S /Q %acdir%\bin_unix

rem remove source files (those are available in the source pkg)
rmdir /S /Q %acdir%\source

rem copy shadow files
copy /Y %workingacdir%\packages\models\misc\gib01\shadows.dat %acdir%\packages\models\misc\gib01\shadows.dat
copy /Y %workingacdir%\packages\models\misc\gib02\shadows.dat %acdir%\packages\models\misc\gib02\shadows.dat
copy /Y %workingacdir%\packages\models\misc\gib03\shadows.dat %acdir%\packages\models\misc\gib03\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\akimbo\shadows.dat %acdir%\packages\models\pickups\akimbo\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\ammobox\shadows.dat %acdir%\packages\models\pickups\ammobox\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\health\shadows.dat %acdir%\packages\models\pickups\health\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\helmet\shadows.dat %acdir%\packages\models\pickups\helmet\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\kevlar\shadows.dat %acdir%\packages\models\pickups\kevlar\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\nade\shadows.dat %acdir%\packages\models\pickups\nade\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\pistolclips\shadows.dat %acdir%\packages\models\pickups\pistolclips\shadows.dat
copy /Y %workingacdir%\packages\models\playermodels\shadows.dat %acdir%\packages\models\playermodels\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\assault\world\shadows.dat %acdir%\packages\models\weapons\assault\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\carbine\world\shadows.dat %acdir%\packages\models\weapons\carbine\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\grenade\static\shadows.dat %acdir%\packages\models\weapons\grenade\static\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\grenade\world\shadows.dat %acdir%\packages\models\weapons\grenade\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\knife\world\shadows.dat %acdir%\packages\models\weapons\knife\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\pistol\world\shadows.dat %acdir%\packages\models\weapons\pistol\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\shotgun\world\shadows.dat %acdir%\packages\models\weapons\shotgun\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\sniper\world\shadows.dat %acdir%\packages\models\weapons\sniper\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\subgun\world\shadows.dat %acdir%\packages\models\weapons\subgun\world\shadows.dat

pause
