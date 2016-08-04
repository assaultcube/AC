rem add new release tag to master branch of AssaulCube and documentation before start of AC installer preparing
rem local branches should be compatible with remote
rem you need to have installed 7-Zip and Subversion and added 7z.exe and svn.exe to PATH
set acdir=ac
set acdircompiled=ac_compiled
if not exist "%acdir%\" (mkdir %acdir%) else (rmdir /S /Q %acdir% & mkdir %acdir%)
if not exist "%acdircompiled%\" (mkdir %acdircompiled%) else (rmdir /S /Q %acdircompiled% & mkdir %acdircompiled%)

rem get tag with name of latest AC version
git describe --tags --abbrev=0 > tmpFile
set /p latest= < tmpFile
del tmpFile

rem get AC files from latest local release tag
cd ..\..\..\
git archive --format zip --output source\vcpp\buildEnv\%acdir%\%latest%.zip %latest%
cd source\vcpp\buildEnv\%acdir%
7z x %latest%.zip
7z x %latest%.zip -o..\%acdircompiled%
del %latest%.zip

rem get documentation files from latest remote release tag
svn export --force https://github.com/assaultcube/assaultcube.github.io/tags/%latest%/htdocs/docs/ docs

pause
