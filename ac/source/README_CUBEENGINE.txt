Cube source code license, usage, and documentation.

You should read this file IN ITS ENTIRETY if you wish to do anything with
the cube source code, even a mere build. Own builds are not necessarily
compatible with the officially released binaries, read below.

You may use the cube source code if you abide by the ZLIB license
http://www.opensource.org/licenses/zlib-license.php
(very similar to the BSD license) with the additional clause below:


LICENSE
=======

Cube game engine source code, 20 dec 2003 release.

Copyright (C) 2001-2003 Wouter van Oortmerssen.

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

additional clause specific to Cube:

4. Source versions may not be "relicensed" under a different license
   without my explicitly written permission.


LICENSE NOTES
=============
The license covers the source code found in the "src" directory of this
archive, the included enet network library which cube uses is covered by
the "No problem Bugroff" license, which is however compatible with the
above license for all practical purposes.

Game media included in the cube game (maps, textures, sounds, models etc.)
are not covered by this license, and may have individual copyrights and
distribution restrictions (see individual readmes).


USAGE
=====
Compiling the sources should be straight forward.

Unix users need to make sure to have the development version of all libs
installed (OpenGL, SDL, SDL_Mixer, SDL_Image, zlib, libpng). The included
makefiles can be used to build.

Windows users can use the included visual studio .net 2003 project files, which
references the lib/include directories for the external libraries and should
thus be self contained. The project is set up assuming you have the normal
cube binary distribution as a subdirectory "cube" of the root of this archive,
so release mode builds will place executables in the bin dir ready for testing
and distribution. Do not come ask me for help in compiling or modifying the
sources, if you can't figure out how to do this yourself you probably shouldn't
be touching the files anyway.

The cube sources are very small, compact, and non redundant, so anyone
wishing to modify the source code should be able to gain an overview of
cube's inner workings by simply reading through the source code in its
entirety. Small amounts of comments should guide you through the more
tricky sections.

When reading the source code and trying to understand cube's internal design,
keep in mind the goal of cube: minimalism. I wanted to create a very complete
game / game engine with absolutely minimal means, and made a sport out of it
keeping the implementation small and simple. Cube is not a commercial product,
it is merely the author's idea of a fun little programming project.


OPEN SOURCE
===========
Cube is open source (see ZLIB license above). This only means that you have
great freedom using it for your own projects, but does NOT mean the main cube
code is an "open source project" in the sense that everyone is invited to
contribute to it. The main cube code will remain a one man project (me), as my
minimalistic design is highly incompatible with the open source philosophy. If you
add to the cube source code, you fork the code and it becomes your own project,
do not ask for me to integrate your changes into the main branch, no matter
how brilliant they are.


CHEATING
========
If you want to use cube as a base for a game where the multiplayer aspect is
important and used by a large community, you need to be aware that cube's
thick client - thin server architecture is extremely cheat sensitive. If you
release a cube based game with source code equivalent to the binaries, some
minor changes can give anyone an aimbot or other cheats in online games.
There are several ways to make this less easy, some of which are:

1. only distribute binaries (the ZLIB license allows this). Executables can still
   be hacked, but unless you have a really large online community, noone will
   probably bother.
2. write a network proxy, such as qizmo used with QuakeWorld (whose sources are
   also open source). The proxy is a small closed source program that checksums
   the executable, and maybe also some game media. You can then make servers
   or game admins request this information and ban cheaters.
3. release the sources with an incompatible network protocol or other changes
   compared to the binaries you release.
4. build serious cheat detection into the game. Since all clients are their own
   "servers", you can make them all keep track of stats for the other players,
   such as health etc. you can make all sorts of consistency checks on shots,
   movement speed and items. If the discrepancy for a certain client becomes
   too big, all clients but the cheater can send their "vote" to the server
   for having him banned. Even better, you can add server side stat checking.

For the cube's own game I chose option 3, i.e. you can only play the official
cube game using the binaries supplied by me, and you can't compile your own clients
for multiplayer use (you can still make custom clients that work with matching
custom servers, or play cube single player maps compatible with the real thing).
This situation is not ideal, but there is no easy way around it.

This scheme is probably very easy to defeat if you are capable of using disassemblers
and packet sniffers, but please contrain yourself and don't ruin the fun of the
cube multiplayer community. Thanks.


AUTHOR
======
Wouter van Oortmerssen aka Aardappel
wvo@fov120.com
http://wouter.fov120.com

For additional authors/contributors, see the cube binary distribution readme.html
