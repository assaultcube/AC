@echo off
rem windows only.
rem automatically transforms the reference to a cubescript.
rem requires MSXSL, available at
rem http://www.microsoft.com/downloads/details.aspx?familyid=2fb55371-c94e-4373-b0e9-db4816552e41&displaylang=en
@echo on

msxsl.exe -o ..\..\config\docs.cfg -v ..\reference.xml ..\transformations\cuberef2cubescript.xslt
pause