#include "cube.h"

// creation of scoreboard pseudo-menu

void *scoremenu = NULL, *teammenu = NULL, *ctfmenu = NULL;

void showscores(bool on)
{
    menuset(on ? (m_ctf ? ctfmenu : (m_teammode ? teammenu : scoremenu)) : NULL);
}

COMMAND(showscores, ARG_DOWN);

struct sline { string s; };
static vector<sline> scorelines;
static string modeline, teamline;

void renderscore(void *menu, playerent *d, int cn)
{
    const char *status = "";
    if(d->clientrole==CR_MASTER) status = "\f0";
    else if(d->clientrole==CR_ADMIN) status = "\f3";
    else if(d->state==CS_DEAD) status = "\f4";
    s_sprintfd(lag)("%d", d->plag);
    string &s = scorelines.add().s;
    int deaths = d->lifesequence + (d->state==CS_DEAD ? 1 : 0);
    if(m_ctf) s_sprintf(s)("%d\t%d\t%d\t%s\t%s\t%s\t%s%s\t%d", d->flagscore, d->frags, deaths, d->state==CS_LAGGED ? "LAG" : lag, colorping(d->ping), d->team, status, colorname(d), cn);
    else if(m_teammode) s_sprintf(s)("%d\t%d\t%s\t%s\t%s\t%s%s\t%d", d->frags, deaths, d->state==CS_LAGGED ? "LAG" : lag, colorping(d->ping), m_teammode ? d->team : "", status, colorname(d), cn);
	else s_sprintf(s)("%d\t%d\t%s\t%s\t%s%s\t%d", d->frags, deaths, d->state==CS_LAGGED ? "LAG" : lag, colorping(d->ping), status, colorname(d), cn);
}

static int scorecmp(const playerent **x, const playerent **y)
{   
    if((*x)->flagscore > (*y)->flagscore) return -1;
    if((*x)->flagscore < (*y)->flagscore) return 1;
    if((*x)->frags > (*y)->frags) return -1;
    if((*x)->frags < (*y)->frags) return 1;
    if((*x)->lifesequence > (*y)->lifesequence) return 1;
    if((*x)->lifesequence < (*y)->lifesequence) return -1;
    return strcmp((*x)->name, (*y)->name);
}

struct teamscore
{
    char *team;
    int score, flagscore;
    teamscore() {}
    teamscore(char *s, int n, int f = 0) : team(s), score(n), flagscore(f) {}
};

static int teamscorecmp(const teamscore *x, const teamscore *y)
{
    if(x->flagscore > y->flagscore) return -1;
    if(x->flagscore < y->flagscore) return 1;
    if(x->score > y->score) return -1;
    if(x->score < y->score) return 1;
    return strcmp(x->team, y->team);
}

vector<teamscore> teamscores;

void addteamscore(playerent *d)
{
    if(!d || !d->team[0]) return;
    loopv(teamscores) if(!strcmp(teamscores[i].team, d->team))
    {
        teamscores[i].score += d->frags;
        if(m_ctf) teamscores[i].flagscore += d->flagscore;
        return;
    }
    teamscores.add(teamscore(d->team, d->frags, m_ctf ? d->flagscore : 0));
}

void renderscores(void *menu, bool init)
{
    modeline[0] = '\0';
    teamline[0] = '\0';
    scorelines.setsize(0);

    vector<playerent *> scores;
    scores.add(player1);
    loopv(players) if(players[i]) scores.add(players[i]);
    scores.sort(scorecmp);
    loopv(scores) renderscore(menu, scores[i], scores[i]->clientnum);

    if(init)
    {
        int sel = scores.find(player1);
        if(sel>=0) menuselect(menu, sel);
    }

    s_strcat(modeline, modestr(gamemode));
    if(getclientmap()[0])
    {
        s_strcat(modeline, ": ");
        s_strcat(modeline, getclientmap());
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

    if(m_teammode)
    {
        teamscores.setsize(0);
        loopv(players) addteamscore(players[i]);
        addteamscore(player1);
        teamscores.sort(teamscorecmp);
        loopv(teamscores)
        {
            string s;
            if(m_ctf) s_sprintf(s)("[ %s: %d flags  %d frags ]", teamscores[i].team, teamscores[i].flagscore, teamscores[i].score);
            else s_sprintf(s)("[ %s: %d ]", teamscores[i].team, teamscores[i].score);
            s_strcat(teamline, s);
        }
    }
    loopv(scorelines) menumanual(menu, i, scorelines[i].s);

    menuheader(menu, modeline, teamline);
}

