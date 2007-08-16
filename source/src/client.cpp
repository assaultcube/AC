// client.cpp, mostly network related client game code

#include "cube.h"
#include "bot/bot.h"

VAR(connected, 1, 0, 0);

ENetHost *clienthost = NULL;
ENetPeer *curpeer = NULL, *connpeer = NULL;
int connmillis = 0, connattempts = 0, discmillis = 0;
bool c2sinit = false;       // whether we need to tell the other clients our stats

int getclientnum() { return player1 ? player1->clientnum : -1; }

bool multiplayer(bool msg)
{
    // check not correct on listen server?
    if(curpeer && msg) conoutf("operation not available in multiplayer");
    return curpeer!=NULL;
}

bool allowedittoggle()
{
    bool allow = !curpeer || gamemode==1;
    if(!allow) conoutf("editing in multiplayer requires coopedit mode (1)");
    return allow; 
}

void setrate(int rate)
{
   if(!curpeer) return;
   enet_host_bandwidth_limit(clienthost, rate, rate);
}

VARF(rate, 0, 0, 25000, setrate(rate));

void throttle();

VARF(throttle_interval, 0, 5, 30, throttle());
VARF(throttle_accel,    0, 2, 32, throttle());
VARF(throttle_decel,    0, 2, 32, throttle());

void throttle()
{
    if(!curpeer) return;
    ASSERT(ENET_PEER_PACKET_THROTTLE_SCALE==32);
    enet_peer_throttle_configure(curpeer, throttle_interval*1000, throttle_accel, throttle_decel);
}

string clientpassword = "";

void abortconnect()
{
    if(!connpeer) return;
    clientpassword[0] = '\0';
    if(connpeer->state!=ENET_PEER_STATE_DISCONNECTED) enet_peer_reset(connpeer);
    connpeer = NULL;
    if(curpeer) return;
    enet_host_destroy(clienthost);
    clienthost = NULL;
}

void connects(char *servername, char *port, char *password)
{   
    if(connpeer)
    {
        conoutf("aborting connection attempt");
        abortconnect();
    }

    s_strcpy(clientpassword, password ? password : "");

    ENetAddress address;
    int p = atoi(port);
    address.port = p > 0 ? p : CUBE_DEFAULT_SERVER_PORT;

    if(servername)
    {
        addserver(servername);
        conoutf("attempting to connect to %s", servername);
        if(!resolverwait(servername, &address))
        {
            conoutf("\f3could not resolve server %s", servername);
            clientpassword[0] = '\0';
            return;
        }
    }
    else
    {
        conoutf("attempting to connect over LAN");
        address.host = ENET_HOST_BROADCAST;
    }

    if(!clienthost) clienthost = enet_host_create(NULL, 2, rate, rate);

    if(clienthost)
    {
        connpeer = enet_host_connect(clienthost, &address, 3); 
        enet_host_flush(clienthost);
        connmillis = lastmillis;
        connattempts = 0;
    }
    else 
    {
        conoutf("\f3could not connect to server");
        clientpassword[0] = '\0';
    }
}

void connectadmin(char *servername, char *password)
{
    if(!password) return;
    connects(servername, password);
    if(clienthost) addmsg(SV_SETADMIN, "ris", 1, password); // in case the server is not private locked or pwd protected
}

void lanconnect()
{
    connects(0);
}  

void disconnect(int onlyclean, int async)
{
    bool cleanup = onlyclean!=0;
    if(curpeer)
    {
        if(!discmillis)
        {
            enet_peer_disconnect(curpeer, DISC_NONE);
            enet_host_flush(clienthost);
            discmillis = lastmillis;
        }
        if(curpeer->state!=ENET_PEER_STATE_DISCONNECTED)
        {
            if(async) return;
            enet_peer_reset(curpeer);
        }
        curpeer = NULL;
        discmillis = 0;
        connected = 0;
        conoutf("disconnected");
        cleanup = true;
    }

    if(cleanup)
    {
        c2sinit = false;
        player1->clientnum = -1;
        player1->lifesequence = 0;
        player1->clientrole = CR_DEFAULT;
        if(m_botmode) BotManager.EndMap();
        loopv(players) zapplayer(players[i]);
        localdisconnect();
    }
    if(!connpeer && clienthost)
    {
        enet_host_destroy(clienthost);
        clienthost = NULL;
    }
    if(!onlyclean) localconnect();
}

void trydisconnect()
{
    if(connpeer)
    {
        conoutf("aborting connection attempt");
        abortconnect();
        return;
    }
    if(!curpeer)
    {
        conoutf("not connected");
        return;
    }
    conoutf("attempting to disconnect...");
    disconnect(0, !discmillis);
}

void toserver(char *text) 
{ 
    bool toteam = text && text[0] == '%';
    if(toteam) text++;
    conoutf("%s:\f%d %s", colorname(player1), toteam ? 1 : 0, text);
    addmsg(toteam ? SV_TEAMTEXT : SV_TEXT, "rs", text);
}

void echo(char *text) { conoutf("%s", text); }

COMMAND(echo, ARG_VARI);
COMMANDN(say, toserver, ARG_VARI);
COMMANDN(connect, connects, ARG_3STR);
COMMAND(connectadmin, ARG_2STR);
COMMAND(lanconnect, ARG_NONE);
COMMANDN(disconnect, trydisconnect, ARG_NONE);

// collect c2s messages conveniently

vector<uchar> messages;

void addmsg(int type, const char *fmt, ...)
{
    static uchar buf[MAXTRANS];
    ucharbuf p(buf, MAXTRANS);
    putint(p, type);
    int numi = 1, nums = 0;
    bool reliable = false;
    if(fmt)
    {
        va_list args;
        va_start(args, fmt);
        while(*fmt) switch(*fmt++)
        {
            case 'r': reliable = true; break;
            case 'v':
            {
                int n = va_arg(args, int);
                int *v = va_arg(args, int *);
                loopi(n) putint(p, v[i]);
                numi += n;
                break;
            }

            case 'i':
            {
                int n = isdigit(*fmt) ? *fmt++-'0' : 1;
                loopi(n) putint(p, va_arg(args, int));
                numi += n;
                break;
            }
            case 's': sendstring(va_arg(args, const char *), p); nums++; break;
        }
        va_end(args);
    }
    int num = nums?0:numi, msgsize = msgsizelookup(type);
    if(msgsize && num!=msgsize) { s_sprintfd(s)("inconsistent msg size for %d (%d != %d)", type, num, msgsize); fatal(s); }
    int len = p.length();
    messages.add(len&0xFF);
    messages.add((len>>8)|(reliable ? 0x80 : 0));
    loopi(len) messages.add(buf[i]);
}

static int lastupdate = 0, lastping = 0, laststate = -1;
bool senditemstoserver = false;     // after a map change, since server doesn't have map data

void sendpackettoserv(int chan, ENetPacket *packet)
{
    if(curpeer) enet_peer_send(curpeer, chan, packet);
    else localclienttoserver(chan, packet);
}

void c2skeepalive()
{
    if(clienthost) enet_host_service(clienthost, NULL, 0);
}

extern string masterpwd;

void c2sinfo(playerent *d)                  // send update to the server
{
    if(d->clientnum<0) return;              // we haven't had a welcome message from the server yet
    if(lastmillis-lastupdate<40) return;    // don't update faster than 25fps
    
    bool hasmsg = senditemstoserver || !c2sinit || messages.length() || lastmillis-lastping>250;
    // limit updates for dead players to the ping rate of 4fps, as above
    if(!hasmsg && laststate==CS_DEAD && player1->state==CS_DEAD) return;
    
    laststate = player1->state;

    ENetPacket *packet = enet_packet_create(NULL, 100, 0);
    ucharbuf q(packet->data, packet->dataLength);

    putint(q, SV_POS);
    putint(q, d->clientnum);
    putuint(q, (int)(d->o.x*DMF));       // quantize coordinates to 1/16th of a cube, between 1 and 3 bytes
    putuint(q, (int)(d->o.y*DMF));
    putuint(q, (int)(d->o.z*DMF));
    putuint(q, (int)d->yaw);
    putint(q, (int)d->pitch);
    putint(q, (int)d->roll);
    putint(q, (int)(d->vel.x*DVELF));
    putint(q, (int)(d->vel.y*DVELF));
    putint(q, (int)(d->vel.z*DVELF));
    // pack rest in 1 int: strafe:2, move:2, onfloor:1, onladder: 1
    putint(q, (d->strafe&3) | ((d->move&3)<<2) | (((int)d->onfloor)<<4) | (((int)d->onladder)<<5) );

    enet_packet_resize(packet, q.length());
    sendpackettoserv(0, packet);

    if(hasmsg)
    {
        packet = enet_packet_create (NULL, MAXTRANS, 0);
        ucharbuf p(packet->data, packet->dataLength);
    
        if(!c2sinit)    // tell other clients who I am
        {
            packet->flags = ENET_PACKET_FLAG_RELIABLE;
            c2sinit = true;
            putint(p, SV_INITC2S);
            sendstring(player1->name, p);
            sendstring(player1->team, p);
            putint(p, player1->skin);
        }
        if(senditemstoserver)
        {
            packet->flags = ENET_PACKET_FLAG_RELIABLE;
            putint(p, SV_ITEMLIST);
            if(!m_noitems) putitems(p);
            putint(p, -1);
            senditemstoserver = false;
        }
        int i = 0;
        while(i < messages.length()) // send messages collected during the previous frames
        {
            int len = messages[i] | ((messages[i+1]&0x7F)<<8);
            if(p.remaining() < len) break;
            if(messages[i+1]&0x80) packet->flags = ENET_PACKET_FLAG_RELIABLE;
            p.put(&messages[i+2], len);
            i += 2 + len;
        }
        messages.remove(0, i);
        if(lastmillis-lastping>250)
        {
            putint(p, SV_PING);
            putint(p, lastmillis);
            lastping = lastmillis;
        }
        if(!p.length()) enet_packet_destroy(packet);
        else
        {
            enet_packet_resize(packet, p.length());
            sendpackettoserv(1, packet);
        }
    }
    if(clienthost) enet_host_flush(clienthost);
    lastupdate = lastmillis;
}

void sendintro()
{
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, 0);
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_CONNECT);
    sendstring(clientpassword, p);
    clientpassword[0] = '\0';
    putint(p, player1->nextprimary);
    enet_packet_resize(packet, p.length());
    sendpackettoserv(1, packet);
}

void gets2c()           // get updates from the server
{
    ENetEvent event;
    if(!clienthost) return;
    if(connpeer && lastmillis/3000 > connmillis/3000)
    {
        conoutf("attempting to connect...");
        connmillis = lastmillis;
        ++connattempts;
        if(connattempts > 3)
        {
            conoutf("\f3could not connect to server");
            abortconnect();
            return;
        }
    }
    while(clienthost!=NULL && enet_host_service(clienthost, &event, 0)>0)
    switch(event.type)
    {
        case ENET_EVENT_TYPE_CONNECT:
            disconnect(1);
            curpeer = connpeer;
            connpeer = NULL;
            connected = 1;
            conoutf("connected to server");
            throttle();
            if(rate) setrate(rate);
            if(editmode) toggleedit();
            sendintro();
            break;
         
        case ENET_EVENT_TYPE_RECEIVE:
            if(discmillis) conoutf("attempting to disconnect...");
            else localservertoclient(event.channelID, event.packet->data, (int)event.packet->dataLength);
            enet_packet_destroy(event.packet);
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            extern const char *disc_reason(int reason);
            if(event.data>=DISC_NUM) event.data = DISC_NONE;
            if(event.peer==connpeer)
            {
                conoutf("\f3could not connect to server");
                abortconnect();
            }
            else
            {
                if(!discmillis || event.data) conoutf("\f3server network error, disconnecting (%s) ...", disc_reason(event.data));
                disconnect();
            }
            return;

        default:
            break;
    }
}

// sendmap/getmap commands, should be replaced by more intuitive map downloading

cvector securemaps;

void clearsecuremaps() { securemaps.deletecontentsa(); }
void securemap(char *map) { if(map) securemaps.add(newstring(map)); }
bool securemapcheck(char *map)
{
    if(strstr(map, "maps/")==map || strstr(map, "maps\\")==map) map += strlen("maps/");
    loopv(securemaps) if(!strcmp(securemaps[i], map))
    {
        conoutf("\f3map %s is secured, you can not send or overwrite it", map);
        return true;
    }
    return false;
}


void sendmap(char *mapname)
{
    if(*mapname)
    {
        save_world(mapname);
        changemap(mapname);
    }    
    else mapname = getclientmap();
    if(securemapcheck(mapname)) return;
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
    }
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
    conoutf("requesting map from server...");
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_RECVMAP);
    enet_packet_resize(packet, p.length());
    sendpackettoserv(2, packet);
}

COMMAND(sendmap, ARG_1STR);
COMMAND(getmap, ARG_NONE);
COMMAND(clearsecuremaps, ARG_NONE);
COMMAND(securemap, ARG_1STR);

