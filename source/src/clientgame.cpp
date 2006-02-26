// clientgame.cpp: core game related stuff

#include "cube.h"
#define sleep my_sleep
int nextmode = 0;         // nextmode becomes gamemode after next map load
VAR(gamemode, 1, 0, 0);

void mode(int n) { addmsg(1, 2, SV_GAMEMODE, nextmode = n); };
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
    //d->quadmillis = 0;
    d->gunselect = GUN_PISTOL;
    d->mag[6]=10;
    d->gunwait = 0;
    d->attacking = false;
    d->lastaction = 0;

    
    radd(d);
    
    
    //loopi(NUMGUNS) if(d->nextprimary!=i) d->ammo[i] = 0;
    //d->mag[GUN_KNIFE] = 1;
    //equp with default pistol
    
    //d->mag[GUN_PISTOL] = 8;
    //if(d->nextprimary!=GUN_PISTOL)
    //  d->ammo[GUN_PISTOL] = 16;
    //else
    //  d->ammo[GUN_PISTOL] = 32;
    //temp for testing guns
    //d->ammo[GUN_SHOTGUN] = 100;
    //d->ammo[GUN_SUBGUN] = 400;
    //d->ammo[GUN_SNIPER] = 100;
    //d->ammo[GUN_ASSULT] = 400;

    //end tmp

    /*
    if(m_noitems)
    {
        d->gunselect = GUN_RIFLE;
        d->armour = 0;
        if(m_noitemsrail)
        {
            d->health = 1;
            d->ammo[GUN_RIFLE] = 100;
        }
        else
        {
            if(gamemode==12) { d->gunselect = GUN_FIST; return; };  // eihruls secret "instafist" mode
            d->health = 256;
            if(m_tarena)
            {
                int gun1 = rnd(4)+1;
                baseammo(d->gunselect = gun1);
                for(;;)
                {
                    int gun2 = rnd(4)+1;
                    if(gun1!=gun2) { baseammo(gun2); break; };
                };
            }
            else if(m_arena)    // insta arena
            {
                d->ammo[GUN_RIFLE] = 100;
            }
            else // efficiency
            {
                loopi(4) baseammo(i+1);
                d->gunselect = GUN_CG;
            };
            d->ammo[GUN_CG] /= 2;
        };
    }
    else
    {
        d->ammo[GUN_SG] = 5;
    };
    */
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
    d->eyeheight = 4.25f;
    d->aboveeye = 0.7f;
    d->frags = 0;
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
    d->nextprimary = 1;
    d->hasarmour = false;
    spawnstate(d);
    return d;
};
void respawnself()
{
	spawnplayer(player1);
	showscores(false);
};

void respawn()
{
    if(player1->state==CS_DEAD)
    { 
        player1->attacking = false;
        if(m_arena) { conoutf("waiting for new round to start..."); return; };
        respawnself();
    };
};

void arenacount(dynent *d, int &alive, int &dead, char *&lastteam, char *&lastname, bool &oneteam)
//void arenacount(dynent *d, int &alive, int &dead, char *&lastteam, bool &oneteam)
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
                        conoutf("team %s has won the round", (int)lastteam);
                  else 
                        conoutf("%s is the survior!", (int)lastname);
            }
            else conoutf("everyone died!");
            arenarespawnwait = lastmillis+5000;
            arenadetectwait  = lastmillis+10000;
            player1->roll = 0;
        }; 
    };
};


void checkquad(int time)
{
/*    if(player1->quadmillis && (player1->quadmillis -= time)<0)
    {
        player1->quadmillis = 0;
        playsoundc(S_PUPOUT);
        conoutf("quad damage is over");
    };
*/
};

//generic function to check all kinds of crazy gamemode specific things
void checkgame(int time)
{
	
	//allow faster movements when editing
/*	if (editmode)
	{
		player1->maxspeed=22;
		return;
	}
	else  //otherwise max speed is 16 and 14 with armour
	{
		if(player1->armour>50)
			player1->maxspeed = 14;
		if(player1->armour>0)
			player1->maxspeed = 15;
		player1->maxspeed = 16;
	};
*/
	
	//force to a team if team mode
	if (m_teammode)
	{
		if (strcmp(player1->team,"T") && strcmp(player1->team,"CT"))
		{
                        player1->state = CS_DEAD;
                        //conoutf("set team to T or CT to joing game");
			if (rnd(2)<1) strcpy_s(player1->team,"T");
			else strcpy_s(player1->team,"CT");	
		}
	
		//add checks here to force-drop flag if switching team with flag

		//add checks for end conditions of ctf
	};


        //reset zoom if dead

	//etc
	//checkquad(time);

        if (player1->state==CS_DEAD)
        {
            player1->primary = player1->nextprimary;
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
};

extern void explode_nade(physent *i);

void mphysents()
{
    loopv(physents) if(physents[i])
    {
        physent *p = physents[i];
        if(p->throwed) moveplayer(p, 2, false);
        if(lastmillis - p->millis >= p->lifetime)
        {
            explode_nade(physents[i]);
            physents.remove(i);
            i--;
        };
    };
};

int sleepwait = 0;
string sleepcmd;
void sleep(char *msec, char *cmd) { sleepwait = atoi(msec)+lastmillis; strcpy_s(sleepcmd, cmd); };
COMMAND(sleep, ARG_2STR);

void updateworld(int millis)        // main game update loop
{
    if(lastmillis)
    {     
        curtime = millis - lastmillis;
        if(sleepwait && lastmillis>sleepwait) { execute(sleepcmd); sleepwait = 0; };
        physicsframe();
        checkgame(curtime);
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
    conoutf("can't find entity spawn spot! (%d, %d)", (int)d->o.x, (int)d->o.y);
    // leave ent at original pos, possibly stuck
};

int spawncycle = -1;
int fixspawn = 2;

void spawnplayer(dynent *d)   // place at random spawn. also used by monsters!
{
    if(d==player1) gun_changed=true;

    int r = fixspawn-->0 ? 4 : rnd(10)+1;
    loopi(r) spawncycle = findentity(PLAYERSTART, spawncycle+1);
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
    else if(player1->attacking = on) respawn();
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

void selfdamage(int damage, int actor, dynent *act)
{
    if(player1->state!=CS_ALIVE || editmode || intermission) return;
    damageblend(damage);
	demoblend(damage);
    int ad = damage*30/100;//int ad = damage*(player1->armourtype+1)*20/100;     // let armour absorb when possible
    if(ad>player1->armour) ad = player1->armour;
    player1->armour -= ad;
    damage -= ad;
    float droll = damage/0.5f;
    player1->roll += player1->roll>0 ? droll : (player1->roll<0 ? -droll : (rnd(2) ? droll : -droll));  // give player a kick depending on amount of damage
    if((player1->health -= damage)<=0)
    {
        if(actor==-2)
        {
            conoutf("you got killed by %s!", (int)&act->name);
        }
        else if(actor==-1)
        {
            actor = getclientnum();
            conoutf("you suicided!");
            addmsg(1, 2, SV_FRAGS, --player1->frags);
        }
        else
        {
            dynent *a = getclient(actor);
            if(a)
            {
                if(isteam(a->team, player1->team))
                {
                    conoutf("you got fragged by a teammate (%s)", (int)a->name);
                }
                else
                {
                    conoutf("you got fragged by %s", (int)a->name);
                };
            };
        };
        showscores(true);
        addmsg(1, 2, SV_DIED, actor);
        player1->lifesequence++;
        player1->attacking = false;
        player1->state = CS_DEAD;
        player1->oldpitch = player1->pitch;
        player1->pitch = 0;
        player1->roll = 60;
        playsound(S_DIE1+rnd(2));
        spawnstate(player1);
        player1->lastaction = lastmillis;
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

void addc() { strcpy(getclient(1)->team, "CT"); getclient(1)->o = player1->o; strcpy(getclient(1)->name, "sucker"); } COMMAND(addc, ARG_NONE); // FIXME

void initclient()
{
    clientmap[0] = 0;
    initclientnet();
};

void startmap(char *name)   // called just after a map load
{
    //if(netmapstart()) { gamemode = 0;};  //needs fixed to switch modes?
    netmapstart(); //should work
    sleepwait = 0;
    //put call to clear/restart game mode extras here
    projreset(); 
    shotlinereset();
    spawncycle = -1;
    spawnplayer(player1);
    player1->frags = 0;
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
    conoutf("game mode is %s", (int)modestr(gamemode));
}; 

COMMANDN(map, changemap, ARG_1STR);

void suicide()
{
      if (multiplayer()) conoutf("there are others willing to kill you...");
      else conoutf("this is not the answer");
      selfdamage(1000, -1, player1);
      //if(d==player1) selfdamage(dm, -1, d);
      //else { addmsg(1, 4, SV_DAMAGE, d, dm, d->lifesequence); playsound(S_FALL1+rnd(5), &d->o); };
      //particle_splash(3, 1000, 1000, player1->o);  //edit out?
      demodamage(1000, player1->o);
      playsound(S_SUICIDE, &player1->o);

};

COMMAND(suicide, ARG_NONE);

void killdummies()
{
    loopv(players) if(strcmp(players[i]->name, "dummy") == 0) { players[i]->lastaction = lastmillis; players[i]->health=1; };
}; COMMAND(killdummies, ARG_NONE);
