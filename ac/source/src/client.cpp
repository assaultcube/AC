// client.cpp, mostly network related client game code

#include "cube.h"

ENetHost *clienthost = NULL;
int connecting = 0;
int connattempts = 0;
int disconnecting = 0;
int clientnum = -1;         // our client id in the game
bool c2sinit = false;       // whether we need to tell the other clients our stats

int getclientnum() { return clientnum; };

bool multiplayer()
{
    // check not correct on listen server?
    if(clienthost) conoutf("operation not available in multiplayer");
    return clienthost!=NULL;
};

bool allowedittoggle()
{
    bool allow = !clienthost || gamemode==1;
    if(!allow) conoutf("editing in multiplayer requires coopedit mode (1)");
    return allow; 
};

VARF(rate, 0, 0, 25000, if(clienthost && (!rate || rate>1000)) enet_host_bandwidth_limit (clienthost, rate, rate));

void throttle();

VARF(throttle_interval, 0, 5, 30, throttle());
VARF(throttle_accel,    0, 2, 32, throttle());
VARF(throttle_decel,    0, 2, 32, throttle());

void throttle()
{
    if(!clienthost || connecting) return;
    ASSERT(ENET_PEER_PACKET_THROTTLE_SCALE==32);
    enet_peer_throttle_configure(clienthost->peers, throttle_interval*1000, throttle_accel, throttle_decel);
};

void ctf_team(char *name)
{   
    bool isvalidteam = strcmp(name, "RVSF") == 0 || strcmp(name, "CLA") == 0;
    if(strcmp(name, player1->team) == 0 && isvalidteam) return;
    c2sinit = false;
    ctf_death();
    if(isvalidteam) strcpy(player1->team, name);
    else strcpy(player1->team, rb_team_string(rnd(2)));
    player1->lastaction = lastmillis;
    player1->state = CS_DEAD;
};

void newname(char *name) { c2sinit = false; s_strncpy(player1->name, name, 16); };
void newskin(int skin) { player1->nextskin = max(0, min(skin, (rb_team_int(player1->team)==TEAM_CLA ? 3 : 5))); };
void newteam(char *name)
{
    if(m_teammode) ctf_team(name);
    else { s_strncpy(player1->team, name, 5); c2sinit=false; };
};

COMMANDN(team, newteam, ARG_1STR);
COMMANDN(name, newname, ARG_1STR);
COMMANDN(skin, newskin, ARG_1INT);

void connects(char *servername)
{   
    disconnect(1);  // reset state
    addserver(servername);

    conoutf("attempting to connect to %s", servername);
    ENetAddress address = { ENET_HOST_ANY, CUBE_SERVER_PORT };
    if(enet_address_set_host(&address, servername) < 0)
    {
        conoutf("could not resolve server %s", servername);
        return;
    };

    clienthost = enet_host_create(NULL, 1, rate, rate);

    if(clienthost)
    {
        enet_host_connect(clienthost, &address, 1); 
        enet_host_flush(clienthost);
        connecting = lastmillis;
        connattempts = 0;
    }
    else
    {
        conoutf("could not connect to server");
        disconnect();
    };
};

void disconnect(int onlyclean, int async)
{
    if(clienthost) 
    {
        if(!connecting && !disconnecting) 
        {
            enet_peer_disconnect(clienthost->peers);
            enet_host_flush(clienthost);
            disconnecting = lastmillis;
        };
        if(clienthost->peers->state != ENET_PEER_STATE_DISCONNECTED)
        {
            if(async) return;
            enet_peer_reset(clienthost->peers);
        };
        enet_host_destroy(clienthost);
    };

    if(clienthost && !connecting) conoutf("disconnected");
    clienthost = NULL;
    connecting = 0;
    connattempts = 0;
    disconnecting = 0;
    clientnum = -1;
    c2sinit = false;
    player1->lifesequence = 0;
    loopv(players) zapdynent(players[i]);
    
    localdisconnect();

    if(!onlyclean) { stop(); localconnect(); };
};

void trydisconnect()
{
    if(!clienthost)
    {
        conoutf("not connected");
        return;
    };
    if(connecting) 
    {
        conoutf("aborting connection attempt");
        disconnect();
        return;
    };
    conoutf("attempting to disconnect...");
    disconnect(0, !disconnecting);
};

string ctext;
void toserver(char *text) { conoutf("%s:\f %s", player1->name, text); s_strncpy(ctext, text, 80); };
void echo(char *text) { conoutf("%s", text); };

COMMAND(echo, ARG_VARI);
COMMANDN(say, toserver, ARG_VARI);
COMMANDN(connect, connects, ARG_1STR);
COMMANDN(disconnect, trydisconnect, ARG_NONE);

// collect c2s messages conveniently

vector<ivector> messages;

void addmsg(int rel, int num, int type, ...)
{
    if(demoplayback) return;
    if(num!=msgsizelookup(type)) { s_sprintfd(s)("inconsistant msg size for %d (%d != %d)", type, num, msgsizelookup(type)); fatal(s); };
    if(messages.length()==100) { conoutf("command flood protection (type %d)", type); return; };
    ivector &msg = messages.add();
    msg.add(num);
    msg.add(rel);
    msg.add(type);
    va_list marker;
    va_start(marker, type); 
    loopi(num-1) msg.add(va_arg(marker, int));
    va_end(marker);  
};

void server_err()
{
    conoutf("server network error or player kick, disconnecting...");
    disconnect();
};

int lastupdate = 0, lastping = 0;
string toservermap;
bool senditemstoserver = false;     // after a map change, since server doesn't have map data

string clientpassword;
char *toserverpwd = NULL;
void password(char *p) { s_strcpy(clientpassword, p); };
COMMAND(password, ARG_1STR);

bool netmapstart() { senditemstoserver = true; return clienthost!=NULL; };

void initclientnet()
{
    ctext[0] = 0;
    toservermap[0] = 0;
    clientpassword[0] = 0;
	toserverpwd = NULL;
    newname("unnamed");
    ctf_team("red");
};

void sendpackettoserv(void *packet)
{
    if(clienthost) { enet_host_broadcast(clienthost, 0, (ENetPacket *)packet); enet_host_flush(clienthost); }
    else localclienttoserver((ENetPacket *)packet);
}

void c2skeepalive()
{
	if(clientnum<0 || !clienthost) return;
	if(lastmillis-lastupdate<40) return;

	ENetEvent event;
	while(clienthost!=NULL && enet_host_service(clienthost, &event, 0)>0) {}; // fixme

	lastupdate = SDL_GetTicks();
};

extern string masterpwd;

void c2sinfo(dynent *d)                     // send update to the server
{
    if(clientnum<0) return;                 // we haven't had a welcome message from the server yet
    if(lastmillis-lastupdate<40) return;    // don't update faster than 25fps
    ENetPacket *packet = enet_packet_create (NULL, MAXTRANS, 0);
    uchar *start = packet->data;
    uchar *p = start+2;
    bool serveriteminitdone = false;
	if(toserverpwd)
	{
		packet->flags = ENET_PACKET_FLAG_RELIABLE;
		putint(p, SV_PWD);
		sendstring(toserverpwd, p);
		toserverpwd[0] = 0;
		toserverpwd = NULL;
	}
    else if(toservermap[0])                      // suggest server to change map
    {                                       // do this exclusively as map change may invalidate rest of update
        packet->flags = ENET_PACKET_FLAG_RELIABLE;
        putint(p, SV_MAPCHANGE);
        sendstring(toservermap, p);
        toservermap[0] = 0;
        putint(p, nextmode);
    }
	else if(masterpwd[0])
	{
		packet->flags = ENET_PACKET_FLAG_RELIABLE;
		putint(p, SV_GETMASTER);
		sendstring(masterpwd, p);
		masterpwd[0] = 0;
	}
    else
    {
        putint(p, SV_POS);
        putint(p, clientnum);
        putint(p, (int)(d->o.x*DMF));       // quantize coordinates to 1/16th of a cube, between 1 and 3 bytes
        putint(p, (int)(d->o.y*DMF));
        putint(p, (int)(d->o.z*DMF));
        putint(p, (int)(d->yaw*DAF));
        putint(p, (int)(d->pitch*DAF));
        putint(p, (int)(d->roll*DAF));
        putint(p, (int)(d->vel.x*DVF));     // quantize to 1/100, almost always 1 byte
        putint(p, (int)(d->vel.y*DVF));
        putint(p, (int)(d->vel.z*DVF));
        // pack rest in 1 byte: strafe:2, move:2, onfloor:1, state:3
        putint(p, (d->strafe&3) | ((d->move&3)<<2) | (((int)d->onfloor)<<4) | ((editmode ? CS_EDITING : d->state)<<5) );
 
        if(gun_changed)
        {
            putint(p, SV_WEAPCHANGE);
            putint(p, player1->gunselect);
            gun_changed = false;       
        };
        if(senditemstoserver)
        {
            packet->flags = ENET_PACKET_FLAG_RELIABLE;
            putint(p, SV_ITEMLIST);
            if(!m_noitems) putitems(p);
            putint(p, -1);
            senditemstoserver = false;
            serveriteminitdone = true;
        };
        if(ctext[0])    // player chat, not flood protected for now
        {
            packet->flags = ENET_PACKET_FLAG_RELIABLE;
            putint(p, SV_TEXT);
            sendstring(ctext, p);
            ctext[0] = 0;
        };
        if(!c2sinit)    // tell other clients who I am
        {
            packet->flags = ENET_PACKET_FLAG_RELIABLE;
            c2sinit = true;
            putint(p, SV_INITC2S);
            sendstring(player1->name, p);
            sendstring(player1->team, p);
            putint(p, player1->skin);
            putint(p, player1->lifesequence);
        };
        loopv(messages)     // send messages collected during the previous frames
        {
            ivector &msg = messages[i];
            if(msg[1]) packet->flags = ENET_PACKET_FLAG_RELIABLE;
            loopi(msg[0]) putint(p, msg[i+2]);
        };
        messages.setsize(0);
        if(lastmillis-lastping>250)
        {
            putint(p, SV_PING);
            putint(p, lastmillis);
            lastping = lastmillis;
        };
    };
    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
    enet_packet_resize(packet, p-start);
    incomingdemodata(start, p-start, true);
    if(clienthost) { enet_host_broadcast(clienthost, 0, packet); enet_host_flush(clienthost); }
    else localclienttoserver(packet);
    lastupdate = lastmillis;
    if(serveriteminitdone) loadgamerest();  // hack
};

void gets2c()           // get updates from the server
{
    ENetEvent event;
    if(!clienthost) return;
    if(connecting && lastmillis/3000 > connecting/3000)
    {
        conoutf("attempting to connect...");
        connecting = lastmillis;
        ++connattempts; 
        if(connattempts > 3)
        {
            conoutf("could not connect to server");
            disconnect();
            return;
        };
    };
    while(clienthost!=NULL && enet_host_service(clienthost, &event, 0)>0)
    switch(event.type)
    {
        case ENET_EVENT_TYPE_CONNECT:
            conoutf("connected to server");
            connecting = 0;
            throttle();
            break;
         
        case ENET_EVENT_TYPE_RECEIVE:
            if(disconnecting) 
				conoutf("attempting to disconnect...");
            else localservertoclient(event.packet->data, event.packet->dataLength);
            enet_packet_destroy(event.packet);
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            if(disconnecting) disconnect();
            else 
				server_err();
            return;
    }
};
