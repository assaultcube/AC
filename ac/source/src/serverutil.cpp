// misc useful functions used by the server

#include "cube.h"

// all network traffic is in 32bit ints, which are then compressed using the following simple scheme (assumes that most values are small).

void putint(uchar *&p, int n)
{
    if(n<128 && n>-127) { *p++ = n; }
    else if(n<0x8000 && n>=-0x8000) { *p++ = 0x80; *p++ = n; *p++ = n>>8;  }
    else { *p++ = 0x81; *p++ = n; *p++ = n>>8; *p++ = n>>16; *p++ = n>>24; };
};

int getint(uchar *&p)
{
    int c = *((char *)p);
    p++;
    if(c==-128) { int n = *p++; n |= *((char *)p)<<8; p++; return n;}
    else if(c==-127) { int n = *p++; n |= *p++<<8; n |= *p++<<16; return n|(*p++<<24); } 
    else return c;
};

void sendstring(char *t, uchar *&p)
{
    while(*t) putint(p, *t++);
    putint(p, 0);
};

const char *modenames[] =
{
    "team deathmatch", "coopedit", "deathmatch", "survior",
    "team survior", "ctf", "pistols only!", "botmatch",
};
      
//const char *modestr(int n) { return (n>=-2 && n<12) ? modenames[n+2] : "unknown"; };
const char *modestr(int n) { return (n>=0 && n<=7) ? modenames[n] : "unknown"; };

/* Modified by Rick: New messages and different sizes for existing ones
char msgsizesl[] =               // size inclusive message token, 0 for variable or not-checked sizes
{ 
    SV_INITS2C, 4, SV_INITC2S, 0, SV_POS, 12, SV_TEXT, 0, SV_SOUND, 2, SV_CDIS, 2,
    SV_DIED, 2, SV_DAMAGE, 4, SV_SHOsT, 8, SV_FRAGS, 2,
    SV_TIMEUP, 2, SV_EDITENT, 10, SV_MAPRELOAD, 2, SV_ITEMACC, 2,
    SV_MAPCHANGE, 0, SV_ITEMSPAWN, 2, SV_ITEMPICKUP, 3, SV_DENIED, 2,
    SV_PING, 2, SV_PONG, 2, SV_CLIENTPING, 2, SV_GAMEMODE, 2,
    SV_EDITH, 7, SV_EDITT, 7, SV_EDITS, 6, SV_EDITD, 6, SV_EDITE, 6,
    SV_SENDMAP, 0, SV_RECVMAP, 1, SV_SERVMSG, 0, SV_ITEMLIST, 0,
    SV_EXT, 0,
    -1
};
*/

char msgsizesl[] =               // size inclusive message token, 0 for variable or not-checked sizes
{ 
    SV_INITS2C, 4, SV_INITC2S, 0, SV_POS, 12, SV_TEXT, 0, SV_SOUND, 2, SV_CDIS, 2,
    SV_DIED, 2, SV_DAMAGE, 4, SV_SHOT, 9, SV_FRAGS, 2,
    SV_TIMEUP, 2, SV_EDITENT, 10, SV_MAPRELOAD, 2, SV_ITEMACC, 2,
    SV_MAPCHANGE, 0, SV_ITEMSPAWN, 2, SV_ITEMPICKUP, 3, SV_DENIED, 2,
    SV_PING, 2, SV_PONG, 2, SV_CLIENTPING, 2, SV_GAMEMODE, 2,
    SV_EDITH, 7, SV_EDITT, 7, SV_EDITS, 6, SV_EDITD, 6, SV_EDITE, 6,
    SV_SENDMAP, 0, SV_RECVMAP, 1, SV_SERVMSG, 0, SV_ITEMLIST, 0, SV_WEAPCHANGE, 2,
    SV_MODELSKIN, 2,
    SV_FLAGPICKUP, 2, SV_FLAGDROP, 2, SV_FLAGRETURN, 2, SV_FLAGSCORE, 2, SV_FLAGINFO, 0, SV_FLAGS, 2, // EDIT: AH
    // Added by Rick: Bot specific messages
    SV_BOTSOUND, 3, SV_BOTDIS, 2, SV_BOTDIED, 4, SV_CLIENT2BOTDMG, 4, SV_BOT2BOTDMG, 2,
    SV_BOTFRAGS, 3, SV_ADDBOT, 5, SV_BOTUPDATE, 12, SV_BOTCOMMAND, 0,
    // End add
    SV_BOTITEMPICKUP, 3, SV_BOT2CLIENTDMG, 5, SV_DIEDBYBOT, 2,
    SV_EXT, 0,
    -1
};

char msgsizelookup(int msg)
{
    for(char *p = msgsizesl; *p>=0; p += 2) if(*p==msg) return p[1];
    return -1;
};

// sending of maps between clients

string copyname;
int copysize;
uchar *copydata = NULL;

void sendmaps(int n, string mapname, int mapsize, uchar *mapdata)
{
    if(mapsize <= 0 || mapsize > 256*256) return;
    strcpy_s(copyname, mapname);
    copysize = mapsize;
    if(copydata) free(copydata);
    copydata = (uchar *)alloc(mapsize);
    memcpy(copydata, mapdata, mapsize);
}

ENetPacket *recvmap(int n)
{
    if(!copydata) return NULL;
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS + copysize, ENET_PACKET_FLAG_RELIABLE);
    uchar *start = packet->data;
    uchar *p = start+2;
    putint(p, SV_RECVMAP);
    sendstring(copyname, p);
    putint(p, copysize);
    memcpy(p, copydata, copysize);
    p += copysize;
    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
    enet_packet_resize(packet, p-start);
	return packet;
}

#ifdef STANDALONE

void localservertoclient(uchar *buf, int len) {};
void fatal(char *s, char *o) { cleanupserver(); printf("servererror: %s\n", s); exit(1); };
void *alloc(int s) { void *b = calloc(1,s); if(!b) fatal("no memory!"); return b; };

int main(int argc, char* argv[]) // EDIT: AH
{
    int uprate = 0, maxcl = 4;
    char *sdesc = "", *ip = "", *master = NULL, *passwd = "", *maprot = "";
    
    for(int i = 1; i<argc; i++)
    {
        char *a = &argv[i][2];
        if(argv[i][0]=='-') switch(argv[i][1])
        {
            case 'u': uprate = atoi(a); break;
            case 'n': sdesc  = a; break;
            case 'i': ip     = a; break;
            case 'm': master = a; break;
            case 'p': passwd = a; break;
            case 'c': maxcl  = atoi(a); break;
            case 'r': maprot = a; break; // EDIT: AH
            default: printf("WARNING: unknown commandline option\n");
        };
    };
    
    if(enet_initialize()<0) fatal("Unable to initialise network module");
    initserver(true, uprate, sdesc, ip, master, passwd, maxcl, maprot);
    return 0;
};
#endif



