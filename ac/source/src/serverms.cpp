// all server side masterserver and pinging functionality

#include "cube.h"

#ifdef STANDALONE
bool resolverwait(const char *name, ENetAddress *address)
{
    return enet_address_set_host(address, name) >= 0;
}
#endif

ENetSocket mssock = ENET_SOCKET_NULL;

void httpgetsend(ENetAddress &ad, char *hostname, char *req, char *ref, char *agent)
{
    if(mssock!=ENET_SOCKET_NULL)
    {
        enet_socket_destroy(mssock);
        mssock = ENET_SOCKET_NULL;
    }
    if(ad.host==ENET_HOST_ANY)
    {
        printf("looking up %s...\n", hostname);
        if(!resolverwait(hostname, &ad)) return;
    }
    mssock = enet_socket_create(ENET_SOCKET_TYPE_STREAM, NULL);
    if(mssock==ENET_SOCKET_NULL) { printf("could not open socket\n"); return; }
    if(enet_socket_connect(mssock, &ad)<0) 
    { 
        printf("could not connect\n"); 
        enet_socket_destroy(mssock);
        mssock = ENET_SOCKET_NULL;
        return; 
    }
    ENetBuffer buf;
    s_sprintfd(httpget)("GET %s HTTP/1.0\nHost: %s\nReferer: %s\nUser-Agent: %s\n\n", req, hostname, ref, agent);
    buf.data = httpget;
    buf.dataLength = strlen((char *)buf.data);
    printf("sending request to %s...\n", hostname);
    enet_socket_send(mssock, NULL, &buf, 1);
}  

void httpgetrecieve(ENetBuffer &buf)
{
    if(mssock==ENET_SOCKET_NULL) return;
    enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
    if(enet_socket_wait(mssock, &events, 0) >= 0 && events)
    {
        int len = enet_socket_receive(mssock, NULL, &buf, 1);
        if(len<=0)
        {
            enet_socket_destroy(mssock);
            mssock = ENET_SOCKET_NULL;
            return;
        }
        buf.data = ((char *)buf.data)+len;
        ((char*)buf.data)[0] = 0;
        buf.dataLength -= len;
    }
}  

uchar *stripheader(uchar *b)
{
    char *s = strstr((char *)b, "\n\r\n");
    if(!s) s = strstr((char *)b, "\n\n");
    return s ? (uchar *)s : b;
}

ENetAddress masterserver = { ENET_HOST_ANY, 80 };
int updmaster = 0;
string masterbase;
string masterpath;
uchar masterrep[MAXTRANS];
ENetBuffer masterb;

void updatemasterserver(int seconds)
{
    if(seconds>updmaster)       // send alive signal to masterserver every hour of uptime
    {
		s_sprintfd(path)("%sregister.do?action=add", masterpath);
		httpgetsend(masterserver, masterbase, path, "actioncubeserver", "ActionCube Server");
		masterrep[0] = 0;
		masterb.data = masterrep;
		masterb.dataLength = MAXTRANS-1;
        updmaster = seconds+60*60;
    }
} 

void checkmasterreply()
{
    bool busy = mssock!=ENET_SOCKET_NULL;
    httpgetrecieve(masterb);
    if(busy && mssock==ENET_SOCKET_NULL) printf("masterserver reply: %s\n", stripheader(masterrep));
} 

uchar *retrieveservers(uchar *buf, int buflen)
{
    s_sprintfd(path)("%sretrieve.do?item=list", masterpath);
    httpgetsend(masterserver, masterbase, path, "actioncubeserver", "ActionCube Server");
    ENetBuffer eb;
    buf[0] = 0;
    eb.data = buf;
    eb.dataLength = buflen-1;
    while(mssock!=ENET_SOCKET_NULL) httpgetrecieve(eb);
    return stripheader(buf);
}

ENetSocket pongsock = ENET_SOCKET_NULL;
string serverdesc;

void serverms(int mode, int numplayers, int minremain, char *smapname, int seconds)        
{
    checkmasterreply();
    updatemasterserver(seconds);

	// reply all server info requests
	ENetBuffer buf;
    ENetAddress addr;
    uchar pong[MAXTRANS];
    int len;
    enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
    buf.data = pong;
    while(enet_socket_wait(pongsock, &events, 0) >= 0 && events)
    {
        buf.dataLength = sizeof(pong);
        len = enet_socket_receive(pongsock, &addr, &buf, 1);
        if(len < 0) return;
        ucharbuf p(&pong[len], sizeof(pong)-len);
        putint(p, PROTOCOL_VERSION);
        putint(p, mode);
        putint(p, numplayers);
        putint(p, minremain);
        sendstring(smapname, p);
        sendstring(serverdesc, p);
        putint(p, maxclients);
        buf.dataLength = len + p.length();
        enet_socket_send(pongsock, &addr, &buf, 1);
    }
}      

void servermsinit(const char *master, char *sdesc, bool listen)
{
	const char *mid = strstr(master, "/");
    if(!mid) mid = master;
    s_strcpy(masterpath, mid);
    s_strncpy(masterbase, master, mid-master+1);
    s_strcpy(serverdesc, sdesc);

	if(listen)
	{
        ENetAddress address = { ENET_HOST_ANY, CUBE_SERVINFO_PORT };
        pongsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM, &address);
        if(pongsock == ENET_SOCKET_NULL) fatal("could not create server info socket\n");
	}
}
