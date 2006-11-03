// clientgame.cpp: core game related stuff

#include "cube.h"
int nextmode = 0;         // nextmode becomes gamemode after next map load
VAR(gamemode, 1, 0, 0);

flaginfo flaginfos[2];

void mode(int n) 
{
	if(n==7 && clienthost) conoutf("this mode is for singleplayer only");
	else addmsg(1, 2, SV_GAMEMODE, nextmode = n); 
};
COMMAND(mode, ARG_1INT);

bool intermission = false;  

dynent *player1 = newdynent();          // our client
dvector players;                        // other clients

vector <physent *>physents;

VAR(sensitivity, 0, 10, 1000);
VAR(sensitivityscale, 1, 1, 100);
VAR(invmouse, 0, 0, 1);

int lastmillis = 0;
int curtime;
string clientmap;

extern int framesinmap;

char *getclientmap() { return clientmap; };

void resetmovement(dynent *d)
{
    d->k_left = false;
    d->k_right = false;
    d->k_up = false;
    d->k_down = false;  
    d->jumpnext = false;
    d->strafe = 0;
    d->move = 0;
};

extern bool c2sinit;
extern int dblend;

void spawnstate(dynent *d)              // reset player state not persistent accross spawns
{
    resetmovement(d);
    d->vel.x = d->vel.y = d->vel.z = 0; 
    d->onfloor = false;
    d->timeinair = 0;
    d->health = 100;
    d->armour = 0;
    //d->hasarmour = false;
    //d->armourtype = A_BLUE;
    d->akimbomillis = 0;
    d->gunselect = GUN_PISTOL;
    d->gunwait = 0;
    d->attacking = false;
    d->lastaction = lastmillis;
    d->weaponchanging = false;
    if(d==player1) 
    {
        gun_changed = true;
        player1->primary = player1->nextprimary;
        if(player1->skin!=player1->nextskin)
        {
            c2sinit=false;
            player1->skin=player1->nextskin;
        }
        scoped = false;
    };
    radd(d);
	dblend = 0;
    d->akimbo = false;
};
    
dynent *newdynent()                 // create a new blank player or monster
{
    dynent *d = (dynent *)gp()->alloc(sizeof(dynent));
    d->o.x = 0;
    d->o.y = 0;
    d->o.z = 0;
    d->yaw = 270;
    d->pitch = 0;
    d->roll = 0;
    d->maxspeed = 16;  //16 max speed, 14 max with armour
    d->outsidemap = false;
    d->inwater = false;
    d->radius = 1.1f;
    d->eyeheight = 4.5f; //4.25f;
    d->aboveeye = 0.7f;
    d->frags = 0;
    d->flagscore = 0; // EDIT: AH
    d->plag = 0;
    d->ping = 0;
    d->lastupdate = lastmillis;
    d->enemy = NULL;
    d->monsterstate = 0;
    d->name[0] = d->team[0] = 0;
    d->blocked = false;
    d->lifesequence = 0;
    d->state = CS_ALIVE;
    d->shots = 0;
    d->reloading = false;
    d->primary = d->nextprimary = GUN_ASSAULT;
    d->hasarmour = false;
    d->gunselect = GUN_PISTOL;
    d->onladder = false;
    d->isphysent = false;
    d->inhandnade = NULL;
    d->skin = d->nextskin = 0;
    d->bIsBot = false;
	d->pBot = NULL;
    d->weaponchanging = false;
	d->lastanimswitchtime = -1;
	loopi(NUMGUNS) d->ammo[i] = d->mag[i] = 0;
	d->skin = rnd(1 + rb_team_int(d->team) == TEAM_CLA ? 3 : 5);
    spawnstate(d);
    return d;
};

void ctf_death() // EDIT: AH
{
    int flag = rb_opposite(rb_team_int(player1->team));
    flaginfo &f = flaginfos[flag];
    if(f.state==CTFF_STOLEN && f.thief==player1)
    {
        addmsg(1, 2, SV_FLAGDROP, flag);
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

void arenacount(dynent *d, int &alive, int &dead, char *&lastteam, char *&lastname, bool &oneteam)
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
        // Added by Rick: Count bot stuff
        loopv(bots) if(bots[i]) arenacount(bots[i], alive, dead, lastteam, lastname, oneteam);        
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
		player1->akimbo = false;
		player1->akimbomillis = 0;
		player1->mag[GUN_PISTOL] = min(magsize(GUN_PISTOL), player1->mag[GUN_PISTOL]);
		if(player1->gunselect==GUN_PISTOL) weaponswitch(GUN_PISTOL);
		playsoundc(S_PUPOUT);
	}
};

void zapdynent(dynent *&d)
{
    if(d) gp()->dealloc(d, sizeof(dynent));
    d = NULL;
};

extern int democlientnum;

void otherplayers()
{
    loopv(players) if(players[i])
    {
        const int lagtime = lastmillis-players[i]->lastupdate;
        if(lagtime>1000 && players[i]->state==CS_ALIVE)
        {
            players[i]->state = CS_LAGGED;
            continue;
        };
        if(lagtime && players[i]->state != CS_DEAD && (!demoplayback || i!=democlientnum)) moveplayer(players[i], 2, false);   // use physics to extrapolate player position
    };
    // Added by Rick
    if (!ishost())
    {
         loopv(bots)
         {
             if(bots[i] && bots[i]->state != CS_DEAD && (!demoplayback))
                  moveplayer(bots[i], 2, false);   // use physics to extrapolate bot position
         }
    }
    // End add    
};

struct scriptsleep
{
	int wait;
	string cmd;
	scriptsleep(int msec, char *command) { wait = msec+lastmillis; strcpy_s(cmd, command); };
};

vector<scriptsleep> sleeps;

/*int sleepwait = 0;
string sleepcmd;*/
//void addsleep(char *msec, char *cmd) { sleepwait = atoi(msec)+lastmillis; strcpy_s(sleepcmd, cmd); };
void addsleep(char *msec, char *cmd) { scriptsleep s(atoi(msec), cmd); sleeps.add(s); };
COMMANDN(sleep, addsleep, ARG_2STR);

void updateworld(int millis)        // main game update loop
{
    if(lastmillis)
    {     
        curtime = millis - lastmillis;
        //if(sleepwait && lastmillis>sleepwait) { sleepwait = 0; execute(sleepcmd); };
		loopv(sleeps) if(sleeps[i].wait && lastmillis > sleeps[i].wait) { execute(sleeps[i].cmd); sleeps.remove(i); i--; };
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
        mphysents();
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
    lastmillis = millis;
};

void entinmap(dynent *d)    // brute force but effective way to find a free spawn spot in the map
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
        dynent *other = players[i];
        if(!other) continue;
        if(isteam(team,other->team))continue; // its a teammate
        vec place =  {v->x, v->y, v->z};
        vdistsquared(distsquared, t, place, other->o);
        if(nearestPlayerDistSquared == -1) nearestPlayerDistSquared = distsquared; // first run
        else if(distsquared < nearestPlayerDistSquared) nearestPlayerDistSquared = distsquared; // if a player is closer
    };
    if(nearestPlayerDistSquared >= securespawndist * securespawndist || nearestPlayerDistSquared == -1) return -1; // a distance more than securespawndist means the place is free
    else return (int) sqrt(nearestPlayerDistSquared);
};

int spawncycle = -1;
int fixspawn = 2;

void spawnplayer(dynent *d, bool secure)   // place at random spawn
{
    loopj(10) // EDIT: AH
    {
        int r = fixspawn-->0 ? 4 : rnd(10)+1;
        loopi(r) spawncycle = findplayerstart(m_teammode ? rb_team_int(d->team) : 100, spawncycle+1);
        if(spawncycle!=-1 && secure)
        {   
            entity &e = ents[spawncycle];
            vec pos = { e.x, e.y, e.z };
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

void jumpn(bool on) { if(!intermission && (player1->jumpnext = on) ) respawn(); };

COMMAND(backward, ARG_DOWN);
COMMAND(forward, ARG_DOWN);
COMMAND(left, ARG_DOWN);
COMMAND(right, ARG_DOWN);
COMMANDN(jump, jumpn, ARG_DOWN);
COMMAND(attack, ARG_DOWN);
COMMAND(showscores, ARG_DOWN);

void fixplayer1range()
{
    const float MAXPITCH = 90.0f;
    if(player1->pitch>MAXPITCH) player1->pitch = MAXPITCH;
    if(player1->pitch<-MAXPITCH) player1->pitch = -MAXPITCH;
    while(player1->yaw<0.0f) player1->yaw += 360.0f;
    while(player1->yaw>=360.0f) player1->yaw -= 360.0f;
};

void mousemove(int dx, int dy)
{
    if(player1->state==CS_DEAD || intermission) return;
    const float SENSF = 33.0f;     // try match quake sens
    player1->yaw += (dx/SENSF)*(sensitivity/(float)sensitivityscale);
    player1->pitch -= (dy/SENSF)*(sensitivity/(float)sensitivityscale)*(invmouse ? -1 : 1);
	fixplayer1range();
};

// damage arriving from the network, monsters, yourself, all ends up here.

void selfdamage(int damage, int actor, dynent *act, bool gib)
{   
	if(!act) return;
    if(player1->state!=CS_ALIVE || editmode || intermission) return;
    damageblend(damage);
	demoblend(damage);
    int ad = damage*30/100; // let armour absorb when possible
    if(ad>player1->armour) ad = player1->armour;
    player1->armour -= ad;
    damage -= ad;
    float droll = damage/0.5f;
    player1->roll += player1->roll>0 ? droll : (player1->roll<0 ? -droll : (rnd(2) ? droll : -droll));  // give player a kick depending on amount of damage
    if((player1->health -= damage)<=0)
    {
        if(actor==-2)
        {
            conoutf("you got killed by %s!", &act->name);
        }
        else if(actor==-1)
        {
            actor = getclientnum();
            conoutf("you suicided!");
            addmsg(1, 2, SV_FRAGS, --player1->frags);
        }
        else
        {
            // Modified by Rick
            //dynent *a = getclient(actor);
            dynent *a;
            if(act->bIsBot) a = act;
            else a = getclient(actor);
            // End mod
            
            if(a)
            {
                if(isteam(a->team, player1->team))
                {
                    conoutf("you got fragged by a teammate (%s)", a->name);
                }
                else
                {
                    conoutf("you got fragged by %s", a->name);
                };
            };
        };
        // EDIT: AH
        if(m_ctf) ctf_death();
        showscores(true);
		setscope(false);
        if(act->bIsBot) addmsg(1, 2, SV_DIEDBYBOT, actor);
		else addmsg(1, 2, gib ? SV_GIBDIED : SV_DIED, actor);
        player1->lifesequence++;
        player1->attacking = false;
        player1->state = CS_DEAD;
        player1->oldpitch = player1->pitch;
        player1->pitch = 0;
        player1->roll = 60;
        playsound(S_DIE1+rnd(2));
		if(gib) addgib(player1);
        spawnstate(player1);
        //player1->lastaction = lastmillis;
		//fixme
		if (act->bIsBot) addmsg(1, 3, SV_BOTFRAGS, BotManager.GetBotIndex(act), ++act->frags);
    }
    else
    {
        playsound(S_PAIN6);
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

dynent *getclient(int cn)   // ensure valid entity
{
    if(cn<0 || cn>=MAXCLIENTS)
    {
        neterr("clientnum");
        return NULL;
    };
    while(cn>=players.length()) players.add(NULL);
    return players[cn] ? players[cn] : (players[cn] = newdynent());
};

// Added by Rick
dynent *getbot(int cn)   // ensure valid entity
{
    if(cn<0 || cn>=MAXCLIENTS)
    {
        neterr("botnum");
        return NULL;
    };
    
    while(cn>=bots.length()) bots.add(NULL);
    if (!bots[cn])
    {
        bots[cn] = newdynent();
        if (bots[cn])
        {
           bots[cn]->pBot = NULL;
           bots[cn]->bIsBot = true;
        }
    }

    return bots[cn];
};
// End add by Rick

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
//	else sleepwait = 0;
    // End add by Rick            
    projreset();
    if(m_ctf) preparectf();
    shotlinereset();
    spawncycle = -1;
    spawnplayer(player1, true);
    player1->frags = 0;
    player1->flagscore = 0;
    loopv(players) if(players[i]) players[i]->frags = 0;
    resetspawns();
    strcpy_s(clientmap, name);
    if(editmode) toggleedit();
    setvar("gamespeed", 100);
    setvar("fog", 180);
    setvar("fogcolour", 0x8099B3);
    showscores(false);
    intermission = false;
    framesinmap = 0;
    conoutf("game mode is %s", modestr(gamemode));
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
    if(!f.thief) return;
    bool ownflag = flag == rb_team_int(player1->team);
    switch(action)
    {
        case SV_FLAGPICKUP:
        {
            playsound(S_FLAGPICKUP);
            if(f.thief==player1) 
            {
                conoutf("you got the enemy flag");
                f.pick_ack = true;
            }
            else conoutf("%s got %s flag", f.thief->name, (ownflag ? "your": "the enemy"));
            break;
        };
        case SV_FLAGDROP:
        {
            playsound(S_FLAGDROP);
            if(f.thief==player1) conoutf("you lost the flag");
            else conoutf("%s lost %s flag", f.thief->name, (ownflag ? "your" : "the enemy"));
            break;
        };
        case SV_FLAGRETURN:
        {
            playsound(S_FLAGRETURN);
            if(f.thief==player1) conoutf("you returned your flag");
            else conoutf("%s returned %s flag", f.thief->name, (ownflag ? "your" : "the enemy"));
            break;
        };
        case SV_FLAGSCORE:
        {
            playsound(S_FLAGSCORE);
            if(f.thief==player1) 
            {
                conoutf("you scored");
                addmsg(1, 2, SV_FLAGS, ++player1->flagscore);
            }
            else conoutf("%s scored for %s team", f.thief->name, (ownflag ? "the enemy" : "your"));
            break;
        };
        default: break;
    };
};

string masterpwd;

void getmaster(char *pwd)
{
	if(!pwd[0]) return;
	if(strlen(pwd)>15) { conoutf("the master password has a maximum length of 15 characters"); return; };
	strcpy_s(masterpwd, pwd);
};

void mastercommand(int cmd, int a)
{
	addmsg(1, 3, SV_MASTERCMD, cmd, a);
};

void showmastermenu(int m) // 0=kick, 1=ban
{
	int menu = (m == MCMD_KICK ? 3 : 4);
	purgemenu(menu);

	loopv(players)
	{
		if(players[i]) menumanual(menu, i, players[i]->name, (char *) i); // ugly hack
	};
	menuset(menu);
};

COMMANDN(masterlogin, getmaster, ARG_1STR);
COMMAND(mastercommand, ARG_2INT);
COMMAND(showmastermenu, ARG_1INT);