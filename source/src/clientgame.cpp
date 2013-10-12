// clientgame.cpp: core game related stuff

#include "cube.h"
#include "bot/bot.h"

int nextmode = 0;   // nextmode becomes gamemode after next map load
VAR(gamemode, 1, 0, 0);
VAR(nextGameMode, 1, 0, 0);
VARP(modeacronyms, 0, 1, 1);

flaginfo flaginfos[2];

void mode(int *n)
{
    nextmode = *n;
    nextGameMode = nextmode;
    if(m_mp(*n) || !multiplayer()) addmsg(SV_GAMEMODE, "ri", *n);
}
COMMAND(mode, "i");

bool intermission = false;
int arenaintermission = 0;
struct serverstate servstate = { 0 };

playerent *player1 = newplayerent();          // our client
vector<playerent *> players;                  // other clients

int lastmillis = 0, totalmillis = 0, nextmillis = 0;
int lasthit = 0;
int curtime = 0;
string clientmap = "";
int spawnpermission = SP_WRONGMAP;

char *getclientmap() { return clientmap; }

int getclientmode() { return gamemode; }

extern bool sendmapidenttoserver;

void setskin(playerent *pl, int skin, int team)
{
    if(!pl) return;
    pl->setskin(team, skin);
}

extern char *global_name;

bool duplicatename(playerent *d, char *name = NULL)
{
    if(!name) name = d->name;
    if(d!=player1 && !strcmp(name, player1->name)) return true;
    if(!strcmp(name, "you")) return true;
    loopv(players) if(players[i] && d!=players[i] && !strcmp(name, players[i]->name)) return true;
    global_name = player1->name; // this certainly is not the best place to put this
    return false;
}

char *colorname(playerent *d, char *name, const char *prefix)
{
    if(!name) name = d->name;
    if(name[0] && !duplicatename(d, name)) return name;
    static string cname[4];
    static int num = 0;
    num = (num + 1) % 4;
    formatstring(cname[num])("%s%s \fs\f6(%d)\fr", prefix, name, d->clientnum);
    return cname[num];
}

char *colorping(int ping)
{
    static string cping;
    formatstring(cping)("\fs\f%d%d\fr", ping <= 500 ? 0 : ping <= 1000 ? 2 : 3, ping);
    return cping;
}

char *colorpj(int pj)
{
    static string cpj;
    formatstring(cpj)("\fs\f%d%d\fr", pj <= 90 ? 0 : pj <= 170 ? 2 : 3, pj);
    return cpj;
}

const char *highlight(const char *text)
{
    static char result[MAXTRANS + 10];
    const char *marker = getalias("HIGHLIGHT"), *sep = " ,;:!\"'";
    if(!marker || !strstr(text, player1->name)) return text;
    filterrichtext(result, marker);
    defformatstring(subst)("\fs%s%s\fr", result, player1->name);
    char *temp = newstring(text);
    char *s = strtok(temp, sep), *l = temp, *c, *r = result;
    result[0] = '\0';
    while(s)
    {
        if(!strcmp(s, player1->name))
        {
            if(MAXTRANS - strlen(result) > strlen(subst) + (s - l))
            {
                for(c = l; c < s; c++) *r++ = text[c - temp];
                *r = '\0';
                strcat(r, subst);
            }
            l = s + strlen(s);
        }
        s = strtok(NULL, sep);
    }
    if(MAXTRANS - strlen(result) > strlen(text) - (l - temp)) strcat(result, text + (l - temp));
    delete[] temp;
    return *result ? result : text;
}

void ignore(int *cn)
{
    playerent *d = getclient(*cn);
    if(d && d != player1) d->ignored = true;
}

void listignored()
{
    string pl;
    pl[0] = '\0';
    loopv(players) if(players[i] && players[i]->ignored) concatformatstring(pl, ", %s", colorname(players[i]));
    if(*pl) conoutf(_("ignored players: %s"), pl + 2);
    else conoutf(_("no players were ignored."));
}

void clearignored(int *cn)
{
    loopv(players) if(players[i] && (*cn < 0 || *cn == i)) players[i]->ignored = false;
}

void muteplayer(int *cn)
{
    playerent *d = getclient(*cn);
    if(d && d != player1) d->muted = true;
}

void listmuted()
{
    string pl;
    pl[0] = '\0';
    loopv(players) if(players[i] && players[i]->muted) concatformatstring(pl, ", %s", colorname(players[i]));
    if(*pl) conoutf(_("muted players: %s"), pl + 2);
    else conoutf(_("no players were muted."));
}

void clearmuted(char *cn)
{
    loopv(players) if(players[i] && (*cn < 0 || *cn == i)) players[i]->muted = false;
}

COMMAND(ignore, "i");
COMMAND(listignored, "");
COMMAND(clearignored, "i");
COMMAND(muteplayer, "i");
COMMAND(listmuted, "");
COMMAND(clearmuted, "i");

void newname(const char *name)
{
    if(name[0])
    {
        string tmpname;
        filtertext(tmpname, name, 0, MAXNAMELEN);
        if(identexists("onNameChange"))
        {
            defformatstring(onnamechange)("onNameChange %d \"%s\"", player1->clientnum, tmpname);
            execute(onnamechange);
        }
        copystring(player1->name, tmpname);//12345678901234//
        if(!player1->name[0]) copystring(player1->name, "unarmed");
        updateclientname(player1);
        addmsg(SV_SWITCHNAME, "rs", player1->name);
    }
    else conoutf(_("your name is: %s"), player1->name);
    //alias(_("curname"), player1->name); // WTF? stef went crazy - this isn't something to translate either.
    alias("curname", player1->name);
}

int teamatoi(const char *name)
{
    string uc;
    strtoupper(uc, name);
    loopi(TEAM_NUM) if(!strcmp(teamnames[i], uc)) return i;
    return -1;
}

void newteam(char *name)
{
    if(*name)
    {
        int nt = teamatoi(name);
        if(nt == player1->team) return; // same team
        if(!team_isvalid(nt)) { conoutf(_("%c3\"%s\" is not a valid team name (try CLA, RVSF or SPECTATOR)"), CC, name); return; }
        if(team_isspect(nt))
        {
            if(player1->state != CS_DEAD) { conoutf(_("you'll need to be in a \"dead\" state to become a spectator")); return; }
            if(!multiplayer()) { conoutf(_("you cannot spectate in singleplayer")); return; }
        }
        if(player1->state == CS_EDITING) conoutf(_("you can't change team while editing"));
        else addmsg(SV_SWITCHTEAM, "ri", nt);
    }
    else conoutf(_("your team is: %s"), team_string(player1->team));
}

void benchme()
{
    if(team_isactive(player1->team) && servstate.mastermode == MM_MATCH)
        addmsg(SV_SWITCHTEAM, "ri", team_tospec(player1->team));
}

int _setskin(int s, int t)
{
    setskin(player1, s, t);
    addmsg(SV_SWITCHSKIN, "rii", player1->skin(0), player1->skin(1));
    return player1->skin(t);
}

COMMANDF(skin_cla, "i", (int *s) { intret(_setskin(*s, TEAM_CLA)); });
COMMANDF(skin_rvsf, "i", (int *s) { intret(_setskin(*s, TEAM_RVSF)); });
COMMANDF(skin, "i", (int *s) { intret(_setskin(*s, player1->team)); });

void curmodeattr(char *attr)
{
    if(!strcmp(attr, "team")) { intret(m_teammode); return; }
    else if(!strcmp(attr, "arena")) { intret(m_arena); return; }
    else if(!strcmp(attr, "flag")) { intret(m_flags); return; }
    else if(!strcmp(attr, "bot")) { intret(m_botmode); return; }
    intret(0);
}

COMMANDN(team, newteam, "s");
COMMANDN(name, newname, "s");
COMMAND(benchme, "");
COMMANDF(isclient, "i", (int *cn) { intret(getclient(*cn) != NULL ? 1 : 0); } );
COMMANDF(curmastermode, "", (void) { intret(servstate.mastermode); });
COMMANDF(curautoteam, "", (void) { intret(servstate.autoteam); });
COMMAND(curmodeattr, "s");
COMMANDF(curmap, "i", (int *cleaned) { result(*cleaned ? behindpath(getclientmap()) : getclientmap()); });
COMMANDF(curplayers, "", (void) { intret(players.length() + 1); });
VARP(showscoresondeath, 0, 1, 1);
VARP(autoscreenshot, 0, 0, 1);

void stopdemo()
{
    if(watchingdemo) enddemoplayback();
    else conoutf(_("not playing a demo"));
}
COMMAND(stopdemo, "");

// macros for playerinfo() & teaminfo(). Use this to replace pstats_xxx ?
#define ATTR_INT(name, attribute)    if(!strcmp(attr, #name)) { intret(attribute); return; }
#define ATTR_FLOAT(name, attribute)  if(!strcmp(attr, #name)) { floatret(attribute); return; }
#define ATTR_STR(name, attribute)    if(!strcmp(attr, #name)) { result(attribute); return; }

void playerinfo(int *cn, const char *attr)
{
    if(!*attr || !attr) return;

    int clientnum = *cn; // get player clientnum
    playerent *p = clientnum < 0 ? player1 : getclient(clientnum);
    if(!p)
    {
        if(!m_botmode && multiplayer(false)) // bot clientnums are still glitchy, causing this message to sometimes appear in offline/singleplayer when it shouldn't??? -Bukz 2012may
            conoutf("invalid clientnum cn: %s attr: %s", cn, attr);
        return;
    }

    if(p == player1)
    {
        ATTR_INT(magcontent, p->weaponsel->mag);
        ATTR_INT(ammo, p->weaponsel->ammo);
        ATTR_INT(primary, p->primary);
        ATTR_INT(curweapon, p->weaponsel->type);
        ATTR_INT(nextprimary, p->nextprimary);
    }

    if(p == player1
        || (team_base(p->team) == team_base(player1->team) && m_teammode)
        || player1->team == TEAM_SPECT
        || m_coop)
    {
        ATTR_INT(health, p->health);
        ATTR_INT(armour, p->armour);
        ATTR_INT(attacking, p->attacking);
        ATTR_INT(scoping, p->scoping);
        ATTR_FLOAT(x, p->o.x);
        ATTR_FLOAT(y, p->o.y);
        ATTR_FLOAT(z, p->o.z);
    }
    ATTR_STR(name, p->name);
    ATTR_INT(team, p->team);
    ATTR_INT(ping, p->ping);
    ATTR_INT(pj, p->plag);
    ATTR_INT(state, p->state);
    ATTR_INT(role, p->clientrole);
    ATTR_INT(frags, p->frags);
    ATTR_INT(flags, p->flagscore);
    ATTR_INT(points, p->points);
    ATTR_INT(deaths, p->deaths);
    ATTR_INT(tks, p->tks);
    ATTR_INT(alive, p->state == CS_ALIVE ? 1 : 0);
    ATTR_INT(spect, p->team == TEAM_SPECT || p->spectatemode == SM_FLY ? 1 : 0);
    ATTR_INT(cn, p->clientnum); // only useful to get player1's client number.
    ATTR_INT(skin_cla, p->skin(TEAM_CLA));
    ATTR_INT(skin_rvsf, p->skin(TEAM_RVSF));
    ATTR_INT(skin, p->skin(player1->team));

    string addrstr = "";
    uint2ip(p->address, addr);
    if(addr[3] != 0 || player1->clientrole==CR_ADMIN)
        formatstring(addrstr)("%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]); // full IP
    else formatstring(addrstr)("%d.%d.%d.x", addr[0], addr[1], addr[2]); // censored IP
    ATTR_STR(ip, addrstr);

    conoutf("invalid attribute: %s", attr);
}

void playerinfolocal(const char *attr)
{
    int cn = -1;
    playerinfo(&cn, attr);
}

COMMANDN(player, playerinfo, "is");
COMMANDN(player1, playerinfolocal, "s");

void teaminfo(const char *team, const char *attr)
{
    if(!team || !attr || !m_teammode) return;
    int t = teamatoi(team); // get player clientnum
    if(!team_isactive(t))
    {
        conoutf("invalid team: %s", team);
        return;
    }
    int t_flags = 0;
    int t_frags = 0;
    int t_deaths = 0;
    int t_points = 0;

    string teammembers = "", tmp;

    loopv(players) if(players[i] && players[i]->team == t)
    {
        t_frags += players[i]->frags;
        t_deaths += players[i]->deaths;
        t_points += players[i]->points;
        t_flags += players[i]->flagscore;
        sprintf(tmp, "%s%d ", teammembers, players[i]->clientnum);
        concatstring(teammembers, tmp);
    }

    loopv(discscores) if(discscores[i].team == t)
    {
        t_frags += discscores[i].frags;
        t_deaths += discscores[i].deaths;
        t_points += discscores[i].points;
        t_flags += discscores[i].flags;
    }

    if(player1->team == t)
    {
        t_frags += player1->frags;
        t_deaths += player1->deaths;
        t_points += player1->points;
        t_flags += player1->flagscore;
        sprintf(tmp, "%s%d ", teammembers, player1->clientnum);
        concatstring(teammembers, tmp);
    }

    ATTR_INT(flags, t_flags);
    ATTR_INT(frags, t_frags);
    ATTR_INT(deaths, t_deaths);
    ATTR_INT(points, t_points);
    ATTR_STR(name, team_string(t));
    ATTR_STR(players, teammembers);
    conoutf("invalid attribute: %s", attr);
}

COMMAND(teaminfo, "ss");

void deathstate(playerent *pl)
{
    pl->state = CS_DEAD;
    pl->spectatemode = SM_DEATHCAM;
    pl->respawnoffset = pl->lastpain = lastmillis;
    pl->move = pl->strafe = 0;
    pl->pitch = pl->roll = 0;
    pl->attacking = false;
    pl->weaponsel->onownerdies();

    if(pl==player1)
    {
        if(showscoresondeath) showscores(true);
        setscope(false);
        setburst(false);
        if(editmode) toggleedit(true);
        damageblend(-1);
        if(pl->team == TEAM_SPECT) spectatemode(SM_FLY);
        else if(team_isspect(pl->team)) spectatemode(SM_FOLLOW1ST);
        if(pl->spectatemode == SM_DEATHCAM) player1->followplayercn = FPCN_DEATHCAM;
    }
    else pl->resetinterp();
}

void spawnstate(playerent *d)              // reset player state not persistent accross spawns
{
    d->respawn();
    d->spawnstate(gamemode);
    if(d==player1)
    {
        setscope(false);
        setburst(false);
    }
    if(d->deaths==0) d->resetstats();
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
    weapon::equipplayer(d);
    spawnstate(d); // move like above
    int nextcn = 0;
    bool lukin = true;
    while(lukin)
    {
        bool used = nextcn==getclientnum();
        loopv(players) if(!used && players[i]) if(players[i]->clientnum==nextcn) used = true;
        if(!used) lukin = false; else nextcn++;
    }
    loopv(players) if(i!=getclientnum() && !players[i])
    {
        players[i] = d;
        d->clientnum = nextcn;
        return d;
    }
    if(players.length()==getclientnum()) players.add(NULL);
    d->clientnum = nextcn;
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

VAR(lastpm, 1, -1, 0);
void zapplayer(playerent *&d)
{
    if(d && d->clientnum == lastpm) lastpm = -1;
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
        copystring(str, "\f3");
    }
    if(curticks >= maxticks) return false;
    nextticks = min(nextticks, maxticks);
    while(curticks < nextticks)
    {
        if(++curticks%5) concatstring(str, ".");
        else
        {
            defformatstring(sec)("%d", maxsecs - (curticks/5));
            concatstring(str, sec);
        }
    }
    if(nextticks < maxticks) hudeditf(HUDMSG_TIMER|HUDMSG_OVERWRITE, flash ? str : str+2);
    else hudeditf(HUDMSG_TIMER, msg);
    return true;
}

int lastspawnattempt = 0;

void showrespawntimer()
{
    if(intermission || spawnpermission > SP_OK_NUM) return;
    if(m_arena)
    {
        if(!arenaintermission) return;
        showhudtimer(5, arenaintermission, "FIGHT!", lastspawnattempt >= arenaintermission && lastmillis < lastspawnattempt+100);
    }
    else if(player1->state==CS_DEAD && m_flags && (!player1->isspectating() || player1->spectatemode==SM_DEATHCAM))
    {
        int secs = 5;
        showhudtimer(secs, player1->respawnoffset, "READY!", lastspawnattempt >= arenaintermission && lastmillis < lastspawnattempt+100);
    }
}

struct scriptsleep { int wait, millis; char *cmd; bool persist; };
vector<scriptsleep> sleeps;

void addsleep(int msec, const char *cmd, bool persist)
{
    scriptsleep &s = sleeps.add();
    s.wait = max(msec, 1);
    s.millis = lastmillis;
    s.cmd = newstring(cmd);
    s.persist = persist;
}

void addsleep_(int *msec, char *cmd, int *persist)
{
    addsleep(*msec, cmd, *persist != 0);
}

void resetsleep(bool force)
{
    loopv(sleeps) if(!sleeps[i].persist || force)
    {
        DELETEA(sleeps[i].cmd);
        sleeps.remove(i);
    }
}

COMMANDN(sleep, addsleep_, "isi");
COMMANDF(resetsleeps, "", (void) { resetsleep(true); });

void updateworld(int curtime, int lastmillis)        // main game update loop
{
    // process command sleeps
    loopv(sleeps)
    {
        if(lastmillis - sleeps[i].millis >= sleeps[i].wait)
        {
            char *cmd = sleeps[i].cmd;
            sleeps[i].cmd = NULL;
            execute(cmd);
            delete[] cmd;
            if(sleeps[i].cmd || !sleeps.inrange(i)) break;
            sleeps.remove(i--);
        }
    }

    syncentchanges();
    physicsframe();
    checkweaponstate();
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
float nearestenemy(vec place, int team)
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
        int type = m_teammode ? team_base(d->team) : 100;
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
                // 2013jun28:lucas: SKB suggested to use FFA spawns only in FFA modes, which seems reasonable.
                spawncycle = /*m_ktf && */numspawn[2] > 4 ? findentity(PLAYERSTART, spawncycle+1, 100) : findentity(PLAYERSTART, spawncycle+1);
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
    if(identexists("onSpawn")/* && (m_teammode && d->team == player1->team)*/)
    {
        defformatstring(onspawn)("onSpawn %d", d->clientnum);
        execute(onspawn);
    }
}

void spawnplayer(playerent *d)
{
    d->respawn();
    d->spawnstate(gamemode);
    d->state = (d==player1 && editmode) ? CS_EDITING : CS_ALIVE;
    findplayerstart(d);
}

void respawnself()
{
    if( m_mp(gamemode) ) addmsg(SV_TRYSPAWN, "r");
    else
    {
        showscores(false);
        setscope(false);
        setburst(false);
        lasthit = 0;
        spawnplayer(player1);
        player1->lifesequence++;
        player1->weaponswitch(player1->primweap);
        player1->weaponchanging -= player1->weapons[player1->gunselect]->weaponchangetime/2; // 2011jan16:ft: for a little no-shoot after spawn
    }
}

extern int checkarea(int maplayout_factor, char *maplayout);
extern int MA;
extern float Mh;

bool bad_map() // this function makes a pair with good_map from clients2c
{
    return (gamemode != GMODE_COOPEDIT && ( Mh >= MAXMHEIGHT || MA >= MAXMAREA ));
}

inline const char * spawn_message()
{
    if (spawnpermission == SP_WRONGMAP)
    {
        // Don't use "/n" within these messages. If you do, the words won't align to the middle of the screen.
        if (securemapcheck(getclientmap()))
        return "3The server will NOT allow spawning or getmap!";   // Also see client.cpp which has a conoutf message
        else return "3You must be on the correct map to spawn. Type /getmap to download it.";
    }
    else if (m_coop) return "3Type /getmap or send a map and vote for it to start co-op edit.";
    else if (multiplayer(false)) return "4Awaiting permission to spawn. \f2DON'T PANIC!";
    else return ""; // theres no waiting for permission in sp
    // Despite its many glaring (and occasionally fatal) inaccuracies, AssaultCube itself has outsold the
    // Encyclopedia Galactica because it is slightly cheaper, and because it has the words "Don't Panic"
    // in large, friendly letters.
}

int waiting_permission = 0;

bool tryrespawn()
{
    if ( m_mp(gamemode) && multiplayer(false) && bad_map() )
    {
        hudoutf("This map is not supported in multiplayer. Read the docs about map quality/dimensions.");
    }
    else if(spawnpermission > SP_OK_NUM)
    {
        hudeditf(HUDMSG_TIMER, "\f%s", spawn_message());
    }
    else if(player1->state==CS_DEAD || player1->state==CS_SPECTATE)
    {
        if(team_isspect(player1->team))
        {
            respawnself();
            return true;
        }
        else
        {
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
                if (lastmillis > waiting_permission)
                {
                    waiting_permission = lastmillis + 1000;
                    respawnself();
                }
                else hudeditf(HUDMSG_TIMER, "\f%s", spawn_message());
                return true;
            }
            else lastspawnattempt = lastmillis;
        }
    }
    return false;
}

VARP(hitsound, 0, 0, 2);

// client kill messages
void setkillmessage(int gun, bool gib, const char *message)
{
    if(!message || !*message)
    {
        result(killmessage(gun, gib));
        return;
    }
    if(gun < 0 || gun >= NUMGUNS)
    {
        conoutf("invalid gun specified");
        return;
    }
    copystring(killmessages[gib?1:0][gun], message, sizeof(killmessages[gib?1:0][gun]));
}

COMMANDF(fragmessage, "is", (int *gun, const char *message) { setkillmessage(*gun, false, message); });
COMMANDF(gibmessage, "is", (int *gun, const char *message) { setkillmessage(*gun, true, message); });

void burstshots(int gun, int shots)
{
    // args are passed as strings to differentiate 2 cases : shots_str == "0" or shots_str is empty (not specified from cubescript).
    if(gun >= 0 && gun < NUMGUNS && guns[gun].isauto)
    {
        if(shots >= 0) burstshotssettings[gun] = min(shots, (guns[gun].magsize-1));
        else intret(burstshotssettings[gun]);
    }
    else conoutf(_("invalid gun specified"));
}

COMMANDF(burstshots, "ii", (int *g, int *s) { burstshots(*g, *s); });

// damage arriving from the network, monsters, yourself, all ends up here.

void dodamage(int damage, playerent *pl, playerent *actor, int gun, bool gib, bool local)
{
    if(pl->state != CS_ALIVE || intermission) return;
    pl->respawnoffset = pl->lastpain = lastmillis;
    // could the author of the FIXME below please elaborate what's to fix?! (ft:2011mar28)
    // I suppose someone wanted to play the hitsound for player1 or spectated player (lucas:2011may22)
    playerent *h = player1->isspectating() && player1->followplayercn >= 0 && (player1->spectatemode == SM_FOLLOW1ST || player1->spectatemode == SM_FOLLOW3RD || player1->spectatemode == SM_FOLLOW3RD_TRANSPARENT) ? getclient(player1->followplayercn) : NULL;
    if(!h) h = player1;
    if(identexists("onHit"))
    {
        defformatstring(o)("onHit %d %d %d %d %d", actor->clientnum, pl->clientnum, damage, gun, gib ? 1 : 0);
        execute(o);
    }
    if(actor==h && pl!=actor)
    {
        if( (hitsound == 1 || (hitsound && h != player1) ) && lasthit != lastmillis) audiomgr.playsound(S_HITSOUND, SP_HIGH);
        lasthit = lastmillis;
    }

    if (pl != player1)
    {
        damageeffect(damage, pl);
        audiomgr.playsound(S_PAIN1+rnd(5), pl);
    }

    if(local) damage = pl->dodamage(damage, gun);
    else if(actor==player1) return;

    if(pl==player1)
    {
        updatedmgindicator(actor->o);
        damageblend(damage);
        pl->damageroll(damage);
    }

    if(pl->health<=0) { if(local) dokill(pl, actor, gib, gun >= 0 ? gun : actor->weaponsel->type); }
    else if(pl==player1) audiomgr.playsound(S_PAIN6, SP_HIGH);
    else audiomgr.playsound(S_PAIN1+rnd(5), pl);
}

void dokill(playerent *pl, playerent *act, bool gib, int gun)
{
    if(pl->state!=CS_ALIVE || intermission) return;

    if(identexists("onKill"))
    {
        defformatstring(killevent)("onKill %d %d %d %d", act->clientnum, pl->clientnum, gun, gib ? 1 : 0);
        execute(killevent);
    }

    string pname, aname, death;
    copystring(pname, pl==player1 ? "you" : colorname(pl));
    copystring(aname, act==player1 ? "you" : colorname(act));
    copystring(death, killmessage(gun, gib));
    void (*outf)(const char *s, ...) = (pl == player1 || act == player1) ? hudoutf : conoutf;

    if(pl==act)
    {
        outf("\f2%s suicided%s", pname, pl==player1 ? "!" : "");
    }
    else if(isteam(pl->team, act->team))
    {
        if(pl==player1) outf("\f2you were %s by teammate %s", death, aname);
        else outf("%s%s %s teammate %s", act==player1 ? "\f3" : "\f2", aname, death, pname);
    }
    else
    {
        if(pl==player1) outf("\f2you were %s by %s", death, aname);
        else outf("\f2%s %s %s", aname, death, pname);
    }

    if(pl == act || isteam(pl->team, act->team))
    {
        if(pl != act) act->tks++;
        if(!m_mp(gamemode)) act->frags--;
    }
    else if(!m_mp(gamemode)) act->frags += ( gib && gun != GUN_GRENADE && gun != GUN_SHOTGUN) ? 2 : 1;

    if(gib)
    {
        if(pl!=act && gun == GUN_SNIPER) audiomgr.playsound(S_HEADSHOT, SP_LOW);
        addgib(pl);
    }

    deathstate(pl);
    pl->deaths++;
    audiomgr.playsound(S_DIE1+rnd(2), pl);
}

void pstat_weap(int *cn)
{
    string weapstring = "";
    playerent *pl = getclient(*cn);
    if(pl) loopi(NUMGUNS) concatformatstring(weapstring, "%s%d %d", strlen(weapstring) ? " " : "", pl->pstatshots[i], pl->pstatdamage[i]);
    result(weapstring);
}

COMMAND(pstat_weap, "i");

VAR(minutesremaining, 1, 0, 0);
VAR(gametimecurrent, 1, 0, 0);
VAR(gametimemaximum, 1, 0, 0);
VAR(lastgametimeupdate, 1, 0, 0);

void silenttimeupdate(int milliscur, int millismax)
{
    lastgametimeupdate = lastmillis;
    gametimecurrent = milliscur;
    gametimemaximum = millismax;
    minutesremaining = (gametimemaximum - gametimecurrent + 60000 - 1) / 60000;
}

void timeupdate(int milliscur, int millismax)
{
    bool display = lastmillis - lastgametimeupdate > 1000; // avoid double-output

    silenttimeupdate(milliscur, millismax);

    if(!display) return;
    if(!minutesremaining)
    {
        intermission = true;
        extern bool needsautoscreenshot;
        if(autoscreenshot) needsautoscreenshot = true;
        player1->attacking = false;
        conoutf(_("intermission:"));
        conoutf(_("game has ended!"));
        consolescores();
        showscores(true);
        if(identexists("start_intermission")) execute("start_intermission");
    }
    else
    {
        extern int clockdisplay; // only output to console if no hud-clock is being shown
        if(minutesremaining==1)
        {
            audiomgr.musicsuggest(M_LASTMINUTE1 + rnd(2), 70*1000, true);
            hudoutf("1 minute left!");
            if(identexists("onLastMin")) execute("onLastMin");
        }
        else if(clockdisplay==0) conoutf(_("time remaining: %d minutes"), minutesremaining);
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
    if(cn == player1->clientnum) return player1;
    return players.inrange(cn) ? players[cn] : NULL;
}

void initclient()
{
    newname("unarmed");
    player1->team = TEAM_SPECT;
}

entity flagdummies[2] = // in case the map does not provide flags
{
    entity(-1, -1, -1, CTF_FLAG, 0, 0, 0, 0),
    entity(-1, -1, -1, CTF_FLAG, 0, 1, 0, 0)
};

void initflag(int i)
{
    flaginfo &f = flaginfos[i];
    f.flagent = &flagdummies[i];
    f.pos = vec(f.flagent->x, f.flagent->y, f.flagent->z);
    f.ack = true;
    f.actor = NULL;
    f.actor_cn = -1;
    f.team = i;
    f.state = m_ktf ? CTFF_IDLE : CTFF_INBASE;
}

void zapplayerflags(playerent *p)
{
    loopi(2) if(flaginfos[i].state==CTFF_STOLEN && flaginfos[i].actor==p) initflag(i);
}

void preparectf(bool cleanonly=false)
{
    loopi(2) initflag(i);
    if(!cleanonly)
    {
        loopv(ents)
        {
            entity &e = ents[i];
            if(e.type==CTF_FLAG)
            {
                e.spawned = true;
                if(e.attr2>=2) { conoutf(_("%c3invalid ctf-flag entity (%i)"), CC, i); e.attr2 = 0; }
                flaginfo &f = flaginfos[e.attr2];
                f.flagent = &e;
                f.pos.x = (float) e.x;
                f.pos.y = (float) e.y;
                f.pos.z = (float) e.z;
            }
        }
    }
}

struct gmdesc { int mode; char *desc; };
vector<gmdesc> gmdescs;

void gamemodedesc(int *modenr, char *desc)
{
    if(!desc) return;
    struct gmdesc &gd = gmdescs.add();
    gd.mode = *modenr;
    gd.desc = newstring(desc);
}

COMMAND(gamemodedesc, "is");

void resetmap(bool mrproper)
{
    resetsleep();
    resetzones();
    clearminimap();
    cleardynlights();
    pruneundos();
    changedents.setsize(0);
    particlereset();
    if(mrproper)
    {
        audiomgr.clearworldsounds();
        setvar("gamespeed", 100);
        setvar("paused", 0);
        setvar("fog", 180);
        setvar("fogcolour", 0x8099B3);
        setvar("shadowyaw", 45);
    }
}

int suicided = -1;
extern bool good_map();
extern bool item_fail;
extern int MA, F2F, Ma, Hhits;
extern float Mh;

VARP(mapstats_hud, 0, 0, 1);

void showmapstats()
{
    conoutf("\f2Map Quality Stats");
    conoutf("  The mean height is: %.2f", Mh);
    if (Hhits) conoutf("  Height check is: %d", Hhits);
    if (MA) conoutf("  The max area is: %d (of %d)", MA, Ma);
    if (m_flags && F2F < 1000) conoutf("  Flag-to-flag distance is: %d", (int)fSqrt(F2F));
    if (item_fail) conoutf("  There are one or more items too close to each other in this map");
}
COMMAND(showmapstats, "");

VARP(showmodedescriptions, 0, 1, 1);
extern bool canceldownloads;

void startmap(const char *name, bool reset)   // called just after a map load
{
    canceldownloads = false;
    copystring(clientmap, name);
    sendmapidenttoserver = true;
    // Added by Rick
    if(m_botmode) BotManager.BeginMap(name);
    else kickallbots();
    // End add by Rick
    clearbounceents();
    preparectf(!m_flags);
    suicided = -1;
    spawncycle = -1;
    lasthit = 0;
    if(m_valid(gamemode) && !m_mp(gamemode)) respawnself();
    else findplayerstart(player1);
    if(good_map()==MAP_IS_BAD) conoutf(_("You cannot play in this map due to quality requisites. Please, report this incident."));
    if (mapstats_hud) showmapstats();

    if(!reset) return;

    player1->frags = player1->flagscore = player1->deaths = player1->lifesequence = player1->points = player1->tks = 0;
    loopv(players) if(players[i]) players[i]->frags = players[i]->flagscore = players[i]->deaths = players[i]->lifesequence = players[i]->points = players[i]->tks = 0;
    if(editmode) toggleedit(true);
    intermission = false;
    showscores(false);
    needscoresreorder = true;
    minutesremaining = -1;
    lastgametimeupdate = 0;
    arenaintermission = 0;
    bool noflags = (m_ctf || m_ktf) && (!numflagspawn[0] || !numflagspawn[1]);
    if(*clientmap) conoutf(_("game mode is \"%s\"%s"), modestr(gamemode, modeacronyms > 0), noflags ? " - \f2but there are no flag bases on this map" : "");

    if(showmodedescriptions && (multiplayer(false) || m_botmode))
    {
        loopv(gmdescs) if(gmdescs[i].mode == gamemode)
        {
            //conoutf(_("%c1%s"), CC, gmdescs[i].desc); // 3rd useless call to translation - these should be translated inside the cube-script-definition
            conoutf("\f1%s", gmdescs[i].desc);
        }
    }

    // run once
    if(firstrun)
    {
        per_idents = false;
        execfile("config/firstrun.cfg");
        per_idents = true;
        firstrun = false;
    }
    // execute mapstart event once
    const char *mapstartonce = getalias("mapstartonce");
    if(mapstartonce && mapstartonce[0])
    {
        addsleep(0, mapstartonce); // do this as a sleep to make sure map changes don't recurse inside a welcome packet
        // BTW: in v1.0.4 sleep 1 was required to make it work on initial mapload [flowtron:2010jun25]
        alias("mapstartonce", "");
    }
    // execute mapstart event
    const char *mapstartalways = getalias("mapstartalways");
    if(mapstartalways && mapstartalways[0])
    {
        addsleep(0, mapstartalways);
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

COMMAND(suicide, "");

// console and audio feedback

void flagmsg(int flag, int message, int actor, int flagtime)
{
    static int musicplaying = -1;
    playerent *act = getclient(actor);
    if(actor != getclientnum() && !act && message != FM_RESET) return;
    bool own = flag == team_base(player1->team);
    bool neutral = team_isspect(player1->team);
    bool firstperson = actor == getclientnum();
    bool teammate = !act ? true : isteam(player1->team, act->team);
    bool firstpersondrop = false;
    defformatstring(ownerstr)("the %s", teamnames[flag]);
    const char *teamstr = m_ktf ? "the" : neutral ? ownerstr : own ? "your" : "the enemy";
    const char *flagteam = (m_ktf && !neutral) ? (teammate ? "your teammate " : "your enemy ") : "";

    if(identexists("onFlag"))
    {
        defformatstring(onflagevent)("onFlag %d %d %d", message, actor, flag);
        execute(onflagevent);
    }

    switch(message)
    {
        case FM_PICKUP:
            audiomgr.playsound(S_FLAGPICKUP, SP_HIGHEST);
            if(firstperson)
            {
                hudoutf("\f2you have the %sflag", m_ctf ? "enemy " : "");
                audiomgr.musicsuggest(M_FLAGGRAB, m_ctf ? 90*1000 : 900*1000, true);
                musicplaying = flag;
            }
            else hudoutf("\f2%s%s has %s flag", flagteam, colorname(act), teamstr);
            break;
        case FM_LOST:
        case FM_DROP:
        {
            const char *droplost = message == FM_LOST ? "lost" : "dropped";
            audiomgr.playsound(S_FLAGDROP, SP_HIGHEST);
            if(firstperson)
            {
                hudoutf("\f2you %s the flag", droplost);
                firstpersondrop = true;
            }
            else hudoutf("\f2%s %s %s flag", colorname(act), droplost, teamstr);
            break;
        }
        case FM_RETURN:
            audiomgr.playsound(S_FLAGRETURN, SP_HIGHEST);
            if(firstperson) hudoutf("\f2you returned your flag");
            else hudoutf("\f2%s returned %s flag", colorname(act), teamstr);
            break;
        case FM_SCORE:
            audiomgr.playsound(S_FLAGSCORE, SP_HIGHEST);
            if(firstperson)
            {
                hudoutf("\f2you scored");
                if(m_ctf) firstpersondrop = true;
            }
            else hudoutf("\f2%s scored for %s", colorname(act), neutral ? teamnames[act->team] : teammate ? "your team" : "the enemy team");
            break;
        case FM_KTFSCORE:
        {
            audiomgr.playsound(S_KTFSCORE, SP_HIGHEST);
            const char *ta = firstperson ? "you have" : colorname(act);
            const char *tb = firstperson ? "" : " has";
            const char *tc = firstperson ? "" : flagteam;
            int m = flagtime / 60;
            if(m)
                hudoutf("\f2%s%s%s kept the flag for %d minute%s %d seconds now", tc, ta, tb, m, m == 1 ? "" : "s", flagtime % 60);
            else
                hudoutf("\f2%s%s%s kept the flag for %d seconds now", tc, ta, tb, flagtime);
            break;
        }
        case FM_SCOREFAIL: // sound?
            hudoutf("\f2%s failed to score (own team flag not taken)", firstperson ? "you" : colorname(act));
            break;
        case FM_RESET:
            audiomgr.playsound(S_FLAGRETURN, SP_HIGHEST);
            hudoutf("the server reset the flag");
            firstpersondrop = true;
            break;
    }
    if(firstpersondrop && flag == musicplaying)
    {
        audiomgr.musicfadeout(M_FLAGGRAB);
        musicplaying = -1;
    }
}

void dropflag() { tryflagdrop(true); }
COMMAND(dropflag, "");

char *votestring(int type, const char *arg1, const char *arg2, const char *arg3)
{
    const char *msgs[] = { "kick player %s, reason: %s", "ban player %s, reason: %s", "remove all bans", "set mastermode to %s", "%s autoteam", "force player %s to team %s", "give admin to player %s", "load map %s in mode %s%s%s", "%s demo recording for the next match", "stop demo recording", "clear all demos", "set server description to '%s'", "shuffle teams"};
    const char *msg = msgs[type];
    char *out = newstring(MAXSTRLEN);
    out[MAXSTRLEN] = '\0';
    switch(type)
    {
        case SA_KICK:
        case SA_BAN:
        case SA_FORCETEAM:
        case SA_GIVEADMIN:
        {
            int cn = atoi(arg1);
            playerent *p = getclient(cn);
            if(!p) break;
            if (type == SA_KICK || type == SA_BAN)
            {
                string reason = "";
                if(m_teammode) formatstring(reason)("%s (%d tks, ping %d)", arg2, p->tks, p->ping);
                else formatstring(reason)("%s (ping %d)", arg2, p->ping);
                formatstring(out)(msg, colorname(p), reason);
            }
            else if(type == SA_FORCETEAM)
            {
                int team = atoi(arg2);
                formatstring(out)(msg, colorname(p), team_isvalid(team) ? teamnames[team] : "");
            }
            else formatstring(out)(msg, colorname(p));
            break;
        }
        case SA_MASTERMODE:
            formatstring(out)(msg, mmfullname(atoi(arg1)));
            break;
        case SA_AUTOTEAM:
        case SA_RECORDDEMO:
            formatstring(out)(msg, atoi(arg1) == 0 ? "disable" : "enable");
            break;
        case SA_MAP:
        {
            int n = atoi(arg2);
            string timestr = "";
            if(arg3 && arg3[0])
            {
                int time = atoi(arg3);
                if(time > 0 && time != defaultgamelimit(n)) formatstring(timestr)(" for %d minutes", time);
            }

            if ( n >= GMODE_NUM )
            {
                formatstring(out)(msg, arg1, modestr(n-GMODE_NUM, modeacronyms > 0)," (in the next game)", timestr);
            }
            else
            {
                formatstring(out)(msg, arg1, modestr(n, modeacronyms > 0), "", timestr);
            }
            break;
        }
        case SA_SERVERDESC:
            formatstring(out)(msg, arg1);
            break;
        default:
            formatstring(out)(msg, arg1, arg2);
            break;
    }
    return out;
}

votedisplayinfo *newvotedisplayinfo(playerent *owner, int type, const char *arg1, const char *arg2, const char *arg3)
{
    if(type < 0 || type >= SA_NUM) return NULL;
    votedisplayinfo *v = new votedisplayinfo();
    v->owner = owner;
    v->type = type;
    v->millis = totalmillis + (30+10)*1000;
    char *votedesc = votestring(type, arg1, arg2, arg3);
    copystring(v->desc, votedesc);
    DELETEA(votedesc);
    return v;
}

votedisplayinfo *curvote = NULL, *calledvote = NULL;

void callvote(int type, const char *arg1, const char *arg2, const char *arg3)
{
    if(calledvote) return;
    votedisplayinfo *v = newvotedisplayinfo(player1, type, arg1, arg2, arg3);
    if(v)
    {
        calledvote = v;
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putint(p, SV_CALLVOTE);
        putint(p, v->type);
        switch(v->type)
        {
            case SA_KICK:
            case SA_BAN:
                putint(p, atoi(arg1));
                sendstring(arg2, p);
                break;
            case SA_MAP:
                sendstring(arg1, p);
                putint(p, atoi(arg2));
                putint(p, atoi(arg3));
                break;
            case SA_SERVERDESC:
                sendstring(arg1, p);
                break;
            case SA_STOPDEMO:
                // compatibility
                break;
            case SA_REMBANS:
            case SA_SHUFFLETEAMS:
                break;
            case SA_FORCETEAM:
                putint(p, atoi(arg1));
                putint(p, atoi(arg2));
                break;
            default:

				
				
				
				putint(p, atoi(arg1));
                break;
        }
        sendpackettoserv(1, p.finalize());
        if(identexists("onCallVote"))
        {
            defformatstring(runas)("%s %d %d [%s] [%s]", "onCallVote", type, player1->clientnum, arg1, arg2);
            execute(runas);
        }
    }
    else conoutf(_("%c3invalid vote"), CC);
}

void scallvote(int *type, const char *arg1, const char *arg2)
{
    if(type && inmainloop)
    {
        int t = *type;
        switch (t)
        {
            case SA_MAP:
            {
                //FIXME: this stupid conversion of ints to strings and back should
                //  really be replaced with a saner method
                char m[4];
                sprintf(&m[0], "%d", nextmode);
                callvote(t, arg1, &m[0], arg2);
                break;
            }
            case SA_KICK:
            case SA_BAN:
            {
                if (!arg1 || !isdigit(arg1[0]) || !arg2 || strlen(arg2) <= 3 || !multiplayer(false))
                {
                    if(!multiplayer(false))
                        conoutf(_("%c3%s is not available in singleplayer."), CC, t == SA_BAN ? "Ban" : "Kick");
                    else if(arg1 && !isdigit(arg1[0])) conoutf(_("%c3invalid vote"), CC);
                    else conoutf(_("%c3invalid reason"), CC);
                    break;
                }
            }
            case SA_FORCETEAM:
            {
                int team = atoi(arg2);
                if(team < 0) arg2 = (team == 0) ? "RVSF" : "CLA";
                // fall through
            }
            default:
                callvote(t, arg1, arg2);
        }
    }
}

int vote(int v)
{
    if(!curvote || v < 0 || v >= VOTE_NUM) return 0;
    if(curvote->localplayervoted) { conoutf(_("%c3you voted already"), CC); return 0; }
    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    putint(p, SV_VOTE);
    putint(p, v);
    sendpackettoserv(1, p.finalize());
    if(!curvote) return 0;
    curvote->stats[v]++;
    curvote->localplayervoted = true;
    return 1;
}

VAR(votepending, 1, 0, 0);

void displayvote(votedisplayinfo *v)
{
    if(!v) return;
    DELETEP(curvote);
    curvote = v;
    conoutf(_("%s called a vote: %s"), v->owner ? colorname(v->owner) : "", curvote->desc);
    audiomgr.playsound(S_CALLVOTE, SP_HIGHEST);
    curvote->localplayervoted = false;
    votepending = 1;
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
    conoutf(_("%c3could not vote: %s"), CC, voteerrorstr(e));
    DELETEP(calledvote);
}

void votecount(int v) { if(curvote && v >= 0 && v < VOTE_NUM) curvote->stats[v]++; }
void voteresult(int v)
{
    if(curvote && v >= 0 && v < VOTE_NUM)
    {
        curvote->result = v;
        curvote->millis = totalmillis + 5000;
        conoutf(_("vote %s"), v == VOTE_YES ? _("passed") : _("failed"));
        if(multiplayer(false)) audiomgr.playsound(v == VOTE_YES ? S_VOTEPASS : S_VOTEFAIL, SP_HIGH);
        if(identexists("onVoteEnd")) execute("onVoteEnd");
        votepending = 0;
    }
}

void clearvote() { DELETEP(curvote); DELETEP(calledvote); }

const char *modestrings[] =
{
    "tdm", "coop", "dm", "lms", "ts", "ctf", "pf", "btdm", "bdm", "lss",
    "osok", "tosok", "bosok", "htf", "tktf", "ktf", "tpf", "tlss", "bpf", "blss", "btsurv", "btosok"
};

void setnext(char *mode, char *map)
{
    if(!multiplayer(false)) { //RR 10/12/12 - Is this the action we want?
        conoutf("You cannot use setnext in singleplayer.");
        return;
    }
    if(!mode || !map) return;
    loopi(GMODE_NUM)
    {
        switch(i)
        {
            case GMODE_COOPEDIT:
            case GMODE_BOTTEAMDEATHMATCH:
            case GMODE_BOTDEATHMATCH:
            case GMODE_BOTONESHOTONEKILL:
            case GMODE_BOTLSS:
			case GMODE_BOTPISTOLFRENZY:
			case GMODE_BOTTEAMONESHOTONKILL:
                continue;
        }
        if(!strcmp(mode, modestrings[i]))
        {
            nextmode=i+GMODE_NUM;
	        string nm = ""; itoa(nm, nextmode);
            callvote(SA_MAP, map, nm, "0");
            break;
        }
    }
}
COMMAND(setnext, "ss");

void gonext(int *arg1)
{
    if(calledvote || !multiplayer(false)) return;
    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    putint(p, SV_CALLVOTE);
    putint(p, SA_MAP);
    sendstring("+1", p);
    putint(p, *arg1);
    putint(p, -1);
    sendpackettoserv(1, p.finalize());
}
COMMAND(gonext, "i");

COMMANDN(callvote, scallvote, "iss"); //fixme,ah
COMMANDF(vote, "i", (int *v) { vote(*v); });

void cleanplayervotes(playerent *p)
{
    if(calledvote && calledvote->owner==p) calledvote->owner = NULL;
    if(curvote && curvote->owner==p) curvote->owner = NULL;
}

void whois(int *cn)
{
    loopv(players) if(players[i] && players[i]->type == ENT_PLAYER && (*cn == -1 || players[i]->clientnum == *cn))
    {
        playerent *p = players[i];
        uint2ip(p->address, ip);
        if(m_teammode) conoutf(_("%c0INFO: %c5%s has %d teamkills."), CC, CC, p->name, p->tks);
        if(ip[3] != 0 || player1->clientrole==CR_ADMIN)
            conoutf("WHOIS client %d:\n\f5name\t%s\n\f5IP\t%d.%d.%d.%d", *cn, colorname(p), ip[0], ip[1], ip[2], ip[3]); // full IP
        else conoutf("WHOIS client %d:\n\f5name\t%s\n\f5IP\t%d.%d.%d.x", *cn, colorname(p), ip[0], ip[1], ip[2]); // censored IP
    }
}
COMMAND(whois, "i");

void findcn(char *name)
{
    loopv(players) if(players[i] && !strcmp(name, players[i]->name))
    {
        intret(players[i]->clientnum);
        return;
    }
    if(!strcmp(name, player1->name)) { intret(player1->clientnum); return; }
    intret(-1);
}
COMMAND(findcn, "s");

int sessionid = 0;

void setadmin(int *claim, char *password)
{
    if(!*claim && (player1->clientrole))
    {
        conoutf(_("you released admin status"));
        addmsg(SV_SETADMIN, "ri", 0);
    }
    else if(*claim != 0 && password)
        addmsg(SV_SETADMIN, "ris", *claim, genpwdhash(player1->name, password, sessionid));
}

COMMAND(setadmin, "is");

struct mline { string name, cmd; };
static vector<mline> mlines;

void *kickmenu = NULL, *banmenu = NULL, *forceteammenu = NULL, *giveadminmenu = NULL;

void refreshsopmenu(void *menu, bool init)
{
    menureset(menu);
    mlines.shrink(0);
    mlines.reserve(players.length());
    loopv(players) if(players[i])
    {
        mline &m = mlines.add();
        copystring(m.name, colorname(players[i]));
        string kbr;
        if(getalias("_kickbanreason")!=NULL) formatstring(kbr)(" [ %s ]", getalias("_kickbanreason")); // leading space!
        formatstring(m.cmd)("%s %d%s", menu==kickmenu ? "kick" : (menu==banmenu ? "ban" : (menu==forceteammenu ? "forceteam" : "giveadmin")), i, (menu==kickmenu||menu==banmenu)?(strlen(kbr)>8?kbr:" NONE"):""); // 8==3 + "format-extra-chars"
        menumanual(menu, m.name, m.cmd);
    }
}

extern bool watchingdemo;

// rotate through all spec-able players
playerent *updatefollowplayer(int shiftdirection)
{
    if(!shiftdirection)
    {
        playerent *f = players.inrange(player1->followplayercn) ? players[player1->followplayercn] : NULL;
        if(f && (watchingdemo || !f->isspectating())) return f;
    }

    // collect spec-able players
    vector<playerent *> available;
    loopv(players) if(players[i])
    {
        if(player1->team != TEAM_SPECT && !watchingdemo && m_teammode && team_base(players[i]->team) != team_base(player1->team)) continue;
        if(players[i]->state==CS_DEAD || players[i]->isspectating()) continue;
        available.add(players[i]);
    }
    if(!available.length()) return NULL;

    // rotate
    int oldidx = -1;
    if(players.inrange(player1->followplayercn)) oldidx = available.find(players[player1->followplayercn]);
    if(oldidx<0) oldidx = 0;
    int idx = (oldidx+shiftdirection) % available.length();
    if(idx<0) idx += available.length();

    player1->followplayercn = available[idx]->clientnum;
    return players[player1->followplayercn];
}

void spectate()
{
    if(m_demo) return;
    if(!team_isspect(player1->team)) addmsg(SV_SWITCHTEAM, "ri", TEAM_SPECT);
    else tryrespawn();
}

void setfollowplayer(int cn)
{
    // silently ignores invalid player-cn value passed
    if(players.inrange(cn) && players[cn])
    {
        if(!(m_teammode && !watchingdemo && team_base(players[cn]->team) != team_base(player1->team)))
        {
            player1->followplayercn = cn;
            if(player1->spectatemode == SM_FLY) player1->spectatemode = SM_FOLLOW1ST;
        }
    }
}

// set new spect mode
void spectatemode(int mode)
{
    if((player1->state != CS_DEAD && player1->state != CS_SPECTATE && !team_isspect(player1->team)) || (!m_teammode && !team_isspect(player1->team) && servstate.mastermode == MM_MATCH)) return;  // during ffa matches only SPECTATORS can spectate
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
                playerent *f = getclient(player1->followplayercn);
                if(f)
                {
                    player1->o = f->o;
                    player1->yaw = f->yaw;
                    player1->pitch = 0.0f;
                    player1->resetinterp();
                }
                else entinmap(player1); // or drop 'em at a random place
                player1->followplayercn = FPCN_FLY;
            }
            break;
        }
        case SM_OVERVIEW:
            player1->followplayercn = FPCN_OVERVIEW;
        break;
        default: break;
    }
    player1->spectatemode = mode;
}

void togglespect() // cycle through all spectating modes
{
    if(m_botmode)
        spectatemode(SM_FLY);
    else
    {
        int mode;
        if(player1->spectatemode==SM_NONE) mode = SM_FOLLOW1ST; // start with 1st person spect
        else mode = SM_FOLLOW1ST + ((player1->spectatemode - SM_FOLLOW1ST + 1) % (SM_OVERVIEW-SM_FOLLOW1ST)); // replace SM_OVERVIEW by SM_NUM to enable overview mode
        spectatemode(mode);
    }
}

void changefollowplayer(int shift)
{
    updatefollowplayer(shift);
}

COMMAND(spectate, "");
COMMANDF(spectatemode, "i", (int *mode) { spectatemode(*mode); });
COMMAND(togglespect, "");
COMMANDF(changefollowplayer, "i", (int *dir) { changefollowplayer(*dir); });
COMMANDF(setfollowplayer, "i", (int *cn) { setfollowplayer(*cn); });

void serverextension(char *ext, char *args)
{
    if(!ext || !ext[0]) return;
    size_t n = args ? strlen(args)+1 : 0;
    if(n>0) addmsg(SV_EXTENSION, "rsis", ext, n, args);
    else addmsg(SV_EXTENSION, "rsi", ext, n);
}

COMMAND(serverextension, "ss");
