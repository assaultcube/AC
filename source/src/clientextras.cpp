// clientextras.cpp: stuff that didn't fit in client.cpp or clientgame.cpp :)

#include "cube.h"

void renderclient(playerent *d, char *mdlname, char *vwepname)
{
    int varseed = (int)(size_t)d;
    int anim = ANIM_IDLE|ANIM_LOOP;
    float speed = 100.0f;
    float mz = d->o.z-d->eyeheight;
    int basetime = -((int)(size_t)d&0xFFF);
    if(d->state==CS_DEAD)
    {
		loopv(bounceents) if(bounceents[i]->bouncestate==GIB && bounceents[i]->owner==d) return;
        d->pitch = 0.1f;
        int r = 6;
        anim = ANIM_DEATH;
        varseed += d->lastaction;
        basetime = d->lastaction;
        int t = lastmillis-d->lastaction;
        if(t<0 || t>20000) return;
        if(t>(r-1)*100-50) 
		{ 
			anim = ANIM_LYING_DEAD|ANIM_NOINTERP|ANIM_LOOP;
			if(t>(r+10)*100) 
			{ 
				t -= (r+10)*100; 
				mz -= t*t/10000000000.0f*t; 
			};
		};
        //if(mz<-1000) return;
    }
    else if(d->state==CS_EDITING)                   { anim = ANIM_JUMP|ANIM_END; }
    else if(d->state==CS_LAGGED)                    { anim = ANIM_SALUTE|ANIM_LOOP; }
    else if(lastmillis-d->lastpain<300)             { anim = ANIM_PAIN; speed = 300.0f/4; varseed += d->lastpain; basetime = d->lastpain; }
    else if(!d->onfloor && d->timeinair>0)          { anim = ANIM_JUMP|ANIM_END; }
    else if(d->gunselect==d->lastattackgun && lastmillis-d->lastaction<300)
                                                    { anim = ANIM_ATTACK; speed = 300.0f/8; basetime = d->lastaction; }
    else if(!d->move && !d->strafe)                 { anim = ANIM_IDLE|ANIM_LOOP; }
    else                                            { anim = ANIM_RUN|ANIM_LOOP; speed = 1860/d->maxspeed; };
    rendermodel(mdlname, anim, 0, 1.5f, d->o.x, mz, d->o.y, d->yaw+90, d->pitch/4, speed, basetime, d, vwepname);
};

extern int democlientnum;

void renderplayer(playerent *d)
{
    if(!d) return;
   
    int team = rb_team_int(d->team);
    s_sprintfd(mdl)("playermodels/%s/0%i", team==TEAM_CLA ? "terrorist" : "counterterrorist", 1 + max(0, min(d->skin, (team==TEAM_CLA ? 3 : 5))));
    string vwep;
    if(d->gunselect>=0 && d->gunselect<NUMGUNS) s_sprintf(vwep)("weapons/%s/world", hudgunnames[d->gunselect]);
    else vwep[0] = 0;
    renderclient(d, mdl, vwep[0] ? vwep : NULL);
};

void renderclients()
{
    playerent *d;
    loopv(players) if((d = players[i]) && (!demoplayback || i!=democlientnum))
    {
        if(strcmp(d->name, "dummy") == 0)
        {
            d->gunselect = player1->gunselect;
            d->health != 1 ? d->state=CS_ALIVE : d->state=CS_DEAD;
        };
        renderplayer(d);
    };
    
    if(player1->state==CS_DEAD) renderplayer(player1);
};

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

void renderscore(playerent *d)
{
    s_sprintfd(lag)("%d", d->plag);
    s_sprintfd(name) ("(%s)", d->name); 
    if(m_ctf) s_sprintf(scorelines.add().s)("%d\t%d\t%s\t%d\t%s\t%s", d->flagscore, d->frags, d->state==CS_LAGGED ? "LAG" : lag, d->ping, d->team, d->state==CS_DEAD ? name : d->name);
	else s_sprintf(scorelines.add().s)("%d\t%s\t%d\t%s\t%s", d->frags, d->state==CS_LAGGED ? "LAG" : lag, d->ping, m_teammode ? d->team : "", d->state==CS_DEAD ? name : d->name);
    menumanual(menu, scorelines.length()-1, scorelines.last().s);
};

const int maxteams = 4;
char *teamname[maxteams];
int teamscore[maxteams], teamflagscore[maxteams], teamsused; // EDIT: AH
string teamscores;
int timeremain = 0;

void addteamscore(playerent *d)
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
    if(m_ctf) teamflagscore[teamsused] = d->flagscore;
	teamsused++;
};

void renderscores()
{
    if(!scoreson) return;
    menu = m_ctf ? 2 : 0;
    scorelines.setsize(0);
    if(!demoplayback) renderscore(player1);
    loopv(players) if(players[i]) renderscore(players[i]);

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
            if(m_ctf) s_sprintf(sc)("[ %s: %d flags  %d frags ]", teamname[j], teamflagscore[j], teamscore[j]);
            else s_sprintf(sc)("[ %s: %d ]", teamname[j], teamscore[j]);
            s_strcat(teamscores, sc);
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
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_SENDMAP);
    sendstring(mapname, p);
    putint(p, mapsize);
    if(65535 - p.length() < mapsize)
    {
        conoutf("map %s is too large to send", mapname);
        delete[] mapdata;
        enet_packet_destroy(packet);
        return;
    };
    p.put(mapdata, mapsize);
    delete[] mapdata; 
    enet_packet_resize(packet, p.length());
    sendpackettoserv(2, packet);
    conoutf("sending map %s to server...", mapname);
    s_sprintfd(msg)("[map %s uploaded to server, \"getmap\" to receive it]", mapname);
    toserver(msg);
}

void getmap()
{
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_RECVMAP);
    enet_packet_resize(packet, p.length());
    sendpackettoserv(2, packet);
    conoutf("requesting map from server...");
}

COMMAND(sendmap, ARG_1STR);
COMMAND(getmap, ARG_NONE);

