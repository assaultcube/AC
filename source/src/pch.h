#ifdef __GNUC__
#define gamma __gamma
#endif

#include <math.h>

#ifdef __GNUC__
#undef gamma
#endif

#include <string.h>
#ifdef WIN32
    #define strcasecmp(a,b) _stricmp(a,b)
#endif
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#ifdef __GNUC__
#include <new>
#else
#include <new.h>
#endif

#ifdef WIN32
    #define WIN32_LEAN_AND_MEAN
    #include "windows.h"
    #define _WINDOWS
    #define ZLIB_DLL
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
#else
#include "SDL_endian.h"
#endif

#include <zlib.h>
#include <enet/enet.h>

#include "AL/al.h" 
#include "AL/alc.h" 
#include "AL/alut.h"
#include "vorbis/vorbisfile.h"

