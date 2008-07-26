// server.cpp: little more than enhanced multicaster
// runs dedicated or as client coroutine

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#define _dup    dup
#define _fileno fileno
#endif

#include "pch.h"
#include "cube.h" 
#include "servercontroller.h"

void resetmap(const char *newname, int newmode, int newtime = -1, bool notify = true);
void disconnect_client(int n, int reason = -1);

servercontroller *svcctrl = NULL;
struct log *logger = NULL;

#define valid_flag(f) (f >= 0 && f < 2)

static const int DEATHMILLIS = 300;

enum { GE_NONE = 0, GE_SHOT, GE_EXPLODE, GE_HIT, GE_AKIMBO, GE_RELOAD, GE_SUICIDE, GE_PICKUP };
enum { ST_EMPTY, ST_LOCAL, ST_TCPIP };

int mastermode = MM_OPEN;

struct shotevent
{
    int type;
    int millis, id;
    int gun;
    float from[3], to[3];
};

struct explodeevent
{
    int type;
    int millis, id;
    int gun;
};

struct hitevent
{
    int type;
    int target;
    int lifesequence;
    union
    {
        int info;
        float dist;
    };
    float dir[3];
};

struct suicideevent
{
    int type;
};

struct pickupevent
{
    int type;
    int ent;
};

struct akimboevent
{
    int type;
    int millis, id;
};

struct reloadevent
{
    int type;
    int millis, id;
    int gun;
};

union gameevent
{
    int type;
    shotevent shot;
    explodeevent explode;
    hitevent hit;
    suicideevent suicide;
    pickupevent pickup;
    akimboevent akimbo;
    reloadevent reload;
};

template <int N>
struct projectilestate
{
    int projs[N];
    int numprojs;

    projectilestate() : numprojs(0) {}

    void reset() { numprojs = 0; }

    void add(int val)
    {
        if(numprojs>=N) numprojs = 0;
        projs[numprojs++] = val;
    }

    bool remove(int val)
    {
        loopi(numprojs) if(projs[i]==val)
        {
            projs[i] = projs[--numprojs];
            return true;
        }
        return false;
    }
};

struct clientstate : playerstate
{
    vec o;
    int state;
    int lastdeath, lastspawn, lifesequence;
    int lastshot;
    projectilestate<8> grenades;
    int akimbos, akimbomillis;
    int flagscore, frags, teamkills, deaths, shotdamage, damage;

    clientstate() : state(CS_DEAD) {}

    bool isalive(int gamemillis)
    {
        return state==CS_ALIVE || (state==CS_DEAD && gamemillis - lastdeath <= DEATHMILLIS);
    }

    bool waitexpired(int gamemillis)
    {
        int wait = gamemillis - lastshot;
        loopi(NUMGUNS) if(wait < gunwait[i]) return false;
        return true;
    }

    void reset()
    {
        state = CS_DEAD;
        lifesequence = -1;
        grenades.reset();
        akimbos = 0;
        akimbomillis = 0;
        flagscore = frags = teamkills = deaths = shotdamage = damage = 0;
        respawn();
    }

    void respawn()
    {
        playerstate::respawn();
        o = vec(-1e10f, -1e10f, -1e10f);
        lastdeath = 0;
        lastspawn = -1;
        lastshot = 0;
        akimbos = 0;
        akimbomillis = 0;
    }
};

struct savedscore
{
    string name;
    uint ip;
    int frags, flagscore, deaths, teamkills, shotdamage, damage, lifesequence;

    void save(clientstate &cs)
    {
        frags = cs.frags;
        flagscore = cs.flagscore;
        deaths = cs.deaths;
        teamkills = cs.teamkills;
        shotdamage = cs.shotdamage;
        damage = cs.damage;
        lifesequence = cs.lifesequence;
    }

    void restore(clientstate &cs)
    {
        cs.frags = frags;
        cs.flagscore = flagscore;
        cs.deaths = deaths;
        cs.teamkills = teamkills;
        cs.shotdamage = shotdamage;
        cs.damage = damage;
        cs.lifesequence = lifesequence;
    }
};

static vector<savedscore> scores;

struct client                   // server side version of "dynent" type
{
    int type;
    int clientnum;
    ENetPeer *peer;
    string hostname;
    string name, team;
    int vote;
    int role;
    int connectmillis;
    bool isauthed; // for passworded servers
    bool timesync;
    int gameoffset, lastevent, lastvotecall;
    int demoflags;
    clientstate state;
    vector<gameevent> events;
    vector<uchar> position, messages;

    gameevent &addevent()
    {
        static gameevent dummy;
        if(events.length()>100) return dummy;
        return events.add();
    }

    void mapchange()
    {
        vote = VOTE_NEUTRAL;
        state.reset();
        events.setsizenodelete(0);
        timesync = false;
        lastevent = 0;
    }

    void reset()
    {
        name[0] = team[0] = demoflags = 0;
        position.setsizenodelete(0);
        messages.setsizenodelete(0);
        isauthed = false;
        role = CR_DEFAULT;
        lastvotecall = 0;
        mapchange();
    }

    void zap()
    {
        type = ST_EMPTY;
        role = CR_DEFAULT;
        isauthed = false;
    }
};

vector<client *> clients;

bool valid_client(int cn)
{
    return clients.inrange(cn) && clients[cn]->type != ST_EMPTY;
}

struct worldstate
{
    enet_uint32 uses;
    vector<uchar> positions, messages;
};

vector<worldstate *> worldstates;

void cleanworldstate(ENetPacket *packet)
{
   loopv(worldstates)
   {
       worldstate *ws = worldstates[i];
       if(packet->data >= ws->positions.getbuf() && packet->data <= &ws->positions.last()) ws->uses--;
       else if(packet->data >= ws->messages.getbuf() && packet->data <= &ws->messages.last()) ws->uses--;
       else continue;
       if(!ws->uses)
       {
           delete ws;
           worldstates.remove(i);
       }
       break;
   }
}

int bsend = 0, brec = 0, laststatus = 0, servmillis = 0;

void recordpacket(int chan, void *data, int len);

void sendpacket(int n, int chan, ENetPacket *packet, int exclude = -1)
{
    if(n<0)
    {
        recordpacket(chan, packet->data, (int)packet->dataLength);
        loopv(clients) if(i!=exclude) sendpacket(i, chan, packet);
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
        if(c.type!=ST_TCPIP) continue;
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
    if(psize) recordpacket(0, ws.positions.getbuf(), psize);
    if(msize) recordpacket(1, ws.messages.getbuf(), msize);
    loopi(psize) { uchar c = ws.positions[i]; ws.positions.add(c); }
    loopi(msize) { uchar c = ws.messages[i]; ws.messages.add(c); }
    ws.uses = 0;
    loopv(clients)
    {
        client &c = *clients[i];
        if(c.type!=ST_TCPIP) continue;
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

int maxclients = DEFAULTCLIENTS, scorethreshold = -5;
string smapname;

const char *adminpasswd = NULL, *motd = NULL;

int countclients(int type, bool exclude = false)
{
    int num = 0;
    loopv(clients) if((clients[i]->type!=type)==exclude) num++;
    return num;
}

int numclients() { return countclients(ST_EMPTY, true); }
int numlocalclients() { return countclients(ST_LOCAL); }
int numnonlocalclients() { return countclients(ST_TCPIP); }

int freeteam()
{
	int teamsize[2] = {0, 0};
	loopv(clients) if(clients[i]->type!=ST_EMPTY) teamsize[team_int(clients[i]->team)]++;
	if(teamsize[0] == teamsize[1]) return rnd(2);
	return teamsize[0] < teamsize[1] ? 0 : 1;
}

savedscore *findscore(client &c, bool insert)
{
    if(c.type!=ST_TCPIP) return NULL;
    if(!insert) loopv(clients)
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
    loopv(scores)
    {
        savedscore &sc = scores[i];
        if(!strcmp(sc.name, c.name) && sc.ip==c.peer->address.host) return &sc;
    }
    if(!insert) return NULL;
    savedscore &sc = scores.add();
    s_strcpy(sc.name, c.name);
    sc.ip = c.peer->address.host;
    return &sc;
}

struct server_entity            // server side version of "entity" type
{
    int type;
    bool spawned;
    int spawntime;
};

vector<server_entity> sents;

bool notgotitems = true;        // true when map has changed and waiting for clients to send item

// allows the gamemode macros to work with the server mode
#define gamemode smode
int smode = 0;

void restoreserverstate(vector<entity> &ents)   // hack: called from savegame code, only works in SP
{
    loopv(sents)
    {
        sents[i].spawned = ents[i].spawned;
        sents[i].spawntime = 0;
    } 
}

static int interm = 0, minremain = 0, gamemillis = 0, gamelimit = 0;
static bool mapreload = false, autoteam = true;

static string serverpassword = "";

bool isdedicated;
ENetHost *serverhost = NULL;

void process(ENetPacket *packet, int sender, int chan);
void welcomepacket(ucharbuf &p, int n);

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
    sendf(c->clientnum, 1, "ri6vv", SV_SPAWNSTATE, gs.lifesequence,
        gs.health, gs.armour,
        gs.primary, gs.gunselect,
        NUMGUNS, gs.ammo, NUMGUNS, gs.mag);
    gs.lastspawn = gamemillis;
}

// demo

struct demofile
{
    string info;
    uchar *data;
    int len;
};

#define MAXDEMOS 5
vector<demofile> demos;

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
    if(demos.length()>=MAXDEMOS)
    {
        delete[] demos[0].data;
        demos.remove(0);
    }
    demofile &d = demos.add();
    time_t t = time(NULL);
    char *timestr = ctime(&t), *trim = timestr + strlen(timestr);
    while(trim>timestr && isspace(*--trim)) *trim = '\0';
    s_sprintf(d.info)("%s: %s, %s, %.2f%s", timestr, modestr(gamemode), smapname, len > 1024*1024 ? len/(1024*1024.f) : len/1024.0f, len > 1024*1024 ? "MB" : "kB");
    s_sprintfd(msg)("Demo \"%s\" recorded\nPress F10 to download it from the server..", d.info);
    sendservmsg(msg);
    d.data = new uchar[len];
    d.len = len;
    fread(d.data, 1, len, demotmp);
    fclose(demotmp);
    demotmp = NULL;
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

    demorecord = f;
    recordpackets = false;

    demoheader hdr;
    memcpy(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic));
    hdr.version = DEMO_VERSION;
    hdr.protocol = PROTOCOL_VERSION;
    endianswap(&hdr.version, sizeof(int), 1);
    endianswap(&hdr.protocol, sizeof(int), 1);
    gzwrite(demorecord, &hdr, sizeof(demoheader));

    uchar buf[MAXTRANS];
    ucharbuf p(buf, sizeof(buf));
    welcomepacket(p, -1);
    writedemo(1, buf, p.len);

    loopv(clients)
    {
        client *ci = clients[i];
        if(ci->type==ST_EMPTY) continue;

        uchar header[16];
        ucharbuf q(&buf[sizeof(header)], sizeof(buf)-sizeof(header));
        putint(q, SV_INITC2S);
        sendstring(ci->name, q);
        sendstring(ci->team, q);

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
    putint(p, demos.length());
    loopv(demos) sendstring(demos[i].info, p);
    enet_packet_resize(packet, p.length());
    sendpacket(cn, 1, packet);
    if(!packet->referenceCount) enet_packet_destroy(packet);
}

static void cleardemos(int n)
{
    if(!n)
    {
        loopv(demos) delete[] demos[i].data;
        demos.setsize(0);
        sendservmsg("cleared all demos");
    }
    else if(demos.inrange(n-1))
    {
        delete[] demos[n-1].data;
        demos.remove(n-1);
        s_sprintfd(msg)("cleared demo %d", n);
        sendservmsg(msg);
    }
}

void senddemo(int cn, int num)
{
    if(!num) num = demos.length();
    if(!demos.inrange(num-1)) return;
    demofile &d = demos[num-1];
    sendf(cn, 2, "rim", SV_SENDDEMO, d.len, d.data); 
}

void enddemoplayback()
{
    if(!demoplayback) return;
    gzclose(demoplayback);
    demoplayback = NULL;

    sendf(-1, 1, "rii", SV_DEMOPLAYBACK, 0);

    sendservmsg("demo playback finished");
    
    loopv(clients)
    {
        ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        ucharbuf p(packet->data, packet->dataLength);
        welcomepacket(p, clients[i]->clientnum);
        enet_packet_resize(packet, p.length());
        sendpacket(clients[i]->clientnum, 1, packet);
        if(!packet->referenceCount) enet_packet_destroy(packet);
    }
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
        else if(hdr.protocol!=PROTOCOL_VERSION) s_sprintf(msg)("demo \"%s\" requires an %s version of AssaultCube", file, hdr.protocol<PROTOCOL_VERSION ? "older" : "newer");
    }
    if(msg[0])
    {
        if(demoplayback) { gzclose(demoplayback); demoplayback = NULL; }
        sendservmsg(msg);
        return;
    }

    s_sprintf(msg)("playing demo \"%s\"", file);
    sendservmsg(msg);

    sendf(-1, 1, "rii", SV_DEMOPLAYBACK, 1);

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
} sflaginfos[2];

void sendflaginfo(int flag, int action, int cn = -1)
{
    sflaginfo &f = sflaginfos[flag];
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_FLAGINFO);
    putint(p, flag);
    putint(p, f.state);
    putint(p, action);
    if(f.state==CTFF_STOLEN || action==SV_FLAGRETURN) putint(p, f.actor_cn);
    else if(f.state==CTFF_DROPPED) loopi(3) putint(p, int(f.pos[i]*DMF));
    enet_packet_resize(packet, p.length());
    sendpacket(cn, 1, packet);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
}

void flagaction(int flag, int action, int sender)
{
    if(!valid_flag(flag)) return;
	sflaginfo &f = sflaginfos[flag];

	switch(action)
	{
		case SV_FLAGPICKUP:
		{
			if(f.state == CTFF_STOLEN) return;
			f.state = CTFF_STOLEN;
			f.actor_cn = sender;
            logger->writeline(log::info,"[%s] %s stole the flag", clients[sender]->hostname, clients[sender]->name);
			break;
		}
		case SV_FLAGDROP:
		{
			if(f.state!=CTFF_STOLEN || (sender != -1 && f.actor_cn != sender)) return;
            f.state = CTFF_DROPPED;
            loopi(3) f.pos[i] = clients[sender]->state.o[i];
            logger->writeline(log::info,"[%s] %s dropped the flag", clients[sender]->hostname, clients[sender]->name);
            break;
		}
		case SV_FLAGRETURN:
		{
            if(f.state!=CTFF_DROPPED) return;
            f.state = CTFF_INBASE;
            f.actor_cn = sender;
            logger->writeline(log::info,"[%s] %s returned the flag", clients[sender]->hostname, clients[sender]->name);
			break;
		}
		case SV_FLAGRESET:
		{
			if(sender != -1 && f.actor_cn != sender) return;
			f.state = CTFF_INBASE;
            logger->writeline(log::info,"The server reset the flag");
			break;
		}
		case SV_FLAGSCORE:
		{
			if(f.state != CTFF_STOLEN) return;
			f.state = CTFF_INBASE;
            logger->writeline(log::info, "[%s] %s scored with the flag for %s", clients[sender]->hostname, clients[sender]->name, clients[sender]->team);
			break;
		}
		default: return;
	}

	f.lastupdate = gamemillis;
	sendflaginfo(flag, action);
}

void ctfreset()
{
    loopi(2) 
    {
        sflaginfos[i].actor_cn = 0;
        sflaginfos[i].state = CTFF_INBASE;
        sflaginfos[i].lastupdate = -1;
    }
}

bool canspawn(client *c, bool connecting = false)
{
    if(m_arena)
    {
        if(connecting && numnonlocalclients()<=2) return true;
        return false;
    }
    return true;
}

int arenaround = 0;

void arenacheck()
{
    if(!m_arena || interm || gamemillis<arenaround || clients.empty()) return;

    if(arenaround)
    {
        arenaround = 0;
        loopv(clients) if(clients[i]->type!=ST_EMPTY) 
        {
            clients[i]->state.respawn();
            sendspawn(clients[i]);
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
    loopv(clients)
    {
        client &c = *clients[i];
        if(c.type==ST_EMPTY) continue;
        if(c.state.state==CS_ALIVE || (c.state.state==CS_DEAD && c.state.lastspawn>=0))
        {
            if(!alive) alive = &c;
            else if(!m_teammode || strcmp(alive->team, c.team)) return;
        }
        else if(c.state.state==CS_DEAD) dead = true;
    }
    if(!dead) return;
    sendf(-1, 1, "ri2", SV_ARENAWIN, !alive ? -1 : alive->clientnum);
    arenaround = gamemillis+5000;
}

void sendteamtext(char *text, int sender)
{
    if(!m_teammode || !clients[sender]->team[0]) return;
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_TEAMTEXT);
    putint(p, sender);
    sendstring(text, p);
    enet_packet_resize(packet, p.length());
    loopv(clients) if(i!=sender && !strcmp(clients[i]->team, clients[sender]->team)) sendpacket(i, 1, packet);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
}

void sendvoicecomteam(int sound, int sender)
{
    if(!m_teammode || !clients[sender]->team[0]) return;
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_VOICECOMTEAM);
    putint(p, sender);
    putint(p, sound);
    enet_packet_resize(packet, p.length());
    loopv(clients) if(i!=sender && !strcmp(clients[i]->team, clients[sender]->team)) sendpacket(i, 1, packet);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
}

void resetitems() { sents.setsize(0); notgotitems = true; }

int spawntime(int type)
{
    int np = 0;
    loopv(clients) if(clients[i]) np++;
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
    if(!sents.inrange(i)) return false;
    server_entity &e = sents[i];
    if(!e.spawned) return false;
    if(sender>=0)
    {
        client *cl = clients[sender];
        if(cl->type==ST_TCPIP)
        {
            if(cl->state.state!=CS_ALIVE || !cl->state.canpickup(e.type)) return false;
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
    actor->state.damage += damage;
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
        target->state.deaths++;
        if(target!=actor)
        {
            if (!isteam(target->team, actor->team)) actor->state.frags += gib ? 2 : 1;
            else 
            { 
                actor->state.frags--; 
                actor->state.teamkills++; 
            }
        }
        else actor->state.frags--;
        sendf(-1, 1, "ri4", gib ? SV_GIBDIED : SV_DIED, target->clientnum, actor->clientnum, actor->state.frags);
        target->position.setsizenodelete(0);
        ts.state = CS_DEAD;
        ts.lastdeath = gamemillis;
        logger->writeline(log::info, "[%s] %s %s %s", actor->hostname, actor->name, gib ? "gibbed" : "fragged", target->name);
        // don't issue respawn yet until DEATHMILLIS has elapsed
        // ts.respawn();

        if(actor->state.frags < scorethreshold) disconnect_client(actor->clientnum, DISC_AUTOKICK);
    }
}

#include "serverevents.h"

struct configset
{
    string mapname;
    int mode;
    int time;
    bool vote;
};

vector<configset> configsets;
int curcfgset = -1;

void readscfg(char *cfg)
{
    configsets.setsize(0);

    string s;
    s_strcpy(s, cfg);
    char *buf = loadfile(path(s), NULL);
    if(!buf) return;
    char *p, *l;
    
    p = buf;
    while((p = strstr(p, "//")) != NULL) // remove comments
        while(p[0] != '\n' && p[0] != '\0') p++[0] = ' ';
    
    l = buf;
    bool lastline = false;
    while((p = strstr(l, "\n")) != NULL || (l[0] && (lastline=true))) // remove empty/invalid lines
    {
        size_t len = lastline ? strlen(l) : p-l;
        string line;
        s_strncpy(line, l, len+1);
        char *d = line;
        int n = 0;
		while((p = strstr(d, ":")) != NULL) { d = p+1; n++; }
        if(n!=3) memset(l, ' ', len+1);
        if(lastline) { l[len+1] = 0; break; }
        l += len+1;
    }
         
    configset c;
    int argc = 0;
    string argv[4];

    p = strtok(buf, ":\n\0");
    while(p != NULL)
    {
        strcpy(argv[argc], p);
        if(++argc==4)
        {
            int numspaces;
            for(numspaces = 0; argv[0][numspaces]==' '; numspaces++){} // ignore space crap
            strcpy(c.mapname, argv[0]+numspaces);
            c.mode = atoi(argv[1]);
            c.time = atoi(argv[2]);
            c.vote = atoi(argv[3]) > 0;
            configsets.add(c);
            argc = 0;
        }
        p = strtok(NULL, ":\n\0");
    }
}

void resetvotes()
{
    loopv(clients) clients[i]->vote = VOTE_NEUTRAL;
}

void forceteam(int client, int team, bool respawn)
{
    if(!valid_client(client) || team < 0 || team > 1) return;
    sendf(client, 1, "riii", SV_FORCETEAM, team, respawn ? 1 : 0);
}

void shuffleteams(bool respawn = true)
{
	int numplayers = numclients();
	int teamsize[2] = {0, 0};
	loopv(clients) if(clients[i]->type!=ST_EMPTY)
	{
		int team = rnd(2);
		if(teamsize[team] >= numplayers/2) team = team_opposite(team);
		forceteam(i, team, respawn);
		teamsize[team]++;
	}
}

bool mapavailable(const char *mapname);

void resetmap(const char *newname, int newmode, int newtime, bool notify)
{
    if(m_demo) enddemoplayback();
    else enddemorecord();

	bool lastteammode = m_teammode;
    smode = newmode;
    s_strcpy(smapname, newname);

    minremain = newtime >= 0 ? newtime : (m_teammode ? 15 : 10);
    gamemillis = 0;
    gamelimit = minremain*60000;

    mapreload = false;
    interm = 0;
    laststatus = servmillis-61*1000;
    resetvotes();
    resetitems();
    scores.setsize(0);
    ctfreset();
    if(notify) 
    {
        // change map
        sendf(-1, 1, "risii", SV_MAPCHANGE, smapname, smode, mapavailable(smapname) ? 1 : 0);
        if(smode>1 || (smode==0 && numnonlocalclients()>0)) sendf(-1, 1, "ri2", SV_TIMEUP, minremain);
    }
    if(m_arena) arenaround = 0;
    if(notify)
    {
        // shuffle if previous mode wasn't a team-mode
        if(m_teammode && !lastteammode) shuffleteams(false); 
        // send spawns
        loopv(clients) if(clients[i]->type!=ST_EMPTY)
        {
            client *c = clients[i];
            c->mapchange();
            if(m_mp(smode)) sendspawn(c);
        }
    }
    if(m_demo) setupdemoplayback();
    else if(demonextmatch)
    {
        demonextmatch = false;
        setupdemorecord();
    }
}

void nextcfgset(bool notify = true) // load next maprotation set
{   
    curcfgset++;
    if(curcfgset>=configsets.length() || curcfgset<0) curcfgset=0;
    
    configset &c = configsets[curcfgset];
    resetmap(c.mapname, c.mode, c.time, notify);
}

struct ban
{
	ENetAddress address;
	int millis;
};

vector<ban> bans;

bool isbanned(int cn)
{
	if(!valid_client(cn)) return false;
	client &c = *clients[cn];
	loopv(bans)
	{
		ban &b = bans[i];
		if(b.millis < servmillis) { bans.remove(i--); }
		if(b.address.host == c.peer->address.host) { return true; }
	}
	return false;
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

void changeclientrole(int client, int role, char *pwd = NULL, bool force=false)
{
    if(!isdedicated || !valid_client(client)) return;
    if(force || role == CR_DEFAULT || (role == CR_ADMIN && pwd && pwd[0] && adminpasswd && !strcmp(adminpasswd, pwd)))
    {
        if(role == clients[client]->role) return;
        if(role > CR_DEFAULT) loopv(clients) clients[i]->role = CR_DEFAULT;
        clients[client]->role = role;
        sendserveropinfo(-1);
        logger->writeline(log::info,"[%s] set role of player %s to %s", clients[client]->hostname, clients[client]->name[0] ? clients[client]->name : "[unnamed]", role == CR_ADMIN ? "admin" : "normal player"); // flowtron : connecting players haven't got a name yet (connectadmin) 
    }
    else if(pwd && pwd[0]) disconnect_client(client, DISC_SOPLOGINFAIL); // avoid brute-force
}

#include "serveractions.h"

struct voteinfo
{
    int owner, callmillis, result;
    serveraction *action;
    
    voteinfo() : owner(0), callmillis(0), result(VOTE_NEUTRAL), action(NULL) {}

    void end(int result) 
    { 
        resetvotes();
        sendf(-1, 1, "ri2", SV_VOTERESULT, result);
        if(result == VOTE_YES && action)
        {
            if(demorecord) enddemorecord();
            action->perform(); 
        }
        this->result = result;
    }

    bool isvalid() { return valid_client(owner) && action != NULL && action->isvalid(); }
    bool isalive() { return servmillis < callmillis+40*1000; }
    
    void evaluate(bool forceend = false)
    {
        int stats[VOTE_NUM] = {0};
        loopv(clients) 
            if(clients[i]->type!=ST_EMPTY && clients[i]->connectmillis < callmillis) 
            { 
                stats[clients[i]->vote]++; 
            };
        
        bool admin = clients[owner]->role==CR_ADMIN || (!isdedicated && clients[owner]->type==ST_LOCAL);
        int total = stats[VOTE_NO]+stats[VOTE_YES]+stats[VOTE_NEUTRAL];
        const float requiredcount = 0.51f;
        if(stats[VOTE_YES]/(float)total > requiredcount || admin) 
            end(VOTE_YES);
        else if(forceend || stats[VOTE_NO]/(float)total > requiredcount || stats[VOTE_NO] >= stats[VOTE_YES]+stats[VOTE_NEUTRAL]) 
            end(VOTE_NO);
        else return;
    }
};

static voteinfo *curvote = NULL;

bool vote(int sender, int vote) // true if the vote was placed successfully
{
    if(!curvote || !valid_client(sender) || vote < VOTE_YES || vote > VOTE_NO) return false;
    if(clients[sender]->vote != VOTE_NEUTRAL)
    {
        sendf(sender, 1, "ri2", SV_CALLVOTEERR, VOTEE_MUL);
        return false;
    }
    else
    {
        clients[sender]->vote = vote;
        curvote->evaluate();
        return true;
    }
}

void callvotesuc(voteinfo *v)
{
    if(!v->isvalid()) return;
    curvote = v;
    clients[v->owner]->lastvotecall = servmillis;
    
    sendf(v->owner, 1, "ri", SV_CALLVOTESUC);
    logger->writeline(log::info, "[%s] client %s called a vote: %s", clients[v->owner]->hostname, clients[v->owner]->name, v->action->desc ? v->action->desc : "[unknown]");
}

void callvoteerr(voteinfo *v, int error)
{
    if(!v->isvalid()) return;
    sendf(v->owner, 1, "ri2", SV_CALLVOTEERR, error);
    logger->writeline(log::info, "[%s] client %s failed to call a vote: %s (%s)", clients[v->owner]->hostname, clients[v->owner]->name, v->action->desc ? v->action->desc : "[unknown]", voteerrorstr(error));
}

bool callvote(voteinfo *v) // true if a regular vote was called
{
    int area = isdedicated ? EE_DED_SERV : EE_LOCAL_SERV;
    int error = -1;

    if(!v || !v->isvalid()) error = VOTEE_INVALID;
    else if(v->action->role > clients[v->owner]->role) error = VOTEE_PERMISSION;
    else if(!(area & v->action->area)) error = VOTEE_AREA;
    else if(curvote) error = VOTEE_CUR;
    else if(configsets.length() && curcfgset < configsets.length() && !configsets[curcfgset].vote && clients[v->owner]->role == CR_DEFAULT) 
        error = VOTEE_DISABLED;
    else if(clients[v->owner]->lastvotecall && servmillis - clients[v->owner]->lastvotecall < 60*1000 && clients[v->owner]->role != CR_ADMIN && numclients()>1) 
        error = VOTEE_MAX;

    if(error >= 0) 
    { 
        callvoteerr(v, error); 
        return false; 
    }
    else
    {
        callvotesuc(v);
        return true;
    }
}

const char *disc_reason(int reason)
{
    static const char *disc_reasons[] = { "normal", "end of packet", "client num", "kicked by server operator", "banned by server operator", "tag type", "connection refused due to ban", "wrong password", "failed admin login", "server FULL - maxclients", "server mastermode is \"private\"", "auto kick - did your score drop below the threshold?" };
    return reason >= 0 && (size_t)reason < sizeof(disc_reasons)/sizeof(disc_reasons[0]) ? disc_reasons[reason] : "unknown";
}

void disconnect_client(int n, int reason)
{
    if(!clients.inrange(n) || clients[n]->type!=ST_TCPIP) return;
    if(m_ctf) loopi(2) if(sflaginfos[i].state==CTFF_STOLEN && sflaginfos[i].actor_cn==n) flagaction(i, SV_FLAGDROP, n);
    client &c = *clients[n];
    savedscore *sc = findscore(c, true);
    if(sc) sc->save(c.state);
    if(reason>=0) logger->writeline(log::info, "[%s] disconnecting client %s (%s)", c.hostname, c.name, disc_reason(reason));
    else logger->writeline(log::info, "[%s] disconnected client %s", c.hostname, c.name);
    c.peer->data = (void *)-1;
    if(reason>=0) enet_peer_disconnect(c.peer, reason);
	clients[n]->zap();
    sendf(-1, 1, "rii", SV_CDIS, n);
    if(curvote) curvote->evaluate();
}

void sendwhois(int sender, int cn)
{
    if(!valid_client(sender) || !valid_client(cn)) return;
    if(clients[cn]->type == ST_TCPIP && clients[cn]->peer)
    {        
        uint ip = clients[cn]->peer->address.host;
        if(clients[sender]->role != CR_ADMIN) ip &= 0xFFFF; // only admin gets full IP
        sendf(sender, 1, "ri3", SV_WHOISINFO, cn, ip);
    }
}

// sending of maps between clients

string copyname; 
int copysize, copymapsize, copycfgsize;
uchar *copydata = NULL;

bool mapavailable(const char *mapname) { return !strcmp(copyname, mapname); }

void sendmapserv(int n, string mapname, int mapsize, int cfgsize, uchar *data)
{   
    if(!mapname[0] || mapsize <= 0 || mapsize + cfgsize > MAXMAPSENDSIZE) return;
    s_strcpy(copyname, mapname);
    copymapsize = mapsize;
    copycfgsize = cfgsize;
    copysize = mapsize + cfgsize;
    DELETEA(copydata);
    copydata = new uchar[copysize];
    memcpy(copydata, data, copysize);
}

ENetPacket *getmapserv(int n)
{
    if(!copydata) return NULL;
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS + copysize, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_RECVMAP);
    sendstring(copyname, p);
    putint(p, copymapsize);
    putint(p, copycfgsize);
    p.put(copydata, copysize);
    enet_packet_resize(packet, p.length());
    return packet;
}

void welcomepacket(ucharbuf &p, int n)
{
    putint(p, SV_INITS2C);
    putint(p, n);
    putint(p, PROTOCOL_VERSION);
    if(!smapname[0] && configsets.length()) nextcfgset(false);
    int numcl = numclients();
    putint(p, smapname[0] && !m_demo ? numcl : -1);
    putint(p, serverpassword[0] ? 1 : 0);
    if(smapname[0])
    {
        putint(p, SV_MAPCHANGE);
        sendstring(smapname, p);
        putint(p, smode);
        putint(p, mapavailable(smapname) ? 1 : 0);
        if(smode>1 || (smode==0 && numnonlocalclients()>0))
        {
            putint(p, SV_TIMEUP);
            putint(p, minremain);
        }
        if(numcl>1 || m_demo)
        {
            putint(p, SV_ITEMLIST);
            loopv(sents) if(sents[i].spawned)
            {
                putint(p, i);
                putint(p, sents[i].type);
            }
            putint(p, -1);
        }
    }
    client *c = valid_client(n) ? clients[n] : NULL;
    if(c && c->type == ST_TCPIP && serveroperator() != -1) sendserveropinfo(n);
    if(autoteam && numcl>1)
    {
        putint(p, SV_FORCETEAM);
        putint(p, freeteam());
        putint(p, 0);
    }
    if(c && (m_demo || m_mp(smode)))
    {
        if(!canspawn(c, true))
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
            loopi(NUMGUNS) putint(p, gs.ammo[i]);
            loopi(NUMGUNS) putint(p, gs.mag[i]);
            gs.lastspawn = gamemillis;
        }
    }     
    if(clients.length()>1)
    {
        putint(p, SV_RESUME);
        loopv(clients)
        {
            client &c = *clients[i];
            if(c.type!=ST_TCPIP || c.clientnum==n) continue;
            putint(p, c.clientnum);
            putint(p, c.state.state);
            putint(p, c.state.lifesequence);
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
    if(motd)
    {
        putint(p, SV_TEXT);
        sendstring(motd, p);
    }
}

int checktype(int type, client *cl)
{
    if(cl && cl->type==ST_LOCAL) return type;
    // only allow edit messages in coop-edit mode
    static int edittypes[] = { SV_EDITENT, SV_EDITH, SV_EDITT, SV_EDITS, SV_EDITD, SV_EDITE };
    if(cl && smode!=1) loopi(sizeof(edittypes)/sizeof(int)) if(type == edittypes[i]) return -1;
    // server only messages
    static int servtypes[] = { SV_INITS2C, SV_MAPRELOAD, SV_SERVMSG, SV_GIBDAMAGE, SV_DAMAGE, SV_HITPUSH, SV_SHOTFX, SV_DIED, SV_SPAWNSTATE, SV_FORCEDEATH, SV_ITEMACC, SV_ITEMSPAWN, SV_TIMEUP, SV_CDIS, SV_PONG, SV_RESUME, SV_FLAGINFO, SV_ARENAWIN, SV_SENDDEMOLIST, SV_SENDDEMO, SV_DEMOPLAYBACK, SV_CLIENT, SV_CALLVOTESUC, SV_CALLVOTEERR, SV_VOTERESULT, SV_WHOISINFO };
    if(cl) loopi(sizeof(servtypes)/sizeof(int)) if(type == servtypes[i]) return -1;
    return type;
}

// server side processing of updates: does very little and most state is tracked client only
// could be extended to move more gameplay to server (at expense of lag)

void process(ENetPacket *packet, int sender, int chan)   // sender may be -1
{
    ucharbuf p(packet->data, packet->dataLength);
    char text[MAXTRANS];
    client *cl = sender>=0 ? clients[sender] : NULL;
    int type;

    if(cl && !cl->isauthed)
    {
        int clientrole = CR_DEFAULT;
        
        if(chan==0) return;
        else if(chan!=1 || getint(p)!=SV_CONNECT) disconnect_client(sender, DISC_TAGT);
        else
        {
            int nonlocalclients = numnonlocalclients();
            getstring(text, p);
            cl->state.nextprimary = getint(p);
            if(adminpasswd && adminpasswd[0] && !strcmp(text, adminpasswd)) // pass admins always through
            { 
                cl->isauthed = true;
                clientrole = CR_ADMIN;
                loopv(bans) if(bans[i].address.host == cl->peer->address.host) { bans.remove(i); break; } // remove admin bans
                if(nonlocalclients>maxclients) loopi(nonlocalclients) if(i != sender && clients[i]->type==ST_TCPIP) 
                {
                    disconnect_client(i, DISC_MAXCLIENTS); // disconnect someone else to fit maxclients again
                    break;
                }
                logger->writeline(log::info, "[%s] client %s logged in using the admin password", cl->hostname, cl->name);
            }
            else if(serverpassword[0])
            {
                if(!strcmp(text, serverpassword)) 
                {
                    cl->isauthed = true;
                    logger->writeline(log::info, "[%s] client %s logged in", cl->hostname, cl->name);
                }
                else disconnect_client(sender, DISC_WRONGPW);
            }
            else if(mastermode==MM_PRIVATE) disconnect_client(sender, DISC_MASTERMODE);
            else if(nonlocalclients>maxclients) disconnect_client(sender, DISC_MAXCLIENTS);
            else if(isbanned(sender)) disconnect_client(sender, DISC_MBAN);
            else cl->isauthed = true;
        }
        if(!cl->isauthed) return;
        
        ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        ucharbuf p(packet->data, packet->dataLength); 
        welcomepacket(p, sender);
        enet_packet_resize(packet, p.length());
        sendpacket(sender, 1, packet);
        if(smapname[0] && m_ctf) loopi(2) sendflaginfo(i, -1, sender);
        if(clientrole != CR_DEFAULT) changeclientrole(sender, clientrole, NULL, true);
    }

    if(packet->flags&ENET_PACKET_FLAG_RELIABLE) reliablemessages = true;

    #define QUEUE_MSG { if(cl->type==ST_TCPIP) while(curmsg<p.length()) cl->messages.add(p.buf[curmsg++]); }
    #define QUEUE_INT(n) { if(cl->type==ST_TCPIP) { curmsg = p.length(); ucharbuf buf = cl->messages.reserve(5); putint(buf, n); cl->messages.addbuf(buf); } }
    #define QUEUE_STR(text) { if(cl->type==ST_TCPIP) { curmsg = p.length(); ucharbuf buf = cl->messages.reserve(2*(int)strlen(text)+1); sendstring(text, buf); cl->messages.addbuf(buf); } }

    int curmsg;
    while((curmsg = p.length()) < p.maxlen) switch(type = checktype(getint(p), cl))
    {
        case SV_TEAMTEXT:
            getstring(text, p);
            filtertext(text, text);
            if(cl) logger->writeline(log::info, "[%s] %s says to team %s: '%s'", cl->hostname, cl->name, cl->team, text);
            sendteamtext(text, sender);
            break;

        case SV_TEXT:
            QUEUE_MSG;
            getstring(text, p);
            filtertext(text, text);
            if(cl) logger->writeline(log::info, "[%s] %s says: '%s'", cl->hostname, cl->name, text);
            QUEUE_STR(text);
            break;

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
            bool newclient = false;
            if(!cl->name[0]) newclient = true;
            getstring(text, p);
            filtertext(text, text, false, MAXNAMELEN);
            if(!text[0]) s_strcpy(text, "unarmed");
            QUEUE_STR(text);
            if(!newclient && strcmp(cl->name, text)) logger->writeline(log::info,"[%s] %s changed his name to %s", cl->hostname, cl->name, text);
            s_strncpy(cl->name, text, MAXNAMELEN+1);
            if(newclient && cl->type==ST_TCPIP)
            {
                savedscore *sc = findscore(*cl, false);
                if(sc)
                {
                    sc->restore(cl->state);
                    sendf(-1, 1, "ri2i8vvi", SV_RESUME, sender, cl->state.state, cl->state.lifesequence, cl->state.gunselect, sc->flagscore, sc->frags, sc->deaths, cl->state.health, cl->state.armour, NUMGUNS, cl->state.ammo, NUMGUNS, cl->state.mag, -1);
                }
            }
            getstring(text, p);
            filtertext(cl->team, text, false, MAXTEAMLEN);
            QUEUE_STR(text);
            getint(p);
            QUEUE_MSG;
            break;
        }
               
        case SV_ITEMLIST:
        {
            int n;
            while((n = getint(p))!=-1) if(notgotitems)
            {
                server_entity se = { getint(p), false, 0 };
                while(sents.length()<=n) sents.add(se);
                sents[n].spawned = true;
            }
            notgotitems = false;
            QUEUE_MSG;
            break;
        }

        case SV_ITEMPICKUP:
        {
            int n = getint(p);
            gameevent &pickup = cl->addevent();
            pickup.type = GE_PICKUP;
            pickup.pickup.ent = n;
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
            cl->state.state = CS_DEAD;
            cl->state.respawn();
            sendf(-1, 1, "rii", SV_FORCEDEATH, cl->clientnum);
            break;

        case SV_TRYSPAWN:
            if(cl->state.state!=CS_DEAD || cl->state.lastspawn>=0 || !canspawn(cl)) break;
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
            QUEUE_MSG;
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
 
        case SV_PING:
            sendf(sender, 1, "ii", SV_PONG, getint(p));
            break;

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
            if(cl->type==ST_TCPIP && (cl->state.state==CS_ALIVE || cl->state.state==CS_EDITING))
            {
                cl->position.setsizenodelete(0);
                while(curmsg<p.length()) cl->position.add(p.buf[curmsg++]);
            }
            break;
        }

        case SV_NEXTMAP:
        {
            getstring(text, p);
            filtertext(text, text);
            int mode = getint(p);
            if(mapreload || numclients() == 1) resetmap(text, mode);
            break;
        }

        case SV_SENDMAP:
        {
            getstring(text, p);
            filtertext(text, text);
            int mapsize = getint(p);
            int cfgsize = getint(p);
            if(p.remaining() < mapsize + cfgsize)
            {
                p.forceoverread();
                break;
            }
            sendmapserv(sender, text, mapsize, cfgsize, &p.buf[p.len]);
            p.len += mapsize + cfgsize;
            break;
        }

        case SV_RECVMAP:
        {
            ENetPacket *mappacket = getmapserv(cl->clientnum);
            if(mappacket)
            {
                sendpacket(cl->clientnum, 2, mappacket);
                cl->state.state = CS_DEAD; // allow respawn after map download
                cl->state.reset();
            }
            else sendservmsg("no map to get", cl->clientnum);
            break;
        }
			
		case SV_FLAGPICKUP:
		case SV_FLAGDROP:
		case SV_FLAGRETURN:
		case SV_FLAGSCORE:
		case SV_FLAGRESET:
		{
			flagaction(getint(p), type, sender);
			break;
		}

        case SV_FLAGS:
            cl->state.flagscore = getint(p);
            QUEUE_MSG;
            break;

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
                    else vi->action = new mapaction(newstring(text), mode);
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
                case SA_FORCETEAM:
                    vi->action = new forceteamaction(getint(p));
                    break;
                case SA_GIVEADMIN:
                    vi->action = new giveadminaction(getint(p) > 0);
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
            }
            vi->owner = sender;
            vi->callmillis = servmillis;
            if(callvote(vi)) { QUEUE_MSG; }
            else delete vi;
            break;
        }

        case SV_VOTE:
        {
            if(vote(sender, getint(p))) QUEUE_MSG;
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

        default:
        {
            int size = msgsizelookup(type);
            if(size==-1) { if(sender>=0) disconnect_client(sender, DISC_TAGT); return; }
            loopi(size-1) getint(p);
            QUEUE_MSG;
            break;
        }
    }

    if(p.overread() && sender>=0) disconnect_client(sender, DISC_EOP);
}

void localclienttoserver(int chan, ENetPacket *packet)
{
    process(packet, 0, chan);
    if(!packet->referenceCount) enet_packet_destroy(packet);
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
        minremain = gamemillis>=gamelimit ? 0 : (gamelimit - gamemillis + 60000 - 1)/60000;
        sendf(-1, 1, "ri2", SV_TIMEUP, minremain);
    }
    if(!interm && minremain<=0) interm = gamemillis+10000;
}

void resetserverifempty()
{
    loopv(clients) if(clients[i]->type!=ST_EMPTY) return;
    resetmap("", 0, 10, false);
	mastermode = MM_OPEN;
	autoteam = true;
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

void serverslice(uint timeout)   // main server update, called from cube main loop in sp, or dedicated server loop
{
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
        if(m_ctf) loopi(2)
        {
            sflaginfo &f = sflaginfos[i];
		    if(f.state==CTFF_DROPPED && gamemillis-f.lastupdate>30000) flagaction(i, SV_FLAGRESET, -1);
        }
        if(m_arena) arenacheck();
    }

    if(curvote)
    {
        if(!curvote->isalive()) curvote->evaluate(true);
        if(curvote->result!=VOTE_NEUTRAL) DELETEP(curvote);
    }
   
    int nonlocalclients = numnonlocalclients();

    if((smode>1 || (gamemode==0 && nonlocalclients)) && gamemillis-diff>0 && gamemillis/60000!=(gamemillis-diff)/60000)
        checkintermission();
    if(interm && gamemillis>interm)
    {
        if(demorecord) enddemorecord();
        interm = 0;

        if(configsets.length()) nextcfgset();
        else loopv(clients) if(clients[i]->type!=ST_EMPTY)
        {
            sendf(i, 1, "rii", SV_MAPRELOAD, 0);    // ask a client to trigger map reload
            mapreload = true;
            break;
        }
    }

    resetserverifempty();
    
    if(!isdedicated) return;     // below is network only

    serverms(smode, numclients(), minremain, smapname, servmillis, serverhost->address.port);

    if(servmillis-laststatus>60*1000)   // display bandwidth stats, useful for server ops
    {
        laststatus = servmillis;     
		if(nonlocalclients || bsend || brec) 
		{ 
            bool multipleclients = numclients()>1;
            static const char *roles[] = { "normal", "admin" };
            loopv(clients)
            {
                if(clients[i]->type == ST_EMPTY || !clients[i]->name[0]) continue;
                if(!i) logger->writeline(log::info, "\ncn name                       team frag death flags  role    host");

                logger->writeline(log::info, "%2d %-25s %5s %4d %5d %5d  %-6s  %s",
                    clients[i]->clientnum, 
                    clients[i]->name,      
                    clients[i]->team,      
                    clients[i]->state.frags,
                    clients[i]->state.lifesequence,
                    clients[i]->state.flagscore,
                    roles[clients[i]->role],       
                    clients[i]->hostname);
            }
            if(multipleclients) logger->writeline(log::info, "\n");

            if (m_teammode && multipleclients)
            {

                cvector teams;
                bool flag;
                loopv(clients)
                {
                    flag = true;
                    loopvj(teams)
                    {
                        if(strcmp(clients[i]->team,teams[j])==0 || !clients[i]->team[0])
                        {
                            flag = false;
                            break;
                        }
                    }
                    if(flag) teams.add(clients[i]->team);
                }

                loopv(teams)
                {
                    int fragscore = 0;
                    int flagscore = 0;
                    loopvj(clients)
                    {
                        if(!(strcmp(clients[j]->team,teams[i])==0)) continue;
                        fragscore += clients[j]->state.frags;
                        flagscore += clients[j]->state.flagscore;
                    }
                    logger->writeline(log::info, "Team %5s: %3d frags", teams[i], fragscore);
                    if(m_ctf) logger->writeline(log::info, "Team %5s: %3d flags", teams[i], flagscore); // ctf only
                }
            }
	
		    time_t rawtime;
		    struct tm * timeinfo;
		    char buffer [80];

		    time (&rawtime);
		    timeinfo = localtime (&rawtime);
		    strftime (buffer,80,"%d-%m-%Y %H:%M:%S",timeinfo);

            logger->writeline(log::info, "Status at %s: %d remote clients, %.1f send, %.1f rec (K/sec)", buffer, nonlocalclients, bsend/60.0f/1024, brec/60.0f/1024);
            logger->writeline(log::info, "Time remaining: %d minutes for %s game, mastermode %d.", minremain, modestr(gamemode), mastermode);
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
				char hn[1024];
				s_strcpy(c.hostname, (enet_address_get_host_ip(&c.peer->address, hn, sizeof(hn))==0) ? hn : "unknown");
                logger->writeline(log::info,"[%s] client connected", c.hostname);
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
    if(serverhost) enet_host_destroy(serverhost);
    if(svcctrl) svcctrl->stop();
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

void extinfo_statsbuf(ucharbuf &p, int pid, int bpos, ENetSocket &pongsock, ENetAddress &addr, ENetBuffer &buf, int len)
{
    loopv(clients)
    {
        if(clients[i]->type == ST_EMPTY) continue;
        if(pid>-1 && clients[i]->clientnum!=pid) continue;
        
        putint(p,EXT_PLAYERSTATS_RESP_STATS);  // send player stats following
        putint(p,clients[i]->clientnum);  //add player id
        sendstring(clients[i]->name,p);         //Name
        sendstring(clients[i]->team,p);         //Team
        putint(p,clients[i]->state.frags);      //Frags
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

        if(pid>-1) break;
        p.len=bpos;
    }
}

void extinfo_teamscorebuf(ucharbuf &p)
{
    if(!m_teammode)
    {
        putint(p,EXT_ERROR); // send error
        putint(p,minremain);    //remaining play time
        return;
    }

    putint(p, m_teammode ? EXT_ERROR_NONE : EXT_ERROR);
    putint(p, minremain);
    putint(p, gamemode);
    if(!m_teammode) return;

    cvector teams;
    bool flag;
    loopv(clients)
    {
        flag = true;
        loopvj(teams)
        {
            if(strcmp(clients[i]->team,teams[j])==0 || !clients[i]->team[0])
            {
                flag = false;
                break;
            }
        }
        if(flag) teams.add(clients[i]->team);
    }

    loopv(teams)
    {
        sendstring(teams[i],p); //team
        int fragscore = 0;
        int flagscore = 0;
        loopvj(clients)
        {
            if(!(strcmp(clients[j]->team,teams[i])==0)) continue;
            fragscore += clients[j]->state.frags;
            flagscore += clients[j]->state.flagscore;
        }
        putint(p,fragscore); //add fragscore per team
        if(m_ctf) //when capture mode
        {
            putint(p,flagscore); //add flagscore per team
        }
        else //all other team modes
        {
            putint(p,-1); //flagscore not available
        }
        putint(p,-1);
    }
}


#ifndef STANDALONE
void localdisconnect()
{
    loopv(clients) if(clients[i]->type==ST_LOCAL) clients[i]->zap();
}

void localconnect()
{
    client &c = addclient();
    c.type = ST_LOCAL;
    c.role = CR_ADMIN;
    s_strcpy(c.hostname, "local");
    sendintro();
}
#endif

void initserver(bool dedicated, int uprate, const char *sdesc, const char *ip, int serverport, const char *master, const char *passwd, int maxcl, const char *maprot, const char *adminpwd, const char *srvmsg, int scthreshold)
{
    if(serverport<=0) serverport = CUBE_DEFAULT_SERVER_PORT;
    if(passwd) s_strcpy(serverpassword, passwd);
    maxclients = maxcl > 0 ? min(maxcl, MAXCLIENTS) : DEFAULTCLIENTS;
    servermsinit(master ? master : AC_MASTER_URI, ip, CUBE_SERVINFO_PORT(serverport), sdesc, dedicated);
   
    s_sprintfd(identity)("%s[%d]", ip && ip[0] ? ip : "local", serverport); 
    logger = newlogger(identity);
    if(dedicated) logger->open(); // log on ded servers only
    logger->writeline(log::info, "logging local AssaultCube server now..");

    if(isdedicated = dedicated)
    {
        ENetAddress address = { ENET_HOST_ANY, serverport };
        if(*ip && enet_address_set_host(&address, ip)<0) logger->writeline(log::warning, "server ip not resolved!");
        serverhost = enet_host_create(&address, maxclients+1, 0, uprate);
        if(!serverhost) fatal("could not create server host");
        loopi(maxclients) serverhost->peers[i].data = (void *)-1;
		if(!maprot || !maprot[0]) maprot = newstring("config/maprot.cfg");
        readscfg(path(maprot, true));
        if(adminpwd && adminpwd[0]) adminpasswd = adminpwd;
        if(srvmsg && srvmsg[0]) motd = srvmsg;
        scorethreshold = min(-1, scthreshold);
    }

    resetserverifempty();

    if(isdedicated)       // do not return, this becomes main loop
    {
        #ifdef WIN32
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        #endif
        logger->writeline(log::info, "dedicated server started, waiting for clients...\nCtrl-C to exit\n");
        atexit(enet_deinitialize);
        atexit(cleanupserver);
        for(;;) serverslice(5);
    }
}

#ifdef STANDALONE

void localservertoclient(int chan, uchar *buf, int len) {}
void fatal(const char *s, ...) 
{ 
    cleanupserver(); 
    s_sprintfdlv(msg,s,s);
    s_sprintfd(out)("fatal: %s", msg);
    if(logger) logger->writeline(log::error, out);
    else puts(out);
    exit(EXIT_FAILURE); 
}

int main(int argc, char **argv)
{   
    int uprate = 0, maxcl = DEFAULTCLIENTS, scthreshold = -5, port = 0;
    const char *sdesc = "", *ip = "", *master = NULL, *passwd = "", *maprot = "", *adminpasswd = NULL, *srvmsg = NULL, *service = NULL;

    for(int i = 1; i<argc; i++)
    {
        char *a = &argv[i][2];
        if(argv[i][0]=='-') switch(argv[i][1])
        {
            case 'u': uprate = atoi(a); break;
            case 'n': sdesc  = a; break;
            case 'i': ip     = a; break;
            case 'm': master = a; break;
            case 'p': passwd = a; break;
            case 'c': maxcl  = atoi(a); break;
            case 'r': maprot = a; break; 
            case 'x': adminpasswd = a; break;
            case 'o': srvmsg = a; break;
            case 'k': scthreshold = atoi(a); break;
            case 's': service = a; break;
            case 'f': port = atoi(a); break;
            default: logger->writeline(log::warning, "WARNING: unknown commandline option");
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
    initserver(true, uprate, sdesc, ip, port, master, passwd, maxcl, maprot, adminpasswd, srvmsg, scthreshold);
    return EXIT_SUCCESS;
}
#endif

