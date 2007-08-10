// clientgame.cpp: core game related stuff

#include "cube.h"
#include "bot/bot.h"

int nextmode = 0;         // nextmode becomes gamemode after next map load
VAR(gamemode, 1, 0, 0);

flaginfo flaginfos[2];

void mode(int n) 
{
	if(m_mp(n) || !multiplayer()) addmsg(SV_GAMEMODE, "ri", nextmode = n); 
}
COMMAND(mode, ARG_1INT);

bool intermission = false;
bool autoteambalance = false;

playerent *player1 = newplayerent();          // our client
vector<playerent *> players;                        // other clients

int lastmillis = 0;
int curtime;
string clientmap;

extern int framesinmap;

char *getclientmap() { return clientmap; }

extern bool c2sinit, senditemstoserver;
extern int dblend;

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
    loopv(players) if(players[i] && d!=players[i] && !strcmp(name, players[i]->name)) return true;
    return false;
}

char *colorname(playerent *d, int num, char *name, char *prefix)
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

void newname(char *name) 
{
    if(name[0])
    {
        c2sinit = false; 
        filtertext(player1->name, name, false, MAXNAMELEN);
        if(!player1->name[0]) s_strcpy(player1->name, "unarmed");
    }
    else conoutf("your name is: %s", player1->name);
}
    
int smallerteam()
{
    int teamsize[2] = {0, 0};
    loopv(players) if(players[i]) teamsize[team_int(players[i]->team)]++;
    teamsize[team_int(player1->team)]++;
    if(teamsize[0] == teamsize[1]) return -1;
    return teamsize[0] < teamsize[1] ? 0 : 1;
}
    
void changeteam(int team, bool forced) // force team and respawn
{
    c2sinit = false;
    if(m_ctf) tryflagdrop(NULL);
    filtertext(player1->team, team_string(team), false, MAXTEAMLEN);
    if(!forced) deathstate(player1);
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

COMMANDN(team, newteam, ARG_1STR);
COMMANDN(name, newname, ARG_1STR);
COMMANDN(skin, newskin, ARG_1INT);

extern void thrownade(playerent *d, const vec &vel, bounceent *p);

void deathstate(playerent *pl)
{
	pl->lastpain = lastmillis;
    pl->attacking = false;
    pl->state = CS_DEAD;
    pl->pitch = 0;
    pl->roll = 60;
	pl->strafe = 0;
    if(pl == player1)
    {
	    dblend = 0;
	    if(pl->inhandnade) thrownade(pl, vec(0,0,0), pl->inhandnade);
    }
}

void spawnstate(playerent *d)              // reset player state not persistent accross spawns
{   
    d->respawn();
    d->spawnstate(gamemode);
    //d->lastaction = lastmillis;
    if(d==player1)
    {
        if(player1->skin!=player1->nextskin) setskin(player1, player1->nextskin);
        setscope(false);
    }
}
    
playerent *newplayerent()                 // create a new blank player
{
    playerent *d = new playerent;
    d->lastupdate = lastmillis;
	setskin(d, rnd(6));
    spawnstate(d);
    return d;
}

botent *newbotent()                 // create a new blank player
{
    botent *d = new botent;
    d->lastupdate = lastmillis;
    setskin(d, rnd(6));
    spawnstate(d);
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
    }
}

void respawnself()
{
    if(m_mp(gamemode)) addmsg(SV_TRYSPAWN, "r");
    else
    {
	    spawnplayer(player1);
	    showscores(false);
    }
}

void respawn()
{
	if(player1->state==CS_DEAD && lastmillis>player1->lastpain+(m_ctf ? 5000 : 2000))
    { 
        player1->attacking = false;
        if(m_arena) { conoutf("waiting for new round to start..."); return; }
        respawnself();
		weaponswitch(player1->primary);
		player1->lastaction -= WEAPONCHANGE_TIME/2;
    }
}

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
}

void zapplayer(playerent *&d)
{
    removebounceents(d);
    removeprojectiles(d);
    DELETEP(d);
}

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
        moveplayer(players[i], 2, false);   // use physics to extrapolate player position
    }
}

struct scriptsleep { int wait; char *cmd; };
vector<scriptsleep> sleeps;

void addsleep(char *msec, char *cmd) 
{ 
    scriptsleep &s = sleeps.add();
    s.wait = atoi(msec)+lastmillis;
    s.cmd = newstring(cmd);
}

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
        }
    }
    physicsframe();
    checkakimbo();
    checkweaponswitch();
	//if(m_arena) arenarespawn();
	//arenarespawn();
    moveprojectiles((float)curtime);
    if(getclientnum()>=0) shoot(player1, worldpos);     // only shoot when connected to server
    gets2c();           // do this first, so we have most accurate information when our player moves
    movebounceents();
    otherplayers();
    //monsterthink();
    
    // Added by Rick: let bots think
    if(m_botmode) BotManager.Think();            
    
    //put game mode extra call here
    if(player1->state==CS_DEAD)
    {
        if(lastmillis-player1->lastpain<2000)
        {
	        player1->move = player1->strafe = 0;
	        moveplayer(player1, 10, false);
        }
    }
    else if(!intermission)
    {
        moveplayer(player1, 20, true);
        checkitems(player1);
    }
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

void findplayerstart(playerent *d)
{
    int r = fixspawn-->0 ? 4 : rnd(10)+1;
    entity *e = NULL;
    if(m_teammode || m_arena)
    {
        int type = m_teammode ? team_int(d->team) : 100;
        loopi(r) spawncycle = findentity(PLAYERSTART, spawncycle+1, type);
        if(spawncycle >= 0) e = &ents[spawncycle];
    }
    else
    {
        float bestdist = -1;

        loopi(r)
        {
            spawncycle = findentity(PLAYERSTART, spawncycle+1);
            if(spawncycle < 0) continue;
            float dist = nearestenemy(vec(ents[spawncycle].x, ents[spawncycle].y, ents[spawncycle].z), d->team);
            if(!e || dist < 0 || (bestdist >= 0 && dist > bestdist)) { e = &ents[spawncycle]; bestdist = dist; }
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
    findplayerstart(d);
    d->respawn();
    d->spawnstate(gamemode);
    d->state = d==player1 && editmode ? CS_EDITING : CS_ALIVE;
}

void showteamkill() { player1->lastteamkill = lastmillis; }

// damage arriving from the network, monsters, yourself, all ends up here.

void dodamage(int damage, playerent *pl, playerent *actor, bool gib, bool local)
{   
    if(pl->state!=CS_ALIVE || intermission) return;

    pl->lastpain = lastmillis;
    if(local) damage = pl->dodamage(damage);
    else if(actor==player1) return;

    if(pl==player1)
    {
        damageblend(damage);
        pl->damageroll(damage);
    }
    else damageeffect(damage, pl);

    if(pl->health<=0) { if(local) dokill(pl, actor, gib); }
    else if(pl==player1) playsound(S_PAIN6);
    else playsound(S_PAIN1+rnd(5), &pl->o);

#if 0
    if((pl->health -= damage)<=0)
    {
        s_sprintfd(death)("%s", gib ? "gibbed" : "fragged");
        if(pl->type==ENT_BOT)
        {
            if(pl==act) 
            { 
                --pl->frags; 
                conoutf("\f2%s suicided", colorname(pl)); 
            }
            else if(isteam(pl->team, act->team))
            {
                --act->frags; 
                conoutf("\f2%s %s %s teammate (%s)", act==player1 ? "you" : colorname(act, 0), death, act==player1 ? "a" : "his", colorname(pl, 1));
                if(act==player1) showteamkill();
            }
            else
            {
                act->frags += gib ? 2 : 1;
                conoutf("\f2%s %s %s", act==player1 ? "you" : colorname(act, 0), death, colorname(pl, 1));
            }
        }
        else if(act==pl)
        {
            actor = getclientnum();
            conoutf("\f2you suicided!");
            addmsg(SV_FRAGS, "ri", --pl->frags);
        }
        else if(act)
        {
            if(isteam(act->team, player1->team)) conoutf("\f2you got %s by a teammate (%s)", death, colorname(act));
            else conoutf("\f2you got %s by %s", death, colorname(act));
        }
        if(pl==player1) 
        {
            showscores(true);
            setscope(false);
            addmsg(gib ? SV_GIBDIED : SV_DIED, "ri", actor);
            if(m_ctf) tryflagdrop(act && isteam(act->team, player1->team));
        }
        deathstate(pl);
        pl->lifesequence++;
        playsound(S_DIE1+rnd(2), pl!=player1 ? &pl->o : NULL);
        if(act && act->gunselect == GUN_SNIPER && gib) playsound(S_HEADSHOT);
        if(gib) addgib(pl);
        if(pl!=player1 || act->type==ENT_BOT) act->frags += gib ? 2 : 1;
    }
    else
    {
        playsound(S_PAIN6, pl!=player1 ? &pl->o : NULL);
    }
#endif
}

void dokill(playerent *pl, playerent *act, bool gib)
{
    if(pl->state!=CS_ALIVE || intermission) return;

    string pname, aname, death;
    s_strcpy(pname, pl==player1 ? "you" : colorname(pl));
    s_strcpy(aname, act==player1 ? "you" : colorname(act));
    s_strcpy(death, gib ? "gibbed" : "fragged");

    if(pl==act)
        conoutf("\f2%s suicided%s", pname, pl==player1 ? "!" : "");
    else if(isteam(pl->team, act->team))
    {
        if(pl==player1) conoutf("\f2you got %s by a teammate (%s)", death, aname);
        else conoutf("\f2%s %s %s teammate (%s)", aname, death, act==player1 ? "a" : "his", pname);
        if(act==player1) showteamkill();
    }
    else
    {
        if(pl==player1) conoutf("\f2you got %s by %s", death, aname);
        else conoutf("\f2%s %s %s", aname, death, pname);
    }

    pl->state = CS_DEAD;
    pl->lastaction = lastmillis;
    pl->pitch = 0;
    pl->roll = 60;
    pl->strafe = 0;
    pl->attacking = false;
    pl->lifesequence++;
    playsound(S_DIE1+rnd(2), pl!=player1 ? &pl->o : NULL);
    if(pl==player1)
    {
        showscores(true);
        setscope(false);
        dblend = 0;
        if(pl->inhandnade) thrownade(pl, vec(0,0,0), pl->inhandnade);
        if(m_ctf) tryflagdrop(pl!=act && isteam(act->team, pl->team));
    }
    if(gib)
    {
        if(pl!=act && act->gunselect == GUN_SNIPER) playsound(S_HEADSHOT);
        addgib(pl);
    }
    if(!m_mp(gamemode))
    {
        if(pl==act || isteam(pl->team, act->team)) act->frags--;
        else act->frags += gib ? 2 : 1;
    }
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
        showscores(true);
		execute("start_intermission");
    }
    else
    {
        conoutf("time remaining: %d minutes", timeremain);
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
    changeteam(rnd(2));
}

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
                if(e.attr2>2) { conoutf("\f3invalid ctf-flag entity (%i)", i); e.attr2 = 0; }
                flaginfo &f = flaginfos[e.attr2];
                f.ack = false;
                f.actor = NULL;
                f.actor_cn = -1;
				f.team = e.attr2;
                f.flag = &e;
                f.state = CTFF_INBASE;
                f.originalpos.x = (float) e.x;
                f.originalpos.y = (float) e.y;
                f.originalpos.z = (float) e.z;
            }
        }
    }
}

int suicided = -1;

void startmap(char *name)   // called just after a map load
{
    clearminimap();
    //if(netmapstart()) { gamemode = 0;}  //needs fixed to switch modes?
    senditemstoserver = true;
    //monsterclear();
    // Added by Rick
	if(m_botmode) BotManager.BeginMap(name);
    else kickallbots();
    // End add by Rick            
    projreset();
    clearbounceents();
    resetspawns();
    if(m_ctf) preparectf();
    particlereset();
    suicided = -1;
    spawncycle = -1;
    if(!m_mp(gamemode)) spawnplayer(player1);
    else findplayerstart(player1);
    player1->frags = player1->flagscore = 0;
    loopv(players) if(players[i]) players[i]->frags = players[i]->flagscore = players[i]->lifesequence = 0;
    s_strcpy(clientmap, name);
    if(editmode) toggleedit();
    setvar("gamespeed", 100);
    setvar("fog", 180);
    setvar("fogcolour", 0x8099B3);
    showscores(false);
    intermission = false;
    minutesremaining = -1;
    if(*clientmap) conoutf("game mode is \"%s\"", modestr(gamemode));
}

COMMANDN(map, changemap, ARG_1STR);

void suicide()
{
	if(player1->state!=CS_ALIVE) return;
    if(!m_mp(gamemode)) dokill(player1, player1);
    else if(suicided!=player1->lifesequence)
    {
        addmsg(SV_SUICIDE, "r");
        suicided = player1->lifesequence;
    }
}

COMMAND(suicide, ARG_NONE);

// console and audio feedback

void flagmsg(int flag, int action) 
{
    flaginfo &f = flaginfos[flag];
    if(!f.actor || !f.ack) return;
    bool own = flag == team_int(player1->team), firstperson = f.actor == player1;
    const char *teamstr = flag == team_int(player1->team) ? "your" : "the enemy";

    switch(action)
    {
        case SV_FLAGPICKUP:
        {
            playsound(S_FLAGPICKUP);
            if(firstperson) conoutf("\f2you got the enemy flag");
            else conoutf("\f2%s got %s flag", colorname(f.actor), teamstr);
            break;
        }
        case SV_FLAGDROP:
        {
            playsound(S_FLAGDROP);
            if(firstperson) conoutf("\f2you lost the flag");
            else conoutf("\f2%s lost %s flag", colorname(f.actor), teamstr);
            break;
        }
        case SV_FLAGRETURN:
        {
            playsound(S_FLAGRETURN);
            if(firstperson) conoutf("\f2you returned your flag");
            else conoutf("\f2%s returned %s flag", colorname(f.actor), teamstr);
            break;
        }
        case SV_FLAGSCORE:
        {
            playsound(S_FLAGSCORE);
            if(firstperson) 
            {
                conoutf("\f2you scored");
                addmsg(SV_FLAGS, "ri", ++player1->flagscore);
            }
            else conoutf("\f2%s scored for %s team", colorname(f.actor), (own ? "the enemy" : "your"));
            break;
        }
		case SV_FLAGRESET:
		{
			playsound(S_FLAGRETURN);
			conoutf("the server reset the flag");
			break;
		}
        default: break;
    }
}

// server administration

void setmaster(int claim)
{
    addmsg(SV_SETMASTER, "ri", claim != 0 ? 1 : 0);
}

void setadmin(char *claim, char *password)
{
    if(!claim) return;
	if(strlen(password) > 15) { conoutf("the admin password has a maximum length of 15 characters"); return; }
    else addmsg(SV_SETADMIN, "ris", atoi(claim) != 0 ? 1 : 0, password);
}

void serveropcommand(int cmd, int a) // one of MCMD_*
{
	addmsg(SV_SERVOPCMD, "rii", cmd, a);
}

void kick(int player) { serveropcommand(SOPCMD_KICK, player); }
void ban(int player) { serveropcommand(SOPCMD_BAN, player); }
void removebans() { serveropcommand(SOPCMD_REMBANS, 0); }
void autoteam(int enable) { serveropcommand(SOPCMD_AUTOTEAM, enable); }
void mastermode(int mode) { serveropcommand(SOPCMD_MASTERMODE, mode); }
void forceteam(int player) { serveropcommand(SOPCMD_FORCETEAM, player); }
void givemaster(int player) { serveropcommand(SOPCMD_GIVEMASTER, player); }

COMMAND(setmaster, ARG_1INT);
COMMAND(setadmin, ARG_2STR);
COMMAND(kick, ARG_1INT);
COMMAND(ban, ARG_1INT);
COMMAND(removebans, ARG_NONE);
COMMAND(autoteam, ARG_1INT);
COMMAND(mastermode, ARG_1INT);
COMMAND(forceteam, ARG_1INT);
COMMAND(givemaster, ARG_1INT);

struct mline { string name, cmd; };
static vector<mline> mlines;

void *kickmenu = NULL, *banmenu = NULL, *forceteammenu = NULL, *givemastermenu = NULL;

void refreshsopmenu(void *menu, bool init)
{
    int item = 0;
    mlines.setsize(0);
    loopv(players) if(players[i])
    {
        mline &m = mlines.add();
        s_strcpy(m.name, colorname(players[i]));
        s_sprintf(m.cmd)("%s %d", menu==kickmenu ? "kick" : (menu==banmenu ? "ban" : (menu==forceteammenu ? "forceteam" : "givemaster")), i);
        menumanual(menu, item++, m.name, m.cmd);
    }
}

