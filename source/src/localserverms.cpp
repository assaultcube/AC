// local master server used for LAN server-searching

#include "pch.h"
#include "cube.h"

extern int servmillis;
extern struct log *logger;

masterserver::masterserver() : created(false), lastpurge(0)
{
    socket = ENET_SOCKET_NULL;
}

bool masterserver::addserver(ENetAddress &addr)
{
    // update server if existing
    loopv(servers)
    {
        server &s = servers[i];
        if(s.addr.host==addr.host && s.addr.port==addr.port)
        {
            s.addmillis = servmillis;
            logger->writeline(log::info, "local LAN masterserver: updated local server (port %d)", s.addr.port);
            return true;
        }
    }

    // add new entry
    server s = { addr, servmillis };
    servers.add(s);
    logger->writeline(log::info, "local LAN masterserver: added local server (port %d) to the list", s.addr.port);

    return true;
}

char *masterserver::getserverlist()
{
    // construct cube-script serverlist
    char *buf = newstring("// local servers\n", _MAXDEFSTR);

    loopv(servers)
    {
        server &s = servers[i];
        string host;
        enet_address_get_host(&s.addr, host, _MAXDEFSTR);
        s_sprintfd(serverline)("addserver %s %d;\n", host, s.addr.port);
        s_strcat(buf, serverline);
    }

    return buf;
}

void masterserver::processrequests()
{
    ENetAddress remoteaddr;
    ENetBuffer requestbuf;
    uchar data[MAXTRANS] = { 0 };
    requestbuf.data = data;
    int len;
    enet_uint32 events = ENET_SOCKET_WAIT_RECEIVE;

    while(enet_socket_wait(socket, &events, 0) >= 0 && events)
    {
        requestbuf.dataLength = sizeof(data);
        len = enet_socket_receive(socket, &remoteaddr, &requestbuf, 1);

        // block all non-local connections
        /*enet_uint32 loopback = 16777343;
        if(address.host!=remoteaddr.host && !(address.host==ENET_HOST_ANY && remoteaddr.host==loopback))
        {
            enet_socket_destroy(datasocket);
            datasocket = socket;
            continue;
        }*/

        char *request = ((char *)requestbuf.data);

        // split request string
        hashtable<const char *, string> args;
        const char *argseparators = "?&";
        char *t = strpbrk(request, argseparators);
        while(t)
        {
            char *namebegin = t+1;
            char *nameend = strchr(namebegin, '=');
            char *valuebegin = nameend+1;
            char *valueend = strpbrk(valuebegin, "& ");

            if(namebegin < nameend && nameend < valuebegin && valuebegin < valueend)
            {
                char *name = newstring(namebegin, nameend-namebegin);
                char *str = (char*)args[name];
                s_strncpy(str, valuebegin, valueend-valuebegin+1);
            }

            t = strpbrk(t+1, argseparators);
        }

        // response buf
        const string error400 = "HTTP/1.0 400 Bad Request\nContent-Type: text/plain\n\nBAD REQUEST\n\n";
        const string error500 = "HTTP/1.0 500 Internal Server Error\nContent-Type: text/plain\n\nINTERNAL SERVER ERROR\n\n";
        ENetBuffer responsebuf;
        string response = { 0 };
        responsebuf.data = response;

        // process requests
        char *action = (char *)args.access("action");
        char *port = (char *)args.access("port");
        char *item = (char *)args.access("item");
        if(action)
        {
            if(!strcmp(action, "add") && port) // add requests
            {
                ENetAddress srvaddr = { remoteaddr.host, atoi(port) };
                if(addserver(srvaddr))
                {
                    //s_strcpy(response, "HTTP/1.0 200 OK\nContent-Type: text/plain\n\nServer was successfully added to the local LAN masterserver.\n\n");
                }
                else
                {
                    s_strcpy(response, error500);
                }
            }
            else
            {
                s_strcpy(response, error400);
            }
        }
        else if(item)
        {
            if(!strcmp(item, "list")) // list requests
            {
                s_strcpy(response, "HTTP/1.0 200 OK\nContent-Type: text/plain\n\n");
                char *servers = getserverlist();
                s_strcat(response, servers);
                s_strcat(response, "\n\n");
                delete[] servers;
            }
        }
        else
        {
            s_strcpy(response, error400);
        }

        size_t resplen = strlen(response);
        if(resplen > 0)
        {
            responsebuf.dataLength = resplen;
            enet_socket_send(socket, &remoteaddr, &responsebuf, 1);
        }

        args.clear();
    }
}

void masterserver::purgeserverlist()
{
    const int PURGEINTERVALMILLIS = 60*1000;
    const int MAXAGEMILLIS = 60*60*1000;

    if(!lastpurge || servmillis-lastpurge > PURGEINTERVALMILLIS)
    {
        loopv(servers)
        {
            server &s = servers[i];
            int age = servmillis-s.addmillis;
            if(age > MAXAGEMILLIS)
            {
                logger->writeline(log::info, "local LAN masterserver: removed local server (port %d) from list", s.addr.port);
                servers.remove(i--);
            }
        }

        lastpurge = servmillis;
    }
}

void masterserver::update()
{
    if(!created) return;

    processrequests();
    purgeserverlist();
}

void masterserver::create(ENetAddress &addr)
{
    if(created) return;

    socket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM, &addr);
    if(socket == ENET_SOCKET_NULL)
    {
        logger->writeline(log::info, "local LAN masterserver: could not create masterserver instance, another server probably does the job already");
    }
    else
    {
        enet_socket_set_option(socket, ENET_SOCKOPT_NONBLOCK, 1);
        address = addr;
        created = true;
        logger->writeline(log::info, "local LAN masterserver: created masterserver instance");
    }
}

