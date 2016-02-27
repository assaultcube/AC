// all server side masterserver and pinging functionality

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

extern struct servercommandline scl;
bool usemaster = false;
ENetSocket mastersock = ENET_SOCKET_NULL;
ENetAddress masteraddress = { ENET_HOST_ANY, ENET_PORT_ANY }, serveraddress = { ENET_HOST_ANY, ENET_PORT_ANY };
string mastername = AC_MASTER_URI;
int masterport = AC_MASTER_PORT;
int lastupdatemaster = 0;
vector<char> masterout, masterin;
int masteroutpos = 0, masterinpos = 0;

void disconnectmaster()
{
    if(mastersock == ENET_SOCKET_NULL) return;

    enet_socket_destroy(mastersock);
    mastersock = ENET_SOCKET_NULL;

    masterout.setsize(0);
    masterin.setsize(0);
    masteroutpos = masterinpos = 0;

    masteraddress.host = ENET_HOST_ANY;
    masteraddress.port = ENET_PORT_ANY;

    //lastupdatemaster = 0;
}

ENetSocket connectmaster()
{
    if(!mastername[0] || !usemaster) return ENET_SOCKET_NULL;
    if(scl.maxclients > MAXCL) { logline(ACLOG_WARNING, "maxclient exceeded: cannot register"); return ENET_SOCKET_NULL; }

    if(masteraddress.host == ENET_HOST_ANY)
    {
        logline(ACLOG_INFO, "looking up %s:%d...", mastername, masterport);
        masteraddress.port = masterport;
        if(!resolverwait(mastername, &masteraddress)) return ENET_SOCKET_NULL;
    }
    ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
    if(sock != ENET_SOCKET_NULL && serveraddress.host != ENET_HOST_ANY && enet_socket_bind(sock, &serveraddress) < 0)
    {
        enet_socket_destroy(sock);
        sock = ENET_SOCKET_NULL;
    }
    if(sock == ENET_SOCKET_NULL || connectwithtimeout(sock, mastername, masteraddress) < 0)
    {
        logline(ACLOG_WARNING, sock==ENET_SOCKET_NULL ? "could not open socket" : "could not connect");
        return ENET_SOCKET_NULL;
    }

    enet_socket_set_option(sock, ENET_SOCKOPT_NONBLOCK, 1);
    return sock;
}

bool requestmaster(const char *req)
{
    if(mastersock == ENET_SOCKET_NULL)
    {
        mastersock = connectmaster();
        if(mastersock == ENET_SOCKET_NULL) return false;
    }

    masterout.put(req, strlen(req));
    return true;
}

bool requestmasterf(const char *fmt, ...)
{
    defvformatstring(req, fmt, fmt);
    return requestmaster(req);
}

extern void processmasterinput(const char *cmd, int cmdlen, const char *args);

void processmasterinput()
{
    if(masterinpos >= masterin.length()) return;

    char *input = &masterin[masterinpos], *end = (char *)memchr(input, '\n', masterin.length() - masterinpos);
    while(end)
    {
        *end++ = '\0';

        const char *args = input;
        while(args < end && !isspace(*args)) args++;
        int cmdlen = args - input;
        while(args < end && isspace(*args)) args++;

        if(!strncmp(input, "failreg", cmdlen))
            logline(ACLOG_WARNING, "master server registration failed: %s", args);
        else if(!strncmp(input, "succreg", cmdlen))
        {
            logline(ACLOG_INFO, "master server registration succeeded");
        }
        else processmasterinput(input, cmdlen, args);

        masterinpos = end - masterin.getbuf();
        input = end;
        end = (char *)memchr(input, '\n', masterin.length() - masterinpos);
    }

    if(masterinpos >= masterin.length())
    {
        masterin.setsize(0);
        masterinpos = 0;
    }
}

void flushmasteroutput()
{
    if(masterout.empty()) return;

    ENetBuffer buf;
    buf.data = &masterout[masteroutpos];
    buf.dataLength = masterout.length() - masteroutpos;
    int sent = enet_socket_send(mastersock, NULL, &buf, 1);
    if(sent >= 0)
    {
        masteroutpos += sent;
        if(masteroutpos >= masterout.length())
        {
            masterout.setsize(0);
            masteroutpos = 0;
        }
    }
    else disconnectmaster();
}

void flushmasterinput()
{
    if(masterin.length() >= masterin.capacity())
        masterin.reserve(4096);

    ENetBuffer buf;
    buf.data = masterin.getbuf() + masterin.length();
    buf.dataLength = masterin.capacity() - masterin.length();
    int recv = enet_socket_receive(mastersock, NULL, &buf, 1);
    if(recv > 0)
    {
        masterin.advance(recv);
        processmasterinput();
    }
    else disconnectmaster();
}

extern int totalclients;

// send alive signal to masterserver after 40 minutes of uptime and if currently in intermission (so theoretically <= 1 hour)
// TODO?: implement a thread to drop this "only in intermission" business, we'll need it once AUTH gets active!
static inline void updatemasterserver(int millis, int port, int _interm)
{
    if(!lastupdatemaster || ((millis-lastupdatemaster)>40*60*1000 && (_interm || !totalclients)))
    {
        string servdesc, pubkey;
        filtertext(servdesc, scl.servdesc_full[0] ? scl.servdesc_full : "noname", FTXT__GLOBALNAME, 20);
        if(mastername[0]) requestmasterf("regserv %d %s-%s %d\n", port, servdesc, scl.ssk ? bin2hex(pubkey, scl.ssk + 32, 32) : "nokey", AC_VERSION);
        lastupdatemaster = millis + 1;
    }
}

ENetSocket pongsock = ENET_SOCKET_NULL, lansock = ENET_SOCKET_NULL;
extern int getpongflags(enet_uint32 ip);

void serverms(int mode, int numplayers, int minremain, char *smapname, int millis, const ENetAddress &localaddr, int *mnum, int *msend, int *mrec, int *cnum, int *csend, int *crec, int protocol_version, const char *servdesccur, int _interm)
{
    flushmasteroutput();
    updatemasterserver(millis, localaddr.port, _interm);

    static ENetSocketSet sockset;
    ENET_SOCKETSET_EMPTY(sockset);
    ENetSocket maxsock = pongsock;
    ENET_SOCKETSET_ADD(sockset, pongsock);
    if(mastersock != ENET_SOCKET_NULL)
    {
        maxsock = max(maxsock, mastersock);
        ENET_SOCKETSET_ADD(sockset, mastersock);
    }
    if(lansock != ENET_SOCKET_NULL)
    {
        maxsock = max(maxsock, lansock);
        ENET_SOCKETSET_ADD(sockset, lansock);
    }
    if(enet_socketset_select(maxsock, &sockset, NULL, 0) <= 0) return;

    // reply all server info requests
    static uchar data[MAXTRANS];
    ENetBuffer buf;
    ENetAddress addr;
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
            (*mnum)++; *mrec += len; std = true;
            putint(po, protocol_version);
            putint(po, mode);
            putint(po, numplayers);
            putint(po, minremain);
            sendstring(smapname, po);
            sendstring(servdesccur, po);
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

    if(mastersock != ENET_SOCKET_NULL && ENET_SOCKETSET_CHECK(sockset, mastersock)) flushmasterinput();
}

void servermsinit(bool listen)
{
    if(scl.master && *scl.master) copystring(mastername, scl.master);
    usemaster = !listen || (listen && scl.master && scl.ssk);  // true: connect to master, false: LAN-only server
    disconnectmaster();

    if(listen)
    {
        ENetAddress address = { ENET_HOST_ANY, (enet_uint16) CUBE_SERVINFO_PORT(scl.serverport) };
        if(*scl.ip)
        {
            if(enet_address_set_host(&address, scl.ip) < 0) logline(ACLOG_WARNING, "server ip \"%s\" not resolved", scl.ip);
            else serveraddress.host = address.host;
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
    if(!usemaster) logline(ACLOG_INFO, "server is LAN-only - without connection to masterserver");
}
