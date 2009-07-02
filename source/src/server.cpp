// server.cpp: little more than enhanced multicaster
// runs dedicated or as client coroutine

#include "pch.h"

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#define _dup    dup
#define _fileno fileno
#endif

#include "cube.h"

#define DEBUGCOND (true)

#include "server.h"
#include "servercontroller.h"
#include "serverfiles.h"

// config
servercontroller *svcctrl = NULL;
servercommandline scl;
servermaprot maprot;
serveripblacklist ipblacklist;
servernickblacklist nickblacklist;
serverpasswords passwords;
serverinfofile infofiles;

// server state
bool isdedicated = false;
ENetHost *serverhost = NULL;
int bsend = 0, brec = 0, laststatus = 0, servmillis = 0, lastfillup = 0;

vector<client *> clients;
vector<worldstate *> worldstates;
vector<savedscore> savedscores;
vector<ban> bans;
vector<demofile> demofiles;

int mastermode = MM_OPEN;
static bool autoteam = true;

static bool forceintermission = false;

string servdesc_current;
ENetAddress servdesc_caller;
bool custom_servdesc = false;

// current game
string smapname, nextmapname;
int smode = 0, nextgamemode;
static int interm = 0, minremain = 0, gamemillis = 0, gamelimit = 0;
mapstats smapstats;
vector<server_entity> sents;
char *maplayout = NULL;
int maplayout_factor;
servermapbuffer mapbuffer;


bool valid_client(int cn)
{
    return clients.inrange(cn) && clients[cn]->type != ST_EMPTY;
}

void cleanworldstate(ENetPacket *packet)
{
   loopv(worldstates)
   {
       worldstate *ws = worldstates[i];
       if(ws->positions.inbuf(packet->data) || ws->messages.inbuf(packet->data)) ws->uses--;
       else continue;
       if(!ws->uses)
       {
           delete ws;
           worldstates.remove(i);
       }
       break;
   }
}

void sendpacket(int n, int chan, ENetPacket *packet, int exclude = -1)
{
    if(n<0)
    {
        recordpacket(chan, packet->data, (int)packet->dataLength);
        loopv(clients) if(i!=exclude && (clients[i]->type!=ST_TCPIP || clients[i]->isauthed)) sendpacket(i, chan, packet);
        return;
    }
    switch(clients[n]->type)
    {
        case ST_TCPIP:
        {
            enet_peer_send(clients[n]->peer, chan, packet);
            bsend += (int)packet->dataLength;
            break;
        }

        case ST_LOCAL:
            localservertoclient(chan, packet->data, (int)packet->dataLength);
            break;
    }
}

static bool reliablemessages = false;

bool buildworldstate()
{
    static struct { int posoff, msgoff, msglen; } pkt[MAXCLIENTS];
    worldstate &ws = *new worldstate;
    loopv(clients)
    {
        client &c = *clients[i];
        if(c.type!=ST_TCPIP || !c.isauthed) continue;
        if(c.position.empty()) pkt[i].posoff = -1;
        else
        {
            pkt[i].posoff = ws.positions.length();
            loopvj(c.position) ws.positions.add(c.position[j]);
        }
        if(c.messages.empty()) pkt[i].msgoff = -1;
        else
        {
            pkt[i].msgoff = ws.messages.length();
            ucharbuf p = ws.messages.reserve(16);
            putint(p, SV_CLIENT);
            putint(p, c.clientnum);
            putuint(p, c.messages.length());
            ws.messages.addbuf(p);
            loopvj(c.messages) ws.messages.add(c.messages[j]);
            pkt[i].msglen = ws.messages.length()-pkt[i].msgoff;
        }
    }
    int psize = ws.positions.length(), msize = ws.messages.length();
    if(psize)
    {
        recordpacket(0, ws.positions.getbuf(), psize);
        ucharbuf p = ws.positions.reserve(psize);
        p.put(ws.positions.getbuf(), psize);
        ws.positions.addbuf(p);
    }
    if(msize)
    {
        recordpacket(1, ws.messages.getbuf(), msize);
        ucharbuf p = ws.messages.reserve(msize);
        p.put(ws.messages.getbuf(), msize);
        ws.messages.addbuf(p);
    }
    ws.uses = 0;
    loopv(clients)
    {
        client &c = *clients[i];
        if(c.type!=ST_TCPIP || !c.isauthed) continue;
        ENetPacket *packet;
        if(psize && (pkt[i].posoff<0 || psize-c.position.length()>0))
        {
            packet = enet_packet_create(&ws.positions[pkt[i].posoff<0 ? 0 : pkt[i].posoff+c.position.length()],
                                        pkt[i].posoff<0 ? psize : psize-c.position.length(),
                                        ENET_PACKET_FLAG_NO_ALLOCATE);
            sendpacket(c.clientnum, 0, packet);
            if(!packet->referenceCount) enet_packet_destroy(packet);
            else { ++ws.uses; packet->freeCallback = cleanworldstate; }
        }
        c.position.setsizenodelete(0);

        if(msize && (pkt[i].msgoff<0 || msize-pkt[i].msglen>0))
        {
            packet = enet_packet_create(&ws.messages[pkt[i].msgoff<0 ? 0 : pkt[i].msgoff+pkt[i].msglen],
                                        pkt[i].msgoff<0 ? msize : msize-pkt[i].msglen,
                                        (reliablemessages ? ENET_PACKET_FLAG_RELIABLE : 0) | ENET_PACKET_FLAG_NO_ALLOCATE);
            sendpacket(c.clientnum, 1, packet);
            if(!packet->referenceCount) enet_packet_destroy(packet);
            else { ++ws.uses; packet->freeCallback = cleanworldstate; }
        }
        c.messages.setsizenodelete(0);
    }
    reliablemessages = false;
    if(!ws.uses)
    {
        delete &ws;
        return false;
    }
    else
    {
        worldstates.add(&ws);
        return true;
    }
}

int countclients(int type, bool exclude = false)
{
    int num = 0;
    loopv(clients) if((clients[i]->type!=type)==exclude) num++;
    return num;
}

int numclients() { return countclients(ST_EMPTY, true); }
int numlocalclients() { return countclients(ST_LOCAL); }
int numnonlocalclients() { return countclients(ST_TCPIP); }

int numauthedclients()
{
    int num = 0;
    loopv(clients) if(clients[i]->type!=ST_EMPTY && clients[i]->isauthed) num++;
    return num;
}

int numactiveclients()
{
    int num = 0;
    loopv(clients) if(clients[i]->type!=ST_EMPTY && clients[i]->isauthed && clients[i]->isonrightmap && team_isactive(clients[i]->team)) num++;
    return num;
}

int freeteam(int pl = -1)
{
    int teamsize[2] = {0, 0};
    int teamscore[2] = {0, 0};
    int t;
    int sum = calcscores();
    loopv(clients) if(clients[i]->type!=ST_EMPTY && i != pl && clients[i]->isauthed && team_isactive(clients[i]->team))
    {
        t = team_base(clients[i]->team);
        teamsize[t]++;
        teamscore[t] += clients[i]->at3_score;
    }
    if(teamsize[0] == teamsize[1])
    {
        return sum > 200 ? (teamscore[0] < teamscore[1] ? 0 : 1) : rnd(2);
    }
    return teamsize[0] < teamsize[1] ? 0 : 1;
}

int findcnbyaddress(ENetAddress *address)
{
    loopv(clients)
    {
        if(clients[i]->type == ST_TCPIP && clients[i]->peer->address.host == address->host && clients[i]->peer->address.port == address->port)
            return i;
    }
    return -1;
}

savedscore *findscore(client &c, bool insert)
{
    if(c.type!=ST_TCPIP) return NULL;
    if(!insert)
    {
        loopv(clients)
        {
            client &o = *clients[i];
            if(o.type!=ST_TCPIP) continue;
            if(o.clientnum!=c.clientnum && o.peer->address.host==c.peer->address.host && !strcmp(o.name, c.name))
            {
                static savedscore curscore;
                curscore.save(o.state);
                return &curscore;
            }
        }
    }
    loopv(savedscores)
    {
        savedscore &sc = savedscores[i];
        if(!strcmp(sc.name, c.name) && sc.ip==c.peer->address.host) return &sc;
    }
    if(!insert) return NULL;
    savedscore &sc = savedscores.add();
    s_strcpy(sc.name, c.name);
    sc.ip = c.peer->address.host;
    return &sc;
}

void restoreserverstate(vector<entity> &ents)   // hack: called from savegame code, only works in SP
{
    loopv(sents)
    {
        sents[i].spawned = ents[i].spawned;
        sents[i].spawntime = 0;
    }
}

void sendf(int cn, int chan, const char *format, ...)
{
    int exclude = -1;
    bool reliable = false;
    if(*format=='r') { reliable = true; ++format; }
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    ucharbuf p(packet->data, packet->dataLength);
    va_list args;
    va_start(args, format);
    while(*format) switch(*format++)
    {
        case 'x':
            exclude = va_arg(args, int);
            break;

        case 'v':
        {
            int n = va_arg(args, int);
            int *v = va_arg(args, int *);
            loopi(n) putint(p, v[i]);
            break;
        }

        case 'i':
        {
            int n = isdigit(*format) ? *format++-'0' : 1;
            loopi(n) putint(p, va_arg(args, int));
            break;
        }
        case 's': sendstring(va_arg(args, const char *), p); break;
        case 'm':
        {
            int n = va_arg(args, int);
            enet_packet_resize(packet, packet->dataLength+n);
            p.buf = packet->data;
            p.maxlen += n;
            p.put(va_arg(args, uchar *), n);
            break;
        }
    }
    va_end(args);
    enet_packet_resize(packet, p.length());
    sendpacket(cn, chan, packet, exclude);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
}

void sendservmsg(const char *msg, int client=-1)
{
    sendf(client, 1, "ris", SV_SERVMSG, msg);
}

void spawnstate(client *c)
{
    clientstate &gs = c->state;
    gs.spawnstate(smode);
    gs.lifesequence++;
}

void sendspawn(client *c)
{
    clientstate &gs = c->state;
    spawnstate(c);
    sendf(c->clientnum, 1, "ri7vv", SV_SPAWNSTATE, gs.lifesequence,
        gs.health, gs.armour,
        gs.primary, gs.gunselect, m_arena ? c->spawnindex : -1,
        NUMGUNS, gs.ammo, NUMGUNS, gs.mag);
    gs.lastspawn = gamemillis;
}

// demo
bool demonextmatch = false;
FILE *demotmp = NULL;
gzFile demorecord = NULL, demoplayback = NULL;
bool recordpackets = false;
int nextplayback = 0;

void writedemo(int chan, void *data, int len)
{
    if(!demorecord) return;
    int stamp[3] = { gamemillis, chan, len };
    endianswap(stamp, sizeof(int), 3);
    gzwrite(demorecord, stamp, sizeof(stamp));
    gzwrite(demorecord, data, len);
}

void recordpacket(int chan, void *data, int len)
{
    if(recordpackets) writedemo(chan, data, len);
}

void enddemorecord()
{
    if(!demorecord) return;

    gzclose(demorecord);
    recordpackets = false;
    demorecord = NULL;

#ifdef WIN32
    demotmp = fopen(path("demos/demorecord", true), "rb");
#endif
    if(!demotmp) return;

    fseek(demotmp, 0, SEEK_END);
    int len = ftell(demotmp);
    rewind(demotmp);
    if(demofiles.length() >= scl.maxdemos)
    {
        delete[] demofiles[0].data;
        demofiles.remove(0);
    }
    demofile &d = demofiles.add();
    s_sprintf(d.info)("%s: %s, %s, %.2f%s", asctime(), modestr(gamemode), smapname, len > 1024*1024 ? len/(1024*1024.f) : len/1024.0f, len > 1024*1024 ? "MB" : "kB");
    s_sprintf(d.file)("%s_%s_%s", timestring(), behindpath(smapname), modestr(gamemode, true));
    s_sprintfd(msg)("Demo \"%s\" recorded\nPress F10 to download it from the server..", d.info);
    sendservmsg(msg);
    logline(ACLOG_INFO, "Demo \"%s\" recorded.", d.info);
    d.data = new uchar[len];
    d.len = len;
    fread(d.data, 1, len, demotmp);
    fclose(demotmp);
    demotmp = NULL;
    if(scl.demopath[0])
    {
        s_sprintf(msg)("%s%s.dmo", scl.demopath, d.file);
        path(msg);
        FILE *demo = openfile(msg, "wb");
        if(demo)
        {
            int wlen = (int) fwrite(d.data, 1, d.len, demo);
            fclose(demo);
            logline(ACLOG_INFO, "demo written to file \"%s\" (%d bytes)", msg, wlen);
        }
        else
        {
            logline(ACLOG_INFO, "failed to write demo to file \"%s\"", msg);
        }
    }
}

void setupdemorecord()
{
    if(numlocalclients() || !m_mp(gamemode) || gamemode==1) return;

#ifdef WIN32
    gzFile f = gzopen(path("demos/demorecord", true), "wb9");
    if(!f) return;
#else
    demotmp = tmpfile();
    if(!demotmp) return;
    setvbuf(demotmp, NULL, _IONBF, 0);

    gzFile f = gzdopen(_dup(_fileno(demotmp)), "wb9");
    if(!f)
    {
        fclose(demotmp);
        demotmp = NULL;
        return;
    }
#endif

    sendservmsg("recording demo");
    logline(ACLOG_INFO, "Demo recording started.");

    demorecord = f;
    recordpackets = false;

    demoheader hdr;
    memcpy(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic));
    hdr.version = DEMO_VERSION;
    hdr.protocol = SERVER_PROTOCOL_VERSION;
    endianswap(&hdr.version, sizeof(int), 1);
    endianswap(&hdr.protocol, sizeof(int), 1);
    memset(hdr.desc, 0, DHDR_DESCCHARS);
    s_sprintfd(desc)("%s, %s, %s %s", modestr(gamemode, false), behindpath(smapname), asctime(), servdesc_current);
    if(strlen(desc) > DHDR_DESCCHARS)
        s_sprintf(desc)("%s, %s, %s %s", modestr(gamemode, true), behindpath(smapname), asctime(), servdesc_current);
    desc[DHDR_DESCCHARS - 1] = '\0';
    strcpy(hdr.desc, desc);
    gzwrite(demorecord, &hdr, sizeof(demoheader));

    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    welcomepacket(p, -1, packet);
    writedemo(1, p.buf, p.len);
    enet_packet_destroy(packet);

    uchar buf[MAXTRANS];
    loopv(clients)
    {
        client *ci = clients[i];
        if(ci->type==ST_EMPTY) continue;

        uchar header[16];
        ucharbuf q(&buf[sizeof(header)], sizeof(buf)-sizeof(header));
        putint(q, SV_INITC2S);
        sendstring(ci->name, q);
        putint(q, ci->team);
        putint(q, ci->skin);

        ucharbuf h(header, sizeof(header));
        putint(h, SV_CLIENT);
        putint(h, ci->clientnum);
        putuint(h, q.len);

        memcpy(&buf[sizeof(header)-h.len], header, h.len);

        writedemo(1, &buf[sizeof(header)-h.len], h.len+q.len);
    }
}

void listdemos(int cn)
{
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    if(!packet) return;
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_SENDDEMOLIST);
    putint(p, demofiles.length());
    loopv(demofiles) sendstring(demofiles[i].info, p);
    enet_packet_resize(packet, p.length());
    sendpacket(cn, 1, packet);
    if(!packet->referenceCount) enet_packet_destroy(packet);
}

static void cleardemos(int n)
{
    if(!n)
    {
        loopv(demofiles) delete[] demofiles[i].data;
        demofiles.setsize(0);
        sendservmsg("cleared all demos");
    }
    else if(demofiles.inrange(n-1))
    {
        delete[] demofiles[n-1].data;
        demofiles.remove(n-1);
        s_sprintfd(msg)("cleared demo %d", n);
        sendservmsg(msg);
    }
}

void senddemo(int cn, int num)
{
    if(!num) num = demofiles.length();
    if(!demofiles.inrange(num-1))
    {
        if(demofiles.empty()) sendservmsg("no demos available", cn);
        else
        {
            s_sprintfd(msg)("no demo %d available", num);
            sendservmsg(msg, cn);
        }
        return;
    }
    demofile &d = demofiles[num-1];
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS + d.len, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_SENDDEMO);
    sendstring(d.file, p);
    putint(p, d.len);
    p.put(d.data, d.len);
    enet_packet_resize(packet, p.length());
    sendpacket(cn, 2, packet);
    if(!packet->referenceCount) enet_packet_destroy(packet);
}

void enddemoplayback()
{
    if(!demoplayback) return;
    gzclose(demoplayback);
    demoplayback = NULL;

    loopv(clients) sendf(i, 1, "ri3", SV_DEMOPLAYBACK, 0, i);

    sendservmsg("demo playback finished");

    loopv(clients) sendwelcome(clients[i]);
}

void setupdemoplayback()
{
    demoheader hdr;
    string msg;
    msg[0] = '\0';
    s_sprintfd(file)("demos/%s.dmo", smapname);
    path(file);
    demoplayback = opengzfile(file, "rb9");
    if(!demoplayback) s_sprintf(msg)("could not read demo \"%s\"", file);
    else if(gzread(demoplayback, &hdr, sizeof(demoheader))!=sizeof(demoheader) || memcmp(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic)))
        s_sprintf(msg)("\"%s\" is not a demo file", file);
    else
    {
        endianswap(&hdr.version, sizeof(int), 1);
        endianswap(&hdr.protocol, sizeof(int), 1);
        if(hdr.version!=DEMO_VERSION) s_sprintf(msg)("demo \"%s\" requires an %s version of AssaultCube", file, hdr.version<DEMO_VERSION ? "older" : "newer");
        else if(hdr.protocol != PROTOCOL_VERSION && !(hdr.protocol < 0 && hdr.protocol == -PROTOCOL_VERSION)) s_sprintf(msg)("demo \"%s\" requires an %s version of AssaultCube", file, hdr.protocol<PROTOCOL_VERSION ? "older" : "newer");
    }
    if(msg[0])
    {
        if(demoplayback) { gzclose(demoplayback); demoplayback = NULL; }
        sendservmsg(msg);
        return;
    }

    s_sprintf(msg)("playing demo \"%s\"", file);
    sendservmsg(msg);

    sendf(-1, 1, "ri3", SV_DEMOPLAYBACK, 1, -1);

    if(gzread(demoplayback, &nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
    {
        enddemoplayback();
        return;
    }
    endianswap(&nextplayback, sizeof(nextplayback), 1);
}

void readdemo()
{
    if(!demoplayback) return;
    while(gamemillis>=nextplayback)
    {
        int chan, len;
        if(gzread(demoplayback, &chan, sizeof(chan))!=sizeof(chan) ||
           gzread(demoplayback, &len, sizeof(len))!=sizeof(len))
        {
            enddemoplayback();
            return;
        }
        endianswap(&chan, sizeof(chan), 1);
        endianswap(&len, sizeof(len), 1);
        ENetPacket *packet = enet_packet_create(NULL, len, 0);
        if(!packet || gzread(demoplayback, packet->data, len)!=len)
        {
            if(packet) enet_packet_destroy(packet);
            enddemoplayback();
            return;
        }
        sendpacket(-1, chan, packet);
        if(!packet->referenceCount) enet_packet_destroy(packet);
        if(gzread(demoplayback, &nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
        {
            enddemoplayback();
            return;
        }
        endianswap(&nextplayback, sizeof(nextplayback), 1);
    }
}

//

struct sflaginfo
{
    int state;
    int actor_cn;
    float pos[3];
    int lastupdate;
    int stolentime;
    short x, y;          // flag entity location
} sflaginfos[2];

void putflaginfo(ucharbuf &p, int flag)
{
    sflaginfo &f = sflaginfos[flag];
    putint(p, SV_FLAGINFO);
    putint(p, flag);
    putint(p, f.state);
    switch(f.state)
    {
        case CTFF_STOLEN:
            putint(p, f.actor_cn);
            break;
        case CTFF_DROPPED:
            loopi(3) putuint(p, (int)(f.pos[i]*DMF));
            break;
    }
}

bool flagdistance(sflaginfo &f, int cn)
{
    if(!valid_client(cn)) return false;
    client &c = *clients[cn];
    vec v(-1, -1, c.state.o.z);
    switch(f.state)
    {
        case CTFF_INBASE:
            v.x = f.x; v.y = f.y;
            break;
        case CTFF_DROPPED:
            v.x = f.pos[0]; v.y = f.pos[1];
            break;
    }
    if(v.x < 0) return true;
    float dist = c.state.o.dist(v);
    if(dist > 10)                // <2.5 would be normal, LAG may increase the value
    {
        c.farpickups++;
        logline(ACLOG_INFO, "[%s] %s touched the %s flag at distance %.2f (%d)", c.hostname, c.name, team_string(&f == sflaginfos + 1), dist, c.farpickups);
        return false;
    }
    return true;
}

void sendflaginfo(int flag = -1, int cn = -1)
{
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    if(flag >= 0) putflaginfo(p, flag);
    else loopi(2) putflaginfo(p, i);
    enet_packet_resize(packet, p.length());
    sendpacket(cn, 1, packet);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
}

void flagmessage(int flag, int message, int actor, int cn = -1)
{
    if(message == FM_KTFSCORE)
        sendf(cn, 1, "riiiii", SV_FLAGMSG, flag, message, actor, (gamemillis - sflaginfos[flag].stolentime) / 1000);
    else
        sendf(cn, 1, "riiii", SV_FLAGMSG, flag, message, actor);
}

void flagaction(int flag, int action, int actor)
{
    if(!valid_flag(flag)) return;
    sflaginfo &f = sflaginfos[flag];
    sflaginfo &of = sflaginfos[team_opposite(flag)];
    bool deadactor = valid_client(actor) ? clients[actor]->state.state != CS_ALIVE : true;
    int score = 0;
    int message = -1;

    if(m_ctf || m_htf)
    {
        switch(action)
        {
            case FA_PICKUP:  // ctf: f = enemy team    htf: f = own team
            case FA_STEAL:
            {
                if(deadactor || f.state != (action == FA_STEAL ? CTFF_INBASE : CTFF_DROPPED)) return;
                flagdistance(f, actor);
                int team = team_base(clients[actor]->team);
                if(m_ctf) team = team_opposite(team);
                if(team != flag) return;
                f.state = CTFF_STOLEN;
                f.actor_cn = actor;
                message = FM_PICKUP;
                break;
            }
            case FA_LOST:
                if(actor == -1) actor = f.actor_cn;
            case FA_DROP:
                if(f.state!=CTFF_STOLEN || f.actor_cn != actor) return;
                f.state = CTFF_DROPPED;
                loopi(3) f.pos[i] = clients[actor]->state.o[i];
                message = action == FA_LOST ? FM_LOST : FM_DROP;
                break;
            case FA_RETURN:
                if(f.state!=CTFF_DROPPED || m_htf) return;
                f.state = CTFF_INBASE;
                message = FM_RETURN;
                break;
            case FA_SCORE:  // ctf: f = carried by actor flag,  htf: f = hunted flag (run over by actor)
                if(m_ctf)
                {
                    if(f.state != CTFF_STOLEN || f.actor_cn != actor || of.state != CTFF_INBASE) return;
                    flagdistance(of, actor);
                    score = 1;
                    message = FM_SCORE;
                }
                else // m_htf
                {
                    if(f.state != CTFF_DROPPED) return;
                    flagdistance(f, actor);
                    score = (of.state == CTFF_STOLEN) ? 1 : 0;
                    message = score ? FM_SCORE : FM_SCOREFAIL;
                    if(of.actor_cn == actor) score *= 2;
                }
                f.state = CTFF_INBASE;
                break;

            case FA_RESET:
                f.state = CTFF_INBASE;
                message = FM_RESET;
                break;
        }
    }
    else if(m_ktf)  // f: active flag, of: idle flag
    {
        switch(action)
        {
            case FA_STEAL:
                if(deadactor || f.state != CTFF_INBASE) return;
                flagdistance(f, actor);
                f.state = CTFF_STOLEN;
                f.actor_cn = actor;
                f.stolentime = gamemillis;
                message = FM_PICKUP;
                break;
            case FA_SCORE:  // f = carried by actor flag
                if(actor != -1 || f.state != CTFF_STOLEN) return; // no client msg allowed here
                if(valid_client(f.actor_cn) && clients[f.actor_cn]->state.state == CS_ALIVE)
                {
                    actor = f.actor_cn;
                    score = 1;
                    message = FM_KTFSCORE;
                    break;
                }
            case FA_LOST:
                if(actor == -1) actor = f.actor_cn;
            case FA_DROP:
                if(f.actor_cn != actor || f.state != CTFF_STOLEN) return;
            case FA_RESET:
                if(f.state == CTFF_STOLEN)
                {
                    actor = f.actor_cn;
                    message = FM_LOST;
                }
                f.state = CTFF_IDLE;
                of.state = CTFF_INBASE;
                sendflaginfo(team_opposite(flag));
                break;
        }
    }
    if(score)
    {
        clients[actor]->state.flagscore += score;
        sendf(-1, 1, "riii", SV_FLAGCNT, actor, clients[actor]->state.flagscore);
    }
    if(valid_client(actor))
    {
        client &c = *clients[actor];
        switch(message)
        {
            case FM_PICKUP:
                logline(ACLOG_INFO,"[%s] %s %s the flag", c.hostname, c.name, action == FA_STEAL ? "stole" : "picked up");
                break;
            case FM_DROP:
            case FM_LOST:
                logline(ACLOG_INFO,"[%s] %s %s the flag", c.hostname, c.name, message == FM_LOST ? "lost" : "dropped");
                break;
            case FM_RETURN:
                logline(ACLOG_INFO,"[%s] %s returned the flag", c.hostname, c.name);
                break;
            case FM_SCORE:
                logline(ACLOG_INFO, "[%s] %s scored with the flag for %s, new score %d", c.hostname, c.name, team_string(c.team), c.state.flagscore);
                break;
            case FM_KTFSCORE:
                logline(ACLOG_INFO, "[%s] %s scored, carrying for %d seconds, new score %d", c.hostname, c.name, (gamemillis - f.stolentime) / 1000, c.state.flagscore);
                break;
            case FM_SCOREFAIL:
                logline(ACLOG_INFO, "[%s] %s failed to score", c.hostname, c.name);
                break;
            default:
                logline(ACLOG_INFO, "flagaction %d, actor %d, flag %d, message %d", action, actor, flag, message);
                break;
        }
    }
    else
    {
        switch(message)
        {
            case FM_RESET:
                logline(ACLOG_INFO,"the server reset the flag for team %s", team_string(flag));
                break;
            default:
                logline(ACLOG_INFO, "flagaction %d with invalid actor cn %d, flag %d, message %d", action, actor, flag, message);
                break;
        }
    }

    f.lastupdate = gamemillis;
    sendflaginfo(flag);
    if(message >= 0)
        flagmessage(flag, message, valid_client(actor) ? actor : -1);
}

int clienthasflag(int cn)
{
    if(m_flags && valid_client(cn))
    {
        loopi(2) { if(sflaginfos[i].state==CTFF_STOLEN && sflaginfos[i].actor_cn==cn) return i; }
    }
    return -1;
}

void ctfreset()
{
    int idleflag = m_ktf ? rnd(2) : -1;
    loopi(2)
    {
        sflaginfos[i].actor_cn = -1;
        sflaginfos[i].state = i == idleflag ? CTFF_IDLE : CTFF_INBASE;
        sflaginfos[i].lastupdate = -1;
    }
}

void sdropflag(int cn)
{
    int fl = clienthasflag(cn);
    if(fl >= 0) flagaction(fl, FA_LOST, cn);
}

void resetflag(int cn)
{
    int fl = clienthasflag(cn);
    if(fl >= 0) flagaction(fl, FA_RESET, -1);
}

void htf_forceflag(int flag)
{
    sflaginfo &f = sflaginfos[flag];
    int besthealth = 0, numbesthealth = 0;
    loopv(clients) if(clients[i]->type!=ST_EMPTY)
    {
        if(clients[i]->state.state == CS_ALIVE && team_base(clients[i]->team) == flag)
        {
            if(clients[i]->state.health == besthealth)
                numbesthealth++;
            else
            {
                if(clients[i]->state.health > besthealth)
                {
                    besthealth = clients[i]->state.health;
                    numbesthealth = 1;
                }
            }
        }
    }
    if(numbesthealth)
    {
        int pick = rnd(numbesthealth);
        loopv(clients) if(clients[i]->type!=ST_EMPTY)
        {
            if(clients[i]->state.state == CS_ALIVE && team_base(clients[i]->team) == flag && --pick < 0)
            {
                f.state = CTFF_STOLEN;
                f.actor_cn = i;
                sendflaginfo(flag);
                flagmessage(flag, FM_PICKUP, i);
                logline(ACLOG_INFO,"[%s] %s got forced to pickup the flag", clients[i]->hostname, clients[i]->name);
                break;
            }
        }
    }
    f.lastupdate = gamemillis;
}


bool canspawn(client *c, bool connecting = false)
{
    if(m_arena)
    {
        if(connecting && numauthedclients()<=2) return true;
        return false;
    }
    return true;
}

int arenaround = 0;

struct twoint { int index, value; };
int cmpscore(const int *a, const int *b) { return clients[*a]->at3_score - clients[*b]->at3_score; }
int cmptwoint(const struct twoint *a, const struct twoint *b) { return a->value - b->value; }
ivector tdistrib;
vector<twoint> sdistrib;

void distributeteam(int team)
{
    int numsp = team == 100 ? smapstats.spawns[2] : smapstats.spawns[team];
    if(!numsp) numsp = 30; // no spawns: try to distribute anyway
    twoint ti;
    tdistrib.setsize(0);
    loopv(clients) if(clients[i]->type!=ST_EMPTY)
    {
        if(team == 100 || team == clients[i]->team)
        {
            tdistrib.add(i);
            clients[i]->at3_score = rand();
        }
    }
    tdistrib.sort(cmpscore); // random player order
    sdistrib.setsize(0);
    loopi(numsp)
    {
        ti.index = i;
        ti.value = rand();
        sdistrib.add(ti);
    }
    sdistrib.sort(cmptwoint); // random spawn order
    int x = 0;
    loopv(tdistrib)
    {
        clients[tdistrib[i]]->spawnindex = sdistrib[x++].index;
        x %= sdistrib.length();
    }
}

void distributespawns()
{
    loopv(clients) if(clients[i]->type!=ST_EMPTY)
    {
        clients[i]->spawnindex = -1;
    }
    if(m_teammode)
    {
        distributeteam(0);
        distributeteam(1);
    }
    else
    {
        distributeteam(100);
    }
}

void arenacheck()
{
    if(!m_arena || interm || gamemillis<arenaround || !numactiveclients()) return;

    if(arenaround)
    {   // start new arena round
        arenaround = 0;
        distributespawns();
        loopv(clients) if(clients[i]->type!=ST_EMPTY && clients[i]->isauthed && team_isactive(clients[i]->team))
        {
            if(clients[i]->isonrightmap)
            {
                clients[i]->state.respawn();
                sendspawn(clients[i]);
            }
            else sendservmsg("\f3you have to be on the right map to spawn: type /getmap", i);    // FIXME
        }
        return;
    }

#ifndef STANDALONE
    if(m_botmode && clients[0]->type==ST_LOCAL)
    {
        bool alive = false, dead = false;
        loopv(players) if(players[i])
        {
            if(players[i]->state==CS_DEAD) dead = true;
            else alive = true;
        }
        if((dead && !alive) || player1->state==CS_DEAD)
        {
            sendf(-1, 1, "ri2", SV_ARENAWIN, player1->state==CS_ALIVE ? getclientnum() : (alive ? -2 : -1));
            arenaround = gamemillis+5000;
        }
        return;
    }
#endif
    client *alive = NULL;
    bool dead = false;
    int lastdeath = 0;
    loopv(clients)
    {
        client &c = *clients[i];
        if(c.type==ST_EMPTY || !c.isauthed) continue;
        if(c.state.state==CS_ALIVE || (c.state.state==CS_DEAD && c.state.lastspawn>=0))
        {
            if(!alive) alive = &c;
            else if(!m_teammode || alive->team != c.team) return;
        }
        else if(c.state.state==CS_DEAD)
        {
            dead = true;
            lastdeath = max(lastdeath, c.state.lastdeath);
        }
    }

    if(!dead || gamemillis < lastdeath + 500) return;
    sendf(-1, 1, "ri2", SV_ARENAWIN, alive ? alive->clientnum : -1);
    arenaround = gamemillis+5000;
    if(autoteam && m_teammode) refillteams(true);
}

#define SPAMREPEATINTERVAL  20   // detect doubled lines only if interval < 20 seconds
#define SPAMMAXREPEAT       3    // 4th time is SPAM
#define SPAMCHARPERMINUTE   220  // good typist
#define SPAMCHARINTERVAL    30   // allow 20 seconds typing at maxspeed

bool spamdetect(client *cl, char *text) // checks doubled lines and average typing speed
{
    if(cl->type != ST_TCPIP || cl->role == CR_ADMIN) return false;
    bool spam = false;
    int pause = servmillis - cl->lastsay;
    if(pause < 0 || pause > 90*1000) pause = 90*1000;
    cl->saychars -= (SPAMCHARPERMINUTE * pause) / (60*1000);
    cl->saychars += (int)strlen(text);
    if(cl->saychars < 0) cl->saychars = 0;
    if(text[0] && !strcmp(text, cl->lastsaytext) && servmillis - cl->lastsay < SPAMREPEATINTERVAL*1000)
    {
        spam = ++cl->spamcount > SPAMMAXREPEAT;
    }
    else
    {
         s_strcpy(cl->lastsaytext, text);
         cl->spamcount = 0;
    }
    cl->lastsay = servmillis;
    if(cl->saychars > (SPAMCHARPERMINUTE * SPAMCHARINTERVAL) / 60)
        spam = true;
    return spam;
}

void sendteamtext(char *text, int sender)
{
    if(!valid_client(sender) || clients[sender]->team == TEAM_VOID) return;
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_TEAMTEXT);
    putint(p, sender);
    sendstring(text, p);
    enet_packet_resize(packet, p.length());
    loopv(clients) if(i!=sender)
    {
        if(clients[i]->team == clients[sender]->team || !m_teammode) // send to everyone in non-team mode
            sendpacket(i, 1, packet);
    }
    if(packet->referenceCount==0) enet_packet_destroy(packet);
}

void sendvoicecomteam(int sound, int sender)
{
    if(!valid_client(sender) || clients[sender]->team == TEAM_VOID) return;
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_VOICECOMTEAM);
    putint(p, sender);
    putint(p, sound);
    enet_packet_resize(packet, p.length());
    loopv(clients) if(i!=sender)
    {
        if(clients[i]->team == clients[sender]->team || !m_teammode)
            sendpacket(i, 1, packet);
    }
    if(packet->referenceCount==0) enet_packet_destroy(packet);
}

int spawntime(int type)
{
    int np = numclients();
    np = np<3 ? 4 : (np>4 ? 2 : 3);         // spawn times are dependent on number of players
    int sec = 0;
    switch(type)
    {
        case I_CLIPS:
        case I_AMMO:
        case I_GRENADE: sec = np*2; break;
        case I_HEALTH: sec = np*5; break;
        case I_ARMOUR: sec = 20; break;
        case I_AKIMBO: sec = 60; break;
    }
    return sec*1000;
}

bool serverpickup(int i, int sender)         // server side item pickup, acknowledge first client that gets it
{
    const char *hn = sender >= 0 && clients[sender]->type == ST_TCPIP ? clients[sender]->hostname : NULL;
    if(!sents.inrange(i))
    {
        if(hn) logline(ACLOG_INFO, "[%s] tried to pick up entity #%d - doesn't exist on this map", hn, i);
        return false;
    }
    server_entity &e = sents[i];
    if(!e.spawned)
    {
        if(!e.spawntime && hn) logline(ACLOG_INFO, "[%s] tried to pick up entity #%d - can't be picked up in this gamemode or at all", hn, i);
        return false;
    }
    if(sender>=0)
    {
        client *cl = clients[sender];
        if(cl->type==ST_TCPIP)
        {
            if(cl->state.state!=CS_ALIVE || !cl->state.canpickup(e.type)) return false;
            if(e.hascoord)
            {
                vec v(e.x, e.y, cl->state.o.z);
                float dist = cl->state.o.dist(v);
                if(dist > 10)                // <2.5 would be normal, LAG may increase the value
                {
                    cl->farpickups++;
                    logline(ACLOG_INFO, "[%s] %s picked up entity type %d #%d, distance %.2f (%d)", cl->hostname, cl->name, e.type, i, dist, cl->farpickups);
                }
            }
        }
        sendf(-1, 1, "ri3", SV_ITEMACC, i, sender);
        cl->state.pickup(sents[i].type);
    }
    e.spawned = false;
    e.spawntime = spawntime(e.type);
    return true;
}

void checkitemspawns(int diff)
{
    if(!diff) return;
    loopv(sents) if(sents[i].spawntime)
    {
        sents[i].spawntime -= diff;
        if(sents[i].spawntime<=0)
        {
            sents[i].spawntime = 0;
            sents[i].spawned = true;
            sendf(-1, 1, "ri2", SV_ITEMSPAWN, i);
        }
    }
}

void serverdamage(client *target, client *actor, int damage, int gun, bool gib, const vec &hitpush = vec(0, 0, 0))
{
    clientstate &ts = target->state;
    ts.dodamage(damage);
    actor->state.damage += damage != 1000 ? damage : 0;
    sendf(-1, 1, "ri6", gib ? SV_GIBDAMAGE : SV_DAMAGE, target->clientnum, actor->clientnum, damage, ts.armour, ts.health);
    if(target!=actor && !hitpush.iszero())
    {
        vec v(hitpush);
        if(!v.iszero()) v.normalize();
        sendf(target->clientnum, 1, "ri6", SV_HITPUSH, gun, damage,
            int(v.x*DNF), int(v.y*DNF), int(v.z*DNF));
    }
    if(ts.health<=0)
    {
        int targethasflag = clienthasflag(target->clientnum);
        bool tk = false, suic = false;
        target->state.deaths++;
        if(target!=actor)
        {
            if(!isteam(target->team, actor->team)) actor->state.frags += gib ? 2 : 1;
            else
            {
                actor->state.frags--;
                actor->state.teamkills++;
                tk = true;
            }
        }
        else
        { // suicide
            actor->state.frags--;
            suic = true;
            logline(ACLOG_INFO, "[%s] %s suicided", actor->hostname, actor->name);
        }
        sendf(-1, 1, "ri4", gib ? SV_GIBDIED : SV_DIED, target->clientnum, actor->clientnum, actor->state.frags);
        if((suic || tk) && (m_htf || m_ktf) && targethasflag >= 0)
        {
            actor->state.flagscore--;
            sendf(-1, 1, "riii", SV_FLAGCNT, actor->clientnum, actor->state.flagscore);
        }
        target->position.setsizenodelete(0);
        ts.state = CS_DEAD;
        ts.lastdeath = gamemillis;
        if(!suic) logline(ACLOG_INFO, "[%s] %s %s%s %s", actor->hostname, actor->name, gib ? "gibbed" : "fragged", tk ? " his teammate" : "", target->name);
        if(m_flags && targethasflag >= 0)
        {
            if(m_ctf)
                flagaction(targethasflag, tk ? FA_RESET : FA_LOST, -1);
            else if(m_htf)
                flagaction(targethasflag, FA_LOST, -1);
            else // ktf || tktf
                flagaction(targethasflag, FA_RESET, -1);
        }
        // don't issue respawn yet until DEATHMILLIS has elapsed
        // ts.respawn();

        if(isdedicated && actor->type == ST_TCPIP)
        {
            if(actor->state.frags < scl.banthreshold)
            {
                ban b = { actor->peer->address, servmillis+20*60*1000 };
                bans.add(b);
                disconnect_client(actor->clientnum, DISC_AUTOBAN);
            }
            else if(actor->state.frags < scl.kickthreshold) disconnect_client(actor->clientnum, DISC_AUTOKICK);
        }
    }
}

#include "serverevents.h"

bool updatedescallowed(void) { return scl.servdesc_pre[0] || scl.servdesc_suf[0]; }

void updatesdesc(const char *newdesc, ENetAddress *caller = NULL)
{
    if(!newdesc || !newdesc[0] || !updatedescallowed())
    {
        s_strcpy(servdesc_current, scl.servdesc_full);
        custom_servdesc = false;
    }
    else
    {
        s_sprintf(servdesc_current)("%s%s%s", scl.servdesc_pre, newdesc, scl.servdesc_suf);
        custom_servdesc = true;
        if(caller) servdesc_caller = *caller;
    }
}

void forceteam(int client, int team, bool respawn, bool notify = false)
{
    if(!valid_client(client) || team < 0 || team > 1) return;
    if(clients[client]->lastforce && (servmillis - clients[client]->lastforce) < 2000) return;
    sendf(client, 1, "riii", SV_FORCETEAM, team, (respawn ? 1 : 0) | (respawn && !notify ? 2 : 0));
    clients[client]->lastforce = servmillis;
    if(notify) sendf(-1, 1, "riii", SV_FORCENOTIFY, client, team);
}

int calcscores() // skill eval
{
    int fp12 = (m_ctf || m_htf) ? 55 : 33;
    int fp3 = (m_ctf || m_htf) ? 25 : 15;
    int sum = 0;
    loopv(clients) if(clients[i]->type!=ST_EMPTY)
    {
        clientstate &cs = clients[i]->state;
        sum += clients[i]->at3_score = (cs.frags * 100) / (cs.deaths ? cs.deaths : 1)
                                     + (cs.flagscore < 3 ? fp12 * cs.flagscore : 2 * fp12 + fp3 * (cs.flagscore - 2));
    }
    return sum;
}

ivector shuffle;

void shuffleteams(bool respawn = true)
{
    int numplayers = numclients();
    int team, sums = calcscores();
    if(gamemillis < 2 * 60 *1000)
    { // random
        int teamsize[2] = {0, 0};
        loopv(clients) if(clients[i]->type!=ST_EMPTY)
        {
            sums += rnd(1000);
            team = sums & 1;
            if(teamsize[team] >= numplayers/2) team = team_opposite(team);
            forceteam(i, team, respawn);
            teamsize[team]++;
            sums >>= 1;
        }
    }
    else
    { // skill sorted
        shuffle.setsize(0);
        sums /= 4 * numplayers + 2;
        team = rnd(2);
        loopv(clients) if(clients[i]->type!=ST_EMPTY) { clients[i]->at3_score += rnd(sums | 1); shuffle.add(i); }
        shuffle.sort(cmpscore);
        loopi(shuffle.length())
        {
            forceteam(shuffle[i], team, respawn);
            team = !team;
        }
    }
}

bool refillteams(bool now, bool notify)  // force only minimal amounts of players
{
    static int lasttime_eventeams = 0;
    int teamsize[2] = {0, 0}, teamscore[2] = {0, 0}, moveable[2] = {0, 0};
    bool switched = false;

    calcscores();
    loopv(clients) if(clients[i]->type!=ST_EMPTY)     // playerlist stocktaking
    {
        client *c = clients[i];
        c->at3_dontmove = true;
        if(c->isauthed)
        {
            if(team_isactive(c->team)) // only active players count
            {
                teamsize[c->team]++;
                teamscore[c->team] += c->at3_score;
                if(clienthasflag(i) < 0)
                {
                    c->at3_dontmove = false;
                    moveable[c->team]++;
                    if(c->lastforce && (servmillis - c->lastforce) < 3000) return false; // possible unanswered forceteam commands
                }
            }
        }
    }
    int bigteam = teamsize[1] > teamsize[0];
    int allplayers = teamsize[0] + teamsize[1];
    int diffnum = teamsize[bigteam] - teamsize[!bigteam];
    int diffscore = teamscore[bigteam] - teamscore[!bigteam];
    if(lasttime_eventeams > gamemillis) lasttime_eventeams = 0;
    if(diffnum > 1)
    {
        if(now || gamemillis - lasttime_eventeams > 8000 + allplayers * 1000 || diffnum > 2 + allplayers / 10)
        {
            // time to even out teams
            loopv(clients) if(clients[i]->type!=ST_EMPTY && clients[i]->team != bigteam) clients[i]->at3_dontmove = true;  // dont move small team players
            while(diffnum > 1 && moveable[bigteam] > 0)
            {
                // pick best fitting cn
                int pick = -1;
                int bestfit = 1000000000;
                int targetscore = diffscore / (diffnum & ~1);
                loopv(clients) if(clients[i]->type!=ST_EMPTY && !clients[i]->at3_dontmove) // try all still movable players
                {
                    int fit = targetscore - clients[i]->at3_score;
                    if(fit < 0 ) fit = -(fit * 15) / 10;       // avoid too good players
                    int forcedelay = clients[i]->at3_lastforce ? (1000 - (gamemillis - clients[i]->at3_lastforce) / (5 * 60)) : 0;
                    if(forcedelay > 0) fit += (fit * forcedelay) / 600;   // avoid lately forced players
                    if(fit < bestfit + fit * rnd(100) / 400)   // search 'almost' best fit
                    {
                        bestfit = fit;
                        pick = i;
                    }
                }
                if(pick < 0) break; // should really never happen
                // move picked player
                forceteam(pick, !bigteam, true, notify);

                diffnum -= 2;
                diffscore -= 2 * clients[pick]->at3_score;
                moveable[bigteam]--;
                clients[pick]->at3_dontmove = true;
                clients[pick]->at3_lastforce = gamemillis;  // try not to force this player again for the next 5 minutes
                switched = true;
            }
        }
    }
    if(diffnum < 2)
        lasttime_eventeams = gamemillis;
    return switched;
}

void resetserver(const char *newname, int newmode, int newtime)
{
    if(m_demo) enddemoplayback();
    else enddemorecord();

    smode = newmode;
    s_strcpy(smapname, newname);

    minremain = newtime >= 0 ? newtime : (m_teammode ? 15 : 10);
    gamemillis = 0;
    gamelimit = minremain*60000;

    interm = 0;
    if(!laststatus) laststatus = servmillis-61*1000;
    lastfillup = servmillis;
    sents.setsize(0);
    savedscores.setsize(0);
    ctfreset();
}

void startgame(const char *newname, int newmode, int newtime, bool notify)
{
    bool lastteammode = m_teammode;
    resetserver(newname, newmode, newtime);

    if(custom_servdesc && findcnbyaddress(&servdesc_caller) < 0)
    {
        updatesdesc(NULL);
        if(notify)
        {
            sendservmsg("server description reset to default");
            logline(ACLOG_INFO, "server description reset to '%s'", servdesc_current);
        }
    }

    mapbuffer.clear();
    if(isdedicated) mapbuffer.load();
    mapstats *ms = getservermapstats(smapname, isdedicated);
    if(ms)
    {
        smapstats = *ms;
        loopi(2)
        {
            sflaginfo &f = sflaginfos[i];
            if(smapstats.flags[i] == 1)    // don't check flag positions, if there is more than one flag per team
            {
                short *fe = smapstats.entposs + smapstats.flagents[i] * 3;
                f.x = *fe;
                fe++;
                f.y = *fe;
            }
            else f.x = f.y = -1;
        }
        entity e;
        loopi(smapstats.hdr.numents)
        {
            e.type = smapstats.enttypes[i];
            e.transformtype(smode);
            server_entity se = { e.type, false, true, 0, smapstats.entposs[i * 3], smapstats.entposs[i * 3 + 1]};
            sents.add(se);
            if(e.fitsmode(smode)) sents[i].spawned = true;
        }
        mapbuffer.setrevision();
    }
    else
    {
        memset(&smapstats, 0, sizeof(smapstats));
        if(isdedicated) sendservmsg("\f3server error: map not found - please start another map");
    }
    if(notify)
    {
        // change map
        sendf(-1, 1, "risiii", SV_MAPCHANGE, smapname, smode, mapbuffer.available(), mapbuffer.revision);
        if(smode>1 || (smode==0 && numnonlocalclients()>0)) sendf(-1, 1, "ri2", SV_TIMEUP, minremain);
    }
    logline(ACLOG_INFO, "\nGame start: %s on %s, %d players, %d minutes, mastermode %d, (map rev %d/%d, itemlist %spreloaded, 'getmap' %sprepared)",
        modestr(smode), smapname, numclients(), minremain, mastermode, smapstats.hdr.maprevision, smapstats.cgzsize, ms ? "" : "not ", mapbuffer.available() ? "" : "not ");

    arenaround = 0;
    if(m_arena)
    {
        distributespawns();
    }
    if(notify)
    {
        // shuffle if previous mode wasn't a team-mode
        if(m_teammode)
        {
            if(!lastteammode)
                shuffleteams(false);
            else if(autoteam)
                refillteams(true, false);
        }
        // send spawns
        loopv(clients) if(clients[i]->type!=ST_EMPTY)
        {
            client *c = clients[i];
            c->mapchange();
            if(m_mp(smode)) sendspawn(c);
        }
    }
    if(m_demo) setupdemoplayback();
    else if((demonextmatch || scl.demoeverymatch) && *newname && numnonlocalclients() > 0)
    {
        demonextmatch = false;
        setupdemorecord();
    }
    if(notify && m_ktf) sendflaginfo();

    nextmapname[0] = '\0';
    forceintermission = false;
}

bool isbanned(int cn)
{
    if(!valid_client(cn)) return false;
    client &c = *clients[cn];
    if(c.type==ST_LOCAL) return false;
    loopv(bans)
    {
        ban &b = bans[i];
        if(b.millis < servmillis) { bans.remove(i--); }
        if(b.address.host == c.peer->address.host) { return true; }
    }
    return ipblacklist.check(c.peer->address.host);
}

int serveroperator()
{
    loopv(clients) if(clients[i]->type!=ST_EMPTY && clients[i]->role > CR_DEFAULT) return i;
    return -1;
}

void sendserveropinfo(int receiver)
{
    int op = serveroperator();
    sendf(receiver, 1, "riii", SV_SERVOPINFO, op, op >= 0 ? clients[op]->role : -1);
}

#include "serveractions.h"

struct voteinfo
{
    int owner, callmillis, result;
    serveraction *action;

    voteinfo() : owner(0), callmillis(0), result(VOTE_NEUTRAL), action(NULL) {}

    void end(int result)
    {
        if(action && !action->isvalid()) result = VOTE_NO; // don't perform() invalid votes
        sendf(-1, 1, "ri2", SV_VOTERESULT, result);
        this->result = result;
        if(result == VOTE_YES)
        {
            if(valid_client(owner)) clients[owner]->lastvotecall = 0;
            if(action) action->perform();
        }
        loopv(clients) clients[i]->vote = VOTE_NEUTRAL;
    }

    bool isvalid() { return valid_client(owner) && action != NULL && action->isvalid(); }
    bool isalive() { return servmillis - callmillis < 40*1000; }

    void evaluate(bool forceend = false)
    {
        if(result!=VOTE_NEUTRAL) return; // block double action
        if(action && !action->isvalid()) end(VOTE_NO);
        int stats[VOTE_NUM] = {0};
        int adminvote = VOTE_NEUTRAL;
        loopv(clients)
            if(clients[i]->type!=ST_EMPTY && clients[i]->connectmillis < callmillis)
            {
                stats[clients[i]->vote]++;
                if(clients[i]->role==CR_ADMIN) adminvote = clients[i]->vote;
            };

        bool admin = clients[owner]->role==CR_ADMIN || (!isdedicated && clients[owner]->type==ST_LOCAL);
        int total = stats[VOTE_NO]+stats[VOTE_YES]+stats[VOTE_NEUTRAL];
        const float requiredcount = 0.51f;
        if(stats[VOTE_YES]/(float)total > requiredcount || admin || adminvote == VOTE_YES)
            end(VOTE_YES);
        else if(forceend || stats[VOTE_NO]/(float)total > requiredcount || stats[VOTE_NO] >= stats[VOTE_YES]+stats[VOTE_NEUTRAL] || adminvote == VOTE_NO)
            end(VOTE_NO);
        else return;
    }
};

static voteinfo *curvote = NULL;

bool svote(int sender, int vote, ENetPacket *msg) // true if the vote was placed successfully
{
    if(!curvote || !valid_client(sender) || vote < VOTE_YES || vote > VOTE_NO) return false;
    if(clients[sender]->vote != VOTE_NEUTRAL)
    {
        sendf(sender, 1, "ri2", SV_CALLVOTEERR, VOTEE_MUL);
        return false;
    }
    else
    {
        sendpacket(-1, 1, msg, sender);

        clients[sender]->vote = vote;
        logline(ACLOG_DEBUG,"[%s] client %s voted %s", clients[sender]->hostname, clients[sender]->name, vote == VOTE_NO ? "no" : "yes");
        curvote->evaluate();
        return true;
    }
}

void scallvotesuc(voteinfo *v)
{
    if(!v->isvalid()) return;
    DELETEP(curvote);
    curvote = v;
    clients[v->owner]->lastvotecall = servmillis;

    sendf(v->owner, 1, "ri", SV_CALLVOTESUC);
    logline(ACLOG_INFO, "[%s] client %s called a vote: %s", clients[v->owner]->hostname, clients[v->owner]->name, v->action->desc ? v->action->desc : "[unknown]");
}

void scallvoteerr(voteinfo *v, int error)
{
    if(!valid_client(v->owner)) return;
    sendf(v->owner, 1, "ri2", SV_CALLVOTEERR, error);
    logline(ACLOG_INFO, "[%s] client %s failed to call a vote: %s (%s)", clients[v->owner]->hostname, clients[v->owner]->name, v->action->desc ? v->action->desc : "[unknown]", voteerrorstr(error));
}

bool scallvote(voteinfo *v, ENetPacket *msg) // true if a regular vote was called
{
    int area = isdedicated ? EE_DED_SERV : EE_LOCAL_SERV;
    int error = -1;

    if(!v || !v->isvalid()) error = VOTEE_INVALID;
    else if(v->action->role > clients[v->owner]->role) error = VOTEE_PERMISSION;
    else if(!(area & v->action->area)) error = VOTEE_AREA;
    else if(curvote && curvote->result==VOTE_NEUTRAL) error = VOTEE_CUR;
    else if(clients[v->owner]->role == CR_DEFAULT && v->action->isdisabled()) error = VOTEE_DISABLED;
    else if(clients[v->owner]->lastvotecall && servmillis - clients[v->owner]->lastvotecall < 60*1000 && clients[v->owner]->role != CR_ADMIN && numclients()>1)
        error = VOTEE_MAX;

    if(error>=0)
    {
        scallvoteerr(v, error);
        return false;
    }
    else
    {
        sendpacket(-1, 1, msg, v->owner);

        scallvotesuc(v);
        return true;
    }
}

void changeclientrole(int client, int role, char *pwd, bool force)
{
    pwddetail pd;
    if(!isdedicated || !valid_client(client)) return;
    pd.line = -1;
    if(force || role == CR_DEFAULT || (role == CR_ADMIN && pwd && pwd[0] && passwords.check(clients[client]->name, pwd, clients[client]->salt, &pd) && !pd.denyadmin))
    {
        if(role == clients[client]->role) return;
        if(role > CR_DEFAULT)
        {
            loopv(clients) clients[i]->role = CR_DEFAULT;
        }
        clients[client]->role = role;
        sendserveropinfo(-1);
        if(pd.line > -1)
            logline(ACLOG_INFO,"[%s] player %s used admin password in line %d", clients[client]->hostname, clients[client]->name[0] ? clients[client]->name : "[unnamed]", pd.line);
        logline(ACLOG_INFO,"[%s] set role of player %s to %s", clients[client]->hostname, clients[client]->name[0] ? clients[client]->name : "[unnamed]", role == CR_ADMIN ? "admin" : "normal player"); // flowtron : connecting players haven't got a name yet (connectadmin)
    }
    else if(pwd && pwd[0]) disconnect_client(client, DISC_SOPLOGINFAIL); // avoid brute-force
    if(curvote) curvote->evaluate();
}

const char *disc_reason(int reason)
{
    static const char *disc_reasons[] = { "normal", "end of packet", "client num", "kicked by server operator", "banned by server operator", "tag type", "connection refused due to ban", "wrong password", "failed admin login", "server FULL - maxclients", "server mastermode is \"private\"", "auto kick - did your score drop below the threshold?", "auto ban - did your score drop below the threshold?", "duplicate connection", "inappropriate nickname" };
    return reason >= 0 && (size_t)reason < sizeof(disc_reasons)/sizeof(disc_reasons[0]) ? disc_reasons[reason] : "unknown";
}

void disconnect_client(int n, int reason)
{
    if(!clients.inrange(n) || clients[n]->type!=ST_TCPIP) return;
    sdropflag(n);
    client &c = *clients[n];
    const char *scoresaved = "";
    if(c.haswelcome)
    {
        savedscore *sc = findscore(c, true);
        if(sc)
        {
            sc->save(c.state);
            scoresaved = ", score saved";
        }
    }
    int sp = (servmillis - c.connectmillis) / 1000;
    if(reason>=0) logline(ACLOG_INFO, "[%s] disconnecting client %s (%s) cn %d, %d seconds played%s", c.hostname, c.name, disc_reason(reason), n, sp, scoresaved);
    else logline(ACLOG_INFO, "[%s] disconnected client %s cn %d, %d seconds played%s", c.hostname, c.name, n, sp, scoresaved);
    c.peer->data = (void *)-1;
    if(reason>=0) enet_peer_disconnect(c.peer, reason);
    clients[n]->zap();
    sendf(-1, 1, "rii", SV_CDIS, n);
    if(curvote) curvote->evaluate();
}

void sendwhois(int sender, int cn)
{
    if(!valid_client(sender) || !valid_client(cn)) return;
    if(clients[cn]->type == ST_TCPIP)
    {
        uint ip = clients[cn]->peer->address.host;
        if(clients[sender]->role != CR_ADMIN) ip &= 0xFFFF; // only admin gets full IP
        sendf(sender, 1, "ri3", SV_WHOISINFO, cn, ip);
    }
}

void sendresume(client &c, bool broadcast)
{
    sendf(broadcast ? -1 : c.clientnum, 1, "rxi2i9vvi", broadcast ? c.clientnum : -1, SV_RESUME,
            c.clientnum,
            c.state.state,
            c.state.lifesequence,
            c.state.primary,
            c.state.gunselect,
            c.state.flagscore,
            c.state.frags,
            c.state.deaths,
            c.state.health,
            c.state.armour,
            NUMGUNS, c.state.ammo,
            NUMGUNS, c.state.mag,
            -1);
}

void sendinits2c(client &c)
{
    sendf(c.clientnum, 1, "ri5", SV_INITS2C, c.clientnum, isdedicated ? SERVER_PROTOCOL_VERSION : PROTOCOL_VERSION, c.salt, scl.serverpassword[0] ? 1 : 0);
}

void welcomepacket(ucharbuf &p, int n, ENetPacket *packet, bool forcedeath)
{
    #define CHECKSPACE(n) \
    { \
        int space = (n); \
        if(p.remaining() < space) \
        { \
           enet_packet_resize(packet, packet->dataLength + max(MAXTRANS, space - p.remaining())); \
           p.buf = packet->data; \
           p.maxlen = (int)packet->dataLength; \
        } \
    }

    if(!smapname[0]) maprot.next(false);

    client *c = valid_client(n) ? clients[n] : NULL;
    int numcl = numclients();

    putint(p, SV_WELCOME);
    putint(p, smapname[0] && !m_demo ? numcl : -1);
    if(smapname[0] && !m_demo)
    {
        putint(p, SV_MAPCHANGE);
        sendstring(smapname, p);
        putint(p, smode);
        putint(p, mapbuffer.available());
        putint(p, mapbuffer.revision);
        if(smode>1 || (smode==0 && numnonlocalclients()>0))
        {
            putint(p, SV_TIMEUP);
            putint(p, minremain);
        }
        if(numcl>0)
        {
            putint(p, SV_ITEMLIST);
            loopv(sents) if(sents[i].spawned)
            {
                putint(p, i);
                CHECKSPACE(256);
            }
            putint(p, -1);
        }
        if(m_flags)
        {
            CHECKSPACE(256);
            loopi(2) putflaginfo(p, i);
        }
    }
    if(c && c->type == ST_TCPIP && serveroperator() != -1) sendserveropinfo(n);
    if(numcl>1)
    {
        putint(p, SV_FORCETEAM);
        putint(p, freeteam(n));
        putint(p, 0);
    }
    if(c) c->lastforce = servmillis;
    bool restored = false;
    if(c)
    {
        if(c->type==ST_TCPIP)
        {
            savedscore *sc = findscore(*c, false);
            if(sc)
            {
                sc->restore(c->state);
                restored = true;
            }
        }

        CHECKSPACE(256);
        if(!canspawn(c, true) || forcedeath)
        {
            putint(p, SV_FORCEDEATH);
            putint(p, n);
            sendf(-1, 1, "ri2x", SV_FORCEDEATH, n, n);
        }
        else
        {
            clientstate &gs = c->state;
            spawnstate(c);
            putint(p, SV_SPAWNSTATE);
            putint(p, gs.lifesequence);
            putint(p, gs.health);
            putint(p, gs.armour);
            putint(p, gs.primary);
            putint(p, gs.gunselect);
            putint(p, -1);
            loopi(NUMGUNS) putint(p, gs.ammo[i]);
            loopi(NUMGUNS) putint(p, gs.mag[i]);
            gs.lastspawn = gamemillis;
        }
    }
    if(clients.length()>1 || restored)
    {
        putint(p, SV_RESUME);
        loopv(clients)
        {
            client &c = *clients[i];
            if(c.type!=ST_TCPIP || (c.clientnum==n && !restored)) continue;
            CHECKSPACE(256);
            putint(p, c.clientnum);
            putint(p, c.state.state);
            putint(p, c.state.lifesequence);
            putint(p, c.state.primary);
            putint(p, c.state.gunselect);
            putint(p, c.state.flagscore);
            putint(p, c.state.frags);
            putint(p, c.state.deaths);
            putint(p, c.state.health);
            putint(p, c.state.armour);
            loopi(NUMGUNS) putint(p, c.state.ammo[i]);
            loopi(NUMGUNS) putint(p, c.state.mag[i]);
        }
        putint(p, -1);
    }
    putint(p, SV_AUTOTEAM);
    putint(p, autoteam ? 1 : 0);
    const char *motd = scl.motd[0] ? scl.motd : infofiles.getmotd(c ? c->lang : "");
    if(motd)
    {
        CHECKSPACE(5+2*(int)strlen(motd)+1);
        putint(p, SV_TEXT);
        sendstring(motd, p);
    }

    #undef CHECKSPACE
}

void sendwelcome(client *cl, int chan, bool forcedeath)
{
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    welcomepacket(p, cl->clientnum, packet, forcedeath);
    enet_packet_resize(packet, p.length());
    sendpacket(cl->clientnum, chan, packet);
    if(!packet->referenceCount) enet_packet_destroy(packet);
    cl->haswelcome = true;
}

void forcedeath(client *cl)
{
    cl->state.state = CS_DEAD;
    cl->state.respawn();
    sendf(-1, 1, "rii", SV_FORCEDEATH, cl->clientnum);
}

int checktype(int type, client *cl)
{
    if(cl && cl->type==ST_LOCAL) return type;
    // only allow edit messages in coop-edit mode
    static int edittypes[] = { SV_EDITENT, SV_EDITH, SV_EDITT, SV_EDITS, SV_EDITD, SV_EDITE, SV_NEWMAP };
    if(cl && smode!=GMODE_COOPEDIT) loopi(sizeof(edittypes)/sizeof(int)) if(type == edittypes[i]) return -1;
    // server only messages
    static int servtypes[] = { SV_INITS2C, SV_WELCOME, SV_CDIS, SV_GIBDIED, SV_DIED,
                        SV_GIBDAMAGE, SV_DAMAGE, SV_HITPUSH, SV_SHOTFX,
                        SV_SPAWNSTATE, SV_FORCEDEATH, SV_RESUME, SV_TIMEUP,
                        SV_ITEMACC, SV_MAPCHANGE, SV_ITEMSPAWN, SV_PONG,
                        SV_SERVMSG, SV_ITEMLIST, SV_FLAGINFO, SV_FLAGMSG, SV_FLAGCNT,
                        SV_ARENAWIN, SV_SERVOPINFO,
                        SV_CALLVOTESUC, SV_CALLVOTEERR, SV_VOTERESULT,
                        SV_FORCETEAM, SV_AUTOTEAM, SV_WHOISINFO,
                        SV_SENDDEMOLIST, SV_SENDDEMO, SV_DEMOPLAYBACK,
                        SV_CLIENT, SV_FORCENOTIFY };
    if(cl) loopi(sizeof(servtypes)/sizeof(int)) if(type == servtypes[i]) return -1;
    if (type < 0 || type >= SV_NUM) return -1;
    return type;
}

// server side processing of updates: does very little and most state is tracked client only
// could be extended to move more gameplay to server (at expense of lag)

void process(ENetPacket *packet, int sender, int chan)   // sender may be -1
{
    ucharbuf p(packet->data, packet->dataLength);
    char text[MAXTRANS];
    client *cl = sender>=0 ? clients[sender] : NULL;
    pwddetail pd;
    int type;

    if(cl && !cl->isauthed)
    {
        int clientrole = CR_DEFAULT;

        if(chan==0) return;
        else if(chan!=1 || getint(p)!=SV_CONNECT) disconnect_client(sender, DISC_TAGT);
        else
        {
            cl->acversion = getint(p);
            cl->acbuildtype = getint(p);
            s_sprintfd(tags)(", AC: %d|%x", cl->acversion, cl->acbuildtype);
            getstring(text, p);
            filtertext(text, text, 0, MAXNAMELEN);
            if(!text[0]) s_strcpy(text, "unarmed");
            s_strncpy(cl->name, text, MAXNAMELEN+1);

            getstring(text, p);
            s_strcpy(cl->pwd, text);
            getstring(text, p);
            filterlang(cl->lang, text);
            int wantrole = getint(p);
            cl->state.nextprimary = getint(p);
            bool banned = isbanned(sender);
            bool srvfull = numnonlocalclients() > scl.maxclients;
            bool srvprivate = mastermode == MM_PRIVATE;
            int bl = 0, wl = nickblacklist.checkwhitelist(*cl);
            if(wl == NWL_PASS) s_strcat(tags, ", nickname whitelist match");
            if(wl == NWL_UNLISTED) bl = nickblacklist.checkblacklist(cl->name);
            if(wl == NWL_IPFAIL || wl == NWL_PWDFAIL)
            { // nickname matches whitelist, but IP is not in the required range or PWD doesn't match
                logline(ACLOG_INFO, "[%s] '%s' matches nickname whitelist: wrong %s%s", cl->hostname, cl->name, wl == NWL_IPFAIL ? "IP" : "PWD", tags);
                disconnect_client(sender, DISC_BADNICK);
            }
            else if(bl > 0)
            { // nickname matches blacklist
                logline(ACLOG_INFO, "[%s] '%s' matches nickname blacklist line %d%s", cl->hostname, cl->name, bl, tags);
                disconnect_client(sender, DISC_BADNICK);
            }
            else if(passwords.check(cl->name, text, cl->salt, &pd) && (!pd.denyadmin || (banned && !srvfull && !srvprivate))) // pass admins always through
            { // admin (or deban) password match
                cl->isauthed = true;
                if(!pd.denyadmin && wantrole == CR_ADMIN) clientrole = CR_ADMIN;
                if(banned)
                {
                    loopv(bans) if(bans[i].address.host == cl->peer->address.host) { bans.remove(i); s_strcat(tags, ", ban removed"); break; } // remove admin bans
                }
                if(srvfull)
                {
                    loopv(clients) if(i != sender && clients[i]->type==ST_TCPIP)
                    {
                        disconnect_client(i, DISC_MAXCLIENTS); // disconnect someone else to fit maxclients again
                        break;
                    }
                }
                logline(ACLOG_INFO, "[%s] %s logged in using the admin password in line %d%s", cl->hostname, cl->name, pd.line, tags);
            }
            else if(scl.serverpassword[0] && !(srvprivate || srvfull || banned))
            { // server password required
                if(!strcmp(genpwdhash(cl->name, scl.serverpassword, cl->salt), text))
                {
                    cl->isauthed = true;
                    logline(ACLOG_INFO, "[%s] %s client logged in (using serverpassword)%s", cl->hostname, cl->name, tags);
                }
                else disconnect_client(sender, DISC_WRONGPW);
            }
            else if(srvprivate) disconnect_client(sender, DISC_MASTERMODE);
            else if(srvfull) disconnect_client(sender, DISC_MAXCLIENTS);
            else if(banned) disconnect_client(sender, DISC_BANREFUSE);
            else
            {
                cl->isauthed = true;
                logline(ACLOG_INFO, "[%s] %s logged in (default)%s", cl->hostname, cl->name, tags);
            }
        }
        if(!cl->isauthed) return;

        if(cl->type==ST_TCPIP)
        {
            loopv(clients) if(i != sender)
            {
                client *dup = clients[i];
                if(dup->type==ST_TCPIP && dup->peer->address.host==cl->peer->address.host && dup->peer->address.port==cl->peer->address.port)
                    disconnect_client(i, DISC_DUP);
            }
        }

        sendwelcome(cl);
        if(findscore(*cl, false)) sendresume(*cl, true);
        if(clientrole != CR_DEFAULT) changeclientrole(sender, clientrole, NULL, true);
    }

    if(packet->flags&ENET_PACKET_FLAG_RELIABLE) reliablemessages = true;

    #define QUEUE_MSG { if(cl->type==ST_TCPIP) while(curmsg<p.length()) cl->messages.add(p.buf[curmsg++]); }
    #define QUEUE_BUF(size, body) { \
        if(cl->type==ST_TCPIP) \
        { \
            curmsg = p.length(); \
            ucharbuf buf = cl->messages.reserve(size); \
            { body; } \
            cl->messages.addbuf(buf); \
        } \
    }
    #define QUEUE_INT(n) QUEUE_BUF(5, putint(buf, n))
    #define QUEUE_UINT(n) QUEUE_BUF(4, putuint(buf, n))
    #define QUEUE_STR(text) QUEUE_BUF(2*(int)strlen(text)+1, sendstring(text, buf))
    #define MSG_PACKET(packet) \
        ENetPacket *packet = enet_packet_create(NULL, 16 + p.length() - curmsg, ENET_PACKET_FLAG_RELIABLE); \
        ucharbuf buf(packet->data, packet->dataLength); \
        putint(buf, SV_CLIENT); \
        putint(buf, cl->clientnum); \
        putuint(buf, p.length() - curmsg); \
        buf.put(&p.buf[curmsg], p.length() - curmsg); \
        enet_packet_resize(packet, buf.length());

    int curmsg;
    while((curmsg = p.length()) < p.maxlen)
    {
        type = checktype(getint(p), cl);

        #ifdef _DEBUG
        if(type!=SV_POS && type!=SV_CLIENTPING && type!=SV_PING && type!=SV_CLIENT)
        {
            DEBUGVAR(cl->name);
            ASSERT(type>=0 && type<SV_NUM);
            DEBUGVAR(messagenames[type]);
            protocoldebug(true);
        }
        else protocoldebug(false);
        #endif

        switch(type)
        {
            case SV_TEAMTEXT:
                getstring(text, p);
                filtertext(text, text);
                if(!spamdetect(cl, text))
                {
                    logline(ACLOG_INFO, "[%s] %s says to team %s: '%s'", cl->hostname, cl->name, team_string(cl->team), text);
                    sendteamtext(text, sender);
                }
                else
                {
                    logline(ACLOG_INFO, "[%s] %s says to team %s: '%s', SPAM detected", cl->hostname, cl->name, team_string(cl->team), text);
                    sendservmsg("\f3please do not spam", sender);
                }
                break;

            case SV_TEXT:
            {
                int mid1 = curmsg, mid2 = p.length();
                getstring(text, p);
                filtertext(text, text);
                if(!spamdetect(cl, text))
                {
                    logline(ACLOG_INFO, "[%s] %s says: '%s'", cl->hostname, cl->name, text);
                    if(cl->type==ST_TCPIP) while(mid1<mid2) cl->messages.add(p.buf[mid1++]);
                    QUEUE_STR(text);
                }
                else
                {
                    logline(ACLOG_INFO, "[%s] %s says: '%s', SPAM detected", cl->hostname, cl->name, text);
                    sendservmsg("\f3please do not spam", sender);
                }
                break;
            }

            case SV_VOICECOM:
                getint(p);
                QUEUE_MSG;
                break;

            case SV_VOICECOMTEAM:
                sendvoicecomteam(getint(p), sender);
                break;

            case SV_INITC2S:
            {
                QUEUE_MSG;
                getstring(text, p);
                filtertext(text, text, 0, MAXNAMELEN);
                if(!text[0]) s_strcpy(text, "unarmed");
                QUEUE_STR(text);
                bool namechanged = strcmp(cl->name, text) != 0;
                if(namechanged) logline(ACLOG_INFO,"[%s] %s changed his name to %s", cl->hostname, cl->name, text);
                s_strncpy(cl->name, text, MAXNAMELEN+1);
                cl->team = getint(p);
                if(!team_isvalid(cl->team)) cl->team = TEAM_VOID;
                cl->skin = getint(p);
                QUEUE_MSG;
                if(namechanged)
                {
                    switch(nickblacklist.checkwhitelist(*cl))
                    {
                        case NWL_PWDFAIL:
                        case NWL_IPFAIL:
                            logline(ACLOG_INFO, "[%s] '%s' matches nickname whitelist: wrong IP/PWD", cl->hostname, cl->name);
                            disconnect_client(sender, DISC_BADNICK);
                            break;

                        case NWL_UNLISTED:
                        {
                            int l = nickblacklist.checkblacklist(cl->name);
                            if(l >= 0)
                            {
                                logline(ACLOG_INFO, "[%s] '%s' matches nickname blacklist line %d", cl->hostname, cl->name, l);
                                disconnect_client(sender, DISC_BADNICK);
                            }
                            break;
                        }
                    }
                }
                break;
            }

            case SV_MAPIDENT:
            {
                int gzs = getint(p);
                int rev = getint(p);
                if(!isdedicated || (smapstats.cgzsize == gzs && smapstats.hdr.maprevision == rev)) cl->isonrightmap = true;
                else
                {
                    forcedeath(cl);
                    logline(ACLOG_INFO, "[%s] %s is on the wrong map: revision %d/%d", cl->hostname, cl->name, rev, gzs);
                }
                QUEUE_MSG;
                break;
            }

            case SV_ITEMPICKUP:
            {
                int n = getint(p);
                if(!arenaround || arenaround - gamemillis > 2000)
                {
                    gameevent &pickup = cl->addevent();
                    pickup.type = GE_PICKUP;
                    pickup.pickup.ent = n;
                }
                else
                { // no nade pickup during last two seconds of lss intermission
                    if(sents.inrange(n) && sents[n].spawned)
                        sendf(sender, 1, "ri2", SV_ITEMSPAWN, n);
                }
                break;
            }

            case SV_WEAPCHANGE:
            {
                int gunselect = getint(p);
                if(gunselect<0 && gunselect>=NUMGUNS) break;
                cl->state.gunselect = gunselect;
                QUEUE_MSG;
                break;
            }

            case SV_PRIMARYWEAP:
            {
                int nextprimary = getint(p);
                if(nextprimary<0 && nextprimary>=NUMGUNS) break;
                cl->state.nextprimary = nextprimary;
                break;
            }

            case SV_CHANGETEAM:
                if(cl->state.state==CS_ALIVE) forcedeath(cl);
                break;

            case SV_TRYSPAWN:
                if(!cl->isonrightmap) sendservmsg("\f3you can't spawn until you download the map from the server; please type /getmap", sender);
                if(cl->state.state!=CS_DEAD || cl->state.lastspawn>=0 || !canspawn(cl) || !cl->isonrightmap) break;
                if(cl->state.lastdeath) cl->state.respawn();
                sendspawn(cl);
                break;

            case SV_SPAWN:
            {
                int ls = getint(p), gunselect = getint(p);
                if((cl->state.state!=CS_ALIVE && cl->state.state!=CS_DEAD) || ls!=cl->state.lifesequence || cl->state.lastspawn<0 || gunselect<0 || gunselect>=NUMGUNS) break;
                cl->state.lastspawn = -1;
                cl->state.state = CS_ALIVE;
                cl->state.gunselect = gunselect;
                QUEUE_BUF(5*(5 + 2*NUMGUNS),
                {
                    putint(buf, SV_SPAWN);
                    putint(buf, cl->state.lifesequence);
                    putint(buf, cl->state.health);
                    putint(buf, cl->state.armour);
                    putint(buf, cl->state.gunselect);
                    loopi(NUMGUNS) putint(buf, cl->state.ammo[i]);
                    loopi(NUMGUNS) putint(buf, cl->state.mag[i]);
                });
                break;
            }

            case SV_SUICIDE:
            {
                gameevent &suicide = cl->addevent();
                suicide.type = GE_SUICIDE;
                break;
            }

            case SV_SHOOT:
            {
                gameevent &shot = cl->addevent();
                shot.type = GE_SHOT;
                #define seteventmillis(event) \
                { \
                    event.id = getint(p); \
                    if(!cl->timesync || (cl->events.length()==1 && cl->state.waitexpired(gamemillis))) \
                    { \
                        cl->timesync = true; \
                        cl->gameoffset = gamemillis - event.id; \
                        event.millis = gamemillis; \
                    } \
                    else event.millis = cl->gameoffset + event.id; \
                }
                seteventmillis(shot.shot);
                shot.shot.gun = getint(p);
                loopk(3) shot.shot.from[k] = getint(p)/DMF;
                loopk(3) shot.shot.to[k] = getint(p)/DMF;
                int hits = getint(p);
                loopk(hits)
                {
                    gameevent &hit = cl->addevent();
                    hit.type = GE_HIT;
                    hit.hit.target = getint(p);
                    hit.hit.lifesequence = getint(p);
                    hit.hit.info = getint(p);
                    loopk(3) hit.hit.dir[k] = getint(p)/DNF;
                }
                break;
            }

            case SV_EXPLODE:
            {
                gameevent &exp = cl->addevent();
                exp.type = GE_EXPLODE;
                seteventmillis(exp.explode);
                exp.explode.gun = getint(p);
                exp.explode.id = getint(p);
                int hits = getint(p);
                loopk(hits)
                {
                    gameevent &hit = cl->addevent();
                    hit.type = GE_HIT;
                    hit.hit.target = getint(p);
                    hit.hit.lifesequence = getint(p);
                    hit.hit.dist = getint(p)/DMF;
                    loopk(3) hit.hit.dir[k] = getint(p)/DNF;
                }
                break;
            }

            case SV_AKIMBO:
            {
                gameevent &akimbo = cl->addevent();
                akimbo.type = GE_AKIMBO;
                seteventmillis(akimbo.akimbo);
                break;
            }

            case SV_RELOAD:
            {
                gameevent &reload = cl->addevent();
                reload.type = GE_RELOAD;
                seteventmillis(reload.reload);
                reload.reload.gun = getint(p);
                break;
            }

            case SV_SCOPE:
            {
                gameevent &scoping = cl->addevent();
                scoping.type = GE_SCOPING;
                seteventmillis(scoping.scoping);
                scoping.scoping.scoped = getint(p);
                break;
            }

            case SV_PING:
                sendf(sender, 1, "ii", SV_PONG, getint(p));
                break;

            case SV_CLIENTPING:
            {
                int ping = getint(p);
                if(cl) cl->ping = cl->ping == 9999 ? ping : (cl->ping * 4 + ping) / 5;
                QUEUE_MSG;
                break;
            }

            case SV_POS:
            {
                int cn = getint(p);
                if(cn!=sender)
                {
                    disconnect_client(sender, DISC_CN);
    #ifndef STANDALONE
                    conoutf("ERROR: invalid client (msg %i)", type);
    #endif
                    return;
                }
                loopi(3) clients[cn]->state.o[i] = getuint(p)/DMF;
                getuint(p);
                loopi(5) getint(p);
                getuint(p);
                if(!cl->isonrightmap) break;
                if(cl->type==ST_TCPIP && (cl->state.state==CS_ALIVE || cl->state.state==CS_EDITING))
                {
                    cl->position.setsizenodelete(0);
                    while(curmsg<p.length()) cl->position.add(p.buf[curmsg++]);
                }
                if(maplayout)
                {
                    vec &po = clients[cn]->state.o;
                    int ls = (1 << maplayout_factor) - 1;
                    if(po.x < 0 || po.y < 0 || po.x > ls || po.y > ls || maplayout[((int) po.x) + (((int) po.y) << maplayout_factor)] > po.z + 3)
                    {
                        if(gamemillis > 10000 && (servmillis - clients[cn]->connectmillis) > 10000) clients[cn]->mapcollisions++;    // assume map to be loaded after 10 seconds: fixme
                        if((clients[cn]->mapcollisions % 25) == 1)
                        {
                            logline(ACLOG_INFO, "[%s] %s collides with the map (%d)", clients[cn]->hostname, clients[cn]->name, clients[cn]->mapcollisions);
                        }
                    }
                }
                break;
            }

            case SV_SENDMAP:
            {
                getstring(text, p);
                filtertext(text, text);
                const char *sentmap = behindpath(text), *reject = NULL;
                int mapsize = getint(p);
                int cfgsize = getint(p);
                int cfgsizegz = getint(p);
                int revision = getint(p);
                if(p.remaining() < mapsize + cfgsizegz)
                {
                    p.forceoverread();
                    break;
                }
                int mp = findmappath(sentmap);
                if(readonlymap(mp))
                {
                    reject = "map is ro";
                    s_sprintfd(msg)("\f3map upload rejected: map %s is readonly", sentmap);
                    sendservmsg(msg, sender);
                }
                else if(mp == MAP_NOTFOUND && !strchr(scl.mapperm, 'c') && cl->role < CR_ADMIN) // default: only admins can create maps
                {
                    reject = "no permission for initial upload";
                    sendservmsg("\f3initial map upload rejected: you need to be admin", sender);
                }
                else if(mp == MAP_TEMP && revision >= mapbuffer.revision && !strchr(scl.mapperm, 'u') && cl->role < CR_ADMIN) // default: only admins can update maps
                {
                    reject = "no permission to update";
                    sendservmsg("\f3map update rejected: you need to be admin", sender);
                }
                else if(mp == MAP_TEMP && revision < mapbuffer.revision && !strchr(scl.mapperm, 'r') && cl->role < CR_ADMIN) // default: only admins can revert maps to older revisions
                {
                    reject = "no permission to revert revision";
                    sendservmsg("\f3map revert to older revision rejected: you need to be admin to upload an older map", sender);
                }
                else
                {
                    if(mapbuffer.sendmap(sentmap, mapsize, cfgsize, cfgsizegz, &p.buf[p.len]))
                    {
                        logline(ACLOG_INFO,"[%s] %s sent map %s, rev %d, %d + %d(%d) bytes written",
                                    clients[sender]->hostname, clients[sender]->name, sentmap, revision, mapsize, cfgsize, cfgsizegz);
                        s_sprintfd(msg)("%s (%d) up%sed map %s, rev %d%s", clients[sender]->name, sender, mp == MAP_NOTFOUND ? "load": "dat", sentmap, revision,
                            strcmp(sentmap, behindpath(smapname)) || smode == GMODE_COOPEDIT ? "" : " (restart game to use new map version)");
                        sendservmsg(msg);
                    }
                    else
                    {
                        reject = "write failed (no 'incoming'?)";
                        sendservmsg("\f3map upload failed", sender);
                    }
                }
                if (reject)
                {
                    logline(ACLOG_INFO,"[%s] %s sent map %s rev %d, rejected: %s",
                                clients[sender]->hostname, clients[sender]->name, sentmap, revision, reject);
                }
                p.len += mapsize + cfgsizegz;
                break;
            }

            case SV_RECVMAP:
            {
                ENetPacket *mappacket = mapbuffer.getmap();
                if(mappacket)
                {
                    resetflag(cl->clientnum); // drop ctf flag
                    savedscore *sc = findscore(*cl, true); // save score
                    if(sc) sc->save(cl->state);
                    sendpacket(cl->clientnum, 2, mappacket);
                    cl->mapchange();
                    sendwelcome(cl, 2, true); // resend state properly
                }
                else sendservmsg("no map to get", cl->clientnum);
                break;
            }

            case SV_REMOVEMAP:
            {
                getstring(text, p);
                filtertext(text, text);
                string filename;
                const char *rmmap = behindpath(text), *reject = NULL;
                int mp = findmappath(rmmap);
                int reqrole = strchr(scl.mapperm, 'D') ? CR_ADMIN : (strchr(scl.mapperm, 'd') ? CR_DEFAULT : CR_ADMIN + 100);
                if(cl->role < reqrole) reject = "no permission";
                else if(readonlymap(mp)) reject = "map is readonly";
                else if(mp == MAP_NOTFOUND) reject = "map not found";
                else
                {
                    s_sprintf(filename)(SERVERMAP_PATH_INCOMING "%s.cgz", rmmap);
                    remove(filename);
                    s_sprintf(filename)(SERVERMAP_PATH_INCOMING "%s.cfg", rmmap);
                    remove(filename);
                    s_sprintfd(msg)("map '%s' deleted", rmmap);
                    sendservmsg(msg, sender);
                    logline(ACLOG_INFO,"[%s] deleted map %s", clients[sender]->hostname, rmmap);
                }
                if (reject)
                {
                    logline(ACLOG_INFO,"[%s] deleting map %s failed: %s", clients[sender]->hostname, rmmap, reject);
                    s_sprintfd(msg)("\f3can't delete map '%s', %s", rmmap, reject);
                    sendservmsg(msg, sender);
                }
                break;
            }

            case SV_FLAGACTION:
            {
                int action = getint(p);
                int flag = getint(p);
                if(!m_flags || flag < 0 || flag > 1 || action < 0 || action > FA_NUM) break;
                flagaction(flag, action, sender);
                break;
            }

            case SV_SETADMIN:
            {
                bool claim = getint(p) != 0;
                getstring(text, p);
                changeclientrole(sender, claim ? CR_ADMIN : CR_DEFAULT, text);
                break;
            }

            case SV_CALLVOTE:
            {
                voteinfo *vi = new voteinfo;
                int type = getint(p);
                switch(type)
                {
                    case SA_MAP:
                    {
                        getstring(text, p);
                        filtertext(text, text);
                        int mode = getint(p);
                        if(mode==GMODE_DEMO) vi->action = new demoplayaction(text);
                        else vi->action = new mapaction(newstring(text), mode, sender);
                        break;
                    }
                    case SA_KICK:
                        vi->action = new kickaction(getint(p));
                        break;
                    case SA_BAN:
                        vi->action = new banaction(getint(p));
                        break;
                    case SA_REMBANS:
                        vi->action = new removebansaction();
                        break;
                    case SA_MASTERMODE:
                        vi->action = new mastermodeaction(getint(p));
                        break;
                    case SA_AUTOTEAM:
                        vi->action = new autoteamaction(getint(p) > 0);
                        break;
                    case SA_SHUFFLETEAMS:
                        vi->action = new shuffleteamaction();
                        break;
                    case SA_FORCETEAM:
                        vi->action = new forceteamaction(getint(p), sender);
                        break;
                    case SA_GIVEADMIN:
                        vi->action = new giveadminaction(getint(p));
                        break;
                    case SA_RECORDDEMO:
                        vi->action = new recorddemoaction(getint(p)!=0);
                        break;
                    case SA_STOPDEMO:
                        vi->action = new stopdemoaction();
                        break;
                    case SA_CLEARDEMOS:
                        vi->action = new cleardemosaction(getint(p));
                        break;
                    case SA_SERVERDESC:
                        getstring(text, p);
                        filtertext(text, text);
                        vi->action = new serverdescaction(newstring(text), sender);
                        break;
                }
                vi->owner = sender;
                vi->callmillis = servmillis;
                MSG_PACKET(msg);
                if(!scallvote(vi, msg)) delete vi;
                if(!msg->referenceCount) enet_packet_destroy(msg);
                break;
            }

            case SV_VOTE:
            {
                int n = getint(p);
                MSG_PACKET(msg);
                svote(sender, n, msg);
                if(valid_client(sender) && !msg->referenceCount) enet_packet_destroy(msg); // check sender existence first because he might have been disconnected due to a vote
                break;
            }

            case SV_WHOIS:
            {
                sendwhois(sender, getint(p));
                break;
            }

            case SV_LISTDEMOS:
                listdemos(sender);
                break;

            case SV_GETDEMO:
                senddemo(sender, getint(p));
                break;

            case SV_EXTENSION:
            {
                // AC server extensions
                //
                // rules:
                // 1. extensions MUST NOT modify gameplay or the beavior of the game in any way
                // 2. extensions may ONLY be used to extend or automate server administration tasks
                // 3. extensions may ONLY operate on the server and must not send any additional data to the connected clients
                // 4. extensions not adhering to these rules may cause the hosting server being banned from the masterserver
                //
                // also note that there is no guarantee that custom extensions will work in future AC versions


                getstring(text, p, 64);
                char *ext = text;   // extension specifier in the form of OWNER::EXTENSION, see sample below
                int n = getint(p);  // length of data after the specifier
                if(n > 50) return;

                // sample
                if(!strcmp(ext, "driAn::writelog"))
                {
                    // owner:       driAn - root@sprintf.org
                    // extension:   writelog - WriteLog v1.0
                    // description: writes a custom string to the server log
                    // access:      requires admin privileges
                    // usage:       /serverextension driAn::writelog "your log message here.."

                    getstring(text, p, n);
                    if(valid_client(sender) && clients[sender]->role==CR_ADMIN) logline(ACLOG_INFO, "%s", text);
                }
                // else if()

                // add other extensions here

                else for(; n > 0; n--) getint(p); // ignore unknown extensions

                break;
            }

            default:
            {
                int size = msgsizelookup(type);
                if(size==-1) { if(sender>=0) disconnect_client(sender, DISC_TAGT); return; }
                loopi(size-1) getint(p);
                QUEUE_MSG;
                break;
            }
        }
    }

    if(p.overread() && sender>=0) disconnect_client(sender, DISC_EOP);

    #ifdef _DEBUG
    protocoldebug(false);
    #endif
}

void localclienttoserver(int chan, ENetPacket *packet)
{
    process(packet, 0, chan);
}

client &addclient()
{
    client *c = NULL;
    loopv(clients) if(clients[i]->type==ST_EMPTY) { c = clients[i]; break; }
    if(!c)
    {
        c = new client;
        c->clientnum = clients.length();
        clients.add(c);
    }
    c->reset();
    return *c;
}

void checkintermission()
{
    if(minremain>0)
    {
        minremain = gamemillis>=gamelimit || forceintermission ? 0 : (gamelimit - gamemillis + 60000 - 1)/60000;
        sendf(-1, 1, "ri2", SV_TIMEUP, minremain);
    }
    if(!interm && minremain<=0) interm = gamemillis+10000;
    forceintermission = false;
}

void resetserverifempty()
{
    loopv(clients) if(clients[i]->type!=ST_EMPTY) return;
    resetserver("", 0, 10);
    mastermode = MM_OPEN;
    autoteam = true;
    nextmapname[0] = '\0';
}

void sendworldstate()
{
    static enet_uint32 lastsend = 0;
    if(clients.empty()) return;
    enet_uint32 curtime = enet_time_get()-lastsend;
    if(curtime<40) return;
    bool flush = buildworldstate();
    lastsend += curtime - (curtime%40);
    if(flush) enet_host_flush(serverhost);
    if(demorecord) recordpackets = true; // enable after 'old' worldstate is sent
}

void rereadcfgs(void)
{
    maprot.read();
    ipblacklist.read();
    nickblacklist.read();
    passwords.read();
}

void loggamestatus(const char *reason)
{
    int fragscore[2] = {0, 0}, flagscore[2] = {0, 0}, pnum[2] = {0, 0}, n;
    string text;
    s_sprintf(text)("%d minutes remaining", minremain);
    logline(ACLOG_INFO, "");
    logline(ACLOG_INFO, "Game status: %s on %s, %s, %s%c %s",
                      modestr(gamemode), smapname, reason ? reason : text, mmfullname(mastermode), custom_servdesc ? ',' : '\0', servdesc_current);
    logline(ACLOG_INFO, "cn name             %s%sfrag death %sping role    host", m_teammode ? "team " : "", m_flags ? "flag " : "", m_teammode ? "tk " : "");
    loopv(clients)
    {
        client &c = *clients[i];
        if(c.type == ST_EMPTY || !c.name[0]) continue;
        s_sprintf(text)("%2d %-16s ", c.clientnum, c.name);                 // cn name
        if(m_teammode) s_strcatf(text, "%-4s ", team_string(c.team, true)); // teamname (abbreviated)
        if(m_flags) s_strcatf(text, "%4d ", c.state.flagscore);             // flag
        s_strcatf(text, "%4d %5d", c.state.frags, c.state.deaths);          // frag death
        if(m_teammode) s_strcatf(text, " %2d", c.state.teamkills);          // tk
        logline(ACLOG_INFO, "%s%5d %s  %s", text, c.ping, c.role == CR_ADMIN ? "admin " : "normal", c.hostname);
        if(team_isactive(c.team))
        {
            flagscore[c.team] += c.state.flagscore;
            fragscore[c.team] += c.state.frags;
            pnum[c.team] += 1;
        }
    }
    if(m_teammode)
    {
        loopi(2) logline(ACLOG_INFO, "Team %4s:%3d players,%5d frags%c%5d flags", team_string(i), pnum[i], fragscore[i], m_flags ? ',' : '\0', flagscore[i]);
    }
    logline(ACLOG_INFO, "");
}

void serverslice(uint timeout)   // main server update, called from cube main loop in sp, or dedicated server loop
{
    static int msend = 0, mrec = 0, csend = 0, crec = 0, mnum = 0, cnum = 0;
#ifdef STANDALONE
    int nextmillis = (int)enet_time_get();
    if(svcctrl) svcctrl->keepalive();
#else
    int nextmillis = isdedicated ? (int)enet_time_get() : lastmillis;
#endif
    int diff = nextmillis - servmillis;
    gamemillis += diff;
    servmillis = nextmillis;

    if(m_demo) readdemo();

    if(minremain>0)
    {
        processevents();
        checkitemspawns(diff);
        bool ktfflagingame = false;
        if(m_flags) loopi(2)
        {
            sflaginfo &f = sflaginfos[i];
            if(f.state == CTFF_DROPPED && gamemillis-f.lastupdate > (m_ctf ? 30000 : 10000)) flagaction(i, FA_RESET, -1);
            if(m_htf && f.state == CTFF_INBASE && gamemillis-f.lastupdate > (smapstats.hasflags ? 10000 : 1000))
            {
                htf_forceflag(i);
            }
            if(m_ktf && f.state == CTFF_STOLEN && gamemillis-f.lastupdate > 15000)
            {
                flagaction(i, FA_SCORE, -1);
            }
            if(f.state == CTFF_INBASE || f.state == CTFF_STOLEN) ktfflagingame = true;
        }
        if(m_ktf && !ktfflagingame) flagaction(rnd(2), FA_RESET, -1); // ktf flag watchdog
        if(m_arena) arenacheck();
    }

    if(curvote)
    {
        if(!curvote->isalive()) curvote->evaluate(true);
        if(curvote->result!=VOTE_NEUTRAL) DELETEP(curvote);
    }

    int nonlocalclients = numnonlocalclients();

    if(forceintermission || ((smode>1 || (gamemode==0 && nonlocalclients)) && gamemillis-diff>0 && gamemillis/60000!=(gamemillis-diff)/60000))
        checkintermission();
    if(interm && gamemillis>interm)
    {
        loggamestatus("game finished");
        if(demorecord) enddemorecord();
        interm = 0;

        //start next game
        if(nextmapname[0]) startgame(nextmapname, nextgamemode);
        else maprot.next();
    }

    resetserverifempty();

    if(!isdedicated) return;     // below is network only

    serverms(smode, numclients(), minremain, smapname, servmillis, serverhost->address, &mnum, &msend, &mrec, &cnum, &csend, &crec, SERVER_PROTOCOL_VERSION);

    if(autoteam && m_teammode && !m_arena && !interm && servmillis - lastfillup > 5000 && refillteams()) lastfillup = servmillis;

    if(servmillis-laststatus>60*1000)   // display bandwidth stats, useful for server ops
    {
        laststatus = servmillis;
        rereadcfgs();
        if(nonlocalclients || bsend || brec)
        {
            if(nonlocalclients) loggamestatus(NULL);
            logline(ACLOG_INFO, "Status at %s: %d remote clients, %.1f send, %.1f rec (K/sec);"
                                         " Ping num: %d, %d send, %d rec; CSL num: %d, %d send, %d rec (bytes)",
                                          timestring(true, "%d-%m-%Y %H:%M:%S"), nonlocalclients, bsend/60.0f/1024, brec/60.0f/1024,
                                          mnum, msend, mrec, cnum, csend, crec);
            mnum = msend = mrec = cnum = csend = crec = 0;
        }
        bsend = brec = 0;
    }

    ENetEvent event;
    bool serviced = false;
    while(!serviced)
    {
        if(enet_host_check_events(serverhost, &event) <= 0)
        {
            if(enet_host_service(serverhost, &event, timeout) <= 0) break;
            serviced = true;
        }
        switch(event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                client &c = addclient();
                c.type = ST_TCPIP;
                c.peer = event.peer;
                c.peer->data = (void *)(size_t)c.clientnum;
                c.connectmillis = servmillis;
                c.salt = rand()*((servmillis%1000)+1);
                char hn[1024];
                s_strcpy(c.hostname, (enet_address_get_host_ip(&c.peer->address, hn, sizeof(hn))==0) ? hn : "unknown");
                logline(ACLOG_INFO,"[%s] client connected", c.hostname);
                sendinits2c(c);
                break;
            }

            case ENET_EVENT_TYPE_RECEIVE:
            {
                brec += (int)event.packet->dataLength;
                int cn = (int)(size_t)event.peer->data;
                if(valid_client(cn)) process(event.packet, cn, event.channelID);
                if(event.packet->referenceCount==0) enet_packet_destroy(event.packet);
                break;
            }

            case ENET_EVENT_TYPE_DISCONNECT:
            {
                int cn = (int)(size_t)event.peer->data;
                if(!valid_client(cn)) break;
                disconnect_client(cn);
                break;
            }

            default:
                break;
        }
    }
    sendworldstate();
}

void cleanupserver()
{
    if(serverhost) { enet_host_destroy(serverhost); serverhost = NULL; }
    if(svcctrl)
    {
        svcctrl->stop();
        DELETEP(svcctrl);
    }
    exitlogging();
}

int getpongflags(enet_uint32 ip)
{
    int flags = mastermode << PONGFLAG_MASTERMODE;
    flags |= scl.serverpassword[0] ? 1 << PONGFLAG_PASSWORD : 0;
    loopv(bans) if(bans[i].address.host == ip) { flags |= 1 << PONGFLAG_BANNED; break; }
    flags |= ipblacklist.check(ip) ? 1 << PONGFLAG_BLACKLIST : 0;
    return flags;
}

void extping_namelist(ucharbuf &p)
{
    loopv(clients)
    {
        if(clients[i]->type == ST_TCPIP && clients[i]->isauthed) sendstring(clients[i]->name, p);
    }
    sendstring("", p);
}

void extping_serverinfo(ucharbuf &pi, ucharbuf &po)
{
    char lang[3];
    lang[0] = tolower(getint(pi)); lang[1] = tolower(getint(pi)); lang[2] = '\0';
    const char *reslang = lang, *buf = infofiles.getinfo(lang); // try client language
    if(!buf) buf = infofiles.getinfo(reslang = "en");     // try english
    sendstring(buf ? reslang : "", po);
    if(buf)
    {
        for(const char *c = buf; *c && po.remaining() > MAXINFOLINELEN + 10; c += strlen(c) + 1) sendstring(c, po);
        sendstring("", po);
    }
}

void extping_maprot(ucharbuf &po)
{
    putint(po, CONFIG_MAXPAR);
    string text;
    bool abort = false;
    loopv(maprot.configsets)
    {
        if(po.remaining() < 100) abort = true;
        configset &c = maprot.configsets[i];
        filtertext(text, c.mapname, 0);
        text[30] = '\0';
        sendstring(abort ? "-- list truncated --" : text, po);
        loopi(CONFIG_MAXPAR) putint(po, c.par[i]);
        if(abort) break;
    }
    sendstring("", po);
}

void extinfo_cnbuf(ucharbuf &p, int cn)
{
    if(cn == -1) // add all available player ids
    {
        loopv(clients) if(clients[i]->type != ST_EMPTY)
            putint(p,clients[i]->clientnum);
    }
    else if(valid_client(cn)) // add single player only
    {
        putint(p,clients[cn]->clientnum);
    }
}

void extinfo_statsbuf(ucharbuf &p, int pid, int bpos, ENetSocket &pongsock, ENetAddress &addr, ENetBuffer &buf, int len, int *csend)
{
    loopv(clients)
    {
        if(clients[i]->type != ST_TCPIP) continue;
        if(pid>-1 && clients[i]->clientnum!=pid) continue;

        putint(p,EXT_PLAYERSTATS_RESP_STATS);  // send player stats following
        putint(p,clients[i]->clientnum);  //add player id
        putint(p,clients[i]->ping);             //Ping
        sendstring(clients[i]->name,p);         //Name
        sendstring(team_string(clients[i]->team),p); //Team
        putint(p,clients[i]->state.frags);      //Frags
        putint(p,clients[i]->state.flagscore);  //Flagscore
        putint(p,clients[i]->state.deaths);     //Death
        putint(p,clients[i]->state.teamkills);  //Teamkills
        putint(p,clients[i]->state.damage*100/max(clients[i]->state.shotdamage,1)); //Accuracy
        putint(p,clients[i]->state.health);     //Health
        putint(p,clients[i]->state.armour);     //Armour
        putint(p,clients[i]->state.gunselect);  //Gun selected
        putint(p,clients[i]->role);             //Role
        putint(p,clients[i]->state.state);      //State (Alive,Dead,Spawning,Lagged,Editing)
        uint ip = clients[i]->peer->address.host; // only 3 byte of the ip address (privacy protected)
        p.put((uchar*)&ip,3);

        buf.dataLength = len + p.length();
        enet_socket_send(pongsock, &addr, &buf, 1);
        *csend += (int)buf.dataLength;

        if(pid>-1) break;
        p.len=bpos;
    }
}

void extinfo_teamscorebuf(ucharbuf &p)
{
    putint(p, m_teammode ? EXT_ERROR_NONE : EXT_ERROR);
    putint(p, gamemode);
    putint(p, minremain);
    if(!m_teammode) return;

    int teamsizes[TEAM_NUM] = { 0 }, fragscores[TEAM_NUM] = { 0 }, flagscores[TEAM_NUM] = { 0 };
    loopv(clients) if(clients[i]->type!=ST_EMPTY && team_isvalid(clients[i]->team))
    {
        teamsizes[clients[i]->team] += 1;
        fragscores[clients[i]->team] += clients[i]->state.frags;
        flagscores[clients[i]->team] += clients[i]->state.flagscore;
    }

    loopi(TEAM_NUM) if(teamsizes[i])
    {
        sendstring(team_string(i), p); // team name
        putint(p, fragscores[i]); // add fragscore per team
        putint(p, m_flags ? flagscores[i] : -1); // add flagscore per team
        putint(p, -1); // ?
    }
}


#ifndef STANDALONE
void localdisconnect()
{
    loopv(clients) if(clients[i]->type==ST_LOCAL) clients[i]->zap();
}

void localconnect()
{
    modprotocol = false;
    client &c = addclient();
    c.type = ST_LOCAL;
    c.role = CR_ADMIN;
    s_strcpy(c.hostname, "local");
    sendinits2c(c);
}
#endif

void initserver(bool dedicated)
{
    srand(time(NULL));

    string identity;
    if(scl.logident[0]) filtertext(identity, scl.logident, 0);
    else s_sprintf(identity)("%s#%d", scl.ip[0] ? scl.ip : "local", scl.serverport);
    int conthres = scl.verbose > 1 ? ACLOG_DEBUG : (scl.verbose ? ACLOG_VERBOSE : ACLOG_INFO);
    if(dedicated && !initlogging(identity, scl.syslogfacility, conthres, scl.filethres, scl.syslogthres, scl.logtimestamp))
        printf("WARNING: logging not started!\n");
    logline(ACLOG_INFO, "logging local AssaultCube server (version %d, protocol %d/%d) now..", AC_VERSION, SERVER_PROTOCOL_VERSION, EXT_VERSION);

    s_strcpy(servdesc_current, scl.servdesc_full);
    servermsinit(scl.master ? scl.master : AC_MASTER_URI, scl.ip, CUBE_SERVINFO_PORT(scl.serverport), dedicated);

    if((isdedicated = dedicated))
    {
        ENetAddress address = { ENET_HOST_ANY, scl.serverport };
        if(scl.ip[0] && enet_address_set_host(&address, scl.ip)<0) logline(ACLOG_WARNING, "server ip not resolved!");
        serverhost = enet_host_create(&address, scl.maxclients+1, 0, scl.uprate);
        if(!serverhost) fatal("could not create server host");
        loopi(scl.maxclients) serverhost->peers[i].data = (void *)-1;

        maprot.init(scl.maprot);
        maprot.next(false, true); // ensure minimum maprot length of '1'
        passwords.init(scl.pwdfile, scl.adminpasswd);
        ipblacklist.init(scl.blfile);
        nickblacklist.init(scl.nbfile);
        infofiles.init(scl.infopath, scl.motdpath);
        infofiles.getinfo("en"); // cache 'en' serverinfo
        if(scl.demoeverymatch) logline(ACLOG_VERBOSE, "recording demo of every game (holding up to %d in memory)", scl.maxdemos);
        if(scl.demopath[0]) logline(ACLOG_VERBOSE,"all recorded demos will be written to: \"%s\"", scl.demopath);
        if(scl.voteperm[0]) logline(ACLOG_VERBOSE,"vote permission string: \"%s\"", scl.voteperm);
        if(scl.mapperm[0]) logline(ACLOG_VERBOSE,"map permission string: \"%s\"", scl.mapperm);
        logline(ACLOG_VERBOSE,"server description: \"%s\"", scl.servdesc_full);
        if(scl.servdesc_pre[0] || scl.servdesc_suf[0]) logline(ACLOG_VERBOSE,"custom server description: \"%sCUSTOMPART%s\"", scl.servdesc_pre, scl.servdesc_suf);
        logline(ACLOG_VERBOSE,"maxclients: %d, kick threshold: %d, ban threshold: %d", scl.maxclients, scl.kickthreshold, scl.banthreshold);
        if(scl.master) logline(ACLOG_VERBOSE,"master server URL: \"%s\"", scl.master);
        if(scl.serverpassword[0]) logline(ACLOG_VERBOSE,"server password: \"%s\"", hiddenpwd(scl.serverpassword));
    }

    resetserverifempty();

    if(isdedicated)       // do not return, this becomes main loop
    {
        #ifdef WIN32
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        #endif
        logline(ACLOG_INFO, "dedicated server started, waiting for clients...");
        logline(ACLOG_INFO, "Ctrl-C to exit");
        atexit(enet_deinitialize);
        atexit(cleanupserver);
        enet_time_set(0);
        for(;;) serverslice(5);
    }
}

#ifdef STANDALONE

void localservertoclient(int chan, uchar *buf, int len) {}
void fatal(const char *s, ...)
{
    s_sprintfdlv(msg,s,s);
    s_sprintfd(out)("AssaultCube fatal error: %s", msg);
    if (logline(ACLOG_ERROR, "%s", out));
    else puts(out);
    cleanupserver();
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    #ifdef WIN32
    //atexit((void (__cdecl *)(void))_CrtDumpMemoryLeaks);
    #ifndef _DEBUG
    #ifndef __GNUC__
    __try {
    #endif
    #endif
    #endif

    const char *service = NULL;

    for(int i = 1; i<argc; i++)
    {
        if(!scl.checkarg(argv[i]))
        {
            char *a = &argv[i][2];
            if(!scl.checkarg(argv[i]) && argv[i][0]=='-') switch(argv[i][1])
            {
                case '-':
                    if(!strncmp(argv[i], "--wizard", 8))
                    {
                        return wizardmain(argc-1, argv+1);
                    }
                    break;
                case 'S': service = a; break;
                default: printf("WARNING: unknown commandline option\n");
            }
            else printf("WARNING: unknown commandline argument\n");
        }
    }

    if(service && !svcctrl)
    {
        #ifdef WIN32
        svcctrl = new winservice(service);
        #endif
        if(svcctrl)
        {
            svcctrl->argc = argc; svcctrl->argv = argv;
            svcctrl->start();
        }
    }

    if(enet_initialize()<0) fatal("Unable to initialise network module");
    initserver(true);
    return EXIT_SUCCESS;

    #if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
    } __except(stackdumper(0, GetExceptionInformation()), EXCEPTION_CONTINUE_SEARCH) { return 0; }
    #endif
}
#endif

