// serverbrowser.cpp: eihrul's concurrent resolver, and server browser window management

#include "cube.h"
#include "SDL_thread.h"

struct resolverthread
{
    SDL_Thread *thread;
    char *query;
    int starttime;
};

struct resolverresult
{
    char *query;
    ENetAddress address;
};

vector<resolverthread> resolverthreads;
vector<char *> resolverqueries;
vector<resolverresult> resolverresults;
SDL_mutex *resolvermutex;
SDL_sem *resolversem;
int resolverlimit = 1000;

int resolverloop(void * data)
{
    resolverthread *rt = (resolverthread *)data;
    for(;;)
    {
        SDL_SemWait(resolversem);
        SDL_LockMutex(resolvermutex);
        if(resolverqueries.empty())
        {
            SDL_UnlockMutex(resolvermutex);
            continue;
        }
        rt->query = resolverqueries.pop();
        rt->starttime = lastmillis;
        SDL_UnlockMutex(resolvermutex);
        ENetAddress address = { ENET_HOST_ANY, CUBE_SERVINFO_PORT };
        enet_address_set_host(&address, rt->query);
        SDL_LockMutex(resolvermutex);
        resolverresult &rr = resolverresults.add();
        rr.query = rt->query;
        rr.address = address;
        rt->query = NULL;
        rt->starttime = 0;
        SDL_UnlockMutex(resolvermutex);
    };
    return 0;
};

void resolverinit(int threads, int limit)
{
    resolverlimit = limit;
    resolversem = SDL_CreateSemaphore(0);
    resolvermutex = SDL_CreateMutex();

    while(threads > 0)
    {
        resolverthread &rt = resolverthreads.add();
        rt.query = NULL;
        rt.starttime = 0;
        rt.thread = SDL_CreateThread(resolverloop, &rt);
        --threads;
    };
};

void resolverstop(resolverthread &rt, bool restart)
{
    SDL_LockMutex(resolvermutex);
    SDL_KillThread(rt.thread);
    rt.query = NULL;
    rt.starttime = 0;
    rt.thread = NULL;
    if(restart) rt.thread = SDL_CreateThread(resolverloop, &rt);
    SDL_UnlockMutex(resolvermutex);
}; 

void resolverclear()
{
    SDL_LockMutex(resolvermutex);
    resolverqueries.setsize(0);
    resolverresults.setsize(0);
    while (SDL_SemTryWait(resolversem) == 0);
    loopv(resolverthreads)
    {
        resolverthread &rt = resolverthreads[i];
        resolverstop(rt, true);
    };
    SDL_UnlockMutex(resolvermutex);
};

void resolverquery(char *name)
{
    SDL_LockMutex(resolvermutex);
    resolverqueries.add(name);
    SDL_SemPost(resolversem);
    SDL_UnlockMutex(resolvermutex);
};

bool resolvercheck(char **name, ENetAddress *address)
{
    SDL_LockMutex(resolvermutex);
    if(!resolverresults.empty())
    {
        resolverresult &rr = resolverresults.pop();
        *name = rr.query;
        *address = rr.address;
        SDL_UnlockMutex(resolvermutex);
        return true;
    }
    loopv(resolverthreads)
    {
        resolverthread &rt = resolverthreads[i];
        if(rt.query)
        {
            if(lastmillis - rt.starttime > resolverlimit)        
            {
                resolverstop(rt, true);
                *name = rt.query;
                SDL_UnlockMutex(resolvermutex);
                return true;
            };
        };    
    };
    SDL_UnlockMutex(resolvermutex);
    return false;
};

struct serverinfo
{
    string name;
    string full;
    string map;
    string sdesc;
    int mode, numplayers, ping, protocol, minremain;
    ENetAddress address;
};

vector<serverinfo> servers;
ENetSocket pingsock = ENET_SOCKET_NULL;
int lastinfo = 0;

char *getservername(int n) { return servers[n].name; };

void addserver(char *servername)
{
    loopv(servers) if(strcmp(servers[i].name, servername)==0) return;
    serverinfo &si = servers.insert(0, serverinfo());
    strcpy_s(si.name, servername);
    si.full[0] = 0;
    si.mode = 0;
    si.numplayers = 0;
    si.ping = 9999;
    si.protocol = 0;
    si.minremain = 0;
    si.map[0] = 0;
    si.sdesc[0] = 0;
    si.address.host = ENET_HOST_ANY;
    si.address.port = CUBE_SERVINFO_PORT;
};

void pingservers()
{
    ENetBuffer buf;
    uchar ping[MAXTRANS];
    uchar *p;
    loopv(servers)
    {
        serverinfo &si = servers[i];
        if(si.address.host == ENET_HOST_ANY) continue;
        p = ping;
        putint(p, lastmillis);
        buf.data = ping;
        buf.dataLength = p - ping;
        enet_socket_send(pingsock, &si.address, &buf, 1);
    };
    lastinfo = lastmillis;
};
  
void checkresolver()
{
    char *name = NULL;
    ENetAddress addr = { ENET_HOST_ANY, CUBE_SERVINFO_PORT };
    while(resolvercheck(&name, &addr))
    {
        if(addr.host == ENET_HOST_ANY) continue;
        loopv(servers)
        {
            serverinfo &si = servers[i];
            if(name == si.name)
            {
                si.address = addr;
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
    uchar ping[MAXTRANS], *p;
    char text[MAXTRANS];
    buf.data = ping; 
    buf.dataLength = sizeof(ping);
    while(enet_socket_wait(pingsock, &events, 0) >= 0 && events)
    {
        if(enet_socket_receive(pingsock, &addr, &buf, 1) <= 0) return;  
        loopv(servers)
        {
            serverinfo &si = servers[i];
            if(addr.host == si.address.host)
            {
                p = ping;
                si.ping = lastmillis - getint(p);
                si.protocol = getint(p);
                if(si.protocol!=PROTOCOL_VERSION) si.ping = 9998;
                si.mode = getint(p);
                si.numplayers = getint(p);
                si.minremain = getint(p);
                sgetstr();
                strcpy_s(si.map, text);
                sgetstr();
                strcpy_s(si.sdesc, text);                
                break;
            };
        };
    };
};

int sicompare(const serverinfo *a, const serverinfo *b)
{
    return a->ping>b->ping ? 1 : (a->ping<b->ping ? -1 : strcmp(a->name, b->name));
};

void refreshservers()
{
    checkresolver();
    checkpings();
    if(lastmillis - lastinfo >= 5000) pingservers();
    servers.sort((void *)sicompare);
    int maxmenu = 16;
    loopv(servers)
    {
        serverinfo &si = servers[i];
        if(si.address.host != ENET_HOST_ANY && si.ping != 9999)
        {
            if(si.protocol!=PROTOCOL_VERSION) sprintf_s(si.full)("%s [different cube protocol]", si.name);
            else sprintf_s(si.full)("%d\t%d\t%s, %s: %s %s", si.ping, si.numplayers, si.map[0] ? si.map : "[unknown]", modestr(si.mode), si.name, si.sdesc);
        }
        else
        {
            sprintf_s(si.full)(si.address.host != ENET_HOST_ANY ? "%s [waiting for server response]" : "%s [unknown host]\t", si.name);
        }
        si.full[50] = 0; // cut off too long server descriptions
        menumanual(1, i, si.full);
        if(!--maxmenu) return;
    };
};

void servermenu()
{
    if(pingsock == ENET_SOCKET_NULL)
    {
        pingsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM, NULL);
        resolverinit(1, 1000);
    };
    resolverclear();
    loopv(servers) resolverquery(servers[i].name);
    refreshservers();
    menuset(1);
};

void updatefrommaster()
{
    const int MAXUPD = 32000;
    uchar buf[MAXUPD];
    uchar *reply = retrieveservers(buf, MAXUPD);
    if(!*reply || strstr((char *)reply, "<html>") || strstr((char *)reply, "<HTML>")) conoutf("master server not replying");
    else { servers.setsize(0); execute((char *)reply); };
    servermenu();
};

COMMAND(addserver, ARG_1STR);
COMMAND(servermenu, ARG_NONE);
COMMAND(updatefrommaster, ARG_NONE);

void writeservercfg()
{
    FILE *f = fopen("servers.cfg", "w");
    if(!f) return;
    fprintf(f, "// servers connected to are added here automatically\n\n");
    loopvrev(servers) fprintf(f, "addserver %s\n", servers[i].name);
    fclose(f);
};


