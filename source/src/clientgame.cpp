// clientgame.cpp: core game related stuff

#include "pch.h"
#include "cube.h"
#include "bot/bot.h"

int nextmode = 0;         // nextmode becomes gamemode after next map load
VAR(gamemode, 1, 0, 0);
VARP(modeacronyms, 0, 0, 1);

flaginfo flaginfos[2];

void mode(int n)
{
	if(m_mp(n) || !multiplayer()) addmsg(SV_GAMEMODE, "ri", nextmode = n);
}
COMMAND(mode, ARG_1INT);

bool intermission = false;
bool autoteambalance = false;
int arenaintermission = 0;

playerent *player1 = newplayerent();          // our client
vector<playerent *> players;                        // other clients

int lastmillis = 0, totalmillis = 0;
int curtime = 0;
string clientmap;

extern int framesinmap;

char *getclientmap() { return clientmap; }

void mapname() { result(getclientmap()); }
COMMAND(mapname, ARG_NONE);

extern bool c2sinit, senditemstoserver;

void setskin(playerent *pl, uint skin)
{
	if(!pl) return;
	if(pl == player1) c2sinit=false;
	const int maxskin[2] = { 3, 5 };
	pl->skin = skin % (maxskin[team_int(pl->team)]+1);
}

bool duplicatename(playerent *d, char *name = NULL)
{
    if(!name) name = d->name;
    if(d!=player1 && !strcmp(name, player1->name)) return true;
    if(!strcmp(name, "you")) return true;
    loopv(players) if(players[i] && d!=players[i] && !strcmp(name, players[i]->name)) return true;
    return false;
}

char *colorname(playerent *d, int num, char *name, const char *prefix)
{
    if(!name) name = d->name;
    if(name[0] && !duplicatename(d, name)) return name;
    static string cname[4];
    s_sprintf(cname[num])("%s%s \fs\f6(%d)\fr", prefix, name, d->clientnum);
    return cname[num];
}

char *colorping(int ping)
{
    static string cping;
    if(multiplayer(false)) s_sprintf(cping)("\fs\f%d%d\fr", ping <= 500 ? 0 : ping <= 1000 ? 2 : 3, ping);
    else s_sprintf(cping)("%d", ping);
    return cping;
}

void newname(const char *name)
{
    if(name[0])
    {
        c2sinit = false;
        filtertext(player1->name, name, 0, MAXNAMELEN);
        if(!player1->name[0]) s_strcpy(player1->name, "unarmed");
    }
    else conoutf("your name is: %s", player1->name);
    alias("curname", player1->name);
}

int smallerteam()
{
    int teamsize[2] = {0, 0};
    loopv(players) if(players[i]) teamsize[team_int(players[i]->team)]++;
    teamsize[team_int(player1->team)]++;
    if(teamsize[0] == teamsize[1]) return -1;
    return teamsize[0] < teamsize[1] ? 0 : 1;
}

void changeteam(int team, bool respawn) // force team and respawn
{
    c2sinit = false;
    if(m_flags) tryflagdrop(false);
    filtertext(player1->team, team_string(team), 0, MAXTEAMLEN);
    if(respawn) addmsg(SV_CHANGETEAM, "r");
}

void newteam(char *name)
{
    if(name[0])
    {
        if(m_teammode)
        {
            if(!strcmp(name, player1->team)) return; // same team
            if(!team_valid(name)) { conoutf("\f3\"%s\" is not a valid team name (try CLA or RVSF)", name); return; }

            bool checkteamsize =  autoteambalance && players.length() >= 1 && !m_botmode;
            int freeteam = smallerteam();

            if(team_valid(name))
            {
                int team = team_int(name);
                if(checkteamsize && team != freeteam)
                {
                    conoutf("\f3the %s team is already full", name);
                    return;
                }
                changeteam(team);
            }
            else changeteam(checkteamsize ? (uint)freeteam : rnd(2)); // random assignement
        }
    }
    else conoutf("your team is: %s", player1->team);
}

void newskin(int skin) { player1->nextskin = skin; }
int curteam() { return team_int(player1->team); }
int currole() { return player1->clientrole; }
int curmode() { return gamemode; }
void curmap(int cleaned)
{
    extern string smapname;
    result(cleaned ? behindpath(smapname) : smapname);
}

COMMANDN(team, newteam, ARG_1STR);
COMMANDN(name, newname, ARG_1STR);
COMMANDN(skin, newskin, ARG_1INT);
COMMAND(curteam, ARG_IVAL);
COMMAND(currole, ARG_IVAL);
COMMAND(curmode, ARG_IVAL);
COMMAND(curmap, ARG_1INT);
VARP(showscoresondeath, 0, 1, 1);

void deathstate(playerent *pl)
{
    pl->state = CS_DEAD;
    pl->spectatemode = SM_DEATHCAM;
    pl->respawnoffset = pl->lastpain = lastmillis;
    pl->move = pl->strafe = 0;
    pl->pitch = pl->roll = 0;
    pl->attacking = false;
    pl->weaponsel->onownerdies();

    if(pl == player1)
    {
        if(showscoresondeath) showscores(true);
        setscope(false);
        if(editmode) toggleedit(true);
    }
    else pl->resetinterp();
}

void spawnstate(playerent *d)              // reset player state not persistent accross spawns
{
    d->respawn();
    d->spawnstate(gamemode);
    if(d==player1)
    {
        if(player1->skin!=player1->nextskin) setskin(player1, player1->nextskin);
        setscope(false);
    }
}

playerent *newplayerent()                 // create a new blank player
{
    playerent *d = new playerent;
    d->lastupdate = totalmillis;
	setskin(d, rnd(6));
	weapon::equipplayer(d); // flowtron : avoid overwriting d->spawnstate(gamemode) stuff from the following line (this used to be called afterwards)
    spawnstate(d);
    return d;
}

botent *newbotent()                 // create a new blank player
{
    botent *d = new botent;
    d->lastupdate = totalmillis;
    setskin(d, rnd(6));
    spawnstate(d);
    weapon::equipplayer(d);
    loopv(players) if(i!=getclientnum() && !players[i])
    {
        players[i] = d;
        d->clientnum = i;
        return d;
    }
    if(players.length()==getclientnum()) players.add(NULL);
    d->clientnum = players.length();
    players.add(d);
    return d;
}

void freebotent(botent *d)
{
    loopv(players) if(players[i]==d)
    {
        DELETEP(players[i]);
        players.remove(i);
    }
}

void zapplayer(playerent *&d)
{
    DELETEP(d);
}

void movelocalplayer()
{
    if(player1->state==CS_DEAD && !player1->allowmove())
    {
        if(lastmillis-player1->lastpain<2000)
        {
	        player1->move = player1->strafe = 0;
	        moveplayer(player1, 10, false);
        }
    }
    else if(!intermission)
    {
        moveplayer(player1, 10, true);
        checkitems(player1);
    }
}

// use physics to extrapolate player position
VARP(smoothmove, 0, 75, 100);
VARP(smoothdist, 0, 8, 16);

void predictplayer(playerent *d, bool move)
{
    d->o = d->newpos;
    d->o.z += d->eyeheight;
    d->yaw = d->newyaw;
    d->pitch = d->newpitch;
    if(move)
    {
        moveplayer(d, 1, false);
        d->newpos = d->o;
        d->newpos.z -= d->eyeheight;
    }
    float k = 1.0f - float(lastmillis - d->smoothmillis)/smoothmove;
    if(k>0)
    {
        d->o.add(vec(d->deltapos).mul(k));
        d->yaw += d->deltayaw*k;
        if(d->yaw<0) d->yaw += 360;
        else if(d->yaw>=360) d->yaw -= 360;
        d->pitch += d->deltapitch*k;
    }
}

void moveotherplayers()
{
    loopv(players) if(players[i] && players[i]->type==ENT_PLAYER)
    {
        playerent *d = players[i];
        const int lagtime = totalmillis-d->lastupdate;
        if(!lagtime || intermission) continue;
        else if(lagtime>1000 && d->state==CS_ALIVE)
        {
            d->state = CS_LAGGED;
            continue;
        }
        if(d->state==CS_ALIVE || d->state==CS_EDITING)
        {
            if(smoothmove && d->smoothmillis>0) predictplayer(d, true);
            else moveplayer(d, 1, false);
        }
        else if(d->state==CS_DEAD && lastmillis-d->lastpain<2000) moveplayer(d, 1, true);
    }
}


bool showhudtimer(int maxsecs, int startmillis, const char *msg, bool flash)
{
    static string str = "";
    static int tickstart = 0, curticks = -1, maxticks = -1;
    int nextticks = (lastmillis - startmillis) / 200;
    if(tickstart!=startmillis || maxticks != 5*maxsecs)
    {
        tickstart = startmillis;
        maxticks = 5*maxsecs;
        curticks = -1;
        s_strcpy(str, "\f3");
    }
    if(curticks >= maxticks) return false;
    nextticks = min(nextticks, maxticks);
    while(curticks < nextticks)
    {
        if(++curticks%5) s_strcat(str, ".");
        else
        {
            s_sprintfd(sec)("%d", maxsecs - (curticks/5));
            s_strcat(str, sec);
        }
    }
    if(nextticks < maxticks) hudeditf(HUDMSG_TIMER|HUDMSG_OVERWRITE, flash ? str : str+2);
    else hudeditf(HUDMSG_TIMER, msg);
    return true;
}

int lastspawnattempt = 0;

void showrespawntimer()
{
    if(intermission) return;
    if(m_arena)
    {
        if(!arenaintermission) return;
        showhudtimer(5, arenaintermission, "FIGHT!", lastspawnattempt >= arenaintermission && lastmillis < lastspawnattempt+100);
    }
    else if(player1->state==CS_DEAD && (!player1->isspectating() || player1->spectatemode==SM_DEATHCAM))
    {
        int secs = m_flags ? 5 : 2;
        showhudtimer(secs, player1->respawnoffset, "READY!", lastspawnattempt >= arenaintermission && lastmillis < lastspawnattempt+100);
    }
}

struct scriptsleep { int wait; char *cmd; };
vector<scriptsleep> sleeps;

void addsleep(int msec, const char *cmd)
{
    scriptsleep &s = sleeps.add();
    s.wait = msec+lastmillis;
    s.cmd = newstring(cmd);
}

void addsleep_(char *msec, char *cmd)
{
    addsleep(atoi(msec), cmd);
}

void resetsleep()
{
    loopv(sleeps) DELETEA(sleeps[i].cmd);
    sleeps.setsize(0);
}

COMMANDN(sleep, addsleep_, ARG_2STR);

void updateworld(int curtime, int lastmillis)        // main game update loop
{
    // process command sleeps
	loopv(sleeps)
    {
        if(sleeps[i].wait && lastmillis > sleeps[i].wait)
        {
            char *cmd = sleeps[i].cmd;
            sleeps[i].cmd = NULL;
	        execute(cmd);
			delete[] cmd;
            if(sleeps.length() > i) sleeps.remove(i--);
        }
    }

    physicsframe();
    checkweaponswitch();
    checkakimbo();
    if(getclientnum()>=0) shoot(player1, worldpos);     // only shoot when connected to server
    movebounceents();
    moveotherplayers();
    gets2c();
    showrespawntimer();

    // Added by Rick: let bots think
    if(m_botmode) BotManager.Think();

    movelocalplayer();
    c2sinfo(player1);   // do this last, to reduce the effective frame lag
}

#define SECURESPAWNDIST 15
int spawncycle = -1;
int fixspawn = 2;

// returns -1 for a free place, else dist to the nearest enemy
float nearestenemy(vec place, char *team)
{
    float nearestenemydist = -1;
    loopv(players)
    {
        playerent *other = players[i];
        if(!other || isteam(team, other->team)) continue;
        float dist = place.dist(other->o);
        if(dist < nearestenemydist || nearestenemydist == -1) nearestenemydist = dist;
    }
    if(nearestenemydist >= SECURESPAWNDIST || nearestenemydist < 0) return -1;
    else return nearestenemydist;
}

void findplayerstart(playerent *d, bool mapcenter, int arenaspawn)
{
    int r = fixspawn-->0 ? 4 : rnd(10)+1;
    entity *e = NULL;
    if(!mapcenter)
    {
        int type = m_teammode ? team_int(d->team) : 100;
        if(m_arena && arenaspawn >= 0)
        {
            int x = -1;
            loopi(arenaspawn + 1) x = findentity(PLAYERSTART, x+1, type);
            if(x >= 0) e = &ents[x];
        }
        else if((m_teammode || m_arena) && !m_ktf) // ktf uses ffa spawns
        {
            loopi(r) spawncycle = findentity(PLAYERSTART, spawncycle+1, type);
            if(spawncycle >= 0) e = &ents[spawncycle];
        }
        else
        {
            float bestdist = -1;

            loopi(r)
            {
                spawncycle = m_ktf && numspawn[2] > 5 ? findentity(PLAYERSTART, spawncycle+1, 100) : findentity(PLAYERSTART, spawncycle+1);
                if(spawncycle < 0) continue;
                float dist = nearestenemy(vec(ents[spawncycle].x, ents[spawncycle].y, ents[spawncycle].z), d->team);
                if(!e || dist < 0 || (bestdist >= 0 && dist > bestdist)) { e = &ents[spawncycle]; bestdist = dist; }
            }
        }
    }

    if(e)
    {
        d->o.x = e->x;
        d->o.y = e->y;
        d->o.z = e->z;
        d->yaw = e->attr1;
        d->pitch = 0;
        d->roll = 0;
    }
    else
    {
        d->o.x = d->o.y = (float)ssize/2;
        d->o.z = 4;
    }

    entinmap(d);
}

void spawnplayer(playerent *d)
{
    d->respawn();
    d->spawnstate(gamemode);
    d->state = d==player1 && editmode ? CS_EDITING : CS_ALIVE;
    findplayerstart(d);
}

void respawnself()
{
    if(m_mp(gamemode)) addmsg(SV_TRYSPAWN, "r");
    else
    {
        showscores(false);
        setscope(false);
	    spawnplayer(player1);
        player1->lifesequence++;
        player1->weaponswitch(player1->primweap);
        player1->weaponchanging -= weapon::weaponchangetime/2;
    }
}

bool tryrespawn()
{
    if(player1->state==CS_DEAD)
    {
        // set min wait time when in spectate mode (avoid unfair specting)
        if(player1->isspectating() && player1->spectatemode!=SM_DEATHCAM && !m_arena)
        {
            spectate(SM_DEATHCAM);
            player1->respawnoffset = lastmillis; // count again
        }

        int respawnmillis = player1->respawnoffset+(m_arena ? 0 : (m_flags ? 5000 : 2000));
        if(lastmillis>respawnmillis)
        {
            player1->attacking = false;
            if(m_arena)
            {
                if(!arenaintermission) hudeditf(HUDMSG_TIMER, "waiting for new round to start...");
                else lastspawnattempt = lastmillis;
                return false;
            }
            respawnself();
            return true;
        }
        else lastspawnattempt = lastmillis;
        //else hudonlyf("wait %3.1f seconds to respawn", (respawnmillis-lastmillis)/(float)1000);
    }
    return false;
}

// damage arriving from the network, monsters, yourself, all ends up here.

void dodamage(int damage, playerent *pl, playerent *actor, bool gib, bool local)
{
    if(pl->state != CS_ALIVE || intermission) return;

    pl->respawnoffset = pl->lastpain = lastmillis;
    if(local) damage = pl->dodamage(damage);
    else if(actor==player1) return;

    if(pl==player1)
    {
        updatedmgindicator(actor->o);
        pl->damageroll(damage);
    }
    else damageeffect(damage, pl);

    if(pl->health<=0) { if(local) dokill(pl, actor, gib); }
    else if(pl==player1) playsound(S_PAIN6, SP_HIGH);
    else playsound(S_PAIN1+rnd(5), pl);
}

void dokill(playerent *pl, playerent *act, bool gib)
{
    if(pl->state!=CS_ALIVE || intermission) return;

    string pname, aname, death;
    s_strcpy(pname, pl==player1 ? "you" : colorname(pl));
    s_strcpy(aname, act==player1 ? "you" : colorname(act));
    s_strcpy(death, gib ? "gibbed" : "fragged");
    void (*outf)(const char *s, ...) = (pl == player1 || act == player1) ? hudoutf : conoutf;

    if(pl==act)
        outf("\f2%s suicided%s", pname, pl==player1 ? "!" : "");
    else if(isteam(pl->team, act->team))
    {
        if(pl==player1) outf("\f2you got %s by teammate %s", death, aname);
        else outf("%s%s %s teammate %s", act==player1 ? "\f3" : "\f2", aname, death, pname);
    }
    else
    {
        if(pl==player1) outf("\f2you got %s by %s", death, aname);
        else outf("\f2%s %s %s", aname, death, pname);
    }

    if(gib)
    {
        if(pl!=act && act->weaponsel->type == GUN_SNIPER) playsound(S_HEADSHOT, SP_LOW);
        addgib(pl);
    }

    if(!m_mp(gamemode))
    {
        if(pl==act || isteam(pl->team, act->team)) act->frags--;
        else act->frags += gib ? 2 : 1;
    }

    deathstate(pl);
    pl->deaths++;
    playsound(S_DIE1+rnd(2), pl);
}

VAR(minutesremaining, 1, 0, 0);

void timeupdate(int timeremain)
{
    minutesremaining = timeremain;
    if(!timeremain)
    {
        intermission = true;
        player1->attacking = false;
        conoutf("intermission:");
        conoutf("game has ended!");
        consolescores();
        showscores(true);
		if(identexists("start_intermission")) execute("start_intermission");
    }
    else
    {
        conoutf("time remaining: %d minutes", timeremain);
        if(timeremain==1)
        {
            musicsuggest(M_LASTMINUTE1 + rnd(2), 70*1000, true);
            hudoutf("1 minute left!");
        }
    }
}

playerent *newclient(int cn)   // ensure valid entity
{
    if(cn<0 || cn>=MAXCLIENTS)
    {
        neterr("clientnum");
        return NULL;
    }
    while(cn>=players.length()) players.add(NULL);
    playerent *d = players[cn];
    if(d) return d;
    d = newplayerent();
    players[cn] = d;
    d->clientnum = cn;
    return d;
}

playerent *getclient(int cn)   // ensure valid entity
{
    return players.inrange(cn) ? players[cn] : NULL;
}

void initclient()
{
    clientmap[0] = 0;
    newname("unarmed");
    changeteam(rnd(2), false);
}

entity flagdummies[2]; // in case the map does not provide flags

void preparectf(bool cleanonly=false)
{
    loopi(2) flaginfos[i].flagent = &flagdummies[i];
    if(!cleanonly)
    {
        loopi(2)
        {
            flaginfo &f = flaginfos[i];
            f.ack = true;
            f.actor = NULL;
            f.actor_cn = -1;
            f.team = i;
            f.state = m_ktf ? CTFF_IDLE : CTFF_INBASE;
        }
        loopv(ents)
        {
            entity &e = ents[i];
            if(e.type==CTF_FLAG)
            {
                e.spawned = true;
                if(e.attr2>2) { conoutf("\f3invalid ctf-flag entity (%i)", i); e.attr2 = 0; }
                flaginfo &f = flaginfos[e.attr2];
                f.flagent = &e;
                f.originalpos.x = (float) e.x;
                f.originalpos.y = (float) e.y;
                f.originalpos.z = (float) e.z;
            }
        }
    }
}

struct gmdesc { int mode; char *desc; };
vector<gmdesc> gmdescs;

void gamemodedesc(char *modenr, char *desc)
{
    if(!modenr || !desc) return;
    struct gmdesc &gd = gmdescs.add();
    gd.mode = atoi(modenr);
    gd.desc = newstring(desc);
}

COMMAND(gamemodedesc, ARG_2STR);

void resetmap()
{
    //resetsleep();
    clearminimap();
    cleardynlights();
    pruneundos();
    clearworldsounds();
    particlereset();
    setvar("gamespeed", 100);
    setvar("paused", 0);
    setvar("fog", 180);
    setvar("fogcolour", 0x8099B3);
    setvar("shadowyaw", 45);
}

int suicided = -1;

void startmap(const char *name, bool reset)   // called just after a map load
{
    s_strcpy(clientmap, name);
    senditemstoserver = true;
    // Added by Rick
	if(m_botmode) BotManager.BeginMap(name);
    else kickallbots();
    // End add by Rick
    clearbounceents();
    resetspawns();
    preparectf(!m_flags);
    suicided = -1;
    spawncycle = -1;
    if(m_valid(gamemode) && !m_mp(gamemode)) respawnself();
    else findplayerstart(player1);

    if(!reset) return;

    player1->frags = player1->flagscore = player1->deaths = player1->lifesequence = 0;
    loopv(players) if(players[i]) players[i]->frags = players[i]->flagscore = players[i]->deaths = players[i]->lifesequence = 0;
    if(editmode) toggleedit(true);
    showscores(false);
    intermission = false;
    minutesremaining = -1;
    arenaintermission = 0;
    bool noflags = (m_ctf || m_ktf) && (!numflagspawn[0] || !numflagspawn[1]);
    if(*clientmap) conoutf("game mode is \"%s\"%s", modestr(gamemode, modeacronyms > 0), noflags ? " - \f2but there are no flag bases on this map" : "");
    if(multiplayer(false) || m_botmode)
    {
        loopv(gmdescs) if(gmdescs[i].mode == gamemode)
        {
            s_sprintfd(desc)("\f1%s", gmdescs[i].desc);
            conoutf(desc);
        }
    }

    // run once
    if(firstrun)
    {
        persistidents = false;
        execfile("config/firstrun.cfg");
        persistidents = true;
        firstrun = false;
    }
    // execute mapstart event
    const char *mapstartonce = getalias("mapstartonce");
    if(mapstartonce && mapstartonce[0])
    {
        addsleep(0, mapstartonce); // do this as a sleep to make sure map changes don't recurse inside a welcome packet
        alias("mapstartonce", "");
    }
}

void suicide()
{
    if(player1->state == CS_ALIVE && suicided!=player1->lifesequence)
    {
        addmsg(SV_SUICIDE, "r");
        suicided = player1->lifesequence;
    }
}

COMMAND(suicide, ARG_NONE);

// console and audio feedback

void flagmsg(int flag, int message, int actor, int flagtime)
{
    playerent *act = getclient(actor);
    if(actor != getclientnum() && !act && message != FM_RESET) return;
    bool own = flag == team_int(player1->team);
    bool firstperson = actor == getclientnum();
    bool teammate = !act ? true : isteam(player1->team, act->team);
    const char *teamstr = m_ktf ? "the" : own ? "your" : "the enemy";

    switch(message)
    {
        case FM_PICKUP:
            playsound(S_FLAGPICKUP, SP_HIGHEST);
            if(firstperson)
            {
                hudoutf("\f2you got the %sflag", m_ctf ? "enemy " : "");
                musicsuggest(M_FLAGGRAB, m_ctf ? 90*1000 : 900*1000, true);
            }
            else hudoutf("\f2%s got %s flag", colorname(act), teamstr);
            break;
        case FM_LOST:
        case FM_DROP:
        {
            const char *droplost = message == FM_LOST ? "lost" : "dropped";
            playsound(S_FLAGDROP, SP_HIGHEST);
            if(firstperson)
            {
                hudoutf("\f2you %s the flag", droplost);
                musicfadeout(M_FLAGGRAB);
            }
            else hudoutf("\f2%s %s %s flag", colorname(act), droplost, teamstr);
            break;
        }
        case FM_RETURN:
            playsound(S_FLAGRETURN, SP_HIGHEST);
            if(firstperson) hudoutf("\f2you returned your flag");
            else hudoutf("\f2%s returned %s flag", colorname(act), teamstr);
            break;
        case FM_SCORE:
            playsound(S_FLAGSCORE, SP_HIGHEST);
            if(firstperson)
            {
                hudoutf("\f2you scored");
                if(m_ctf) musicfadeout(M_FLAGGRAB);
            }
            else hudoutf("\f2%s scored for %s team", colorname(act), teammate ? "your" : "the enemy");
            break;
        case FM_KTFSCORE:
        {
            playsound(S_VOTEPASS, SP_HIGHEST); // need better ktf sound here
            const char *ta = firstperson ? "you have" : colorname(act);
            const char *tb = firstperson ? "" : " has";
            const char *tc = teammate && !firstperson ? "your teammate " : "";
            int m = flagtime / 60;
            if(m)
                hudoutf("\f2%s%s%s been keeping the flag for %d minute%s %d seconds now", tc, ta, tb, m, m == 1 ? "" : "s", flagtime % 60);
            else
                hudoutf("\f2%s%s%s been keeping the flag for %d seconds now", tc, ta, tb, flagtime);
            break;
        }
        case FM_SCOREFAIL: // sound?
            hudoutf("\f2%s failed to score (own team flag not taken)", firstperson ? "you" : colorname(act));
            break;
        case FM_RESET:
            playsound(S_FLAGRETURN, SP_HIGHEST);
            hudoutf("the server reset the flag");
            if(firstperson) musicfadeout(M_FLAGGRAB);
            break;
    }
}

void dropflag() { tryflagdrop(true); }
COMMAND(dropflag, ARG_NONE);

char *votestring(int type, char *arg1, char *arg2)
{
    const char *msgs[] = { "kick player %s", "ban player %s", "remove all bans", "set mastermode to %s", "%s autoteam", "force player %s to the enemy team", "give admin to player %s", "load map %s in mode %s", "%s demo recording for the next match", "stop demo recording", "clear all demos", "set server description to '%s'", "shuffle teams"};
    const char *msg = msgs[type];
    char *out = newstring(_MAXDEFSTR);
    out[_MAXDEFSTR] = '\0';
    switch(type)
    {
        case SA_KICK:
        case SA_BAN:
        case SA_FORCETEAM:
        case SA_GIVEADMIN:
        {
            int cn = atoi(arg1);
            playerent *p = (cn == getclientnum() ? player1 : getclient(cn));
            if(!p) break;
            s_sprintf(out)(msg, colorname(p));
            break;
        }
        case SA_MASTERMODE:
            s_sprintf(out)(msg, atoi(arg1) == 0 ? "Open" : "Private");
            break;
        case SA_AUTOTEAM:
        case SA_RECORDDEMO:
            s_sprintf(out)(msg, atoi(arg1) == 0 ? "disable" : "enable");
            break;
        case SA_MAP:
            s_sprintf(out)(msg, arg1, modestr(atoi(arg2), modeacronyms > 0));
            break;
        case SA_SERVERDESC:
            s_sprintf(out)(msg, arg1);
            break;
        default:
            s_sprintf(out)(msg, arg1, arg2);
            break;
    }
    return out;
}

votedisplayinfo *newvotedisplayinfo(playerent *owner, int type, char *arg1, char *arg2)
{
    if(type < 0 || type >= SA_NUM) return NULL;
    votedisplayinfo *v = new votedisplayinfo();
    v->owner = owner;
    v->type = type;
    v->millis = totalmillis + (30+10)*1000;
    char *votedesc = votestring(type, arg1, arg2);
    s_strcpy(v->desc, votedesc);
    DELETEA(votedesc);
    return v;
}

votedisplayinfo *curvote = NULL, *calledvote = NULL;

void callvote(int type, char *arg1, char *arg2)
{
    if(calledvote) return;
    votedisplayinfo *v = newvotedisplayinfo(player1, type, arg1, arg2);
    if(v)
    {
        calledvote = v;
        ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        ucharbuf p(packet->data, packet->dataLength);
        putint(p, SV_CALLVOTE);
        putint(p, v->type);
        switch(v->type)
        {
            case SA_MAP:
                sendstring(arg1, p);
                putint(p, nextmode);
                break;
            case SA_SERVERDESC:
                sendstring(arg1, p);
                break;
            case SA_STOPDEMO:
            case SA_REMBANS:
            case SA_SHUFFLETEAMS:
                break;
            default:
                putint(p, atoi(arg1));
                break;
        }
        enet_packet_resize(packet, p.length());
        sendpackettoserv(1, packet);
    }
    else conoutf("\f3invalid vote");
}

void scallvote(char *type, char *arg1, char *arg2)
{
    if(type)
    {
        int t = atoi(type);
        if(t==SA_MAP) // FIXME
        {
            string n;
            itoa(n, nextmode);
            callvote(t, arg1, n);
        }
        else callvote(t, arg1, arg2);
    }
}

void vote(int v)
{
    if(!curvote || v < 0 || v >= VOTE_NUM) return;
    if(curvote->localplayervoted) { conoutf("\f3you voted already"); return; }
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_VOTE);
    putint(p, v);
    enet_packet_resize(packet, p.length());
    sendpackettoserv(1, packet);
	if(!curvote) { /*printf(":: curvote vanished!\n");*/ return; } // flowtron - happens when I call "/stopdemo"! .. seems the map-load happens in-between
    curvote->stats[v]++;
    curvote->localplayervoted = true;
}

void displayvote(votedisplayinfo *v)
{
    if(!v) return;
    DELETEP(curvote);
    curvote = v;
    conoutf("%s called a vote: %s", v->owner ? colorname(v->owner) : "", curvote->desc);
    playsound(S_CALLVOTE, SP_HIGHEST);
    curvote->localplayervoted = false;
}

void callvotesuc()
{
    if(!calledvote) return;
    displayvote(calledvote);
    calledvote = NULL;
    vote(VOTE_YES); // not automatically done by callvote to keep a clear sequence
}

void callvoteerr(int e)
{
    if(e < 0 || e >= VOTEE_NUM) return;
    conoutf("\f3could not vote: %s", voteerrorstr(e));
    DELETEP(calledvote);
}

void votecount(int v) { if(curvote && v >= 0 && v < VOTE_NUM) curvote->stats[v]++; }
void voteresult(int v)
{
    if(curvote && v >= 0 && v < VOTE_NUM)
    {
        curvote->result = v;
        curvote->millis = totalmillis + 5000;
        conoutf("vote %s", v == VOTE_YES ? "passed" : "failed");
        if(multiplayer(false)) playsound(v == VOTE_YES ? S_VOTEPASS : S_VOTEFAIL, SP_HIGH);
    }
}

void clearvote() { DELETEP(curvote); DELETEP(calledvote); }

COMMANDN(callvote, scallvote, ARG_3STR); //fixme,ah
COMMAND(vote, ARG_1INT);

void whois(int cn)
{
    addmsg(SV_WHOIS, "ri", cn);
}

COMMAND(whois, ARG_1INT);

int sessionid = 0;

void setadmin(char *claim, char *password)
{
    if(!claim || !password) return;
    else addmsg(SV_SETADMIN, "ris", atoi(claim), genpwdhash(player1->name, password, sessionid));
}

COMMAND(setadmin, ARG_2STR);

void changemap(const char *name)                      // silently request map change, server may ignore
{
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_NEXTMAP);
    sendstring(name, p);
    putint(p, nextmode);
    enet_packet_resize(packet, p.length());
    sendpackettoserv(1, packet);
}

struct mline { string name, cmd; };
static vector<mline> mlines;

void *kickmenu = NULL, *banmenu = NULL, *forceteammenu = NULL, *giveadminmenu = NULL;

void refreshsopmenu(void *menu, bool init)
{
    menureset(menu);
    mlines.setsize(0);
    loopv(players) if(players[i])
    {
        mline &m = mlines.add();
        s_strcpy(m.name, colorname(players[i]));
        s_sprintf(m.cmd)("%s %d", menu==kickmenu ? "kick" : (menu==banmenu ? "ban" : (menu==forceteammenu ? "forceteam" : "giveadmin")), i);
        menumanual(menu, m.name, m.cmd);
    }
}

extern bool watchingdemo;

playerent *updatefollowplayer(int shiftdirection)
{
    if(!shiftdirection)
    {
        playerent *f = players.inrange(player1->followplayercn) ? players[player1->followplayercn] : NULL;
        if(f && (watchingdemo || !f->isspectating())) return f;
    }

    vector<playerent *> available;
    loopv(players) if(players[i])
    {
        if(m_teammode && !isteam(players[i]->team, player1->team)) continue;
        if(players[i]->state==CS_DEAD || players[i]->isspectating()) continue;
        available.add(players[i]);
    }
    if(!available.length()) return NULL;

    int oldidx = -1;
    if(players.inrange(player1->followplayercn)) oldidx = available.find(players[player1->followplayercn]);
    if(oldidx<0) oldidx = 0;
    int idx = (oldidx+shiftdirection) % available.length();
    if(idx<0) idx += available.length();

    player1->followplayercn = available[idx]->clientnum;
    return players[player1->followplayercn];
}

void spectate(int mode) // set new spect mode
{
    if(!player1->isspectating()) return;
    if(mode == player1->spectatemode) return;
    showscores(false);
    switch(mode)
    {
        case SM_FOLLOW1ST:
        case SM_FOLLOW3RD:
        case SM_FOLLOW3RD_TRANSPARENT:
        {
            if(players.length() && updatefollowplayer()) break;
            else mode = SM_FLY;
        }
        case SM_FLY:
        {
            if(player1->spectatemode != SM_FLY)
            {
                // set spectator location to last followed player
                playerent *f = updatefollowplayer();
                if(f)
                {
                    player1->o = f->o;
                    player1->yaw = f->yaw;
                    player1->pitch = 0.0f;
                    player1->resetinterp();
                }
                else entinmap(player1); // or drop 'em at a random place
            }
            break;
        }
        default: break;
    }
    player1->spectatemode = mode;
}


void togglespect() // cycle through all spectating modes
{
    if(m_botmode) spectate(SM_FLY);
    else
    {
        int mode;
        if(player1->spectatemode==SM_NONE) mode = SM_FOLLOW1ST; // start with 1st person spect
        else mode = SM_FOLLOW1ST + ((player1->spectatemode - SM_FOLLOW1ST + 1) % (SM_NUM-SM_FOLLOW1ST));
        spectate(mode);
    }
}

void changefollowplayer(int shift)
{
    updatefollowplayer(shift);
}

COMMAND(spectate, ARG_1INT);
COMMAND(togglespect, ARG_NONE);
COMMAND(changefollowplayer, ARG_1INT);

int isalive() { return player1->state==CS_ALIVE ? 1 : 0; }
COMMANDN(alive, isalive, ARG_NONE);

void serverextension(char *ext, char *args)
{
    if(!ext || !ext[0]) return;
    size_t n = args ? strlen(args)+1 : 0;
    if(n>0) addmsg(SV_EXTENSION, "rsis", ext, n, args);
    else addmsg(SV_EXTENSION, "rsi", ext, n);
}

COMMAND(serverextension, ARG_2STR);

