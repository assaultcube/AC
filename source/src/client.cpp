// client.cpp, mostly network related client game code

#include "cube.h"
#include "bot/bot.h"

VAR(connected, 1, 0, 0);

ENetHost *clienthost = NULL;
ENetPeer *curpeer = NULL, *connpeer = NULL;
int connmillis = 0, connattempts = 0, discmillis = 0;
SVAR(curdemofile, "n/a");
extern bool clfail, cllock;
extern int searchlan;

int getclientnum() { return player1 ? player1->clientnum : -1; }

bool multiplayer(bool msg)
{
    // check not correct on listen server?
    if(curpeer && msg) conoutf(_("operation not available in multiplayer"));
    return curpeer!=NULL;
}

bool allowedittoggle()
{
    bool allow = !curpeer || gamemode==1;
    if(!allow) conoutf(_("editing in multiplayer requires coopedit mode (1)"));
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
bool modprotocol = false;

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

void connectserv_(const char *servername, int serverport = 0, const char *password = NULL, int role = CR_DEFAULT)
{
    if(serverport <= 0) serverport = CUBE_DEFAULT_SERVER_PORT;
    if(watchingdemo) enddemoplayback();
    if(!clfail && cllock && searchlan<2) return;

    if(connpeer)
    {
        conoutf(_("aborting connection attempt"));
        abortconnect();
    }
    connectrole = role;
    copystring(clientpassword, password ? password : "");
    ENetAddress address;
    address.port = serverport;

    if(servername)
    {
        addserver(servername, serverport, 0);
        conoutf(_("%c2attempting to %sconnect to %c5%s%c4:%d%c2"), CC, role==CR_DEFAULT?"":"\f8admin\f2", CC, servername, CC, serverport, CC);
        if(!resolverwait(servername, &address))
        {
            conoutf(_("%c2could %c3not resolve%c2 server %c5%s%c2"), CC, CC, CC, CC, servername, CC);
            clientpassword[0] = '\0';
            connectrole = CR_DEFAULT;
            return;
        }
    }
    else
    {
        conoutf(_("%c2attempting to connect over %c1LAN%c2"), CC, CC, CC);
        address.host = ENET_HOST_BROADCAST;
    }

    if(!clienthost)
        clienthost = enet_host_create(NULL, 2, 3, 0, 0);

    if(clienthost)
    {
        connpeer = enet_host_connect(clienthost, &address, 3, 0);
        enet_host_flush(clienthost);
        connmillis = totalmillis;
        connattempts = 0;
        if(!m_mp(gamemode)) gamemode = GMODE_TEAMDEATHMATCH;
    }
    else
    {
        conoutf(_("%c2could %c3not connect%c2 to server"),CC,CC,CC);
        clientpassword[0] = '\0';
        connectrole = CR_DEFAULT;
    }
}

void connectserv(char *servername, int *serverport, char *password)
{
    modprotocol = false;
    connectserv_(servername, *serverport, password);
}

void connectadmin(char *servername, int *serverport, char *password)
{
    modprotocol = false;
    if(!password[0]) return;
    connectserv_(servername, *serverport, password, CR_ADMIN);
}

void lanconnect()
{
    modprotocol = false;
    connectserv_(NULL);
}

void modconnectserv(char *servername, int *serverport, char *password)
{
    modprotocol = true;
    connectserv_(servername, *serverport, password);
}

void modconnectadmin(char *servername, int *serverport, char *password)
{
    modprotocol = true;
    if(!password[0]) return;
    connectserv_(servername, *serverport, password, CR_ADMIN);
}

void modlanconnect()
{
    modprotocol = true;
    connectserv_(NULL);
}

void whereami()
{
    conoutf("you are at (%.2f,%.2f)", player1->o.x, player1->o.y);
}

void go_to(float *x, float *y, char *showmsg)
{
    if(player1->state != CS_EDITING) return;
    player1->newpos.x = *x;
    player1->newpos.y = *y;
    if(!showmsg || !*showmsg || strcmp(showmsg, "0")!=0)
        conoutf("you are going to (%.2f; %.2f)", *x, *y);
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
        conoutf(_("disconnected"));
        cleanup = true;
    }

    if(cleanup)
    {
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
    if(identexists("onDisconnect"))
    {
        defformatstring(ondisconnect)("onDisconnect %d", -1);
        execute(ondisconnect);
    }}

void trydisconnect()
{
    if(connpeer)
    {
        conoutf(_("aborting connection attempt"));
        abortconnect();
        return;
    }
    if(!curpeer)
    {
        conoutf(_("not connected"));
        return;
    }
    conoutf(_("attempting to disconnect..."));
    disconnect(0, !discmillis);
}

void _toserver(char *text, int msg, int msgt)
{
    bool toteam = text && text[0] == '%' && (m_teammode || team_isspect(player1->team));
    if(!toteam && text[0] == '%' && strlen(text) > 1) text++; // convert team-text to normal-text if no team-mode is active
    if(toteam) text++;
    filtertext(text, text);
    trimtrailingwhitespace(text);
    if(servstate.mastermode == MM_MATCH && servstate.matchteamsize && !team_isactive(player1->team) && !(player1->team == TEAM_SPECT && player1->clientrole == CR_ADMIN)) toteam = true; // spect chat
    if(*text)
    {
        if(msg == SV_TEXTME) conoutf("\f%d%s %s", toteam ? 1 : 0, colorname(player1), highlight(text));
        else conoutf("%s:\f%d %s", colorname(player1), toteam ? 1 : 0, highlight(text));
        addmsg(toteam ? msgt : msg, "rs", text);
    }
}

void toserver(char *text)
{
    _toserver(text, SV_TEXT, SV_TEAMTEXT);
}

void toserverme(char *text)
{
    _toserver(text, SV_TEXTME, SV_TEAMTEXTME);
}

void echo(char *text)
{
    const char *s = strtok(text, "\n");
    do
    {
        conoutf("%s", s ? s : "");
        s = strtok(NULL, "\n");
    }
    while(s);
}

VARP(allowhudechos, 0, 1, 1);
void hudecho(char *text)
{
    const char *s = strtok(text, "\n");
    void (*outf)(const char *s, ...) = allowhudechos ? hudoutf : conoutf;
    do
    {
        outf("%s", s ? s : "");
        s = strtok(NULL, "\n");
    }
    while(s);
}

void pm(char *text)
{
    if(!text || !text[0]) return;
    int cn = -1;
    char digit;
    while ((digit = *text++) != '\0')
    {
        if (digit < '0' || digit > '9') break;
        if(cn < 0) cn = 0;
        else cn *= 10;
        cn += digit - '0';
    }
    playerent *to = getclient(cn);
    if(!to)
    {
        conoutf("invalid client number specified");
        return;
    }

    if(!isspace(digit)) { --text; }

    // FIXME:
    /*if(!text || !text[0] || !isdigit(text[0])) return;
    int cn = -1;
    char *numend = strpbrk(text, " \t");
    if(!numend) return;
    string cnbuf;
    copystring(cnbuf, text, min(numend-text+1, MAXSTRLEN));
    cn = atoi(cnbuf);
    playerent *to = getclient(cn);
    if(!to)
    {
        conoutf("invalid client number specified");
        return;
    }

    if(*numend) numend++;*/
    // :FIXME

    filtertext(text, text);
    trimtrailingwhitespace(text);

    addmsg(SV_TEXTPRIVATE, "ris", cn, text);
    conoutf("to %s:\f9 %s", colorname(to), highlight(text));
}
COMMAND(pm, "c");

COMMAND(echo, "c");
COMMAND(hudecho, "c");
COMMANDN(say, toserver, "c");
COMMANDN(me, toserverme, "c");
COMMANDN(connect, connectserv, "sis");
COMMAND(connectadmin, "sis");
COMMAND(lanconnect, "");
COMMANDN(modconnect, modconnectserv, "sis");
COMMAND(modconnectadmin, "sis");
COMMAND(modlanconnect, "");
COMMANDN(disconnect, trydisconnect, "");
COMMAND(whereami, "");
COMMAND(go_to, "ffs");

void current_version(char *text)
{
    int version = atoi(text);
    if (version && AC_VERSION<version) conoutf("YOUR VERSION OF ASSAULTCUBE IS OUTDATED!\nYOU MUST UPDATE ASSAULTCUBE\nplease visit %s for more information",AC_MASTER_URI);
}
COMMAND(current_version, "s");

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
            case 's':
            {
                const char *t = va_arg(args, const char *);
                if(t) sendstring(t, p); nums++; break;
            }
        }
        va_end(args);
    }
    int num = nums?0:numi, msgsize = msgsizelookup(type);
    if(msgsize && num!=msgsize) { fatal("inconsistent msg size for %d (%d != %d)", type, num, msgsize); }
    int len = p.length();
    messages.add(len&0xFF);
    messages.add((len>>8)|(reliable ? 0x80 : 0));
    loopi(len) messages.add(buf[i]);
}

static int lastupdate = -1000, lastping = 0;
bool sendmapidenttoserver = false;

void sendpackettoserv(int chan, ENetPacket *packet)
{
    if(curpeer) enet_peer_send(curpeer, chan, packet);
    else localclienttoserver(chan, packet);
}

void c2skeepalive()
{
    if(clienthost && (curpeer || connpeer)) enet_host_service(clienthost, NULL, 0);
}

extern string masterpwd;
bool sv_pos = true;

void c2sinfo(playerent *d)                  // send update to the server
{
    if(d->clientnum<0) return;              // we haven't had a welcome message from the server yet
    if(totalmillis-lastupdate<40) return;    // don't update faster than 25fps

    if(d->state==CS_ALIVE || d->state==CS_EDITING)
    {
        packetbuf q(100);
        int cn = d->clientnum,
            x = (int)(d->o.x*DMF),          // quantize coordinates to 1/16th of a cube, between 1 and 3 bytes
            y = (int)(d->o.y*DMF),
            z = (int)((d->o.z - d->eyeheight)*DMF),
            ya = (int)((512 * d->yaw) / 360.0f),
            pi = (int)((127 * d->pitch) / 90.0f),
            r = (int)(31*d->roll/20),
            dxt = (int)(d->vel.x*DVELF),
            dyt = (int)(d->vel.y*DVELF),
            dzt = (int)(d->vel.z*DVELF),
            dx = dxt - d->vel_t.i[0],
            dy = dyt - d->vel_t.i[1],
            dz = dzt - d->vel_t.i[2],
            // pack rest in 1 int: strafe:2, move:2, onfloor:1, onladder: 1
            f = (d->strafe&3) | ((d->move&3)<<2) | (((int)d->onfloor)<<4) | (((int)d->onladder)<<5) | ((d->lifesequence&1)<<6) | (((int)d->crouching)<<7),
            g = (dx?1:0) | ((dy?1:0)<<1) | ((dz?1:0)<<2) | ((r?1:0)<<3) | (((int)d->scoping)<<4) | (((int)d->shoot)<<5);
            d->vel_t.i[0] = dxt;
            d->vel_t.i[1] = dyt;
            d->vel_t.i[2] = dzt;
        int usefactor = sfactor < 7 ? 7 : sfactor, sizexy = 1 << (usefactor + 4);
        if(cn >= 0 && cn < 32 &&
            usefactor <= 7 + 3 &&       // map size 7..10
            x >= 0 && x < sizexy &&
            y >= 0 && y < sizexy &&
            z >= -2047 && z <= 2047 &&
            ya >= 0 && ya < 512 &&
            pi >= -128 && pi <= 127 &&
            r >= -32 && r <= 31 &&
            dx >= -8 && dx <= 7 && // FIXME
            dy >= -8 && dy <= 7 &&
            dz >= -8 && dz <= 7)
        { // compact POS packet
            bool noroll = !r, novel = !dx && !dy && !dz;
            bitbuf<packetbuf> b(q);
            putint(q, SV_POSC);
            b.putbits(5, cn);
            b.putbits(2, usefactor - 7);
            b.putbits(usefactor + 4, x);
            b.putbits(usefactor + 4, y);
            b.putbits(9, ya);
            b.putbits(8, pi + 128);
            b.putbits(1, noroll ? 1 : 0);
            if(!noroll) b.putbits(6, r + 32);
            b.putbits(1, novel ? 1 : 0);
            if(!novel)
            {
                b.putbits(4, dx + 8);
                b.putbits(4, dy + 8);
                b.putbits(4, dz + 8);
            }
            b.putbits(8, f);
            b.putbits(1, z < 0 ? 1 : 0);
            if(z < 0) z = -z;                 // z is encoded with 3..10 bits minimum (fitted to byte boundaries), or full 11 bits if necessary
            int s = (b.rembits() - 1 + 8) % 8;
            if(s < 3) s += 8;
            if(z >= (1 << s)) s = 11;
            b.putbits(1, s == 11 ? 1 : 0);
            b.putbits(s, z);
            b.putbits(1, d->scoping ? 1 : 0);
            b.putbits(1, d->shoot ? 1 : 0);
        }
        else
        { // classic POS packet
            putint(q, SV_POS);
            putint(q, d->clientnum);
            putuint(q, x);
            putuint(q, y);
            putuint(q, z);
            putuint(q, (int)d->yaw);
            putint(q, (int)d->pitch);
            putuint(q, g);
            if (r) putint(q, (int)(125*d->roll/20));
            if (dx) putint(q, dx);
            if (dy) putint(q, dy);
            if (dz) putint(q, dz);
            putuint(q, f);
        }
        sendpackettoserv(0, q.finalize());
        d->shoot = false;
    }

    if(sendmapidenttoserver || messages.length() || totalmillis-lastping>250)
    {
        packetbuf p(MAXTRANS);

        if(sendmapidenttoserver) // new map
        {
            p.reliable();
            putint(p, SV_MAPIDENT);
            putint(p, maploaded);
            putint(p, hdr.maprevision);
            sendmapidenttoserver = false;
        }
        int i = 0;
        while(i < messages.length()) // send messages collected during the previous frames
        {
            int len = messages[i] | ((messages[i+1]&0x7F)<<8);
            if(p.remaining() < len) break;
            if(messages[i+1]&0x80) p.reliable();
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
        if(p.length()) sendpackettoserv(1, p.finalize());
    }

    if(clienthost) enet_host_flush(clienthost);
    lastupdate = totalmillis;
}

int getbuildtype()
{
    return (isbigendian() ? 0x80 : 0 )|(adler((unsigned char *)guns, sizeof(guns)) % 31 << 8)|
        #ifdef WIN32
            0x40 |
        #endif
        #ifdef __APPLE__
            0x20 |
        #endif
        #ifdef _DEBUG
            0x08 |
        #endif
        #ifdef __GNUC__
            0x04 |
        #endif
            0;
}

void sendintro()
{
    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    putint(p, SV_CONNECT);
    putint(p, AC_VERSION);
    putint(p, getbuildtype());
    sendstring(player1->name, p);
    sendstring(genpwdhash(player1->name, clientpassword, sessionid), p);
    sendstring(!lang || strlen(lang) != 2 ? "" : lang, p);
    putint(p, connectrole);
    clientpassword[0] = '\0';
    connectrole = CR_DEFAULT;
    putint(p, player1->nextprimweap->type);
    loopi(2) putint(p, player1->skin(i));
    sendpackettoserv(1, p.finalize());
}

void gets2c()           // get updates from the server
{
    ENetEvent event;
    if(!clienthost || (!curpeer && !connpeer)) return;
    if(connpeer && totalmillis/3000 > connmillis/3000)
    {
        conoutf(_("attempting to connect..."));
        connmillis = totalmillis;
        ++connattempts;
        if(connattempts > 3)
        {
            conoutf(_("%c3could not connect to server"), CC);
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
            conoutf(_("connected to server"));
            if(identexists("onConnect"))
            {
                defformatstring(onconnect)("onConnect %d", -1);
                execute(onconnect);
            }
            throttle();
            if(editmode) toggleedit(true);
            break;

        case ENET_EVENT_TYPE_RECEIVE:
        {
            extern packetqueue pktlogger;
            pktlogger.queue(event.packet);

            if(discmillis) conoutf(_("attempting to disconnect..."));
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
                conoutf(_("%c3could not connect to server"), CC);
                abortconnect();
            }
            else
            {
                if(!discmillis || event.data) conoutf(_("%c3server network error, disconnecting (%s) ..."), CC, disc_reason(event.data));
                disconnect();
            }
            return;
        }

        default:
            break;
    }
}

// for AUTH:

vector<authkey *> authkeys;

VARP(autoauth, 0, 1, 1);

authkey *findauthkey(const char *desc)
{
    loopv(authkeys) if(!strcmp(authkeys[i]->desc, desc) && !strcmp(authkeys[i]->name, player1->name)) return authkeys[i];
    loopv(authkeys) if(!strcmp(authkeys[i]->desc, desc)) return authkeys[i];
    return NULL;
}

void addauthkey(const char *name, const char *key, const char *desc)
{
    loopvrev(authkeys) if(!strcmp(authkeys[i]->desc, desc) && !strcmp(authkeys[i]->name, name)) delete authkeys.remove(i);
    if(name[0] && key[0]) authkeys.add(new authkey(name, key, desc));
}

bool _hasauthkey(const char *name, const char *desc)
{
    if(!name[0] && !desc[0]) return authkeys.length() > 0;
    loopvrev(authkeys) if(!strcmp(authkeys[i]->desc, desc) && !strcmp(authkeys[i]->name, name)) return true;
    return false;
}

void genauthkey(const char *secret)
{
    if(!secret[0]) { conoutf("you must specify a secret password"); return; }
    vector<char> privkey, pubkey;
    genprivkey(secret, privkey, pubkey);
    conoutf("private key: %s", privkey.getbuf());
    conoutf("public key: %s", pubkey.getbuf());
}

void saveauthkeys()
{
    if(authkeys.length())
    {
        stream *f = openfile("config/auth.cfg", "w");
        if(!f) { conoutf("failed to open config/auth.cfg for writing"); return; }
        loopv(authkeys)
        {
            authkey *a = authkeys[i];
            f->printf("authkey \"%s\" \"%s\" \"%s\"\n", a->name, a->key, a->desc);
        }
        conoutf("saved authkeys to config/auth.cfg");
        delete f;
    }
    else conoutf("you need to use 'addauthkey USER KEY DESC' first; one DESC 'public', one DESC empty(=private)");
}

bool tryauth(const char *desc)
{
    authkey *a = findauthkey(desc);
    if(!a) return false;
    a->lastauth = lastmillis;
    addmsg(SV_AUTHTRY, "rss", a->desc, a->name);
    return true;
}

COMMANDN(authkey, addauthkey, "sss");
COMMANDF(hasauthkey, "ss", (char *name, char *desc) { intret(_hasauthkey(name, desc) ? 1 : 0); });
COMMAND(genauthkey, "s");
COMMAND(saveauthkeys, "");
COMMANDF(auth, "s", (char *desc) { intret(tryauth(desc)); });

// :for AUTH

// sendmap/getmap commands, should be replaced by more intuitive map downloading

vector<char *> securemaps;

void resetsecuremaps() { securemaps.deletearrays(); }
void securemap(char *map) { if(map) securemaps.add(newstring(map)); }
bool securemapcheck(const char *map, bool msg)
{
    if(strstr(map, "maps/")==map || strstr(map, "maps\\")==map) map += strlen("maps/");
    loopv(securemaps) if(!strcmp(securemaps[i], map))
    {
        if(msg)
        {
            conoutf(_("%c3map %c4%s%c3 is secured, this means you CAN'T send, receive or overwrite it"), CC, CC, map, CC);
            if(connected)
            {
                conoutf(_("%c3if you get this error alot you or the server may be running an outdated game"), CC);
                conoutf(_("%c3you may check for updates at %c1http://assault.cubers.net/download.html"), CC, CC);
            }
            return true;
        }
    }
    return false;
}

void sendmap(char *mapname)
{
    if(!*mapname) mapname = getclientmap();
    if(securemapcheck(mapname)) return;
    if(gamemode == GMODE_COOPEDIT && !strcmp(getclientmap(), mapname)) save_world(mapname);

    int mapsize, cfgsize, cfgsizegz, revision;
    uchar *mapdata = readmap(path(mapname), &mapsize, &revision);
    if(!mapdata) return;
    uchar *cfgdata = readmcfggz(path(mapname), &cfgsize, &cfgsizegz);
    if(!cfgdata) { cfgsize = 0; cfgsizegz = 0; }

    packetbuf p(MAXTRANS + mapsize + cfgsizegz, ENET_PACKET_FLAG_RELIABLE);

    putint(p, SV_SENDMAP);
    sendstring(mapname, p);
    putint(p, mapsize);
    putint(p, cfgsize);
    putint(p, cfgsizegz);
    putint(p, revision);
    if(MAXMAPSENDSIZE - p.length() < mapsize + cfgsizegz || cfgsize > MAXCFGFILESIZE)
    {
        conoutf(_("map %s is too large to send"), mapname);
        delete[] mapdata;
        if(cfgsize) delete[] cfgdata;
        return;
    }
    p.put(mapdata, mapsize);
    delete[] mapdata;
    if(cfgsizegz)
    {
        p.put(cfgdata, cfgsizegz);
        delete[] cfgdata;
    }

    sendpackettoserv(2, p.finalize());
    conoutf(_("sending map %s to server..."), mapname);
}

void getmap(char *name, char *callback)
{
    if((!name || !*name)
        || (connected && !strcmp(name, getclientmap())) )
    {
        conoutf(_("requesting map from server..."));
        packetbuf p(10, ENET_PACKET_FLAG_RELIABLE);
        putint(p, SV_RECVMAP);
        sendpackettoserv(2, p.finalize());
    }
    else
    {
        defformatstring(package)("packages/maps/%s.cgz", name);
        requirepackage(PCK_MAP, package);
        if(downloadpackages())
        {
            if(callback && *callback) execute(callback);
            conoutf("map %s installed successfully", name);
        }
        else conoutf("\f3map download failed.");
    }
}

void deleteservermap(char *mapname)
{
    const char *name = behindpath(mapname);
    if(!*name || securemapcheck(name)) return;
    addmsg(SV_REMOVEMAP, "rs", name);
}

string demosubpath;
void getdemo(int *idx, char *dsp)
{
    if(dsp && dsp[0]) formatstring(demosubpath)("%s/", dsp);
    else copystring(demosubpath, "");
    if(*idx<=0) conoutf(_("getting demo..."));
    else conoutf(_("getting demo %d..."), *idx);
    addmsg(SV_GETDEMO, "ri", *idx);
}

void listdemos()
{
    conoutf(_("listing demos..."));
    addmsg(SV_LISTDEMOS, "r");
}

void shiftgametime(int newmillis)
{
    newmillis = max(0, newmillis);
    int gamemillis = gametimecurrent + (lastmillis - lastgametimeupdate);

    if(!watchingdemo) { conoutf("You have to be watching a demo to change game time"); return; }
    if(newmillis > gametimemaximum) { conoutf("Invalid time specified"); return; }

    if(newmillis < gamemillis)
    {
        // if rewinding
        if(!curdemofile || !curdemofile[0]) return;
        watchingdemo = false;
        callvote(SA_MAP, curdemofile, "-1");
        nextmillis = newmillis;
    }
    else
    {
        nextmillis = newmillis - gamemillis;
    }
}

void setminutesremaining(int *minutes)
{
    shiftgametime(gametimemaximum - (*minutes)*60000);
}

void rewinddemo(int *seconds)
{
    int gamemillis = gametimecurrent+(lastmillis-lastgametimeupdate);
    shiftgametime(gamemillis - (*seconds)*1000);
}

COMMAND(sendmap, "s");
COMMAND(getmap, "ss");
COMMAND(deleteservermap, "s");
COMMAND(resetsecuremaps, "");
COMMAND(securemap, "s");
COMMAND(getdemo, "is");
COMMAND(listdemos, "");
COMMANDN(setmr, setminutesremaining, "i");
COMMANDN(rewind, rewinddemo, "i");

// packages auto - downloader

// arrays
vector<pckserver *> pckservers;
hashtable<const char *, package *> pendingpackages;

// cubescript
VARP(autodownload, 0, 1, 1);

void resetpckservers()
{
    pckservers.deletecontents();
}

void addpckserver(char *addr)
{
    pckserver *srcserver = new pckserver();
    srcserver->addr = newstring(addr);
    srcserver->pending = true;
    pckservers.add(srcserver);
}

COMMAND(resetpckservers, "");
COMMAND(addpckserver, "s");

// cURL / Network

bool havecurl = false, canceldownloads = false;

void setupcurl()
{
    if(curl_global_init(CURL_GLOBAL_NOTHING)) conoutf(_("\f3could not init cURL, content downloads not available"));
    else
    {
        havecurl = true;
        execfile("config/pcksources.cfg");
    }
}

// detect the "nearest" server
int pckserversort(pckserver **a, pckserver **b)
{
    if((*a)->ping < 0) return ((*b)->ping < 0) ? 0 : 1;
    if((*b)->ping < 0) return -1;
    
    return (*a)->ping == (*b)->ping ? 0 : ((*a)->ping < (*b)->ping ? -1 : 1);
}

SDL_Thread* pingpcksrvthread = NULL;
SDL_mutex *pingpcksrvlock = NULL;
int pingpckservers(void *data)
{
    SDL_mutexP(pingpcksrvlock);
    vector<pckserver> serverstoping;
    loopv(pckservers) serverstoping.add(*pckservers[i]);
    SDL_mutexV(pingpcksrvlock);
    // measure the time it took to receive each's server response
    // not very accurate but it should do the trick
    loopv(serverstoping)
    {
        double ping = 0, namelookup = 0;
        pckserver *serv = &serverstoping[i];
        CURL *cu = curl_easy_init();
        curl_easy_setopt(cu, CURLOPT_URL, serv->addr);
        curl_easy_setopt(cu, CURLOPT_NOSIGNAL, 1);	    // Fixes crashbug for some buggy libcurl versions (Linux)
        curl_easy_setopt(cu, CURLOPT_NOPROGRESS, 1);
        curl_easy_setopt(cu, CURLOPT_NOBODY, 1);            // don't download response body (as its size may vary a lot from one server to another)
        curl_easy_setopt(cu, CURLOPT_CONNECTTIMEOUT, 10);   // the timeout should be large here
        int result = curl_easy_perform(cu);
        curl_easy_getinfo(cu, CURLINFO_TOTAL_TIME, &ping);
        curl_easy_getinfo(cu, CURLINFO_NAMELOOKUP_TIME, &namelookup);
        ping -= namelookup; // ignore DNS lookup time as it should be performed only once
        curl_easy_cleanup(cu);
        cu = NULL;
        if(result == CURLE_OPERATION_TIMEDOUT || result == CURLE_COULDNT_RESOLVE_HOST)
            serv->responsive = false;
        else
            serv->ping = (int)(ping*1000);
    }

    SDL_mutexP(pingpcksrvlock);
    loopv(serverstoping) loopvj(pckservers) if(!strcmp(serverstoping[i].addr, pckservers[j]->addr)) *pckservers[j] = serverstoping[i];
    pckservers.sort(pckserversort);
    // print the results. we should make it silent once tested enough
    loopv(pckservers)
    {
        pckserver *serv = pckservers[i];
        if(serv->ping > 0) conoutf("%d. %s (%d ms)", i+1, serv->addr, serv->ping);
        else conoutf("%d. %s (did not reply)", i+1, serv->addr);
    }
    SDL_mutexV(pingpcksrvlock);

    SDL_DestroyMutex(pingpcksrvlock);
    pingpcksrvthread = NULL;
    pingpcksrvlock = NULL;
    return 0;
}

void sortpckservers()
{
    if(pingpcksrvthread || !havecurl) return;
    pingpcksrvlock = SDL_CreateMutex();
    pingpcksrvthread = SDL_CreateThread(pingpckservers, NULL);
}
COMMAND(sortpckservers, "");

static size_t write_callback(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

static int progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
    package *pck = (package *)clientp;
    loadingscreen(_("downloading package %d out of %d...\n%s %.0f/%.0f KB (%.1f%%)\n(ESC to cancel)"), pck->number + 1, pck->number + pendingpackages.numelems,
        pck->name, dlnow/double(1000.0), dltotal/double(1000.0), dltotal == 0 ? 0 : (dlnow/dltotal * double(100.0)));
    if(interceptkey(SDLK_ESCAPE))
    {
        canceldownloads = true;
        loadingscreen();
        return 1;
    }
    return 0;
}

int processdownload(package *pck)
{
    string tmpname = "";
    copystring(tmpname, findfile(path("tmp", true), "rb"));
    if(!pck->pending)
    {
        switch(pck->type)
        {
            case PCK_TEXTURE: case PCK_AUDIO:
            {
                const char *pckname = findfile(path(pck->name, true), "w+");
                preparedir(pckname);
                // with textures/sounds, the image/audio file itself is sent. Just need to copy it from the temporary file
                if(!copyfile(tmpname, pckname)) conoutf(_("\f3failed to install"), pckname);
                break;
            }

            case PCK_MAP: case PCK_MAPMODEL:
            {
                addzip(tmpname, pck->name, NULL, true, pck->type);
                break;
            }

            case PCK_SKYBOX:
            {
                char *fname = newstring(pck->name), *ls = strrchr(fname, '/');
                if(ls) *ls = '\0';
                addzip(tmpname, fname, NULL, true, pck->type);
                break;
            }

            default:
                conoutf(_("could not install package %s"), pck->name);
                break;
        }
        delfile(tmpname);
    }
    return 0;
}

// download a package
double dlpackage(package *pck)
{
    if(!pck || !pck->source) return false;
    const char *tmpname = findfile(path("tmp", true), "wb");
    FILE *outfile = fopen(tmpname, "wb");
    string req, pckname = "";
    sprintf(req, "%s/%s%s", pck->source->addr, strreplace(pckname, pck->name, " ", "%20"), (pck->type==PCK_MAP || pck->type==PCK_MAPMODEL || pck->type==PCK_SKYBOX) ? ".zip" : "");
    conoutf(_("downloading %s from %s ..."), pck->name, pck->source->addr);
    if(!outfile)
    {
        pck->pending = false;
        conoutf("\f3failed to write temporary file \"%s\"", tmpname);
        return 0;
    }

    int result, httpresult = 0;
    double dlsize;
    pck->curl = curl_easy_init();
    curl_easy_setopt(pck->curl, CURLOPT_URL, req);
    curl_easy_setopt(pck->curl, CURLOPT_NOSIGNAL, 1);		 // Fixes crashbug for some buggy libcurl versions (Linux)
    curl_easy_setopt(pck->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(pck->curl, CURLOPT_WRITEDATA, outfile);
    curl_easy_setopt(pck->curl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(pck->curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
    curl_easy_setopt(pck->curl, CURLOPT_PROGRESSDATA, pck);
    curl_easy_setopt(pck->curl, CURLOPT_CONNECTTIMEOUT, 10);     // generous timeout for Bukz ;)
    result = curl_easy_perform(pck->curl);
    curl_easy_getinfo(pck->curl, CURLINFO_RESPONSE_CODE, &httpresult);
    curl_easy_getinfo(pck->curl, CURLINFO_SIZE_DOWNLOAD, &dlsize);
    curl_easy_cleanup(pck->curl);
    pck->curl = NULL;
    fclose(outfile);

    pck->pending = false;
    if(result == CURLE_OPERATION_TIMEDOUT || result == CURLE_COULDNT_RESOLVE_HOST)
    {
        // mark source unresponsive
        pck->source->responsive = false;
        conoutf(_("\f3could not connect to %s"), pck->source->addr);

        // try to find another source
        pckserver *source = NULL;
        loopv(pckservers) if(pckservers[i]->responsive) { source = pckservers[i]; break; }
        if(!source)
        {
            conoutf(_("\f3no more servers to connect to"));
            canceldownloads = true;
            return 0;
        }

        // update all packages
        enumerate(pendingpackages, package *, pack,
        {
            pack->source = source;
        });

        pck->pending = true;

        return dlpackage(pck); // retry current
    }
    if(!result && httpresult == 200) processdownload(pck);
    else if(result == CURLE_ABORTED_BY_CALLBACK) conoutf(_("\f3download cancelled"));
    else conoutf(_("\f2request for %s failed (cURL %d, HTTP %d)"), req, result, httpresult);
    return (!result && httpresult == 200) ? dlsize : 0;
}

int lastpackagesdownload = 0;
int downloadpackages()
{
    if(!pendingpackages.numelems) return 0;
    else if(totalmillis - lastpackagesdownload < 2000)
    {
        conoutf("\f3quick download attempt aborted");
        return 0;
    }

    double total = 0;
    int downloaded = 0;
    enumerate(pendingpackages, package *, pck,
    {
        if(!canceldownloads)
        {
            if(connected) c2skeepalive(); // try to avoid time out
            pck->number = downloaded++;
            total += dlpackage(pck);
        }
        pendingpackages.remove(pck->name);
        delete pck;
    });
    canceldownloads = false;
    lastpackagesdownload = lastmillis;
    return (int)total;
}

bool requirepackage(int type, const char *path)
{
    if(!havecurl || canceldownloads || type < 0 || type >= PCK_NUM || pendingpackages.access(path)) return false;

    package *pck = new package;
    pck->name = unixpath(newstring(path));
    pck->type = type;
    loopv(pckservers) if(pckservers[i]->responsive) { pck->source = pckservers[i]; break; }
    if(!pck->source) { conoutf(_("\f3no responsive source server found, can't download")); return false; }
    pck->pending = true;

    pendingpackages.access(pck->name, pck);

    return true;
}


void writepcksourcecfg()
{
    stream *f = openfile(path("config/pcksources.cfg", true), "w");
    if(!f) return;
    f->printf("// list of package source servers (only add servers you trust!)\n");
    loopv(pckservers) f->printf("\naddpckserver %s", pckservers[i]->addr);
    f->printf("\n");
    delete f;
}
