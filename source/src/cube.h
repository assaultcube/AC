#ifndef __CUBE_H__
#define __CUBE_H__

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
#endif

#include <zlib.h>

#include <enet/enet.h>

#include "tools.h"
#include "geom.h"
#include "world.h"
#include "model.h"
#include "protocol.h"
#include "sound.h"
#include "entity.h"
#include "command.h"
#include "vote.h"
#include "console.h"

typedef vector<char *> cvector;
typedef vector<int> ivector;

// globals ooh naughty

extern sqr *world, *wmip[];             // map data, the mips are sequential 2D arrays in memory
extern header hdr;                      // current map header
extern int sfactor, ssize;              // ssize = 2^sfactor
extern int cubicsize, mipsize;          // cubicsize = ssize^2
extern physent *camera1;                // camera representing perspective of player, usually player1
extern playerent *player1;              // special client ent that receives input and acts as camera
extern vector<playerent *> players;     // all the other clients (in multiplayer)
extern vector<bounceent *> bounceents;
extern bool editmode;
extern vector<entity> ents;             // map entities
extern vec worldpos, camup, camright;   // current target of the crosshair in the world
extern vec hitpos;
extern int lastmillis, totalmillis;     // last time
extern int curtime;                     // current frame time
extern int gamemode, nextmode;
extern int gamespeed;
extern int xtraverts;
extern bool minimap, reflecting, refracting;
extern bool intermission;
extern int maxclients;
extern hashtable<char *, enet_uint32> mapinfo;
extern bool hasTE, hasMT, hasMDA;
extern int hwtexsize;

#include "protos.h"				// external function decls

#define AC_VERSION 940
#define AC_MASTER_URI "masterserver094.cubers.net/cgi-bin/actioncube.pl/" // FIXME, change DNS on ac release

#endif

