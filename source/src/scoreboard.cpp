// creation of scoreboard pseudo-menu

#include "pch.h"
#include "cube.h"

void *scoremenu = NULL, *teammenu = NULL, *ctfmenu = NULL;

void showscores(bool on)
{
    if(on) showmenu(m_flags ? "ctf score" : (m_teammode ? "team score" : "score"), false);
    else
    {
        closemenu("score");
        closemenu("team score");
        closemenu("ctf score");
    }
}

COMMAND(showscores, ARG_DOWN);

struct sline
{
    string s;
    color *bgcolor;
    sline() : bgcolor(NULL) {}
};

static vector<sline> scorelines;

struct teamscore
{
    int team, frags, deaths, flagscore;
    vector<playerent *> teammembers;
    teamscore(int t) : team(t), frags(0), deaths(0), flagscore(0) {}

    void addscore(playerent *d)
    {
        if(!d) return;
        teammembers.add(d);
        frags += d->frags;
        deaths += d->deaths;
        if(m_flags) flagscore += d->flagscore;
    }
};

static int teamscorecmp(const teamscore *x, const teamscore *y)
{
    if(x->flagscore > y->flagscore) return -1;
    if(x->flagscore < y->flagscore) return 1;
    if(x->frags > y->frags) return -1;
    if(x->frags < y->frags) return 1;
    if(x->deaths < y->deaths) return -1;
    if(x->deaths > y->deaths) return 1;
    return x->team > y->team;
}

static int scorecmp(const playerent **x, const playerent **y)
{
    if((*x)->flagscore > (*y)->flagscore) return -1;
    if((*x)->flagscore < (*y)->flagscore) return 1;
    if((*x)->frags > (*y)->frags) return -1;
    if((*x)->frags < (*y)->frags) return 1;
    if((*x)->deaths > (*y)->deaths) return 1;
    if((*x)->deaths < (*y)->deaths) return -1;
    if((*x)->lifesequence > (*y)->lifesequence) return 1;
    if((*x)->lifesequence < (*y)->lifesequence) return -1;
    return strcmp((*x)->name, (*y)->name);
}

struct scoreratio
{
    float ratio;
    int precision;

    void calc(int frags, int deaths)
    {
        // ratio
        if(frags>=0 && deaths>0) ratio = (float)frags/(float)deaths;
        else if(frags>=0 && deaths==0) ratio = frags;
        else ratio = 0.0f;

        // precision
        if(ratio<10.0f) precision = 2;
        else if(ratio>=10.0f && ratio<100.0f) precision = 1;
        else precision = 0;
    }
};

void renderscore(void *menu, playerent *d)
{
    const char *status = "";
    static color localplayerc(0.2f, 0.2f, 0.2f, 0.2f);
    if(d->clientrole==CR_ADMIN) status = d->state==CS_DEAD ? "\f7" : "\f3";
    else if(d->state==CS_DEAD) status = "\f4";
    s_sprintfd(lag)("%d", d->plag);
    sline &line = scorelines.add();
    line.bgcolor = d==player1 ? &localplayerc : NULL;
    string &s = line.s;
    scoreratio sr;
    sr.calc(d->frags, d->deaths);
    if(m_flags) s_sprintf(s)("%d\t%d\t%d\t%.*f\t%s\t%s\t%d\t%s%s", d->flagscore, d->frags, d->deaths, sr.precision, sr.ratio, d->state==CS_LAGGED ? "LAG" : lag, colorping(d->ping), d->clientnum, status, colorname(d));
    else if(m_teammode) s_sprintf(s)("%d\t%d\t%.*f\t%s\t%s\t%d\t%s%s", d->frags, d->deaths, sr.precision, sr.ratio, d->state==CS_LAGGED ? "LAG" : lag, colorping(d->ping), d->clientnum, status, colorname(d));
    else s_sprintf(s)("%d\t%d\t%.*f\t%s\t%s\t%d\t%s%s", d->frags, d->deaths, sr.precision, sr.ratio, d->state==CS_LAGGED ? "LAG" : lag, colorping(d->ping), d->clientnum, status, colorname(d));
}

void renderteamscore(void *menu, teamscore *t)
{
    if(!scorelines.empty()) // space between teams
    {
        sline &space = scorelines.add();
        space.s[0] = 0;
    }
    sline &line = scorelines.add();
    s_sprintfd(plrs)("(%d %s)", t->teammembers.length(), t->teammembers.length() == 1 ? "player" : "players");
    scoreratio sr;
    sr.calc(t->frags, t->deaths);
    if(m_flags) s_sprintf(line.s)("%d\t%d\t%d\t%.*f\t\t\t\t%s\t\t%s", t->flagscore, t->frags, t->deaths, sr.precision, sr.ratio, team_string(t->team), plrs);
    else if(m_teammode) s_sprintf(line.s)("%d\t%d\t%.*f\t\t\t\t%s\t\t%s", t->frags, t->deaths, sr.precision, sr.ratio, team_string(t->team), plrs);
    static color teamcolors[2] = { color(1.0f, 0, 0, 0.2f), color(0, 0, 1.0f, 0.2f) };
    line.bgcolor = &teamcolors[t->team];
    loopv(t->teammembers) renderscore(menu, t->teammembers[i]);
}

extern bool watchingdemo;

void renderscores(void *menu, bool init)
{
    static string modeline, serverline;

    modeline[0] = '\0';
    serverline[0] = '\0';
    scorelines.setsize(0);

    vector<playerent *> scores;
    if(!watchingdemo) scores.add(player1);
    loopv(players) if(players[i]) scores.add(players[i]);
    scores.sort(scorecmp);

    if(init)
    {
        int sel = scores.find(player1);
        if(sel>=0) menuselect(menu, sel);
    }

    if(getclientmap()[0])
    {
        bool fldrprefix = !strncmp(getclientmap(), "maps/", strlen("maps/"));
        s_sprintf(modeline)("\"%s\" on map %s", modestr(gamemode, modeacronyms > 0), fldrprefix ? getclientmap()+strlen("maps/") : getclientmap());
    }

    extern int minutesremaining;
    if((gamemode>1 || (gamemode==0 && multiplayer(false))) && minutesremaining >= 0)
    {
        if(!minutesremaining) s_strcat(modeline, ", intermission");
        else
        {
            s_sprintfd(timestr)(", %d %s remaining", minutesremaining, minutesremaining==1 ? "minute" : "minutes");
            s_strcat(modeline, timestr);
        }
    }

    if(multiplayer(false))
    {
        serverinfo *s = getconnectedserverinfo();
        if(s) s_sprintf(serverline)("%s:%d %s", s->name, s->port, s->sdesc);
    }

    if(m_teammode)
    {
        teamscore teamscores[2] = { teamscore(TEAM_CLA), teamscore(TEAM_RVSF) };

        loopv(players)
        {
            if(!players[i]) continue;
            teamscores[team_int(players[i]->team)].addscore(players[i]);
        }
        if(!watchingdemo) teamscores[team_int(player1->team)].addscore(player1);
        loopi(2) teamscores[i].teammembers.sort(scorecmp);

        int sort = teamscorecmp(&teamscores[TEAM_CLA], &teamscores[TEAM_RVSF]);
        loopi(2) renderteamscore(menu, &teamscores[sort < 0 ? i : (i+1)&1]);
    }
    else
    {
        loopv(scores) renderscore(menu, scores[i]);
    }

    menureset(menu);
    loopv(scorelines) menumanual(menu, scorelines[i].s, NULL, scorelines[i].bgcolor);
    menuheader(menu, modeline, serverline);

    // update server stats
    static int lastrefresh = 0;
    if(!lastrefresh || lastrefresh+5000<lastmillis)
    {
        refreshservers(NULL, init);
        lastrefresh = lastmillis;
    }
}

#define MAXJPGCOM 65533  // maximum JPEG comment length

void addstr(char *dest, const char *src) { if(strlen(dest) + strlen(src) < MAXJPGCOM) strcat(dest, src); }

const char *asciiscores(bool destjpg)
{
    static char *buf = NULL;
    static string team, flags, text;
    playerent *d;
    scoreratio sr;
    vector<playerent *> scores;

    if(!buf) buf = (char *) malloc(MAXJPGCOM +1);
    if(!buf) return "";

    if(!watchingdemo) scores.add(player1);
    loopv(players) if(players[i]) scores.add(players[i]);
    scores.sort(scorecmp);

    buf[0] = '\0';
    if(destjpg)
    {
        s_sprintf(text)("AssaultCube Screenshot (%s)\n", asctime());
        addstr(buf, text);
    }
    if(getclientmap()[0])
    {
        s_sprintf(text)("\n\"%s\" on map %s", modestr(gamemode, 0), getclientmap(), asctime());
        addstr(buf, text);
    }
    if(multiplayer(false))
    {
        serverinfo *s = getconnectedserverinfo();
        if(s)
        {
            string sdesc;
			filtertext(sdesc, s->sdesc, 1);
			s_sprintf(text)(", %s:%d %s", s->name, s->port, sdesc);
            addstr(buf, text);
        }
    }
    if(destjpg)
        addstr(buf, "\n");
    else
    {
        s_sprintf(text)("\n%sfrags deaths ratio cn%s name\n", m_flags ? "flags " : "", m_teammode ? " team" : "");
        addstr(buf, text);
    }
    loopv(scores)
    {
        d = scores[i];
        sr.calc(d->frags, d->deaths);
        s_sprintf(team)(destjpg ? ", %s" : " %-4s", d->team);
        s_sprintf(flags)(destjpg ? "%d/" : " %4d ", d->flagscore);
        if(destjpg)
            s_sprintf(text)("%s%s (%s%d/%d)\n", d->name, m_teammode ? team : "", m_flags ? flags : "", d->frags, d->deaths);
        else
            s_sprintf(text)("%s %4d   %4d %5.2f %2d%s %s%s\n", m_flags ? flags : "", d->frags, d->deaths, sr.ratio, d->clientnum,
                            m_teammode ? team : "", d->name, d->clientrole==CR_ADMIN ? " (admin)" : d==player1 ? " (you)" : "");
        addstr(buf, text);
    }
    if(destjpg)
    {
        extern int minutesremaining;
        s_sprintf(text)("(%sfrags/deaths), %d minute%s remaining\n", m_flags ? "flags/" : "", minutesremaining, minutesremaining == 1 ? "" : "s");
        addstr(buf, text);
    }
    return buf;
}

void consolescores()
{
    printf("%s\n", asciiscores());
}
