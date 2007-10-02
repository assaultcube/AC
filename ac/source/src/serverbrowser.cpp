// serverbrowser.cpp: eihrul's concurrent resolver, and server browser window management

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
        rt->starttime = lastmillis;
        SDL_UnlockMutex(resolvermutex);
    
        ENetAddress address = { ENET_HOST_ANY, ENET_PORT_ANY };
        enet_address_set_host(&address, rt->query);

        SDL_LockMutex(resolvermutex);
        if(thread == rt->thread)
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
        if(rt.query && lastmillis - rt.starttime > RESOLVERLIMIT)
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
        show_out_of_renderloop_progress(min(float(timeout)/RESOLVERLIMIT, 1), text);
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

int connectwithtimeout(ENetSocket sock, char *hostname, ENetAddress &address)
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
        show_out_of_renderloop_progress(min(float(timeout)/CONNLIMIT, 1), text);
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

struct serverinfo
{
    char *name;
    string full;
    string map;
    string sdesc;
    string cmd;
    int mode, numplayers, maxclients, ping, protocol, minremain, resolved, port;
    ENetAddress address;
};

enum { UNRESOLVED = 0, RESOLVING, RESOLVED };

vector<serverinfo> servers;
ENetSocket pingsock = ENET_SOCKET_NULL;
int lastinfo = 0;

char *getservername(int n) { return servers[n].name; }

void addserver(char *servername, char *serverport)
{
    loopv(servers) if(strcmp(servers[i].name, servername)==0 && servers[i].port == atoi(serverport)) return;
    serverinfo &si = servers.insert(0, serverinfo());
    si.name = newstring(servername);
    si.port = atoi(serverport);
    si.full[0] = 0;
    si.mode = 0;
    si.numplayers = 0;
    si.maxclients = 0;
    si.ping = 9999;
    si.protocol = 0;
    si.minremain = 0;
    si.map[0] = 0;
    si.sdesc[0] = 0;
    si.resolved = UNRESOLVED;
    si.address.host = ENET_HOST_ANY;
    si.address.port = CUBE_SERVINFO_PORT(si.port);
}

void pingservers()
{
    ENetBuffer buf;
    uchar ping[MAXTRANS];
    loopv(servers)
    {
        serverinfo &si = servers[i];
        if(si.address.host == ENET_HOST_ANY) continue;
        ucharbuf p(ping, sizeof(ping));
        putint(p, lastmillis);
        buf.data = ping;
        buf.dataLength = p.length();
        enet_socket_send(pingsock, &si.address, &buf, 1);
    }
    lastinfo = lastmillis;
}
  
void checkresolver()
{
    int resolving = 0;
    loopv(servers)
    {
        serverinfo &si = servers[i];
        if(si.resolved == RESOLVED) continue;
        if(si.address.host == ENET_HOST_ANY)
        {
            if(si.resolved == UNRESOLVED) { si.resolved = RESOLVING; resolverquery(si.name); }
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
            serverinfo &si = servers[i];
            if(name == si.name)
            {
                si.resolved = RESOLVED;
                si.address.host = addr.host;
                addr.host = ENET_HOST_ANY;
                break;
            }
        }
    }
}

void checkpings()
{
    enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
    ENetBuffer buf;
    ENetAddress addr;
    uchar ping[MAXTRANS];
    char text[MAXTRANS];
    buf.data = ping; 
    buf.dataLength = sizeof(ping);
    while(enet_socket_wait(pingsock, &events, 0) >= 0 && events)
    {
        int len = enet_socket_receive(pingsock, &addr, &buf, 1);
        if(len <= 0) return;  
        loopv(servers)
        {
            serverinfo &si = servers[i];
            if(addr.host == si.address.host && addr.port == si.address.port)
            {
                ucharbuf p(ping, len);
                si.ping = lastmillis - getint(p);
                si.protocol = getint(p);
                if(si.protocol!=PROTOCOL_VERSION) si.ping = 9998;
                si.mode = getint(p);
                si.numplayers = getint(p);
                si.minremain = getint(p);
                getstring(text, p);
                s_strcpy(si.map, text);
                getstring(text, p);
                s_strcpy(si.sdesc, text);                
                si.maxclients = getint(p);
                break;
            }
        }
    }
}

int sicompare(const serverinfo *a, const serverinfo *b)
{
    if((a->protocol==PROTOCOL_VERSION) > (b->protocol==PROTOCOL_VERSION)) return -1;
    if((b->protocol==PROTOCOL_VERSION) > (a->protocol==PROTOCOL_VERSION)) return 1;
    if(a->numplayers<b->numplayers) return 1;
    if(a->numplayers>b->numplayers) return -1;
    if(a->ping>b->ping) return 1;
    if(a->ping<b->ping) return -1;
    int namecmp = strcmp(a->name, b->name);
    if(namecmp) return namecmp;
    if(a->port>b->port) return 1;
    else return -1;
}

void *servmenu = NULL;

void refreshservers(void *menu, bool init)
{
    if(init)
    {
        if(pingsock == ENET_SOCKET_NULL)
        {
            pingsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM, NULL);
            resolverinit();
        }
        resolverclear();
        loopv(servers) resolverquery(servers[i].name);
    }

    checkresolver();
    checkpings();
    if(lastmillis - lastinfo >= 5000) pingservers();
    servers.sort(sicompare);
    loopv(servers)
    {
        serverinfo &si = servers[i];
        if(si.address.host != ENET_HOST_ANY && si.ping != 9999)
        {
            if(si.protocol!=PROTOCOL_VERSION) s_sprintf(si.full)("%s:%d [%s protocol]", si.name, si.port, si.protocol<PROTOCOL_VERSION ? "older" : "newer");
            else if(si.map[0]) s_sprintf(si.full)("%d\t%d/%d\t%s, %s: %s:%d %s", si.ping, si.numplayers, si.maxclients, si.map, modestr(si.mode), si.name, si.port, si.sdesc);
            else s_sprintf(si.full)("%d\t%d/%d\tempty: %s:%d %s", si.ping, si.numplayers, si.maxclients, si.name, si.port, si.sdesc);
        }
        else
        {
            s_sprintf(si.full)(si.address.host != ENET_HOST_ANY ? "%s:%d [waiting for server response]" : "%s:%d [unknown host]\t", si.name, si.port);
        }
        si.full[50] = 0; // cut off too long server descriptions
        s_sprintf(si.cmd)("connect %s %d", si.name, si.port);
        menumanual(menu, i, si.full, si.cmd);
    }
}

void updatefrommaster()
{
    uchar buf[32000];
    uchar *reply = retrieveservers(buf, sizeof(buf));
    if(!*reply || strstr((char *)reply, "<html>") || strstr((char *)reply, "<HTML>")) conoutf("master server not replying");
    else 
    { 
        loopv(servers) delete[] servers[i].name;
        servers.setsize(0); 
        execute((char *)reply); 
    }
}

COMMAND(addserver, ARG_2STR);
COMMAND(updatefrommaster, ARG_NONE);

void writeservercfg()
{
    FILE *f = openfile("config/servers.cfg", "w");
    if(!f) return;
    fprintf(f, "// servers connected to are added here automatically\n\n");
    loopvrev(servers) fprintf(f, "addserver %s %d\n", servers[i].name, servers[i].port);
    fclose(f);
}
