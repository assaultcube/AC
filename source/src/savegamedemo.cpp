// loading and saving of savegames & demos, dumps the spawn state of all mapents, the full state of all dynents (monsters + player)

#include "cube.h"

gzFile f = NULL;
bool demorecording = false;
bool demoplayback = false;
bool demoloading = false;
vector<playerent *> playerhistory;
int democlientnum = 0;

void startdemo();

void gzcheck(int a, int b) { if(a!=b) fatal("savegame file corrupt (short)"); }

void gzput(int i) { if(gzputc(f, i)==-1) fatal("gzputc"); }
void gzputi(int i) { gzcheck(gzwrite(f, &i, sizeof(int)), sizeof(int)); }
void gzputv(vec &v) { gzcheck(gzwrite(f, &v, sizeof(vec)), sizeof(vec)); }

int gzget() { char c = gzgetc(f); return c; }
int gzgeti() { int i; gzcheck(gzread(f, &i, sizeof(int)), sizeof(int)); return i; }
void gzgetv(vec &v) { gzcheck(gzread(f, &v, sizeof(vec)), sizeof(vec)); }

void stop()
{
    if(f)
    {
        if(demorecording) gzputi(-1);
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
    f = gzopen(fn, "wb9");
    if(!f) { conoutf("could not write %s", fn); return; }
    gzwrite(f, (void *)"CUBESAVE", 8);
    gzputc(f, SDL_BYTEORDER==SDL_LIL_ENDIAN ? 1 : 0);
    gzputi(SAVEGAMEVERSION);
    gzputi(sizeof(playerent));
    gzwrite(f, getclientmap(), _MAXDEFSTR);
    gzputi(gamemode);
    gzputi(ents.length());
    loopv(ents) gzputc(f, ents[i].spawned);
    gzwrite(f, player1, sizeof(playerent));
    gzputi(players.length());
    loopv(players)
    {
        gzput(players[i]==NULL);
        gzwrite(f, players[i], sizeof(playerent));
    }
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
    f = gzopen(fn, "rb9");
    if(!f) { conoutf("could not open %s", fn); return; }
    
    string buf;
    gzread(f, buf, 8);
    if(strncmp(buf, "CUBESAVE", 8)) goto out;
    if(gzgetc(f)!=(SDL_BYTEORDER==SDL_LIL_ENDIAN ? 1 : 0)) goto out;     // not supporting save->load accross incompatible architectures simpifies things a LOT
    if(gzgeti()!=SAVEGAMEVERSION || gzgeti()!=sizeof(playerent)) goto out;
    string mapname;
    gzread(f, mapname, _MAXDEFSTR);
    nextmode = gzgeti();
    changemap(mapname); // continue below once map has been loaded and client & server have updated 
    return;
    out:    
    conoutf("aborting: savegame/demo from a different version of AssaultCube or cpu architecture");
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
        
    if(gzgeti()!=ents.length()) return loadgameout();
    loopv(ents)
    {
        ents[i].spawned = gzgetc(f)!=0;   
    }
    restoreserverstate(ents);
    
    gzread(f, player1, sizeof(playerent));
    fixplayerstate(player1);    
    
    int nplayers = gzgeti();
    loopi(nplayers) if(!gzget())
    {
        playerent *d = newclient(i);
        ASSERT(d);
        gzread(f, d, sizeof(playerent));        
        fixplayerstate(d);
    }
    
    conoutf("savegame restored");
    if(demoloading) startdemo(); else stop();
}

// demo functions

int starttime = 0;
int playbacktime = 0;
int ddamage, bdamage;
vec dorig;

void record(char *name)
{
    int cn = getclientnum();
    if(cn<0) return;
    s_sprintfd(fn)("demos/%s.cdgz", name);
    savestate(fn);
    gzputi(cn);
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
    gzputi(lastmillis-starttime);
    gzputi(len);
    gzput(chan);
    gzwrite(f, buf, len);
    gzput(extras);
    if(extras)
    {
        gzput(player1->gunselect);
        gzput(player1->lastattackgun);
        gzputi(player1->gunwait);
        ASSERT(player1->lastaction == 0 || player1->lastaction-starttime >= 0);
        gzputi(player1->lastaction-starttime);
        gzputi(player1->lastanimswitchtime[0]-starttime);
        gzputi(player1->lastanimswitchtime[1]-starttime);
        loopi(NUMGUNS) { gzput(player1->ammo[i]); gzput(player1->mag[i]); }
        gzput(player1->akimbo ? 1 : 0 | (player1->reloading ? 1 : 0) << 1 | (player1->weaponchanging ? 1 : 0) << 2);
        switch(player1->gunselect)
        {
            case GUN_GRENADE:
                gzputi(player1->thrownademillis-starttime);
                break;
            case GUN_PISTOL:
                if(player1->akimbo)
                {
                    gzputi(player1->akimbolastaction[0]-starttime);
                    gzputi(player1->akimbolastaction[1]-starttime);
                }
                break;
            case GUN_SNIPER:
                gzput(scoped ? 1 : 0);
                break;
        }
        
        gzputi(player1->health);
        gzputi(player1->armour);
        gzput(player1->state);
		gzputi(bdamage);
		bdamage = 0;
		gzputi(ddamage);
		if(ddamage)	{ gzputv(dorig); ddamage = 0; }
        // FIXME: add all other client state which is not send through the network
    }
}

void settingsbackup(bool save)
{
    static string name, team;
    static int gunselect;

    if(save)
    { 
        s_strcpy(name, player1->name);
        s_strcpy(team, player1->team);
        gunselect = player1->gunselect;
    }
    else
    {
        s_strcpy(player1->name, name);
        s_strcpy(player1->team, team);
        player1->gunselect = gunselect;
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
    player1 = newplayerent();
    disconnect(0, 0);
    settingsbackup(false);
}

VAR(demoplaybackspeed, 10, 100, 1000);
int scaletime(int t) { return (int)(t*(100.0f/demoplaybackspeed))+starttime; }

void readdemotime()
{   
    if(gzeof(f) || (playbacktime = gzgeti())==-1)
    {
        stopreset();
        return;
    }
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
    if(cur >= 0) demoplayer = plrs[(cur+i) % plrs.length()];
}

void startdemo()
{
    democlientnum = gzgeti();
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
        int len = gzgeti();
        if(len<1 || len>MAXTRANS)
        {
            conoutf("error: huge packet during demo play (%d)", len);
            stopreset();
            return;
        }
        int chan = gzget();
        uchar buf[MAXTRANS];
        gzread(f, buf, len);
        
        int extras = gzget();
        bool updatehistory = (!extras && playerhistory.length());
        if(updatehistory) memcpy(player1, playerhistory.last(), sizeof(playerent));
        localservertoclient(chan, buf, len);  // update game state
        if(updatehistory) memcpy(playerhistory.last(), player1, sizeof(playerent));
 
        playerent *target = (playerent *)players[democlientnum];
        ASSERT(target); 
        
        if(extras)     // read additional client side state not present in normal network stream
        {
            target->gunselect = gzget();
            target->lastattackgun = gzget();
            target->gunwait = gzgeti();
            target->lastaction = scaletime(gzgeti());
            //ASSERT(target->lastaction >= 0);
            if(target->lastaction < 0) target->lastaction = 0;
            target->lastanimswitchtime[0] = scaletime(gzgeti());
            target->lastanimswitchtime[1] = scaletime(gzgeti());
            loopi(NUMGUNS) { target->ammo[i] = gzget(); target->mag[i] = gzget(); }
            uchar flags = gzget();
            target->akimbo = flags&1 ? true : false;
            target->reloading = (flags>>1)&1 ? true : false;
            target->weaponchanging = (flags>>2)&1 ? true : false;
            switch(target->gunselect)
            {
                case GUN_GRENADE:
                    target->thrownademillis = scaletime(gzgeti());
                    break;
                case GUN_PISTOL:
                    if(player1->akimbo)
                    {
                        target->akimbolastaction[0] = scaletime(gzgeti());
                        target->akimbolastaction[1] = scaletime(gzgeti());
                    }
                    break;
                case GUN_SNIPER:
                    scoped = gzget() ? true : false;
                    break;
            }

            target->health = gzgeti();
            target->armour = gzgeti();
            target->state = gzget();
			if((bdamage = gzgeti())) damageblend(bdamage);
			if((ddamage = gzgeti())) { gzgetv(dorig); particle_splash(3, ddamage, 1000, dorig); };
            // FIXME: set more client state here
            target->lastmove = playbacktime;
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
