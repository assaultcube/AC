// creation of scoreboard pseudo-menu

#include "cube.h"

void *scoremenu = NULL, *teammenu = NULL, *ctfmenu = NULL;

void showscores(int on)
{
    if(on) showmenu(m_flags ? "ctf score" : (m_teammode ? "team score" : "score"), false);
    else if (!intermission)
    {
        closemenu("score");
        closemenu("team score");
        closemenu("ctf score");
    }
}

COMMAND(showscores, ARG_1INT);

struct sline
{
    string s;
    color *bgcolor;
    sline() : bgcolor(NULL) {}
};

static vector<sline> scorelines;
vector<discscore> discscores;

struct teamscore
{
    int team, frags, deaths, flagscore, points;
    vector<playerent *> teammembers;
    teamscore(int t) : team(t), frags(0), deaths(0), flagscore(0), points(0) {}

    void addplayer(playerent *d)
    {
        if(!d) return;
        teammembers.add(d);
        frags += d->frags;
        deaths += d->deaths;
        points += d->points;
        if(m_flags) flagscore += d->flagscore;
    }

    void addscore(discscore &d)
    {
        frags += d.frags;
        deaths += d.deaths;
        points += d.points;
        if(m_flags) flagscore += d.flags;
    }
};

static int teamscorecmp(const teamscore *x, const teamscore *y)
{
    if(x->flagscore > y->flagscore) return -1;
    if(x->flagscore < y->flagscore) return 1;
    if(x->points > y->points) return -1;
    if(x->points < y->points) return 1;
    if(x->frags > y->frags) return -1;
    if(x->frags < y->frags) return 1;
    if(x->deaths < y->deaths) return -1;
    return 1;
}

static int scorecmp(const playerent **x, const playerent **y)
{
    if((*x)->flagscore > (*y)->flagscore) return -1;
    if((*x)->flagscore < (*y)->flagscore) return 1;
    if((*x)->points > (*y)->points) return -1;
    if((*x)->points < (*y)->points) return 1;
    if((*x)->frags > (*y)->frags) return -1;
    if((*x)->frags < (*y)->frags) return 1;
    if((*x)->deaths > (*y)->deaths) return 1;
    if((*x)->deaths < (*y)->deaths) return -1;
    if((*x)->lifesequence > (*y)->lifesequence) return 1;
    if((*x)->lifesequence < (*y)->lifesequence) return -1;
    return strcmp((*x)->name, (*y)->name);
}

static int discscorecmp(const discscore *x, const discscore *y)
{
    if(x->team < y->team) return -1;
    if(x->team > y->team) return 1;
    if(m_flags && x->flags > y->flags) return -1;
    if(m_flags && x->flags < y->flags) return 1;
    if(x->frags > y->frags) return -1;
    if(x->frags < y->frags) return 1;
    if(x->deaths > y->deaths) return 1;
    if(x->deaths < y->deaths) return -1;
    return strcmp(x->name, y->name);
}

const char *scoreratio(int frags, int deaths)
{
    static string res;
    float ratio = (float)(frags >= 0 ? frags : 0) / (float)(deaths > 0 ? deaths : 1);
    int precision = 0;
    if(ratio<10.0f) precision = 2;
    else if(ratio<100.0f) precision = 1;
    formatstring(res)("%.*f", precision, ratio);
    return res;
}

void renderdiscscores(int team)
{
    loopv(discscores) if(team == team_group(discscores[i].team))
    {
        discscore &d = discscores[i];
        sline &line = scorelines.add();
        const char *spect = team_isspect(d.team) ? "\f4" : "";
//         const char *sr = scoreratio(d.frags, d.deaths);
        float ratio = (float)(d.frags >= 0 ? d.frags : 0) / (float)(d.deaths > 0 ? d.deaths : 1);
        const char *clag = team_isspect(d.team) ? "SPECT" : "";
        if(m_flags) formatstring(line.s)("%s%d\t%d\t%d\t%.1f\t%d\t%s\tDISC\t\t%s", spect, d.flags, d.frags, d.deaths, ratio, d.points, clag, d.name);
        else formatstring(line.s)("%s%d\t%d\t%.1f\t%d\t%s\tDISC\t\t%s", spect, d.frags, d.deaths, ratio, d.points, clag, d.name);
    }
}

void renderscore(playerent *d)
{
    const char *status = "";
    static color localplayerc(0.2f, 0.2f, 0.2f, 0.2f);
    if(d->clientrole==CR_ADMIN) status = d->state==CS_DEAD ? "\f7" : "\f3";
    else if(d->state==CS_DEAD) status = "\f4";
    const char *spect = team_isspect(d->team) ? "\f4" : "";
//    const char *sr = scoreratio(d->frags, d->deaths);
    float ratio = (float)(d->frags >= 0 ? d->frags : 0) / (float)(d->deaths > 0 ? d->deaths : 1);
    const char *clag = team_isspect(d->team) ? "SPECT" : (d->state==CS_LAGGED ? "LAG" : colorpj(d->plag));
    const char *cping = colorping(d->ping);
    const char *ign = d->ignored ? " (ignored)" : (d->muted ? " (muted)" : "");
    sline &line = scorelines.add();
    line.bgcolor = d==player1 ? &localplayerc : NULL;
    string &s = line.s;
    if(m_flags) formatstring(s)("%s%d\t%d\t%d\t%.1f\t%d\t%s\t%s\t%d\t%s%s%s", spect, d->flagscore, d->frags, d->deaths, ratio, d->points, clag, cping, d->clientnum, status, colorname(d), ign);
    else formatstring(s)("%s%d\t%d\t%.1f\t%d\t%s\t%s\t%d\t%s%s%s", spect, d->frags, d->deaths, ratio, d->points, clag, cping, d->clientnum, status, colorname(d), ign);
}

void renderteamscore(teamscore *t)
{
    if(!scorelines.empty()) // space between teams
    {
        sline &space = scorelines.add();
        space.s[0] = 0;
    }
    sline &line = scorelines.add();
    defformatstring(plrs)("(%d %s)", t->teammembers.length(), t->teammembers.length() == 1 ? "player" : "players");
//    const char *sr = scoreratio(t->frags, t->deaths);
    float ratio = (float)(t->frags >= 0 ? t->frags : 0) / (float)(t->deaths > 0 ? t->deaths : 1);
    if(m_flags) formatstring(line.s)("%d\t%d\t%d\t%.1f\t%d\t\t\t\t%s\t\t%s", t->flagscore, t->frags, t->deaths, ratio, t->points, team_string(t->team), plrs);
    else formatstring(line.s)("%d\t%d\t%.1f\t%d\t\t\t\t%s\t\t%s", t->frags, t->deaths, ratio, t->points, team_string(t->team), plrs);
    static color teamcolors[2] = { color(1.0f, 0, 0, 0.2f), color(0, 0, 1.0f, 0.2f) };
    line.bgcolor = &teamcolors[team_base(t->team)];
    loopv(t->teammembers) renderscore(t->teammembers[i]);
}

extern bool watchingdemo;

void renderscores(void *menu, bool init)
{
    static string modeline, serverline;

    modeline[0] = '\0';
    serverline[0] = '\0';
    scorelines.shrink(0);

    vector<playerent *> scores;
    if(!watchingdemo) scores.add(player1);
    loopv(players) if(players[i]) scores.add(players[i]);
    scores.sort(scorecmp);
    discscores.sort(discscorecmp);

    int spectators = 0;
    loopv(scores) if(scores[i]->team == TEAM_SPECT) spectators++;
    loopv(discscores) if(discscores[i].team == TEAM_SPECT) spectators++;

    if(getclientmap()[0])
    {
        bool fldrprefix = !strncmp(getclientmap(), "maps/", strlen("maps/"));
        formatstring(modeline)("\"%s\" on map %s", modestr(gamemode, modeacronyms > 0), fldrprefix ? getclientmap()+strlen("maps/") : getclientmap());
    }

    extern int minutesremaining;
    if((gamemode>1 || (gamemode==0 && (multiplayer(false) || watchingdemo))) && minutesremaining >= 0)
    {
        if(!minutesremaining) concatstring(modeline, ", intermission");
        else
        {
            defformatstring(timestr)(", %d %s remaining", minutesremaining, minutesremaining==1 ? "minute" : "minutes");
            concatstring(modeline, timestr);
        }
    }

    if(multiplayer(false))
    {
        serverinfo *s = getconnectedserverinfo();
        if(s)
        {
            if(servstate.mastermode > MM_OPEN) concatformatstring(serverline, servstate.mastermode == MM_MATCH ? "M%d " : "P ", servstate.matchteamsize);
            concatformatstring(serverline, "%s:%d %s", s->name, s->port, s->sdesc);
        }
    }

    if(m_teammode)
    {
        teamscore teamscores[2] = { teamscore(TEAM_CLA), teamscore(TEAM_RVSF) };

        loopv(scores) if(scores[i]->team != TEAM_SPECT)
        {
            teamscores[team_base(scores[i]->team)].addplayer(scores[i]);
        }

        loopv(discscores) if(discscores[i].team != TEAM_SPECT)
        {
            teamscores[team_base(discscores[i].team)].addscore(discscores[i]);
        }

        int sort = teamscorecmp(&teamscores[TEAM_CLA], &teamscores[TEAM_RVSF]) < 0 ? 0 : 1;
        loopi(2)
        {
            renderteamscore(&teamscores[sort ^ i]);
            renderdiscscores(sort ^ i);
        }
    }
    else
    { // ffa mode

        loopv(scores) if(scores[i]->team != TEAM_SPECT) renderscore(scores[i]);
        loopi(2) renderdiscscores(i);
    }
    if(spectators)
    {
        if(!scorelines.empty()) // space between teams and spectators
        {
            sline &space = scorelines.add();
            space.s[0] = 0;
        }
        renderdiscscores(TEAM_SPECT);
        loopv(scores) if(scores[i]->team == TEAM_SPECT) renderscore(scores[i]);
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
    vector<playerent *> scores;

    if(!buf) buf = (char *) malloc(MAXJPGCOM +1);
    if(!buf) return "";

    if(!watchingdemo) scores.add(player1);
    loopv(players) if(players[i]) scores.add(players[i]);
    scores.sort(scorecmp);

    buf[0] = '\0';
    if(destjpg)
    {
        formatstring(text)("AssaultCube Screenshot (%s)\n", asctime());
        addstr(buf, text);
    }
    if(getclientmap()[0])
    {
        formatstring(text)("\n\"%s\" on map %s", modestr(gamemode, 0), getclientmap(), asctime());
        addstr(buf, text);
    }
    if(multiplayer(false))
    {
        serverinfo *s = getconnectedserverinfo();
        if(s)
        {
            string sdesc;
			filtertext(sdesc, s->sdesc, 1);
			formatstring(text)(", %s:%d %s", s->name, s->port, sdesc);
            addstr(buf, text);
        }
    }
    if(destjpg)
        addstr(buf, "\n");
    else
    {
        formatstring(text)("\n%sfrags deaths ratio cn%s name\n", m_flags ? "flags " : "", m_teammode ? " team" : "");
        addstr(buf, text);
    }
    loopv(scores)
    {
        d = scores[i];
        const char *sr = scoreratio(d->frags, d->deaths);
        formatstring(team)(destjpg ? ", %s" : " %-4s", team_string(d->team, true));
        formatstring(flags)(destjpg ? "%d/" : " %4d ", d->flagscore);
        if(destjpg)
            formatstring(text)("%s%s (%s%d/%d)\n", d->name, m_teammode ? team : "", m_flags ? flags : "", d->frags, d->deaths);
        else
            formatstring(text)("%s %4d   %4d %5s %2d%s %s%s\n", m_flags ? flags : "", d->frags, d->deaths, sr, d->clientnum,
                            m_teammode ? team : "", d->name, d->clientrole==CR_ADMIN ? " (admin)" : d==player1 ? " (you)" : "");
        addstr(buf, text);
    }
    discscores.sort(discscorecmp);
    loopv(discscores)
    {
        discscore &d = discscores[i];
        const char *sr = scoreratio(d.frags, d.deaths);
        formatstring(team)(destjpg ? ", %s" : " %-4s", team_string(d.team, true));
        formatstring(flags)(destjpg ? "%d/" : " %4d ", d.flags);
        if(destjpg)
            formatstring(text)("%s(disconnected)%s (%s%d/%d)\n", d.name, m_teammode ? team : "", m_flags ? flags : "", d.frags, d.deaths);
        else
            formatstring(text)("%s %4d   %4d %5s --%s %s(disconnected)\n", m_flags ? flags : "", d.frags, d.deaths, sr, m_teammode ? team : "", d.name);
        addstr(buf, text);
    }
    if(destjpg)
    {
        extern int minutesremaining;
        formatstring(text)("(%sfrags/deaths), %d minute%s remaining\n", m_flags ? "flags/" : "", minutesremaining, minutesremaining == 1 ? "" : "s");
        addstr(buf, text);
    }
    return buf;
}

void consolescores()
{
    printf("%s\n", asciiscores());
}
