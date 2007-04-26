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

#include <SDL.h>
#include <SDL_image.h>

#define GL_GLEXT_LEGACY
#define __glext_h__
#define NO_SDL_GLEXT
#include <SDL_opengl.h>
#undef __glext_h__

#ifdef __APPLE__
#include "OpenGL/glext.h"
#else
#include "GL/glext.h"
#endif

#include <enet/enet.h>

#include <zlib.h>

#include "tools.h"
#include "geom.h"
#include "world.h"
#include "model.h"
#include "protocol.h"
#include "entity.h"
#include "sound.h"
#include "command.h"

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
extern int lastmillis;                  // last time
extern int curtime;                     // current frame time
extern int gamemode, nextmode;
extern int xtraverts;
extern bool minimap, reflecting, refracting;
extern bool demoplayback;
extern bool intermission;
extern int maxclients;
extern bool hasoverbright, hasmultitexture;

extern int VIRTW;                       // virtual screen size for text & HUD
#define VIRTH 1800
#define FONTH 64
#define PIXELTAB (VIRTW/12)

#include "protos.h"				// external function decls

#define AC_VERSION 920

#endif

