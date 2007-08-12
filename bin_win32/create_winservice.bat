@echo off

echo Adding AssaultCube Server to the Service Control Manager DB..
echo .

set path="%CD%\ac_server.exe -s -c4"
set util=%windir%\system32\sc.exe

%util% create assaultcubeserver binPath= %path% DisplayName= "AssaultCube Server"
%util% config assaultcubeserver binPath= %path%

pause