// server.cpp: little more than enhanced multicaster
// runs dedicated or as client coroutine

#include "cube.h"

#ifdef STANDALONE
#define DEBUGCOND (true)
#else
VARP(serverdebug, 0, 0, 1);
#define DEBUGCOND (serverdebug==1)
#endif

#include "server.h"
#include "servercontroller.h"
#include "serverfiles.h"

#include "signal.h"

struct servergame
{
    int mastermode;
    bool autoteam;
    int matchteamsize;
    bool forceintermission;
    string servdesc_current;
    ENetAddress servdesc_caller;
    bool custom_servdesc;
    int sispaused = 0;

    // current game
    string smapname, nextmapname;
    int smode, nextgamemode, srvgamesalt;
    int interm;
    int minremain, gamemillis, gamelimit, nextsendscore;
    int arenaround, arenaroundstartmillis;
    int lastfillup;
    vector<server_entity> sents;
    sflaginfo sflaginfos[2];

    bool recordpackets;
    int demosequence;   // # of demo to fetch with F10 (usually curent game, until one minute after it finished)

    servermap *curmap;
    char *layout;  // NULL: edit mode or singleplayer without properly loaded map
    uchar *coop_mapdata;
    int coop_cgzlen, coop_cfglen, coop_cfglengz;

    void init()
    {
        mastermode = MM_OPEN;
        autoteam = true;
        matchteamsize = 0;
        forceintermission = false;
        custom_servdesc = false;
        smode = 0;
        interm = 0;
        minremain = 0;
        gamemillis = 0;
        gamelimit = 0;
        nextsendscore = 0;
        arenaround = 0;
        arenaroundstartmillis = 0;
        lastfillup = 0;
        sents.setsize(0);
        loopi(2) sflaginfos[i].actor_cn = -1;
        recordpackets = false;
        demosequence = 0;
        curmap = NULL;
        layout = NULL;
        coop_mapdata = NULL;
        coop_cgzlen = coop_cfglen = coop_cfglengz = 0;
    }

    void free()
    {
        DELETEA(layout);
        DELETEA(coop_mapdata);
    }
} cursgame, *sg = &cursgame;

// config
vector<servermap *> servermaps, gamesuggestions;             // all available maps kept in memory
string servpubkey;
uchar ssk[64] = { 0 }, *spk = ssk + 32;
enet_uint32 servownip; // IP address of this server, as seen by the master or other clients/servers

servercontroller *svcctrl = NULL;
serverparameter serverparameters;
servercommandline scl;
servermaprot maprot;
serveripblacklist ipblacklist;
serveripcclist geoiplist;
servernickblacklist nickblacklist;
serverforbiddenlist forbiddenlist;
serverpasswords passwords;
serverinfofile serverinfoinfo;
serverinfofile serverinfomotd;

// server state
bool isdedicated = false;
ENetHost *serverhost = NULL;

int laststatus = 0,                 // last per-minute log status
    servmillis = 0,                 // millis since server start
    servclock = 0;                  // unixtime in minutes

vector<client *> clients;
vector<worldstate *> worldstates;
vector<savedscore> savedscores;
vector<ban> bans;

long int incoming_size = 0;
int shuffleskillthreshold = 200; 
//int connectskillthreshold = 0;

// cmod
int totalclients = 0;
int cn2boot;

// synchronising the worker threads...
const char *endis[] = { "disabled", "enabled", "" };
SERVPARLIST(dumpmaprot, 0, 1, 0, endis, "ddump maprot parameters for all maps (once, to logs/debug/maprot_debug_verbose.txt)");
SERVPARLIST(dumpparameters, 0, 0, 0, endis, "ddump server parameters when updated");
//SERVPAR(vitaautosave, 0, 10, 24 * 60, "sVita file autosave interval in minutes (0: only when server is empty)"); // deprecated
SERVPAR(vitamaxage, 3, 12, 120, "sOmit vitas from autosave, if the last login has been more than the specified number of months ago");

//int lastvitasave = 0; // deprecated
bool pushvitasforsaving(bool forceupdate = false)
{
    if(isdedicated)
    {
        if(vitastosave) // only if the last push has been processed
        {
            return false;
        }
        else
        {
            //lastvitasave = servmillis;
            vitastosave = new vector<vitakey_s>;
            vitakey_s vk;
            int too_old = 0;
            enumeratekt(vitas, uchar32, k, vita_s, v,
            {
                if((servclock - v.vs[VS_LASTLOGIN]) / (60 * 24 * 31) <= vitamaxage // Vita is within the max age (months)
                   || (v.vs[VS_BAN] == 1 || (servclock - v.vs[VS_BAN]) < 0) // Do not remove if vita has the ban, whitelist, or admin flag set
                   || (v.vs[VS_WHITELISTED] == 1 || (servclock - v.vs[VS_WHITELISTED]) < 0)
                   || (v.vs[VS_ADMIN] == 1 || (servclock - v.vs[VS_ADMIN]) < 0))
                {
                    vk.k = &k;
                    vk.v = &v;
                    vitastosave->add(vk);
                }
                else too_old++;
            });
            if(too_old) mlog(ACLOG_VERBOSE, "omitted %d vitas from autosave (exceeding maxage of %d months)", too_old, vitamaxage);
            if(forceupdate && readserverconfigsthread_sem) readserverconfigsthread_sem->post();
        }
        return true;
    }
    return false;
}

bool triggerpollrestart = false;

void poll_serverthreads()       // called once per mainloop-timeslice
{
    static vector<servermap *> servermapstodelete;
    static int stage = 0, lastworkerthreadstart = 0, nextserverconfig = 0;
    static bool debugmaprot = true, serverwasnotempty = false;

    switch(stage) // cycle through some tasks once per minute (mostly file polling)
    {
        default: // start first thread
        {
            if(startnewservermapsepoch || readmapsthread_sem->getvalue()) fatal("thread management mishap");  // zero tolerance...

            // wake readmapsthread
            mlog(ACLOG_DEBUG,"waking readmapsthread");
            startnewservermapsepoch = true;
            servclock = (int) (time(NULL) / (time_t) 60);
            readmapsthread_sem->post();
            stage = 1;
            lastworkerthreadstart = servmillis;
            triggerpollrestart = false;
            break;
        }
        case 1:  // readmapsthread building/updating the list of maps in memory
        {
            servermap *fresh = (servermap *) servermapdropbox;
            if(fresh)
            {
                if(fresh == servermapdropbox)
                {
                    // got new servermap...
                    loopv(servermaps)
                    {
                        if(!strcmp(servermaps[i]->fname, fresh->fname))   // we don't check paths here - map filenames have to be unique
                        {  // found map of same name
                            mlog(ACLOG_VERBOSE,"marked servermap %s%s for deletion", servermaps[i]->fpath, servermaps[i]->fname);
                            servermapstodelete.add(servermaps.remove(i)); // mark old version for deletion
                        }
                    }
                    if(fresh->isok)
                    {
                        servermaps.add(fresh);
                        mlog(servmillis>2345?ACLOG_INFO:ACLOG_VERBOSE,"added servermap %s%s", fresh->fpath, fresh->fname); // 1st time only in VERBOSE
                        maprot.initmap(fresh, NULL);
                        servermaps.sort(servermapsortname); // keep list sorted at all times
                    }
                    else delete fresh;
                    servermapdropbox = NULL;
                }
                readmapsthread_sem->post();
            }
            else if(!startnewservermapsepoch)
            {
                // readmapsthread is done
                if(servmillis<2000)mlog(ACLOG_INFO,"added %d servermaps",servermaps.length());
                while(!readmapsthread_sem->trywait())
                    ;
                stage++;
            }
            break;
        }
        case 2:  // wake readserverconfigsthread
        {
            recheckallserverconfigs = true;
            nextserverconfig = 0;
            readserverconfigsthread_sem->post();
            stage++;
            break;
        }
        case 3:  // readserverconfigsthread
        {
            if(!recheckallserverconfigs)
            {
                if(serverconfigs.inrange(nextserverconfig))
                {
                    serverconfigs[nextserverconfig]->process();  // processing of config files is done in mainloop, one at a time
                    nextserverconfig++;
                }
                else
                {
                    if(vitaupdatebuf)
                    {
                        mlog(ACLOG_INFO, "read/updated %d vitas from %s", parsevitas(vitaupdatebuf, vitaupdatebuflen), vitafilename_update_backup);;
                        DELETEA(vitaupdatebuf);
                    }
                    stage++;
                }
            }
            break;
        }
        case 4:  // check for fresh config files
        {
            if(maprot.updated)
            {
                maprot.updated = false;
                defformatstring(fname)("%sdebug/maprot_debug_verbose.txt", scl.logfilepath);
                stream *f = debugmaprot && dumpmaprot ? openfile(path(fname), "w") : NULL;
                loopv(servermaps) maprot.initmap(servermaps[i], f); // may stall a few msec if there are very many maps
                DELETEP(f);
                debugmaprot = false; // only once during server start
            }
            else if(serverparameters.updated)
            {
                serverparameters.updated = false;
                if(dumpparameters)
                {
                    defformatstring(fname)("%sdebug/serverparameter_dump_%s.txt", scl.logfilepath, numtime());
                    stream *f = openfile(path(fname), "w");
                    f->printf("// server parameter dump, %s\n\n", asctimestr());
                    serverparameters.dump(f);
                    delete f;
                }
            }
            else if(!vitastosave && serverwasnotempty && numclients() == 0)
            {
                serverwasnotempty = !pushvitasforsaving(false);
            }
            else stage++;
            break;
        }
        case 5:  // log changed server diagnostics parameters
            serverparameters.log();
            stage++;
            break;

        case 6:  // pause worker threads for a while (restart once a minute)
        {
            if(triggerpollrestart || servmillis - lastworkerthreadstart > 60 * 1000) stage = 0;
            else if(numclients() == 0)
            {   // empty server and nothing to do:
                loopvrev(servermapstodelete)
                {
                    gamesuggestions.removeobj(servermapstodelete[i]);
                    delete servermapstodelete.remove(i);    // delete outdated servermaps
                }
            }
            else serverwasnotempty = true;
            break;
        }
    }

    // update thread-safe data structures (vectors and hashtables can be read concurrently - but to be updated, they need to be locked)
    poll_logbuffers();


}

SERVPAR(gamepenalty_cutoff, 30, 60, 120, "gNumber of minutes to remember that a map+mode combination has been played");
SERVPAR(gamepenalty_random, 1, 1, 60, "gAmount of random weight to add to each map+mode combination");
SERVPAR(mappenalty_cutoff, 45, 60, 240, "gNumber of minutes to remember that a map has been played");
SERVPAR(mappenalty_weight, 0, 100, 200, "gInfluence of play history of a map (0: no influence, 100: normal, 200: high influence)");
SERVPAR(modepenalty_weight, -100, 50, 100, "gInfluence of play history of a game mode (-100: stay with played mode, 0: ignore played mode, 100: suggest other modes)");

void servermaprot::calcmappenalties() // sum up, how long ago each map+mode was played -> get individual penalties for every map+mode combination
{
    int modecutoff = gamepenalty_cutoff * 60, mapcutoff = mappenalty_cutoff * 60;
    loopv(servermaps)
    {
        servermap &s = *servermaps[i];
        s.mappenalty = 0;
        loopi(GMODE_NUM) s.penalty[i] = 0;
        loopi(GAMEHISTLEN) if(s.lastplayed[i] && ((1 << s.lastmodes[i]) & s.modes_allowed))
        {
            int d = (servmillis - s.lastplayed[i]) / 1000, m = s.lastmodes[i];
            if(d < modecutoff)
            {
                s.penalty[m] += ((modecutoff - d + 119) * PENALTYUNIT) / modecutoff; // last minute -> 1 PENALTYUNIT, "cutoff" minutes ago -> 0 PENALTYUNIT, linear inbetween
            }
            if(d < mapcutoff)
            {
                s.mappenalty += ((mapcutoff - d + 59) * PENALTYUNIT) / mapcutoff; // last minute -> 1 PENALTYUNIT, "cutoff" minutes ago -> 0 PENALTYUNIT, linear inbetween
            }
        }
        loopi(GMODE_NUM) if((1 << i) & s.modes_allowed)
        {
            s.penalty[i] = (s.penalty[i] * (100 - s.modes[i].repeat)) / 100;
            s.penalty[i] -= s.modes[i].weight * PENALTYUNIT;
            s.penalty[i] += rnd(gamepenalty_random * PENALTYUNIT);
        }
    }
}

servermap *randommap()
{
    if(servermaps.length() < 1) fatal("no maps available");
    servermap *s = servermaps[rnd(servermaps.length())];
    s->bestmode = 0; // tdm
    return s;
}

SERVPARLIST(dumpsuggestions, 0, 1, 0, endis, "ddump maprot suggestions whenever recalculated");

servermap *servermaprot::recalcgamesuggestions(int numpl) // regenerate list of playable games (map+mode)
{
    calcmodepenalties(modepenalty_weight);
    calcmappenalties();
    gamesuggestions.setsize(0);
    if(numpl >= MAXPLAYERS) numpl = MAXPLAYERS - 1;
    loopv(servermaps)
    {
        servermap &s = *servermaps[i];
        s.bestmode = -1;
        s.weight = INT_MIN;
        int bestpen = INT_MAX;
        int modemask = (numpl < 0 ? s.modes_allowed : s.modes_pn[numpl]) & s.modes_auto;
        loopi(GMODE_NUM) if(modemask & (1 << i))
        {
            if((s.penalty[i] + modepenalty[i]) < bestpen)
            {
                bestpen = s.penalty[i] + modepenalty[i];
                s.bestmode = i;
            }
        }
        if(s.bestmode >= 0)
        {
            s.weight = -bestpen - ((mappenalty_weight + 1) * s.mappenalty) / 100;
            gamesuggestions.add(&s);
        }
    }
    gamesuggestions.sort(servermapsortweight);
    if(dumpsuggestions)
    {
        defformatstring(fname)("%sdebug/maprotsuggestions_dump_%s.txt", scl.logfilepath, numtime());
        stream *f = openfile(path(fname), "w");
        f->printf("// maprot suggestions dump, %s\n\nmode penalties:", asctimestr());
        string b;
        loopj(GMODE_NUM) if((1 << j) & GMMASK__MPNOCOOP) f->printf(" %s:%d", gmode_enum(1 << j, b), modepenalty[j]);
        f->printf("\n\n");
        loopv(gamesuggestions)
        {
            servermap &s = *gamesuggestions[i];
            f->printf("map: \"%s%s\", weight: %d, bestmode: %d (%s), mappenalty: %d\n  mode penalties:", s.fpath, s.fname, s.weight, s.bestmode, gmode_enum(1 << s.bestmode, b), s.mappenalty);
            loopj(GMODE_NUM) if(s.penalty[j]) f->printf(" %s:%d", gmode_enum(1 << j, b), s.penalty[j]);
            f->printf("\n\n");
        }
        delete f;
    }
    return gamesuggestions.length() ? gamesuggestions[0] : randommap();
}

servermap *getservermap(const char *mapname) // check, if the server knows a map (by filename)
{
    servermap *k = new servermap(mapname,""), **res = servermaps.search(&k, servermapsortname);
    return res ? *res : NULL;
}

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

void sendpacket(int n, int chan, ENetPacket *packet, int exclude, bool demopacket)
{
    if(n<0)
    {
        recordpacket(chan, packet->data, (int)packet->dataLength);
        loopv(clients) if(i!=exclude && (clients[i]->type!=ST_TCPIP || clients[i]->isauthed)) sendpacket(i, chan, packet, -1, demopacket);
        return;
    }
    switch(clients[n]->type)
    {
        case ST_TCPIP:
        {
            enet_peer_send(clients[n]->peer, chan, packet);
            break;
        }

        case ST_LOCAL:
            localservertoclient(chan, packet->data, (int)packet->dataLength, demopacket);
            break;
    }
}

static bool reliablemessages = false;

bool buildworldstate()
{
    static struct { int posoff, poslen, msgoff, msglen; } pkt[MAXCLIENTS];
    worldstate &ws = *new worldstate;
    loopv(clients)
    {
        client &c = *clients[i];
        if(c.type!=ST_TCPIP || !c.isauthed) continue;
        c.overflow = 0;
        if(c.position.empty()) pkt[i].posoff = -1;
        else
        {
            pkt[i].posoff = ws.positions.length();
            ws.positions.put(c.position.getbuf(), c.position.length());
            pkt[i].poslen = ws.positions.length() - pkt[i].posoff;
            c.position.setsize(0);
        }
        if(c.messages.empty()) pkt[i].msgoff = -1;
        else
        {
            pkt[i].msgoff = ws.messages.length();
            putint(ws.messages, SV_CLIENT);
            putint(ws.messages, c.clientnum);
            putuint(ws.messages, c.messages.length());
            ws.messages.put(c.messages.getbuf(), c.messages.length());
            pkt[i].msglen = ws.messages.length() - pkt[i].msgoff;
            c.messages.setsize(0);
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
        if(psize && (pkt[i].posoff<0 || psize-pkt[i].poslen>0))
        {
            packet = enet_packet_create(&ws.positions[pkt[i].posoff<0 ? 0 : pkt[i].posoff+pkt[i].poslen],
                                        pkt[i].posoff<0 ? psize : psize-pkt[i].poslen,
                                        ENET_PACKET_FLAG_NO_ALLOCATE);
            sendpacket(c.clientnum, 0, packet);
            if(!packet->referenceCount) enet_packet_destroy(packet);
            else { ++ws.uses; packet->freeCallback = cleanworldstate; }
        }

        if(msize && (pkt[i].msgoff<0 || msize-pkt[i].msglen>0))
        {
            packet = enet_packet_create(&ws.messages[pkt[i].msgoff<0 ? 0 : pkt[i].msgoff+pkt[i].msglen],
                                        pkt[i].msgoff<0 ? msize : msize-pkt[i].msglen,
                                        (reliablemessages ? ENET_PACKET_FLAG_RELIABLE : 0) | ENET_PACKET_FLAG_NO_ALLOCATE);
            sendpacket(c.clientnum, 1, packet);
            if(!packet->referenceCount) enet_packet_destroy(packet);
            else { ++ws.uses; packet->freeCallback = cleanworldstate; }
        }
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

client *getclientbysequence(int seq)
{
    loopv(clients) if(clients[i]->type != ST_EMPTY && clients[i]->clientsequence == seq) return clients[i];
    return NULL;
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

int *numteamclients(int exclude = -1)
{
    static int num[TEAM_NUM];
    loopi(TEAM_NUM) num[i] = 0;
    loopv(clients) if(i != exclude && clients[i]->type!=ST_EMPTY && clients[i]->isauthed && clients[i]->isonrightmap && team_isvalid(clients[i]->team)) num[clients[i]->team]++;
    return num;
}

int sendservermode(bool send = true)
{
    int sm = (sg->autoteam ? AT_ENABLED : AT_DISABLED) | ((sg->mastermode & MM_MASK) << 2) | (sg->matchteamsize << 4);
    if(send) sendf(-1, 1, "ri2", SV_SERVERMODE, sm);
    return sm;
}

void changematchteamsize(int newteamsize)
{
    if(newteamsize < 0) return;
    if(sg->matchteamsize != newteamsize)
    {
        sg->matchteamsize = newteamsize;
        sendservermode();
    }
    if(sg->mastermode == MM_MATCH && sg->matchteamsize && m_teammode)
    {
        int size[2] = { 0 };
        loopv(clients) if(clients[i]->type!=ST_EMPTY && clients[i]->isauthed && clients[i]->isonrightmap)
        {
            if(team_isactive(clients[i]->team))
            {
                if(++size[clients[i]->team] > sg->matchteamsize) updateclientteam(i, team_tospec(clients[i]->team), FTR_SILENTFORCE);
            }
        }
    }
}

void changemastermode(int newmode)
{
    if(sg->mastermode != newmode)
    {
        sg->mastermode = newmode;
        senddisconnectedscores(-1);
        if(sg->mastermode != MM_MATCH)
        {
            loopv(clients) if(clients[i]->type!=ST_EMPTY && clients[i]->isauthed)
            {
                if(clients[i]->team == TEAM_CLA_SPECT || clients[i]->team == TEAM_RVSF_SPECT) updateclientteam(i, TEAM_SPECT, FTR_SILENTFORCE);
            }
        }
        else if(sg->matchteamsize) changematchteamsize(sg->matchteamsize);
    sendservermode();
    }
}

void setpausemode(int newmode)
{
    if (sg->sispaused != newmode) {
        sg->sispaused = newmode;
    }

    loopv(clients) if (clients[i]->type != ST_EMPTY)
    {
        if (!valid_client(i)) return;
        clients[i]->ispaused = newmode;
    }

    sendf(-1, 1, "ri2", SV_PAUSEMODE, newmode);
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
    enet_uint32 mask = ENET_HOST_TO_NET_32(sg->mastermode == MM_MATCH ? 0xFFFF0000 : 0xFFFFFFFF); // in match mode, reconnecting from /16 subnet is allowed
    if(!insert)
    {
        loopv(clients)
        {
            client &o = *clients[i];
            if(o.type!=ST_TCPIP || !o.isauthed) continue;
            if(o.clientnum!=c.clientnum && o.peer->address.host==c.peer->address.host && !strcmp(o.name, c.name))
            {
                static savedscore curscore;
                curscore.save(o.state, o.team);
                return &curscore;
            }
        }
    }
    loopv(savedscores)
    {
        savedscore &sc = savedscores[i];
        if(!strcmp(sc.name, c.name) && (sc.ip & mask) == (c.peer->address.host & mask)) return &sc;
    }
    if(!insert) return NULL;
    savedscore &sc = savedscores.add();
    copystring(sc.name, c.name);
    sc.ip = c.peer->address.host;
    return &sc;
}

void sendf(int cn, int chan, const char *format, ...)
{
    int exclude = -1;
    bool reliable = false;
    if(*format=='r') { reliable = true; ++format; }
    packetbuf p(MAXTRANS, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    va_list args;
    va_start(args, format);
    while(*format) switch(*format++)
    {
        case 'x': // exclude client from getting this message
            exclude = va_arg(args, int);
            ASSERT(cn == -1);
            break;

        case 'v': // integer array of variable length
        {
            int n = va_arg(args, int);
            int *v = va_arg(args, int *);
            loopi(n) putint(p, v[i]);
            break;
        }

        case 'k': // 32 bytes (uchar)
            p.put(va_arg(args, uchar *), 32);
            break;

        case 'i': // one to nine integer arguments
        {
            int n = isdigit(*format) ? *format++-'0' : 1;
            loopi(n) putint(p, va_arg(args, int));
            break;
        }

        case 'a': // one to nine mostly positive integer arguments
        {
            int n = isdigit(*format) ? *format++-'0' : 1;
            loopi(n) putaint(p, va_arg(args, int));
            break;
        }

        case 'u': // unsigned integer argument
            putuint(p, va_arg(args, int));
            break;

        case 'I': // IPv4 address argument
            putip4(p, va_arg(args, enet_uint32));
            break;

        case 's': // string
            sendstring(va_arg(args, const char *), p);
            break;

        case 'm': // raw message block
        {
            int n = va_arg(args, int);
            p.put(va_arg(args, uchar *), n);
            break;
        }
    }
    va_end(args);
    sendpacket(cn, chan, p.finalize(), exclude);
}

void sendservmsg(const char *msg, int cn = -1)
{
    sendf(cn, 1, "ris", SV_SERVMSG, msg);
}

void sendservmsgverbose(const char *msg, int cn = -1) // channel server spam here...
{
    sendf(cn, 1, "ris", SV_SERVMSGVERB, msg);
}

void sendspawn(client *c)
{
    if(team_isspect(c->team)) return;
    clientstate &gs = c->state;
    gs.respawn();
    gs.spawnstate(sg->smode);
    gs.lifesequence++;
    sendf(c->clientnum, 1, "ri7vv", SV_SPAWNSTATE, gs.lifesequence,
        gs.health, gs.armour,
        gs.primary, gs.gunselect, m_arena ? c->spawnindex : -1,
        NUMGUNS, gs.ammo, NUMGUNS, gs.mag);
    gs.lastspawn = sg->gamemillis;
}

// simultaneous demo recording, fully buffered

#define DEMORINGBUFSIZE (1<<18) // 256KB
#define MAXDEMOS 24

SERVPAR(demo_max_number, 5, 7, MAXDEMOS - 1, "DMaximum number of demo files in RAM");
SERVPARLIST(demo_save, 0, 1, 0, endis, "DWrite demos to file");
SERVSTR(demo_path, "", 0, 63, FTXT__DEMONAME, "DDemo path (and filename) prefix");
SERVSTR(demo_filenameformat, "%w_%h_%n_%Mmin_%G", 0, 63, FTXT__FORMATSTRING, "DDemo file format string");
SERVSTR(demo_timestampformat, "%Y%m%d_%H%M", 0, 23, FTXT__FORMATSTRING, "DDemo timestamp format string");
SERVPARLIST(demo_debug, 0, 1, 0, endis, "DExcessive logging during demo recording (FIXME)");

typedef ringbuf<uchar, DEMORINGBUFSIZE> demoringbuf;
typedef vector<uchar> demobuf;
ringbuf<demoringbuf *, MAXDEMOS> demoringbuf_store;
ringbuf<demobuf *, MAXDEMOS> demobuf_store;

struct demo_s
{
    volatile int sequence;
    string info, mapname, filename;
    vector<int> clients_waiting, clients_served;
    demoringbuf *rb;    // uncomressed ringbuffer
    stream *gz, *dbs;   // compression handle
    demobuf *db;        // temp compressed file
    uchar *data;        // finished compressed file
    int len;            // bytes in data
    int gmode, timeplayed, timeremain, timestamp;
    uchar tiger[TIGERHASHSIZE]; // raw data checksum
    void *tigerstate;

                        // flags only valid for sequence > 0
    bool finish;        // game ended, finish up buffer and copy to 'data'
    bool error;         // buffering overflow, demo lost -> double DEMORINGBUFSIZE and recompile
    bool remove;        // cleanup slot
    bool done;          // demo ready to be downloaded
    bool save;          // write demo to file

    demo_s() : sequence(0), rb(NULL), gz(NULL), db(NULL), data(NULL), tigerstate(NULL) {}
};

demo_s demos[MAXDEMOS];

demo_s *initdemoslot() // fetch empty demo slot, clean up surplus slots
{
    static int demosequencecounter = 0, lastdemoslot = 0;
    const int demosequencewraparound = max(126, MAXDEMOS * 4);
    ASSERT(ismainthread() && MAXDEMOS < 126);
    for(;;)
    {
        int used = 0;
        loopi(MAXDEMOS) if(demos[i].sequence) used++;
        if(used >= demo_max_number)
        { // drop old demos (oldest sequence counters first)
            int oldestage = -1, oldestdemo;
            loopi(MAXDEMOS) if(demos[i].sequence)
            {
                int age = (demosequencecounter - demos[i].sequence + demosequencewraparound) % demosequencewraparound;
                if(age > oldestage)
                {
                    oldestage = age;
                    oldestdemo = i;
                }
            }
            if(oldestage >= 0)
            {
                demo_s &c = demos[oldestdemo];
                mlog(ACLOG_VERBOSE, "dropping demo #%d \"%s\", age %d", c.sequence, c.info, oldestage);
                if(!c.done) mlog(ACLOG_ERROR, "clearing unfinished demo slot #%d, probably leaked some memory...", c.sequence);
                else DELETEA(c.data);
                c.rb = NULL;
                c.db = NULL;
                c.data = NULL;
                c.tigerstate = NULL;
                c.gz = c.dbs = NULL;
                c.sequence = 0;
            }
        }
        else break;
    }
    demo_s *d = NULL;
    int n;
    loopi(MAXDEMOS) if(!demos[(n = (i + lastdemoslot) % MAXDEMOS)].sequence) { d = demos + n; lastdemoslot = n + 1; break; }
    ASSERT(d != NULL);
    d->clients_waiting.setsize(0);
    d->clients_served.setsize(0);
    d->rb = demoringbuf_store.length() ? demoringbuf_store.remove() : new demoringbuf;
    d->db = demobuf_store.length() ? demobuf_store.remove() : new demobuf;
    DELETEA(d->data);
    d->rb->clear();
    d->db->setsize(0);
    d->len = 0;
    d->tigerstate = tigerhash_init(d->tiger);
    d->dbs = openvecfile(d->db, false); // no autodelete: we'll keep db allocated
    d->gz = opengzfile(NULL, "wb", d->dbs);
    d->remove = d->finish = d->error = d->done = false;
    for(bool tryagain = true; tryagain; )
    { // get unique sequence number (brute force)
        demosequencecounter = (d->sequence = (demosequencecounter + 1)) % demosequencewraparound;
        tryagain = false;
        loopi(MAXDEMOS) if(demos[i].sequence == d->sequence && demos + i != d) tryagain = true;
    }
    if(demo_debug) mlog(ACLOG_INFO, "initdemoslot(): use slot %d, sequence #%d", n, d->sequence);
    return d;
}

sl_semaphore sem_demoworkerthread(1, NULL); // trigger demoworkerthread

int demoworkerthread(void *logfileprefix) // demo worker thread: compress demo data and write demo files
{
    int len;
    while(1)
    {
        loopi(MAXDEMOS) if(demos[i].sequence)
        {
            demo_s &s = demos[i];
            while(s.gz && s.rb && (len = s.rb->length()))
            { // compress until rb is empty
                uchar *rbp = s.rb->peek(&len);
                if(len != s.gz->write(rbp, len)) s.error = true;
                s.rb->remove(&len);
            }
            if(s.finish && s.gz && s.rb && !s.rb->length())
            { // close and copy zip file
                if(demo_debug) tlog(ACLOG_INFO, "demoworkerthread(): close slot %d, sequence #%d", i, s.sequence);
                tigerhash_finish(s.tiger, s.tigerstate);
                s.tigerstate = NULL;
                string msg;
                bin2hex(msg, s.tiger, TIGERHASHSIZE);
                defformatstring(text)("SERVDEMOEND<%s>", msg);
                ed25519_sign((uchar*)msg, NULL, (uchar*)text, strlen(text), ssk);
                int stamp[3] = { -1, -1, TIGERHASHSIZE + 64 };
                lilswap(stamp, 3);
                s.gz->write(stamp, sizeof(stamp));
                s.gz->write(s.tiger, TIGERHASHSIZE);
                s.gz->write(msg, 64);
                DELETEP(s.gz);
                s.len = s.dbs->size();
                s.data = new uchar[s.len];
                s.dbs->seek(0, SEEK_SET);
                s.dbs->read(s.data, s.len);
                if(s.save)
                {
                    stream *demo = openfile(s.filename, "wb");
                    if(demo)
                    {
                        int wlen = (int) demo->write(s.data, s.len);
                        delete demo;
                        tlog(ACLOG_INFO, "demo #%d written to file \"%s\" (%d bytes)", s.sequence, s.filename, wlen);
                    }
                    else
                    {
                        tlog(ACLOG_INFO, "failed to write demo #%d to file \"%s\"", s.sequence, s.filename);
                    }
                }
                else tlog(ACLOG_INFO, "recording demo #%d \"%s\" (%d bytes) finished, not saved to file", s.sequence, s.filename, s.len);
                s.done = true;
            }
            if(s.finish || s.error || s.remove)
            { // cleanup demo slot
                if(demo_debug) tlog(ACLOG_INFO, "demoworkerthread(): cleanup slot %d, sequence #%d", i, s.sequence);
                if(s.error || s.remove) s.done = false;
                DELETEP(s.gz);
                DELETEP(s.dbs);
                if(s.db)
                { // keep the demo buffer for the next recording
                    if(!demobuf_store.full()) demobuf_store.add(s.db);
                    else delete s.db;
                    s.db = NULL;
                }
                if(s.rb)
                { // keep the demo ringbuffer for the next recording
                    if(!demoringbuf_store.full()) demoringbuf_store.add(s.rb);
                    else delete s.rb;
                    s.rb = NULL;
                }
                s.finish = s.error = s.remove = false;
                if(!s.done) s.sequence = 0;
            }
        }
        sem_demoworkerthread.timedwait(500); // wait for mainloop to add some work
    }
    return 0;
}

demo_s *demorecord = NULL;

void writedemo(int chan, void *data, int len)
{
    if(demorecord && !demorecord->sequence) demorecord = NULL;
    if(!demorecord || !demorecord->rb) return;
    int stamp[3] = { sg->gamemillis, chan, len };
    if(demorecord->rb->maxsize() - demorecord->rb->length() > int(sizeof(stamp)) + len)
    {
        lilswap(stamp, 3);
        demorecord->rb->add((uchar *)stamp, sizeof(stamp));
        tigerhash_add(demorecord->tiger, (uchar *)stamp, sizeof(stamp), demorecord->tigerstate);
        demorecord->rb->add((uchar *)data, len);
        tigerhash_add(demorecord->tiger, (uchar *)data, len, demorecord->tigerstate);
        sem_demoworkerthread.post();
    }
    else
    {
        if(!demorecord->error) mlog(ACLOG_ERROR, "demorecord ringbuffer overflow -> discarding demo #%d", demorecord->sequence);
        demorecord->error = true;  // ringbuffer-overflow: discard demo
    }
}

void recordpacket(int chan, void *data, int len)
{
    if(sg->recordpackets) writedemo(chan, data, len);
}

void recordpacket(int chan, ENetPacket *packet)
{
    if(sg->recordpackets) writedemo(chan, packet->data, (int)packet->dataLength);
}

void demorecord_beginintermission()
{ // send hash to clients to sign and restart demo hash
    if(!demorecord) return;

    uchar interm_tiger[TIGERHASHSIZE];
    tigerhash_finish(demorecord->tiger, demorecord->tigerstate);
    memcpy(interm_tiger, demorecord->tiger, TIGERHASHSIZE);
    demorecord->tigerstate = tigerhash_init(demorecord->tiger); // start again to hash the rest of the demo

    if(demo_debug) mlog(ACLOG_INFO, "demorecord_beginintermission(): sequence #%d", demorecord->sequence);
    sendf(-1, 1, "rim", SV_DEMOCHECKSUM, TIGERHASHSIZE, interm_tiger);
}

void enddemorecord()
{
    if(!demorecord) return;
    sg->recordpackets = false;

    if(demo_debug) mlog(ACLOG_INFO, "enddemorecord(): sequence #%d", demorecord->sequence);
    if(sg->gamemillis < DEMO_MINTIME)
    {
        mlog(ACLOG_INFO, "Demo #%d discarded.", demorecord->sequence);
        demorecord->remove = true;
    }
    else
    { // finish recording, assemble game data,
        int len = demorecord->dbs ? demorecord->dbs->size() : 0;  // (still a few bytes missing, sue me)
        int mr = sg->gamemillis >= sg->gamelimit ? 0 : (sg->gamelimit - sg->gamemillis + 60000 - 1)/60000;
        formatstring(demorecord->info)("%s: %s, %s", asctimestr(), modestr(gamemode), sg->smapname);
        if(mr) concatformatstring(demorecord->info, ", %d mr", mr);
        defformatstring(info)("%s, %.2fMB", demorecord->info, (len + 10000) / (1024*1024.f));
        defformatstring(msg)("Demo #%d \"%s\" recorded\nPress F10 to download it from the server..", demorecord->sequence, info);
        sendservmsgverbose(msg);
        mlog(ACLOG_INFO, "Demo \"%s\" recorded.", info);

        copystring(demorecord->mapname, behindpath(sg->smapname));
        demorecord->gmode = gamemode;
        demorecord->timeplayed = sg->gamemillis >= sg->gamelimit ? sg->gamelimit/1000 : sg->gamemillis/1000;
        demorecord->timeremain = sg->gamemillis >= sg->gamelimit ? 0 : (sg->gamelimit - sg->gamemillis)/1000;
        demorecord->timestamp = int(time(NULL) / 60);
        demorecord->save = demo_save != 0;
        string buf;
        formatstring(demorecord->filename)("%s%s.dmo", demo_path, formatdemofilename(demo_filenameformat, demo_timestampformat, demorecord->mapname, gamemode, demorecord->timestamp, demorecord->timeplayed, demorecord->timeremain, servownip, buf));
        path(demorecord->filename);

        sg->demosequence = demorecord->sequence;
        demorecord->finish = true;
    }
    demorecord = NULL;
    sem_demoworkerthread.post();
}

void setupdemorecord()
{
    if(numlocalclients() || !m_mp(gamemode) || gamemode == GMODE_COOPEDIT) return;

    ASSERT(demorecord == NULL);

    demorecord = initdemoslot();
    if(!sg->demosequence) sg->demosequence = demorecord->sequence;
    formatstring(demorecord->info)("%s %s", modestr(gamemode), sg->smapname);

    sendservmsgverbose("recording demo");
    mlog(ACLOG_INFO, "Demo recording started (#%d).", demorecord->sequence);

    sg->recordpackets = false;

    demoheader hdr;
    memset(&hdr, 0, sizeof(demoheader));
    memcpy(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic));
    hdr.version = DEMO_VERSION;
    hdr.protocol = SERVER_PROTOCOL_VERSION;
    lilswap(&hdr.version, 1);
    lilswap(&hdr.protocol, 1);
    defformatstring(desc)("%s, %s, %s %s", modestr(gamemode, false), behindpath(sg->smapname), asctimestr(), sg->servdesc_current);
    if(strlen(desc) > DHDR_DESCCHARS)
        formatstring(desc)("%s, %s, %s %s", modestr(gamemode, true), behindpath(sg->smapname), asctimestr(), sg->servdesc_current);
    desc[DHDR_DESCCHARS - 1] = '\0';
    strcpy(hdr.desc, desc);
    const char *bl = "";
    loopv(clients)
    {
        client *ci = clients[i];
        if(ci->type==ST_EMPTY) continue;
        if(strlen(hdr.plist) + strlen(ci->name) < DHDR_PLISTCHARS - 2) { strcat(hdr.plist, bl); strcat(hdr.plist, ci->name); }
        bl = " ";
    }
    tigerhash_add(demorecord->tiger, (uchar *)&hdr, sizeof(demoheader), demorecord->tigerstate);
    demorecord->rb->add((uchar *)&hdr, sizeof(demoheader));

    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    welcomepacket(p, -1);
    writedemo(1, p.buf, p.len);
    if(demo_debug) mlog(ACLOG_INFO, "setupdemorecord(): sequence #%d, %s on %s", demorecord->sequence, modestr(gamemode, false), behindpath(sg->smapname));
}

int donedemosort(demo_s **a, demo_s **b) { return (*a)->timestamp - (*b)->timestamp; }

void listdemos(int cn)
{
    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    putint(p, SV_SENDDEMOLIST);
    vector<demo_s *> donedemos;
    loopi(MAXDEMOS) if(demos[i].sequence && demos[i].done) donedemos.add(demos + i);
    putint(p, donedemos.length());
    donedemos.sort(donedemosort);
    loopv(donedemos)
    {
        demo_s &d = *donedemos[i];
        putint(p, d.sequence);
        sendstring(d.info, p);
        putint(p, d.len);
        sendstring(d.mapname, p);
        putint(p, d.gmode);
        putint(p, d.timeplayed);
        putint(p, d.timeremain);
        putint(p, d.timestamp);
    }
    sendpacket(cn, 1, p.finalize());
}

static void cleardemos(int n)
{
    if(!n)
    {
        loopi(MAXDEMOS) if(demos[i].done) demos[i].remove = true;
        sendservmsg("cleared all demos");
    }
    else loopi(MAXDEMOS) if(demos[i].sequence == n && demos[i].done)
    {
        demos[i].remove = true;
        defformatstring(msg)("cleared demo #%d", n);
        sendservmsg(msg);
    }
}

void senddemo(int cn, int num)
{
    client *cl = cn>=0 ? clients[cn] : NULL;
    if(num < 1) num = sg->demosequence;
    demo_s *d = NULL;
    loopi(MAXDEMOS) if(demos[i].sequence == num) d = demos + i;
    if(d)
    {
        bool alreadywaiting = false, alreadyserved = false;
        loopv(d->clients_waiting) if(d->clients_waiting[i] == cl->clientsequence) alreadywaiting = true;
        loopv(d->clients_served) if(d->clients_served[i] == cl->clientsequence) alreadyserved = true;
        if(alreadyserved)
        {
            sendservmsg("\f3Sorry, you have already downloaded this demo.", cl->clientnum);
        }
        else if(!alreadywaiting)
        {
            d->clients_waiting.add(cl->clientsequence);
            if(!d->done)
            {
                defformatstring(msg)("scheduled download of demo #%d \"%s\"\n...download will begin, once the demo is finished recording", num, d->info);
                sendservmsg(msg, cn);
            }
        }
    }
    else
    {
        defformatstring(msg)("demo #%d not available", num);
        sendservmsg(msg, cn);
    }
}

void checkdemotransmissions()
{
    static int dpointer = 0, curcl = -1, curclseq = 0, curdemoseq = 0;//, curdemosent = 0;
    if(curcl >= 0)
    { // demo transmission in progress
        client *cl = clients[curcl];
        demo_s *d = NULL;
        loopi(MAXDEMOS) if(demos[i].sequence == curdemoseq && demos[i].done) d = demos + i;
        if(d && cl->clientsequence == curclseq)
        { // demo and client still exist: continue transmission

            // FIXME....
            packetbuf p(MAXTRANS + d->len, ENET_PACKET_FLAG_RELIABLE);
            putint(p, SV_SENDDEMO);
            putint(p, d->sequence);
            sendstring(d->mapname, p);
            putint(p, d->gmode);
            putint(p, d->timeplayed);
            putint(p, d->timeremain);
            putint(p, d->timestamp);
            putint(p, d->len);
            p.put(d->data, d->len);
            sendpacket(cl->clientnum, 2, p.finalize());

        }
        curcl = -1; // done
    }
    else
    {
        demo_s *d = demos + dpointer;
        if(d->sequence && d->done && d->clients_waiting.length())
        {
            client *cl = getclientbysequence(d->clients_waiting[0]);
            if(cl)
            {
                d->clients_served.add(d->clients_waiting.remove(0));
                curcl = cl->clientnum;
                curclseq = cl->clientsequence;
                curdemoseq = d->sequence;
                //curdemosent = 0;
            }
        }
        else dpointer = (dpointer + 1) % MAXDEMOS;
    }
}

int demoprotocol;
bool watchingdemo = false;
stream *demoplayback = NULL;
int nextplayback = 0;

void enddemoplayback()
{
    if(!demoplayback) return;
    delete demoplayback;
    demoplayback = NULL;
    watchingdemo = false;

    loopv(clients) sendf(i, 1, "risi", SV_DEMOPLAYBACK, "", i);

    sendservmsg("demo playback finished");

    loopv(clients) sendwelcome(clients[i]);
}

void setupdemoplayback()
{
    demoheader hdr;
    string msg;
    msg[0] = '\0';
    defformatstring(file)("demos/%s.dmo", sg->smapname);
    path(file);
    demoplayback = opengzfile(file, "rb");
    if(!demoplayback) formatstring(msg)("could not read demo \"%s\"", file);
    else if(demoplayback->read(&hdr, sizeof(demoheader))!=sizeof(demoheader) || memcmp(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic)))
        formatstring(msg)("\"%s\" is not a demo file", file);
    else
    {
        lilswap(&hdr.version, 1);
        lilswap(&hdr.protocol, 1);
        if(hdr.version!=DEMO_VERSION) formatstring(msg)("demo \"%s\" requires an %s version of AssaultCube", file, hdr.version<DEMO_VERSION ? "older" : "newer");
        else if(hdr.protocol != PROTOCOL_VERSION && !(hdr.protocol < 0 && hdr.protocol == -PROTOCOL_VERSION)) formatstring(msg)("demo \"%s\" requires an %s version of AssaultCube", file, hdr.protocol<PROTOCOL_VERSION ? "older" : "newer");
        demoprotocol = hdr.protocol;
    }
    if(msg[0])
    {
        if(demoplayback) { delete demoplayback; demoplayback = NULL; }
        sendservmsg(msg);
        return;
    }

    formatstring(msg)("playing demo \"%s\"", file);
    sendservmsg(msg);
    sendf(-1, 1, "risi", SV_DEMOPLAYBACK, sg->smapname, -1);
    watchingdemo = true;

    if(demoplayback->read(&nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
    {
        enddemoplayback();
        return;
    }
    lilswap(&nextplayback, 1);
}

void readdemo()
{
    if(!demoplayback) return;
    while(sg->gamemillis >= nextplayback)
    {
        int chan, len;
        if(demoplayback->read(&chan, sizeof(chan))!=sizeof(chan) ||
           demoplayback->read(&len, sizeof(len))!=sizeof(len))
        {
            enddemoplayback();
            return;
        }
        lilswap(&chan, 1);
        lilswap(&len, 1);
        ENetPacket *packet = enet_packet_create(NULL, len, 0);
        if(!packet || demoplayback->read(packet->data, len)!=len)
        {
            if(packet) enet_packet_destroy(packet);
            enddemoplayback();
            return;
        }
        sendpacket(-1, chan, packet, -1, true);
        if(!packet->referenceCount) enet_packet_destroy(packet);
        if(demoplayback->read(&nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
        {
            enddemoplayback();
            return;
        }
        lilswap(&nextplayback, 1);
    }
}

void putflaginfo(packetbuf &p, int flag)
{
    sflaginfo &f = sg->sflaginfos[flag];
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

inline void send_item_list(packetbuf &p)
{
    putint(p, SV_ITEMLIST);
    loopv(sg->sents) if(sg->sents[i].spawned) putint(p, i);
    putint(p, -1);
    if(m_flags_) loopi(2) putflaginfo(p, i);
}

#include "serverchecks.h"

bool flagdistance(sflaginfo &f, int cn)
{
    if(!valid_client(cn) || m_demo) return false;
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
    bool lagging = (c.ping > 1000 || c.spj > 100);
    if(v.x < 0 && !lagging) return true;
    float dist = c.state.o.dist(v);
    int pdist = check_pdist(&c,dist);
    if(pdist)
    {
        c.farpickups++;
        mlog(ACLOG_INFO, "[%s] %s %s the %s flag at distance %.2f (%d)",
                c.hostname, c.name, (pdist==2?"tried to touch":"touched"), team_string(&f == sg->sflaginfos + 1), dist, c.farpickups);
        if (pdist==2) return false;
    }
    return lagging ? false : true; // today I found a lag hacker :: Brahma, 19-oct-2010... lets test it a bit
}

void sendflaginfo(int flag = -1, int cn = -1)
{
    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    if(flag >= 0) putflaginfo(p, flag);
    else loopi(2) putflaginfo(p, i);
    sendpacket(cn, 1, p.finalize());
}

void flagmessage(int flag, int message, int actor, int cn = -1)
{
    if(message == FM_KTFSCORE)
        sendf(cn, 1, "riiiii", SV_FLAGMSG, flag, message, actor, (sg->gamemillis - sg->sflaginfos[flag].stolentime) / 1000);
    else
        sendf(cn, 1, "riiii", SV_FLAGMSG, flag, message, actor);
}

void flagaction(int flag, int action, int actor)
{
    if(!valid_flag(flag)) return;
    sflaginfo &f = sg->sflaginfos[flag];
    sflaginfo &of = sg->sflaginfos[team_opposite(flag)];
    bool deadactor = valid_client(actor) ? clients[actor]->state.state != CS_ALIVE || team_isspect(clients[actor]->team): true;
    int abort = 0;
    int score = 0;
    int message = -1;

    if(m_ctf || m_htf)
    {
        switch(action)
        {
            case FA_PICKUP:  // ctf: f = enemy team    htf: f = own team
            case FA_STEAL:
            {
                if(deadactor || f.state != (action == FA_STEAL ? CTFF_INBASE : CTFF_DROPPED) || !flagdistance(f, actor)) { abort = 10; break; }
                int team = team_base(clients[actor]->team);
                if(m_ctf) team = team_opposite(team);
                if(team != flag) { abort = 11; break; }
                f.state = CTFF_STOLEN;
                f.actor_cn = actor;
                message = FM_PICKUP;
                break;
            }
            case FA_LOST:
                if(actor == -1) actor = f.actor_cn;
            case FA_DROP:
                if(f.state!=CTFF_STOLEN || f.actor_cn != actor) { abort = 12; break; }
                f.state = CTFF_DROPPED;
                loopi(3) f.pos[i] = clients[actor]->state.o[i];
                message = action == FA_LOST ? FM_LOST : FM_DROP;
                break;
            case FA_RETURN:
                if(f.state!=CTFF_DROPPED || m_htf) { abort = 13; break; }
                f.state = CTFF_INBASE;
                message = FM_RETURN;
                break;
            case FA_SCORE:  // ctf: f = carried by actor flag,  htf: f = hunted flag (run over by actor)
                if(m_ctf)
                {
                    if(f.state != CTFF_STOLEN || f.actor_cn != actor || of.state != CTFF_INBASE || !flagdistance(of, actor)) { abort = 14; break; }
                    score = 1;
                    message = FM_SCORE;
                }
                else // m_htf
                {
                    if(f.state != CTFF_DROPPED || !flagdistance(f, actor)) { abort = 15; break; }
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
                if(deadactor || f.state != CTFF_INBASE || !flagdistance(f, actor)) { abort = 20; break; }
                f.state = CTFF_STOLEN;
                f.actor_cn = actor;
                f.stolentime = sg->gamemillis;
                message = FM_PICKUP;
                break;
            case FA_SCORE:  // f = carried by actor flag
                if(actor != -1 || f.state != CTFF_STOLEN) { abort = 21; break; } // no client msg allowed here
                if(valid_client(f.actor_cn) && clients[f.actor_cn]->state.state == CS_ALIVE && !team_isspect(clients[f.actor_cn]->team))
                {
                    actor = f.actor_cn;
                    score = 1;
                    message = FM_KTFSCORE;
                    break;
                }
            case FA_LOST:
                if(actor == -1) actor = f.actor_cn;
            case FA_DROP:
                if(f.actor_cn != actor || f.state != CTFF_STOLEN) { abort = 22; break; }
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
    if(abort)
    {
        mlog(ACLOG_DEBUG,"aborting flagaction(flag %d, action %d, actor %d), reason %d, resending flag states", flag, action, actor, abort);
        sendflaginfo();
        return;
    }
    if(score)
    {
        client *c = clients[actor];
        c->state.flagscore += score;
        c->incrementvitacounter(VS_FLAGS, score);
        sendf(-1, 1, "riii", SV_FLAGCNT, actor, c->state.flagscore);
    }
    if(valid_client(actor))
    {
        client &c = *clients[actor];
        switch(message)
        {
            case FM_PICKUP:
                mlog(ACLOG_INFO,"[%s] %s %s the flag", c.hostname, c.name, action == FA_STEAL ? "stole" : "picked up");
                break;
            case FM_DROP:
            case FM_LOST:
                mlog(ACLOG_INFO,"[%s] %s %s the flag", c.hostname, c.name, message == FM_LOST ? "lost" : "dropped");
                break;
            case FM_RETURN:
                mlog(ACLOG_INFO,"[%s] %s returned the flag", c.hostname, c.name);
                break;
            case FM_SCORE:
                if(m_htf)
                    mlog(ACLOG_INFO, "[%s] %s hunted the flag for %s, new score %d", c.hostname, c.name, team_string(c.team), c.state.flagscore);
                else
                   mlog(ACLOG_INFO, "[%s] %s scored with the flag for %s, new score %d", c.hostname, c.name, team_string(c.team), c.state.flagscore);
                break;
            case FM_KTFSCORE:
                mlog(ACLOG_INFO, "[%s] %s scored, carrying for %d seconds, new score %d", c.hostname, c.name, (sg->gamemillis - f.stolentime) / 1000, c.state.flagscore);
                break;
            case FM_SCOREFAIL:
                mlog(ACLOG_INFO, "[%s] %s failed to score", c.hostname, c.name);
                break;
            default:
                mlog(ACLOG_INFO, "flagaction %d, actor %d, flag %d, message %d", action, actor, flag, message);
                break;
        }
    }
    else
    {
        switch(message)
        {
            case FM_RESET:
                mlog(ACLOG_INFO,"the server reset the flag for team %s", team_string(flag));
                break;
            default:
                mlog(ACLOG_INFO, "flagaction %d with invalid actor cn %d, flag %d, message %d", action, actor, flag, message);
                break;
        }
    }

    f.lastupdate = sg->gamemillis;
    sendflaginfo(flag);
    if(message >= 0)
        flagmessage(flag, message, valid_client(actor) ? actor : -1);
}

int clienthasflag(int cn)
{
    if(m_flags_ && valid_client(cn))
    {
        loopi(2) { if(sg->sflaginfos[i].state==CTFF_STOLEN && sg->sflaginfos[i].actor_cn==cn) return i; }
    }
    return -1;
}

void ctfreset()
{
    int idleflag = m_ktf ? rnd(2) : -1;
    loopi(2)
    {
        sg->sflaginfos[i].actor_cn = -1;
        sg->sflaginfos[i].state = i == idleflag ? CTFF_IDLE : CTFF_INBASE;
        sg->sflaginfos[i].lastupdate = -1;
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
    sflaginfo &f = sg->sflaginfos[flag];
    int besthealth = 0;
    vector<int> clientnumbers;
    loopv(clients) if(clients[i]->type!=ST_EMPTY)
    {
        if(clients[i]->state.state == CS_ALIVE && team_base(clients[i]->team) == flag)
        {
            if(clients[i]->state.health == besthealth)
                clientnumbers.add(i);
            else
            {
                if(clients[i]->state.health > besthealth)
                {
                    besthealth = clients[i]->state.health;
                    clientnumbers.shrink(0);
                    clientnumbers.add(i);
                }
            }
        }
    }

    if(clientnumbers.length())
    {
        int pick = rnd(clientnumbers.length());
        client *cl = clients[clientnumbers[pick]];
        f.state = CTFF_STOLEN;
        f.actor_cn = cl->clientnum;
        sendflaginfo(flag);
        flagmessage(flag, FM_PICKUP, cl->clientnum);
        mlog(ACLOG_INFO,"[%s] %s got forced to pickup the flag", cl->hostname, cl->name);
    }
    f.lastupdate = sg->gamemillis;
}

int cmpscore(const int *a, const int *b) { return clients[*a]->at3_score - clients[*b]->at3_score; }

vector<twoint> sdistrib;

void distributeteam(int team)
{
    int numsp = 0;
    if(sg->curmap) numsp = team == 100 ? sg->curmap->entstats.spawns[2] : sg->curmap->entstats.spawns[team & 1];
    if(!numsp) numsp = 30; // no spawns: try to distribute anyway
    twoint ti;
    sdistrib.shrink(0);
    loopi(numsp)
    {
        ti.val = i;
        ti.key = rnd(0x1000000);
        sdistrib.add(ti);
    }
    sdistrib.sort(cmpintasc); // random spawn order
    loopv(clients)
    {
        clients[i]->spawnindex = sdistrib[i % sdistrib.length()].val;
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

void checkitemspawns(int);

void arenacheck()
{
    if(!m_arena || sg->interm || sg->gamemillis < sg->arenaround || !numactiveclients()) return;

    if(sg->arenaround)
    {   // start new arena round
        sg->arenaround = 0;
        sg->arenaroundstartmillis = sg->gamemillis;
        distributespawns();
        checkitemspawns(60*1000); // the server will respawn all items now
        loopv(clients) if(clients[i]->type!=ST_EMPTY && clients[i]->isauthed)
        {
            if(clients[i]->isonrightmap && team_isactive(clients[i]->team))
                sendspawn(clients[i]);
        }
        return;
    }

#ifndef STANDALONE
    if(m_botmode && clients[0]->type==ST_LOCAL)
    {
        int enemies = 0, alive_enemies = 0;
        playerent *alive = NULL;
        loopv(players) if(players[i] && (!m_teammode || players[i]->team == team_opposite(player1->team)))
        {
            enemies++;
            if(players[i]->state == CS_ALIVE)
            {
                alive = players[i];
                alive_enemies++;
            }
        }
        if(player1->state != CS_DEAD) alive = player1;
        if(enemies && (!alive_enemies || player1->state == CS_DEAD))
        {
            sendf(-1, 1, "ri2", SV_ARENAWIN, m_teammode ? (alive ? alive->clientnum : -1) : (alive && alive->type == ENT_BOT ? -2 : player1->state == CS_ALIVE ? player1->clientnum : -1));
            sg->arenaround = sg->gamemillis + 5000;
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
        if(c.type==ST_EMPTY || !c.isauthed || !c.isonrightmap || team_isspect(c.team)) continue;
        if(c.state.state==CS_ALIVE || ((c.state.state==CS_DEAD || c.state.state==CS_SPECTATE) && c.state.lastspawn>=0))
        {
            if(!alive) alive = &c;
            else if(!m_teammode || alive->team != c.team) return;
        }
        else if(c.state.state==CS_DEAD || c.state.state==CS_SPECTATE)
        {
            dead = true;
            lastdeath = max(lastdeath, c.state.lastdeath);
        }
    }

    if(sg->autoteam && m_teammode && sg->mastermode != MM_MATCH)
    {
        int *ntc = numteamclients();
        if((!ntc[0] || !ntc[1]) && (ntc[0] > 1 || ntc[1] > 1)) refillteams(true, FTR_AUTOTEAM);
    }
    if(!dead || sg->gamemillis < lastdeath + 500) return;
    sendf(-1, 1, "ri2", SV_ARENAWIN, alive ? alive->clientnum : -1);
    sg->arenaround = sg->gamemillis + 5000;
    if(sg->autoteam && m_teammode) refillteams(true);
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
         copystring(cl->lastsaytext, text);
         cl->spamcount = 0;
    }
    cl->lastsay = servmillis;
    if(cl->saychars > (SPAMCHARPERMINUTE * SPAMCHARINTERVAL) / 60)
        spam = true;
    return spam;
}

// chat message distribution matrix:
//
// /------------------------ common chat          C c C c c C C c C
// |/----------------------- RVSF chat            T
// ||/---------------------- CLA chat                 T
// |||/--------------------- spect chat             t   t t T   t T
// ||||                                           | | | | | | | | |
// ||||                                           | | | | | | | | |      C: normal chat
// ||||   team modes:                chat goes to | | | | | | | | |      T: team chat
// XX     -->   RVSF players                >-----/ | | | | | | | |      c: normal chat in all mastermodes except 'match'
// XX X   -->   RVSF spect players          >-------/ | | | | | | |      t: all chat in mastermode 'match', otherwise only team chat
// X X    -->   CLA players                 >---------/ | | | | | |
// X XX   -->   CLA spect players           >-----------/ | | | | |
// X  X   -->   SPECTATORs                  >-------------/ | | | |
// XXXX   -->   SPECTATORs (admin)          >---------------/ | | |
//        ffa modes:                                          | | |
// X      -->   any player (ffa mode)       >-----------------/ | |
// X  X   -->   any spectator (ffa mode)    >-------------------/ |
// X  X   -->   admin spectator             >---------------------/

// purpose:
//  a) give spects a possibility to chat without annoying the players (even in ffa),
//  b) no hidden messages from spects to active teams,
//  c) no spect talk to players during 'match'

void sendteamtext(char *text, int sender, int msgtype)
{
    if(!valid_client(sender) || clients[sender]->team == TEAM_NUM) return;
    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    putint(p, msgtype);
    putint(p, sender);
    sendstring(text, p);
    ENetPacket *packet = p.finalize();
    recordpacket(1, packet);
    int &st = clients[sender]->team;
    loopv(clients)
    {
        int &rt = clients[i]->team;
        if((rt == TEAM_SPECT && clients[i]->role == CR_ADMIN) ||  // spect-admin reads all
           (team_isactive(st) && st == team_group(rt)) ||         // player to own team + own spects
           (team_isspect(st) && team_isspect(rt)) ||              // spectator to other spectators
           (i == sender))                                         // "local" echo
            sendpacket(i, 1, packet);
    }
}

void sendvoicecomteam(int sound, int sender)
{
    if(!valid_client(sender) || clients[sender]->team == TEAM_NUM) return;
    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    putint(p, SV_VOICECOMTEAM);
    putint(p, sender);
    putint(p, sound);
    ENetPacket *packet = p.finalize();
    loopv(clients) if(i!=sender)
    {
        if(clients[i]->team == clients[sender]->team || !m_teammode)
            sendpacket(i, 1, packet);
    }
}

int numplayers()
{
    int count = 1;
#ifdef STANDALONE
    count = numclients();
#else
    if(m_botmode)
    {
        extern vector<botent *> bots;
        loopv(bots) if(bots[i]) count++;
    }
    if(m_demo)
    {
        count = numclients();
    }
#endif
    return count;
}

int spawntime(int type)
{
    int np = numplayers();
    np = np<3 ? 4 : (np>4 ? 2 : 3);    // Some spawn times are dependent on the number of players.
    int sec = 0;
    switch(type)
    {
    // Please update ./ac_website/htdocs/docs/introduction.html if these times change.
        case I_CLIPS:
        case I_AMMO: sec = np*2; break;
        case I_GRENADE: sec = np + 5; break;
        case I_HEALTH: sec = np*5; break;
        case I_HELMET:
        case I_ARMOUR: sec = 25; break;
        case I_AKIMBO: sec = 60; break;
    }
    return sec*1000;
}

bool serverpickup(int i, int sender)         // server side item pickup, acknowledge first client that gets it
{
    const char *hn = sender >= 0 && clients[sender]->type == ST_TCPIP ? clients[sender]->hostname : NULL;
    if(!sg->sents.inrange(i))
    {
        if(hn && !m_coop) mlog(ACLOG_INFO, "[%s] tried to pick up entity #%d - doesn't exist on this map", hn, i);
        return false;
    }
    server_entity &e = sg->sents[i];
    if(!e.spawned)
    {
        if(!e.legalpickup && hn && !m_demo) mlog(ACLOG_INFO, "[%s] tried to pick up entity #%d (%s) - can't be picked up in this gamemode or at all", hn, i, entnames[e.type]);
        return false;
    }
    if(sender>=0)
    {
        client *cl = clients[sender];
        if(cl->type==ST_TCPIP)
        {
            if(cl->state.state != CS_ALIVE || !cl->state.canpickup(e.type)) return false;
            vec v(e.x, e.y, cl->state.o.z);
            float dist = cl->state.o.dist(v);
            int pdist = check_pdist(cl,dist);
            if (pdist)
            {
                cl->farpickups++;
                if (!m_demo) mlog(ACLOG_INFO, "[%s] %s %s up entity #%d (%s), distance %.2f (%d)",
                     cl->hostname, cl->name, (pdist==2?"tried to pick":"picked"), i, entnames[e.type], dist, cl->farpickups);
                if (pdist==2) return false;
            }
        }
        sendf(-1, 1, "ri3", SV_ITEMACC, i, sender);
        cl->state.pickup(sg->sents[i].type);
        if (m_lss && sg->sents[i].type == I_GRENADE) cl->state.pickup(sg->sents[i].type); // get two nades at lss
    }
    e.spawned = false;
    if(!m_lms) e.spawntime = spawntime(e.type);
    return true;
}

void checkitemspawns(int diff)
{
    if(!diff) return;
    loopv(sg->sents) if(sg->sents[i].spawntime)
    {
        sg->sents[i].spawntime -= diff;
        if(sg->sents[i].spawntime<=0)
        {
            sg->sents[i].spawntime = 0;
            sg->sents[i].spawned = true;
            sendf(-1, 1, "ri2", SV_ITEMSPAWN, i);
        }
    }
}

void serverdamage(client *target, client *actor, int damage, int gun, bool gib, const vec &hitpush = vec(0, 0, 0))
{
    if ( m_arena && gun == GUN_GRENADE && sg->arenaroundstartmillis + 2000 > sg->gamemillis && target != actor ) return;
    clientstate &ts = target->state;
    ts.dodamage(damage, gun);
    if(damage < INT_MAX)
    {
        actor->state.damage += damage;

        sendf(-1, 1, "ri7", gib ? SV_GIBDAMAGE : SV_DAMAGE, target->clientnum, actor->clientnum, gun, damage, ts.armour, ts.health);

        if (!isteam(target->team, actor->team))
        {
            actor->incrementvitacounter(VS_DAMAGE, damage);
            actor->state.enemyfire += damage;
        }
        else
        {
            actor->incrementvitacounter(VS_FRIENDLYDAMAGE, damage);
            actor->state.friendlyfire += damage;
        }

        if(target!=actor)
        {
            if(!hitpush.iszero())
            {
                vec v(hitpush);
                if(!v.iszero()) v.normalize();
                sendf(target->clientnum, 1, "ri6", SV_HITPUSH, gun, damage,
                      int(v.x*DNF), int(v.y*DNF), int(v.z*DNF));
            }
        }
    }
    if(ts.health<=0)
    {
        int targethasflag = clienthasflag(target->clientnum);
        bool tk = false, suic = false;
        target->state.deaths++;
        target->incrementvitacounter(VS_DEATHS, 1);

        if(target!=actor)
        {
            if(!isteam(target->team, actor->team))
            {
                actor->state.frags += gib && gun != GUN_GRENADE && gun != GUN_SHOTGUN ? 2 : 1;
                actor->incrementvitacounter(VS_FRAGS, 1);
            }
            else
            {
                actor->state.frags--;
                actor->state.teamkills++;
                actor->incrementvitacounter(VS_TKS, 1);
                tk = true;
            }
        }
        else
        { // suicide
            actor->state.frags--;
            actor->state.suicides++;
            actor->incrementvitacounter(VS_SUICIDES, 1);
            suic = true;
            mlog(ACLOG_INFO, "[%s] %s suicided", actor->hostname, actor->name);
        }
        sendf(-1, 1, "ri5", gib ? SV_GIBDIED : SV_DIED, target->clientnum, actor->clientnum, actor->state.frags, gun);
        if((suic || tk) && (m_htf || m_ktf) && targethasflag >= 0)
        {
            actor->state.flagscore--;
            actor->state.antiflags++;
            actor->incrementvitacounter(VS_ANTIFLAGS, 1);
            sendf(-1, 1, "riii", SV_FLAGCNT, actor->clientnum, actor->state.flagscore);
        }
        target->position.setsize(0);
        ts.state = CS_DEAD;
        ts.lastdeath = sg->gamemillis;
        if(!suic) mlog(ACLOG_INFO, "[%s] %s %s%s %s", actor->hostname, actor->name, valid_weapon(gun) ? killmessages[gib ? 1 : 0][gun] : "smurfed", tk ? " their teammate" : "", target->name);
        if(m_flags_ && targethasflag >= 0)
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

        if(isdedicated && actor->type == ST_TCPIP && tk)
        {
            if( actor->state.frags < scl.banthreshold ||
                /** teamkilling more than 6 (defaults), more than 2 per minute and less than 4 frags per tk */
                ( actor->state.teamkills >= -scl.banthreshold &&
                  actor->state.teamkills * 30 * 1000 > sg->gamemillis &&
                  actor->state.frags < 4 * actor->state.teamkills ) )
            {
                addban(actor, DISC_AUTOBAN);
            }
            else if( actor->state.frags < scl.kickthreshold ||
                     /** teamkilling more than 5 (defaults), more than 1 tk per minute and less than 4 frags per tk */
                     ( actor->state.teamkills >= -scl.kickthreshold &&
                       actor->state.teamkills * 60 * 1000 > sg->gamemillis &&
                       actor->state.frags < 4 * actor->state.teamkills ) ) disconnect_client(actor->clientnum, DISC_AUTOKICK);
        }
    } else if ( target!=actor && isteam(target->team, actor->team) ) check_ffire (target, actor, damage); // friendly fire counter
}

#include "serverevents.h"

bool updatedescallowed(void) { return scl.servdesc_pre[0] || scl.servdesc_suf[0]; }

void updatesdesc(const char *newdesc, ENetAddress *caller = NULL)
{
    if(!newdesc || !newdesc[0] || !updatedescallowed())
    {
        copystring(sg->servdesc_current, scl.servdesc_full);
        sg->custom_servdesc = false;
    }
    else
    {
        formatstring(sg->servdesc_current)("%s%s%s", scl.servdesc_pre, newdesc, scl.servdesc_suf);
        sg->custom_servdesc = true;
        if(caller) sg->servdesc_caller = *caller;
    }
}

int canspawn(client *c)   // beware: canspawn() doesn't check m_arena!
{
    // more readable code; TODO clear up intention of especially "failwild" here and more generally the use of TEAM_ANYACTIVE
    bool failclientbasics = (!c || c->type == ST_EMPTY || !c->isauthed);
    bool failteam = !(c->team == TEAM_ANYACTIVE || team_isvalid(c->team));//TEST WAS JUST:!team_isvalid(c->team);//but since the code is calling updateclientteam(BLAH, TEAM_ANYACTIVE, BLAH) all over the place..
    bool failwait = false;
    bool failwild = false;
    if( c->type == ST_TCPIP ){
        failwait = ( c->state.lastdeath > 0 ? sg->gamemillis - c->state.lastdeath : servmillis - c->connectmillis ) < (m_arena ? 0 : m_flags_ ? 5000 : 2000);
        // failwild: this one contains a number of aspects, it's a /wild/ conglomeration.
        if( totalclients > 3 ){
                bool c1 = servmillis - c->connectmillis < 1000 + c->state.reconnections * 2000; // seems to delay respawn if you've reconnected too often
                bool c2 = sg->gamemillis > 10000; // only allow after 10 seconds of gametime
                //.. c3 = totalclients > 3; // all of this only applies if there are four or more players
                bool c4 = !team_isspect(c->team); // all of this only applies if the designated team is not a spectator team
                failwild = c1 && c2 && c4; // && c3
        }
    }
    if(failclientbasics || failteam || failwait || failwild ) return SP_OK_NUM; // equivalent to SP_DENY
    if(!c->isonrightmap) return SP_WRONGMAP;
    if(sg->mastermode == MM_MATCH && sg->matchteamsize)
    {
        if(c->team == TEAM_SPECT || (team_isspect(c->team) && !m_teammode)) return SP_SPECT;
        if(c->team == TEAM_CLA_SPECT || c->team == TEAM_RVSF_SPECT)
        {
            if(numteamclients()[team_base(c->team)] >= sg->matchteamsize) return SP_SPECT;
            else return SP_REFILLMATCH;
        }
    }
    return SP_OK;
}

void autospawncheck()
{
    if(sg->mastermode != MM_MATCH || !m_autospawn || sg->interm) return;

    loopv(clients) if(clients[i]->type!=ST_EMPTY && clients[i]->isauthed && team_isactive(clients[i]->team))
    {
        client *cl = clients[i];
        if((cl->state.state == CS_DEAD || cl->state.state == CS_SPECTATE)
            && canspawn(cl) == SP_OK && !cl->autospawn)
        {
            sendspawn(cl);
            cl->autospawn = true;
        }
    }
}

bool updateclientteam(int cln, int newteam, int ftr)
{
    if(!valid_client(cln)) return false;
    if(!team_isvalid(newteam) && newteam != TEAM_ANYACTIVE) newteam = TEAM_SPECT;
    client &cl = *clients[cln];
    if(cl.team == newteam && ftr != FTR_AUTOTEAM) return true; // no change
    int *teamsizes = numteamclients(cln);
    if( sg->mastermode == MM_OPEN && cl.state.forced && ftr == FTR_PLAYERWISH && newteam < TEAM_SPECT && team_base(cl.team) != team_base(newteam) ) return false; // player forced to team => deny wish to change
    if(newteam == TEAM_ANYACTIVE) // when spawning from spect – 20210601: or just after dying in a deathmatch .. apparently.
    {
        if(sg->mastermode == MM_MATCH && cl.team < TEAM_SPECT)
        {
            newteam = team_base(cl.team);
        }
        else
        {
            // join (autoteam => smaller || balanced => weaker(*)) team. (*): if theres enough data to establish strength, otherwise fallback to random
            if(sg->autoteam && teamsizes[TEAM_CLA] != teamsizes[TEAM_RVSF]) newteam = teamsizes[TEAM_CLA] < teamsizes[TEAM_RVSF] ? TEAM_CLA : TEAM_RVSF;
            else
            {
                int teamscore[2] = {0, 0}, sum = calcscores(); // sum != 0 is either <0 if match data based or >0 vita based and exceeded shuffleteamthreshold
                loopv(clients) if(clients[i]->type!=ST_EMPTY && i != cln && clients[i]->isauthed && clients[i]->team != TEAM_SPECT)
                {
                    teamscore[team_base(clients[i]->team)] += clients[i]->at3_score;
                }
                newteam = sum==0 ? rnd(2) : (teamscore[TEAM_CLA] < teamscore[TEAM_RVSF] ? TEAM_CLA : TEAM_RVSF);
            }
        }
    }
    if(ftr == FTR_PLAYERWISH)
    {
        if(sg->mastermode == MM_MATCH && sg->matchteamsize && m_teammode)
        {
            if(newteam != TEAM_SPECT && (team_base(newteam) != team_base(cl.team) || !m_teammode)) return false; // no switching sides in match mode when teamsize is set
        }
        if(team_isactive(newteam))
        {
            if(!m_teammode && cl.state.state == CS_ALIVE) return false;  // no comments
            if(sg->mastermode == MM_MATCH)
            {
                if(m_teammode && sg->matchteamsize && teamsizes[newteam] >= sg->matchteamsize) return false;  // ensure maximum team size
            }
            else
            {
                if(m_teammode && sg->autoteam && teamsizes[newteam] > teamsizes[team_opposite(newteam)]) return false; // don't switch to an already bigger team
            }
        }
        else if(sg->mastermode != MM_MATCH || !m_teammode) newteam = TEAM_SPECT; // only match mode (team) has more than one spect team
    }
    if(cl.team == newteam && ftr != FTR_AUTOTEAM) return true; // no change
    if(cl.team != newteam) sdropflag(cl.clientnum);
    if(ftr != FTR_INFO && (team_isspect(newteam) || (team_isactive(newteam) && team_isactive(cl.team)))) forcedeath(&cl);
    sendf(-1, 1, "riii", SV_SETTEAM, cln, newteam | ((ftr == FTR_SILENTFORCE ? FTR_INFO : ftr) << 4));
    if(ftr != FTR_INFO && !team_isspect(newteam) && team_isspect(cl.team)) sendspawn(&cl);
    if (team_isspect(newteam)) {
        cl.state.state = CS_SPECTATE;
        cl.state.lastdeath = sg->gamemillis;
        if(m_flags_ && clienthasflag(cl.clientnum) >= 0) // switching to spectate while holding the flag punishment
        {
            cl.state.antiflags++;
            cl.incrementvitacounter(VS_ANTIFLAGS, 1);
            if(m_htf || m_ktf){
                cl.state.flagscore--;
                sendf(-1, 1, "riii", SV_FLAGCNT, cl.clientnum, cl.state.flagscore);
            }
        }
    }
    cl.team = newteam;
    return true;
}

int calcscoresvita() // vita skill eval
{
    int sum = 0;
    loopv(clients) if(clients[i]->type!=ST_EMPTY)
    {
        if(clients[i]->vita && clients[i]->vita->vs[VS_MINUTESACTIVE] > 0)
        {
            int *vs = clients[i]->vita->vs;
            int cskill = ((vs[VS_FRAGS]*100+vs[VS_DAMAGE]*2+vs[VS_FLAGS]*10) - (vs[VS_DEATHS]*50+vs[VS_FRIENDLYDAMAGE]*3+vs[VS_ANTIFLAGS]*15+vs[VS_TKS]*100+vs[VS_SUICIDES]*100))/vs[VS_MINUTESACTIVE];
            clients[i]->at3_score = cskill;
            sum += cskill; // vita skill sum will be >=0
        }
    }
    return sum;
}

int calcscoresmatch() // match skill eval
{
    int sum = 0;
    int nsp = 0; // no score players
    loopv(clients) if(clients[i]->type!=ST_EMPTY)
    {
        if(clients[i]->team < TEAM_CLA_SPECT)
        {
            clientstate &cs = clients[i]->state;
            int cskill = (cs.frags*100 + cs.enemyfire*2 + cs.goodflags*10) - (cs.deaths*50 + cs.friendlyfire*3 + cs.antiflags*15 + cs.teamkills*100 + cs.suicides*100);
            clients[i]->at3_score = cskill;
            sum -= cskill; // match skill sum will be <=0
            if(cskill < 100) nsp++;
        }
    }
    if(sum > -100 * numplayers() || nsp >= numplayers()/2) return 0; // if at least half have tiny scores or the total isn't enough: indicate random seems best
    return sum;
}

int calcscores()
{
    int sum = 0;
    int vitasum = calcscoresvita();
    if((vitasum / numplayers()) > shuffleskillthreshold)
    {
        sum = vitasum;
    }
    else
    {
        sum = calcscoresmatch(); // indicate match skill used by sum<=0
    }
    return sum;
}

vector<int> shuffle;

void shuffleteams(int ftr = FTR_AUTOTEAM)
{
    int team; // top ranked player will stay on same team
    int sum = calcscores();

    // 20220109 only try skill based if each player has more than 200 on average. competitive players against each other currently have ~=1000.
    if(sum==0)
    {
        sendservmsg("using randomness to shuffle teams");
        // random
        int teamsize[2] = {0, 0};
        int half = numplayers()/2;
        int lastrun = 9; // try to avoid alternating teams based on clientnum
        vector<int> done;
        loopv(clients){
            done.add(!(clients[i]->type!=ST_EMPTY && clients[i]->isonrightmap && !team_isspect(clients[i]->team))); // only shuffle active players - team_isactive() does not include TEAM_ANYACTIVE
        }
        loopk(lastrun+1)
        {
            loopv(clients) if(!done[i])
            {
                if(k==lastrun || rnd(lastrun+1)==lastrun)
                {
                    done[i] = true;
                    team = rnd(2);
                    if(teamsize[team]>=half) team = team_opposite(team);
                    updateclientteam(i, team, ftr);
                    teamsize[team]++;
                }
            }
        }
    }
    else
    {
        defformatstring(skillused)("using %s skill data to shuffle teams", sum<0?"match":"vita"); sendservmsg(skillused);
        // skill based
        shuffle.shrink(0);
        loopv(clients) if(clients[i]->type!=ST_EMPTY && clients[i]->isonrightmap && !team_isspect(clients[i]->team))
        {
            shuffle.add(i);
        }else{
            clients[i]->at3_score = 0; 
        }
        shuffle.sort(cmpscore);
        team = !clients[shuffle[0]]->team; // top player will stay on same team
        int weakest2weakest = -1 + ((shuffle.length()%2) ? shuffle.length(): 0);
        loopi(shuffle.length())
        {
            if(i!=weakest2weakest) team = !team; // weakest player will be on "weaker" team if uneven player count
            updateclientteam(shuffle[i], team, ftr);
        }
    }

    if(m_ctf || m_htf)
    {
        ctfreset();
        sendflaginfo();
    }
}

bool refillteams(bool now, int ftr)  // force only minimal amounts of players
{
    if(sg->mastermode == MM_MATCH) return false;
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
                }
            }
        }
    }
    int bigteam = teamsize[1] > teamsize[0];
    int allplayers = teamsize[0] + teamsize[1];
    int diffnum = teamsize[bigteam] - teamsize[!bigteam];
    int diffscore = teamscore[bigteam] - teamscore[!bigteam];
    if(lasttime_eventeams > sg->gamemillis) lasttime_eventeams = 0;
    if(diffnum > 1)
    {
        if(now || sg->gamemillis - lasttime_eventeams > 8000 + allplayers * 1000 || diffnum > 2 + allplayers / 10)
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
                    int fit = targetscore;
                    if(fit < 0 ) fit = -(fit * 15) / 10;       // avoid too good players
                    int forcedelay = clients[i]->at3_lastforce ? (1000 - (sg->gamemillis - clients[i]->at3_lastforce) / (5 * 60)) : 0;
                    if(forcedelay > 0) fit += (fit * forcedelay) / 600;   // avoid lately forced players
                    if(fit < bestfit + fit * rnd(100) / 400)   // search 'almost' best fit
                    {
                        bestfit = fit;
                        pick = i;
                    }
                }
                if(pick < 0) break; // should really never happen
                // move picked player
                clients[pick]->at3_dontmove = true;
                moveable[bigteam]--;
                if(updateclientteam(pick, !bigteam, ftr))
                {
                    diffnum -= 2;
                    diffscore -= 2 * clients[pick]->at3_score;
                    clients[pick]->at3_lastforce = sg->gamemillis;  // try not to force this player again for the next 5 minutes
                    switched = true;
                }
            }
        }
    }
    if(diffnum < 2)
    {
        lasttime_eventeams = sg->gamemillis;
    }
    return switched;
}

void resetserver(const char *newname, int newmode, int newtime)
{
    if(m_demo) enddemoplayback();
    else enddemorecord();

    sg->free(); // for now: clean out old servergame states
    sg->curmap = NULL;

    sg->smode = newmode;
    copystring(sg->smapname, newname);
    sg->srvgamesalt = (rnd(0x1000000)*((servmillis%4200)+1)) ^ rnd(0x1000000);

    sg->minremain = newtime > 0 ? newtime : defaultgamelimit(newmode);
    sg->gamemillis = 0;
    sg->gamelimit = sg->minremain * 60000;
    sg->arenaround = sg->arenaroundstartmillis = 0;

    sg->interm = sg->nextsendscore = 0;
    sg->lastfillup = servmillis;
    sg->sents.shrink(0);
    if(sg->mastermode == MM_PRIVATE)
    {
        loopv(savedscores) savedscores[i].valid = false;
    }
    else savedscores.shrink(0);
    ctfreset();

    sg->sispaused = false;
    sg->nextmapname[0] = '\0';
    sg->forceintermission = false;
}

void startdemoplayback(const char *newname)
{
    if(isdedicated) return;
    resetserver(newname, GMODE_DEMO, -1);
    setupdemoplayback();
}

void startgame(const char *newname, int newmode, int newtime, bool notify)
{
    if(!newname || !*newname || (newmode == GMODE_DEMO && isdedicated)) fatal("startgame() abused");
    if(newmode == GMODE_DEMO)
    {
        startdemoplayback(newname);
    }
    else
    {
        bool lastteammode = m_teammode;
        resetserver(newname, newmode, newtime);   // beware: may clear *newname

        if(sg->custom_servdesc && findcnbyaddress(&sg->servdesc_caller) < 0)
        {
            updatesdesc(NULL);
            if(notify)
            {
                sendservmsg("server description reset to default");
                mlog(ACLOG_INFO, "server description reset to '%s'", sg->servdesc_current);
            }
        }

        servermap *sm = NULL;
        if(isdedicated)
        { // dedicated server: only use pre-loaded maps
            sm = getservermap(sg->smapname);
        }
#ifndef STANDALONE
        else
        { // local server: load map temporarily
            static servermap *localmap = NULL;
            DELETEP(localmap);
            const char *lpath = checklocalmap(sg->smapname);
            if(lpath) localmap = new servermap(sg->smapname, lpath);
            if(localmap)
            {
                localmap->load();
                if(localmap->isok)
                {
                    sm = localmap;
                    if(!(sm->entstats.modes_possible & 1 << sg->smode)) conoutf("\f3map %s does not support game mode %s", sm->fname, fullmodestr(sg->smode));
                }
                else conoutf("\f3local server failed to load map \"%s%s\", error: %s", lpath, sg->smapname, localmap->err);
            }
        }
#endif
        sg->curmap = sm;
        if(sm)
        {
            sg->layout = sm->getlayout();  // get floorplan

            loopi(2)
            {
                sflaginfo &f = sg->sflaginfos[i];
                if(sg->curmap->entstats.hasflags)   // don't check flag positions, if there is more than one flag per team
                {
                    f.x = sg->curmap->entpos_x[sg->curmap->entstats.flagents[i]];
                    f.y = sg->curmap->entpos_y[sg->curmap->entstats.flagents[i]];
                }
                else f.x = f.y = -1;
                f.actor_cn = -1;
            }
            entity e;
            loopi(sg->curmap->numents)
            {
                e.type = sg->curmap->enttypes[i];
                e.transformtype(sg->smode);
                server_entity se = { e.type, false, false, false, 0, sg->curmap->entpos_x[i], sg->curmap->entpos_y[i] };
                sg->sents.add(se);
                if(e.fitsmode(sg->smode)) sg->sents[i].spawned = sg->sents[i].legalpickup = true;
            }
        }
        else if(isdedicated && sg->smode != GMODE_COOPEDIT) fatal("servermap not found");       // FIXME: coop should also start with a map
        if(notify)
        {
            // change map
            sendf(-1, 1, "risiiii", SV_MAPCHANGE, sg->smapname, sg->smode, sm && sm->isdistributable() ? sm->cgzlen : 0, sm && sm->isdistributable() ? sm->maprevision : 0, sg->srvgamesalt);
            if(sg->smode > 1 || (sg->smode == 0 && numnonlocalclients() > 0)) sendf(-1, 1, "ri3", SV_TIMEUP, sg->gamemillis, sg->gamelimit);
        }
        packetbuf q(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        send_item_list(q); // always send the item list when a game starts
        sendpacket(-1, 1, q.finalize());
        defformatstring(gsmsg)("Game start: %s on %s, %d players, %d minutes, mastermode %d, ", modestr(sg->smode), sg->smapname, numclients(), sg->minremain, sg->mastermode);
        if(sg->mastermode == MM_MATCH) concatformatstring(gsmsg, "teamsize %d, ", sg->matchteamsize);
        if(sm) concatformatstring(gsmsg, "(map rev %d/%d, %s, 'getmap' %sprepared)", sm->maprevision, sm->cgzlen, sm->getpathdesc(), sm->isdistributable() ? "" : "not ");
        else concatformatstring(gsmsg, "error: failed to preload map");
        mlog(ACLOG_INFO, "\n%s", gsmsg);
        if(m_arena) distributespawns();
        if(notify)
        {
            // shuffle if previous mode wasn't a team-mode
            if(m_teammode)
            {
                if(!lastteammode)
                    shuffleteams(FTR_INFO);
                else if(sg->autoteam)
                    refillteams(true, FTR_INFO);
            }
            // prepare spawns; players will spawn, once they've loaded the correct map
            loopv(clients) if(clients[i]->type!=ST_EMPTY)
            {
                client *c = clients[i];
                c->mapchange();
                forcedeath(c);
            }
        }
        if(numnonlocalclients() > 0) setupdemorecord();
        if(notify && m_ktf) sendflaginfo();
        if(notify) senddisconnectedscores(-1);
    }
}

struct gbaninfo
{
    enet_uint32 ip, mask;
};

vector<gbaninfo> gbans;

void cleargbans()
{
    gbans.shrink(0);
}

bool checkgban(uint ip)
{
    loopv(gbans) if((ip & gbans[i].mask) == gbans[i].ip) return true;
    return false;
}

void addgban(const char *name)
{
    union { uchar b[sizeof(enet_uint32)]; enet_uint32 i; } ip, mask;
    ip.i = 0;
    mask.i = 0;
    loopi(4)
    {
        char *end = NULL;
        int n = strtol(name, &end, 10);
        if(!end) break;
        if(end > name) { ip.b[i] = n; mask.b[i] = 0xFF; }
        name = end;
        while(*name && *name++ != '.');
    }
    gbaninfo &ban = gbans.add();
    ban.ip = ip.i;
    ban.mask = mask.i;

    loopvrev(clients)
    {
        client &c = *clients[i];
        if(c.type!=ST_TCPIP) continue;
        if(checkgban(c.peer->address.host)) disconnect_client(c.clientnum, DISC_BANNED);
    }
}

inline void addban(client *cl, int reason, int type)
{
    if(!cl) return;
    ban b = { cl->peer->address, servmillis+scl.ban_time, type };
    bans.add(b);
    disconnect_client(cl->clientnum, reason);
}

int getbantype(int cn)
{
    if(!valid_client(cn)) return BAN_NONE;
    client &c = *clients[cn];
    if(c.type==ST_LOCAL) return BAN_NONE;
    if(c.checkvitadate(VS_BAN)) return BAN_VITA;
    if(checkgban(c.peer->address.host)) return BAN_MASTER;
    if(ipblacklist.check(c.peer->address.host)) return BAN_BLACKLIST;
    loopv(bans)
    {
        ban &b = bans[i];
        if(b.millis < servmillis) { bans.remove(i--); }
        if(b.address.host == c.peer->address.host) return b.type;
    }
    return BAN_NONE;
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
    int boot;
    int owner, callmillis, result, num1, num2, type;
    char text[MAXTRANS];
    serveraction *action;
    bool gonext;
    enet_uint32 host;

    voteinfo() : boot(0), owner(0), callmillis(0), result(VOTE_NEUTRAL), action(NULL), gonext(false), host(0) {}
    ~voteinfo() { delete action; }

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
    bool isalive() { return servmillis - callmillis < 30*1000; }

    void evaluate(bool forceend = false)
    {
        if(result!=VOTE_NEUTRAL) return; // block double action
        if(action && !action->isvalid()) end(VOTE_NO);
        int stats[VOTE_NUM] = {0};
        int adminvote = VOTE_NEUTRAL;
        loopv(clients)
            if(clients[i]->type!=ST_EMPTY /*&& clients[i]->connectmillis < callmillis*/) // new connected people will vote now
            {
                stats[clients[i]->vote]++;
                if(clients[i]->role==CR_ADMIN) adminvote = clients[i]->vote;
            };

        bool admin = clients[owner]->role==CR_ADMIN || (!isdedicated && clients[owner]->type==ST_LOCAL);
        int total = stats[VOTE_NO]+stats[VOTE_YES]+stats[VOTE_NEUTRAL];
        const float requiredcount = 0.51f;
        bool min_time = servmillis - callmillis > 10*1000;
#define yes_condition ((min_time && stats[VOTE_YES] - stats[VOTE_NO] > 0.34f*total && totalclients > 4) || stats[VOTE_YES] > requiredcount*total)
#define no_condition (forceend || !valid_client(owner) || stats[VOTE_NO] >= stats[VOTE_YES]+stats[VOTE_NEUTRAL] || adminvote == VOTE_NO)
#define boot_condition (!boot || (boot && valid_client(num1) && clients[num1]->peer->address.host == host))
        if( (yes_condition || admin || adminvote == VOTE_YES) && boot_condition ) end(VOTE_YES);
        else if( no_condition || (min_time && !boot_condition)) end(VOTE_NO);
        else return;
#undef boot_condition
#undef no_condition
#undef yes_condition
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
        mlog(ACLOG_DEBUG,"[%s] client %s voted %s", clients[sender]->hostname, clients[sender]->name, vote == VOTE_NO ? "no" : "yes");
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
    clients[v->owner]->nvotes--; // successful votes do not count as abuse
    sendf(v->owner, 1, "ri", SV_CALLVOTESUC);
    mlog(ACLOG_INFO, "[%s] client %s called a vote: %s", clients[v->owner]->hostname, clients[v->owner]->name, v->action && *v->action->desc ? v->action->desc : "[unknown]");
}

void scallvoteerr(voteinfo *v, int error)
{
    if(!valid_client(v->owner)) return;
    sendf(v->owner, 1, "ri2", SV_CALLVOTEERR, error);
    if(v->type == SA_SHUFFLETEAMS && !m_teammode) sendservmsg("\f3shuffle teams requires teammode", v->owner); // 20220119: w/o changing protocol easiest feedback about why the "invalid vote" error was raised.
    mlog(ACLOG_INFO, "[%s] client %s failed to call a vote: %s (%s)", clients[v->owner]->hostname, clients[v->owner]->name, v->action && *v->action->desc ? v->action->desc : "[unknown]", voteerrorstr(error));
}

bool map_queued = false;
void callvotepacket (int, voteinfo *);

bool scallvote(voteinfo *v, ENetPacket *msg) // true if a regular vote was called
{
    if (!v) return false;
    int area = isdedicated ? EE_DED_SERV : EE_LOCAL_SERV;
    int error = -1;
    client *c = clients[v->owner], *b = ( v->boot && valid_client(cn2boot) ? clients[cn2boot] : NULL );
    v->host = v->boot && b ? b->peer->address.host : 0;

    int time = servmillis - c->lastvotecall;
    if ( c->nvotes > 0 && time > 4*60*1000 ) c->nvotes -= time/(4*60*1000);
    if ( c->nvotes < 0 || c->role == CR_ADMIN ) c->nvotes = 0;
    c->nvotes++;


    if( !v || !v->isvalid() || (v->boot && (!b || cn2boot == v->owner) ) ) error = VOTEE_INVALID;
    else if( v->action->role > c->role ) error = VOTEE_PERMISSION;
    else if( !(area & v->action->area) ) error = VOTEE_AREA;
    else if( curvote && curvote->result==VOTE_NEUTRAL ) error = VOTEE_CUR;
    else if( v->type == SA_MAP && v->num1 >= GMODE_NUM && map_queued ) error = VOTEE_NEXT;
    else if( c->role == CR_DEFAULT && v->action->isdisabled() ) error = VOTEE_DISABLED;
    else if( (c->lastvotecall && servmillis - c->lastvotecall < 60*1000 && c->role != CR_ADMIN && numclients()>1) || c->nvotes > 3 ) error = VOTEE_MAX;
    else if( ( ( v->boot == 1 && c->role < roleconf('w') ) || ( v->boot == 2 && c->role < roleconf('X') ) )
                  && !is_lagging(b) && !b->mute && b->spamcount < 2 )
    {
        /** not same team, with low ratio, not lagging, and not spamming... so, why to kick? */
        if ( !isteam(c->team, b->team) && b->state.frags < ( b->state.deaths > 0 ? b->state.deaths : 1 ) * 3 ) error = VOTEE_WEAK;
        /** same team, with low tk, not lagging, and not spamming... so, why to kick? */
        else if ( isteam(c->team, b->team) && b->state.teamkills < c->state.teamkills ) error = VOTEE_WEAK;
    }
    //else if( c->role == CR_DEFAULT && servmillis - c->connectmillis < 60000 && numclients() > 1 ) error = VOTEE_PERMISSION; // after connection 60s delay before possibility of vote start // VOTE_PERMISSION is misleading but reenable modified if misuse occurs

    if(error>=0)
    {
        scallvoteerr(v, error);
        return false;
    }
    else
    {
        if ( v->type == SA_MAP && v->num1 >= GMODE_NUM ) map_queued = true;
        if (!v->gonext) sendpacket(-1, 1, msg, v->owner); // FIXME in fact, all votes should go to the server, registered, and then go back to the clients
        else callvotepacket (-1, v);                      // also, no vote should exclude the caller... these would provide many code advantages/facilities
        scallvotesuc(v);                                  // but we cannot change the vote system now for compatibility issues... so, TODO
        return true;
    }
}

void callvotepacket (int cn, voteinfo *v = curvote)
{ // FIXME, it would be far smart if the msg buffer from SV_CALLVOTE was simply saved
    int n_yes = 0, n_no = 0;
    loopv(clients) if(clients[i]->type!=ST_EMPTY)
    {
        if ( clients[i]->vote == VOTE_YES ) n_yes++;
        else if ( clients[i]->vote == VOTE_NO ) n_no++;
    }

    packetbuf q(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    putint(q, SV_CALLVOTE);
    putint(q, -1);
    putint(q, v->owner);
    putint(q, n_yes);
    putint(q, n_no);
    putint(q, v->type);
    switch(v->type)
    {
        case SA_KICK:
        case SA_BAN:
            putint(q, v->num1);
            sendstring(v->text, q);
            break;
        case SA_MAP:
            sendstring(v->text, q);
            putint(q, v->num1);
            putint(q, v->num2);
            break;
        case SA_SERVERDESC:
            sendstring(v->text, q);
            break;
        case SA_STOPDEMO:
            // compatibility
            break;
        case SA_REMBANS:
        case SA_SHUFFLETEAMS:
            break;
        case SA_FORCETEAM:
            putint(q, v->num1);
            putint(q, v->num2);
            break;
        case SA_PAUSE:
            putint(q, v->num1);
            break;
        default:
            putint(q, v->num1);
            break;
    }
    sendpacket(cn, 1, q.finalize());
}

void changeclientrole(int client, int role, char *pwd, bool force)
{
    pwddetail pd;
    bool success = false;
    if(!isdedicated || !valid_client(client)) return;
    pd.line = -1;
    if(role == clients[client]->role) return;
    if(role == CR_ADMIN && pwd && pwd[0] && passwords.check(clients[client]->name, pwd, clients[client]->salt, &pd) && !pd.denyadmin)
    {
        mlog(ACLOG_INFO,"[%s] player %s used admin password in line %d", clients[client]->hostname, clients[client]->name[0] ? clients[client]->name : "[unnamed]", pd.line);
        success = true;
    }
    if(clients[client]->checkvitadate(VS_ADMIN))
    {
        mlog(ACLOG_INFO,"[%s] player %s claimed admin through valid vita claim (pubkey: %s)", clients[client]->hostname, clients[client]->name[0] ? clients[client]->name : "[unnamed]", clients[client]->pubkeyhex);
        success = true;
    }
    if(force || role == CR_DEFAULT || success)
    {
        if(role > CR_DEFAULT)
        {
            loopv(clients) clients[i]->role = CR_DEFAULT;
        }
        clients[client]->role = role;
        sendserveropinfo(-1);
        mlog(ACLOG_INFO,"[%s] set role of player %s to %s", clients[client]->hostname, clients[client]->name[0] ? clients[client]->name : "[unnamed]", role == CR_ADMIN ? "admin" : "normal player"); // flowtron : connecting players haven't got a name yet (connectadmin)
        if(role > CR_DEFAULT) sendiplist(client);
    }
    else if(pwd && pwd[0]) disconnect_client(client, DISC_SOPLOGINFAIL); // avoid brute-force
    if(curvote) curvote->evaluate();
}

void senddisconnectedscores(int cn)
{
    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    putint(p, SV_DISCSCORES);
    if(sg->mastermode == MM_MATCH)
    {
        loopv(savedscores)
        {
            savedscore &sc = savedscores[i];
            if(sc.valid)
            {
                putint(p, sc.team);
                sendstring(sc.name, p);
                putint(p, sc.flagscore);
                putint(p, sc.frags);
                putint(p, sc.deaths);
            }
        }
    }
    putint(p, -1);
    sendpacket(cn, 1, p.finalize());
}

void disconnect_client(int n, int reason)
{
    ASSERT(ismainthread());
    if(!clients.inrange(n) || clients[n]->type!=ST_TCPIP) return;
    sdropflag(n);
    client &c = *clients[n];
    c.state.lastdisc = servmillis;
    const char *scoresaved = "";
    if(c.haswelcome)
    {
        savedscore *sc = findscore(c, true);
        if(sc)
        {
            sc->save(c.state, c.team);
            scoresaved = ", score saved";
        }
    }
    int sp = (servmillis - c.connectmillis) / 1000;
    if(reason>=0) mlog(ACLOG_INFO, "[%s] disconnecting client %s (%s) cn %d, %d seconds played%s", c.hostname, c.name, disc_reason(reason), n, sp, scoresaved);
    else mlog(ACLOG_INFO, "[%s] disconnected client %s cn %d, %d seconds played%s", c.hostname, c.name, n, sp, scoresaved);
    totalclients--;
    c.peer->data = (void *)-1;
    if(reason>=0) enet_peer_disconnect(c.peer, reason);
    clients[n]->zap();
    sendf(-1, 1, "rii", SV_CDIS, n);
    if(curvote) curvote->evaluate();
    if(*scoresaved && sg->mastermode == MM_MATCH) senddisconnectedscores(-1);
}

void sendiplist(int receiver, int cn)
{
    if(!valid_client(receiver)) return;
    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    putint(p, SV_IPLIST);
    loopv(clients) if(valid_client(i) && clients[i]->type == ST_TCPIP && i != receiver
        && (clients[i]->clientnum == cn || cn == -1))
    {
        putint(p, i);
        putint(p, clients[i]->ip);
    }
    putint(p, -1);
    sendpacket(receiver, 1, p.finalize());
}

void sendresume(client &c, bool broadcast)
{
    sendf(broadcast ? -1 : c.clientnum, 1, "ri3i8ivvi", SV_RESUME,
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
            c.state.teamkills,
            NUMGUNS, c.state.ammo,
            NUMGUNS, c.state.mag,
            -1);
}

bool restorescore(client &c)
{
    //if(ci->local) return false;
    savedscore *sc = findscore(c, false);
    if(sc && sc->valid)
    {
        sc->restore(c.state);
        sc->valid = false;
        if ( c.connectmillis - c.state.lastdisc < 5000 ) c.state.reconnections++;
        else if ( c.state.reconnections ) c.state.reconnections--;
        return true;
    }
    return false;
}

void sendservinfo(client &c)
{
    sendf(c.clientnum, 1, "ri6IIIi3k", SV_SERVINFO, c.clientnum, isdedicated ? SERVER_PROTOCOL_VERSION : PROTOCOL_VERSION, c.needsauth ? 1 : 0, scl.serverpassword[0] ? 1 : 0, c.salt,
        servownip, c.ip, c.ip_censored, (c.connectclock = servclock), c.country[0], c.country[1], spk);
}

void putinitclient(client &c, packetbuf &p)
{
    putint(p, SV_INITCLIENT);
    putint(p, c.clientnum);
    sendstring(c.name, p);
    putint(p, c.skin[TEAM_CLA]);
    putint(p, c.skin[TEAM_RVSF]);
    putint(p, c.team);
    putint(p, c.maxroll);
    putint(p, c.maxrolleffect);
    putint(p, c.ffov);
    putint(p, c.scopefov);
    putint(p, c.ip_censored);
}

void sendinitclient(client &c)
{
    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    putinitclient(c, p);
    sendpacket(-1, 1, p.finalize(), c.clientnum);
}

void welcomeinitclient(packetbuf &p, int exclude = -1)
{
    loopv(clients)
    {
        client &c = *clients[i];
        if(c.type!=ST_TCPIP || !c.isauthed || c.clientnum == exclude) continue;
        putinitclient(c, p);
    }
}

void welcomepacket(packetbuf &p, int n)
{
    if(!sg->smapname[0]) maprot.next(false);

    client *c = valid_client(n) ? clients[n] : NULL;
    int numcl = numclients();

    putint(p, SV_WELCOME);
    putint(p, sg->smapname[0] && !m_demo ? numcl : -1);
    if(sg->smapname[0] && !m_demo)
    {
        putint(p, SV_MAPCHANGE);
        sendstring(sg->smapname, p);
        putint(p, sg->smode);
        putint(p, sg->curmap && sg->curmap->isdistributable() ? sg->curmap->cgzlen : 0);
        putint(p, sg->curmap && sg->curmap->isdistributable() ? sg->curmap->maprevision : 0);
        putint(p, sg->srvgamesalt);
        if(sg->smode>1 || (sg->smode==0 && numnonlocalclients()>0))
        {
            putint(p, SV_TIMEUP);
            putint(p, (sg->gamemillis>=sg->gamelimit || sg->forceintermission) ? sg->gamelimit : sg->gamemillis);
            putint(p, sg->gamelimit);
            //putint(p, sg->minremain*60);
        }
        send_item_list(p); // this includes the flags
    }
    savedscore *sc = NULL;
    if(c)
    {
        if(c->type == ST_TCPIP && serveroperator() != -1) sendserveropinfo(n);
        c->team = sg->mastermode == MM_MATCH && sc ? team_tospec(sc->team) : TEAM_SPECT;
        putint(p, SV_SETTEAM);
        putint(p, n);
        putint(p, c->team | (FTR_INFO << 4));

        putint(p, SV_FORCEDEATH);
        putint(p, n);
        sendf(-1, 1, "ri2x", SV_FORCEDEATH, n, n);
    }
    if(!c || clients.length()>1)
    {
        putint(p, SV_RESUME);
        loopv(clients)
        {
            client &c = *clients[i];
            if(c.type!=ST_TCPIP || c.clientnum==n) continue;
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
            putint(p, c.state.teamkills);
            loopi(NUMGUNS) putint(p, c.state.ammo[i]);
            loopi(NUMGUNS) putint(p, c.state.mag[i]);
        }
        putint(p, -1);
        welcomeinitclient(p, n);
    }
    putint(p, SV_SERVERMODE);
    putint(p, sendservermode(false));
    putint(p, SV_PAUSEMODE);
    putint(p, sg->sispaused);
    const char *motd = scl.motd[0] ? scl.motd : serverinfomotd.getmsg();
    if(*motd)
    {
        putint(p, SV_TEXT);
        sendstring(motd, p);
    }
}

void sendwelcome(client *cl, int chan)
{
    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    welcomepacket(p, cl->clientnum);
    sendpacket(cl->clientnum, chan, p.finalize());
    cl->haswelcome = true;
}

void forcedeath(client *cl)
{
    sdropflag(cl->clientnum);
    cl->state.state = CS_DEAD;
    cl->state.lastdeath = sg->gamemillis;
    cl->state.respawn();
    sendf(-1, 1, "rii", SV_FORCEDEATH, cl->clientnum);
}

int checktype(int type, client *cl)
{
    if(cl && cl->type==ST_LOCAL) return type;
    if(type < 0 || type >= SV_NUM) return -1;
    // server only messages
    static int servtypes[] = { SV_SERVINFO, SV_WELCOME, SV_INITCLIENT, SV_POSN, SV_CDIS, SV_GIBDIED, SV_DIED,
                        SV_GIBDAMAGE, SV_DAMAGE, SV_HITPUSH, SV_SHOTFX,
                        SV_SPAWNSTATE, SV_SPAWNDENY, SV_FORCEDEATH, SV_RESUME,
                        SV_DISCSCORES, SV_TIMEUP, SV_ITEMACC, SV_MAPCHANGE, SV_ITEMSPAWN, SV_PONG,
                        SV_SERVMSG, SV_SERVMSGVERB, SV_ITEMLIST, SV_FLAGINFO, SV_FLAGMSG, SV_FLAGCNT,
                        SV_ARENAWIN, SV_SERVOPINFO,
                        SV_CALLVOTESUC, SV_CALLVOTEERR, SV_VOTERESULT,
                        SV_SETTEAM, SV_TEAMDENY, SV_SERVERMODE, SV_IPLIST,
                        SV_SENDDEMOLIST, SV_SENDDEMO, SV_DEMOPLAYBACK,
                        SV_CLIENT, SV_DEMOCHECKSUM, SV_PAUSEMODE };
    // only allow edit messages in coop-edit mode
    static int edittypes[] = { SV_EDITENT, SV_EDITXY, SV_EDITARCH, SV_EDITBLOCK, SV_EDITD, SV_EDITE, SV_NEWMAP };
    if(cl)
    {
        loopi(sizeof(servtypes)/sizeof(int)) if(type == servtypes[i]) return -1;
        loopi(sizeof(edittypes)/sizeof(int)) if(type == edittypes[i]) return sg->smode==GMODE_COOPEDIT ? type : -1;
        if(++cl->overflow >= 200) return -2;
    }
    return type;
}


// process auth calculations in separate thread, since it may take a while

struct servinfoauth
{
    string clientmsg, servermsg; // client message to verify, server message to sign
    uchar clientpubkey[32];
    int type, wantauth, cn, clientsequence, clientmsglen, servermsglen, success;
};

ringbuf<servinfoauth *, 16> servinfoauths;
volatile servinfoauth *servinfoauth_dropbox = NULL;
volatile bool servinfoauth_done = false;
sl_semaphore servinfoauth_deferred(0, NULL);

int deferredprocessingthread(void *nop)
{
    for(;;)
    {
        servinfoauth_deferred.wait();
        if(!servinfoauth_done && servinfoauths.length())
        {
            servinfoauth *sa = servinfoauths.remove();
            servinfoauth_dropbox = sa;
            sa->success = 0;
            switch(sa->type)
            {
                case SV_SERVINFO_RESPONSE:
                {
                    if(!sa->wantauth) sa->success = 1;
                    else if(sa->clientmsglen && ed25519_sign_check((uchar*)sa->clientmsg, sa->clientmsglen + 64, sa->clientpubkey))
                    { // client message checks out, now sign the answer
                        ed25519_sign((uchar*)sa->servermsg, NULL, (uchar*)sa->servermsg, sa->servermsglen, ssk);
                        sa->success = 1;
                    }
                    break;
                }
            }
            servinfoauth_done = true;
        }
    }
    return 0;
}

void polldeferredprocessing()
{
    static void *deferredprocessingthread_ti = NULL;
    if(!deferredprocessingthread_ti) deferredprocessingthread_ti = sl_createthread(deferredprocessingthread, NULL);

    if(servinfoauth_done)
    {
        servinfoauth *sa = (servinfoauth *)servinfoauth_dropbox;
        servinfoauth_done = false;
        switch(sa->type)
        {
            case SV_SERVINFO_RESPONSE:
            {
                if(clients[sa->cn]->clientsequence == sa->clientsequence) // make sure, it's still the same client using that cn
                {
                    if(!sa->success) disconnect_client(sa->cn, DISC_AUTH);
                    else
                    { // continue connecting
                        client *cl = clients[sa->cn];
                        if(sa->wantauth)
                        {
                            memcpy(cl->pubkey.u, sa->clientpubkey, 32);
                            memcpy(cl->servinforesponse, sa->clientmsg, sa->clientmsglen + 64);
                            bin2hex(cl->pubkeyhex, cl->pubkey.u, 32);
                            cl->vita = vitas.access(cl->pubkey);
                            if(!cl->vita)
                            {
                                vita_s v;
                                memset(&v, 0, sizeof(v));
                                cl->vita = &vitas.access(cl->pubkey, v);
                                cl->vita->vs[VS_FIRSTLOGIN] = servclock;
                            }
                            if(cl->country[0] != '-') cl->vita->addcc(cl->country);
                            cl->vita->addip(cl->ip);
                            cl->vita->vs[VS_LASTLOGIN] = servclock;
                            cl->needsauth = true; // may have been voluntary, but it's done
                            int datecodes = 0, *vs = cl->vita->vs;
                            loopi(VS_NUMCOUNTERS) datecodes |= vs[i] == 1 || (servclock - vs[i]) < 0 ? (1 << i) : 0;
                            sendf(sa->cn, 1, "riimiss", SV_SERVINFO_CONTD, 1, 64, sa->servermsg, datecodes, cl->vita->clan, cl->vita->publiccomment);
                        }
                        else sendf(sa->cn, 1, "rii", SV_SERVINFO_CONTD, 0);
                    }
                }
                break;
            }
        }
        delete sa;
    }
    if(!servinfoauths.empty() && servinfoauth_deferred.getvalue() < 1) servinfoauth_deferred.post();
}

// server side processing of updates: does very little and most state is tracked client only
// could be extended to move more gameplay to server (at expense of lag)

SERVPARLIST(auth_verify_ip, 0, 0, 0, endis, "sVerify server IP reported by the client during auth");

void process(ENetPacket *packet, int sender, int chan)
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
        else if(chan!=1 || ((type = getint(p)) != SV_SERVINFO_RESPONSE && type != SV_CONNECT)) disconnect_client(sender, DISC_TAGT);
        else if(type == SV_SERVINFO_RESPONSE)
        {
            if(servinfoauths.full()) disconnect_client(sender, DISC_OVERLOAD);
            else
            {
                string tmp1, tmp2;
                servinfoauth *sa = servinfoauths.stage(new servinfoauth);
                memset(sa, 0, sizeof(servinfoauth));
                int clientsalt = getint(p), clientclock = getint(p), curpeerport = getint(p);
                enet_uint32 curpeerip = getip4(p);
                if((sa->wantauth = getint(p)))
                {
                    p.get(sa->clientpubkey, 32);
                    p.get((uchar*)sa->clientmsg, 64);
                }
                bool ipfail = servownip && servownip != curpeerip;
                if(ipfail) mlog(ACLOG_INFO, "[%s] client reported different server IP %s:%d (known own IP is %s)", cl->hostname, iptoa(curpeerip, tmp1), curpeerport, iptoa(servownip, tmp2));
                formatstring(text)("SERVINFOCHALLENGE<(%d) cn: %d c: %s (%s) s: %s:%d", cl->salt, sender, iptoa(cl->ip_censored, tmp1), cl->country, iptoa(servownip, tmp2), curpeerport);
                concatformatstring(text, " %s st: %d ct: %d (%d)>", bin2hex(tmp1, spk, 32), cl->connectclock, clientclock, clientsalt);
                sa->clientmsglen = (int)strlen(text);
                if(sa->clientmsglen < MAXSTRLEN - 65) memcpy(sa->clientmsg + 64, text, sa->clientmsglen);
                else sa->clientmsglen = 0;
                formatstring(sa->servermsg)("SERVINFOSIGNED<%s>", bin2hex(tmp1, (uchar*)sa->clientmsg, 64));
                sa->servermsglen = (int)strlen(sa->servermsg);
                if((!sa->wantauth && cl->needsauth) || (ipfail && auth_verify_ip)) { delete sa; disconnect_client(sender, DISC_AUTH); }
                else
                {
                    sa->type = type;
                    sa->cn = sender;
                    sa->clientsequence = cl->clientsequence;
                    servinfoauths.commit();
                }
            }
            return;
        }
        else
        { // SV_CONNECT
            cl->acversion = getint(p);
            cl->acbuildtype = getint(p);
            defformatstring(tags)(", AC: %d|%x", cl->acversion, cl->acbuildtype);
            getstring(text, p);
            filtertext(text, text, FTXT__PLAYERNAME, MAXNAMELEN);
            if(!text[0]) copystring(text, "unarmed");
            copystring(cl->name, text, MAXNAMELEN+1);
            getstring(text, p);
            copystring(cl->pwd, text);
            getstring(text, p);
            filterlang(cl->lang, text);
            int wantrole = getint(p), np = getint(p);
            cl->state.nextprimary = np > 0 && np < NUMGUNS ? np : GUN_ASSAULT;
            loopi(2) cl->skin[i] = getint(p);
            cl->maxroll = getint(p);
            cl->maxrolleffect = getint(p);
            cl->ffov = getint(p);
            cl->scopefov = getint(p);
            int bantype = getbantype(sender);
            bool banned = bantype > BAN_NONE;
            bool srvfull = numnonlocalclients() > scl.maxclients;
            bool srvprivate = sg->mastermode == MM_PRIVATE || sg->mastermode == MM_MATCH;
            bool matchreconnect = sg->mastermode == MM_MATCH && findscore(*cl, false);
            if (cl->pubkeyhex[0]) {
                concatformatstring(tags, ", pubkey: %s", cl->pubkeyhex);
            }
            int bl = 0, wl = nickblacklist.checkwhitelist(*cl);
            if(wl == NWL_PASS) concatstring(tags, ", nickname whitelist match");
            if(wl == NWL_UNLISTED) bl = nickblacklist.checkblacklist(cl->name);

            bool vitawhitelist = cl->checkvitadate(VS_WHITELISTED);
            if (vitawhitelist) {
                concatstring(tags, ", vita whitelist match");
                banned = false;
            }

            if(matchreconnect && !banned)
            { // former player reconnecting to a server in match mode
                cl->isauthed = true;
                mlog(ACLOG_INFO, "[%s] %s logged in (reconnect to match)%s", cl->hostname, cl->name, tags);
            }
            else if(wl == NWL_IPFAIL || wl == NWL_PWDFAIL)
            { // nickname matches whitelist, but IP is not in the required range or PWD doesn't match
                mlog(ACLOG_INFO, "[%s] '%s' matches nickname whitelist: wrong %s%s", cl->hostname, cl->name, wl == NWL_IPFAIL ? "IP" : "PWD", tags);
                disconnect_client(sender, DISC_BADNICK);
            }
            else if(bl > 0)
            { // nickname matches blacklist
                mlog(ACLOG_INFO, "[%s] '%s' matches nickname blacklist line %d%s", cl->hostname, cl->name, bl, tags);
                disconnect_client(sender, DISC_BADNICK);
            }
            else if(passwords.check(cl->name, cl->pwd, cl->salt, &pd, (cl->type==ST_TCPIP ? cl->peer->address.host : 0)) && (!pd.denyadmin || (banned && !srvfull && !srvprivate)) && bantype != BAN_MASTER) // pass admins always through
            { // admin (or deban) password match
                cl->isauthed = true;
                if(!pd.denyadmin && wantrole == CR_ADMIN) clientrole = CR_ADMIN;
                if(bantype == BAN_VOTE)
                {
                    loopv(bans) if(bans[i].address.host == cl->peer->address.host) { bans.remove(i); concatstring(tags, ", ban removed"); break; } // remove admin bans
                }
                if(srvfull)
                {
                    loopv(clients) if(i != sender && clients[i]->type==ST_TCPIP)
                    {
                        disconnect_client(i, DISC_MAXCLIENTS); // disconnect someone else to fit maxclients again
                        break;
                    }
                }
                mlog(ACLOG_INFO, "[%s] %s logged in using the admin password in line %d%s", cl->hostname, cl->name, pd.line, tags);
            }
            else if(wantrole == CR_ADMIN && cl->checkvitadate(VS_ADMIN) && bantype != BAN_MASTER) // pass admins always through
            { // Set as admin in vita and is trying to connect as admin
                cl->isauthed = true;
                clientrole = CR_ADMIN;
                if(bantype == BAN_VOTE)
                {
                    loopv(bans) if(bans[i].address.host == cl->peer->address.host) { bans.remove(i); concatstring(tags, ", ban removed"); break; } // remove admin bans
                }
                if(srvfull)
                {
                    loopv(clients) if(i != sender && clients[i]->type==ST_TCPIP)
                    {
                        disconnect_client(i, DISC_MAXCLIENTS); // disconnect someone else to fit maxclients again
                        break;
                    }
                }
                mlog(ACLOG_INFO, "[%s] %s logged in using the vita admin claim%s", cl->hostname, cl->name, tags);
            }
            else if(scl.serverpassword[0] && !(srvprivate || srvfull || banned))
            { // server password required
                if(!strcmp(genpwdhash(cl->name, scl.serverpassword, cl->salt), cl->pwd))
                {
                    cl->isauthed = true;
                    mlog(ACLOG_INFO, "[%s] %s client logged in (using serverpassword)%s", cl->hostname, cl->name, tags);
                }
                else disconnect_client(sender, DISC_WRONGPW);
            }
            else if(srvprivate) disconnect_client(sender, DISC_PRIVATE);
            else if(srvfull) disconnect_client(sender, DISC_MAXCLIENTS);
            else if(banned) disconnect_client(sender, DISC_BANNED);
            else
            {
                cl->isauthed = true;
                mlog(ACLOG_INFO, "[%s] %s logged in (default)%s", cl->hostname, cl->name, tags);
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

        if(cl->vita) cl->vita->addname(cl->name);
        sendwelcome(cl);
        if(restorescore(*cl)) { sendresume(*cl, true); senddisconnectedscores(-1); }
        else if(cl->type==ST_TCPIP) senddisconnectedscores(sender);
        sendinitclient(*cl);
        if(clientrole != CR_DEFAULT) changeclientrole(sender, clientrole, NULL, true);
        if( curvote && curvote->result == VOTE_NEUTRAL ) callvotepacket (cl->clientnum);

        // send full IPs to admins
        loopv(clients)
        {
            if(clients[i] && clients[i]->clientnum != cl->clientnum && (clients[i]->role == CR_ADMIN || clients[i]->type == ST_LOCAL))
                sendiplist(clients[i]->clientnum, cl->clientnum);
        }
    }

    if(!cl) { mlog(ACLOG_ERROR, "<NULL> client in process()"); return; }  // should never happen anyway

    if(packet->flags&ENET_PACKET_FLAG_RELIABLE) reliablemessages = true;

    #define QUEUE_MSG { if(cl->type==ST_TCPIP) while(curmsg<p.length()) cl->messages.add(p.buf[curmsg++]); }
    #define QUEUE_BUF(body) \
    { \
        if(cl->type==ST_TCPIP) \
        { \
            curmsg = p.length(); \
            { body; } \
        } \
    }
    #define QUEUE_INT(n) QUEUE_BUF(putint(cl->messages, n))
    #define QUEUE_UINT(n) QUEUE_BUF(putuint(cl->messages, n))
    #define QUEUE_STR(text) QUEUE_BUF(sendstring(text, cl->messages))
    #define MSG_PACKET(packet) \
        packetbuf buf(16 + p.length() - curmsg, ENET_PACKET_FLAG_RELIABLE); \
        putint(buf, SV_CLIENT); \
        putint(buf, cl->clientnum); \
        putuint(buf, p.length() - curmsg); \
        buf.put(&p.buf[curmsg], p.length() - curmsg); \
        ENetPacket *packet = buf.finalize();

    int curmsg;
    while((curmsg = p.length()) < p.maxlen)
    {
        type = checktype(getint(p), cl);

        #ifdef _DEBUG
        if(type!=SV_POS && type!=SV_POSC && type!=SV_POSC2 && type!=SV_POSC3 && type!=SV_POSC4 && type!=SV_CLIENTPING && type!=SV_PING && type!=SV_CLIENT)
        {
            DEBUGVAR(cl->name);
            if(type >= 0) { DEBUGVAR(messagenames[type]); }
            protocoldebug(DEBUGCOND);
        }
        else protocoldebug(false);
        #endif

        if(lastclactionslookup(type)) cl->state.lastclaction = sg->gamemillis;

        switch(type)
        {
            case SV_TEAMTEXTME:
            case SV_TEAMTEXT:
                getstring(text, p);
                filtertext(text, text, FTXT__CHAT);
                if(*text)
                {
                    bool canspeech = forbiddenlist.canspeech(text);
                    if(!spamdetect(cl, text) && canspeech) // team chat
                    {
                        mlog(ACLOG_INFO, "[%s] %s%s says to team %s: '%s'", cl->hostname, type == SV_TEAMTEXTME ? "(me) " : "", cl->name, team_string(cl->team), text);
                        sendteamtext(text, sender, type);
                    }
                    else
                    {
                        mlog(ACLOG_INFO, "[%s] %s%s says to team %s: '%s', %s", cl->hostname, type == SV_TEAMTEXTME ? "(me) " : "",
                                cl->name, team_string(cl->team), text, canspeech ? "SPAM detected" : "Forbidden speech");
                        if (canspeech)
                        {
                            sendservmsg("\f3Please do not spam; your message was not delivered.", sender);
                            if ( cl->spamcount > SPAMMAXREPEAT + 2 ) disconnect_client(cl->clientnum, DISC_SPAM);
                        }
                        else
                        {
                            sendservmsg("\f3Watch your language! Your message was not delivered.", sender);
                            kick_abuser(cl->clientnum, cl->badmillis, cl->badspeech, 3);
                        }
                    }
                }
                break;

            case SV_TEXTME:
            case SV_TEXT:
            {
                int mid1 = curmsg, mid2 = p.length();
                getstring(text, p);
                filtertext(text, text, FTXT__CHAT);
                if(*text)
                {
                    bool canspeech = forbiddenlist.canspeech(text);
                    if(!spamdetect(cl, text) && canspeech)
                    {
                        if(sg->mastermode != MM_MATCH || !sg->matchteamsize || team_isactive(cl->team) || (cl->team == TEAM_SPECT && cl->role == CR_ADMIN)) // common chat
                        {
                            mlog(ACLOG_INFO, "[%s] %s%s says: '%s'", cl->hostname, type == SV_TEXTME ? "(me) " : "", cl->name, text);
                            if(cl->type==ST_TCPIP) while(mid1<mid2) cl->messages.add(p.buf[mid1++]);
                            QUEUE_STR(text);
                            ASSERT(SV_TEXT < 126 && SV_TEXTME < 126);
                            sendf(sender, 1, "riiuis", SV_CLIENT, sender, (int) strlen(text) + 2, type, text); // "local" echo
                        }
                        else // spect chat
                        {
                            mlog(ACLOG_INFO, "[%s] %s%s says to team %s: '%s'", cl->hostname, type == SV_TEAMTEXTME ? "(me) " : "", cl->name, team_string(cl->team), text);
                            sendteamtext(text, sender, type == SV_TEXTME ? SV_TEAMTEXTME : SV_TEAMTEXT);
                        }
                    }
                    else
                    {
                        mlog(ACLOG_INFO, "[%s] %s%s says: '%s', %s", cl->hostname, type == SV_TEXTME ? "(me) " : "",
                                cl->name, text, canspeech ? "SPAM detected" : "Forbidden speech");
                        if (canspeech)
                        {
                            sendservmsg("\f3Please do not spam; your message was not delivered.", sender);
                            if ( cl->spamcount > SPAMMAXREPEAT + 2 ) disconnect_client(cl->clientnum, DISC_SPAM);
                        }
                        else
                        {
                            sendservmsg("\f3Watch your language! Your message was not delivered.", sender);
                            kick_abuser(cl->clientnum, cl->badmillis, cl->badspeech, 3);
                        }
                    }
                }
                break;
            }

            case SV_TEXTPRIVATE:
            {
                int targ = getint(p);
                getstring(text, p);
                filtertext(text, text, FTXT__CHAT);

                if(!valid_client(targ)) break;
                client *target = clients[targ];

                if(*text)
                {
                    bool canspeech = forbiddenlist.canspeech(text);
                    if(!spamdetect(cl, text) && canspeech)
                    {
                        bool allowed = !(sg->mastermode == MM_MATCH && cl->team != target->team) && cl->role >= roleconf('t');
                        mlog(ACLOG_INFO, "[%s] %s says to %s: '%s' (%s)", cl->hostname, cl->name, target->name, text, allowed ? "allowed":"disallowed");
                        if(allowed)
                        {
                            sendf(cl->clientnum, 1, "riiis", SV_TEXTPRIVATE, cl->clientnum, target->clientnum, text); // "local" echo
                            sendf(target->clientnum, 1, "riiis", SV_TEXTPRIVATE, cl->clientnum, -1, text);
                        }
                    }
                    else
                    {
                        mlog(ACLOG_INFO, "[%s] %s says to %s: '%s', %s", cl->hostname, cl->name, target->name, text, canspeech ? "SPAM detected" : "Forbidden speech");
                        if (canspeech)
                        {
                            sendservmsg("\f3Please do not spam; your message was not delivered.", sender);
                            if ( cl->spamcount > SPAMMAXREPEAT + 2 ) disconnect_client(cl->clientnum, DISC_SPAM);
                        }
                        else
                        {
                            sendservmsg("\f3Watch your language! Your message was not delivered.", sender);
                            kick_abuser(cl->clientnum, cl->badmillis, cl->badspeech, 3);
                        }
                    }
                }
            }
            break;

            case SV_VOICECOM:
            case SV_VOICECOMTEAM:
            {
                int s = getint(p);
                /* spam filter */
                if ( servmillis > cl->mute ) // client is not muted
                {
                    if(!gamesound_isvoicecom(s)) cl->mute = servmillis + 10000; // vc is invalid
                    else if ( cl->lastvc + 4000 < servmillis ) { if ( cl->spam > 0 ) cl->spam -= (servmillis - cl->lastvc) / 4000; } // no vc in the last 4 seconds
                    else cl->spam++; // the guy is spamming
                    if ( cl->spam < 0 ) cl->spam = 0;
                    cl->lastvc = servmillis; // register
                    if ( cl->spam > 4 ) { cl->mute = servmillis + 10000; break; } // 5 vcs in less than 20 seconds... shut up please
                    if ( type == SV_VOICECOM ) { QUEUE_MSG; }
                    else sendvoicecomteam(s, sender);
                }
            }
            break;

            case SV_MAPIDENT:
            {
                int gzs = getint(p);
                int rev = getint(p);
                if(!isdedicated || (sg->curmap && sg->curmap->cgzlen == gzs && sg->curmap->maprevision == rev) || m_coop)
                { // here any game really starts for a client: spawn, if it's a new game - don't spawn if the game was already running
                    cl->isonrightmap = true;
                    int sp = canspawn(cl);
                    sendf(sender, 1, "rii", SV_SPAWNDENY, sp);
                    cl->spawnperm = sp;
                    if(cl->loggedwrongmap) mlog(ACLOG_INFO, "[%s] %s is now on the right map: revision %d/%d", cl->hostname, cl->name, rev, gzs);
                    bool spawn = false;
                    if(team_isspect(cl->team))
                    {
                        if(numclients() < 2 && !m_demo && sg->mastermode != MM_MATCH) // spawn on empty servers
                        {
                            spawn = updateclientteam(cl->clientnum, TEAM_ANYACTIVE, FTR_INFO);
                        }
                    }
                    else
                    {
                        if((cl->freshgame || numclients() < 2) && !m_demo) spawn = true;
                    }
                    cl->freshgame = false;
                    if(spawn) sendspawn(cl);

                }
                else
                {
                    forcedeath(cl);
                    mlog(ACLOG_INFO, "[%s] %s is on the wrong map: revision %d/%d", cl->hostname, cl->name, rev, gzs);
                    cl->loggedwrongmap = true;
                    sendf(sender, 1, "rii", SV_SPAWNDENY, SP_WRONGMAP);
                }
                QUEUE_MSG;
                break;
            }

            case SV_ITEMPICKUP:
            {
                int n = getint(p);
                if(!sg->arenaround || sg->arenaround - sg->gamemillis > 2000)
                {
                    gameevent &pickup = cl->addevent();
                    pickup.type = GE_PICKUP;
                    pickup.pickup.ent = n;
                }
                break;
            }

            case SV_WEAPCHANGE:
            {
                int gunselect = getint(p);
                if(!valid_weapon(gunselect)) break;
                cl->state.gunselect = gunselect;
                QUEUE_MSG;
                break;
            }

            case SV_PRIMARYWEAP:
            {
                int nextprimary = getint(p);
                if(!valid_weapon(nextprimary)) break;
                cl->state.nextprimary = nextprimary;
                break;
            }

            case SV_SWITCHNAME:
            {
                QUEUE_MSG;
                getstring(text, p);
                filtertext(text, text, FTXT__PLAYERNAME, MAXNAMELEN);
                if(!text[0]) copystring(text, "unarmed");
                QUEUE_STR(text);
                bool namechanged = strcmp(cl->name, text) != 0;
                if(namechanged) mlog(ACLOG_INFO,"[%s] %s changed name to %s", cl->hostname, cl->name, text);
                copystring(cl->name, text, MAXNAMELEN+1);
                if(namechanged)
                {
                    // very simple spam detection (possible FIXME: centralize spam detection)
                    if(cl->type==ST_TCPIP && (servmillis - cl->lastprofileupdate < 1000))
                    {
                        ++cl->fastprofileupdates;
                        if(cl->fastprofileupdates == 3) sendservmsg("\f3Please do not spam", sender);
                        if(cl->fastprofileupdates >= 5) { disconnect_client(sender, DISC_SPAM); break; }
                    }
                    else if(servmillis - cl->lastprofileupdate > 10000) cl->fastprofileupdates = 0;
                    cl->lastprofileupdate = servmillis;

                    switch(nickblacklist.checkwhitelist(*cl))
                    {
                        case NWL_PWDFAIL:
                        case NWL_IPFAIL:
                            mlog(ACLOG_INFO, "[%s] '%s' matches nickname whitelist: wrong IP/PWD", cl->hostname, cl->name);
                            disconnect_client(sender, DISC_BADNICK);
                            break;

                        case NWL_UNLISTED:
                        {
                            int l = nickblacklist.checkblacklist(cl->name);
                            if(l >= 0)
                            {
                                mlog(ACLOG_INFO, "[%s] '%s' matches nickname blacklist line %d", cl->hostname, cl->name, l);
                                disconnect_client(sender, DISC_BADNICK);
                            }
                            break;
                        }
                    }
                    if(cl->vita) cl->vita->addname(cl->name);
                }
                break;
            }

            case SV_SWITCHTEAM:
            {
                int t = getint(p);
                if(!updateclientteam(sender, team_isvalid(t) ? t : TEAM_SPECT, FTR_PLAYERWISH)) sendf(sender, 1, "rii", SV_TEAMDENY, t);
                break;
            }

            case SV_SWITCHSKIN:
            {
                loopi(2) cl->skin[i] = getint(p);
                QUEUE_MSG;

                if(cl->type==ST_TCPIP && (servmillis - cl->lastprofileupdate < 1000))
                {
                    ++cl->fastprofileupdates;
                    if(cl->fastprofileupdates == 3) sendservmsg("\f3Please do not spam", sender);
                    if(cl->fastprofileupdates >= 5) disconnect_client(sender, DISC_SPAM);
                }
                else if(servmillis - cl->lastprofileupdate > 10000) cl->fastprofileupdates = 0;
                cl->lastprofileupdate = servmillis;
                break;
            }

            case SV_TRYSPAWN:
            {
                int sp = canspawn(cl);
                if(team_isspect(cl->team) && sp < SP_OK_NUM)
                {
                    updateclientteam(sender, TEAM_ANYACTIVE, FTR_PLAYERWISH);
                    sp = canspawn(cl);
                }
                if( !m_arena && sp < SP_OK_NUM && sg->gamemillis > cl->state.lastspawn + 1000 ) sendspawn(cl);
                break;
            }

            case SV_SPAWN:
            {
                int ls = getint(p), gunselect = getint(p);
                if((cl->state.state!=CS_ALIVE && cl->state.state!=CS_DEAD && cl->state.state!=CS_SPECTATE) ||
                    ls!=cl->state.lifesequence || cl->state.lastspawn<0 || !valid_weapon(gunselect)) break;
                cl->state.lastspawn = -1;
                cl->state.spawn = sg->gamemillis;
                cl->autospawn = false;
                cl->state.state = CS_ALIVE;
                cl->state.gunselect = gunselect;
                QUEUE_BUF(
                {
                    putint(cl->messages, SV_SPAWN);
                    putint(cl->messages, cl->state.lifesequence);
                    putint(cl->messages, cl->state.health);
                    putint(cl->messages, cl->state.armour);
                    putint(cl->messages, cl->state.gunselect);
                    loopi(NUMGUNS) putint(cl->messages, cl->state.ammo[i]);
                    loopi(NUMGUNS) putint(cl->messages, cl->state.mag[i]);
                });
                break;
            }

            case SV_SPECTCN:
                cl->spectcn = getint(p);
                QUEUE_MSG;                      // FIXME: demo only
                break;

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
                    if(!cl->timesync || (cl->events.length()==1 && cl->state.waitexpired(sg->gamemillis))) \
                    { \
                        cl->timesync = true; \
                        cl->gameoffset = sg->gamemillis - event.id; \
                        event.millis = sg->gamemillis; \
                    } \
                    else event.millis = cl->gameoffset + event.id; \
                }
                seteventmillis(shot.shot);
                shot.shot.gun = getint(p);
                loopk(3) { shot.shot.from[k] = cl->state.o.v[k] + ( k == 2 ? (((cl->f>>7)&1)?2.2f:4.2f) : 0); }
                loopk(3) { float v = getint(p)/DMF; shot.shot.to[k] = ((k<2 && v<0.0f)?0.0f:v); }
                int hits = getint(p);
                loopk(hits)
                {
                    if(p.overread()) break;
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
                    if(p.overread()) break;
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
                    conoutf("ERROR: invalid client (msg %d)", type);
    #endif
                    return;
                }
                cl->state.o.x = getuint(p)/DMF;
                cl->state.o.y = getuint(p)/DMF;
                int z = getuint(p);
                uint64_t ff = getuintn(p, (YAWBITS + PITCHBITS + FLAGBITS + 7) / 8);
                cl->yaw = (int)decodeyaw((ff >> (FLAGBITS + PITCHBITS)) & ((1 << YAWBITS) - 1));
                cl->pitch = (int)decodepitch((ff >> FLAGBITS) & ((1 << PITCHBITS) - 1));
                cl->f = ff & ((1 << FLAGBITS) - 1);
                if(cl->f & (1 << 10)) z = -z;
                cl->state.o.z = z/DMF;
                if(cl->f & (1 << 9))
                {
                    loopi(3) getint(p);
                    cl->state.lastclaction = sg->gamemillis;
                }
                if(((cl->f & 0x1f) / 9) == 2) cl->state.lastclaction = sg->gamemillis; // crouching

                if(!cl->isonrightmap) break;
                if(cl->type==ST_TCPIP && (cl->state.state==CS_ALIVE || cl->state.state==CS_EDITING))
                {
                    cl->position.setsize(0);
                    while(curmsg<p.length()) cl->position.add(p.buf[curmsg++]);
                }
                break;
            }

            case SV_POSC:
            case SV_POSC2:
            case SV_POSC3:
            case SV_POSC4:
            {
                bitbuf<ucharbuf> q(p);
                int cn = q.getbits(5);
                if(cn!=sender)
                {
                    disconnect_client(sender, DISC_CN);
    #ifndef STANDALONE
                    conoutf("ERROR: invalid client (msg %d)", type);
    #endif
                    return;
                }
                int usefactor = type - SV_POSC + 7;
                int xt = q.getbits(usefactor + 4);
                int yt = q.getbits(usefactor + 4);
                cl->yaw = (int)decodeyaw(q.getbits(YAWBITS));
                cl->pitch = (int)decodepitch(q.getbits(PITCHBITS));
                cl->f = q.getbits(FLAGBITS);
                if(cl->f & (1 << 9))
                {
                    q.getbits(4 + 4 + 4);
                    cl->state.lastclaction = sg->gamemillis;
                }
                if(((cl->f & 0x1f) / 9) == 2) cl->state.lastclaction = sg->gamemillis; // crouching

                int zfull = q.getbits(1);
                int s = q.rembits();
                if(s < 3) s += 8;
                if(zfull) s = 11;
                int zt = q.getbits(s);
                if(cl->f & (1 << 10)) zt = -zt;

                if(!sg->curmap) break; // no compact POS packets without loaded servermap
                if(!cl->isonrightmap || p.remaining() || p.overread()) { p.flags = 0; break; }
                if(((cl->f >> 6) & 1) != (cl->state.lifesequence & 1) || usefactor != (sg->curmap->sfactor < 7 ? 7 : sg->curmap->sfactor)) break;
                cl->state.o.x = xt / DMF;
                cl->state.o.y = yt / DMF;
                cl->state.o.z = zt / DMF;
                if(cl->type==ST_TCPIP && (cl->state.state==CS_ALIVE || cl->state.state==CS_EDITING))
                {
                    cl->position.setsize(0);
                    while(curmsg<p.length()) cl->position.add(p.buf[curmsg++]);
                }
                break;
            }

            case SV_SENDMAP:
            {
                getstring(text, p);
                filtertext(text, text, FTXT__MAPNAME);
                const char *sentmap = behindpath(text), *reject = NULL;
                int mapsize = getint(p);
                int cfgsize = getint(p);
                int cfgsizegz = getint(p);
                int revision = getint(p);
                if(!sentmap[0] || mapsize <= 0 || cfgsizegz < 0 || mapsize > MAXMAPSENDSIZE || cfgsizegz > MAXMAPSENDSIZE
                   || mapsize + cfgsizegz > MAXMAPSENDSIZE || mapsize + cfgsizegz > p.remaining() || cfgsize > MAXCFGFILESIZE)
                {
                    p.forceoverread();
                    break;
                }
                servermap *sm = getservermap(sentmap);
                header *h = peekmapheader(&p.buf[p.len], mapsize);
                int actualrevision = h ? h->maprevision : 0;
                if(sm && sm->isro())
                {
                    reject = "map is RO";
                    defformatstring(msg)("\f3map upload rejected: map %s is readonly", sentmap);
                    sendservmsg(msg, sender);
                }
                else if(scl.incoming_limit && (scl.incoming_limit << 20) - incoming_size < mapsize + cfgsizegz)
                {
                    reject = "server incoming reached its limits";
                    sendservmsg("\f3server does not support more incomings: limit reached", sender);
                }
                else if(!sm && strchr(scl.mapperm, 'C') && cl->role < CR_ADMIN)// COOP is considered wonderful
                {
                    reject = "no permission for initial upload";
                    sendservmsg("\f3initial map upload rejected: you need to be admin", sender);
                }
                else if(sm && !sm->isro() && revision >= sm->maprevision && strchr(scl.mapperm, 'U') && cl->role < CR_ADMIN) // COOP is considered wonderful
                {
                    reject = "no permission to update";
                    sendservmsg("\f3map update rejected: you need to be admin", sender);
                }
                else if(revision != actualrevision)
                {
                    reject = "fake revision number";
                }
                else if(sm && !sm->isro() && revision < sm->maprevision && strchr(scl.mapperm, 'R') && cl->role < CR_ADMIN) // COOP is considered wonderful
                {
                    reject = "no permission to revert revision";
                    sendservmsg("\f3map revert to older revision rejected: you need to be admin to upload an older map", sender);
                }
                else
                { // map accepted
                    if(serverwritemap(sentmap, mapsize, cfgsize, cfgsizegz, &p.buf[p.len]))
                    {
                        if(sg->smode == GMODE_COOPEDIT && !strcmp(sentmap, behindpath(sg->smapname)))
                        { // update buffer only in coopedit mode (and on same map)
                            sg->coop_cgzlen = mapsize;
                            sg->coop_cfglen = cfgsize;
                            sg->coop_cfglengz = cfgsizegz;
                            DELETEA(sg->coop_mapdata);
                            sg->coop_mapdata = new uchar[mapsize + cfgsizegz];
                            memcpy(sg->coop_mapdata, &p.buf[p.len], mapsize + cfgsizegz);
                        }
                        incoming_size += mapsize + cfgsize;
                        mlog(ACLOG_INFO,"[%s] %s sent map %s, rev %d, %d + %d(%d) bytes written",
                                    clients[sender]->hostname, clients[sender]->name, sentmap, revision, mapsize, cfgsize, cfgsizegz);
                        defformatstring(msg)("%s (%d) up%sed map %s, rev %d", clients[sender]->name, sender, !sm ? "load": "dat", sentmap, revision);
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
                    mlog(ACLOG_INFO,"[%s] %s sent map %s rev %d, rejected: %s",
                                clients[sender]->hostname, clients[sender]->name, sentmap, revision, reject);
                }
                p.len += mapsize + cfgsizegz;
                break;
            }

            case SV_RECVMAP:
            {
                if(sg->smode == GMODE_COOPEDIT && sg->coop_mapdata) // "getmap" only in coopedit
                {
                    resetflag(cl->clientnum); // drop ctf flag
                    savedscore *sc = findscore(*cl, true); // save score
                    if(sc) sc->save(cl->state, cl->team);
                    packetbuf p(MAXTRANS + sg->coop_cgzlen + sg->coop_cfglengz, ENET_PACKET_FLAG_RELIABLE);
                    putint(p, SV_RECVMAP);
                    sendstring(sg->smapname, p);
                    putint(p, sg->coop_cgzlen);
                    putint(p, sg->coop_cfglen);
                    putint(p, sg->coop_cfglengz);
                    p.put(sg->coop_mapdata, sg->coop_cgzlen + sg->coop_cfglengz);
                    sendpacket(cl->clientnum, 2, p.finalize());
                    cl->mapchange(true);
                    sendwelcome(cl, 2); // resend state properly
                }
                else
                {
                    if(cl->isonrightmap)
                    {
                        mlog(ACLOG_VERBOSE,"%d:%s GETMAP while on right map already.", cl->clientnum, cl->name);
                        sendservmsg("you're already on this map", cl->clientnum);
                    }
                    else
                    {
                        packetbuf p(MAXTRANS + sg->coop_cgzlen + sg->coop_cfglengz, ENET_PACKET_FLAG_RELIABLE);
                        putint(p, SV_RECVMAP);
                        sendstring(sg->smapname, p);
                        putint(p, sg->curmap->cgzlen);
                        putint(p, sg->curmap->cfglen);
                        putint(p, sg->curmap->cfggzlen);
                        p.put(sg->curmap->cgzraw, sg->curmap->cgzlen);
                        if (sg->curmap->cgzlen) p.put(sg->curmap->cfgrawgz, sg->curmap->cfggzlen);
                        sendpacket(cl->clientnum, 2, p.finalize());
                        cl->mapchange(true);
                        sendwelcome(cl, 2); // resend state properly
                    }
                }
                break;
            }

            case SV_REMOVEMAP:
            {
                getstring(text, p);
                filtertext(text, text, FTXT__MAPNAME);
                string filename;
                const char *rmmap = behindpath(text), *reject = NULL;
                servermap *sm = getservermap(rmmap);
                //WAS: To allow "delete map" you need to set an *explicit* D or d via the -M switch. Otherwise *nobody* can delete maps.
                //WAS: int reqrole = strchr(scl.mapperm, 'D') ? CR_ADMIN : (strchr(scl.mapperm, 'd') ? CR_DEFAULT : CR_ADMIN + 100);
                int reqrole = strchr(scl.mapperm, 'd') ? CR_DEFAULT : CR_ADMIN;//COOP is considered wonderful, but admins should be allowed to delete maps by default
                if(cl->role < reqrole) reject = "no permission";
                else if(sm && sm->isro()) reject = "map is readonly";
                else if(!sm) reject = "map not found";
                else
                {
                    formatstring(filename)(SERVERMAP_PATH_INCOMING "%s.cgz", rmmap);
                    remove(filename);
                    formatstring(filename)(SERVERMAP_PATH_INCOMING "%s.cfg", rmmap);
                    remove(filename);
                    defformatstring(msg)("map '%s' deleted", rmmap);
                    sendservmsg(msg, sender);
                    mlog(ACLOG_INFO,"[%s] deleted map %s", clients[sender]->hostname, rmmap);
                }
                if (reject)
                {
                    mlog(ACLOG_INFO,"[%s] deleting map %s failed: %s", clients[sender]->hostname, rmmap, reject);
                    defformatstring(msg)("\f3can't delete map '%s', %s", rmmap, reject);
                    sendservmsg(msg, sender);
                }
                break;
            }

            case SV_FLAGACTION:
            {
                int action = getint(p);
                int flag = getint(p);
                if(!m_flags_ || flag < 0 || flag > 1 || action < 0 || action > FA_NUM) break;
                flagaction(flag, action, sender);
                break;
            }

            case SV_SETADMIN:
            {
                bool claim = getint(p) != 0;
                if(claim)
                {
                    getstring(text, p);
                    changeclientrole(sender, CR_ADMIN, text);
                } else changeclientrole(sender, CR_DEFAULT);
                break;
            }

            case SV_CALLVOTE:
            {
                voteinfo *vi = new voteinfo;
                vi->boot = 0;
                vi->type = getint(p);
                switch(vi->type)
                {
                    case SA_MAP:
                    {
                        getstring(text, p);
                        int mode = getint(p), time = getint(p);
                        vi->gonext = text[0]=='+' && text[1]=='1';
                        if(m_isdemo(mode)) filtertext(text, text, FTXT__DEMONAME);
                        else filtertext(text, behindpath(text), FTXT__MAPNAME);
                        if(time <= 0) time = -1;
                        time = min(time, 60);
                        if(vi->gonext)
                        {
                            servermap *c = maprot.next(false, true);
                            if(c)
                            {
                                strcpy(vi->text,c->fname);
                                vi->num1 = c->bestmode;
                                vi->num2 = c->modes[c->bestmode].time;
                            }
                            else fatal("unable to get next map in maprot");
                        }
                        else
                        {
                            copystring(vi->text, text, MAXTRANS);
                            vi->num1 = mode;
                            vi->num2 = time;
                        }
                        int qmode = (mode >= GMODE_NUM ? mode - GMODE_NUM : mode);
                        if(mode==GMODE_DEMO) vi->action = new demoplayaction(newstring(text));
                        else
                        {
                            char *vmap = newstring(*vi->text ? behindpath(vi->text) : "");
                            vi->action = new mapaction(vmap, qmode, time, sender, qmode!=mode);
                        }
                        break;
                    }
                    case SA_KICK:
                    {
                        vi->num1 = cn2boot = getint(p);
                        getstring(text, p);
                        copystring(vi->text, text, 128);
                        filtertext(text, text, FTXT__KICKBANREASON);
                        vi->action = new kickaction(cn2boot, newstring(text, 128));
                        vi->boot = 1;
                        break;
                    }
                    case SA_BAN:
                    {
                        vi->num1 = cn2boot = getint(p);
                        getstring(text, p);
                        copystring(vi->text, text, 128);
                        filtertext(text, text, FTXT__KICKBANREASON);
                        vi->action = new banaction(cn2boot, newstring(text, 128));
                        vi->boot = 2;
                        break;
                    }
                    case SA_REMBANS:
                        vi->action = new removebansaction();
                        break;
                    case SA_PAUSE:
                        vi->action = new pauseaction(vi->num1 = getint(p));
                        break;
                    case SA_MASTERMODE:
                        vi->action = new mastermodeaction(vi->num1 = getint(p));
                        break;
                    case SA_AUTOTEAM:
                        vi->action = new autoteamaction((vi->num1 = getint(p)) > 0);
                        break;
                    case SA_SHUFFLETEAMS:
                        pushvitasforsaving(true);
                        vi->action = new shuffleteamaction();
                        break;
                    case SA_FORCETEAM:
                        vi->num1 = getint(p);
                        vi->num2 = getint(p);
                        vi->action = new forceteamaction(vi->num1, sender, vi->num2);
                        break;
                    case SA_GIVEADMIN:
                        vi->action = new giveadminaction(vi->num1 = getint(p));
                        break;
                    case SA_RECORDDEMO:
                        vi->action = new recorddemoaction((vi->num1 = getint(p))!=0);
                        break;
                    case SA_STOPDEMO:
                        // compatibility
                        break;
                    case SA_CLEARDEMOS:
                        vi->action = new cleardemosaction(vi->num1 = getint(p));
                        break;
                    case SA_SERVERDESC:
                        getstring(text, p);
                        copystring(vi->text, text, MAXTRANS);
                        filtertext(text, text, FTXT__SERVDESC);
                        vi->action = new serverdescaction(newstring(text), sender);
                        break;
                }
                vi->owner = sender;
                vi->callmillis = servmillis;
                MSG_PACKET(msg);
                if(!scallvote(vi, msg)) delete vi;
                break;
            }

            case SV_VOTE:
            {
                int n = getint(p);
                MSG_PACKET(msg);
                ++msg->referenceCount; // need to increase reference count in case a vote disconnects a player after packet is queued to prevent double-freeing by packetbuf
                svote(sender, n, msg);
                --msg->referenceCount;
                break;
            }

            case SV_LISTDEMOS:
                listdemos(sender);
                break;

            case SV_GETDEMO:
                senddemo(sender, getint(p));
                break;

            case SV_DEMOSIGNATURE: // client acknowledging a demo checksum
                getint(p);
                getip4(p);
                p.get((uchar*)text, 64);
                QUEUE_MSG;
                break;

            case SV_PAUSEMODE:
                break;

            case SV_GETVITA:
            {
                int targetcn = getint(p);

                // If the client is admin or could be admin through vita
                if (!(cl->role >= CR_ADMIN || cl->checkvitadate(VS_ADMIN) || cl->checkvitadate(VS_OWNER))) {
                    sendservmsg("\f3You must be admin to get vita", sender);
                    break;
                }

                if (valid_client(targetcn)) {
                    client *target = clients[targetcn];

                    if (target->vita) {
                        sendf(sender, 1, "riimssv", SV_VITADATA, targetcn, 32, target->pubkey.u,
                              target->vita->privatecomment, target->vita->publiccomment,
                              VS_NUM, target->vita->vs);
                    }
                } else {
                    sendservmsg("\f3Invalid cn to retrieve vita data", sender);
                }
                break;
            }

            case SV_EXTENSION:
            {
                // AC server extensions
                //
                // rules:
                // 1. extensions MUST NOT modify gameplay or the behavior of the game in any way
                // 2. extensions may ONLY be used to extend or automate server administration tasks
                // 3. extensions may ONLY operate on the server and must not send any additional data to the connected clients
                // 4. extensions not adhering to these rules may cause the hosting server being banned from the masterserver
                //
                // also note that there is no guarantee that custom extensions will work in future AC versions


                getstring(text, p, 64);
                char *ext = text;   // extension specifier in the form of OWNER::EXTENSION, see sample below
                int n = getint(p);  // length of data after the specifier
                if(n < 0 || n > 50) return;

                // sample
                if(!strcmp(ext, "driAn::writelog"))
                {
                    // owner:       driAn - root@sprintf.org
                    // extension:   writelog - WriteLog v1.0
                    // description: writes a custom string to the server log
                    // access:      requires admin privileges
                    // usage:       /serverextension driAn::writelog "your log message here.."
                    // note:        There is a 49 character limit. The server will ignore messages with 50+ characters.

                    getstring(text, p, n);
                    if(valid_client(sender) && clients[sender]->role==CR_ADMIN)
                    {
                        mlog(ACLOG_INFO, "[%s] %s writes to log: %s", cl->hostname, cl->name, text);
                        sendservmsg("your message has been logged", sender);
                    }
                }
                else if(!strcmp(ext, "servpar::get"))
                {
                    getstring(text, p, n);
                    if(valid_client(sender))
                    {
                        filtertext(text, text, FTXT__SERVPARNAME);
                        servpar *id = servpars->access(text);
                        string msg;
                        if(id && id->type == SID_INT) formatstring(msg)("%s = %d", id->name, *id->i);
                        else if(id && id->type == SID_STR) formatstring(msg)("%s = \"%s\"", id->name, id->s);
                        else formatstring(msg)("servpar %s not found", text);
                        sendservmsg(msg, sender);
                    }

                }
                else if(!strcmp(ext, "servpar::set"))
                {
                    getstring(text, p, n);
                    if(valid_client(sender) && cl->checkvitadate(VS_OWNER))
                    {
                        char *b, *k = strtok_r(text, " :", &b), *v = strtok_r(NULL, " :", &b);
                        if(k && v)
                        {
                            filtertext(k, k, FTXT__SERVPARNAME);
                            servpar *id = servpars->access(k);
                            string msg;
                            if(!id) formatstring(msg)("servpar %s not found", k);
                            else if(id->nextvalue(v))
                            {
                                serverparameters.busy.wait(); // lock parameter access
                                id->fromfile = false;
                                id->update();
                                serverparameters.busy.post();
                                formatstring(msg)("%s set to %s", id->name, v);
                            }
                            else formatstring(msg)("%s cannot be set to \"%s\"", id->name, v);
                            sendservmsg(msg, sender);
                        }
                    }
                }
                else if(!strcmp(ext, "servpar::reset"))
                {
                    getstring(text, p, n);
                    if(valid_client(sender) && cl->checkvitadate(VS_OWNER))
                    {
                        enumerate(*servpars, servpar, id, id.fromfile = id.fromfile_org);
                        serverparameters.filehash = 0; // make sure, the parameter file is read soon
                        sendservmsg("reset all server parameters to be read from file", sender);
                    }

                }
#ifdef _DEBUG
                else if(!strcmp(ext, "servpar::dump"))
                {
                    getstring(text, p, n);
                    if(valid_client(sender) && cl->checkvitadate(VS_OWNER))
                    {
                        string msg;
                        enumerate(*servpars, servpar, id,
                        {
                            switch(id.type)
                            {
                                case SID_INT: formatstring(msg)("%s = %d, int %d..%d, def %d, %s", id.name, *id.i, id.minval, id.maxval, id.defaultint, id.desc && *id.desc ? id.desc + 1 : ""); break;
                                case SID_STR: formatstring(msg)("%s = \"%s\", def %s, %s", id.name, id.s, id.defaultstr, id.desc && *id.desc ? id.desc + 1 : ""); break;
                                default: msg[0] = '\0';
                            }
                            if(*msg) sendservmsg(msg, sender);
                        });
                    }
                }
#endif
                else if(!strcmp(ext, "set::teamsize"))
                {
                    // intermediate solution to set the teamsize (will be voteable)

                    getstring(text, p, n);
                    if(valid_client(sender) && clients[sender]->role==CR_ADMIN && sg->mastermode == MM_MATCH)
                    {
                        changematchteamsize(atoi(text));
                        defformatstring(msg)("match team size set to %d", sg->matchteamsize);
                        sendservmsg(msg, -1);
                    }
                }
                // else if()

                // add other extensions here

                else for(; n > 0; n--) getint(p); // ignore unknown extensions

                break;
            }

            case SV_EDITBLOCK:
            {
                loopi(5) getuint(p);
                freegzbuf(getgzbuf(p));
                QUEUE_MSG;
                break;
            }

            case -1:
                disconnect_client(sender, DISC_TAGT);
                return;

            case -2:
                disconnect_client(sender, DISC_OVERFLOW);
                return;

            default:
            {
                int size = msgsizelookup(type);
                if(size<=0) { if(sender>=0) disconnect_client(sender, DISC_TAGT); return; }
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
    static int clientsequencecounter = 0;
    client *c = NULL;
    loopv(clients) if(clients[i]->type==ST_EMPTY) { c = clients[i]; break; }
    if(!c)
    {
        c = new client;
        c->clientnum = clients.length();
        clients.add(c);
    }
    c->reset();
    c->clientsequence = ++clientsequencecounter;
    return *c;
}

void checkintermission()
{
    if(sg->minremain>0)
    {
        sg->minremain = (sg->gamemillis >= sg->gamelimit || sg->forceintermission) ? 0 : (sg->gamelimit - sg->gamemillis + 60000 - 1) / 60000;
        sendf(-1, 1, "ri3", SV_TIMEUP, (sg->gamemillis >= sg->gamelimit || sg->forceintermission) ? sg->gamelimit : sg->gamemillis, sg->gamelimit);
        if(demorecord && sg->gamemillis > 100000) sg->demosequence = demorecord->sequence; // after 100 seconds played, switch F10-default from "last game" to "this game"
    }
    if(!sg->interm && sg->minremain <= 0)
    { // intermission starting here
        sg->interm = sg->gamemillis + 10000;
        demorecord_beginintermission();
        pushvitasforsaving(true);
    }
    sg->forceintermission = false;
}

void resetserverifempty()
{
    loopv(clients) if(clients[i]->type!=ST_EMPTY) return;
    resetserver("", 0, 10);
    sg->matchteamsize = 0;
    sg->autoteam = true;
    changemastermode(MM_OPEN);
    sg->nextmapname[0] = '\0';
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
    if(demorecord) sg->recordpackets = true; // enable after 'old' worldstate is sent
}

void loggamestatus(const char *reason)
{
    int fragscore[2] = {0, 0}, flagscore[2] = {0, 0}, pnum[2] = {0, 0};
    string text;
    formatstring(text)("%d minutes remaining", sg->minremain);
    mlog(ACLOG_INFO, "");
    mlog(ACLOG_INFO, "Game status: %s on %s, %s, %s, %d clients%c %s",
                      modestr(gamemode), sg->smapname, reason ? reason : text, mmfullname(sg->mastermode), totalclients, sg->custom_servdesc ? ',' : '\0', sg->servdesc_current);
    if(!scl.loggamestatus) return;
    mlog(ACLOG_INFO, "cn name             %s%s frag death %sping role    host", m_teammode ? "team " : "", m_flags_ ? "flag " : "", m_teammode ? "tk " : "");
    loopv(clients)
    {
        client &c = *clients[i];
        if(c.type == ST_EMPTY || !c.name[0]) continue;
        formatstring(text)("%2d %-16s ", c.clientnum, c.name);                 // cn name
        if(m_teammode) concatformatstring(text, "%-4s ", team_string(c.team, true)); // teamname (abbreviated)
        if(m_flags_) concatformatstring(text, "%4d ", c.state.flagscore);             // flag
        concatformatstring(text, "%4d %5d", c.state.frags, c.state.deaths);          // frag death
        if(m_teammode) concatformatstring(text, " %2d", c.state.teamkills);          // tk
        mlog(ACLOG_INFO, "%s%5d %s  %s", text, c.ping, c.role == CR_ADMIN ? "admin " : "normal", c.hostname);
        if(c.team != TEAM_SPECT)
        {
            int t = team_base(c.team);
            flagscore[t] += c.state.flagscore;
            fragscore[t] += c.state.frags;
            pnum[t] += 1;
        }
    }
    if(sg->mastermode == MM_MATCH)
    {
        loopv(savedscores)
        {
            savedscore &sc = savedscores[i];
            if(sc.valid)
            {
                if(m_teammode) formatstring(text)("%-4s ", team_string(sc.team, true));
                else text[0] = '\0';
                if(m_flags_) concatformatstring(text, "%4d ", sc.flagscore);
                mlog(ACLOG_INFO, "   %-16s %s%4d %5d%s    - disconnected", sc.name, text, sc.frags, sc.deaths, m_teammode ? "  -" : "");
                if(sc.team != TEAM_SPECT)
                {
                    int t = team_base(sc.team);
                    flagscore[t] += sc.flagscore;
                    fragscore[t] += sc.frags;
                    pnum[t] += 1;
                }
            }
        }
    }
    if(m_teammode)
    {
        loopi(2) mlog(ACLOG_INFO, "Team %4s:%3d players,%5d frags%c%5d flags", team_string(i), pnum[i], fragscore[i], m_flags_ ? ',' : '\0', flagscore[i]);
    }
    mlog(ACLOG_INFO, "");
}

static unsigned char chokelog[MAXCLIENTS + 1] = { 0 };

void linequalitystats(int elapsed)
{
    static unsigned int chokes[MAXCLIENTS + 1] = { 0 }, spent[MAXCLIENTS + 1] = { 0 }, chokes_raw[MAXCLIENTS + 1] = { 0 }, spent_raw[MAXCLIENTS + 1] = { 0 };
    if(elapsed)
    { // collect data
        int c1 = 0, c2 = 0, r1 = 0, numc = 0;
        loopv(clients)
        {
            client &c = *clients[i];
            if(c.type != ST_TCPIP) continue;
            numc++;
            enet_uint32 &rtt = c.peer->lastRoundTripTime, &throttle = c.peer->packetThrottle;
            if(rtt < c.bottomRTT + c.bottomRTT / 3)
            {
                if(servmillis - c.connectmillis < 5000)
                    c.bottomRTT = rtt;
                else
                    c.bottomRTT = (c.bottomRTT * 15 + rtt) / 16; // simple IIR
            }
            if(throttle < 22) c1++;
            if(throttle < 11) c2++;
            if(rtt > c.bottomRTT * 2 && rtt - c.bottomRTT > 300) r1++;
        }
        spent_raw[numc] += elapsed;
        int t = numc < 7 ? numc : (numc + 1) / 2 + 3;
        chokes_raw[numc] +=  ((c1 >= t ? c1 + c2 : 0) + (r1 >= t ? r1 : 0)) * elapsed;
    }
    else
    { // calculate compressed statistics
        defformatstring(msg)("Uplink quality [ ");
        int ncs = 0;
        loopj(scl.maxclients)
        {
            int i = j + 1;
            int nc = chokes_raw[i] / 1000 / i;
            chokes[i] += nc;
            ncs += nc;
            spent[i] += spent_raw[i] / 1000;
            chokes_raw[i] = spent_raw[i] = 0;
            int s = 0, c = 0;
            if(spent[i])
            {
                frexp((double)spent[i] / 30, &s);
                if(s < 0) s = 0;
                if(s > 15) s = 15;
                if(chokes[i])
                {
                    frexp(((double)chokes[i]) / spent[i], &c);
                    c = 15 + c;
                    if(c < 0) c = 0;
                    if(c > 15) c = 15;
                }
            }
            chokelog[i] = (s << 4) + c;
            concatformatstring(msg, "%02X ", chokelog[i]);
        }
        mlog(ACLOG_DEBUG, "%s] +%d", msg, ncs);
    }
}

SERVPARLIST(mandatory_auth, 0, 1, 0, endis, "sEnforce IDs for all clients even on unlisted server/LAN game");

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
    if(!sg->sispaused) sg->gamemillis += diff;
    servmillis = nextmillis;
    entropy_add_byte(diff);

#ifndef STANDALONE
    if(m_demo)
    {
        readdemo();
        extern void silenttimeupdate(int milliscur, int millismax);
        silenttimeupdate(sg->gamemillis, gametimemaximum);
    }
#endif

    if(sg->minremain > 0 && !sg->sispaused)
    {
        processevents();
        checkitemspawns(diff);
        bool ktfflagingame = false;
        if(m_flags_) loopi(2)
        {
            sflaginfo &f = sg->sflaginfos[i];
            if(f.state == CTFF_DROPPED && sg->gamemillis-f.lastupdate > (m_ctf ? 30000 : 10000)) flagaction(i, FA_RESET, -1);
            if(m_htf && f.state == CTFF_INBASE && sg->gamemillis-f.lastupdate > (sg->curmap && sg->curmap->entstats.hasflags ? 10000 : 1000))
            {
                htf_forceflag(i);
            }
            if(m_ktf && f.state == CTFF_STOLEN && sg->gamemillis-f.lastupdate > 15000)
            {
                flagaction(i, FA_SCORE, -1);
            }
            if(f.state == CTFF_INBASE || f.state == CTFF_STOLEN) ktfflagingame = true;
        }
        if(m_ktf && !ktfflagingame) flagaction(rnd(2), FA_RESET, -1); // ktf flag watchdog
        if(m_arena) arenacheck();
        else if(m_autospawn) autospawncheck();
        if(scl.afk_limit && sg->mastermode == MM_OPEN && next_afk_check < servmillis && sg->gamemillis > 20 * 1000) check_afk();
    }

    if(curvote)
    {
        if(!curvote->isalive()) curvote->evaluate(true);
        if(curvote->result!=VOTE_NEUTRAL) DELETEP(curvote);
    }

    int nonlocalclients = numnonlocalclients();

    if(sg->forceintermission || ((sg->smode > 1 || (gamemode == 0 && nonlocalclients)) && sg->gamemillis-diff > 0 && sg->gamemillis / 60000 != (sg->gamemillis - diff) / 60000))
        checkintermission();
    if(m_demo && !demoplayback) maprot.restart();
    else if(sg->interm && ( (scl.demo_interm) ? sg->gamemillis > (sg->interm<<1) : sg->gamemillis > sg->interm ) )
    {
        loggamestatus("game finished");
        if(demorecord) enddemorecord();
        sg->interm = sg->nextsendscore = 0;

        //start next game
        if(sg->nextmapname[0]) startgame(sg->nextmapname, sg->nextgamemode);
        else maprot.next();
        sg->nextmapname[0] = '\0';
        map_queued = false;
    }

    resetserverifempty();
    polldeferredprocessing();
    checkdemotransmissions();

    if(!isdedicated) return;     // below is network only

    poll_serverthreads(); // read config and map files in the background, process the results in the main thread

    serverms(sg->smode, numclients(), sg->minremain, sg->smapname, servmillis, serverhost->address, &mnum, &msend, &mrec, &cnum, &csend, &crec, SERVER_PROTOCOL_VERSION, sg->servdesc_current, sg->interm);

    if(sg->autoteam && m_teammode && !m_arena && !sg->interm && servmillis - sg->lastfillup > 5000 && refillteams()) sg->lastfillup = servmillis;

    static unsigned int lastThrottleEpoch = 0;
    if(serverhost->bandwidthThrottleEpoch != lastThrottleEpoch)
    {
        if(lastThrottleEpoch) linequalitystats(serverhost->bandwidthThrottleEpoch - lastThrottleEpoch);
        lastThrottleEpoch = serverhost->bandwidthThrottleEpoch;
    }

    if(servmillis - laststatus > 60 * 1000)   // display bandwidth stats, useful for server ops
    {
        laststatus = servmillis;
        maprot.oneminutepassed(nonlocalclients ? sg->smode : -1);
        if(sg->curmap && nonlocalclients) sg->curmap->oneminuteplayed(servmillis, sg->smode);
        if(nonlocalclients || serverhost->totalSentData || serverhost->totalReceivedData)
        {
            if(nonlocalclients) loggamestatus(NULL);
            mlog(ACLOG_INFO, "Status at %s: %d remote clients, %.1f send, %.1f rec (K/sec);"
                                         " Ping: #%d|%d|%d; CSL: #%d|%d|%d (bytes)",
                                          timestring(true, "%d-%m-%Y %H:%M:%S"), nonlocalclients, serverhost->totalSentData/60.0f/1024, serverhost->totalReceivedData/60.0f/1024,
                                          mnum, msend, mrec, cnum, csend, crec);
            mnum = msend = mrec = cnum = csend = crec = 0;
            linequalitystats(0);
        }
        serverhost->totalSentData = serverhost->totalReceivedData = 0;
        loopv(clients) if(clients[i]->type == ST_TCPIP && clients[i]->isauthed && clients[i]->vita)
        {
            clients[i]->incrementvitacounter(VS_MINUTESCONNECTED, 1);
            if(clients[i]->isonrightmap && team_isactive(clients[i]->team)) clients[i]->incrementvitacounter(VS_MINUTESACTIVE, 1);
        }
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
        entropy_add_byte(event.type);
        switch(event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                client &c = addclient();
                c.type = ST_TCPIP;
                c.needsauth = usemaster || mandatory_auth;
                c.peer = event.peer;
                c.peer->data = (void *)(size_t)c.clientnum;
                c.connectmillis = servmillis;
                c.state.state = CS_SPECTATE;
                c.salt = (rnd(0x1000000)*((servmillis%1000)+1)) ^ rnd(0x1000000);
                char hn[1024];
                copystring(c.hostname, (enet_address_get_host_ip(&c.peer->address, hn, sizeof(hn))==0) ? hn : "unknown");
                c.ip = ENET_NET_TO_HOST_32(c.peer->address.host);
                c.ip_censored = c.ip & ~((1 << CLIENTIPCENSOR) - 1);
                const char *gi = geoiplist.check(c.ip);
                copystring(c.country, gi ? gi : "--", 3);
                filtercountrycode(c.country, c.country);
                entropy_add_byte(c.ip);
                mlog(ACLOG_INFO,"[%s] client connected (%s)", c.hostname, c.country);
                sendservinfo(c);
                totalclients++;
                break;
            }

            case ENET_EVENT_TYPE_RECEIVE:
            {
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
    int flags = sg->mastermode << PONGFLAG_MASTERMODE;
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
    getint(pi); getint(pi); // dummy read language code
    if(strlen(serverinfoinfo.getmsg()))
    {
        sendstring("en", po);
        int pos = 0;
        string buf;
        while(po.remaining() > MAXINFOLINELEN + 10)
        {
            sendstring(serverinfoinfo.getmsgline(buf, &pos), po);
            if(!*buf) break;
        }
    }
    else sendstring("", po);
}

void extping_maprot(ucharbuf &po)
{
    putint(po, CONFIG_MAXPAR);
/*    string text;
    bool abort = false;
    loopv(maprot.configsets)
    {
        if(po.remaining() < 100) abort = true;
        configset &c = maprot.configsets[i];
        filtertext(text, c.mapname, FTXT__MAPNAME);
        text[30] = '\0';
        sendstring(abort ? "-- list truncated --" : text, po);
        loopi(CONFIG_MAXPAR) putint(po, c.par[i]);
        if(abort) break;
    }*/
    sendstring("", po);
}

void extping_uplinkstats(ucharbuf &po)
{
    if(scl.maxclients > 3)
        po.put(chokelog + 4, scl.maxclients - 3); // send logs for 4..n used slots
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

        bool ismatch = sg->mastermode == MM_MATCH;
        putint(p,EXT_PLAYERSTATS_RESP_STATS);  // send player stats following
        putint(p,clients[i]->clientnum);  //add player id
        putint(p,clients[i]->ping);             //Ping
        sendstring(clients[i]->name,p);         //Name
        sendstring(team_string(clients[i]->team),p); //Team
        // "team_string(clients[i]->team)" sometimes return NULL according to RK, causing the server to crash. WTF ?
        putint(p,clients[i]->state.frags);      //Frags
        putint(p,clients[i]->state.flagscore);  //Flagscore
        putint(p,clients[i]->state.deaths);     //Death
        putint(p,clients[i]->state.teamkills);  //Teamkills
        putint(p,ismatch ? 0 : clients[i]->state.damage*100/max(clients[i]->state.shotdamage,1)); //Accuracy
        putint(p,ismatch ? 0 : clients[i]->state.health);     //Health
        putint(p,ismatch ? 0 : clients[i]->state.armour);     //Armour
        putint(p,ismatch ? 0 : clients[i]->state.gunselect);  //Gun selected
        putint(p,clients[i]->role);             //Role
        putint(p,clients[i]->state.state);      //State (Alive,Dead,Spawning,Lagged,Editing)
        uint ip = clients[i]->peer->address.host; // only 3 byte of the ip address (privacy protected)
        p.put((uchar*)&ip,3);
        putint(p,ismatch ? 0 : clients[i]->state.damage); //Damage
        putint(p,ismatch ? 0 : clients[i]->state.shotdamage); //Shot with damage

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
    putint(p, sg->minremain);
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
        putint(p, m_flags_ ? flagscores[i] : -1); // add flagscore per team
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
    servstate.reset();
    client &c = addclient();
    c.type = ST_LOCAL;
    c.role = CR_ADMIN;
    c.salt = 0;
    c.needsauth = false;
    copystring(c.hostname, "local");
    sendservinfo(c);
}
#endif

void processmasterinput(const char *cmd, int cmdlen, const char *args)
{
    string val;
    if(!strncmp(cmd, "cleargbans", cmdlen)) cleargbans();
    else if(sscanf(cmd, "addgban %s", val) == 1) addgban(val);
}

void quitproc(int param)
{
    // this triggers any "atexit"-calls:
    exit(param == 2 ? EXIT_SUCCESS : EXIT_FAILURE); // 3 is the only reply on Win32 apparently, SIGINT == 2 == Ctrl-C
}

const char *loglevelnames[] = { "debug", "verbose", "info", "warning", "error", "disabled", "" };

SERVPARLISTF(logthreshold_console, 0, ACLOG_INFO, 0, loglevelnames, restartlogging, "CConsole log level");
SERVPARLISTF(logthreshold_syslog, 0, ACLOG_NUM, 0, loglevelnames, restartlogging, "CSyslog log level");
SERVPARLISTF(logthreshold_file, 0, ACLOG_INFO, 0, loglevelnames, restartlogging, "CLogfile log level");
SERVSTAT(stat_mainlog_peaklevel, 0, "lMain log peak fill level (0..100%)");
SERVSTAT(stat_theadlog_peaklevel, 0, "lThreadlog peak fill level (0..100%)");

void restartlogging()
{
    if(isdedicated)
    {
        exitlogging();
        string identity;
        if(scl.logident[0]) filtertext(identity, scl.logident, FTXT__LOGIDENT);
        else formatstring(identity)("%s#%d", scl.ip[0] ? scl.ip : "local", scl.serverport);
        initlogging(identity, scl.syslogfacility, logthreshold_console, logthreshold_file, logthreshold_syslog, scl.logtimestamp, scl.logfilepath);
    }
}

void initserver(bool dedicated)
{
    copystring(servpubkey, "");
    if(scl.ssk && hex2bin(ssk, scl.ssk, 32) != 32) scl.ssk = NULL;
    ed25519_pubkey_from_private(spk, ssk);
    bin2hex(servpubkey, spk, 32);
    sg->smapname[0] = '\0';
    servparallfun();

    // init server parameters from commandline
    if(scl.demopath) initserverparameter("demo_path", scl.demopath);
    if(scl.maxdemos > 0) initserverparameter("demo_max_number", scl.maxdemos);

    // start logging
    initserverparameter("logthreshold_console", scl.verbose > 1 ? ACLOG_DEBUG : (scl.verbose ? ACLOG_VERBOSE : ACLOG_INFO));
    initserverparameter("logthreshold_syslog", scl.syslogthres);
    initserverparameter("logthreshold_file", scl.filethres);
    if((isdedicated = dedicated))
    {
        restartlogging();
        if(!logcheck(ACLOG_ERROR)) printf("WARNING: logging not started!\n");
    }
    mlog(ACLOG_INFO, "logging local AssaultCube server (version %d, protocol %d/%d) now..", AC_VERSION, SERVER_PROTOCOL_VERSION, EXT_VERSION);

    copystring(sg->servdesc_current, scl.servdesc_full);
    servermsinit(dedicated);

    if(isdedicated)
    {
        serverparameters.init(scl.parfilepath);
        if(serverparameters.load()) serverparameters.read(); // do this early, so the parameters are properly set during initialisation
        ENetAddress address = { ENET_HOST_ANY, (enet_uint16)scl.serverport };
        if(scl.ip[0] && enet_address_set_host(&address, scl.ip)<0) mlog(ACLOG_WARNING, "server ip not resolved!");
        serverhost = enet_host_create(&address, scl.maxclients+1, 3, 0, scl.uprate);
        if(!serverhost) fatal("could not create server host");
        loopi(scl.maxclients) serverhost->peers[i].data = (void *)-1;

        if(scl.ssk) mlog(ACLOG_INFO,"server public key: %s", servpubkey);
        formatstring(vitafilename)("%s.cfg", scl.vitabasename);
        formatstring(vitafilename_backup)("%s_bak.cfg", scl.vitabasename);
        formatstring(vitafilename_update)("%s_update.cfg", scl.vitabasename);
        formatstring(vitafilename_update_backup_base)("%s_update_", scl.vitabasename);
        path(vitafilename); path(vitafilename_backup); path(vitafilename_update); path(vitafilename_update_backup_base);
        char *vn;
        int gotvitas = readvitas((vn = vitafilename));
        if(gotvitas < 0) gotvitas = readvitas((vn = vitafilename_backup));
        if(gotvitas >= 0) mlog(ACLOG_INFO, "read %d player vitas from %s", gotvitas, vn);
        maprot.init(scl.maprotfile);
        passwords.init(scl.pwdfile, scl.adminpasswd);
        ipblacklist.init(scl.blfile);
        geoiplist.init(scl.geoipfile);
        nickblacklist.init(scl.nbfile);
        forbiddenlist.init(scl.forbidden);
        serverinfoinfo.init(scl.infopath, MAXTRANS / 2);
        serverinfomotd.init(scl.motdpath, MAXSTRLEN - 5);
        mlog(ACLOG_VERBOSE, "holding up to %d recorded demos in memory", demo_max_number);
        if(demo_save) mlog(ACLOG_VERBOSE,"all recorded demos will be written to: \"%s\"", demo_path);
        if(scl.voteperm[0]) mlog(ACLOG_VERBOSE,"vote permission string: \"%s\"", scl.voteperm);
        if(scl.mapperm[0]) mlog(ACLOG_VERBOSE,"map permission string: \"%s\"", scl.mapperm);
        mlog(ACLOG_VERBOSE,"server description: \"%s\"", scl.servdesc_full);
        if(scl.servdesc_pre[0] || scl.servdesc_suf[0]) mlog(ACLOG_VERBOSE,"custom server description: \"%sCUSTOMPART%s\"", scl.servdesc_pre, scl.servdesc_suf);
        mlog(ACLOG_VERBOSE,"maxclients: %d, kick threshold: %d, ban threshold: %d", scl.maxclients, scl.kickthreshold, scl.banthreshold);
        if(scl.master) mlog(ACLOG_VERBOSE,"master server URL: \"%s\"", scl.master);
        if(scl.serverpassword[0]) mlog(ACLOG_VERBOSE,"server password: \"%s\"", hiddenpwd(scl.serverpassword));
#ifdef ACAC
        mlog(ACLOG_INFO, "anticheat: enabled");
#else
        mlog(ACLOG_INFO, "anticheat: disabled");
#endif
    }

    resetserverifempty();

    if(isdedicated)       // do not return, this becomes main loop
    {
        #ifdef WIN32
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        #endif
        // kill -2 / Ctrl-C - see http://msdn.microsoft.com/en-us/library/xdkz3x12%28v=VS.100%29.aspx (or VS-2008?) for caveat (seems not to pertain to AC - 2011feb05:ft)
        if (signal(SIGINT, quitproc) == SIG_ERR) mlog(ACLOG_INFO, "Cannot handle SIGINT!");
        // kill -15 / probably process-manager on Win32 *shrug*
        if (signal(SIGTERM, quitproc) == SIG_ERR) mlog(ACLOG_INFO, "Cannot handle SIGTERM!");
        #ifndef WIN32
        // kill -1
        if (signal(SIGHUP, quitproc) == SIG_ERR) mlog(ACLOG_INFO, "Cannot handle SIGHUP!");
        #endif
        mlog(ACLOG_INFO, "dedicated server started, waiting for clients...");
        mlog(ACLOG_INFO, "Ctrl-C to exit"); // this will now actually call the atexit-hooks below - thanks to SIGINT hooked above - noticed and signal-code-docs found by SKB:2011feb05:ft:
        atexit(enet_deinitialize);
        atexit(cleanupserver);
        enet_time_set(0);

        // start file-IO threads
        readmapsthread_sem = new sl_semaphore(0, NULL);
        defformatstring(readmapslogfilename)("%sreadmaps_log.txt", scl.logfilepath);
        sl_createthread(readmapsthread, (void *)path(readmapslogfilename));
        defformatstring(_modeinfologfilename)("%smisc/modeinfo.txt", scl.logfilepath);
        modeinfologfilename = path(_modeinfologfilename);

        readserverconfigsthread_sem = new sl_semaphore(0, NULL);
        sl_createthread(readserverconfigsthread, NULL);

        sl_createthread(demoworkerthread, NULL);

        for(;;) serverslice(5);
    }
}

#ifdef STANDALONE

void localservertoclient(int chan, uchar *buf, int len, bool demo) {}
void fatal(const char *s, ...)
{
    defvformatstring(msg,s,s);
    defformatstring(out)("AssaultCube fatal error: %s", msg);
    xlog(ACLOG_ERROR, "%s", out);
    puts(out);
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

    setlocale(LC_ALL, "POSIX");
    entropy_init(time(NULL) + (uint)(size_t)&scl + (uint)(size_t)"" + (uint)(size_t)entropy_init + (int)enet_time_get());

    for(int i = 1; i < argc; i++)
    {
        if(!scl.checkarg(argv[i]))
        {
            if (!strncmp(argv[i],"--wizard",8)) return wizardmain(argc, argv);
            else printf("WARNING: unknown commandline argument \"%s\"\n", argv[i]);
        }
    }
#ifdef _DEBUG
    defformatstring(spdoc)("%s/docs/serverparameters.cfg", scl.logfilepath);
    serverparameters_dumpdocu(spdoc);
#endif
    if(scl.service && !svcctrl)
    {
        #ifdef WIN32
        svcctrl = new winservice(scl.service, argc, argv);
        #endif
        if(svcctrl) svcctrl->start();
    }

    if(enet_initialize() < 0) fatal("Unable to initialise network module");
    initserver(true);
    return EXIT_SUCCESS;

    #if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
    } __except(stackdumper(0, GetExceptionInformation()), EXCEPTION_CONTINUE_SEARCH) { return 0; }
    #endif
}
#endif

