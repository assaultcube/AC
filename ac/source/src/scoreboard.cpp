#include "cube.h"

// creation of scoreboard pseudo-menu

void *scoremenu = NULL, *teammenu = NULL, *ctfmenu = NULL;

void showscores(bool on)
{
    menuset(on ? (m_ctf ? ctfmenu : (m_teammode ? teammenu : scoremenu)) : NULL);
}

COMMAND(showscores, ARG_DOWN);

struct sline { string s; };
vector<sline> scorelines;

void renderscore(void *menu, playerent *d, int cn)
{
    const char *status = "";
    if(d->clientrole==CR_MASTER) status = "\f0";
    else if(d->clientrole==CR_ADMIN) status = "\f3";
    else if(d->state==CS_DEAD) status = "\f4";
    s_sprintfd(lag)("%d", d->plag);
    string &s = scorelines.add().s;
    if(m_ctf) s_sprintf(s)("%d\t%d\t%d\t%s\t%d\t%s\t%s%s\t%d", d->flagscore, d->frags, d->lifesequence, d->state==CS_LAGGED ? "LAG" : lag, d->ping, d->team, status, d->name, cn);
    else if(m_teammode) s_sprintf(s)("%d\t%d\t%s\t%d\t%s\t%s%s\t%d", d->frags, d->lifesequence, d->state==CS_LAGGED ? "LAG" : lag, d->ping, m_teammode ? d->team : "", status, d->name, cn);
	else s_sprintf(s)("%d\t%d\t%s\t%d\t%s%s\t%d", d->frags, d->lifesequence, d->state==CS_LAGGED ? "LAG" : lag, d->ping, status, d->name, cn);
    menumanual(menu, scorelines.length()-1, s);
}

static int scorecmp(const playerent **x, const playerent **y)
{   
    if((*x)->flagscore > (*y)->flagscore) return -1;
    if((*x)->flagscore < (*y)->flagscore) return 1;
    if((*x)->frags > (*y)->frags) return -1;
    if((*x)->frags < (*y)->frags) return 1;
    return 0;
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
    return 0;
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
    scorelines.setsize(0);

    vector<playerent *> scores;
    if(!demoplayback) scores.add(player1);
    loopv(players) if(players[i]) scores.add(players[i]);
    scores.sort(scorecmp);
    loopv(scores) renderscore(menu, scores[i], scores[i]->clientnum);

    if(init)
    {
        int sel = scores.find(player1);
        if(sel>=0) menuselect(menu, sel);
    }

    if(m_teammode)
    {
        menumanual(menu, scorelines.length(), "");
        teamscores.setsize(0);
        loopv(players) addteamscore(players[i]);
        if(!demoplayback) addteamscore(player1);
        teamscores.sort(teamscorecmp);
		string &teamline = scorelines.add().s;
		teamline[0] = 0;
        loopv(teamscores)
        {
            string s;
            if(m_ctf) s_sprintf(s)("[ %s: %d flags  %d frags ]", teamscores[i].team, teamscores[i].flagscore, teamscores[i].score);
            else s_sprintf(s)("[ %s: %d ]", teamscores[i].team, teamscores[i].score);
            s_strcat(teamline, s);
        }
		menumanual(menu, scorelines.length(), teamline);
    }
}

