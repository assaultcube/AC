// misc useful functions used by the server

#include "cube.h"

// all network traffic is in 32bit ints, which are then compressed using the following simple scheme (assumes that most values are small).

void putint(ucharbuf &p, int n)
{
    if(n<128 && n>-127) p.put(n);
    else if(n<0x8000 && n>=-0x8000) { p.put(0x80); p.put(n); p.put(n>>8); }
    else { p.put(0x81); p.put(n); p.put(n>>8); p.put(n>>16); p.put(n>>24); };
};

int getint(ucharbuf &p)
{
    int c = (char)p.get();
    if(c==-128) { int n = p.get(); n |= char(p.get())<<8; return n; }
    else if(c==-127) { int n = p.get(); n |= p.get()<<8; n |= p.get()<<16; return n|(p.get()<<24); }
    else return c;
};

// much smaller encoding for unsigned integers up to 28 bits, but can handle signed
void putuint(ucharbuf &p, int n)
{
    if(n < 0 || n >= (1<<21))
    {
        p.put(0x80 | (n & 0x7F));
        p.put(0x80 | ((n >> 7) & 0x7F));
        p.put(0x80 | ((n >> 14) & 0x7F));
        p.put(n >> 21);
    }
    else if(n < (1<<7)) p.put(n);
    else if(n < (1<<14))
    {
        p.put(0x80 | (n & 0x7F));
        p.put(n >> 7);
    }
    else 
    {
        p.put(0x80 | (n & 0x7F));
        p.put(0x80 | ((n >> 7) & 0x7F));
        p.put(n >> 14);
    };
};

int getuint(ucharbuf &p)
{
    int n = p.get();
    if(n & 0x80)
    {
        n += (p.get() << 7) - 0x80;
        if(n & (1<<14)) n += (p.get() << 14) - (1<<14);
        if(n & (1<<21)) n += (p.get() << 21) - (1<<21);
        if(n & (1<<28)) n |= 0xF0000000;
    };
    return n;
};

void sendstring(const char *t, ucharbuf &p)
{
    while(*t) putint(p, *t++);
    putint(p, 0);
};

void getstring(char *text, ucharbuf &p, int len)
{
    char *t = text;
    do
    {
        if(t>=&text[len]) { text[len-1] = 0; return; };
        if(!p.remaining()) { *t = 0; return; };
        *t = getint(p);
    }
    while(*t++);
};

const char *modenames[] =
{
    "team deathmatch", "coopedit", "deathmatch", "survior",
    "team survior", "ctf", "pistols frenzy", "bot team deathmatch", "bot deathmatch", "last swiss standing", 
    "one shot, one kill", "team one shot, one kill"
};

const char *modestr(int n) { return (n>=0 && (size_t)n < sizeof(modenames)/sizeof(modenames[0])) ? modenames[n] : "unknown"; };

char msgsizesl[] =               // size inclusive message token, 0 for variable or not-checked sizes
{ 
    SV_INITS2C, 4, SV_INITC2S, 0, SV_POS, 0, SV_TEXT, 0, SV_SOUND, 2, SV_CDIS, 2,
    SV_GIBDIED, 2, SV_DIED, 2, SV_GIBDAMAGE, 4, SV_DAMAGE, 4, SV_SHOT, 9, SV_FRAGS, 2, SV_RESUME, 3,
    SV_TIMEUP, 2, SV_EDITENT, 10, SV_MAPRELOAD, 2, SV_ITEMACC, 2,
    SV_MAPCHANGE, 0, SV_ITEMSPAWN, 2, SV_ITEMPICKUP, 3, SV_DENIED, 2,
    SV_PING, 2, SV_PONG, 2, SV_CLIENTPING, 2, SV_GAMEMODE, 2,
    SV_EDITH, 7, SV_EDITT, 7, SV_EDITS, 6, SV_EDITD, 6, SV_EDITE, 6,
    SV_SENDMAP, 0, SV_RECVMAP, 1, SV_SERVMSG, 0, SV_ITEMLIST, 0, SV_WEAPCHANGE, 2,
    SV_MODELSKIN, 2,
    SV_FLAGPICKUP, 2, SV_FLAGDROP, 2, SV_FLAGRETURN, 2, SV_FLAGSCORE, 2, SV_FLAGINFO, 0, SV_FLAGS, 2, // EDIT: AH
	SV_GETMASTER, 0, SV_MASTERCMD, 3,
	SV_PWD, 0,
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
    s_strcpy(copyname, mapname);
    copysize = mapsize;
    DELETEA(copydata);
    copydata = new uchar[mapsize];
    memcpy(copydata, mapdata, mapsize);
}

ENetPacket *recvmap(int n)
{
    if(!copydata) return NULL;
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS + copysize, ENET_PACKET_FLAG_RELIABLE);
    ucharbuf p(packet->data, packet->dataLength);
    putint(p, SV_RECVMAP);
    sendstring(copyname, p);
    putint(p, copysize);
    p.put(copydata, copysize);
    enet_packet_resize(packet, p.length());
	return packet;
}

#ifdef STANDALONE

void localservertoclient(int chan, uchar *buf, int len) {};
void fatal(char *s, char *o) { cleanupserver(); printf("servererror: %s\n", s); exit(1); };

int main(int argc, char* argv[]) // EDIT: AH
{
    int uprate = 0, maxcl = DEFAULTCLIENTS;
    char *sdesc = "", *ip = "", *master = NULL, *passwd = "", *maprot = "", *masterpwd = NULL;
    
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
            case 'r': maprot = a; break; 
			case 'x' : masterpwd = a; break; // EDIT: AH
            default: printf("WARNING: unknown commandline option\n");
        };
    };
    
    if(enet_initialize()<0) fatal("Unable to initialise network module");
    initserver(true, uprate, sdesc, ip, master, passwd, maxcl, maprot, masterpwd);
    return 0;
};
#endif



