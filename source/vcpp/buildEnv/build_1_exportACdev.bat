rem add new release tag to master branch of AssaulCube and documentation before start of AC installer preparing
rem local branches should be compatible with remote
rem you need to have installed 7-Zip and Subversion and added 7z.exe and svn.exe to PATH
set ACDIR=AC
set ACDIRTESTING=AC_testing
if not exist "%ACDIR%\" (mkdir %ACDIR%) else (rmdir /S /Q %ACDIR% & mkdir %ACDIR%)
if not exist "%ACDIRTESTING%\" (mkdir %ACDIRTESTING%) else (rmdir /S /Q %ACDIRTESTING% & mkdir %ACDIRTESTING%)

rem get tag with name of latest AC version
git describe --tags --abbrev=0 > tmpFile
set /p NEWACTAG= < tmpFile
del tmpFile

rem get AC files from latest local release tag
cd ..\..\..\
git archive --format zip --output source\vcpp\buildEnv\%ACDIR%\%NEWACTAG%.zip %NEWACTAG%
cd source\vcpp\buildEnv\%ACDIR%
7z x %NEWACTAG%.zip
7z x %NEWACTAG%.zip -o..\%ACDIRTESTING%
del ..\%ACDIRTESTING%\assaultcube.bat
del %NEWACTAG%.zip

rem get documentation files from latest remote release tag
svn export --force https://github.com/assaultcube/assaultcube.github.io/tags/%NEWACTAG%/htdocs/docs/ docs

pause
