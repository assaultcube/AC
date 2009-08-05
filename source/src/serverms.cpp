// all server side masterserver and pinging functionality

#include "pch.h"
#include "cube.h"

#ifdef STANDALONE
bool resolverwait(const char *name, ENetAddress *address)
{
    return enet_address_set_host(address, name) >= 0;
}

int connectwithtimeout(ENetSocket sock, const char *hostname, ENetAddress &remoteaddress)
{
    int result = enet_socket_connect(sock, &remoteaddress);
    if(result<0) enet_socket_destroy(sock);
    return result;
}
#endif

ENetSocket httpgetsend(ENetAddress &remoteaddress, const char *hostname, const char *req, const char *ref, const char *agent, ENetAddress *localaddress = NULL)
{
    if(remoteaddress.host==ENET_HOST_ANY)
    {
        logline(ACLOG_INFO, "looking up %s...", hostname);
        if(!resolverwait(hostname, &remoteaddress)) return ENET_SOCKET_NULL;
    }
    ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
    if(sock!=ENET_SOCKET_NULL && localaddress && enet_socket_bind(sock, localaddress) < 0)
    {
        enet_socket_destroy(sock);
        sock = ENET_SOCKET_NULL;
    }
    if(sock==ENET_SOCKET_NULL || connectwithtimeout(sock, hostname, remoteaddress)<0)
    {
        logline(ACLOG_WARNING, sock==ENET_SOCKET_NULL ? "could not open socket" : "could not connect");
        return ENET_SOCKET_NULL;
    }
    ENetBuffer buf;
    s_sprintfd(httpget)("GET %s HTTP/1.0\nHost: %s\nReferer: %s\nUser-Agent: %s\n\n", req, hostname, ref, agent);
    buf.data = httpget;
    buf.dataLength = strlen((char *)buf.data);
    logline(ACLOG_INFO, "sending request to %s...", hostname);
    enet_socket_send(sock, NULL, &buf, 1);
    return sock;
}

bool httpgetreceive(ENetSocket sock, ENetBuffer &buf, int timeout = 0)
{
    if(sock==ENET_SOCKET_NULL) return false;
    enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;
    if(enet_socket_wait(sock, &events, timeout) >= 0 && events)
    {
        int len = enet_socket_receive(sock, NULL, &buf, 1);
        if(len<=0)
        {
            enet_socket_destroy(sock);
            return false;
        }
        buf.data = ((char *)buf.data)+len;
        ((char*)buf.data)[0] = 0;
        buf.dataLength -= len;
    }
    return true;
}

uchar *stripheader(uchar *b)
{
    char *s = strstr((char *)b, "\n\r\n");
    if(!s) s = strstr((char *)b, "\n\n");
    return s ? (uchar *)s : b;
}

ENetSocket mssock = ENET_SOCKET_NULL;
ENetAddress msaddress = { ENET_HOST_ANY, ENET_PORT_ANY };
ENetAddress masterserver = { ENET_HOST_ANY, 80 };
int lastupdatemaster = 0;
string masterbase;
string masterpath;
uchar masterrep[MAXTRANS];
ENetBuffer masterb;

// send alive signal to masterserver every hour of uptime
void updatemasterserver(int millis, const ENetAddress &localaddr)
{
    if(!millis || millis/(60*60*1000)!=lastupdatemaster)
    {
		s_sprintfd(path)("%sregister.do?action=add&port=%d", masterpath, localaddr.port);
        s_sprintfd(agent)("AssaultCube Server %d", AC_VERSION);
		mssock = httpgetsend(masterserver, masterbase, path, "assaultcubeserver", agent, &msaddress);
		masterrep[0] = 0;
		masterb.data = masterrep;
		masterb.dataLength = MAXTRANS-1;
        lastupdatemaster = millis/(60*60*1000);
    }
}

void checkmasterreply()
{
    if(mssock!=ENET_SOCKET_NULL && !httpgetreceive(mssock, masterb))
    {
        mssock = ENET_SOCKET_NULL;
        string text;
        filtertext(text, (const char *) stripheader(masterrep));
        logline(ACLOG_INFO, "masterserver reply: %s", text);
    }
}

#ifndef STANDALONE

#define RETRIEVELIMIT 20000

uchar *retrieveservers(uchar *buf, int buflen)
{
    buf[0] = '\0';

    s_sprintfd(path)("%sretrieve.do?item=list", masterpath);
    s_sprintfd(agent)("AssaultCube Client %d", AC_VERSION);
    ENetAddress address = masterserver;
    ENetSocket sock = httpgetsend(address, masterbase, path, "assaultcubeclient", agent);
    if(sock==ENET_SOCKET_NULL) return buf;
    /* only cache this if connection succeeds */
    masterserver = address;

    s_sprintfd(text)("retrieving servers from %s... (esc to abort)", masterbase);
    show_out_of_renderloop_progress(0, text);

    ENetBuffer eb;
    eb.data = buf;
    eb.dataLength = buflen-1;

    int starttime = SDL_GetTicks(), timeout = 0;
    while(httpgetreceive(sock, eb, 250))
    {
        timeout = SDL_GetTicks() - starttime;
        show_out_of_renderloop_progress(min(float(timeout)/RETRIEVELIMIT, 1.0f), text);
        SDL_Event event;
        while(SDL_PollEvent(&event))
        {
            if(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) timeout = RETRIEVELIMIT + 1;
        }
        if(timeout > RETRIEVELIMIT)
        {
            buf[0] = '\0';
            enet_socket_destroy(sock);
            return buf;
        }
    }

    return stripheader(buf);
}
#endif

ENetSocket pongsock = ENET_SOCKET_NULL, lansock = ENET_SOCKET_NULL;
extern int getpongflags(enet_uint32 ip);

void serverms(int mode, int numplayers, int minremain, char *smapname, int millis, const ENetAddress &localaddr, int *mnum, int *msend, int *mrec, int *cnum, int *csend, int *crec, int protocol_version)
{
    checkmasterreply();
    if(protocol_version > 0 || strncmp(masterpath, AC_MASTER_URI, 23)) updatemasterserver(millis, localaddr);

    static ENetSocketSet sockset;
    ENET_SOCKETSET_EMPTY(sockset);
    ENetSocket maxsock = pongsock;
    ENET_SOCKETSET_ADD(sockset, pongsock);
    if(lansock != ENET_SOCKET_NULL)
    {
        maxsock = max(maxsock, lansock);
        ENET_SOCKETSET_ADD(sockset, lansock);
    }
    if(enet_socketset_select(maxsock, &sockset, NULL, 0) <= 0) return;

    // reply all server info requests
    ENetBuffer buf;
    ENetAddress addr;
    uchar data[MAXTRANS];
    buf.data = data;
    int len;

    loopi(2)
    {
        ENetSocket sock = i ? lansock : pongsock;
        if(sock == ENET_SOCKET_NULL || !ENET_SOCKETSET_CHECK(sockset, sock)) continue;

        buf.dataLength = sizeof(data);
        len = enet_socket_receive(sock, &addr, &buf, 1);
        if(len < 0) continue;

        // ping & pong buf
        ucharbuf pi(data, len), po(&data[len], sizeof(data)-len);

        bool std = false;
        if(getint(pi) != 0) // std pong
        {
            extern struct servercommandline scl;
            extern string servdesc_current;
            (*mnum)++; *mrec += len; std = true;
            putint(po, protocol_version);
            putint(po, mode);
            putint(po, numplayers);
            putint(po, minremain);
            sendstring(smapname, po);
            sendstring(servdesc_current, po);
            putint(po, scl.maxclients);
            putint(po, getpongflags(addr.host));
            if(pi.remaining())
            {
                int query = getint(pi);
                switch(query)
                {
                    case EXTPING_NAMELIST:
                    {
                        extern void extping_namelist(ucharbuf &p);
                        putint(po, query);
                        extping_namelist(po);
                        break;
                    }
                    case EXTPING_SERVERINFO:
                    {
                        extern void extping_serverinfo(ucharbuf &pi, ucharbuf &po);
                        putint(po, query);
                        extping_serverinfo(pi, po);
                        break;
                    }
                    case EXTPING_MAPROT:
                    {
                        extern void extping_maprot(ucharbuf &po);
                        putint(po, query);
                        extping_maprot(po);
                        break;
                    }
                    case EXTPING_UPLINKSTATS:
                    {
                        extern void extping_uplinkstats(ucharbuf &po);
                        putint(po, query);
                        extping_uplinkstats(po);
                        break;
                    }
                    case EXTPING_NOP:
                    default:
                        putint(po, EXTPING_NOP);
                        break;
                }
            }
        }
        else // ext pong - additional server infos
        {
            (*cnum)++; *crec += len;
            int extcmd = getint(pi);
            putint(po, EXT_ACK);
            putint(po, EXT_VERSION);

            switch(extcmd)
            {
                case EXT_UPTIME:        // uptime in seconds
                {
                    putint(po, uint(millis)/1000);
                    break;
                }

                case EXT_PLAYERSTATS:   // playerstats
                {
                    int cn = getint(pi);     // get requested player, -1 for all
                    if(!valid_client(cn) && cn != -1)
                    {
                        putint(po, EXT_ERROR);
                        break;
                    }
                    putint(po, EXT_ERROR_NONE);              // add no error flag

                    int bpos = po.length();                  // remember buffer position
                    putint(po, EXT_PLAYERSTATS_RESP_IDS);    // send player ids following
                    extinfo_cnbuf(po, cn);
                    *csend += int(buf.dataLength = len + po.length());
                    enet_socket_send(pongsock, &addr, &buf, 1); // send all available player ids
                    po.len = bpos;

                    extinfo_statsbuf(po, cn, bpos, pongsock, addr, buf, len, csend);
                    return;
                }

                case EXT_TEAMSCORE:
                    extinfo_teamscorebuf(po);
                    break;

                default:
                    putint(po,EXT_ERROR);
                    break;
            }
        }

        buf.dataLength = len + po.length();
        enet_socket_send(pongsock, &addr, &buf, 1);
        if(std) *msend += (int)buf.dataLength;
        else *csend += (int)buf.dataLength;
    }
}

void servermsinit(const char *master, const char *ip, int infoport, bool listen)
{
	const char *mid = strstr(master, "/");
    if(mid) s_strncpy(masterbase, master, mid-master+1);
    else s_strcpy(masterbase, (mid = master));
    s_strcpy(masterpath, mid);

	if(listen)
	{
        ENetAddress address = { ENET_HOST_ANY, infoport };
        if(*ip)
        {
            if(enet_address_set_host(&address, ip)<0) logline(ACLOG_WARNING, "server ip not resolved");
            else msaddress.host = address.host;
        }
        pongsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
        if(pongsock != ENET_SOCKET_NULL && enet_socket_bind(pongsock, &address) < 0)
        {
            enet_socket_destroy(pongsock);
            pongsock = ENET_SOCKET_NULL;
        }
        if(pongsock == ENET_SOCKET_NULL) fatal("could not create server info socket");
        else enet_socket_set_option(pongsock, ENET_SOCKOPT_NONBLOCK, 1);
        address.port = CUBE_SERVINFO_PORT_LAN;
        lansock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
        if(lansock != ENET_SOCKET_NULL && (enet_socket_set_option(lansock, ENET_SOCKOPT_REUSEADDR, 1) < 0 || enet_socket_bind(lansock, &address) < 0))
        {
            enet_socket_destroy(lansock);
            lansock = ENET_SOCKET_NULL;
        }
        if(lansock == ENET_SOCKET_NULL) logline(ACLOG_WARNING, "could not create LAN server info socket");
        else enet_socket_set_option(lansock, ENET_SOCKOPT_NONBLOCK, 1);
	}
}
