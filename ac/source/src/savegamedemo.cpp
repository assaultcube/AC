// loading and saving of savegames & demos, dumps the spawn state of all mapents, the full state of all dynents (monsters + player)

#include "cube.h"

enum { // demo file blocktypes (uchar)
      SD_NULL = 0, SD_INIT1, SD_INIT2, SD_INIT3, SD_NETDATA = 3, SD_EXTRAS = 100, SD_PLAYER1, SD_EOF = 255
     };

gzFile f = NULL;
bool demorecording = false;
bool demoplayback = false;
bool demoloading = false;
vector<playerent *> playerhistory;
int democlientnum = 0;
int starttime = 0;

static uchar *iobuf = (uchar *)calloc(1, 65536);      // 64k should be enough for anybody
static ucharbuf dp(iobuf, 65535);

void startdemo();
void stopreset();

void gzcheck(int a, int b) { if(a!=b) fatal("savegame file corrupt (short)"); }

void gzput(int i) { if(gzputc(f, i)==-1) fatal("gzputc"); }
void gzputw(int i) { if(gzputc(f, i & 0xff)==-1 || gzputc(f, (i>>8) & 0xff)==-1 || i >= 1<<16 || i < 0) fatal("gzputw"); }

int gzget() { uchar c = gzgetc(f); return c; }
int gzgetw() { int w = gzgetc(f) | gzgetc(f)<<8; return w; }

void putvec(ucharbuf &p, vec &d) {loopi(3) putfloat(p, d.v[i]);}
void getvec(vec &d, ucharbuf &p) {loopi(3) d.v[i] = getfloat(p);}

int gzwritedp(int id)
{
    int len = dp.length();
    gzput(id); gzputw(len);
    gzcheck(gzwrite(f, iobuf, len), len);
    dp.forcelen(0);
    return len;
}

int gzreaddp(int id, bool seekid = false)
{
    int len, fid;
    do { 
        fid = gzget();
        len = gzgetw();
        gzcheck(gzread(f, iobuf, len), len);
        if (fid == SD_EOF) break;
    } while (seekid && fid != id && !gzeof(f));
    dp.forcelen(0); dp.forcemaxlen(len);
    if(fid != id && fid != SD_EOF)
        conoutf("demo playback aborted: block id error found %d, expected %d", fid, id);
    if(gzeof(f) || fid == SD_EOF || fid != id)
    {
        stopreset();
        return -1;
    }
    return len;
}

void resetdp() { dp.forcelen(0); dp.forcemaxlen(65535); }

void putplayerent(ucharbuf &p, playerent *d)
{
    // physent
    putvec(p, d->o); putvec(p, d->vel);                                           // origin, velocity
    putfloat(p, d->yaw); putfloat(p, d->pitch); putfloat(p, d->roll);             // used as vec in one place
    putfloat(p, d->maxspeed);                                                     // cubes per second, 24 for player
    putint(p, d->timeinair);                                                      // used for fake gravity
    putfloat(p, d->radius); putfloat(p, d->eyeheight); putfloat(p, d->aboveeye);  // bounding box size
    p.put(d->inwater<<0 | d->onfloor<<1 | d->onladder<<2 | d->jumpnext<<3);
    putint(p, d->move); putint(p, d->strafe);
    putint(p, d->state); putint(p, d->type);

    // dynent
    p.put(d->k_left<<0 | d->k_right<<1 | d->k_up<<2 | d->k_down<<3);              // see input code  
    loopi(2)
    {
        putint(p, d->prev[i].anim); putint(p, d->prev[i].frame);
        putint(p, d->prev[i].range); putint(p, d->prev[i].basetime);
        putfloat(p, d->prev[i].speed);
        putint(p, d->current[i].anim); putint(p, d->current[i].frame);
        putint(p, d->current[i].range); putint(p, d->current[i].basetime);
        putfloat(p, d->current[i].speed);
        putint(p, d->lastanimswitchtime[i]);
    }
    // void *lastmodel[2];  seems to be unused...

    // playerent
    putint(p, d->clientnum); putint(p, d->lastupdate); putint(p, d->plag); putint(p, d->ping);
    putint(p, d->lifesequence);                           // sequence id for each respawn, used in damage test
    putint(p, d->frags); putint(p, d->flagscore);
    putint(p, d->health); putint(p, d->armour);
    putint(p, d->gunselect); putint(p, d->gunwait);
    putint(p, d->lastaction); putint(p, d->lastattackgun); putint(p, d->lastmove);
    putint(p, d->lastpain); putint(p, d->lastteamkill);
    putint(p, d->clientrole);
    loopi(NUMGUNS) { putint(p, d->ammo[i]); putint(p, d->mag[i]); }
    sendstring(d->name, p); sendstring(d->team, p);
    putint(p, d->shots);                                  //keeps track of shots from auto weapons
    //p.put(d->attacking<<0 | d->reloading<<1 | d->hasarmour<<2 | d->weaponchanging<<3);
    p.put(d->attacking<<0 | d->reloading<<1 | d->weaponchanging<<3);
    putint(p, d->nextweapon);                             // weapon we switch to
    putint(p, d->primary);                                //primary gun
    putint(p, d->nextprimary);                            // primary after respawning
    putint(p, d->skin); putint(p, d->nextskin);           // skin after respawning
    putint(p, d->thrownademillis);
    // struct bounceent *inhandnade;              stored pointer was evil, anyway...
    putint(p, d->akimbo);
    loopi(2) putint(p, d->akimbolastaction[i]);
    putint(p, d->akimbomillis);

    // poshist Previous stored locations of this player
    putint(p, d->history.nextupdate); putint(p, d->history.curpos); putint(p, d->history.numpos);
    loopi(POSHIST_SIZE) putvec(p, d->history.pos[i]);
}

void getplayerent(playerent *d, ucharbuf &p)
{
    // physent
    getvec(d->o, p); getvec(d->vel, p);                                                      // origin, velocity
    d->yaw = getfloat(p); d->pitch = getfloat(p); d->roll = getfloat(p);                     // used as vec in one place
    d->maxspeed = getfloat(p);                                                               // cubes per second, 24 for player
    d->timeinair = getint(p);                                                                // used for fake gravity
    d->radius = getfloat(p); d->eyeheight = getfloat(p); d->aboveeye = getfloat(p);          // bounding box size
    int e = p.get();
    d->inwater = e & 1; d->onfloor = e>>1 & 1; d->onladder = e>>2 & 1; d->jumpnext = e>>3 & 1;
    d->move = getint(p); d->strafe = getint(p);
    d->state = getint(p); d->type = getint(p);

    // dynent
    e = p.get();
    d->k_left = e & 1; d->k_right = e>>1 & 1; d->k_up = e>>2 & 1; d->k_down = e>>3 & 1;      // see input code  
    loopi(2)
    {
        d->prev[i].anim = getint(p); d->prev[i].frame = getint(p);
        d->prev[i].range = getint(p); d->prev[i].basetime = getint(p);
        d->prev[i].speed = getfloat(p);
        d->current[i].anim = getint(p); d->current[i].frame = getint(p);
        d->current[i].range = getint(p); d->current[i].basetime = getint(p);
        d->current[i].speed = getfloat(p);
        d->lastanimswitchtime[i] = getint(p);
    }
    loopi(2) d->lastmodel[i] = NULL;  // seems to be unused...

    // playerent
    d->clientnum = getint(p); d->lastupdate = getint(p); d->plag = getint(p); d->ping = getint(p);
    d->lifesequence = getint(p);                             // sequence id for each respawn, used in damage test
    d->frags = getint(p); d->flagscore = getint(p);
    d->health = getint(p); d->armour = getint(p);
    d->gunselect = getint(p); d->gunwait = getint(p);
    d->lastaction = getint(p); d->lastattackgun = getint(p); d->lastmove = getint(p);
    d->lastpain = getint(p); d->lastteamkill = getint(p);
    d->clientrole = getint(p);
    loopi(NUMGUNS) { d->ammo[i] = getint(p); d->mag[i] = getint(p); }
    getstring(d->name, p); getstring(d->team, p);
    d->shots = getint(p);                                    //keeps track of shots from auto weapons
    e = p.get();
    //d->attacking = e & 1; d->reloading = e>>1 & 1; d->hasarmour = e>>2 & 1; d->weaponchanging = e>>3 & 1;
    //fixme
    d->nextweapon = getint(p);                               // weapon we switch to
    d->primary = getint(p);                                  //primary gun
    d->nextprimary = getint(p);                              // primary after respawning
    d->skin = getint(p); d->nextskin = getint(p);            // skin after respawning
    d->thrownademillis = getint(p);
    d->inhandnade = NULL;                                    // stored pointer was evil, anyway...
    d->akimbo = getint(p);
    loopi(2) d->akimbolastaction[i] = getint(p);
    d->akimbomillis = getint(p);

    // poshist Previous stored locations of this player
    d->history.nextupdate = getint(p); d->history.curpos = getint(p); d->history.numpos = getint(p);
    loopi(POSHIST_SIZE) getvec(d->history.pos[i], p);
}

void stop()
{
    if(f)
    {
        if(demorecording)
        {
            resetdp();
            putint(dp, lastmillis-starttime);   // total recording time
            gzwritedp(SD_EOF);
        }
        gzclose(f);
    }
    f = NULL;
    demorecording = false;
    demoplayback = false;
    demoloading = false;
    loopv(playerhistory) zapplayer(playerhistory[i]);
    playerhistory.setsize(0);
    setvar("gamespeed", 100);
    extern void recomputecamera();
    recomputecamera();
}

void stopifrecording() { if(demorecording) stop(); }

void savestate(char *fn)
{
    stop();
    f = opengzfile(fn, "wb9");
    if(!f) { conoutf("could not write %s", fn); return; }
    gzwrite(f, (void *)"CUBESAVE", 8);
    gzputc(f, SDL_BYTEORDER==SDL_LIL_ENDIAN ? 1 : 0);
    gzputw(SAVEGAMEVERSION); gzputw(0);                  // compatibility, so far...

    resetdp();
    sendstring(getclientmap(), dp);
    putint(dp, gamemode);
    gzwritedp(SD_INIT1);

    putint(dp, ents.length());
    loopv(ents) putint(dp, ents[i].spawned);
    putplayerent(dp, player1);
    putint(dp, players.length());
    loopv(players)
    {
        int e = players[i]==NULL;
        putint(dp, e);
        if (!e) putplayerent(dp, players[i]);
    }
    gzwritedp(SD_INIT2);
}

void savegame(char *name)
{
   conoutf("can only save classic sp games"); 
   return; 
}

void loadstate(char *fn)
{
    stop();
    if(multiplayer()) return;
    f = opengzfile(fn, "rb9");
    if(!f) { conoutf("could not open %s", fn); return; }
    
    string buf;
    gzread(f, buf, 8);
    if(strncmp(buf, "CUBESAVE", 8)) goto out;
    gzgetc(f);   // who cares...
    if(gzgetw()!=SAVEGAMEVERSION) goto out;
    gzgetw();   // dummy

    if (gzreaddp(SD_INIT1) < 0) goto out;
    string mapname;
    getstring(mapname, dp, _MAXDEFSTR);
    nextmode = getint(dp);
    changemap(mapname); // continue below once map has been loaded and client & server have updated 
    if (dp.remaining() || dp.overread()) conoutf("SD_INIT1 packet size mismatch");         // debug only
    return;
    out:    
    conoutf("aborting: savegame/demo header malformed");
    stop();
}

void loadgame(char *name)
{
    s_sprintfd(fn)("savegames/%s.csgz", name);
    loadstate(fn);
}

void loadgameout()
{
    stop();
    conoutf("loadgame incomplete: savegame from a different version of this map");
}

void fixplayerstate(playerent *d)
{
    if(!d) return;
    d->lastaction = d->lastpain = d->lastteamkill = 0;
    d->resetanim();
}

void loadgamerest()
{
    if(demoplayback || !f) return;

    if (gzreaddp(SD_INIT2) < 0) return;
    if(getint(dp)!=ents.length()) return loadgameout();
    loopv(ents)
    {
        ents[i].spawned = getint(dp)!=0;   
    }
    restoreserverstate(ents);

    getplayerent(player1, dp);
    fixplayerstate(player1);
    
    int nplayers = getint(dp);
    loopi(nplayers) if(!getint(dp))
    {
        playerent *d = newclient(i);
        ASSERT(d);
        getplayerent(d, dp);        
        fixplayerstate(d);
    }
    if (dp.remaining() || dp.overread()) conoutf("SD_INIT2 packet size mismatch");     // debug only
    conoutf("savegame restored");
    if(demoloading) startdemo(); else stop();
}

// demo functions

int playbacktime = 0;
int ddamage, bdamage;
vec dorig;


void record(char *name)
{
    int cn = getclientnum();
    if(cn<0) return;
    s_sprintfd(fn)("demos/%s.cdgz", name);
    savestate(fn);
    resetdp();
    putint(dp, cn);
    gzwritedp(SD_INIT3);
    conoutf("started recording demo to %s", fn);
    demorecording = true;
    starttime = lastmillis;
    ddamage = bdamage = 0;
    //fixme
    player1->lastaction = player1->lastanimswitchtime[0] = player1->lastanimswitchtime[1] = lastmillis;
}

void demodamage(int damage, vec &o) { ddamage = damage; dorig = o; }
void demoblend(int damage) { bdamage = damage; }

void incomingdemodata(int chan, uchar *buf, int len, bool extras)
{
    if(!demorecording) return;
    resetdp();
    putint(dp, lastmillis-starttime);    // playbacktime
    putint(dp, len);
    putint(dp, chan);
    dp.put(buf, len);
    putint(dp, extras ? 1 : 0);     // pre-announce SD_PLAYER1 block for reading convenience
    gzwritedp(SD_NETDATA);
    if(extras)
    {
        putint(dp, player1->gunselect);
        putint(dp, player1->lastattackgun);
        putint(dp, player1->gunwait);
        ASSERT(player1->lastaction == 0 || player1->lastaction-starttime >= 0);
        putint(dp, player1->lastaction-starttime);
        putint(dp, player1->lastanimswitchtime[0]-starttime);
        putint(dp, player1->lastanimswitchtime[1]-starttime);
        loopi(NUMGUNS) { putint(dp, player1->ammo[i]); putint(dp, player1->mag[i]); }
        putint(dp, player1->akimbo ? 1 : 0 | (player1->reloading ? 1 : 0) << 1 | (player1->weaponchanging ? 1 : 0) << 2);
        switch(player1->gunselect)
        {
            case GUN_GRENADE:
                putint(dp, player1->thrownademillis-starttime);
                break;
            case GUN_PISTOL:
                if(player1->akimbo)
                {
                    putint(dp, player1->akimbolastaction[0]-starttime);
                    putint(dp, player1->akimbolastaction[1]-starttime);
                }
                break;
            case GUN_SNIPER:
                putint(dp, scoped ? 1 : 0);
                break;
        }
        
        putint(dp, player1->health);
        putint(dp, player1->armour);
        putint(dp, player1->state);
        putint(dp, bdamage);
        bdamage = 0;
        putint(dp, ddamage);
        if(ddamage)	{ putvec(dp, dorig); ddamage = 0; }
        // FIXME: add all other client state which is not send through the network
        gzwritedp(SD_PLAYER1);
    }
}

static bool hasbackup = false;

void settingsbackup(bool save)
{
    static string name, team;
    static int gunselect;

    if(save)
    { 
        s_strcpy(name, player1->name);
        s_strcpy(team, player1->team);
        gunselect = player1->gunselect;
        hasbackup = true;
    }
    else if (hasbackup)
    {
        s_strcpy(player1->name, name);
        s_strcpy(player1->team, team);
        player1->gunselect = gunselect;
        player1->clientnum = 0;
    }
}

void demo(char *name)
{
    settingsbackup(true);
    s_sprintfd(fn)("demos/%s.cdgz", name);
    loadstate(fn);
    demoloading = true;
}

void stopreset()
{
    conoutf("demo stopped (%d msec elapsed)", lastmillis-starttime);
    if (hasbackup) player1 = newplayerent();
    disconnect(0, 0);
    settingsbackup(false);
}

VAR(demoplaybackspeed, 10, 100, 1000);
int scaletime(int t) { return (int)(t*(100.0f/demoplaybackspeed))+starttime; }

void readdemotime()
{   
    if (gzreaddp(SD_NETDATA, true) < 0)        // skip unneeded packets
        return;                                // demo done
    playbacktime = getint(dp);
    playbacktime = scaletime(playbacktime);
}

bool demopaused = false;
void togglepause() { demopaused = !demopaused; }

playerent *demoplayer = player1;
bool firstpersondemo = true;
bool localdemoplayer1st() { return demoplayback && demoplayer == player1 && firstpersondemo; }

void shiftdemoplayer(int i)
{
    if(!i) return;
    if(demoplayer == player1) 
    { 
        firstpersondemo = !firstpersondemo;
        if(!firstpersondemo) return;
    }
    vector<playerent *> plrs;
    loopv(players) if(players[i] != NULL) plrs.add(players[i]);
    int cur = plrs.find(demoplayer);
    if(cur >= 0)
        demoplayer = plrs[(cur+i) % plrs.length()];
    else
        demoplayer = player1;
}

void startdemo()
{
    if (gzreaddp(SD_INIT3) < 0) return;
    democlientnum = getint(dp);
    demoplayback = true;
    starttime = lastmillis;
    conoutf("now playing demo");
    while(democlientnum>=players.length()) players.add(NULL);
    players[democlientnum] = demoplayer = player1;
    readdemotime();
}

VAR(demodelaymsec, 0, 120, 500);

void catmulrom(vec &z, vec &a, vec &b, vec &c, float s, vec &dest)		// spline interpolation
{
	float s2 = s*s;
	float s3 = s*s2;

	dest = a;
	dest.mul(2*s3 - 3*s2 + 1);
	dest.add(vec(b).mul(-2*s3 + 3*s2));
    dest.add(vec(b).sub(z).mul(0.5f).mul(s3 - 2*s2 + s));
	dest.add(vec(c).sub(a).mul(0.5f).mul(s3 - s2));
}

void fixwrap(dynent *a, dynent *b)
{
	while(b->yaw-a->yaw>180)  a->yaw += 360;  
	while(b->yaw-a->yaw<-180) a->yaw -= 360;
}

void demoplaybackstep()
{
    while(demoplayback && lastmillis>=playbacktime)
    {
        int len = getint(dp);
        if(len<1 || len>MAXTRANS)
        {
            conoutf("error: huge packet during demo play (%d)", len);
            stopreset();
            return;
        }
        int chan = getint(dp);
        uchar *buf = dp.current();
        dp.advance(len);
	int extras = getint(dp);         // extras may be >1 in future...
        if (dp.remaining() || dp.overread()) conoutf("SD_NETDATA packet size mismatch");     // debug only
        bool updatehistory = (!extras && playerhistory.length());
        if (updatehistory) memcpy(player1, playerhistory.last(), sizeof(playerent));
        localservertoclient(chan, buf, len);  // update game state
        if (updatehistory) memcpy(playerhistory.last(), player1, sizeof(playerent));
        
        playerent *target = (playerent *)players[democlientnum];
        ASSERT(target); 
        
        if(extras)     // read additional client side state not present in normal network stream
        {
            if (gzreaddp(SD_PLAYER1) < 0) return;
            target->gunselect = getint(dp);
            target->lastattackgun = getint(dp);
            target->gunwait = getint(dp);
            target->lastaction = scaletime(getint(dp));
            //ASSERT(target->lastaction >= 0);
            if(target->lastaction < 0) target->lastaction = 0;
            target->lastanimswitchtime[0] = scaletime(getint(dp));
            target->lastanimswitchtime[1] = scaletime(getint(dp));
            loopi(NUMGUNS) { target->ammo[i] = getint(dp); target->mag[i] = getint(dp); }
            uchar flags = getint(dp);
            target->akimbo = flags&1 ? true : false;
            target->reloading = (flags>>1)&1 ? true : false;
            target->weaponchanging = (flags>>2)&1 ? true : false;
            switch(target->gunselect)
            {
                case GUN_GRENADE:
                    target->thrownademillis = scaletime(getint(dp));
                    break;
                case GUN_PISTOL:
                    if(player1->akimbo)
                    {
                        target->akimbolastaction[0] = scaletime(getint(dp));
                        target->akimbolastaction[1] = scaletime(getint(dp));
                    }
                    break;
                case GUN_SNIPER:
                    scoped = getint(dp) ? true : false;
                    break;
            }

            target->health = getint(dp);
            target->armour = getint(dp);
            target->state = getint(dp);
			if((bdamage = getint(dp))) damageblend(bdamage);
			if((ddamage = getint(dp))) { getvec(dorig, dp); particle_splash(3, ddamage, 1000, dorig); };
            // FIXME: set more client state here
            target->lastmove = playbacktime;
            if (dp.remaining() || dp.overread()) conoutf("SD_PLAYER1 packet size mismatch");     // debug only
        }
        
        // insert latest copy of player into history
        if(extras && (playerhistory.empty() || playerhistory.last()->lastupdate!=playbacktime))
        {
            playerent *d = newplayerent();
            *d = *target;
            d->lastupdate = playbacktime;
            playerhistory.add(d);
            if(playerhistory.length()>20)
            {
                zapplayer(playerhistory[0]);
                playerhistory.remove(0);
            }
        }
        
        readdemotime();
    }
    
    if(demoplayback)
    {
        if(playerhistory.length()) memcpy(player1, playerhistory.last(), sizeof(playerent));
        int itime = lastmillis-demodelaymsec;
        loopvrev(playerhistory) if(playerhistory[i]->lastupdate<itime)      // find 2 positions in history that surround interpolation time point
        {
            playerent *a = playerhistory[i];
            playerent *b = a;
            if(i+1<playerhistory.length()) b = playerhistory[i+1];
            player1->o = b->o;
            player1->yaw = b->yaw;
            player1->pitch = b->pitch;
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
				if(z->o.dist(c->o)<16)		// if teleport or spawn, dont't interpolate
				{
					catmulrom(z->o, a->o, b->o, c->o, bf, player1->o);
					catmulrom(*(vec *)&z->yaw, *(vec *)&a->yaw, *(vec *)&b->yaw, *(vec *)&c->yaw, bf, *(vec *)&player1->yaw);
				}
				fixcamerarange(player1);
			}
            break;
        }
        //if(player1->state!=CS_DEAD) showscores(false);
    }
}

void stopn() { if(demoplayback) stopreset(); else stop(); conoutf("demo stopped"); }

int demomillis() { return ((lastmillis-starttime)*demoplaybackspeed)/100; }

COMMAND(record, ARG_1STR);
COMMAND(demo, ARG_1STR);
COMMANDN(stop, stopn, ARG_NONE);
COMMANDN(demopause, togglepause, ARG_NONE);
COMMANDN(demoplayer, shiftdemoplayer, ARG_1INT);

COMMAND(savegame, ARG_1STR);
COMMAND(loadgame, ARG_1STR);
