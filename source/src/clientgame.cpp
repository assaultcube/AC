// clientgame.cpp: core game related stuff

#include "cube.h"
int nextmode = 0;         // nextmode becomes gamemode after next map load
VAR(gamemode, 1, 0, 0);

flaginfo flaginfos[2];

void mode(int n) 
{
	if((n>=7 && n<=8) && clienthost) conoutf("this mode is for singleplayer only");
	else addmsg(SV_GAMEMODE, "ri", nextmode = n); 
};
COMMAND(mode, ARG_1INT);

bool intermission = false;  

playerent *player1 = newplayerent();          // our client
vector<playerent *> players;                        // other clients

VAR(sensitivity, 0, 10, 1000);
VAR(sensitivityscale, 1, 1, 100);
VAR(invmouse, 0, 0, 1);

int lastmillis = 0;
int curtime;
string clientmap;

extern int framesinmap;

char *getclientmap() { return clientmap; };

extern bool c2sinit;
extern int dblend;

void spawnstate(playerent *d)              // reset player state not persistent accross spawns
{
    d->respawn();
    //d->lastaction = lastmillis;
    if(d==player1) 
    {
        gun_changed = true;
        player1->primary = m_osok ? GUN_SNIPER : (m_pistol ? GUN_PISTOL : player1->nextprimary);
        if(player1->skin!=player1->nextskin)
        {
            c2sinit=false;
            player1->skin=player1->nextskin;
        }
        setscope(false);
    };
    equip(d);
    if(m_osok) d->health = 1;
	dblend = 0;
};
    
playerent *newplayerent()                 // create a new blank player
{
    playerent *d = new playerent;
    d->lastupdate = lastmillis;
	d->skin = rnd(1 + rb_team_int(d->team) == TEAM_CLA ? 3 : 5);
    spawnstate(d);
    return d;
};

botent *newbotent()                 // create a new blank player
{
    botent *d = new botent;
    d->lastupdate = lastmillis;
    d->skin = rnd(1 + rb_team_int(d->team) == TEAM_CLA ? 3 : 5);
    spawnstate(d);
    loopv(players) if(i!=getclientnum() && !players[i])
    {
        players[i] = d;
        return d;
    };
    if(players.length()==getclientnum()) players.add(NULL);
    players.add(d);
    return d;
};

void freebotent(botent *d)
{
    loopv(players) if(players[i]==d)
    {
        DELETEP(players[i]);
    };
};

void ctf_death() // EDIT: AH
{
    int flag = rb_opposite(rb_team_int(player1->team));
    flaginfo &f = flaginfos[flag];
    if(f.state==CTFF_STOLEN && f.actor==player1)
    {
        addmsg(SV_FLAGDROP, "ri", flag);
        f.flag->spawned = false;
        f.state = CTFF_DROPPED;
    };
};

void respawnself()
{
	spawnplayer(player1);
	showscores(false);
};

void respawn()
{
	if(player1->state==CS_DEAD && lastmillis>player1->lastaction+(m_ctf ? 5000 : 2000))
    { 
        player1->attacking = false;
        if(m_arena) { conoutf("waiting for new round to start..."); return; };
        respawnself();
		weaponswitch(player1->primary);
		player1->lastaction -= WEAPONCHANGE_TIME/2;
    };
};

void arenacount(playerent *d, int &alive, int &dead, char *&lastteam, char *&lastname, bool &oneteam)
{
    if(d->state!=CS_DEAD)
    {
        alive++;
        if(lastteam && strcmp(lastteam, d->team)) oneteam = false;
        lastteam = d->team;
        lastname = d->name;
    }
    else
    {
        dead++;
    };
};

int arenarespawnwait = 0;
int arenadetectwait  = 0;

void arenarespawn()
{
    if (!m_arena) return;
    
    if(arenarespawnwait)
    {
        if(arenarespawnwait<lastmillis)
        {
            arenarespawnwait = 0;
            conoutf("new round starting... fight!");
            respawnself();
            // Added by Rick: Let all bots respawn if were the host
            if (ishost()) BotManager.RespawnBots();
            //End add by Rick
			clearbounceents();
        };
    }
    else if(arenadetectwait==0 || arenadetectwait<lastmillis)
    {
        arenadetectwait = 0;
        int alive = 0, dead = 0;
        char *lastteam = NULL;
        char *lastname = NULL;
        bool oneteam = true;
        loopv(players) if(players[i]) arenacount(players[i], alive, dead, lastteam, lastname, oneteam);
        arenacount(player1, alive, dead, lastteam, lastname, oneteam);
        if(dead>0 && (alive<=1 || (m_teammode && oneteam)))
        {
            conoutf("arena round is over! next round in 5 seconds...");
            if(alive) 
            {
                  if(m_teammode)
                        conoutf("team %s has won the round", lastteam);
                  else 
                        conoutf("%s is the survior!", lastname);
            }
            else conoutf("everyone died!");
            arenarespawnwait = lastmillis+5000;
            arenadetectwait  = lastmillis+10000;
            player1->roll = 0;
        }; 
    };
};


void checkakimbo()
{
	if(player1->akimbo && player1->akimbomillis && player1->akimbomillis<=lastmillis)
	{
		player1->akimbo = 0;
		player1->akimbomillis = 0;
		player1->mag[GUN_PISTOL] = min(magsize(GUN_PISTOL), player1->mag[GUN_PISTOL]);
		if(player1->gunselect==GUN_PISTOL) weaponswitch(GUN_PISTOL);
		playsoundc(S_PUPOUT);
	}
};

void zapplayer(playerent *&d)
{
    DELETEP(d);
};

extern int democlientnum;

void otherplayers()
{
    loopv(players) if(players[i] && players[i]->type==ENT_PLAYER && players[i]->state==CS_ALIVE)
    {
        const int lagtime = lastmillis-players[i]->lastupdate;
        if(lagtime>1000)
        {
            players[i]->state = CS_LAGGED;
            continue;
        }
        else if(!lagtime) continue;
        if(!demoplayback || i!=democlientnum) moveplayer(players[i], 2, false);   // use physics to extrapolate player position
    };
};

struct scriptsleep { int wait; char *cmd; };
vector<scriptsleep> sleeps;

void addsleep(char *msec, char *cmd) 
{ 
    scriptsleep &s = sleeps.add();
    s.wait = atoi(msec)+lastmillis;
    s.cmd = newstring(cmd);
};

COMMANDN(sleep, addsleep, ARG_2STR);

void updateworld(int curtime, int lastmillis)        // main game update loop
{
	loopv(sleeps) 
    {
        scriptsleep &s = sleeps[i];
        if(s.wait && lastmillis > s.wait) 
        { 
            execute(s.cmd); 
            delete[] s.cmd;
            sleeps.remove(i--); 
        };
    };
    physicsframe();
    checkakimbo();
    checkweaponswitch();
	//if(m_arena) arenarespawn();
	arenarespawn();
    moveprojectiles((float)curtime);
    demoplaybackstep();
    if(!demoplayback)
    {
        if(getclientnum()>=0) shoot(player1, worldpos);     // only shoot when connected to server
        gets2c();           // do this first, so we have most accurate information when our player moves
    };
    mbounceents();
    otherplayers();
    if(!demoplayback)
    {
        //monsterthink();
        
        // Added by Rick: let bots think
        BotManager.Think();            
        
        //put game mode extra call here
        if(player1->state==CS_DEAD)
        {
				if(lastmillis-player1->lastaction<2000)
				{
					player1->move = player1->strafe = 0;
					moveplayer(player1, 10, false);
				};
        }
        else if(!intermission)
        {
            moveplayer(player1, 20, true);
				checkitems();
        };
        c2sinfo(player1);   // do this last, to reduce the effective frame lag
    };
};

void entinmap(physent *d)    // brute force but effective way to find a free spawn spot in the map
{
    loopi(100)              // try max 100 times
    {
        float dx = (rnd(21)-10)/10.0f*i;  // increasing distance
        float dy = (rnd(21)-10)/10.0f*i;
        d->o.x += dx;
        d->o.y += dy;
        if(collide(d, true, 0, 0)) return;
        d->o.x -= dx;
        d->o.y -= dy;
    };
    conoutf("can't find entity spawn spot! (%d, %d)", d->o.x, d->o.y);
    // leave ent at original pos, possibly stuck
};

// EDIT: AH
int securespawndist = 15;

// Returns -1 for a free place, if not it returns the vdist to the nearest enemy
int nearestenemy(vec *v, string team)
{
    float nearestPlayerDistSquared = -1;
    loopv(players)
    { 
        playerent *other = players[i];
        if(!other) continue;
        if(isteam(team,other->team))continue; // its a teammate
        vec place(v->x, v->y, v->z);
        float distsquared = vec(other->o).sub(place).squaredlen();
        if(nearestPlayerDistSquared == -1) nearestPlayerDistSquared = distsquared; // first run
        else if(distsquared < nearestPlayerDistSquared) nearestPlayerDistSquared = distsquared; // if a player is closer
    };
    if(nearestPlayerDistSquared >= securespawndist * securespawndist || nearestPlayerDistSquared == -1) return -1; // a distance more than securespawndist means the place is free
    else return (int)sqrtf(nearestPlayerDistSquared);
};

int spawncycle = -1;
int fixspawn = 2;

void spawnplayer(playerent *d, bool secure)   // place at random spawn
{
    loopj(10) // EDIT: AH
    {
        int r = fixspawn-->0 ? 4 : rnd(10)+1;
        loopi(r) spawncycle = findplayerstart(m_teammode ? rb_team_int(d->team) : 100, spawncycle+1);
        if(spawncycle!=-1 && secure)
        {   
            entity &e = ents[spawncycle];
            vec pos(e.x, e.y, e.z);
            if(nearestenemy(&pos, d->team) == -1) break;
        } else break;
    };
    if(spawncycle!=-1)
    {
        d->o.x = ents[spawncycle].x;
        d->o.y = ents[spawncycle].y;
        d->o.z = ents[spawncycle].z;
        d->yaw = ents[spawncycle].attr1;
        d->pitch = 0;
        d->roll = 0;
    }
    else
    {
        d->o.x = d->o.y = (float)ssize/2;
        d->o.z = 4;
    };
    entinmap(d);
    spawnstate(d);
    d->state = CS_ALIVE;
};

// movement input code

#define dir(name,v,d,s,os) void name(bool isdown) { player1->s = isdown; player1->v = isdown ? d : (player1->os ? -(d) : 0); player1->lastmove = lastmillis; };

dir(backward, move,   -1, k_down,  k_up);
dir(forward,  move,    1, k_up,    k_down);
dir(left,     strafe,  1, k_left,  k_right); 
dir(right,    strafe, -1, k_right, k_left); 

void attack(bool on)
{
    if(intermission) return;
    if(editmode) editdrag(on);
	else if(player1->state==CS_DEAD) respawn();
	else player1->attacking = on;
};

void jumpn(bool on) 
{ 
    if(intermission) return;
    if(player1->state==CS_DEAD)
    {
        if(on) respawn();
    }
    else player1->jumpnext = on;
};

COMMAND(backward, ARG_DOWN);
COMMAND(forward, ARG_DOWN);
COMMAND(left, ARG_DOWN);
COMMAND(right, ARG_DOWN);
COMMANDN(jump, jumpn, ARG_DOWN);
COMMAND(attack, ARG_DOWN);
COMMAND(showscores, ARG_DOWN);

void fixcamerarange(physent *cam)
{
    const float MAXPITCH = 90.0f;
    if(cam->pitch>MAXPITCH) cam->pitch = MAXPITCH;
    if(cam->pitch<-MAXPITCH) cam->pitch = -MAXPITCH;
    while(cam->yaw<0.0f) cam->yaw += 360.0f;
    while(cam->yaw>=360.0f) cam->yaw -= 360.0f;
};

void mousemove(int dx, int dy)
{
    if(intermission) return;
    const float SENSF = 33.0f;     // try match quake sens
    camera1->yaw += (dx/SENSF)*(sensitivity/(float)sensitivityscale);
    camera1->pitch -= (dy/SENSF)*(sensitivity/(float)sensitivityscale)*(invmouse ? -1 : 1);
    fixcamerarange();
    if(camera1!=player1 && player1->state!=CS_DEAD)
    {
        player1->yaw = camera1->yaw;
        player1->pitch = camera1->pitch;
    };
};

// damage arriving from the network, monsters, yourself, all ends up here.

void selfdamage(int damage, int actor, playerent *act, bool gib, playerent *pl)
{   
	if(!act) return;
    if(pl->state!=CS_ALIVE || editmode || intermission) return;
    if(pl==player1)
    {
        damageblend(damage);
	    demoblend(damage);
    };
    int ad = damage*30/100; // let armour absorb when possible
    if(ad>pl->armour) ad = player1->armour;
    pl->armour -= ad;
    damage -= ad;
    float droll = damage/0.5f;
    pl->roll += pl->roll>0 ? droll : (pl->roll<0 ? -droll : (rnd(2) ? droll : -droll));  // give player a kick depending on amount of damage
    if((pl->health -= damage)<=0)
    {
        if(pl->type==ENT_BOT)
        {
            if(pl==act) 
            { 
                --pl->frags; 
                conoutf("%s suicided", pl->name); 
            }
            else if(isteam(pl->team, act->team))
            {
                --act->frags; 
                conoutf("%s fragged %s teammate (%s)", act==player1 ? "you" : act->name, act==player1 ? "a" : "his", pl->name);
            }
            else
            {
                ++act->frags;
                conoutf("%s fragged %s", act==player1 ? "you" : act->name, pl->name);
            };
        }
        else if(actor==-2)
        {
            conoutf("you got killed by %s!", &act->name);
        }
        else if(actor==-1)
        {
            actor = getclientnum();
            conoutf("you suicided!");
            addmsg(SV_FRAGS, "ri", --pl->frags);
        }
        else if(act)
        {
            if(isteam(act->team, player1->team))
            {
                conoutf("you got fragged by a teammate (%s)", act->name);
            }
            else
            {
                conoutf("you got fragged by %s", act->name);
            };
        };
        if(pl==player1)
        {
            if(m_ctf) ctf_death();
            showscores(true);
		    setscope(false);
            addmsg(gib ? SV_GIBDIED : SV_DIED, "ri", actor);
        };
        pl->lifesequence++;
        pl->attacking = false;
        pl->state = CS_DEAD;
        pl->oldpitch = pl->pitch;
        pl->pitch = 0;
        pl->roll = 60;
        playsound(S_DIE1+rnd(2), pl!=player1 ? &pl->o : NULL);
		if(gib) addgib(pl);
        spawnstate(pl);
        pl->lastaction = lastmillis;
		if (pl!=player1 || act->type==ENT_BOT) act->frags++;
    }
    else
    {
        playsound(S_PAIN6, pl!=player1 ? &pl->o : NULL);
    };
};

void timeupdate(int timeremain)
{
    if(!timeremain)
    {
        intermission = true;
        player1->attacking = false;
        conoutf("intermission:");
        conoutf("game has ended!");
        showscores(true);
		execute("start_intermission");
    }
    else
    {
        conoutf("time remaining: %d minutes", timeremain);
    };
};

playerent *getclient(int cn)   // ensure valid entity
{
    if(cn<0 || cn>=MAXCLIENTS)
    {
        neterr("clientnum");
        return NULL;
    };
    while(cn>=players.length()) players.add(NULL);
    return players[cn] ? players[cn] : (players[cn] = newplayerent());
};

void initclient()
{
    clientmap[0] = 0;
    initclientnet();
};

entity flagdummies[2]; // in case the map does not provide flags

void preparectf(bool cleanonly=false)
{
    loopi(2) flaginfos[i].flag = &flagdummies[i];
    if(!cleanonly)
    {
        loopv(ents)
        {
            entity &e = ents[i];
            if(e.type==CTF_FLAG) 
            {
                e.spawned = true;
                if(e.attr2>2) { conoutf("invalid ctf-flag entity (%i)", i); e.attr2 = 0; };
                flaginfo &f = flaginfos[e.attr2];
                f.flag = &e;
                f.state = CTFF_INBASE;
                f.originalpos.x = (float) e.x;
                f.originalpos.y = (float) e.y;
                f.originalpos.z = (float) e.z;
            };
        };
        newteam(player1->team); // ensure valid team
    };
};

extern void kickallbots(void);

void startmap(char *name)   // called just after a map load
{
    //if(netmapstart()) { gamemode = 0;};  //needs fixed to switch modes?
    netmapstart(); //should work
    //monsterclear();
    // Added by Rick
	kickallbots(); 
	if(m_botmode) BotManager.BeginMap(name);
    // End add by Rick            
    projreset();
    resetspawns();
    if(m_ctf) preparectf();
    shotlinereset();
    spawncycle = -1;
    spawnplayer(player1, true);
    player1->frags = 0;
    player1->flagscore = 0;
    loopv(players) if(players[i]) players[i]->frags = players[i]->flagscore = 0;
    s_strcpy(clientmap, name);
    if(editmode) toggleedit();
    setvar("gamespeed", 100);
    setvar("fog", 180);
    setvar("fogcolour", 0x8099B3);
    showscores(false);
    intermission = false;
    framesinmap = 0;
    conoutf("game mode is %s", modestr(gamemode));
	clearbounceents();
};

COMMANDN(map, changemap, ARG_1STR);

void suicide()
{
	if(player1->state==CS_DEAD) return;
	selfdamage(1000, -1, player1);
	demodamage(1000, player1->o);
};

COMMAND(suicide, ARG_NONE);

// EDIT: AH
void flagaction(int flag, int action)
{
    flaginfo &f = flaginfos[flag];
    if(!f.actor) return;
    bool ownflag = flag == rb_team_int(player1->team);
    switch(action)
    {
        case SV_FLAGPICKUP:
        {
            playsound(S_FLAGPICKUP);
            if(f.actor==player1) 
            {
                conoutf("you got the enemy flag");
                f.pick_ack = true;
            }
            else conoutf("%s got %s flag", f.actor->name, (ownflag ? "your": "the enemy"));
            break;
        };
        case SV_FLAGDROP:
        {
            playsound(S_FLAGDROP);
            if(f.actor==player1) conoutf("you lost the flag");
            else conoutf("%s lost %s flag", f.actor->name, (ownflag ? "your" : "the enemy"));
            break;
        };
        case SV_FLAGRETURN:
        {
            playsound(S_FLAGRETURN);
            if(f.actor==player1) conoutf("you returned your flag");
            else conoutf("%s returned %s flag", f.actor->name, (ownflag ? "your" : "the enemy"));
            break;
        };
        case SV_FLAGSCORE:
        {
            playsound(S_FLAGSCORE);
            if(f.actor==player1) 
            {
                conoutf("you scored");
                addmsg(SV_FLAGS, "ri", ++player1->flagscore);
            }
            else conoutf("%s scored for %s team", f.actor->name, (ownflag ? "the enemy" : "your"));
            break;
        };
        default: break;
    };
};

void getmaster(char *pwd)
{
	if(!pwd[0]) return;
	if(strlen(pwd)>15) { conoutf("the master password has a maximum length of 15 characters"); return; };
    addmsg(SV_GETMASTER, "rs", pwd);
};

void mastercommand(int cmd, int a)
{
	addmsg(SV_MASTERCMD, "rii", cmd, a);
};

void showmastermenu(int m) // 0=kick, 1=ban
{
	int menu = (m == MCMD_KICK ? 3 : 4);
	purgemenu(menu);

	loopv(players)
	{
		if(players[i]) menumanual(menu, i, players[i]->name, (char *)(size_t)i); // ugly hack
	};
	menuset(menu);
};

COMMANDN(masterlogin, getmaster, ARG_1STR);
COMMAND(mastercommand, ARG_2INT);
COMMAND(showmastermenu, ARG_1INT);
