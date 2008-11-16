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
rmdir /S /Q %acdir%\bin_unix

rem remove source files (those are available in the source pkg)
rmdir /S /Q %acdir%\source

rem remove doc tools
rmdir /S /Q %acdir%\docs\autogen

rem remove unused resources
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\barbwire
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\bush1
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\ceilingfan
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\grid6x8
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\guardtower
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\milkcrate
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\palmtree1
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\pine
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\pipes\concretepipe
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\pipes\metalpipe
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\pipes\metalpipe2
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\razorwire
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\servercluster\rack
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\servercluster\U3_1
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\servercluster\U3_2
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\servercluster\U3_3
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\servercluster\U6_1
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\servercluster\U6_2
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\servercluster\U6_3
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\shelf
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\smallpalm
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\stairs_aqueous\stairs1
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\stairs_aqueous\stairs2
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\stairs_aqueous\stairsacc\beam
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\stairs_aqueous\stairsacc\handrail
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\tree1
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\tree2
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\treetrunk1
rmdir /S /Q %acdir%\packages\models\mapmodels\toca\window1


rem copy shadow files
copy /Y %workingacdir%\packages\models\playermodels\shadows.dat %acdir%\packages\models\playermodels\shadows.dat
copy /Y %workingacdir%\packages\models\misc\gib01\shadows.dat %acdir%\packages\models\misc\gib01\shadows.dat
copy /Y %workingacdir%packages\models\misc\gib02\shadows.dat %acdir%\packages\models\misc\gib02\shadows.dat
copy /Y %workingacdir%\packages\models\misc\gib03\shadows.dat %acdir%\packages\models\misc\gib03\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\akimbo\shadows.dat %acdir%\packages\models\pickups\akimbo\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\ammobox\shadows.dat %acdir%\packages\models\pickups\ammobox\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\health\shadows.dat %acdir%\packages\models\pickups\health\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\kevlar\shadows.dat %acdir%\packages\models\pickups\kevlar\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\nades\shadows.dat %acdir%\packages\models\pickups\nades\shadows.dat
copy /Y %workingacdir%\packages\models\pickups\pistolclips\shadows.dat %acdir%\packages\pickups\pistolclips\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\assault\world\shadows.dat %acdir%\packages\models\weapons\assault\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\grenade\static\shadows.dat %acdir%\packages\models\weapons\grenade\static\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\grenade\world\shadows.dat %acdir%\packages\models\weapons\grenade\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\knife\world\shadows.dat %acdir%\packages\models\weapons\knife\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\pistol\world\shadows.dat %acdir%\packages\models\weapons\pistol\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\shotgun\world\shadows.dat %acdir%\packages\models\weapons\shotgun\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\sniper\world\shadows.dat %acdir%\packages\models\weapons\sniper\world\shadows.dat
copy /Y %workingacdir%\packages\models\weapons\subgun\world\shadows.dat %acdir%\packages\models\weapons\subgun\world\shadows.dat

pause