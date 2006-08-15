// clientextras.cpp: stuff that didn't fit in client.cpp or clientgame.cpp :)

#include "cube.h"

// render players & monsters
// very messy ad-hoc handling of animation frames, should be made more configurable
// idle, run, attack, pain, jump, land, flipoff, salute, taunt, wave, point, crouch idle, crouch walk, crouch attack, crouch pain, crouch death, death, lying dead
//				I	R	A	P	P	P	J	L	F	S	T	W	P	CI	CW	CA	CP	CD	D	D	D	LD	LD	LD
int frame[] = {	0,	40,	46,	54,	58,	62,	66,	69,	72,	84,	95,	112,123,135,154,160,169,173,178,184,190,183,189,197 };
int range[] = {	40,	5,	8,	4, 	4,	4,	3,	3,	12,	11,	17,	11,	13,	19,	6,	9,	4,	5,	6,	6,	8,	1,	1,	1 };

//              D    D    D    D'   D    D    D    D'   A   A'  P   P'  I   I' R,  R'  Q    L    J   J'
/*int frame[] = { 178, 184, 190, 137, 183, 189, 197, 164, 46, 51, 54, 32, 0,  0, 40, 1,  154, 162, 67, 168 };
int range[] = { 6,   6,   8,   28,  1,   1,   1,   1,   8,  19, 4,  18, 40, 1, 6,  15, 6,   1,   1,  1   };*/

VAR(die,0,0,1);

void renderclient(dynent *d, bool team, char *mdlname, float scale)
{
    int n = 3;
	int oldaniminterpt = -1;
    float speed = 100.0f;
    float mz = d->o.z-d->eyeheight+1.55f*scale;
    int basetime = -((int)d&0xFFF);
    if(d->state==CS_DEAD)
    {
        d->pitch = 0.1f;
        int r;
		n = (d->lastaction%3)+18;
		r = range[n];
        basetime = d->lastaction;
        int t = lastmillis-d->lastaction;
        if(t<0 || t>20000) return;
        if(t>(r-1)*100-50) 
		{ 
			n += 3;
			if(t>(r+10)*100) 
			{ 
				t -= (r+10)*100; 
				mz -= t*t/10000000000.0f*t; 
			};
			oldaniminterpt = getvar("animationinterpolationtime"); // disable anim interp. temporarly
			setvar("animationinterpolationtime", 0);
		};
        //if(mz<-1000) return;
    }
    else if(d->state==CS_EDITING)                   { n = 8; }
    else if(d->state==CS_LAGGED)                    { n = 13; }
    else if((!d->move && !d->strafe) /*|| !d->moving*/) { n = 0; }
    else if(!d->onfloor && d->timeinair>100)        { n = 6; }
    else                                            { n = 1; speed = 1200/d->maxspeed*scale; }; 
    rendermodel(mdlname, frame[n], range[n], 0, 1.5f, d->o.x, mz, d->o.y, d->yaw+90, d->pitch/2, team, scale, speed, 0, basetime, true, d);
	if(oldaniminterpt!=-1) setvar("animationinterpolationtime", oldaniminterpt);
};

extern int democlientnum;

void renderplayer(dynent *d)
{
    if(!d) return;
   
    int team = rb_team_int(d->team);
    sprintf_sd(mdl)("playermodels/%s/0%i", team==TEAM_CLA ? "terrorist" : "counterterrorist", 1 + max(0, min(d->skin, (team==TEAM_CLA ? 3 : 5))));
    renderclient(d, isteam(player1->team, d->team), mdl, 1.6f);
    
    if(d->gunselect>=0 && d->gunselect<NUMGUNS)
    {
        sprintf_sd(vwep)("weapons/%s/world", hudgunnames[d->gunselect]);
        renderclient(d, isteam(player1->team, d->team), vwep, 1.6f);
    };
}

void renderclients()
{
    dynent *d;
    loopv(players)
    if((d = players[i]) && (!demoplayback || i!=democlientnum))
    {
        if(strcmp(d->name, "dummy") == 0)
        {
            d->gunselect = player1->gunselect;
            d->moving = true; 
            d->health != 1 ? d->state=CS_ALIVE : d->state=CS_DEAD;
        };
        renderplayer(d);
    };
    
    if(player1->state==CS_DEAD) renderplayer(player1);
};

void spawn_dummy()
{
    dynent *d = newdynent();
    players.add(d);
    d->o = player1->o;
    strcpy(d->team, player1->team);
    d->gunselect = player1->gunselect;
    d->monsterstate = M_NONE;
    d->state = CS_ALIVE;
    strcpy(d->name, "dummy");
}; COMMAND(spawn_dummy, ARG_NONE);

// creation of scoreboard pseudo-menu

bool scoreson = false;

void showscores(bool on)
{
    scoreson = on;
//    menuset(((int)on)-1);
    menuset(on ? (m_ctf ? 2 : 0) : -1); // EDIT: AH
};

struct sline { string s; };
vector<sline> scorelines;
int menu = 0;

void renderscore(dynent *d)
{
    sprintf_sd(lag)("%d", d->plag);
    sprintf_sd(name) ("(%s)", d->name); 
    if(m_ctf) sprintf_s(scorelines.add().s)("%d\t%d\t%s\t%d\t%s\t%s", d->flagscore, d->frags, d->state==CS_LAGGED ? "LAG" : lag, d->ping, d->team, d->state==CS_DEAD ? name : d->name);
    else sprintf_s(scorelines.add().s)("%d\t%s\t%d\t%s\t%s", d->frags, d->state==CS_LAGGED ? "LAG" : lag, d->ping, d->team, d->state==CS_DEAD ? name : d->name);
    menumanual(menu, scorelines.length()-1, scorelines.last().s);
};

const int maxteams = 4;
char *teamname[maxteams];
int teamscore[maxteams], teamflagscore[maxteams], teamsused; // EDIT: AH
string teamscores;
int timeremain = 0;

void addteamscore(dynent *d)
{
    if(!d) return;
    loopi(teamsused) if(strcmp(teamname[i], d->team)==0) 
    { 
        teamscore[i] += d->frags; 
        if(m_ctf) teamflagscore[i] += d->flagscore;    
        return; 
    };
    if(teamsused==maxteams) return;
    teamname[teamsused] = d->team;
    teamscore[teamsused] = d->frags;
    if(m_ctf) teamflagscore[teamsused++] = d->flagscore;
};

void renderscores()
{
    if(!scoreson) return;
    menu = m_ctf ? 2 : 0;
    scorelines.setsize(0);
    if(!demoplayback) renderscore(player1);
    loopv(players) if(players[i]) renderscore(players[i]);

    // Added by Rick: Render Score for bots
    BotManager.RenderBotScore();

    sortmenu(menu, 0, scorelines.length());
    if(m_teammode)
    {
        teamsused = 0;
        loopv(players) addteamscore(players[i]);
        if(!demoplayback) addteamscore(player1);
        teamscores[0] = 0;
        loopj(teamsused)
        {
            string sc;
            if(m_ctf) sprintf_s(sc)("[ %s: %d %d ]", teamname[j], teamflagscore[j], teamscore[j]);
            else sprintf_s(sc)("[ %s: %d ]", teamname[j], teamscore[j]);
            strcat_s(teamscores, sc);
        };
        menumanual(menu, scorelines.length(), "");
        menumanual(menu, scorelines.length()+1, teamscores);
    };
};

// sendmap/getmap commands, should be replaced by more intuitive map downloading

void sendmap(char *mapname)
{
    if(*mapname) save_world(mapname);
    changemap(mapname);
    mapname = getclientmap();
    int mapsize;
    uchar *mapdata = readmap(mapname, &mapsize); 
    if(!mapdata) return;
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS + mapsize, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start+2;
    putint(p, SV_SENDMAP);
    sendstring(mapname, p);
    putint(p, mapsize);
    if(65535 - (p - start) < mapsize)
    {
        conoutf("map %s is too large to send", (int)mapname);
        free(mapdata);
        enet_packet_destroy(packet);
        return;
    };
    memcpy(p, mapdata, mapsize);
    p += mapsize;
    free(mapdata); 
    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
    enet_packet_resize(packet, p-start);
    sendpackettoserv(packet);
    conoutf("sending map %s to server...", (int)mapname);
    sprintf_sd(msg)("[map %s uploaded to server, \"getmap\" to receive it]", mapname);
    toserver(msg);
}

void getmap()
{
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start+2;
    putint(p, SV_RECVMAP);
    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
    enet_packet_resize(packet, p-start);
    sendpackettoserv(packet);
    conoutf("requesting map from server...");
}

COMMAND(sendmap, ARG_1STR);
COMMAND(getmap, ARG_NONE);

