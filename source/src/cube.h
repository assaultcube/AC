#ifndef __CUBE_H__
#define __CUBE_H__

#include "tools.h"
#include "geom.h"
#include "world.h"
#include "model.h"
#include "protocol.h"
#include "sound.h"
#include "weapon.h"
#include "entity.h"
#include "command.h"
#include "vote.h"

#ifndef STANDALONE
#include "console.h"
#endif

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
extern vec worldpos, camup, camright, camdir; // current target of the crosshair in the world
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

