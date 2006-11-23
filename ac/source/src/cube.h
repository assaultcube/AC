#ifndef __CUBE_H__
#define __CUBE_H__

/// one big bad include file for the whole engine... nasty!

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
#include "entity.h"
#include "protocol.h"
#include "sound.h"
#include "command.h"

/* Gamemodes
0	tdm
1	coop edit
2	dm
3	survivor
4	team survior
5	ctf
6	pistols
7	bot tdm
8	bot dm
9	last swiss standing
10	one shot, one kill
11  team one shot, one kill
12  bot one shot, one kill
*/

#define m_lms         (gamemode==3 || gamemode==4)
#define m_ctf	      (gamemode==5)
#define m_pistol      (gamemode==6)
#define m_lss		  (gamemode==9)
#define m_osok		  (gamemode>=10 && gamemode<=12)

#define m_noitems     (m_lms || m_osok)
#define m_noitemsnade (m_lss)
#define m_nopistol	  (m_osok || m_lss)
#define m_noprimary   (m_pistol || m_lss)
#define m_noguns	  (m_nopistol && m_noprimary)
#define m_arena       (m_lms || m_lss || m_osok)
#define m_teammode    (gamemode==0 || gamemode==4 || gamemode==5 || gamemode==7 || gamemode==11)
#define m_tarena      (m_arena && m_teammode)
#define m_botmode	  (gamemode==7 || gamemode == 8 || gamemode==12)
#define m_valid(mode) ((mode)>=0 && (mode)<=12)
#define m_mp(mode)    (m_valid(mode) && (mode)!=7 && (mode)!=8 && (mode)!=12)

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
extern int lastmillis;                  // last time
extern int curtime;                     // current frame time
extern int gamemode, nextmode;
extern int xtraverts;
extern bool demoplayback;
extern bool intermission;

#define VIRTW 2400                      // virtual screen size for text & HUD
#define VIRTH 1800
#define FONTH 64
#define PIXELTAB (VIRTW/12)

#include "protos.h"				// external function decls

#endif

