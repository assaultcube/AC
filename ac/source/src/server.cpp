// server.cpp: little more than enhanced multicaster
// runs dedicated or as client coroutine

#include "cube.h" 

#define m_ctf_s (mode==5)
#define m_teammode_s (mode==0 || mode==4 || mode==5)

enum { ST_EMPTY, ST_LOCAL, ST_TCPIP };

struct client                   // server side version of "dynent" type
{
    int type;
    ENetPeer *peer;
    string hostname;
    string mapvote;
    string name;
    int modevote;
    // Added by Rick: For bot voting
#ifndef STANDALONE
    bool addbot, kickbot, changebotskill;
    short botcount;
    string botteam, botname;
    short botskill;
    bool kickallbots;
#endif
    // End add
	bool ismaster;
};

vector<client> clients;

int maxclients = 8;
string smapname;

char *masterpasswd = NULL;
enum { MCMD_KICK = 0, MCMD_BAN };

int numclients()
{
    int num = 0;
    loopv(clients) if(clients[i].type!=ST_EMPTY) num++;
    return num;
};

vector<server_entity> sents;

bool notgotitems = true;        // true when map has changed and waiting for clients to send item
int mode = 0;

// Added by Rick
#ifndef STANDALONE
extern void kickbot(const char *szName);
extern void kickallbots(void);
#endif
// End add

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
ENetHost * serverhost = NULL;
int bsend = 0, brec = 0, laststatus = 0, lastsec = 0;

#define MAXOBUF 100000

void process(ENetPacket *packet, int sender);
void multicast(ENetPacket *packet, int sender);
void disconnect_client(int n, char *reason);

void send(int n, ENetPacket *packet)
{
	if(!packet) return;
    switch(clients[n].type)
    {
        case ST_TCPIP:
        {
            enet_peer_send(clients[n].peer, 0, packet);
            bsend += packet->dataLength;
            break;
        };

        case ST_LOCAL:
            localservertoclient(packet->data, packet->dataLength);
            break;

    };
};

void send2(bool rel, int cn, int a, int b)
{
    ENetPacket *packet = enet_packet_create(NULL, 32, rel ? ENET_PACKET_FLAG_RELIABLE : 0);
    uchar *start = packet->data;
    uchar *p = start+2;
    putint(p, a);
    putint(p, b);
    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
    enet_packet_resize(packet, p-start);
    if(cn<0) process(packet, -1);
    else send(cn, packet);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
};

struct ctfflag
{
    int state;
    int thief_cn;
    int pos[3];
    int lastupdate;
} ctfflags[2];

bool ctfbroadcast = false;

void sendflaginfo(int flag, int action, int cn = -1)
{
    ctfflag &f = ctfflags[flag];
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start+2;
    putint(p, SV_FLAGINFO);
    putint(p, flag); 
    putint(p, f.state); 
    putint(p, action); 
    if(f.state==CTFF_STOLEN || action==SV_FLAGRETURN) putint(p, f.thief_cn);
    else if(f.state==CTFF_DROPPED) loopi(3) putint(p, f.pos[i]);
    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
    enet_packet_resize(packet,p-start);
    if(cn<0) multicast(packet, -1);
    else send(cn, packet);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
};

void ctfreset(bool send=true)
{
    if(!m_ctf_s) return;
    loopi(2) 
    {
        ctfflags[i].thief_cn = 0;
        ctfflags[i].state = CTFF_INBASE;
        ctfflags[i].lastupdate = -1;
        if(send) sendflaginfo(i, -1);
    };
}

void sendservmsg(char *msg, int client=-1)
{
    ENetPacket *packet = enet_packet_create(NULL, _MAXDEFSTR+10, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start+2;
    putint(p, SV_SERVMSG);
    sendstring(msg, p);
    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
    enet_packet_resize(packet, p-start);
    if(client==-1) multicast(packet, -1); else send(client, packet);
    if(packet->referenceCount==0) enet_packet_destroy(packet);
};

void disconnect_client(int n, char *reason)
{
    if(n<0 || n>=clients.length()) return;
	sprintf_sd(clientreason)("server closed the connection: %s", reason);
	sendservmsg(clientreason, n);
    enet_peer_disconnect(clients[n].peer);
	printf("disconnecting client (%s) [%s]\n", clients[n].hostname, reason);
    clients[n].type = ST_EMPTY;
	clients[n].ismaster = false;
    send2(true, -1, SV_CDIS, n);
};

void resetitems() { sents.setsize(0); notgotitems = true; };

void pickup(uint i, int sec, int sender)         // server side item pickup, acknowledge first client that gets it
{
    if(i>=(uint)sents.length()) return;
    if(sents[i].spawned)
    {
        sents[i].spawned = false;
        sents[i].spawnsecs = sec;
        send2(true, sender, SV_ITEMACC, i);
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
    strcpy_s(s, cfg);
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
        int len = lastline ? strlen(l) : p-l;
        string line;
        strn0cpy(line, l, len+1);
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
            c.vote = (bool) atoi(argv[3]);
            configsets.add(c);
            argc = 0;
        };
        p = strtok(NULL, ":\n\0");
    };
};

void nextcfgset() // load next maprotation set
{   
    curcfgset++;
    if(curcfgset>=configsets.length() || curcfgset<0) curcfgset=0;
    
    configset &c = configsets[curcfgset];
    mode = c.mode;
    minremain = c.time;
    strcpy_s(smapname, c.mapname);
    
    mapend = lastsec+minremain*60;
    mapreload = false;
    interm = 0;
    laststatus = lastsec-61;
    resetitems();
};

void resetvotes()
{
    loopv(clients) clients[i].mapvote[0] = 0;
};

bool vote(char *map, int reqmode, int sender)
{
    if(configsets.length() && curcfgset < configsets.length() && !configsets[curcfgset].vote)
    {
        sprintf_sd(msg)("%s voted, but voting is currently disabled", clients[sender].name);
        sendservmsg(msg);
        return false;
    };
    strcpy_s(clients[sender].mapvote, map);
    clients[sender].modevote = reqmode;
    int yes = 0, no = 0; 
    loopv(clients) if(clients[i].type!=ST_EMPTY)
    {
        if(clients[i].mapvote[0]) { if(strcmp(clients[i].mapvote, map)==0 && clients[i].modevote==reqmode) yes++; else no++; }
        else no++;
    };
    if(yes==1 && no==0) return true;  // single player
    sprintf_sd(msg)("%s suggests %s on map %s (set map to vote)", clients[sender].name, modestr(reqmode), map);
    sendservmsg(msg);
    if(yes/(float)(yes+no) <= 0.5f) return false;
    sendservmsg("vote passed");
    resetvotes();
    return true;
};

//fixmebot
// Added by Rick: Vote for bot commands

#ifndef STANDALONE
void resetbotvotes()
{
    loopv(clients)
    {
         clients[i].addbot = false;
         clients[i].kickbot = false;
         clients[i].changebotskill = false;
         clients[i].kickallbots = false;
         clients[i].botcount = 0;
         clients[i].botteam[0] = 0;
         clients[i].botname[0] = 0;
         clients[i].botskill = -1;
    }
};

bool addbotvote(int count, char *team, int skill, char *name, int sender)
{
    clients[sender].addbot = true;
    clients[sender].kickbot = false;
    clients[sender].changebotskill = false;
    clients[sender].botcount = (count>=0) ? count : 0;
    if (team && team[0]) strcpy_s(clients[sender].botteam, team);
    else clients[sender].botteam[0] = 0;
    clients[sender].botskill = skill;
    if (name && name[0]) strcpy_s(clients[sender].botname, name);
    else clients[sender].botname[0] = 0;
    int yes = 0, no = 0; 
    loopv(clients) if(clients[i].type!=ST_EMPTY)
    {
        if (clients[i].addbot)
        {
              if (!strcmp(clients[i].botteam, team) && !strcmp(clients[i].botname, name) &&
                  (clients[i].botskill == skill) &&
                  (clients[i].botcount == clients[sender].botcount))
                   yes++;
              else
                   no++;
        }
        else no++;
    }
    if(yes==1 && no==0) return true;  // single player
    sprintf_sd(msg)("%s suggests to add a bot", clients[sender].name);
    if (team && team[0])
    {
         strcat(msg, " on team ");
         strcat(msg, team);
    }

    if (skill!=-1)
    {
         strcat(msg, ", with skill ");
         strcat(msg, SkillNrToSkillName(skill));
    }

    if (name && name[0])
    {
         strcat(msg, " named ");
         strcat(msg, name);
    }

    sendservmsg(msg);
    if(yes/(float)(yes+no) <= 0.5f) return false;
    sendservmsg("vote passed");
    resetbotvotes();

    return true;
};


bool kickbotvote(int specific, char *name, int sender)
{
    clients[sender].addbot = false;
    clients[sender].kickbot = true;
    clients[sender].changebotskill = false;
    clients[sender].kickallbots = !specific;
    if (name && name[0]) strcpy_s(clients[sender].botname, name);
    else clients[sender].botname[0] = 0;
    int yes = 0, no = 0; 
    loopv(clients) if(clients[i].type!=ST_EMPTY)
    {
        if (clients[i].kickbot)
        {
             if (clients[sender].kickallbots)
             {
                  if (clients[i].kickallbots) yes++;
                  else no++;
             }
             else
             {
                  if (!clients[i].kickallbots && !strcmp(clients[i].botname, name))
                       yes++;
                  else
                       no++;
             }
        }
        else no++;
    }
    if(yes==1 && no==0) return true;  // single player

    char msg[256];
    if (clients[sender].kickallbots)
         sprintf(msg, "%s suggests to kick all bots", clients[sender].name);
    else
         sprintf(msg, "%s suggests to kick bot %s", clients[sender].name,
                         clients[sender].botname);

    sendservmsg(msg);
    if(yes/(float)(yes+no) <= 0.5f) return false;
    sendservmsg("vote passed");
    resetbotvotes();
    return true;    
};

bool botskillvote(int skill, int sender)
{
    clients[sender].addbot = false;
    clients[sender].kickbot = false;
    clients[sender].changebotskill = true;
    clients[sender].botskill = skill;
    int yes = 0, no = 0; 
    loopv(clients) if(clients[i].type!=ST_EMPTY)
    {
        if (clients[i].changebotskill)
        {
             if (clients[sender].botskill == clients[i].botskill) yes++;
             else no++;
        }
        else no++;
    }
    if(yes==1 && no==0) return true;  // single player

    sprintf_sd(msg)("%s suggests to change the skill of all bots to: %s", clients[sender].name,
                    SkillNrToSkillName(clients[sender].botskill));
    sendservmsg(msg);
    if(yes/(float)(yes+no) <= 0.5f) return false;
    sendservmsg("vote passed");
    resetbotvotes();
    return true;
};

void botcommand(uchar *&p, char *text, int sender)
{
     int type = getint(p);
     switch(EBotCommands(type))
     {
          case COMMAND_ADDBOT:
          {
               string name, team;
               int count = getint(p);
               int skill = getint(p);
               sgetstr();
               strcpy(team, text); 
               sgetstr();
               strcpy(name, text);
               if (addbotvote(count, team, skill, name, sender))
               {
                    while(count>0)
                    {
                         BotManager.CreateBot(team, SkillNrToSkillName(skill), name);
                         count--;
                    }
               }
               break;
          }
          case COMMAND_KICKBOT:
          {
               int specific = getint(p);

               if (specific)
                    sgetstr();

               if (kickbotvote(specific, text, sender))
               {
                    if (specific)
                         kickbot(text);
                    else
                         kickallbots();
               }
          }
          case COMMAND_BOTSKILL:
          {
               int skill = getint(p);
               if (botskillvote(skill, sender))
               {
                    BotManager.ChangeBotSkill(skill, NULL);
               }
          }
     }
}
#endif
// End add


// force map change, EDIT: AH
void changemap()
{
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start+2;
    putint(p, SV_MAPCHANGE);
    sendstring(smapname, p);
    putint(p, mode);
    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
    enet_packet_resize(packet,p-start);
    multicast(packet, -1);
    if(packet->referenceCount==0) enet_packet_destroy(packet);   
};

void getmaster(int sender, char *pwd)
{
	if(!pwd[0] || !isdedicated) return;
	if(masterpasswd && !strcmp(masterpasswd, pwd)) clients[sender].ismaster = true;
	else disconnect_client(sender, "failed master login");
};

void mastercmd(int sender, int cmd, int arg1)
{
	if(!isdedicated) return;
	if(clients[sender].ismaster==false) disconnect_client(sender, "access to master commands denied");
	switch(cmd)
	{
		case MCMD_KICK:
		{
			if(arg1 < 0 && arg1 >= clients.length()) return;
			disconnect_client(sender, "you were kicked from the server");
			break;
		};
		case MCMD_BAN:
		{
			if(arg1 < 0 && arg1 >= clients.length()) return;
			disconnect_client(sender, "you were kicked from the server and banned for 20 minutes");
			break;
		};
	};
};

// server side processing of updates: does very little and most state is tracked client only
// could be extended to move more gameplay to server (at expense of lag)

int tmp_pos[3];

void process(ENetPacket * packet, int sender)   // sender may be -1
{
    if(ENET_NET_TO_HOST_16(*(ushort *)packet->data)!=packet->dataLength)
    {
        disconnect_client(sender, "packet length");
        return;
    };
        
    uchar *end = packet->data+packet->dataLength;
    uchar *p = packet->data+2;
    char text[MAXTRANS];
    int cn = -1, type;

    while(p<end) switch(type = getint(p))
    {
        case SV_CDIS:  // EDIT: AH
        {
            int n = getint(p);
            if(m_ctf_s)
                loopi(2) if(ctfflags[i].state==CTFF_STOLEN && ctfflags[i].thief_cn==n)
                    send2(true, -1, SV_FLAGDROP, i);
            break;
        };
        case SV_TEXT:
            sgetstr();
            break;

        case SV_INITC2S:
            sgetstr();
            strcpy_s(clients[cn].name, text);
            sgetstr();
            getint(p);
            getint(p);
            break;

        case SV_MAPCHANGE:
        {
            sgetstr();
            int reqmode = getint(p);
            if(reqmode<0) reqmode = 0;
            if(smapname[0] && !mapreload && !vote(text, reqmode, sender)) return;
            mapreload = false;
            mode = reqmode;
            minremain = m_ctf_s ? 10 : (m_teammode_s ? 15 : 10);
            mapend = lastsec+minremain*60;
            interm = 0;
            strcpy_s(smapname, text);
            resetitems();
            sender = -1;
            laststatus = lastsec-61;
            ctfbroadcast = true;
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
            break;
        };

        case SV_ITEMPICKUP:
        {
            int n = getint(p);
            pickup(n, getint(p), sender);
            break;
        };
        
        case SV_BOTITEMPICKUP:
        {
            loopi(2) getint(p);
            break;
        };

        case SV_PING:
            send2(false, cn, SV_PONG, getint(p));
            break;

        case SV_POS:
        {
            cn = getint(p);
            if(cn<0 || cn>=clients.length() || clients[cn].type==ST_EMPTY)
            {
                disconnect_client(sender, "client num");
                return;
            };
            int size = msgsizelookup(type);
            assert(size!=-1);
            loopi(3) tmp_pos[i] = getint(p);
            loopi(size-5) getint(p);
            break;
        };

        case SV_SENDMAP:
        {
            sgetstr();
            int mapsize = getint(p);
            sendmaps(sender, text, mapsize, p);
            return;
        }

        case SV_RECVMAP:
			send(sender, recvmap(sender));
            return;
         
		
        // Added by Rick: Bot specifc messages
#ifndef STANDALONE
        case SV_ADDBOT:
            getint(p);
            sgetstr();
            //strcpy_s(clients[cn].name, text);
            sgetstr();
            getint(p);
            break;
        case SV_BOTCOMMAND: // Client asked server for a bot command
        {
            botcommand(p, text, sender);
            break;
        }
#endif
        // End add by Rick
		

        // EDIT: AH
        case SV_FLAGPICKUP:
        {
            int flag = getint(p);
            if(flag<0 || flag>1) return;
            ctfflag &f = ctfflags[flag];
            if(f.state!=CTFF_STOLEN)
            {
                f.state = CTFF_STOLEN;
                f.thief_cn = sender;
                sendflaginfo(flag, SV_FLAGPICKUP);
            };
            break;
        };
        
        case SV_FLAGDROP:
        {
            int flag = getint(p);
            if(flag<0|| flag>1) return;
            ctfflag &f = ctfflags[flag];
            if(f.state==CTFF_STOLEN && (sender==-1 || f.thief_cn==sender))
            {
                f.state = CTFF_DROPPED;
                f.lastupdate = lastsec;
                loopi(3) f.pos[i] = tmp_pos[i];
                sendflaginfo(flag, SV_FLAGDROP);
            };
            break;
        };
        
        case SV_FLAGRETURN:
        {
            int flag = getint(p);
            if(flag<0|| flag>1) return;
            ctfflag &f = ctfflags[flag];
            if(f.state==CTFF_DROPPED)
            {
                f.state = CTFF_INBASE;
                f.thief_cn = sender;
                sendflaginfo(flag, SV_FLAGRETURN);
            };
            break;
        };
        
        case SV_FLAGSCORE:
        {
            int flag = getint(p);
            if(flag<0|| flag>1) return;
            ctfflag &f = ctfflags[flag];
            if(f.state==CTFF_STOLEN)
            {
                f.state = CTFF_INBASE;
                f.thief_cn = sender;
                sendflaginfo(flag, SV_FLAGSCORE);
            };
            break;
        };
    
		case SV_GETMASTER:
		{
			sgetstr();
			getmaster(sender, text);
			break;
		};

		case SV_MASTERCMD:
		{
			mastercmd(sender, getint(p), getint(p));
			break;
		};

        default:
        {
            int size = msgsizelookup(type);
            if(size==-1) { disconnect_client(sender, "tag type"); return; };
            loopi(size-1) getint(p);
        };
    };

    if(p>end) { disconnect_client(sender, "end of packet"); return; };
    multicast(packet, sender);
};

void send_welcome(int n)
{
    ENetPacket * packet = enet_packet_create (NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start+2;
    putint(p, SV_INITS2C);
    putint(p, n);
    putint(p, PROTOCOL_VERSION);
    if(!smapname[0] && configsets.length()) nextcfgset(); // EDIT:AH
    putint(p, smapname[0]);
    sendstring(serverpassword, p);
    putint(p, numclients()>maxclients);
    if(smapname[0])
    {
        putint(p, SV_MAPCHANGE);
        sendstring(smapname, p);
        putint(p, mode);
        putint(p, SV_ITEMLIST);
        loopv(sents) if(sents[i].spawned) putint(p, i);
        putint(p, -1);
    };
    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
    enet_packet_resize(packet, p-start);
    send(n, packet);
    if(smapname[0] && m_ctf_s) loopi(2) sendflaginfo(i, -1, n); // EDIT: AH
};

void multicast(ENetPacket *packet, int sender)
{
    loopv(clients)
    {
        if(i==sender) continue;
        send(i, packet);
    };
};

void localclienttoserver(ENetPacket *packet)
{
    process(packet, 0);
    if(!packet->referenceCount) enet_packet_destroy (packet);
};

client &addclient()
{
    loopv(clients) if(clients[i].type==ST_EMPTY) return clients[i];
    return clients.add();
};

void checkintermission()
{
    if(!minremain)
    {
        interm = lastsec+10;
        mapend = lastsec+1000;
    };
    send2(true, -1, SV_TIMEUP, minremain--);
};

/*void startintermission() { minremain = 0; checkintermission(); };*/

void resetserverifempty()
{
    loopv(clients) if(clients[i].type!=ST_EMPTY) return;
    clients.setsize(0);
    smapname[0] = 0;
    resetvotes();
#ifndef STANDALONE
	resetbotvotes(); // Added by Rick
#endif
    resetitems();
    mode = 0;
    mapreload = false;
    minremain = 10;
    mapend = lastsec+minremain*60;
    interm = 0;
    ctfreset(false);
};

int nonlocalclients = 0;
int lastconnect = 0;

void serverslice(int seconds, unsigned int timeout)   // main server update, called from cube main loop in sp, or dedicated server loop
{
    loopv(sents)        // spawn entities when timer reached
    {
        if(sents[i].spawnsecs && (sents[i].spawnsecs -= seconds-lastsec)<=0)
        {
            sents[i].spawnsecs = 0;
            sents[i].spawned = true;
            send2(true, -1, SV_ITEMSPAWN, i);
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
               sprintf_sd(msg)("the server reset the %s flag", rb_team_string(i));
               sendservmsg(msg);
               send2(true, -1, SV_SOUND, S_FLAGDROP);
            };
        };
    };
    
    lastsec = seconds;
    
	if((mode>1 || (mode==0 && nonlocalclients)) && seconds>mapend-minremain*60) checkintermission();
    if(interm && seconds>interm)
    {
        interm = 0;

        if(configsets.length())  // EDIT: AH
        {
            nextcfgset();
            mapreload = true;
            changemap();
            ctfbroadcast = true;
        }
        else loopv(clients) if(clients[i].type!=ST_EMPTY)
        {
            send2(true, i, SV_MAPRELOAD, 0);    // ask a client to trigger map reload
            mapreload = true;
            break;
        };
    };

    if(ctfbroadcast)
    {
        ctfreset();
        ctfbroadcast = false;
    };

    resetserverifempty();
    
    if(!isdedicated) return;     // below is network only

	int numplayers = numclients();
	serverms(mode, numplayers, minremain, smapname, seconds, numclients()>=maxclients);

    if(seconds-laststatus>60)   // display bandwidth stats, useful for server ops
    {
        nonlocalclients = 0;
        loopv(clients) if(clients[i].type==ST_TCPIP) nonlocalclients++;
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
                c.peer->data = (void *)(&c-&clients[0]);
                char hn[1024];
                strcpy_s(c.hostname, (enet_address_get_host(&c.peer->address, hn, sizeof(hn))==0) ? hn : "localhost");
                printf("client connected (%s)\n", c.hostname);
                send_welcome(lastconnect = &c-&clients[0]);
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE:
                brec += event.packet->dataLength;
                process(event.packet, (int)event.peer->data); 
                if(event.packet->referenceCount==0) enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT: 
                if((int)event.peer->data<0) break;
                printf("disconnected client (%s)\n", clients[(int)event.peer->data].hostname);
                clients[(int)event.peer->data].type = ST_EMPTY;
                send2(true, -1, SV_CDIS, (int)event.peer->data);
                event.peer->data = (void *)-1;
                break;
        };
        
        if(numplayers>maxclients)   
        {
            disconnect_client(lastconnect, "maxclients reached");
        };
    };
};

void cleanupserver()
{
    if(serverhost) enet_host_destroy(serverhost);
};

void localdisconnect()
{
    loopv(clients) if(clients[i].type==ST_LOCAL) clients[i].type = ST_EMPTY;
};

void localconnect()
{
    client &c = addclient();
    c.type = ST_LOCAL;
    strcpy_s(c.hostname, "local");
    send_welcome(&c-&clients[0]); 
};

void initserver(bool dedicated, int uprate, char *sdesc, char *ip, char *master, char *passwd, int maxcl, char *maprot, char *masterpwd) // EDIT: AH
{
    serverpassword = passwd;
    maxclients = maxcl;
	servermsinit(master ? master : "cubebot.bots-united.com/cube/masterserver.cgi/", sdesc, dedicated);
    
    if(isdedicated = dedicated)
    {
        ENetAddress address = { ENET_HOST_ANY, CUBE_SERVER_PORT };
        if(*ip && enet_address_set_host(&address, ip)<0) printf("WARNING: server ip not resolved");
        serverhost = enet_host_create(&address, MAXCLIENTS, 0, uprate);
        if(!serverhost) fatal("could not create server host\n");
        loopi(MAXCLIENTS) serverhost->peers[i].data = (void *)-1;
		if(!maprot || !maprot[0]) maprot = newstring("config/maprot.cfg");
        readscfg(path(maprot)); // EDIT: AH
		if(!masterpwd || !masterpwd[0]) masterpasswd = masterpwd;
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
