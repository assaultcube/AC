// server.cpp: little more than enhanced multicaster
// runs dedicated or as client coroutine

#include "cube.h" 

#define m_ctf_s (mode==5)
#define m_teammode_s (mode==0 || mode==4 || mode==5)

#define valid_client(c) (clients.inrange(c) && clients[c]->type!=ST_EMPTY)
#define valid_flag(f) (f >= 0 && f < 2)

enum { ST_EMPTY, ST_LOCAL, ST_TCPIP };

struct clientscore
{
    int frags, flags;

    void reset()
    {
        frags = flags = 0;
    };
};  

struct savedscore : clientscore
{
    string name;
    uint ip;
};

static vector<savedscore> scores;

struct client                   // server side version of "dynent" type
{
    int type;
    int clientnum;
    ENetPeer *peer;
    string hostname;
    string mapvote;
    string name;
    int modevote;
	uint pos[3];
    clientscore score;
	bool ismaster;
	bool isauthed; // for passworded servers
    vector<uchar> position, messages;
    int positionoffset, messageoffset;

    void reset()
    {
        name[0] = 0;
        score.reset();
        position.setsizenodelete(0);
        messages.setsizenodelete(0);
    };        
};

vector<client *> clients;

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
       };
       break;
   };
};

int bsend = 0, brec = 0, laststatus = 0, lastsec = 0;

void sendpacket(int n, int chan, ENetPacket *packet)
{
    switch(clients[n]->type)
    {
        case ST_TCPIP:
        {
            enet_peer_send(clients[n]->peer, chan, packet);
            bsend += (int)packet->dataLength;
            break;
        };

        case ST_LOCAL:
            localservertoclient(chan, packet->data, (int)packet->dataLength);
            break;
    };
};

static bool reliablemessages = false;

bool buildworldstate()
{
    worldstate &ws = *new worldstate;
    loopv(clients) 
    {
        client &c = *clients[i];
        if(c.type!=ST_TCPIP) continue;
        if(c.position.empty()) c.positionoffset = -1;
        else
        {
            c.positionoffset = ws.positions.length();
            loopvj(c.position) ws.positions.add(c.position[j]);
        };
        if(c.messages.empty()) c.messageoffset = -1;
        else
        {
            c.messageoffset = ws.messages.length();
            ws.messages.add(c.clientnum);
            ws.messages.add(c.messages.length()&0xFF);
            ws.messages.add(c.messages.length()>>8);
            loopvj(c.messages) ws.messages.add(c.messages[j]);
        };
    };
    int psize = ws.positions.length(), msize = ws.messages.length();
    loopi(psize) { uchar c = ws.positions[i]; ws.positions.add(c); };
    loopi(msize) { uchar c = ws.messages[i]; ws.messages.add(c); };
    ws.uses = 0;
    loopv(clients)
    {
        client &c = *clients[i];
        if(c.type!=ST_TCPIP) continue;
        ENetPacket *packet;
        if(psize && (c.positionoffset<0 || psize-c.position.length()>0))
        {
            packet = enet_packet_create(&ws.positions[c.positionoffset<0 ? 0 : c.positionoffset+c.position.length()],
                                        c.positionoffset<0 ? psize : psize-c.position.length(),
                                        ENET_PACKET_FLAG_NO_ALLOCATE);
            sendpacket(c.clientnum, 0, packet);
            if(!packet->referenceCount) enet_packet_destroy(packet);
            else { ++ws.uses; packet->freeCallback = cleanworldstate; };
        };
        c.position.setsizenodelete(0);

        if(msize && (c.messageoffset<0 || msize-3-c.messages.length()>0))
        {
            packet = enet_packet_create(&ws.messages[c.messageoffset<0 ? 0 : c.messageoffset+3+c.messages.length()],
                                        c.messageoffset<0 ? msize : msize-3-c.messages.length(),
                                        (reliablemessages ? ENET_PACKET_FLAG_RELIABLE : 0) | ENET_PACKET_FLAG_NO_ALLOCATE);
            sendpacket(c.clientnum, 2, packet);
            if(!packet->referenceCount) enet_packet_destroy(packet);
            else { ++ws.uses; packet->freeCallback = cleanworldstate; };
        };
        c.messages.setsizenodelete(0);
    };
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
    };
};

int maxclients = DEFAULTCLIENTS;
string smapname;

char *masterpasswd = NULL;

int numclients()
{
    int num = 0;
    loopv(clients) if(clients[i]->type!=ST_EMPTY) num++;
    return num;
};

void zapclient(int c)
{
	if(c<0 || c>=clients.length()) return;
	clients[c]->type = ST_EMPTY;
	clients[c]->ismaster = clients[c]->isauthed = false;
};

clientscore *findscore(client &c, bool insert)
{
    if(c.type!=ST_TCPIP) return NULL;
    if(!insert) loopv(clients)
    {
        client &o = *clients[i];
        if(o.type!=ST_TCPIP) continue;
        if(o.clientnum!=c.clientnum && o.peer->address.host==c.peer->address.host && !strcmp(o.name, c.name)) return &o.score;
    };
    loopv(scores)
    {
        savedscore &sc = scores[i];
        if(!strcmp(sc.name, c.name) && sc.ip==c.peer->address.host) return &sc;
    };
    if(!insert) return NULL;
    savedscore &sc = scores.add();
    sc.reset();
    s_strcpy(sc.name, c.name);
    sc.ip = c.peer->address.host;
    return &sc;
};

void resetscores()
{
    loopv(clients) if(clients[i]->type==ST_TCPIP)
    {
        clients[i]->score.reset();
    };
    scores.setsize(0);
};

vector<server_entity> sents;

bool notgotitems = true;        // true when map has changed and waiting for clients to send item
int mode = 0;


void restoreserverstate(vector<entity> &ents)   // hack: called from savegame code, only works in SP
{
    loopv(sents)
    {
        sents[i].spawned = ents[i].spawned;
        sents[i].spawnsecs = 0;
    }; 
};

int interm = 0, minremain = 0, mapend = 0;
bool mapreload = false;

char *serverpassword = "";

bool isdedicated;
ENetHost *serverhost = NULL;

void process(ENetPacket *packet, int sender, int chan);
void multicast(ENetPacket *packet, int sender, int chan);
void disconnect_client(int n, int reason);

void sendf(int cn, int chan, const char *format, ...)
{
    bool reliable = false;
    if(*format=='r') { reliable = true; ++format; };
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    ucharbuf p(packet->data, packet->dataLength);
    va_list args;
    va_start(args, format);
    while(*format) switch(*format++)
    {
        case 'i':
        {
            int n = isdigit(*format) ? *format++-'0' : 1;
            loopi(n) putint(p, va_arg(args, int));
            break;
        };
        case 's': sendstring(va_arg(args, const char *), p); break;
    };
    va_end(args);
    enet_packet_resize(packet, p.length());
    if(cn<0) multicast(packet, -1, chan);
    else sendpacket(cn, chan, packet);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
};

struct ctfflag
{
    int state;
    int actor_cn;
    int pos[3];
    int lastupdate;
} ctfflags[2];

void sendflaginfo(int flag, int action, int cn = -1)
{
    ctfflag &f = ctfflags[flag];
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_FLAGINFO);
    putint(p, flag); 
    putint(p, f.state); 
    putint(p, action); 
    if(f.state==CTFF_STOLEN || action==SV_FLAGRETURN) putint(p, f.actor_cn);
    else if(f.state==CTFF_DROPPED) loopi(3) putint(p, f.pos[i]);
    enet_packet_resize(packet, p.length());
    if(cn<0) multicast(packet, -1, 1);
    else sendpacket(cn, 1, packet);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
};

void ctfreset()
{
    loopi(2) 
    {
        ctfflags[i].actor_cn = 0;
        ctfflags[i].state = CTFF_INBASE;
        ctfflags[i].lastupdate = -1;
    };
};

void sendservmsg(char *msg, int client=-1)
{
    sendf(client, 1, "ris", SV_SERVMSG, msg);
};

char *disc_reasons[] = { "normal", "end of packet", "client num", "kicked by master", "banned by master", "tag type", "connection refused due to ban", "wrong password", "failed master login", "server FULL (maxclients)"};

void disconnect_client(int n, int reason)
{
    if(n<0 || n>=clients.length() || clients[n]->type!=ST_TCPIP) return;
    clientscore *sc = findscore(*clients[n], true);
    if(sc) *sc = clients[n]->score;
	printf("disconnecting client (%s) [%s]\n", clients[n]->hostname, disc_reasons[reason]);
    enet_peer_disconnect(clients[n]->peer, reason);
	zapclient(n);
    sendf(-1, 1, "rii", SV_CDIS, n);
};

void resetitems() { sents.setsize(0); notgotitems = true; };

void pickup(uint i, int sec, int sender)         // server side item pickup, acknowledge first client that gets it
{
    if(i>=(uint)sents.length()) return;
    if(sents[i].spawned)
    {
        sents[i].spawned = false;
        sents[i].spawnsecs = sec;
        sendf(sender, 1, "rii", SV_ITEMACC, i);
    };
};

// EDIT: AH
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
    while((p = strstr(l, "\n")) != NULL || (l && (lastline=true))) // remove empty/invalid lines
    {
        size_t len = lastline ? strlen(l) : p-l;
        string line;
        s_strncpy(line, l, len+1);
        char *d = line;
        int n = 0;
        while((p = strstr(d, ":")) != NULL && (d = p+1)) n++;
        if(n!=3) memset(l, ' ', len+1);
        if(lastline) break;
        l += len+1;
    };
         
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
            for(numspaces = 0; argv[0][numspaces]==' '; numspaces++){}; // ingore space crap
            strcpy(c.mapname, argv[0]+numspaces);
            c.mode = atoi(argv[1]);
            c.time = atoi(argv[2]);
            c.vote = atoi(argv[3]) > 0;
            configsets.add(c);
            argc = 0;
        };
        p = strtok(NULL, ":\n\0");
    };
};

void resetvotes()
{
    loopv(clients) clients[i]->mapvote[0] = 0;
};

void resetmap(const char *newname, int newmode, int newtime, bool notify = true)
{
    mode = newmode;
    minremain = newtime;
    s_strcpy(smapname, newname);

    mapend = lastsec+minremain*60;
    mapreload = false;
    interm = 0;
    laststatus = lastsec-61;
    resetvotes();
    resetitems();
    resetscores();
    ctfreset();
    if(notify) sendf(-1, 1, "risi", SV_MAPCHANGE, smapname, mode);
};

void nextcfgset(bool notify = true) // load next maprotation set
{   
    curcfgset++;
    if(curcfgset>=configsets.length() || curcfgset<0) curcfgset=0;
    
    configset &c = configsets[curcfgset];
    resetmap(c.mapname, c.mode, c.time, notify);
};

bool vote(char *map, int reqmode, int sender)
{
	if(!valid_client(sender)) return false;

    if(configsets.length() && curcfgset < configsets.length() && !configsets[curcfgset].vote && !clients[sender]->ismaster)
    {
        s_sprintfd(msg)("%s voted, but voting is currently disabled", clients[sender]->name);
        sendservmsg(msg);
        return false;
    };

    s_strcpy(clients[sender]->mapvote, map);
    clients[sender]->modevote = reqmode;
    int yes = 0, no = 0; 
    loopv(clients) if(clients[i]->type!=ST_EMPTY)
    {
        if(clients[i]->mapvote[0]) { if(strcmp(clients[i]->mapvote, map)==0 && clients[i]->modevote==reqmode) yes++; else no++; }
        else no++;
    };
    if(yes==1 && no==0) return true;  // single player
    s_sprintfd(msg)("%s suggests %s on map %s (set map to vote)", clients[sender]->name, modestr(reqmode), map);
    sendservmsg(msg);
    if(yes/(float)(yes+no) <= 0.5f && !clients[sender]->ismaster) return false;
    sendservmsg("vote passed");
    resetvotes();
    return true;
};

struct ban
{
	ENetAddress address;
	int secs;
};

vector<ban> bans;

bool isbanned(int cn)
{
	if(!valid_client(cn)) return false;
	client &c = *clients[cn];
	loopv(bans)
	{
		ban &b = bans[i];
		if(b.secs < lastsec) { bans.remove(i--); };
		if(b.address.host == c.peer->address.host) { return true; };
	};
	return false;
};

void getmaster(int sender, char *pwd)
{ 
	if(!pwd[0] || !isdedicated) return;
	if(masterpasswd && !strcmp(masterpasswd, pwd)) 
	{
		clients[sender]->ismaster = true;
		sendservmsg("master login successful", sender);
	}
	else disconnect_client(sender, DISC_MLOGINFAIL);
};

void mastercmd(int sender, int cmd, int a)
{
	if(!isdedicated) return;
	if(clients[sender]->ismaster==false) return;
	switch(cmd)
	{
		case MCMD_KICK:
		{
			if(!valid_client(a)) return;
			disconnect_client(a, DISC_MKICK);
			sendservmsg("player kicked");
			break;
		};
		case MCMD_BAN:
		{
			if(!valid_client(a)) return;
			ban b = { clients[a]->peer->address, lastsec+20*60 };
			bans.add(b);
			disconnect_client(a, DISC_MBAN);
			sendservmsg("player banned");
			break;
		};
		case MCMD_REMBANS:
		{
			if(bans.length()) bans.setsize(0);
			sendservmsg("bans removed");
			break;
		};
	};
};

void sendmapinfo(int c)
{
	if(!valid_client(c)) return;
	if(smapname[0])
	{
		ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        ucharbuf p(packet->data, packet->dataLength);
		putint(p, SV_MAPCHANGE);
		sendstring(smapname, p);
		putint(p, mode);
		putint(p, SV_ITEMLIST);
		loopv(sents) if(sents[i].spawned) putint(p, i);
		putint(p, -1);
		enet_packet_resize(packet, p.length());
		sendpacket(c, 1, packet);
	};
};

// server side processing of updates: does very little and most state is tracked client only
// could be extended to move more gameplay to server (at expense of lag)

#ifdef STANDALONE
#define CN_CHECK if((cn>=0 && cn!=sender) || !valid_client(cn)) { if(sender>=0) disconnect_client(sender, DISC_CN);  return; };
#define SENDER_CHECK if(sender<0) return;
#else
#define CN_CHECK if((cn>=0 && cn!=sender) || !valid_client(cn)) { if(sender>=0) disconnect_client(sender, DISC_CN); conoutf("ERROR: invalid client (msg %i)", type); return; };
#define SENDER_CHECK if(sender<0) { conoutf("ERROR: invalid sender (msg %i)", type); return; };
#endif

void process(ENetPacket *packet, int sender, int chan)   // sender may be -1
{
    ucharbuf p(packet->data, packet->dataLength);
    char text[MAXTRANS];
    client *cl = sender>=0 ? clients[sender] : NULL;
    int cn = sender, type;

	if(serverpassword[0] && sender>=0 && sender<=clients.length() && !clients[sender]->isauthed)
	{
		int msg = getint(p);
		getstring(text, p);
		if(msg != SV_PWD || !text[0] || strcmp(text, serverpassword)) disconnect_client(sender, DISC_WRONGPW);
		else
		{
			clients[sender]->isauthed = true;
			sendmapinfo(sender);
		};
		return;
	};

    if(packet->flags&ENET_PACKET_FLAG_RELIABLE) reliablemessages = true;

    #define QUEUE_MSG { while(curmsg<p.length()) cl->messages.add(p.buf[curmsg++]); }
    int curmsg;
    while((curmsg = p.length()) < p.maxlen) switch(type = getint(p))
    {
        case SV_CDIS:
        {
            int n = getint(p);
            if(m_ctf_s)
                loopi(2) if(ctfflags[i].state==CTFF_STOLEN && ctfflags[i].actor_cn==n)
                    sendf(-1, 1, "rii", SV_FLAGDROP, i);
            QUEUE_MSG;
            break;
        };

        case SV_TEXT:
            getstring(text, p);
            QUEUE_MSG;
            break;

        case SV_INITC2S:
        {
			CN_CHECK;
            bool newclient = false;
            if(!clients[cn]->name[0]) newclient = true;
            getstring(text, p);
            if(!text[0]) s_strcpy(text, "unarmed");
            s_strcpy(clients[cn]->name, text);
            if(newclient)
            {
                clientscore *sc = findscore(*clients[cn], false);
                if(sc)
                {
                    cl->score = *sc; 
                    sendf(-1, 1, "ri4", SV_RESUME, cn, sc->frags, sc->flags);
                };
            };
            getstring(text, p);
            getint(p);
            getint(p);
            QUEUE_MSG;
            break;
        };

        case SV_MAPCHANGE:
        {
			SENDER_CHECK;
            getstring(text, p);
            int reqmode = getint(p);
            if(reqmode<0) reqmode = 0;
            if(smapname[0] && !mapreload && !vote(text, reqmode, sender)) return;
            resetmap(text, reqmode, m_ctf_s ? 10 : (m_teammode_s ? 15 : 10));
            break;
        };
        
        case SV_ITEMLIST:
        {
            int n;
            while((n = getint(p))!=-1) if(notgotitems)
            {
                server_entity se = { false, 0 };
                while(sents.length()<=n) sents.add(se);
                sents[n].spawned = true;
            };
            notgotitems = false;
            QUEUE_MSG;
            break;
        };

        case SV_ITEMPICKUP:
        {
			SENDER_CHECK;
            int n = getint(p);
            pickup(n, getint(p), sender);
            QUEUE_MSG;
            break;
        };
        
        case SV_PING:
			CN_CHECK;
            sendf(cn, 1, "ii", SV_PONG, getint(p));
            break;

        case SV_POS:
        {
            cn = getint(p);
            CN_CHECK;
            loopi(3) clients[cn]->pos[i] = getuint(p);
            getuint(p);
            loopi(6) getint(p);
            cl->position.setsizenodelete(0);
            while(curmsg<p.length()) cl->position.add(p.buf[curmsg++]);
            break;
        };

        case SV_SENDMAP:
        {
			SENDER_CHECK;
            getstring(text, p);
            int mapsize = getint(p);
            if(p.remaining() < mapsize)
            {
                p.forceoverread();
                break;
            };
            sendmaps(sender, text, mapsize, &p.buf[p.len]);
            p.len += mapsize;
            break;
        };

        case SV_RECVMAP:
        {
			SENDER_CHECK;
            ENetPacket *mappacket = recvmap(sender);
			if(mappacket)
            {
                sendpacket(sender, 3, mappacket);
                if(!mappacket->referenceCount) enet_packet_destroy(mappacket);
            }
            else sendservmsg("no map to get", sender);
            break;
        };

        // EDIT: AH
        case SV_FLAGPICKUP:
        {
			SENDER_CHECK;
            int flag = getint(p);
            if(!valid_flag(flag)) return;
            ctfflag &f = ctfflags[flag];
            if(f.state!=CTFF_STOLEN)
            {
                f.state = CTFF_STOLEN;
                f.actor_cn = sender;
				f.lastupdate = lastsec;
                sendflaginfo(flag, SV_FLAGPICKUP);
				printf("flag picked by %s\n", clients[sender]->name);
            };
            break;
        };
        
        case SV_FLAGDROP:
        {
			SENDER_CHECK;
            int flag = getint(p);
            if(!valid_flag(flag)) return;
            ctfflag &f = ctfflags[flag];
            if(f.state==CTFF_STOLEN && (sender==-1 || f.actor_cn==sender))
            {
                f.state = CTFF_DROPPED;
                f.lastupdate = lastsec;
                loopi(3) f.pos[i] = clients[sender]->pos[i];
                sendflaginfo(flag, SV_FLAGDROP);
            };
            break;
        };
        
        case SV_FLAGRETURN:
        {
			SENDER_CHECK;
            int flag = getint(p);
            if(!valid_flag(flag)) return;
            ctfflag &f = ctfflags[flag];
            if(f.state==CTFF_DROPPED)
            {
                f.state = CTFF_INBASE;
                f.actor_cn = sender;
				f.lastupdate = lastsec;
                sendflaginfo(flag, SV_FLAGRETURN);
            };
            break;
        };
        
        case SV_FLAGSCORE:
        {
			SENDER_CHECK;
            int flag = getint(p);
            if(!valid_flag(flag)) return;
            ctfflag &f = ctfflags[flag];
            if(f.state==CTFF_STOLEN)
            {
                f.state = CTFF_INBASE;
                f.actor_cn = sender;
				f.lastupdate = lastsec;
                sendflaginfo(flag, SV_FLAGSCORE);
            };
            break;
        };
    
		case SV_GETMASTER:
			SENDER_CHECK;
			getstring(text, p);
			getmaster(sender, text);
			break;

		case SV_MASTERCMD:
		{
			SENDER_CHECK;
			int cmd = getint(p);
			int arg = getint(p);
			mastercmd(sender, cmd, arg);
			break;
		};

		case SV_PWD:
			getstring(text, p);
		    break;

        case SV_FRAGS:
            CN_CHECK;
            cl->score.frags = getint(p);
            QUEUE_MSG;
            break;

        case SV_FLAGS:
            CN_CHECK;
            cl->score.flags = getint(p);
            QUEUE_MSG;
            break;

        default:
        {
            int size = msgsizelookup(type);
            if(size==-1) { if(sender>=0) disconnect_client(sender, DISC_TAGT); return; };
            loopi(size-1) getint(p);
            QUEUE_MSG;
            break;
        };
    };

    if(p.overread()) { if(sender>=0) disconnect_client(sender, DISC_EOP); return; };
};

void send_welcome(int n)
{
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_INITS2C);
    putint(p, n);
    putint(p, PROTOCOL_VERSION);
    if(!smapname[0] && configsets.length()) nextcfgset(false); // EDIT:AH
    putint(p, smapname[0]);
	putint(p, serverpassword[0] ? 1 : 0);
	int numcl = numclients();
    if(smapname[0] && !serverpassword[0])
    {
        putint(p, SV_MAPCHANGE);
        sendstring(smapname, p);
        putint(p, mode);
		if(!configsets.length() || numcl > 1)
		{
			putint(p, SV_ITEMLIST);
			loopv(sents) if(sents[i].spawned) putint(p, i);
			putint(p, -1);
		};
    };
    loopv(clients)
    {
        client &c = *clients[i];
        if(c.type!=ST_TCPIP || c.clientnum==n) continue;
        if(!c.score.frags && !c.score.flags) continue;
        putint(p, SV_RESUME);
        putint(p, c.clientnum);
        putint(p, c.score.frags);
        putint(p, c.score.flags);
    };
    enet_packet_resize(packet, p.length());
    sendpacket(n, 1, packet);
    if(smapname[0] && m_ctf_s) loopi(2) sendflaginfo(i, -1, n); // EDIT: AH
};

void multicast(ENetPacket *packet, int sender, int chan)
{
    loopv(clients)
    {
        if(i==sender) continue;
        sendpacket(i, chan, packet);
    };
};

void localclienttoserver(int chan, ENetPacket *packet)
{
    process(packet, 0, chan);
    if(!packet->referenceCount) enet_packet_destroy(packet);
};

client &addclient()
{
    client *c = NULL; 
    loopv(clients) if(clients[i]->type==ST_EMPTY) { c = clients[i]; break; };
    if(!c) 
    { 
        c = new client; 
        c->clientnum = clients.length(); 
        clients.add(c);
    };
    c->reset();
    return *c;
};

void checkintermission()
{
    if(!minremain)
    {
        interm = lastsec+10;
        mapend = lastsec+1000;
    };
    sendf(-1, 1, "rii", SV_TIMEUP, minremain--);
};

/*void startintermission() { minremain = 0; checkintermission(); };*/

void resetserverifempty()
{
    loopv(clients) if(clients[i]->type!=ST_EMPTY) return;
    //clients.setsize(0);
    resetmap("", 0, 10, false);
};

int nonlocalclients = 0;

int refuseconnect(int i)
{
    if(valid_client(i) && isbanned(i)) return DISC_BANREFUSE;
    return DISC_NONE;
};

void sendworldstate()
{
    static enet_uint32 lastsend = 0;
    if(clients.empty()) return;
    enet_uint32 curtime = enet_time_get()-lastsend;
    if(curtime<40) return;
    bool flush = buildworldstate();
    lastsend += curtime - (curtime%40);
    if(flush) enet_host_flush(serverhost);
};

void serverslice(int seconds, unsigned int timeout)   // main server update, called from cube main loop in sp, or dedicated server loop
{
    loopv(sents)        // spawn entities when timer reached
    {
        if(sents[i].spawnsecs && (sents[i].spawnsecs -= seconds-lastsec)<=0)
        {
            sents[i].spawnsecs = 0;
            sents[i].spawned = true;
            sendf(-1, 1, "rii", SV_ITEMSPAWN, i);
        };
    };
    
    if(m_ctf_s)
    {
        loopi(2)
        {
            ctfflag &f = ctfflags[i];
            if(f.state==CTFF_DROPPED && seconds-f.lastupdate>30)
            {
               f.state=CTFF_INBASE;
               sendflaginfo(i, -1);
               s_sprintfd(msg)("the server reset the %s flag", rb_team_string(i));
               sendservmsg(msg);
               sendf(-1, 1, "rii", SV_SOUND, S_FLAGRETURN);
            };
        };
    };
    
    lastsec = seconds;
    
	if((mode>1 || (mode==0 && nonlocalclients)) && seconds>mapend-minremain*60) checkintermission();
    if(interm && seconds>interm)
    {
        interm = 0;

        if(configsets.length()) nextcfgset();
        else loopv(clients) if(clients[i]->type!=ST_EMPTY)
        {
            sendf(i, 1, "rii", SV_MAPRELOAD, 0);    // ask a client to trigger map reload
            mapreload = true;
            break;
        };
    };

    resetserverifempty();
    
    if(!isdedicated) return;     // below is network only

	int numplayers = numclients();
	serverms(mode, numplayers, minremain, smapname, seconds, numclients()>=maxclients);

    if(seconds-laststatus>60)   // display bandwidth stats, useful for server ops
    {
        nonlocalclients = 0;
        loopv(clients) if(clients[i]->type==ST_TCPIP) nonlocalclients++;
        laststatus = seconds;     
		if(nonlocalclients || bsend || brec) 
		{ 
			printf("status: %d remote clients, %.1f send, %.1f rec (K/sec)\n", nonlocalclients, bsend/60.0f/1024, brec/60.0f/1024); 
#ifdef _DEBUG
			fflush(stdout);
#endif
		}
        bsend = brec = 0;
    };

    ENetEvent event;
    if(enet_host_service(serverhost, &event, timeout) > 0)
    {
        switch(event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                client &c = addclient();
                c.type = ST_TCPIP;
                c.peer = event.peer;
                c.peer->data = (void *)(size_t)c.clientnum;
				c.ismaster = c.isauthed = false;
				char hn[1024];
				s_strcpy(c.hostname, (enet_address_get_host(&c.peer->address, hn, sizeof(hn))==0) ? hn : "unknown");
				printf("client connected (%s)\n", c.hostname);
                int reason = DISC_MAXCLIENTS;
                if(nonlocalclients<maxclients && !(reason = refuseconnect(c.clientnum))) send_welcome(c.clientnum);
                else disconnect_client(c.clientnum, reason);
				break;
            };

            case ENET_EVENT_TYPE_RECEIVE:
			{
                brec += (int)event.packet->dataLength;
				int cn = (int)(size_t)event.peer->data;
				if(valid_client(cn)) process(event.packet, cn, event.channelID); 
                if(event.packet->referenceCount==0) enet_packet_destroy(event.packet);
                break;
			};

            case ENET_EVENT_TYPE_DISCONNECT: 
            {
				int cn = (int)(size_t)event.peer->data;
				if(!valid_client(cn)) break;
                client &c = *clients[cn];
                printf("disconnected client (%s)\n", c.hostname);
                clientscore *sc = findscore(c, true);
                if(sc) *sc = c.score;
				zapclient(cn);
                sendf(-1, 1, "rii", SV_CDIS, cn);
                event.peer->data = (void *)-1;
                break;
            };

            default:
                break;
        };
    };
    sendworldstate();
};

void cleanupserver()
{
    if(serverhost) enet_host_destroy(serverhost);
};

void localdisconnect()
{
    loopv(clients) if(clients[i]->type==ST_LOCAL) zapclient(i);
};

void localconnect()
{
    client &c = addclient();
    c.type = ST_LOCAL;
    s_strcpy(c.hostname, "local");
    send_welcome(c.clientnum);
};

void initserver(bool dedicated, int uprate, char *sdesc, char *ip, char *master, char *passwd, int maxcl, char *maprot, char *masterpwd) // EDIT: AH
{
    serverpassword = passwd;
    maxclients = maxcl ? min(maxcl, MAXCLIENTS) : DEFAULTCLIENTS;
	servermsinit(master ? master : "masterserver.cubers.net/cgi-bin/actioncube.pl/", sdesc, dedicated);
    
    if(isdedicated = dedicated)
    {
        ENetAddress address = { ENET_HOST_ANY, CUBE_SERVER_PORT };
        if(*ip && enet_address_set_host(&address, ip)<0) printf("WARNING: server ip not resolved");
        serverhost = enet_host_create(&address, maxclients, 0, uprate);
        if(!serverhost) fatal("could not create server host\n");
        loopi(maxclients) serverhost->peers[i].data = (void *)-1;
		if(!maprot || !maprot[0]) maprot = newstring("config/maprot.cfg");
        readscfg(path(maprot));
		if(masterpwd && masterpwd[0]) masterpasswd = masterpwd;
    };

    resetserverifempty();

    if(isdedicated)       // do not return, this becomes main loop
    {
        #ifdef WIN32
        SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
        #endif
        printf("dedicated server started, waiting for clients...\nCtrl-C to exit\n\n");
        atexit(cleanupserver);
        atexit(enet_deinitialize);
        for(;;) serverslice(/*enet_time_get_sec()*/time(NULL), 5);
    };
};
