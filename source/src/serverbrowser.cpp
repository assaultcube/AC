// serverbrowser.cpp: eihrul's concurrent resolver, and server browser window management

#include "pch.h"
#include "cube.h"
#ifdef __APPLE__
#include <pthread.h>
#endif
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
    resolverqueries.setsize(0);
    resolverresults.setsize(0);
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

    s_sprintfd(text)("resolving %s... (esc to abort)", name);
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

    s_sprintfd(text)("connecting to %s... (esc to abort)", hostname);
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

static serverinfo *newserver(const char *name, uint ip = ENET_HOST_ANY, int port = CUBE_DEFAULT_SERVER_PORT)
{
    serverinfo *si = new serverinfo;
    si->address.host = ip;
    si->address.port = CUBE_SERVINFO_PORT(port);
    if(ip!=ENET_HOST_ANY) si->resolved = serverinfo::RESOLVED;

    if(name) s_strcpy(si->name, name);
    else if(ip==ENET_HOST_ANY || enet_address_get_host_ip(&si->address, si->name, sizeof(si->name)) < 0)
    {
        delete si;
        return NULL;
    }
    si->port = port;

    servers.insert(0, si);

    return si;
}

void addserver(const char *servername, const char *serverport)
{
    int port = atoi(serverport);
    if(port == 0) port = CUBE_DEFAULT_SERVER_PORT;

    loopv(servers) if(strcmp(servers[i]->name, servername)==0 && servers[i]->port == port) return;

    newserver(servername, ENET_HOST_ANY, port);
}

VARP(servpingrate, 1000, 5000, 60000);
VARP(maxservpings, 0, 0, 1000);
VAR(searchlan, 0, 1, 2);

#define PINGBUFSIZE 100
static int pingbuf[PINGBUFSIZE], curpingbuf = 0;

void pingservers(bool issearch, bool onlyconnected)
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
    pingbuf[curpingbuf] = totalmillis;
    putint(p, curpingbuf + 1); // offset by 1 to avoid extinfo trigger
    int baselen = p.length();
    if(onlyconnected)
    {
        serverinfo *si = getconnectedserverinfo();
        if(si)
        {
            //p.len = baselen;
            putint(p, si->getnames || issearch ? EXTPING_NAMELIST : EXTPING_NOP);
            buf.data = ping;
            buf.dataLength = p.length();
            enet_socket_send(pingsock, &si->address, &buf, 1);
        }
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
            putint(p, si.getnames || issearch ? EXTPING_NAMELIST : EXTPING_NOP);
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
        putint(p, issearch ? EXTPING_NAMELIST : EXTPING_NOP);
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
        si->ping = pingtm ? totalmillis - pingtm : 9997;
        int query = getint(p);
        si->protocol = getint(p);
        if(si->protocol!=PROTOCOL_VERSION) si->ping = 9998;
        si->mode = getint(p);
        si->numplayers = getint(p);
        si->minremain = getint(p);
        getstring(text, p);
        filtertext(si->map, text, 1);
        getstring(text, p);
        filterservdesc(si->sdesc, text);
        s_strcpy(si->description, si->sdesc);
        si->maxclients = getint(p);
        if(p.remaining())
        {
            si->pongflags = getint(p);
            if(p.remaining() && getint(p) == query)
            {
                switch(query)
                {
                    case EXTPING_NAMELIST:
                    {
                        si->playernames.setsizenodelete(0);
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
                }
            }
        }
        else
        {
            si->pongflags = 0;
        }
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
            s_sprintf(si->description)("%s  \f1(%s)", si->sdesc, sp);
        }
    }
}

enum { SBS_PING = 0, SBS_NUMPL, SBS_MAXPL, SBS_MINREM, SBS_MAP, SBS_MODE, SBS_IP, SBS_DESC, NUMSERVSORT };

VARP(serversort, 0, 0, NUMSERVSORT-1);
VARP(serversortdir, 0, 0, 1);
VARP(showonlygoodservers, 0, 0, 1);
VAR(shownamesinbrowser, 0, 0, 1);
VARP(showminremain, 0, 0, 1);

int sicompare(serverinfo **ap, serverinfo **bp)
{
    serverinfo *a = *ap, *b = *bp;
    int dir = serversortdir ? -1 : 1;
    if((a->protocol==PROTOCOL_VERSION) > (b->protocol==PROTOCOL_VERSION)) return -dir;
    if((b->protocol==PROTOCOL_VERSION) > (a->protocol==PROTOCOL_VERSION)) return dir;
    if(!a->numplayers && b->numplayers) return dir;
    if(a->numplayers && !b->numplayers) return -dir;
    enet_uint32 ai = ntohl(a->address.host), bi = ntohl(b->address.host);
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

void *servmenu = NULL, *searchmenu = NULL;
vector<char *> namelists;

string cursearch, cursearchuc;

void searchnickname(const char *name)
{
    if(!name || !name[0]) return;
    s_strcpy(cursearch, name);
    s_strcpy(cursearchuc, name);
    strtoupper(cursearchuc);
    showmenu("search");
}
COMMAND(searchnickname, ARG_1STR);

VAR(showallservers, 0, 1, 1);

bool matchplayername(const char *name)
{
    static string nameuc;
    s_strcpy(nameuc, name);
    strtoupper(nameuc);
    return strstr(nameuc, cursearchuc) != NULL;
}

void refreshservers(void *menu, bool init)
{
    static int servermenumillis;
    static bool usedselect = false;
    static string title;
    bool issearch = menu == searchmenu;

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
    if((init && issearch) || totalmillis - lastinfo >= (servpingrate * (issearch ? 2 : 1))/(maxservpings ? (servers.length() + maxservpings - 1) / maxservpings : 1)) pingservers(issearch, menu == NULL);
    if(!init && menu && servers.inrange(((gmenu *)menu)->menusel) && (usedselect || ((gmenu *)menu)->menusel > 0))
    {
        loopv(servers) if(servers[i]->menuline == ((gmenu *)menu)->menusel) oldsel = servers[i];
    }
    servers.sort(sicompare);
    if(menu)
    {
        static const char *titles[NUMSERVSORT] =
        {
            "\fs\f0ping\fr\tplr\tserver%s%s",                               // 0: ping
            "ping\t\fs\f0plr\fr\tserver%s%s",                               // 1: player number
            "ping\tplr\tserver (\fs\f0max players\fr)%s%s",                 // 2: maxplayers
            "ping\tplr\fs\f0\fr\tserver (\fs\f0minutes remaining\fr)%s%s",  // 3: minutes remaining
            "ping\tplr\tserver (\fs\f0map\fr)%s%s",                         // 4: map
            "ping\tplr\tserver (\fs\f0game mode\fr)%s%s",                   // 5: mode
            "ping\tplr\tserver (\fs\f0IP\fr)%s%s",                          // 6: IP
            "ping\tplr\tserver (\fs\f0description\fr)%s%s"                  // 7: description
        };
        bool showmr = showminremain || serversort == SBS_MINREM;
        s_sprintf(title)(titles[serversort], issearch ? "      search results for \f3" : "     (F1: Help)", issearch ? cursearch : "");
        menutitle(menu, title);
        menureset(menu);
        string text;
        int curnl = 0, showedservers = 0;
        bool sbconnectexists = identexists("sbconnect");
        loopv(servers)
        {
            serverinfo &si = *servers[i];
            if(!showallservers && si.lastpingmillis < servermenumillis) continue; // no pong yet
            int banned = ((si.pongflags >> PONGFLAG_BANNED) & 1) | ((si.pongflags >> (PONGFLAG_BLACKLIST - 1)) & 2);
            bool showthisone = !(banned && showonlygoodservers);
            bool serverfull = si.numplayers >= si.maxclients;
            bool needspasswd = (si.pongflags & (1 << PONGFLAG_PASSWORD)) > 0;
            bool isprivate = (si.pongflags >> PONGFLAG_MASTERMODE) > 0;
            char basecolor = banned ? '4' : (curserver == servers[i] ? '1' : '5');
            char plnumcolor = serverfull ? '2' : (needspasswd ? '3' : (isprivate ? '1' : basecolor));
            if(si.address.host != ENET_HOST_ANY && si.ping != 9999)
            {
                if(si.protocol!=PROTOCOL_VERSION)
                {
                	if(!showonlygoodservers) s_sprintf(si.full)("%s:%d [%s]", si.name, si.port, si.protocol<0 ? "modded version" : (si.protocol<PROTOCOL_VERSION ? "older protocol" : "newer protocol"));
                	else showthisone = false;
                }
                else
                {
                    if(showmr) s_sprintf(text)(", (%d)", si.minremain);
                    else text[0] = '\0';
                    if(si.map[0]) s_sprintf(si.full)("\fs\f%c%d\t\fs\f%c%d/%d\fr\t%s, %s%s: %s:%d\fr %s", basecolor, si.ping,
                        plnumcolor, si.numplayers, si.maxclients,
                        si.map, modestr(si.mode, modeacronyms > 0), text, si.name, si.port, si.sdesc);
                    else s_sprintf(si.full)("\fs\f%c%d\t\fs\f%c%d/%d\fr\tempty: %s:%d\fr %s", basecolor, si.ping,
                        plnumcolor, si.numplayers, si.maxclients, si.name, si.port, si.sdesc);
                }
            }
            else
            {
            	if(!showonlygoodservers) s_sprintf(si.full)(si.address.host != ENET_HOST_ANY ? "%s:%d [waiting for server response]" : "%s:%d [unknown host]", si.name, si.port);
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
                si.full[88] = 0; // cut off too long server descriptions
                si.description[75] = 0;
                if(sbconnectexists)
                {
                    filtertext(text, si.sdesc);
                    for(char *p = text; (p = strchr(p, '\"')); *p++ = ' ');
                    text[30] = '\0';
                    s_sprintf(si.cmd)("sbconnect %s %d  %d %d %d %d \"%s\"", si.name, si.port, serverfull ?1:0, needspasswd ?1:0, isprivate ?1:0, banned, text);
                }
                else s_sprintf(si.cmd)("connect %s %d", si.name, si.port);
                menumanual(menu, si.full, si.cmd, NULL, si.description);
                if(!issearch && servers[i] == oldsel)
                {
                    ((gmenu *)menu)->menusel = showedservers;
                    usedselect = true;
                    si.getnames = shownamesinbrowser ? 1 : 0;
                }
                si.menuline = showedservers++;
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
                            s_strcpy(t, "\t");
                        }
                        if(!issearch || matchplayername(si.playernames[j]))
                        {
                            s_strcat(t, " \t");
                            s_strcat(t, si.playernames[j]);
                            cur++;
                        };
                        if(cur == 4)
                        {
                            menumanual(menu, t, NULL, NULL, NULL);
                            cur = 0;
                        }
                    }
                    if(cur) menumanual(menu, t, NULL, NULL, NULL);
                }
            }
            else si.menuline = -1;
        }
        if(issearch && curnl == 0)
        {
            static string notfoundmsg;
            s_sprintf(notfoundmsg)("\t\tpattern \fs\f3%s\fr not found.", cursearch);
            menumanual(menu, notfoundmsg, NULL, NULL, NULL);
        }
    }
}

bool serverskey(void *menu, int code, bool isdown, int unicode)
{
    if(!isdown) return false;
    switch(code)
    {
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

		case SDLK_F7:
			showonlygoodservers = showonlygoodservers ? 0 : 1;
			return true;

		case SDLK_F8:
			showminremain = showminremain ? 0 : 1;
			return true;
    }
    return false;
}

void clearservers()
{
    resolverclear();
    servers.deletecontentsp();
}

VARP(masterupdatefrequency, 1, 60*60, 24*60*60);

void updatefrommaster(int force)
{
    static int lastupdate = 0;
    if(!force && lastupdate && totalmillis-lastupdate<masterupdatefrequency*1000) return;

    uchar buf[32000];
    uchar *reply = retrieveservers(buf, sizeof(buf));
    if(!*reply || strstr((char *)reply, "<html>") || strstr((char *)reply, "<HTML>")) conoutf("master server not replying");
    else
    {
        // preserve currently connected server from deletion
        serverinfo *curserver = getconnectedserverinfo();
        string curname, curport;
        if(curserver)
        {
            s_strcpy(curname, curserver->name);
            s_sprintf(curport)("%d", curserver->port);
        }

        clearservers();
        execute((char *)reply);

        if(curserver) addserver(curname, curport);
        lastupdate = totalmillis;
    }
}

COMMAND(addserver, ARG_2STR);
COMMAND(clearservers, ARG_NONE);
COMMAND(updatefrommaster, ARG_1INT);

void writeservercfg()
{
    FILE *f = openfile(path("config/servers.cfg", true), "w");
    if(!f) return;
    fprintf(f, "// servers connected to are added here automatically\n\n");
    loopvrev(servers) fprintf(f, "addserver %s %d\n", servers[i]->name, servers[i]->port);
    fclose(f);
}
