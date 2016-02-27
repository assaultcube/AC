#ifdef __GNUC__
    #ifdef _FORTIFY_SOURCE
        #undef _FORTIFY_SOURCE
    #endif
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <locale.h>
#include <stdint.h>
#ifdef __GNUC__
    #include <new>
    #include <signal.h>
#else
    #include <new.h>
#endif

#include <zlib.h>
#include <enet/enet.h>

#ifdef WIN32
    #define WIN32_LEAN_AND_MEAN
    #include "windows.h"
    #ifndef _WINDOWS
      #define _WINDOWS
    #endif
    #include <tlhelp32.h>
    #ifndef __GNUC__
        #include <Dbghelp.h>
    #endif
    #define ZLIB_DLL
    #define NO_POSIX_R
#endif

#ifndef STANDALONE
    #include <SDL.h>
    #include <SDL_image.h>

    #define GL_GLEXT_LEGACY
    #define __glext_h__
    #define NO_SDL_GLEXT
    #include <SDL_opengl.h>
    #undef __glext_h__

    #include "GL/glext.h"

    #ifdef __APPLE__
        #include "OpenAL/al.h"
        #include "OpenAL/alc.h"
        #include "Vorbis/vorbisfile.h"
        #define MOD_KEYS_CTRL (KMOD_LMETA|KMOD_RMETA)
    #else
        #include "AL/al.h"
        #include "AL/alc.h"
        #include "vorbis/vorbisfile.h"
        #define MOD_KEYS_CTRL (KMOD_LCTRL|KMOD_RCTRL)
    #endif

    #include <setjmp.h>
#endif

#if defined(WIN32) || defined(__APPLE__) || !defined(STANDALONE) || defined(AC_FORCE_SDL_THREADS)
    #define AC_USE_SDL_THREADS
#endif

