Tips for compiling AssaultCube in Visual Studio:

1. Install Visual C++ Redistributable for used version of Visual Studio.

2. In Visual Studio under "Configuration properties" -> "General" set "Project Toolset" to*:
a) in VS 2010 (last working version on Windows XP): Windows7.1SDK (Windows 7.1 SDK should be installed)
b) in VS 2012: v110_xp or Windows7.1SDK
c) in VS 2013: v120_xp or Windows7.1SDK
d) in VS 2015: v140_xp or Windows7.1SDK

3. In case of VS 2012 and newer versions under "Configuration Properties" -> "Linker" -> "Advanced" there shoud be set "Image has Safe Exception Handlers" to "No".

* support of compiled binaries on Windows XP/7/8/10
