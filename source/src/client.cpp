// client.cpp, mostly network related client game code

#include "pch.h"
#include "cube.h"
#include "bot/bot.h"

VAR(connected, 1, 0, 0);

ENetHost *clienthost = NULL;
ENetPeer *curpeer = NULL, *connpeer = NULL;
int connmillis = 0, connattempts = 0, discmillis = 0;
bool c2sinit = false;       // whether we need to tell the other clients our stats
bool watchingdemo = false;          // flowtron : enables SV_ITEMLIST in demos - req. because mapchanged == false by then

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
int connectrole = CR_DEFAULT;

void abortconnect()
{
    if(!connpeer) return;
    clientpassword[0] = '\0';
    connectrole = CR_DEFAULT;
    if(connpeer->state!=ENET_PEER_STATE_DISCONNECTED) enet_peer_reset(connpeer);
    connpeer = NULL;
#if 0
    if(!curpeer)
    {
        enet_host_destroy(clienthost);
        clienthost = NULL;
    }
#endif
}

void connectserv_(const char *servername, const char *serverport = NULL, const char *password = NULL, int role = CR_DEFAULT)
{
    extern void enddemoplayback();
    if(watchingdemo) enddemoplayback();
    if(connpeer)
    {
        conoutf("aborting connection attempt");
        abortconnect();
    }

    connectrole = role;
    s_strcpy(clientpassword, password ? password : "");

    ENetAddress address;
    int p = 0;
    if(serverport && serverport[0]) p = atoi(serverport);
    address.port = p > 0 ? p : CUBE_DEFAULT_SERVER_PORT;

    if(servername)
    {
        addserver(servername, serverport);
        conoutf("attempting to connect to %s%c%s", servername, address.port != CUBE_DEFAULT_SERVER_PORT ? ':' : 0, serverport);
        if(!resolverwait(servername, &address))
        {
            conoutf("\f3could not resolve server %s", servername);
            clientpassword[0] = '\0';
            connectrole = CR_DEFAULT;
            return;
        }
    }
    else
    {
        conoutf("attempting to connect over LAN");
        address.host = ENET_HOST_BROADCAST;
    }

    if(!clienthost) clienthost = enet_host_create(NULL, 2, 0, 0);

    if(clienthost)
    {
        connpeer = enet_host_connect(clienthost, &address, 3);
        enet_host_flush(clienthost);
        connmillis = totalmillis;
        connattempts = 0;
        if(!m_mp(gamemode)) gamemode = GMODE_TEAMDEATHMATCH;
    }
    else
    {
        conoutf("\f3could not connect to server");
        clientpassword[0] = '\0';
        connectrole = CR_DEFAULT;
    }
}

void connectserv(char *servername, char *serverport, char *password)
{
    connectserv_(servername, serverport, password);
}

void connectadmin(char *servername, char *serverport, char *password)
{
    if(!password[0]) return;
    connectserv_(servername, serverport, password, CR_ADMIN);
}

void lanconnect()
{
    connectserv_(NULL);
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
            discmillis = totalmillis;
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
        kickallbots();
        loopv(players) zapplayer(players[i]);
        clearvote();
        audiomgr.clearworldsounds(false);
        localdisconnect();
    }
#if 0
    if(!connpeer && clienthost)
    {
        enet_host_destroy(clienthost);
        clienthost = NULL;
    }
#endif
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
    bool toteam = text && text[0] == '%' && m_teammode;
    if(!toteam && text[0] == '%' && strlen(text) > 1) text++; // convert team-text to normal-text if no team-mode is active
    if(toteam) text++;
    conoutf("%s:\f%d %s", colorname(player1), toteam ? 1 : 0, text);
    addmsg(toteam ? SV_TEAMTEXT : SV_TEXT, "rs", text);
}

void echo(char *text) { conoutf("%s", text); }

COMMAND(echo, ARG_CONC);
COMMANDN(say, toserver, ARG_CONC);
COMMANDN(connect, connectserv, ARG_3STR);
COMMAND(connectadmin, ARG_3STR);
COMMAND(lanconnect, ARG_NONE);
COMMANDN(disconnect, trydisconnect, ARG_NONE);

void cleanupclient()
{
    abortconnect();
    disconnect(1);
    if(clienthost)
    {
        enet_host_destroy(clienthost);
        clienthost = NULL;
    }
}

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

static int lastupdate = -1000, lastping = 0;
bool senditemstoserver = false;     // after a map change, since server doesn't have map data

void sendpackettoserv(int chan, ENetPacket *packet)
{
    if(curpeer) enet_peer_send(curpeer, chan, packet);
    else localclienttoserver(chan, packet);
    if(!packet->referenceCount) enet_packet_destroy(packet);
}

void c2skeepalive()
{
    if(clienthost && (curpeer || connpeer)) enet_host_service(clienthost, NULL, 0);
}

extern string masterpwd;

void c2sinfo(playerent *d)                  // send update to the server
{
    if(d->clientnum<0) return;              // we haven't had a welcome message from the server yet
    if(totalmillis-lastupdate<40) return;    // don't update faster than 25fps

    if(d->state==CS_ALIVE || d->state==CS_EDITING)
    {
        ENetPacket *packet = enet_packet_create(NULL, 100, 0);
        ucharbuf q(packet->data, packet->dataLength);

        putint(q, SV_POS);
        putint(q, d->clientnum);
        putuint(q, (int)(d->o.x*DMF));       // quantize coordinates to 1/16th of a cube, between 1 and 3 bytes
        putuint(q, (int)(d->o.y*DMF));
        putuint(q, (int)((d->o.z - d->eyeheight)*DMF));
        putuint(q, (int)d->yaw);
        putint(q, (int)d->pitch);
        putint(q, (int)(125*d->roll/20));
        putint(q, (int)(d->vel.x*DVELF));
        putint(q, (int)(d->vel.y*DVELF));
        putint(q, (int)(d->vel.z*DVELF));
        // pack rest in 1 int: strafe:2, move:2, onfloor:1, onladder: 1
        putuint(q, (d->strafe&3) | ((d->move&3)<<2) | (((int)d->onfloor)<<4) | (((int)d->onladder)<<5) | ((d->lifesequence&1)<<6) | (((int)d->crouching)<<7));

        enet_packet_resize(packet, q.length());
        sendpackettoserv(0, packet);
    }

    if(senditemstoserver || !c2sinit || messages.length() || totalmillis-lastping>250)
    {
        ENetPacket *packet = enet_packet_create (NULL, MAXTRANS, 0);
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
            if(maploaded > 0)
            {
                putint(p, SV_ITEMLIST);
                if(!m_noitems) putitems(p);
                putint(p, -1);
            }
            putint(p, SV_SPAWNLIST);
            putint(p, maploaded);
            if(maploaded > 0)
            {
                loopi(3) putint(p, numspawn[i]);
                loopi(2) putint(p, numflagspawn[i]);
            }
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
        if(totalmillis-lastping>250)
        {
            putint(p, SV_PING);
            putint(p, totalmillis);
            lastping = totalmillis;
        }
        if(!p.length()) enet_packet_destroy(packet);
        else
        {
            enet_packet_resize(packet, p.length());
            sendpackettoserv(1, packet);
        }
    }

    if(clienthost) enet_host_flush(clienthost);
    lastupdate = totalmillis;
}

void sendintro()
{
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, 0);
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_CONNECT);
    sendstring(player1->name, p);
    sendstring(genpwdhash(player1->name, clientpassword, sessionid), p);
    putint(p, connectrole);
    clientpassword[0] = '\0';
    connectrole = CR_DEFAULT;
    putint(p, player1->nextprimweap->type);
    enet_packet_resize(packet, p.length());
    sendpackettoserv(1, packet);
}

void gets2c()           // get updates from the server
{
    ENetEvent event;
    if(!clienthost || (!curpeer && !connpeer)) return;
    if(connpeer && totalmillis/3000 > connmillis/3000)
    {
        conoutf("attempting to connect...");
        connmillis = totalmillis;
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
            if(editmode) toggleedit(true);
            break;

        case ENET_EVENT_TYPE_RECEIVE:
		{
            extern packetqueue pktlogger;
            pktlogger.queue(event.packet);

            if(discmillis) conoutf("attempting to disconnect...");
            else servertoclient(event.channelID, event.packet->data, (int)event.packet->dataLength);
            // destroyed in logger
            //enet_packet_destroy(event.packet);
            break;
		}

        case ENET_EVENT_TYPE_DISCONNECT:
		{
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
		}

        default:
            break;
    }
}

// sendmap/getmap commands, should be replaced by more intuitive map downloading

cvector securemaps;

void resetsecuremaps() { securemaps.deletecontentsa(); }
void securemap(char *map) { if(map) securemaps.add(newstring(map)); }
bool securemapcheck(char *map, bool msg)
{
    if(strstr(map, "maps/")==map || strstr(map, "maps\\")==map) map += strlen("maps/");
    loopv(securemaps) if(!strcmp(securemaps[i], map))
    {
        if(msg) conoutf("\f3map %s is secured, you can not send, receive or overwrite it", map);
        return true;
    }
    return false;
}


void sendmap(char *mapname)
{
    if(*mapname && gamemode==1)
    {
        save_world(mapname);
        changemap(mapname); // FIXME!!
    }
    else mapname = getclientmap();
    if(securemapcheck(mapname)) return;

    int mapsize, cfgsize, cfgsizegz;
    uchar *mapdata = readmap(path(mapname), &mapsize);
    uchar *cfgdata = readmcfggz(path(mapname), &cfgsize, &cfgsizegz);
    if(!mapdata) return;
    if(!cfgdata) { cfgsize = 0; cfgsizegz = 0; }

    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS + mapsize + cfgsizegz, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);

    putint(p, SV_SENDMAP);
    sendstring(mapname, p);
    putint(p, mapsize);
    putint(p, cfgsize);
    putint(p, cfgsizegz);
    if(MAXMAPSENDSIZE - p.length() < mapsize + cfgsizegz || cfgsize > MAXCFGFILESIZE)
    {
        conoutf("map %s is too large to send", mapname);
        delete[] mapdata;
        if(cfgsize) delete[] cfgdata;
        enet_packet_destroy(packet);
        return;
    }
    p.put(mapdata, mapsize);
    delete[] mapdata;
    if(cfgsizegz)
    {
        p.put(cfgdata, cfgsizegz);
        delete[] cfgdata;
    }

    enet_packet_resize(packet, p.length());
    sendpackettoserv(2, packet);
    conoutf("sending map %s to server...", mapname);
    s_sprintfd(msg)("[map %s uploaded to server, \"/getmap\" to receive it]", mapname);
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

void getdemo(int i)
{
    if(i<=0) conoutf("getting demo...");
    else conoutf("getting demo %d...", i);
    addmsg(SV_GETDEMO, "ri", i);
}

void listdemos()
{
    conoutf("listing demos...");
    addmsg(SV_LISTDEMOS, "r");
}

COMMAND(sendmap, ARG_1STR);
COMMAND(getmap, ARG_NONE);
COMMAND(resetsecuremaps, ARG_NONE);
COMMAND(securemap, ARG_1STR);
COMMAND(getdemo, ARG_1INT);
COMMAND(listdemos, ARG_NONE);
