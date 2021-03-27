#ifndef __CUBE_H__
#define __CUBE_H__

#include "platform.h"
#include "tools.h"
#include "geom.h"
#include "model.h"
#include "protocol.h"
#include "sound.h"
#include "weapon.h"
#include "entity.h"
#include "world.h"
#include "command.h"

#ifndef STANDALONE
 #include "varray.h"
 #include "vote.h"
 #include "console.h"
 enum
 {
   SDL_AC_BUTTON_WHEELDOWN = -5,
   SDL_AC_BUTTON_WHEELUP = -4,
   SDL_AC_BUTTON_RIGHT = -3,
   SDL_AC_BUTTON_MIDDLE = -2,
   SDL_AC_BUTTON_LEFT = -1,

   // touch layout for menu navigation
   TOUCH_MENU_LEFTSIDE = 10000,
   TOUCH_MENU_RIGHTSIDE_TOP = 10001,
   TOUCH_MENU_RIGHTSIDE_MIDDLE = 10002,
   TOUCH_MENU_RIGHTSIDE_BOTTOM = 10003,
   // touch layout for gameplay - ordering matters
   TOUCH_GAME_LEFTSIDE_TOP_CORNER = 10004,
   TOUCH_GAME_LEFTSIDE_TOPRIGHT = 10005,
   TOUCH_GAME_LEFTSIDE_TOP = 10006,
   TOUCH_GAME_LEFTSIDE_TOPLEFT = 10007,
   TOUCH_GAME_LEFTSIDE_LEFT = 10008,
   TOUCH_GAME_LEFTSIDE_BOTTOMLEFT = 10009,
   TOUCH_GAME_LEFTSIDE_BOTTOM = 10010,
   TOUCH_GAME_LEFTSIDE_BOTTOMRIGHT = 10011,
   TOUCH_GAME_LEFTSIDE_RIGHT = 10012,
   TOUCH_GAME_LEFTSIDE_OUTERCIRCLE = 10013,
   TOUCH_GAME_LEFTSIDE_DOUBLETAP = 10014,

   TOUCH_GAME_RIGHTSIDE_DOUBLETAP = 10020,
   TOUCH_GAME_RIGHTSIDE_TOP_0 = 10021,
   TOUCH_GAME_RIGHTSIDE_TOP_1 = 10022,
   TOUCH_GAME_RIGHTSIDE_TOP_2 = 10023,
 };
#endif

extern sqr *world, *wmip[];             // map data, the mips are sequential 2D arrays in memory
extern header hdr;                      // current map header
extern _mapconfigdata mapconfigdata;    // current mapconfig
extern int sfactor, ssize;              // ssize = 2^sfactor
extern int cubicsize, mipsize;          // cubicsize = ssize^2
extern physent *camera1;                // camera representing perspective of player, usually player1
extern playerent *player1;              // special client ent that receives input and acts as camera
extern vector<playerent *> players;     // all the other clients (in multiplayer)
extern vector<bounceent *> bounceents;
extern bool editmode;
extern int unsavededits;
extern vector<entity> ents;             // map entities
extern vec worldpos, camup, camright, camdir; // current target of the crosshair in the world
extern int lastmillis, totalmillis, skipmillis; // last time
extern int curtime;                     // current frame time
extern int interm;
extern int gamemode, nextmode;
extern int gamespeed;
extern int xtraverts;
extern float fovy, aspect;
extern int farplane;
extern bool minimap, reflecting, refracting;
extern int stenciling, stencilshadow, effective_stencilshadow;
extern bool intermission;
extern int arenaintermission;
extern hashtable<char *, enet_uint32> mapinfo;
extern int hwtexsize, hwmaxaniso;
extern int maploaded, msctrl;
extern float waterlevel;

#define AC_VERSION 1202

#ifdef __ANDROID__
#define AC_MASTER_URI "acmaster.centralus.cloudapp.azure.com"
#else
#define AC_MASTER_URI "ms.cubers.net"
#endif

#define AC_MASTER_PORT 28760
#define MAXCL 16
#define CONFIGROTATEMAX 5               // keep 5 old versions of saved.cfg and init.cfg around

#define DEFAULT_FOG 180
#define DEFAULT_FOGCOLOUR 0x8099B3
#define DEFAULT_SHADOWYAW 45

#include "protos.h"                     // external function decls

#endif

#ifdef MASTER


#include "protos.h"

#endif
