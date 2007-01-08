// clientgame.cpp: core game related stuff

#include "cube.h"
#include "bot/bot.h"

int nextmode = 0;         // nextmode becomes gamemode after next map load
VAR(gamemode, 1, 0, 0);

flaginfo flaginfos[2];

void mode(int n) 
{
	if(m_mp(n) || !multiplayer()) addmsg(SV_GAMEMODE, "ri", nextmode = n); 
};
COMMAND(mode, ARG_1INT);

bool intermission = false;
bool autoteambalance = false;

playerent *player1 = newplayerent();          // our client
vector<playerent *> players;                        // other clients

VARP(sensitivity, 0, 30, 10000);
VARP(sensitivityscale, 1, 10, 10000);
VARP(invmouse, 0, 0, 1);

int lastmillis = 0;
int curtime;
string clientmap;

extern int framesinmap;

char *getclientmap() { return clientmap; };

extern bool c2sinit;
extern int dblend;

void setskin(playerent *pl, uint skin)
{ 
	if(!pl) return;
	if(pl == player1) c2sinit=false;
	const int maxskin[2] = { 3, 5 };
	pl->skin = skin % (maxskin[team_int(pl->team)]+1);
};

extern void throw_nade(playerent *d, vec &vel, bounceent *p);

void deathstate(playerent *pl)
{
	pl->lastaction = lastmillis;
    pl->lifesequence++;
    pl->attacking = false;
    pl->state = CS_DEAD;
    pl->oldpitch = pl->pitch;
    pl->pitch = 0;
    pl->roll = 60;
	pl->strafe = 0;
	dblend = 0;
	if(pl == player1 && pl->inhandnade) throw_nade(pl, vec(0,0,0), pl->inhandnade);
};

void spawnstate(playerent *d)              // reset player state not persistent accross spawns
{
    d->respawn();
    //d->lastaction = lastmillis;
    if(d==player1) 
    {
        gun_changed = true;
        if(player1->skin!=player1->nextskin) setskin(player1, player1->nextskin);
        setscope(false);
    };
    equip(d);
	if(m_osok) d->health = 1;
};
    
playerent *newplayerent()                 // create a new blank player
{
    playerent *d = new playerent;
    d->lastupdate = lastmillis;
	setskin(d, rnd(6));
    spawnstate(d);
    return d;
};

botent *newbotent()                 // create a new blank player
{
    botent *d = new botent;
    d->lastupdate = lastmillis;
    setskin(d, rnd(6));
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
    if(!m_arena || intermission) return;
    
    if(arenarespawnwait)
    {
        if(arenarespawnwait<lastmillis)
        {
            arenarespawnwait = 0;
            conoutf("new round starting... fight!");
            respawnself();
            // Added by Rick: Let all bots respawn if were the host
            if(m_botmode) BotManager.RespawnBots();
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
        if((m_botmode && player1->state==CS_DEAD) || (dead>0 && (alive<=1 || (m_teammode && oneteam))))
        {
            conoutf("arena round is over! next round in 5 seconds...");
            if(alive) 
            {
                  if(m_botmode && player1->state==CS_DEAD)
                        conoutf("the bots have won the round!");
                  else if(m_teammode)
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
        else if(!lagtime || intermission) continue;
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
        if(sleeps[i].wait && lastmillis > sleeps[i].wait) 
        {
	        execute(sleeps[i].cmd);
			delete[] sleeps[i].cmd;
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
        if(m_botmode) BotManager.Think();            
        
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
				checkitems(player1);
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


#define SECURESPAWNDIST 15
int spawncycle = -1;
int fixspawn = 2;

// returns -2 for a free place, else dist to the nearest enemy
float nearestenemy(vec place, char *team)
{
    float nearestenemydist = -1;
    loopv(players)
    {
        playerent *other = players[i];
        if(!other || isteam(team, other->team)) continue;
        float dist = place.dist(other->o);
        if(dist < nearestenemydist || nearestenemydist == -1) nearestenemydist = dist;
    };
    if(nearestenemydist >= SECURESPAWNDIST || nearestenemydist == -1) return -2;
    else return nearestenemydist;
};

int findplayerstart(playerent *d)
{
    int r = fixspawn-->0 ? 4 : rnd(10)+1;

    if(m_teammode) loopi(r) spawncycle = findentity(PLAYERSTART, spawncycle+1, team_int(d->team));
    else if(m_arena) loopi(r) spawncycle = findentity(PLAYERSTART, spawncycle+1, 100);
    else
    {
        int bestent = -1;
        float bestdist = -1;

        loopi(r)
        {
            spawncycle = findentity(PLAYERSTART, spawncycle+1);
            if(spawncycle < 0 || spawncycle >= ents.length()) continue;
            float dist = nearestenemy(vec(ents[spawncycle].x, ents[spawncycle].y, ents[spawncycle].z), d->team);
            if(dist == -2 || (bestdist != -2 && dist > bestdist) || bestent == -1) { bestent = spawncycle; bestdist = dist; }
        };

        return bestent;
    };
    return spawncycle;
};

void spawnplayer(playerent *d)   // place at random spawn
{
    int e = findplayerstart(d);
    if(e!=-1)
    {
        d->o.x = ents[e].x;
        d->o.y = ents[e].y;
        d->o.z = ents[e].z;
        d->yaw = ents[e].attr1;
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
    if(ad>pl->armour) ad = pl->armour;
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
                conoutf("\f2%s suicided", pl->name); 
            }
            else if(isteam(pl->team, act->team))
            {
                --act->frags; 
                conoutf("\f2%s fragged %s teammate (%s)", act==player1 ? "you" : act->name, act==player1 ? "a" : "his", pl->name);
            }
            else
            {
                ++act->frags;
                conoutf("\f2%s fragged %s", act==player1 ? "you" : act->name, pl->name);
            };
        }
        else if(act==pl)
        {
            actor = getclientnum();
            conoutf("\f2you suicided!");
            addmsg(SV_FRAGS, "ri", --pl->frags);
        }
        else if(act)
        {
            if(isteam(act->team, player1->team))
            {
                conoutf("\f2you got fragged by a teammate (%s)", act->name);
            }
            else
            {
                conoutf("\f2you got fragged by %s", act->name);
            };
        };
        if(pl==player1)
        {
            if(m_ctf) tryflagdrop();
            showscores(true);
		    setscope(false);
            addmsg(gib ? SV_GIBDIED : SV_DIED, "ri", actor);
        };
		deathstate(pl);
        //spawnstate(pl); TESTME
		playsound(S_DIE1+rnd(2), pl!=player1 ? &pl->o : NULL);
		if(gib) addgib(pl);
		if(pl!=player1 || act->type==ENT_BOT) act->frags++;
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

playerent *newclient(int cn)   // ensure valid entity
{
    if(cn<0 || cn>=MAXCLIENTS)
    {
        neterr("clientnum");
        return NULL;
    };
    while(cn>=players.length()) players.add(NULL);
    return players[cn] ? players[cn] : (players[cn] = newplayerent());
};

playerent *getclient(int cn)   // ensure valid entity
{
    return players.inrange(cn) ? players[cn] : NULL;
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
                if(e.attr2>2) { conoutf("\f3invalid ctf-flag entity (%i)", i); e.attr2 = 0; };
                flaginfo &f = flaginfos[e.attr2];
				f.team = e.attr2;
                f.flag = &e;
                f.state = CTFF_INBASE;
                f.originalpos.x = (float) e.x;
                f.originalpos.y = (float) e.y;
                f.originalpos.z = (float) e.z;
            };
        };
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
    spawnplayer(player1);
    player1->frags = 0;
    player1->flagscore = 0;
	if(m_teammode) newteam(player1->team); // ensure valid team
    loopv(players) if(players[i]) players[i]->frags = players[i]->flagscore = 0;
    s_strcpy(clientmap, name);
    if(editmode) toggleedit();
    setvar("gamespeed", 100);
    setvar("fog", 180);
    setvar("fogcolour", 0x8099B3);
    showscores(false);
    intermission = false;
    conoutf("game mode is \"%s\"", modestr(gamemode));
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

// console and audio feedback

void flagmsg(int flag, int action) 
{
    flaginfo &f = flaginfos[flag];
    if(!f.actor || !f.ack) return;
    bool own = flag == team_int(player1->team);
    switch(action)
    {
        case SV_FLAGPICKUP:
        {
            playsound(S_FLAGPICKUP);
            if(f.actor==player1) conoutf("\f2you got the enemy flag");
            else conoutf("\f2%s got %s flag", f.actor->name, (own ? "your": "the enemy"));
            break;
        };
        case SV_FLAGDROP:
        {
            playsound(S_FLAGDROP);
            if(f.actor==player1) conoutf("you lost the flag");
            else conoutf("\f2%s lost %s flag", f.actor->name, (own ? "your" : "the enemy"));
            break;
        };
        case SV_FLAGRETURN:
        {
            playsound(S_FLAGRETURN);
            if(f.actor==player1) conoutf("you returned your flag");
            else conoutf("\f2%s returned %s flag", f.actor->name, (own ? "your" : "the enemy"));
            break;
        };
        case SV_FLAGSCORE:
        {
            playsound(S_FLAGSCORE);
            if(f.actor==player1) 
            {
                conoutf("\f2you scored");
                addmsg(SV_FLAGS, "ri", ++player1->flagscore);
            }
            else conoutf("\f2%s scored for %s team", f.actor->name, (own ? "the enemy" : "your"));
            break;
        };
		case SV_FLAGRESET:
		{
			playsound(S_FLAGRETURN);
			conoutf("the server reset the flag");
			break;
		};
        default: break;
    };
};

// server administration

void setmaster(char *claim, char *password)
{
	int c = atoi(claim);
	if(c < 0 || c > 1) return;

	if(password[0])
	{
		if(strlen(password) > 15) { conoutf("the master password has a maximum length of 15 characters"); return; }
		else addmsg(SV_SETMASTERLOGIN, "ris", c, password);
	}
	else addmsg(SV_SETMASTER, "ri", c);
};

void mastercommand(int cmd, int a) // one of MCMD_*
{
	if(player1->ismaster) addmsg(SV_MASTERCMD, "rii", cmd, a);
	else conoutf("\f3you have to be master to perform this action");
};

void kick(int player) { mastercommand(MCMD_KICK, player); };
void ban(int player) { mastercommand(MCMD_BAN, player); };
void removebans() { mastercommand(MCMD_REMBANS, 0); };
void autoteam(int enable) { mastercommand(MCMD_AUTOTEAM, enable); };

COMMAND(setmaster, ARG_2STR);
COMMAND(kick, ARG_1INT);
COMMAND(ban, ARG_1INT);
COMMAND(removebans, ARG_NONE);
COMMAND(autoteam, ARG_1INT);

struct mline { string cmd; };
static vector<mline> mlines;

void *kickmenu = NULL, *banmenu = NULL;

void showmastermenu(int m) // 0=kick, 1=ban
{
	void *menu = m==MCMD_KICK ? kickmenu : banmenu;
    int item = 0;

    mlines.setsize(0);
	loopv(players) if(players[i])
	{
        string &s = mlines.add().cmd;
		s_sprintf(s)("%s %d", m == MCMD_KICK ? "kick" : "ban", i);
        menumanual(menu, item++, players[i]->name, s);
	};
    menuset(menu);
};

COMMAND(showmastermenu, ARG_1INT);

