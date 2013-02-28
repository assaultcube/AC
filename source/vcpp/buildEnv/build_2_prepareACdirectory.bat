@echo off
set acdir=ac
set workingacdir=..\..\..\

rem delete config
del %acdir%\config\saved.cfg
echo "" > %acdir%\config\servers.cfg

rem purge screenshots
del /Q %acdir%\screenshots\*

rem purge demo directory
del /Q %acdir%\demos\*

rem remove linux stuff
del %acdir%\*.sh
del %acdir%\config\*.sh
rmdir /S /Q %acdir%\bin_unix

rem remove source files (those are available in the source pkg)
rmdir /S /Q %acdir%\source

rem copy shadow files
copy /Y %workingacdir%\packages\models\playermodels\shadows.dat %acdir%\packages\models\playermodels\shadows.dat
copy /Y %workingacdir%\packages\models\misc\gib01\shadows.dat %acdir%\packages\models\misc\gib01\shadows.dat
copy /Y %workingacdir%packages\models\misc\gib02\shadows.dat %acdir%\packages\models\misc\gib02\shadows.dat
copy /Y %workingacdir%\packages\models\misc\gib03\shadows.dat %acdir%\packages\models\misc\gib03\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\akimbo\shadows.dat %acdir%\packages\models\pickups\akimbo\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\ammobox\shadows.dat %acdir%\packages\models\pickups\ammobox\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\health\shadows.dat %acdir%\packages\models\pickups\health\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\helmet\shadows.dat %acdir%\packages\models\pickups\helmet\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\kevlar\shadows.dat %acdir%\packages\models\pickups\kevlar\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\nade\shadows.dat %acdir%\packages\models\pickups\nade\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\pistolclips\shadows.dat %acdir%\packages\models\pickups\pistolclips\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\assault\world\shadows.dat %acdir%\packages\models\weapons\assault\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\grenade\static\shadows.dat %acdir%\packages\models\weapons\grenade\static\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\grenade\world\shadows.dat %acdir%\packages\models\weapons\grenade\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\knife\world\shadows.dat %acdir%\packages\models\weapons\knife\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\pistol\world\shadows.dat %acdir%\packages\models\weapons\pistol\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\carbine\world\shadows.dat %acdir%\packages\models\weapons\carbine\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\shotgun\world\shadows.dat %acdir%\packages\models\weapons\shotgun\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\sniper\world\shadows.dat %acdir%\packages\models\weapons\sniper\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\subgun\world\shadows.dat %acdir%\packages\models\weapons\subgun\world\shadows.dat

pause
