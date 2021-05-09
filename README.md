# AssaultCube Mobile

## About

*AssaultCube Mobile* is a FREE, multiplayer, first-person shooter game for mobile devices. Taking place in realistic environments, with fast, arcade gameplay, it's addictive and fun! 

The game is strictly optimized for beginners so that there is a chance of surviving in Google Play Store.

<img src="/docs/screenshot2.jpeg" alt="alt text" width="45%"> <img src="/docs/screenshot.jpeg" alt="alt text" width="45%"> 

You can download the app directly from Google Play: 

<a href="https://play.google.com/store/apps/details?id=net.cubers.assaultcube"><img src="/docs/googleplay.png" alt="alt text" width="20%"></a>

## Features

- Play offline using three difficulty levels.
- Play online against other players in team combat.

## Supported Devices

- Mobile Devices running Android 7.0 (SDK 24 / OpenGL ES 3.2)

## Comparison to AssaultCube

There is a Windows, Linux and Mac version of AssaultCube. AssaultCube Mobile differs from AssaultCube in the following areas:

- There is no Mouse.. ;)
- There is an easy-to-use touch UI instead of the classical menu.
- Graphic effects are reduced.
- The weapon recoil is 50% lower.
- A total of [nine maps](https://github.com/assaultcube/AC/blob/63ba607f50c83dbfcc5bbaccd00e1a46521fd656/source/src/touch/config.h#L34) are supported.
- Singleplayer is limited to three difficulty levels (bot team-deathmatch 3vs3).
- Multiplayer is limited to official servers only (as of now).
- Multiplayer is limited to six clients per server.
- Multiplayer is limited to game modes TDM, CTF, TKTF, HTF. An exception to this is DM if there are less than 4 clients on the server.
- Multiplayer does not support playing against people on Windows, Linux or Mac.

## More info

Learn more at [AssaultCube Homepage](https://assault.cubers.net).

## Redistribution

You may redistribute AssaultCube in any way the license permits, such as the
free unmodified distribution of AssaultCube's source and binaries. If you have
any doubts, you can look at the
[license](https://assault.cubers.net/docs/license.html).

## Building from Source

### Windows 10

This guide assumes you are already familiar with NDK/C++ development on Android.

Toolchain:
- Install Android Studio 4.1.x 
- Open Android Studio, navigate to Tool > SDK Manager
- Navigate to tab SDK Platforms and install:
   - Android 11.0 (R) SDK
- Navigate to tab SDK Tools and install:
   - Android SDK Build-Tools 30.0.3
   - NDK 21.3.6528147
   - Android SDK Platform Tools 30.0.0
   - CMake 3.18.1

Dependencies:
- Build the following dependant libraries
   - GL4ES
   - SDL2
   - SDL2_Image
   - OpenAL Soft (with OBOE backend)
   - Ogg
   - Vorbis

Build:
- Checkout the source code of this branch to your working directory
- Copy the previously built dependant libraries to .\source\android\app\src\main\cpp\lib\
- Open a command prompt in .\source\android and execute copyassets.bat
- Start Android Studio and open the project at .\source\android\
- Navigate to Build > Make Project
- Navigate to Run > Run App

