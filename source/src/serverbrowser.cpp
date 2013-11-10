// serverbrowser.cpp: eihrul's concurrent resolver, and server browser window management

#include "cube.h"

#include "SDL_thread.h"

extern bool isdedicated;

struct resolverthread
{
    SDL_Thread *thread;
    const char *query;
    int starttime;
};

struct resolverresult
{
    const char *query;
    ENetAddress address;
};

vector<resolverthread> resolverthreads;
vector<const char *> resolverqueries;
vector<resolverresult> resolverresults;
SDL_mutex *resolvermutex;
SDL_cond *querycond, *resultcond;

#define RESOLVERTHREADS 1
#define RESOLVERLIMIT 3000

int resolverloop(void * data)
{
    resolverthread *rt = (resolverthread *)data;
    SDL_LockMutex(resolvermutex);
    SDL_Thread *thread = rt->thread;
    SDL_UnlockMutex(resolvermutex);
    if(!thread || SDL_GetThreadID(thread) != SDL_ThreadID())
        return 0;
    while(thread == rt->thread)
    {
        SDL_LockMutex(resolvermutex);
        while(resolverqueries.empty()) SDL_CondWait(querycond, resolvermutex);
        rt->query = resolverqueries.pop();
        rt->starttime = totalmillis;
        SDL_UnlockMutex(resolvermutex);

        ENetAddress address = { ENET_HOST_ANY, ENET_PORT_ANY };
        enet_address_set_host(&address, rt->query);

        SDL_LockMutex(resolvermutex);
        if(rt->query && thread == rt->thread)
        {
            resolverresult &rr = resolverresults.add();
            rr.query = rt->query;
            rr.address = address;
            rt->query = NULL;
            rt->starttime = 0;
            SDL_CondSignal(resultcond);
        }
        SDL_UnlockMutex(resolvermutex);
    }
    return 0;
}

void resolverinit()
{
    resolvermutex = SDL_CreateMutex();
    querycond = SDL_CreateCond();
    resultcond = SDL_CreateCond();

    SDL_LockMutex(resolvermutex);
    loopi(RESOLVERTHREADS)
    {
        resolverthread &rt = resolverthreads.add();
        rt.query = NULL;
        rt.starttime = 0;
        rt.thread = SDL_CreateThread(resolverloop, &rt);
    }
    SDL_UnlockMutex(resolvermutex);
}

void resolverstop(resolverthread &rt)
{
    SDL_LockMutex(resolvermutex);
    if(rt.query)
    {
#ifndef __APPLE__
        SDL_KillThread(rt.thread);
#endif
        rt.thread = SDL_CreateThread(resolverloop, &rt);
    }
    rt.query = NULL;
    rt.starttime = 0;
    SDL_UnlockMutex(resolvermutex);
}

void resolverclear()
{
    if(resolverthreads.empty()) return;

    SDL_LockMutex(resolvermutex);
    resolverqueries.shrink(0);
    resolverresults.shrink(0);
    loopv(resolverthreads)
    {
        resolverthread &rt = resolverthreads[i];
        resolverstop(rt);
    }
    SDL_UnlockMutex(resolvermutex);
}

void resolverquery(const char *name)
{
    if(resolverthreads.empty()) resolverinit();

    SDL_LockMutex(resolvermutex);
    resolverqueries.add(name);
    SDL_CondSignal(querycond);
    SDL_UnlockMutex(resolvermutex);
}

bool resolvercheck(const char **name, ENetAddress *address)
{
    bool resolved = false;
    SDL_LockMutex(resolvermutex);
    if(!resolverresults.empty())
    {
        resolverresult &rr = resolverresults.pop();
        *name = rr.query;
        address->host = rr.address.host;
        resolved = true;
    }
    else loopv(resolverthreads)
    {
        resolverthread &rt = resolverthreads[i];
        if(rt.query && totalmillis - rt.starttime > RESOLVERLIMIT)
        {
            resolverstop(rt);
            *name = rt.query;
            resolved = true;
        }
    }
    SDL_UnlockMutex(resolvermutex);
    return resolved;
}

extern bool isdedicated;

bool resolverwait(const char *name, ENetAddress *address)
{
    if(isdedicated) return enet_address_set_host(address, name) >= 0;

    if(resolverthreads.empty()) resolverinit();

    defformatstring(text)("resolving %s... (esc to abort)", name);
    show_out_of_renderloop_progress(0, text);

    SDL_LockMutex(resolvermutex);
    resolverqueries.add(name);
    SDL_CondSignal(querycond);
    int starttime = SDL_GetTicks(), timeout = 0;
    bool resolved = false;
    for(;;)
    {
        SDL_CondWaitTimeout(resultcond, resolvermutex, 250);
        loopv(resolverresults) if(resolverresults[i].query == name)
        {
            address->host = resolverresults[i].address.host;
            resolverresults.remove(i);
            resolved = true;
            break;
        }
        if(resolved) break;

        timeout = SDL_GetTicks() - starttime;
        show_out_of_renderloop_progress(min(float(timeout)/RESOLVERLIMIT, 1.0f), text);
        SDL_Event event;
        while(SDL_PollEvent(&event))
        {
            if(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) timeout = RESOLVERLIMIT + 1;
        }

        if(timeout > RESOLVERLIMIT) break;
    }
    if(!resolved && timeout > RESOLVERLIMIT)
    {
        loopv(resolverthreads)
        {
            resolverthread &rt = resolverthreads[i];
            if(rt.query == name) { resolverstop(rt); break; }
        }
    }
    SDL_UnlockMutex(resolvermutex);
    return resolved;
}

SDL_Thread *connthread = NULL;
SDL_mutex *connmutex = NULL;
SDL_cond *conncond = NULL;

struct connectdata
{
    ENetSocket sock;
    ENetAddress address;
    int result;
};

// do this in a thread to prevent timeouts
// could set timeouts on sockets, but this is more reliable and gives more control
int connectthread(void *data)
{
    SDL_LockMutex(connmutex);
    if(!connthread || SDL_GetThreadID(connthread) != SDL_ThreadID())
    {
        SDL_UnlockMutex(connmutex);
        return 0;
    }
    connectdata cd = *(connectdata *)data;
    SDL_UnlockMutex(connmutex);

    int result = enet_socket_connect(cd.sock, &cd.address);

    SDL_LockMutex(connmutex);
    if(!connthread || SDL_GetThreadID(connthread) != SDL_ThreadID())
    {
        enet_socket_destroy(cd.sock);
        SDL_UnlockMutex(connmutex);
        return 0;
    }
    ((connectdata *)data)->result = result;
    SDL_CondSignal(conncond);
    SDL_UnlockMutex(connmutex);

    return 0;
}

#define CONNLIMIT 20000

int connectwithtimeout(ENetSocket sock, const char *hostname, ENetAddress &address)
{
    if(isdedicated)
    {
        int result = enet_socket_connect(sock, &address);
        if(result<0) enet_socket_destroy(sock);
        return result;
    }

    defformatstring(text)("connecting to %s:%d... (esc to abort)", hostname, address.port);
    show_out_of_renderloop_progress(0, text);

    if(!connmutex) connmutex = SDL_CreateMutex();
    if(!conncond) conncond = SDL_CreateCond();
    SDL_LockMutex(connmutex);
    connectdata cd = { sock, address, -1 };
    connthread = SDL_CreateThread(connectthread, &cd);

    int starttime = SDL_GetTicks(), timeout = 0;
    for(;;)
    {
        if(!SDL_CondWaitTimeout(conncond, connmutex, 250))
        {
            if(cd.result<0) enet_socket_destroy(sock);
            break;
        }
        timeout = SDL_GetTicks() - starttime;
        show_out_of_renderloop_progress(min(float(timeout)/CONNLIMIT, 1.0f), text);
        SDL_Event event;
        while(SDL_PollEvent(&event))
        {
            if(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) timeout = CONNLIMIT + 1;
        }
        if(timeout > CONNLIMIT) break;
    }

    /* thread will actually timeout eventually if its still trying to connect
     * so just leave it (and let it destroy socket) instead of causing problems on some platforms by killing it
     */
    connthread = NULL;
    SDL_UnlockMutex(connmutex);

    return cd.result;
}

vector<serverinfo *> servers;
ENetSocket pingsock = ENET_SOCKET_NULL;
int lastinfo = 0;

char *getservername(int n) { return servers[n]->name; }

serverinfo *findserverinfo(ENetAddress address)
{
    loopv(servers) if(servers[i]->address.host == address.host && servers[i]->port == address.port) return servers[i];
    return NULL;
}

serverinfo *getconnectedserverinfo()
{
    extern ENetPeer *curpeer;
    if(!curpeer) return NULL;
    return findserverinfo(curpeer->address);
}

static serverinfo *newserver(const char *name, uint ip = ENET_HOST_ANY, int port = CUBE_DEFAULT_SERVER_PORT, int weight = 0)
{
    serverinfo *si = new serverinfo;
    si->address.host = ip;
    si->address.port = CUBE_SERVINFO_PORT(port);
    si->msweight = weight;
    if(ip!=ENET_HOST_ANY) si->resolved = serverinfo::RESOLVED;

    if(name) copystring(si->name, name);
    else if(ip==ENET_HOST_ANY || enet_address_get_host_ip(&si->address, si->name, sizeof(si->name)) < 0)
    {
        delete si;
        return NULL;
    }
    si->port = port;

    servers.insert(0, si);

    return si;
}

void addserver(const char *servername, int serverport, int weight)
{
    if(serverport <= 0) serverport = CUBE_DEFAULT_SERVER_PORT;

    loopv(servers) if(strcmp(servers[i]->name, servername)==0 && servers[i]->port == serverport) return;

    newserver(servername, ENET_HOST_ANY, serverport, weight);
}

VARP(servpingrate, 1000, 5000, 60000);
VARP(maxservpings, 0, 10, 1000);
VAR(searchlan, 0, 1, 2);

#define PINGBUFSIZE 100
static int pingbuf[PINGBUFSIZE], curpingbuf = 0;

int chooseextping(serverinfo *si)
{
    if(si->getinfo != EXTPING_NOP) return si->getinfo;
    if(!si->uplinkqual_age || totalmillis - si->uplinkqual_age > 60 * 1000) return EXTPING_UPLINKSTATS;
    return EXTPING_NOP;
}

void pingservers(bool issearch, serverinfo *onlyconnected)
{
    if(pingsock == ENET_SOCKET_NULL)
    {
        pingsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
        if(pingsock == ENET_SOCKET_NULL)
        {
            lastinfo = totalmillis;
            return;
        }
        enet_socket_set_option(pingsock, ENET_SOCKOPT_NONBLOCK, 1);
        enet_socket_set_option(pingsock, ENET_SOCKOPT_BROADCAST, 1);
    }
    ENetBuffer buf;
    static uchar ping[MAXTRANS];
    ucharbuf p(ping, sizeof(ping));
    curpingbuf = (curpingbuf + 1) % PINGBUFSIZE;
    pingbuf[(curpingbuf + PINGBUFSIZE / 2) % PINGBUFSIZE] = 0;
    pingbuf[curpingbuf] = onlyconnected ? 0 : totalmillis;
    putint(p, curpingbuf + 1); // offset by 1 to avoid extinfo trigger
    int baselen = p.length();
    if(onlyconnected)
    {
        serverinfo *si = onlyconnected;
        //p.len = baselen;
        if(si->getinfo == EXTPING_SERVERINFO)
        {
            putint(p, EXTPING_SERVERINFO);
            const char *silang;
            if(strlen(lang) != 2) silang = "en";
            else silang = lang;
            loopi(2) putint(p, silang[i]);
        }
        else putint(p, issearch ? EXTPING_NAMELIST : chooseextping(si));
        buf.data = ping;
        buf.dataLength = p.length();
        enet_socket_send(pingsock, &si->address, &buf, 1);
    }
    else if(searchlan < 2)
    {
        static int lastping = 0;
        if(lastping >= servers.length()) lastping = 0;
        loopi(maxservpings ? min(servers.length(), maxservpings) : servers.length())
        {
            serverinfo &si = *servers[lastping];
            if(++lastping >= servers.length()) lastping = 0;
            if(si.address.host == ENET_HOST_ANY) continue;
            p.len = baselen;
            putint(p, issearch ? EXTPING_NAMELIST : chooseextping(&si));
            buf.data = ping;
            buf.dataLength = p.length();
            enet_socket_send(pingsock, &si.address, &buf, 1);
        }
    }
    if(searchlan && !onlyconnected)
    {
        ENetAddress address;
        address.host = ENET_HOST_BROADCAST;
        address.port = CUBE_SERVINFO_PORT_LAN;
        p.len = baselen;
        putint(p, issearch ? EXTPING_NAMELIST : EXTPING_UPLINKSTATS);
        buf.data = ping;
        buf.dataLength = p.length();
        enet_socket_send(pingsock, &address, &buf, 1);
    }
    lastinfo = totalmillis;
}

void checkresolver()
{
    int resolving = 0;
    loopv(servers)
    {
        serverinfo &si = *servers[i];
        if(si.resolved == serverinfo::RESOLVED) continue;
        if(si.address.host == ENET_HOST_ANY)
        {
            if(si.resolved == serverinfo::UNRESOLVED) { si.resolved = serverinfo::RESOLVING; resolverquery(si.name); }
            resolving++;
        }
    }
    if(!resolving) return;

    const char *name = NULL;
    ENetAddress addr = { ENET_HOST_ANY, ENET_PORT_ANY };
    while(resolvercheck(&name, &addr))
    {
        loopv(servers)
        {
            serverinfo &si = *servers[i];
            if(name == si.name)
            {
                si.resolved = serverinfo::RESOLVED;
                si.address.host = addr.host;
                addr.host = ENET_HOST_ANY;
                break;
            }
        }
    }
}

#define MAXINFOLINELEN 100  // including color codes

void checkpings()
{
    if(pingsock == ENET_SOCKET_NULL) return;
    enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
    ENetBuffer buf;
    ENetAddress addr;
    static uchar ping[MAXTRANS];
    static char text[MAXTRANS];
    buf.data = ping;
    buf.dataLength = sizeof(ping);
    while(enet_socket_wait(pingsock, &events, 0) >= 0 && events)
    {
        int len = enet_socket_receive(pingsock, &addr, &buf, 1);
        if(len <= 0) return;
        serverinfo *si = NULL;
        loopv(servers) if(addr.host == servers[i]->address.host && addr.port == servers[i]->address.port)
        {
            si = servers[i];
            break;
        }
        if(!si && searchlan) si = newserver(NULL, addr.host, CUBE_SERVINFO_TO_SERV_PORT(addr.port));
        if(!si) continue;

        ucharbuf p(ping, len);
        si->lastpingmillis = totalmillis;
        int pingtm = pingbuf[(getint(p) - 1) % PINGBUFSIZE];
        if(pingtm) si->ping = totalmillis - pingtm;
        int query = getint(p);
        switch(query)
        { // cleanup additional query info
            case EXTPING_SERVERINFO:
                loopi(2) getint(p);
                break;
        }
        si->protocol = getint(p);
        if(si->protocol!=PROTOCOL_VERSION) si->ping = 9998;
        si->mode = getint(p);
        si->numplayers = getint(p);
        si->minremain = getint(p);
        getstring(text, p);
        filtertext(si->map, text, 1);
        getstring(text, p);
        filterservdesc(si->sdesc, text);
        copystring(si->description, si->sdesc);
        si->maxclients = getint(p);
        if(p.remaining())
        {
            si->pongflags = getint(p);
            if(p.remaining() && getint(p) == query)
            {
                switch(query)
                {
                    #define RESETINFOLINES() si->infotexts.setsize(0);   \
                                             ucharbuf q(si->textdata, sizeof(si->textdata))
                    #define ADDINFOLINE(msg) si->infotexts.add((char *)si->textdata + q.length()); \
                                             sendstring(msg, q)
                    case EXTPING_NAMELIST:
                    {
                        si->playernames.setsize(0);
                        ucharbuf q(si->namedata, sizeof(si->namedata));
                        loopi(si->numplayers)
                        {
                            getstring(text, p);
                            filtertext(text, text, 0);
                            if(text[0] && !p.overread())
                            {
                                si->playernames.add((const char *)si->namedata + q.length());
                                sendstring(text, q);
                            }
                            else break;
                        }
                        break;
                    }
                    case EXTPING_SERVERINFO:
                    {
                        RESETINFOLINES();
                        getstring(text, p);
                        if(strlen(text) != 2)
                        {
                            ADDINFOLINE("this server does not provide additional information");
                            break;
                        }
                        strcpy(si->lang, text);
                        while(p.remaining())
                        {
                            getstring(text, p);
                            if(*text && !p.overread())
                            {
                                text[MAXINFOLINELEN] = '\0';
                                cutcolorstring(text, 80);
                                ADDINFOLINE(strcmp(text, ".") ? text : "");
                            }
                            else break;
                        }
                        break;
                    }
                    case EXTPING_MAPROT:
                    {
                        RESETINFOLINES();
                        int n = getint(p);
                        ADDINFOLINE("\f1server map rotation:");
                        ADDINFOLINE("");
                        while(p.remaining())
                        {
                            getstring(text, p);
                            filtertext(text, text, 0);
                            if(*text && !p.overread())
                            {
                                text[MAXINFOLINELEN] = '\0';
                                loopi(n) concatformatstring(text, ", %d", getint(p));
                                ADDINFOLINE(text );
                            }
                            else break;
                        }
                        break;
                    }
                    case EXTPING_UPLINKSTATS:
                    {
                        si->uplinkqual_age = totalmillis;
                        if(si->maxclients > 3)
                        {
                            int maxs = 0, maxc = 0, ts, tc;
                            loopi(si->maxclients - 3)
                            {
                                ts = tc = si->uplinkstats[i + 4] = p.get();
                                if(si->maxclients < 8 || i > 2)
                                {
                                    ts &= 0xF0; tc &= 0x0F;
                                    if(ts > maxs) maxs = ts;
                                    if(ts > 0x40 && tc > maxc) maxc = tc;   // spent time = 2 ^ ((ts >> 4) - 1) * 30 sec, so 0x50 is 8..15 minutes
                                }
                            }
                            if(maxs < 0x90) maxc -= 2;                  // go easy on fresh started servers
                            if(maxs < 0x50) si->uplinkqual = 3;
                            else if(maxc < 3) si->uplinkqual = 5;       // choke_percentage = 1 / 2 ^ (15 - tc), so 0x03 is 0.02%
                            else if(maxc < 6) si->uplinkqual = 4;       // 0.2%
                            else if(maxc == 6) si->uplinkqual = 3;
                            else if(maxc < 9) si->uplinkqual = 2;       // 1.6% (yep, that IS bad)
                            else si->uplinkqual = 1;
                        }
                        if(si->getinfo == EXTPING_UPLINKSTATS)
                        {
                            RESETINFOLINES();
                            if(si->maxclients < 4)
                            {
                                ADDINFOLINE("server is too small for uplink quality statistics");
                            }
                            else
                            {
                                ADDINFOLINE("\f1server uplink quality and usage statistics:");
                                ADDINFOLINE("");
                                ADDINFOLINE("players:\terrors/time");
                                for(int i = 4; i <= si->maxclients; i++)
                                {
                                    defformatstring(msg)("   %d\t", i);
                                    loopj(15) concatformatstring(msg, "\a%c ", '0' + ((si->uplinkstats[i] & 0x0F) > j) + 2 * ((si->uplinkstats[i] & 0xF0) > (j << 4)));
                                    concatformatstring(msg, "\t\t [%02X]", si->uplinkstats[i]);
                                    ADDINFOLINE(msg);
                                }
                                ADDINFOLINE("");
                                ADDINFOLINE("\f4red bar: error rate, green bar: time played");
                            }
                        }
                        break;
                    }
                    #undef RESETINFOLINES
                    #undef ADDINFOLINE
                }
            }
        }
        else
        {
            si->pongflags = 0;
        }
        if(si->getinfo == query) si->getinfo = EXTPING_NOP;
        if(si->pongflags > 0)
        {
            const char *sp = "";
            int mm = si->pongflags >> PONGFLAG_MASTERMODE;
            if(si->pongflags & (1 << PONGFLAG_BANNED))
                sp = "you are banned from this server";
            if(si->pongflags & (1 << PONGFLAG_BLACKLIST))
                sp = "you are blacklisted on this server";
            else if(si->pongflags & (1 << PONGFLAG_PASSWORD))
                sp = "this server is password-protected";
            else if(mm) sp = mmfullname(mm);
            formatstring(si->description)("%s  \f1(%s)", si->sdesc, sp);
        }
    }
}

enum { SBS_PING = 0, SBS_NUMPL, SBS_MAXPL, SBS_MINREM, SBS_MAP, SBS_MODE, SBS_IP, SBS_DESC, NUMSERVSORT };

VARP(serversort, 0, 0, NUMSERVSORT-1);
VARP(serversortdir, 0, 0, 1);
VARP(showonlygoodservers, 0, 0, 1);
VAR(shownamesinbrowser, 0, 0, 1);
VARP(showminremain, 0, 0, 1);
VARP(serversortpreferofficial, 0, 1, 1);

void serversortprepare()
{
    loopv(servers)
    {
        serverinfo &si = *servers[i];
        // basic group weights: used(700) - empty(500) - unusable(200)
        if(si.protocol != PROTOCOL_VERSION) si.weight += 200;
        else if(!si.numplayers) si.weight += 500;
        else
        {
            si.weight += 700;
            if(serversortpreferofficial && securemapcheck(si.map, false)) si.weight += 100;
        }
        si.weight += si.msweight;
    }
}

int sicompare(serverinfo **ap, serverinfo **bp)
{
    serverinfo *a = *ap, *b = *bp;
    int dir = serversortdir ? -1 : 1;
    if(a->weight > b->weight) return -dir;
    if(a->weight < b->weight) return dir;
    enet_uint32 ai = ENET_NET_TO_HOST_32(a->address.host), bi = ENET_NET_TO_HOST_32(b->address.host);
    int ips = ai < bi ? -dir : (ai > bi ? dir : 0);  // most steady base sorting
    switch(serversort)
    {
        case SBS_NUMPL: // player number
            if(a->numplayers < b->numplayers) return dir;
            if(a->numplayers > b->numplayers) return -dir;
            break;
        case SBS_MAXPL: // maxplayers
            if(a->maxclients < b->maxclients) return dir;
            if(a->maxclients > b->maxclients) return -dir;
            break;
        case SBS_MINREM: // minutes remaining
            if(a->minremain < b->minremain) return dir;
            if(a->minremain > b->minremain) return -dir;
            break;
        case SBS_DESC: // description
        {
            static string ad, bd;
            filtertext(ad, a->sdesc);
            filtertext(bd, b->sdesc);
            if(!ad[0] && bd[0]) return dir;
            if(ad[0] && !bd[0]) return -dir;
            int mdir = dir * strcasecmp(ad, bd);
            if(mdir) return mdir;
            break;
        }
        case SBS_IP: // IP
            if(ips) return ips;
            break;
        case SBS_MAP: // map
        {
            int mdir = dir * strcasecmp(a->map, b->map);
            if(mdir) return mdir;
            break;
        }
        case SBS_MODE: // mode
        {
            const char *am = modestr(a->mode, modeacronyms > 0), *bm = modestr(b->mode, modeacronyms > 0);
            int mdir = dir * strcasecmp(am, bm);
            if(mdir) return mdir;
            break;
        }
    }
    if(serversort ==  SBS_PING || (!a->numplayers && !b->numplayers)) // ping
    {
        if(a->ping > b->ping) return dir;
        if(a->ping < b->ping) return -dir;
    }
    if(ips) return ips;
    if(a->port > b->port) return dir;
    else return -dir;
}

void *servmenu = NULL, *searchmenu = NULL, *serverinfomenu = NULL;
vector<char *> namelists;

string cursearch, cursearchuc;

void searchnickname(const char *name)
{
    if(!name || !name[0]) return;
    copystring(cursearch, name);
    copystring(cursearchuc, name);
    strtoupper(cursearchuc);
    showmenu("search");
}
COMMAND(searchnickname, "s");

VAR(showallservers, 0, 1, 1);

bool matchplayername(const char *name)
{
    static string nameuc;
    copystring(nameuc, name);
    strtoupper(nameuc);
    return strstr(nameuc, cursearchuc) != NULL;
}

VARP(serverbrowserhideip, 0, 0, 2);
VARP(serverbrowserhidefavtag, 0, 1, 2);
VAR(showweights, 0, 0, 1);
VARP(hidefavicons, 0, 0, 1);

vector<char *> favcats;
const char *fc_als[] = { "weight", "tag", "desc", "red", "green", "blue", "alpha", "keys", "ignore", "image" };
enum { FC_WEIGHT = 0, FC_TAG, FC_DESC, FC_RED, FC_GREEN, FC_BLUE, FC_ALPHA, FC_KEYS, FC_IGNORE, FC_IMAGE, FC_NUM };

VARF(showonlyfavourites, 0, 0, 100,
{
    if(showonlyfavourites > favcats.length())
    {
        conoutf("showonlyfavourites: %d out of range (0..%d)", showonlyfavourites, favcats.length());
        showonlyfavourites = 0;
    }
});

const char *favcatargname(const char *refdes, int par)
{
    static string text[3];
    static int i = 0;
    if(par < 0 || par >= FC_NUM) return NULL;
    i = (i + 1) % 3;
    formatstring(text[i])("sbfavourite_%s_%s", refdes, fc_als[par]);
    return text[i];
}

void addfavcategory(const char *refdes)
{
    string text, val;
    char alx[FC_NUM];
    if(!refdes) { intret(0); return; }
    filtertext(text, refdes);
    if(!text[0]) { intret(0); return; }
    loopv(favcats) if(!strcmp(favcats[i], text)) { intret(i + 1); return; }
    favcats.add(newstring(text));
    bool oldpersist = per_idents;
    per_idents = false; // only keep changed values
    loopi(FC_NUM) alx[i] = getalias(favcatargname(text, i)) ? 1 : 0;
    loopi(3)
    {
        formatstring(val)("%d", *text & (1 << i) ? 90 : 10);
        if(!alx[i + FC_RED]) alias(favcatargname(text, i + FC_RED), val);
    }
    formatstring(val)("favourites %d", favcats.length());
    const int defk[] = { FC_WEIGHT, FC_DESC, FC_TAG, FC_KEYS, FC_IGNORE, FC_ALPHA };
    const char *defv[] = { "0",     val,     refdes, "",      "",        "20" };
    loopi(sizeof(defk)/sizeof(defk[0])) { if(!alx[defk[i]]) alias(favcatargname(text, defk[i]), defv[i]); }
    per_idents = oldpersist;
    intret(favcats.length());
}

void listfavcats()
{
    const char *str = conc(&favcats[0], favcats.length(), true);
    result(str);
    delete [] str;
}

bool favcatcheckkey(serverinfo &si, const char *key)
{ // IP:port, #gamemode ,%mapname, desc
    string text, keyuc;
    if(isdigit(*key)) // IP
    {
        formatstring(text)("%s:%d", si.name, si.port);
        return !strncmp(text, key, strlen(key));
    }
    else if(si.address.host != ENET_HOST_ANY && si.ping != 9999) switch(*key)
    {
        case '#':
            return si.map[0] && si.mode == atoi(key + 1);
        case '%':
            strtoupper(text, si.map);
            strtoupper(keyuc, key + 1);
            return si.map[0] && key[1] && strstr(text, keyuc);
        case '>':
            return si.ping > atoi(key + 1);
        case '!':
            return !favcatcheckkey(si, key + 1);
        case '+':
            return si.map[0] && favcatcheckkey(si, key + 1);
        case '$':
            if(key[1])
            {
                formatstring(text)("%s \"%s\" %d %d, %d %d %d \"%s\" %d %d", key + 1, si.map, si.mode, si.ping, si.minremain, si.numplayers, si.maxclients, si.name, si.port, si.pongflags);
                filtertext(text, text, 1);
                int cnt = 0;
                for(const char *p = text; (p = strchr(p, '\"')); p++) cnt++;
                return cnt == 4 && execute(text);
            }
            break;
        default:
            filtertext(text, si.sdesc);
            return *key && strstr(text, key);
    }
    return false;
}

const char *favcatcheck(serverinfo &si, const char *ckeys, char *autokeys = NULL)
{
    if(!ckeys) return NULL;
    static char *nkeys = NULL;
    const char *sep = " \t\n\r";
    char *keys = newstring(ckeys), *k = strtok(keys, sep);
    bool res = false;
    DELETEA(nkeys);
    nkeys = newstring(strlen(ckeys));
    nkeys[0] = '\0';
    while(k)
    {
        if(favcatcheckkey(si, k))
        {
            res = true;
            if(autokeys && !(isdigit(*k) && strchr(k, ':'))) concatformatstring(autokeys, *autokeys ? " %s" : "%s", k);
        }
        else
        {
            if(*nkeys) strcat(nkeys, " ");
            strcat(nkeys, k);
        }
        k = strtok(NULL, sep);
    }
    delete[] keys;
    return res ? nkeys : NULL;
}

vector<const char *> favcattags;

bool assignserverfavourites()
{
    int alxn[FC_NUM];
    const char *alx[FC_NUM], *sep = " \t\n\r";
    favcattags.setsize(0);
    bool res = false;
    loopv(servers) { servers[i]->favcat = -1; servers[i]->weight = 0; }
    loopvj(favcats)
    {
        loopi(FC_NUM) { alx[i] = getalias(favcatargname(favcats[j], i)); alxn[i] = alx[i] ? atoi(alx[i]) : 0; }
        favcattags.add(alx[FC_TAG]);
        bool showonlythiscat = j == showonlyfavourites - 1;
        char *keys = newstring(alx[FC_KEYS]), *k = strtok(keys, sep);
        while(k)
        {
            loopv(servers)
            {
                serverinfo &si = *servers[i];
                if(si.address.host == ENET_HOST_ANY || si.ping == 9999 || si.protocol != PROTOCOL_VERSION) continue;
                if((!alxn[FC_IGNORE] || showonlythiscat) && favcatcheckkey(si, k))
                {
                    si.weight += alxn[FC_WEIGHT];
                    res = true;
                    if(si.favcat == -1 || showonlythiscat)
                    {
                        si.favcat = j;
                        if(alxn[FC_ALPHA])
                        {
                            if(!si.bgcolor) si.bgcolor = new color;
                            new (si.bgcolor) color(((float)alxn[FC_RED])/100, ((float)alxn[FC_GREEN])/100, ((float)alxn[FC_BLUE])/100, ((float)alxn[FC_ALPHA])/100);
                        }
                    }
                }
            }
            k = strtok(NULL, sep);
        }
        delete[] keys;
    }
    loopv(servers) if(servers[i]->favcat == -1)
    {
        DELETEA(servers[i]->bgcolor);
    }
    return res;
}

COMMAND(addfavcategory, "s");
COMMAND(listfavcats, "");

static serverinfo *lastselectedserver = NULL;
static bool pinglastselected = false;

void refreshservers(void *menu, bool init)
{
    static int servermenumillis;
    static bool usedselect = false;
    static string title;
    bool issearch = menu == searchmenu;
    bool isinfo = menu == serverinfomenu;
    bool isscoreboard = menu == NULL;

    serverinfo *curserver = getconnectedserverinfo(), *oldsel = NULL;
    if(init)
    {
        loopi(PINGBUFSIZE) if(pingbuf[i] < totalmillis) pingbuf[i] = 0;
        if(resolverthreads.empty()) resolverinit();
        else resolverclear();
        loopv(servers) resolverquery(servers[i]->name);
        servermenumillis = totalmillis;
        usedselect = false;
        if(menu && curserver) oldsel = curserver;
    }

    checkresolver();
    checkpings();
    if(isinfo)
    {
        if(lastselectedserver)
        {
            bool found = false;
            loopv(servers) if(lastselectedserver == servers[i]) { found = true; break; }
            if(!found) lastselectedserver = NULL;
        }
        menutitle(menu, "extended server information (F5: refresh)");
        menureset(menu);
        static string infotext;
        static char dummy = '\0';
        if(lastselectedserver)
        {
            serverinfo &si = *lastselectedserver;
            formatstring(si.full)("%s:%d  %s", si.name, si.port, si.sdesc);
            menumanual(menu, si.full);
            menumanual(menu, &dummy);
            if(si.infotexts.length() && !pinglastselected)
            {
                infotext[0] = '\0';
                loopv(si.infotexts) menuimagemanual(menu, NULL, "bargraphs", si.infotexts[i]);
            }
            else
            {
                copystring(infotext, "-- waiting for server response --");
                if(si.getinfo == EXTPING_NOP) si.getinfo = EXTPING_SERVERINFO;
                if(init || pinglastselected || totalmillis - lastinfo >= servpingrate) pingservers(false, lastselectedserver);
                pinglastselected = false;
            }
        }
        else
            copystring(infotext, "  -- no server selected --");
        if(*infotext) menumanual(menu, infotext);
        return;
    }
    if((init && issearch) || totalmillis - lastinfo >= (servpingrate * (issearch ? 2 : 1))/(maxservpings ? max(1, (servers.length() + maxservpings - 1) / maxservpings) : 1))
        pingservers(issearch, isscoreboard ? curserver : NULL);
    if(!init && menu)// && servers.inrange(((gmenu *)menu)->menusel))
    {
        serverinfo *foundserver = NULL;
        loopv(servers) if(servers[i]->menuline_from == ((gmenu *)menu)->menusel && servers[i]->menuline_to > servers[i]->menuline_from) { foundserver = servers[i]; break; }
        if(foundserver)
        {
            if((usedselect || ((gmenu *)menu)->menusel > 0)) oldsel = foundserver;
            lastselectedserver = foundserver;
        }
    }
    if(isscoreboard) lastselectedserver = curserver;

    bool showfavtag = (assignserverfavourites() || !serverbrowserhidefavtag) && serverbrowserhidefavtag != 2;
    serversortprepare();
    servers.sort(sicompare);
    if(menu)
    {
        static const char *titles[NUMSERVSORT] =
        {
            "%s\fs\f0ping\fr\tplr\tserver%s%s",                               // 0: ping
            "%sping\t\fs\f0plr\fr\tserver%s%s",                               // 1: player number
            "%sping\tplr\tserver (\fs\f0max players\fr)%s%s",                 // 2: maxplayers
            "%sping\tplr\fs\f0\fr\tserver (\fs\f0minutes remaining\fr)%s%s",  // 3: minutes remaining
            "%sping\tplr\tserver (\fs\f0map\fr)%s%s",                         // 4: map
            "%sping\tplr\tserver (\fs\f0game mode\fr)%s%s",                   // 5: mode
            "%sping\tplr\tserver (\fs\f0IP\fr)%s%s",                          // 6: IP
            "%sping\tplr\tserver (\fs\f0description\fr)%s%s"                  // 7: description
        };
        bool showmr = showminremain || serversort == SBS_MINREM;
        formatstring(title)(titles[serversort], showfavtag ? "fav\t" : "", issearch ? "      search results for \f3" : "     (F1: Help/Settings)", issearch ? cursearch : "");
        menutitle(menu, title);
        menureset(menu);
        string text;
        int curnl = 0;
        bool sbconnectexists = identexists("sbconnect");
        loopv(servers)
        {
            serverinfo &si = *servers[i];
            si.menuline_to = si.menuline_from = ((gmenu *)menu)->items.length();
            if( (!showallservers && si.lastpingmillis < servermenumillis) || (si.maxclients>MAXCL && searchlan<2) ) continue; // no pong yet or forbidden
            int banned = ((si.pongflags >> PONGFLAG_BANNED) & 1) | ((si.pongflags >> (PONGFLAG_BLACKLIST - 1)) & 2);
            bool showthisone = !(banned && showonlygoodservers) && !(showonlyfavourites > 0 && si.favcat != showonlyfavourites - 1);
            bool serverfull = si.numplayers >= si.maxclients;
            bool needspasswd = (si.pongflags & (1 << PONGFLAG_PASSWORD)) > 0;
            int mmode = (si.pongflags >> PONGFLAG_MASTERMODE) & MM_MASK;
            char basecolor = banned ? '4' : (curserver == servers[i] ? '1' : '5');
            char plnumcolor = serverfull ? '2' : (needspasswd ? '3' : (mmode != MM_OPEN ? '1' : basecolor));
            const char *favimage = NULL;
            if(si.address.host != ENET_HOST_ANY && si.ping != 9999)
            {
                if(si.protocol!=PROTOCOL_VERSION)
                {
                    if(!showonlygoodservers) formatstring(si.full)("%s:%d [%s]", si.name, si.port, si.protocol<0 ? "modded version" : (si.protocol<PROTOCOL_VERSION ? "older protocol" : "newer protocol"));
                    else showthisone = false;
                }
                else
                {
                    if(!hidefavicons && showfavtag && si.favcat > -1) favimage = getalias(favcatargname(favcats[si.favcat], FC_IMAGE));
                    filterrichtext(text, si.favcat > -1 && !favimage ? favcattags[si.favcat] : "");
                    if(showweights) concatformatstring(text, "(%d)", si.weight);
                    formatstring(si.full)(showfavtag ? (favimage ? "\t" : "\fs%s\fr\t") : "", text);
                    concatformatstring(si.full, "\fs\f%c%s\t\fs\f%c%d/%d\fr\t\a%c  ", basecolor, colorping(si.ping), plnumcolor, si.numplayers, si.maxclients, '0' + si.uplinkqual);
                    if(si.map[0])
                    {
                        concatformatstring(si.full, "%s, %s", si.map, modestr(si.mode, modeacronyms > 0));
                        if(showmr) concatformatstring(si.full, ", (%d)", si.minremain);
                    }
                    else concatformatstring(si.full, "empty");
                    concatformatstring(si.full, serverbrowserhideip < 2 ? ": \fs%s%s:%d\fr" : ": ", serverbrowserhideip == 1 ? "\f4" : "", si.name, si.port);
                    concatformatstring(si.full, "\fr %s", si.sdesc);
                }
            }
            else
            {
                if(!showonlygoodservers) formatstring(si.full)(si.address.host != ENET_HOST_ANY ? "%s:%d [waiting for server response]" : "%s:%d [unknown host]", si.name, si.port);
                else showthisone = false;
            }
            if(issearch && showthisone)
            {
                bool found = false;
                loopvj(si.playernames) if(matchplayername(si.playernames[j])) { found = true; break; };
                if(!found) showthisone = false;
            }
            if(showthisone)
            {
                cutcolorstring(si.full, 76); // cut off too long server descriptions
                cutcolorstring(si.description, 76);
                if(sbconnectexists)
                {
                    filtertext(text, si.sdesc);
                    for(char *p = text; (p = strchr(p, '\"')); *p++ = ' ');
                    text[30] = '\0';
                    formatstring(si.cmd)("sbconnect %s %d %d %d %d %d \"%s\"", si.name, si.port, serverfull ?1:0, needspasswd ?1:0, mmode, banned, text);
                }
                else formatstring(si.cmd)("connect %s %d", si.name, si.port);
                menuimagemanual(menu, favimage, "serverquality", si.full, si.cmd, si.bgcolor, si.description);
                if(!issearch && servers[i] == oldsel)
                {
                    ((gmenu *)menu)->menusel = ((gmenu *)menu)->items.length() - 1;
                    usedselect = true;
                    if(shownamesinbrowser) si.getinfo = EXTPING_NAMELIST;
                }
                if((shownamesinbrowser && servers[i] == oldsel && si.playernames.length()) || issearch)
                {
                    int cur = 0;
                    char *t = NULL;
                    loopvj(si.playernames)
                    {
                        if(cur == 0)
                        {
                            if(namelists.length() < ++curnl) namelists.add(newstringbuf());
                            t = namelists[curnl - 1];
                            copystring(t, showfavtag ? "\t\t" : "\t");
                        }
                        concatformatstring(t, " \t\fs%s%s\fr", !issearch || matchplayername(si.playernames[j]) ? "" : "\f4" ,si.playernames[j]);
                        cur++;
                        if(cur == 4)
                        {
                            menumanual(menu, t, NULL, NULL, NULL);
                            cur = 0;
                        }
                    }
                    if(cur) menumanual(menu, t, NULL, NULL, NULL);
                }
            }
            si.menuline_to = ((gmenu *)menu)->items.length();
        }
        static string notfoundmsg;
        if(issearch)
        {
            if(curnl == 0)
            {
                formatstring(notfoundmsg)("\t\tpattern \fs\f3%s\fr not found.", cursearch);
                menumanual(menu, notfoundmsg, NULL, NULL, NULL);
            }
        }
        else if(!((gmenu *)menu)->items.length() && showonlyfavourites && favcats.inrange(showonlyfavourites - 1))
        {
            const char *desc = getalias(favcatargname(favcats[showonlyfavourites - 1], FC_DESC));
            formatstring(notfoundmsg)("no servers in category \f2%s", desc ? desc : favcattags[showonlyfavourites - 1]);
            menumanual(menu, notfoundmsg, NULL, NULL, NULL);
        }
    }
}

bool serverskey(void *menu, int code, bool isdown, int unicode)
{
    const int fk[] = { SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9, SDLK_0 };
    if(!isdown) return false;
    loopi(sizeof(fk)/sizeof(fk[0])) if(code == fk[i] && favcats.inrange(i))
    {
        int sel = ((gmenu *)menu)->menusel;
        loopvj(servers) if(menu && (servers[j]->menuline_from <= sel && servers[j]->menuline_to > sel))
        {
            string ak; ak[0] = '\0';
            const char *keyalias = favcatargname(favcats[i], FC_KEYS), *key = getalias(keyalias), *rest = favcatcheck(*servers[j], key, ak), *desc = getalias(favcatargname(favcats[i], FC_DESC));
            if(!desc) desc = "";
            if(*ak)
            { // server was automatically added to this favourite group, don't remove
                conoutf(_("server \"%cs%s%cr\" is in category '%cs%s%cr' because of key '%s', please remove manually"), CC, servers[j]->sdesc, CC, CC, desc, CC, ak);
            }
            else if(rest)
            { // remove from favourite group
                conoutf(_("removing server \"%cs%s%cr\" from favourites category '%cs%s%cr' (rest '%s')"), CC, servers[j]->sdesc, CC, CC, desc, CC, rest);
                alias(keyalias, rest);
            }
            else
            { // add IP:port to group
                defformatstring(text)("%s:%d", servers[j]->name, servers[j]->port);
                if(key && *key)
                {
                    char *newkey = newstring(key, strlen(text) + 1 + strlen(key));
                    strcat(newkey, " ");
                    strcat(newkey, text);
                    alias(keyalias, newkey);
                    delete[] newkey;
                }
                else alias(keyalias, text);
                conoutf(_("adding server \"%cs%s%cr\" to favourites category '%cs%s%cr' (new '%s')"), CC, servers[j]->sdesc, CC, CC, desc, CC, getalias(keyalias));
            }
            return true;
        }
    }
    switch(code)
    {
        case SDLK_HOME:
            if(menu) ((gmenu *)menu)->menusel = 0;
            return true;

        case SDLK_LEFT:
            serversort = (serversort+NUMSERVSORT-1) % NUMSERVSORT;
            return true;

        case SDLK_RIGHT:
            serversort = (serversort+1) % NUMSERVSORT;
            return true;

        case SDLK_F5:
            updatefrommaster(1);
            return true;

        case SDLK_F6:
            serversortdir = serversortdir ? 0 : 1;
            return true;

        case SDLK_F9:
            showmenu("serverinfo");
            return true;
    }
    if(menu == searchmenu) return false;
    switch(code)
    {
        case SDLK_F1:
            showmenu("serverbrowser help");
            return true;

        case SDLK_F2:
            shownamesinbrowser = shownamesinbrowser ? 0 : 1;
            return true;

        case SDLK_F3:
            showmenu("search player");
            return true;

        case SDLK_F4:
            showmenu("edit favourites");
            return true;

        case SDLK_F7:
            showonlygoodservers = showonlygoodservers ? 0 : 1;
            return true;

        case SDLK_F8:
            showminremain = showminremain ? 0 : 1;
            return true;
    }
    return false;
}

bool serverinfokey(void *menu, int code, bool isdown, int unicode)
{
    if(!isdown) return false;
    switch(code)
    {
        case SDLK_HOME:
            if(menu) ((gmenu *)menu)->menusel = 0;
            return true;

        case SDLK_F5:
            if(lastselectedserver) lastselectedserver->getinfo = EXTPING_SERVERINFO;
            pinglastselected = true;
            return true;

        case SDLK_F1:
            showmenu("serverinfo help");  // feel free to actually write this menu ;)
            return true;

        case SDLK_F2:  // yep: undocumented :)
            if(lastselectedserver) lastselectedserver->getinfo = EXTPING_UPLINKSTATS;
            pinglastselected = true;
            break;

        case SDLK_m:
            if(lastselectedserver) lastselectedserver->getinfo = EXTPING_MAPROT;
            pinglastselected = true;
            break;
    }
    return false;
}

void clearservers()
{
    resolverclear();
    servers.deletecontents();
}

#define RETRIEVELIMIT 5000
extern char *global_name;
bool cllock = false, clfail = false;

struct resolver_data
{
    int timeout, starttime;
    string text;
};

static int progress_callback(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
    resolver_data *rd = (resolver_data *)clientp;
    rd->timeout = SDL_GetTicks() - rd->starttime;
    show_out_of_renderloop_progress(min(float(rd->timeout)/RETRIEVELIMIT, 1.0f), rd->text);
    if(interceptkey(SDLK_ESCAPE))
    {
        loadingscreen();
        return 1;
    }
    return 0;
}

static size_t write_callback(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

void retrieveservers(vector<char> &data)
{
    if(mastertype == AC_MASTER_HTTP)
    {
        string request;
        sprintf(request, "http://%s/retrieve.do?action=list&name=%s&version=%d&build=%d", mastername, global_name, AC_VERSION, getbuildtype()|(1<<16));

        const char *tmpname = findfile(path("config/servers.cfg", true), "wb");
        FILE *outfile = fopen(tmpname, "w+");
        if(!outfile)
        {
            conoutf("\f3cannot write server list");
            return;
        }

        resolver_data *rd = new resolver_data();
        formatstring(rd->text)("retrieving servers from %s:%d... (esc to abort)", mastername, masterport);
        show_out_of_renderloop_progress(0, rd->text);

        rd->starttime = SDL_GetTicks();
        rd->timeout = 0;

        CURL *curl = curl_easy_init();
        int result = 0, httpresult = 0;

        curl_easy_setopt(curl, CURLOPT_URL, request);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);	// Fixes crashbug for some buggy libcurl versions (Linux)
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, outfile);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, rd);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, RETRIEVELIMIT/1000);
        result = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpresult);
        curl_easy_cleanup(curl);
        curl = NULL;
        if(outfile) fclose(outfile);

        if(result == CURLE_OPERATION_TIMEDOUT || result == CURLE_COULDNT_RESOLVE_HOST)
        {
            clfail = true;
        }
        else clfail = false;

        if(!result && httpresult == 200)
        {
            int size = 0;
            char *content = loadfile(path("config/servers.cfg", true), &size);
            data.shrink(0);
            data.insert(0, content, size);
            if(data.length()) data.add('\0');
        }
        DELETEP(rd);
    }
    else
    {
        ENetSocket sock = connectmaster();
        if(sock == ENET_SOCKET_NULL)
        {
            conoutf("Master server is not replying.");
            clfail = true;
            return;
        }
        clfail = false;
        defformatstring(text)("retrieving servers from %s:%d... (esc to abort)", mastername, masterport);
        show_out_of_renderloop_progress(0, text);
        int starttime = SDL_GetTicks(), timeout = 0;
        string request;
        sprintf(request, "list %s %d %d\n",global_name,AC_VERSION,getbuildtype());
        const char *req = request;
        int reqlen = strlen(req);
        ENetBuffer buf;
        while(reqlen > 0)
        {
            enet_uint32 events = ENET_SOCKET_WAIT_SEND;
            if(enet_socket_wait(sock, &events, 250) >= 0 && events)
            {
                buf.data = (void *)req;
                buf.dataLength = reqlen;
                int sent = enet_socket_send(sock, NULL, &buf, 1);
                if(sent < 0) break;
                req += sent;
                reqlen -= sent;
                if(reqlen <= 0) break;
            }
            timeout = SDL_GetTicks() - starttime;
            show_out_of_renderloop_progress(min(float(timeout)/RETRIEVELIMIT, 1.0f), text);
            if(interceptkey(SDLK_ESCAPE)) timeout = RETRIEVELIMIT + 1;
            if(timeout > RETRIEVELIMIT) break;
        }
        if(reqlen <= 0) for(;;)
        {
            enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
            if(enet_socket_wait(sock, &events, 250) >= 0 && events)
            {
                if(data.length() >= data.capacity()) data.reserve(4096);
                buf.data = data.getbuf() + data.length();
                buf.dataLength = data.capacity() - data.length();
                int recv = enet_socket_receive(sock, NULL, &buf, 1);
                if(recv <= 0) break;
                data.advance(recv);
            }
            timeout = SDL_GetTicks() - starttime;
            show_out_of_renderloop_progress(min(float(timeout)/RETRIEVELIMIT, 1.0f), text);
            if(interceptkey(SDLK_ESCAPE)) timeout = RETRIEVELIMIT + 1;
            if(timeout > RETRIEVELIMIT) break;
        }
        if(data.length()) data.add('\0');
        enet_socket_destroy(sock); 
    }
}

VARP(masterupdatefrequency, 1, 60*60, 24*60*60);

void updatefrommaster(int force)
{
    static int lastupdate = 0;
    if(lastupdate==0) cllock = true;
    if(!force && lastupdate && totalmillis-lastupdate<masterupdatefrequency*1000) return;

    vector<char> data;
    retrieveservers(data);

    if(data.empty())
    {
        if (!clfail) conoutf("Master server is not replying. \f1Get more information at http://masterserver.cubers.net/");
        cllock = !clfail;
    }
    else
    {
        // preserve currently connected server from deletion
        serverinfo *curserver = getconnectedserverinfo();
        string curname;
        if(curserver) copystring(curname, curserver->name);

        clearservers();
        if(!strncmp(data.getbuf(), "addserver", 9)) cllock = false; // the ms could reply other thing... but currently, this is useless
        if(!cllock )
        {
            execute(data.getbuf());
            if(curserver) addserver(curname, curserver->port, curserver->msweight);
        }
        lastupdate = totalmillis;
    }
}

COMMANDF(addserver, "sii", (char *n, int *p, int *w) { addserver(n, *p, *w); });
COMMAND(clearservers, "");
COMMANDF(updatefrommaster, "i", (int *f) { updatefrommaster(*f); });

void writeservercfg()
{
    stream *f = openfile(path("config/servers.cfg", true), "w");
    if(!f) return;
    f->printf("// servers connected to are added here automatically\n");
    loopvrev(servers)
    {
        f->printf("\naddserver %s %d", servers[i]->name, servers[i]->port);
        if(servers[i]->msweight) f->printf(" %d", servers[i]->msweight);
    }
    f->printf("\n");
    delete f;
}
