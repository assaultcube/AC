// clientextras.cpp: stuff that didn't fit in client.cpp or clientgame.cpp :)

#include "cube.h"

// render players & monsters
// very messy ad-hoc handling of animation frames, should be made more configurable

//              D    D    D    D'   D    D    D    D'   A   A'  P   P'  I   I' R,  R'  E    L    J   J'
int frame[] = { 178, 184, 190, 137, 183, 189, 197, 164, 46, 51, 54, 32, 0,  0, 40, 1,  162, 162, 67, 168 };
int range[] = { 6,   6,   8,   28,  1,   1,   1,   1,   8,  19, 4,  18, 40, 1, 6,  15, 1,   1,   1,  1   };

void renderclient(dynent *d, bool team, char *mdlname, bool vwep, float scale)
{
    int n = 3;
    float speed = 100.0f;
    float mz = d->o.z-d->eyeheight+1.55f*scale;
    int basetime = -((int)d&0xFFF);
    if(d->state==CS_DEAD)
    {
        if(vwep) mz = S((int)d->o.x, (int)d->o.y)->floor;
        int r;
        n = (int)d%3; r = range[n];
        basetime = d->lastaction;
        int t = lastmillis-d->lastaction;
        if(t<0 || t>20000) return;
        if(t>(r-1)*100) { n += 4; if(t>(r+10)*100) { t -= (r+10)*100; mz -= t*t/10000000000.0f*t; }; };
        if(mz<-1000) return;
        //mdl = (((int)d>>6)&1)+1;
        //mz = d->o.z-d->eyeheight+0.2f;
        //scale = 1.2f;
        if(vwep) 
        {
            rendermodel(mdlname, 0, 1, 0, 1.5f, d->o.x, mz, d->o.y, -90, 90, team, scale, speed, 0, basetime);
            return;
        };
    }
    else if(d->state==CS_EDITING)                   { n = 16; }
    else if(d->state==CS_LAGGED)                    { n = 17; }
    else if(d->monsterstate==M_ATTACKING)           { n = 8;  }
    else if(d->monsterstate==M_PAIN)                { n = 10; } 
    else if((!d->move && !d->strafe) || !d->moving) { n = 12; } 
    else if(!d->onfloor && d->timeinair>100)        { n = 18; }
    else                                            { n = 14; speed = 1200/d->maxspeed*scale; }; 
    rendermodel(mdlname, frame[n], range[n], 0, 1.5f, d->o.x, mz, d->o.y, d->yaw+90, d->pitch/2, team, scale, speed, 0, basetime);
};

extern int democlientnum;

void renderclients()
{
    dynent *d;
    loopv(players)
          if((d = players[i]) && (!demoplayback || i!=democlientnum))
          {
            //if (d->state==CS_DEAD) break; //cheap hack fix of 
            if(strcmp(d->name, "dummy") == 0)
            {
                d->gunselect = player1->gunselect;
                d->moving = true; 
                d->health != 1 ? d->state=CS_ALIVE : d->state=CS_DEAD;
            };
            
            if (!strcmp(d->team,"CT"))
            {
                  renderclient(d, isteam(player1->team, d->team), "playermodels/counterterrorist", false,1.4f);
            }
            else //if (!strcmp(d->team,"T"))
            {
                  renderclient(d, isteam(player1->team, d->team), "playermodels/terrorist", false, 1.4f);
            }
            
            if(d->gunselect>0 && d->gunselect<NUMGUNS)
            {
                sprintf_sd(vwep)("weapons/%s/world", hudgunnames[d->gunselect]);
                renderclient(d, isteam(player1->team, d->team), vwep, true, 1.4f);
            } else sprintf("hudgun nr out of range, %i\n", d->name);
            
          };
    if(player1->state==CS_DEAD)
    {
        if (!strcmp(player1->team,"CT"))
            renderclient(player1, true, "playermodels/counterterrorist", false,1.4f);
        else
            renderclient(player1, true, "playermodels/terrorist", false, 1.4f);

            sprintf_sd(vwep)("weapons/%s/world", hudgunnames[player1->gunselect]);
            renderclient(player1, true, vwep, true, 1.4f);
    };
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
    menuset(((int)on)-1);
};

struct sline { string s; };
vector<sline> scorelines;

void renderscore(dynent *d)
{
    sprintf_sd(lag)("%d", d->plag);
    sprintf_sd(name) ("(%s)", d->name); 
    sprintf_s(scorelines.add().s)("%d\t%s\t%d\t%s\t%s", d->frags, d->state==CS_LAGGED ? "LAG" : lag, d->ping, (int)d->team, d->state==CS_DEAD ? name : d->name);
    menumanual(0, scorelines.length()-1, scorelines.last().s); 
};

const int maxteams = 4;
char *teamname[maxteams];
int teamscore[maxteams], teamsused;
string teamscores;
int timeremain = 0;

void addteamscore(dynent *d)
{
    if(!d) return;
    loopi(teamsused) if(strcmp(teamname[i], d->team)==0) { teamscore[i] += d->frags; return; };
    if(teamsused==maxteams) return;
    teamname[teamsused] = d->team;
    teamscore[teamsused++] = d->frags;
};

void renderscores()
{
    if(!scoreson) return;
    scorelines.setsize(0);
    if(!demoplayback) renderscore(player1);
    loopv(players) if(players[i]) renderscore(players[i]);
    sortmenu(0, scorelines.length());
    if(m_teammode)
    {
        teamsused = 0;
        loopv(players) addteamscore(players[i]);
        if(!demoplayback) addteamscore(player1);
        teamscores[0] = 0;
        loopj(teamsused)
        {
            sprintf_sd(sc)("[ %s: %d ]", teamname[j], teamscore[j]);
            strcat_s(teamscores, sc);
        };
        menumanual(0, scorelines.length(), "");
        menumanual(0, scorelines.length()+1, teamscores);
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

