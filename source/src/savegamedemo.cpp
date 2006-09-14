// loading and saving of savegames & demos, dumps the spawn state of all mapents, the full state of all dynents (monsters + player)

#include "cube.h"

extern int islittleendian;

gzFile f = NULL;
bool demorecording = false;
bool demoplayback = false;
bool demoloading = false;
dvector playerhistory;
int democlientnum = 0;

void startdemo();

void gzput(int i) { gzputc(f, i); };
void gzputi(int i) { gzwrite(f, &i, sizeof(int)); };
void gzputv(vec &v) { gzwrite(f, &v, sizeof(vec)); };

void gzcheck(int a, int b) { if(a!=b) fatal("savegame file corrupt (short)"); };
int gzget() { char c = gzgetc(f); return c; };
int gzgeti() { int i; gzcheck(gzread(f, &i, sizeof(int)), sizeof(int)); return i; };
void gzgetv(vec &v) { gzcheck(gzread(f, &v, sizeof(vec)), sizeof(vec)); };

void stop()
{
    if(f)
    {
        if(demorecording) gzputi(-1);
        gzclose(f);
    };
    f = NULL;
    demorecording = false;
    demoplayback = false;
    demoloading = false;
    loopv(playerhistory) zapdynent(playerhistory[i]);
    playerhistory.setsize(0);
    // Added by Rick: Remove bots
    loopv(bots)
    {
          if (!bots[i]) continue;
          delete bots[i]->pBot;
          bots[i]->pBot = NULL;
          zapdynent(bots[i]);
    }
    bots.setsize(0);
};

void stopifrecording() { if(demorecording) stop(); };

void savestate(char *fn)
{
    stop();
    f = gzopen(fn, "wb9");
    if(!f) { conoutf("could not write %s", fn); return; };
    gzwrite(f, (void *)"CUBESAVE", 8);
    gzputc(f, islittleendian);  
    gzputi(SAVEGAMEVERSION);
    gzputi(sizeof(dynent));
    gzwrite(f, getclientmap(), _MAXDEFSTR);
    gzputi(gamemode);
    gzputi(ents.length());
    loopv(ents) gzputc(f, ents[i].spawned);
    gzwrite(f, player1, sizeof(dynent));
    gzputi(players.length());
    loopv(players)
    {
        gzput(players[i]==NULL);
        gzwrite(f, players[i], sizeof(dynent));
    };
};

void savegame(char *name)
{
   conoutf("can only save classic sp games"); 
   return; 
};

void loadstate(char *fn)
{
    stop();
    if(multiplayer()) return;
    f = gzopen(fn, "rb9");
    if(!f) { conoutf("could not open %s", fn); return; };
    
    string buf;
    gzread(f, buf, 8);
    if(strncmp(buf, "CUBESAVE", 8)) goto out;
    if(gzgetc(f)!=islittleendian) goto out;     // not supporting save->load accross incompatible architectures simpifies things a LOT
    if(gzgeti()!=SAVEGAMEVERSION || gzgeti()!=sizeof(dynent)) goto out;
    string mapname;
    gzread(f, mapname, _MAXDEFSTR);
    nextmode = gzgeti();
    changemap(mapname); // continue below once map has been loaded and client & server have updated 
    return;
    out:    
    conoutf("aborting: savegame/demo from a different version of cube or cpu architecture");
    stop();
};

void loadgame(char *name)
{
    sprintf_sd(fn)("savegames/%s.csgz", name);
    loadstate(fn);
};

void loadgameout()
{
    stop();
    conoutf("loadgame incomplete: savegame from a different version of this map");
};

void loadgamerest()
{
    if(demoplayback || !f) return;
        
    if(gzgeti()!=ents.length()) return loadgameout();
    loopv(ents)
    {
        ents[i].spawned = gzgetc(f)!=0;   
        if(ents[i].type==CARROT && !ents[i].spawned) trigger(ents[i].attr1, ents[i].attr2, true);
    };
    restoreserverstate(ents);
    
    gzread(f, player1, sizeof(dynent));
    player1->lastaction = lastmillis;
    
    int nplayers = gzgeti();
    loopi(nplayers) if(!gzget())
    {
        dynent *d = getclient(i);
        assert(d);
        gzread(f, d, sizeof(dynent));        
    };
    
    conoutf("savegame restored");
    if(demoloading) startdemo(); else stop();
};

// demo functions

int starttime = 0;
int playbacktime = 0;
int ddamage, bdamage;
vec dorig;

void record(char *name)
{
    int cn = getclientnum();
    if(cn<0) return;
    sprintf_sd(fn)("demos/%s.cdgz", name);
    savestate(fn);
    gzputi(cn);
    conoutf("started recording demo to %s", fn);
    demorecording = true;
    starttime = lastmillis;
	ddamage = bdamage = 0;
};

void demodamage(int damage, vec &o) { ddamage = damage; dorig = o; };
void demoblend(int damage) { bdamage = damage; };

void incomingdemodata(uchar *buf, int len, bool extras)
{
    if(!demorecording) return;
    gzputi(lastmillis-starttime);
    gzputi(len);
    gzwrite(f, buf, len);
    gzput(extras);
    if(extras)
    {
        gzput(player1->gunselect);
        gzput(player1->lastattackgun);
        gzputi(player1->lastaction-starttime);
        gzputi(player1->gunwait);
        gzputi(player1->health);
        gzputi(player1->armour);
        //gzput(player1->armourtype);
        loopi(NUMGUNS) gzput(player1->ammo[i]);
        gzput(player1->state);
		gzputi(bdamage);
		bdamage = 0;
		gzputi(ddamage);
		if(ddamage)	{ gzputv(dorig); ddamage = 0; };
        // FIXME: add all other client state which is not send through the network
    };
};

void demo(char *name)
{
    sprintf_sd(fn)("demos/%s.cdgz", name);
    loadstate(fn);
    demoloading = true;
};

void stopreset()
{
    conoutf("demo stopped (%d msec elapsed)", lastmillis-starttime);
    stop();
    loopv(players) zapdynent(players[i]);
    disconnect(0, 0);
};

VAR(demoplaybackspeed, 10, 100, 1000);
int scaletime(int t) { return (int)(t*(100.0f/demoplaybackspeed))+starttime; };

void readdemotime()
{   
    if(gzeof(f) || (playbacktime = gzgeti())==-1)
    {
        stopreset();
        return;
    };
    playbacktime = scaletime(playbacktime);
};

void startdemo()
{
    democlientnum = gzgeti();
    demoplayback = true;
    starttime = lastmillis;
    conoutf("now playing demo");
    dynent *d = getclient(democlientnum);
    assert(d);
    *d = *player1;
    readdemotime();
};

VAR(demodelaymsec, 0, 120, 500);

void catmulrom(vec &z, vec &a, vec &b, vec &c, float s, vec &dest)		// spline interpolation
{
	vec t1 = b, t2 = c;

	vsub(t1, z); vmul(t1, 0.5f)
	vsub(t2, a); vmul(t2, 0.5f);

	float s2 = s*s;
	float s3 = s*s2;

	dest = a;
	vec t = b;

	vmul(dest, 2*s3 - 3*s2 + 1);
	vmul(t,   -2*s3 + 3*s2);     vadd(dest, t);
    vmul(t1,     s3 - 2*s2 + s); vadd(dest, t1);
	vmul(t2,     s3 -   s2);     vadd(dest, t2);
};

void fixwrap(dynent *a, dynent *b)
{
	while(b->yaw-a->yaw>180)  a->yaw += 360;  
	while(b->yaw-a->yaw<-180) a->yaw -= 360;
};

void demoplaybackstep()
{
    while(demoplayback && lastmillis>=playbacktime)
    {
        int len = gzgeti();
        if(len<1 || len>MAXTRANS)
        {
            conoutf("error: huge packet during demo play (%d)", len);
            stopreset();
            return;
        };
        uchar buf[MAXTRANS];
        gzread(f, buf, len);
        localservertoclient(buf, len);  // update game state
        
        dynent *target = players[democlientnum];
        assert(target); 
        
		int extras;
        if(extras = gzget())     // read additional client side state not present in normal network stream
        {
            target->gunselect = gzget();
            target->lastattackgun = gzget();
            target->lastaction = scaletime(gzgeti());
            target->gunwait = gzgeti();
            target->health = gzgeti();
            target->armour = gzgeti();
            //target->armourtype = gzget();
            loopi(NUMGUNS) target->ammo[i] = gzget();
            target->state = gzget();
            target->lastmove = playbacktime;
			if(bdamage = gzgeti()) damageblend(bdamage);
			if(ddamage = gzgeti()) { gzgetv(dorig); particle_splash(3, ddamage, 1000, dorig); };
            // FIXME: set more client state here
        };
        
        // insert latest copy of player into history
        if(extras && (playerhistory.empty() || playerhistory.last()->lastupdate!=playbacktime))
        {
            dynent *d = newdynent();
            *d = *target;
            d->lastupdate = playbacktime;
            playerhistory.add(d);
            if(playerhistory.length()>20)
            {
                zapdynent(playerhistory[0]);
                playerhistory.remove(0);
            };
        };
        
        readdemotime();
    };
    
    if(demoplayback)
    {
        int itime = lastmillis-demodelaymsec;
        loopvrev(playerhistory) if(playerhistory[i]->lastupdate<itime)      // find 2 positions in history that surround interpolation time point
        {
            dynent *a = playerhistory[i];
            dynent *b = a;
            if(i+1<playerhistory.length()) b = playerhistory[i+1];
            *player1 = *b;
            if(a!=b)                                // interpolate pos & angles
            {
				dynent *c = b;
				if(i+2<playerhistory.length()) c = playerhistory[i+2];
				dynent *z = a;
				if(i-1>=0) z = playerhistory[i-1];
				//if(a==z || b==c) printf("* %d\n", lastmillis);
				float bf = (itime-a->lastupdate)/(float)(b->lastupdate-a->lastupdate);
				fixwrap(a, player1);
				fixwrap(c, player1);
				fixwrap(z, player1);
				vdist(dist, v, z->o, c->o);
				if(dist<16)		// if teleport or spawn, dont't interpolate
				{
					catmulrom(z->o, a->o, b->o, c->o, bf, player1->o);
					catmulrom(*(vec *)&z->yaw, *(vec *)&a->yaw, *(vec *)&b->yaw, *(vec *)&c->yaw, bf, *(vec *)&player1->yaw);
				};
				fixplayer1range();
			};
            break;
        };
        //if(player1->state!=CS_DEAD) showscores(false);
    };
};

void stopn() { if(demoplayback) stopreset(); else stop(); conoutf("demo stopped"); };

// sorry :/
/*COMMAND(record, ARG_1STR);
COMMAND(demo, ARG_1STR);
COMMANDN(stop, stopn, ARG_NONE);*/

COMMAND(savegame, ARG_1STR);
COMMAND(loadgame, ARG_1STR);
