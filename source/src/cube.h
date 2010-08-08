#ifndef __CUBE_H__
#define __CUBE_H__

// to "trick" i18n/gettext
#define CC '\f'

#include "platform.h"
#include "tools.h"
#include "geom.h"
#include "model.h"
#include "protocol.h"
#include "sound.h"
#include "weapon.h"
#include "entity.h"
#include "world.h"
#include "i18n.h"
#include "command.h"

#ifndef STANDALONE
 #include "vote.h"
 #include "console.h"
#endif

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
extern vector<int> eh_ents;             // edithide entities
extern vec worldpos, camup, camright, camdir; // current target of the crosshair in the world
extern int lastmillis, totalmillis;     // last time
extern int curtime;                     // current frame time
extern int interm;
extern int gamemode, nextmode;
extern int gamespeed;
extern int xtraverts;
extern float fovy, aspect;
extern int farplane;
extern bool minimap, reflecting, refracting;
extern int stenciling, stencilshadow;
extern bool intermission;
extern int arenaintermission;
extern hashtable<char *, enet_uint32> mapinfo;
extern int hwtexsize;
extern int numspawn[3], maploaded, numflagspawn[2];
extern int verbose;

#include "protos.h"				// external function decls

#define AC_VERSION 1101
#define AC_MASTER_URI "assault.cubers.net"
#define AC_MASTER_PORT 28760
#define MAXCL 20

#endif

