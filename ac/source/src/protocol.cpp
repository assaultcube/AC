// misc useful functions used by the server

#include "cube.h"

// all network traffic is in 32bit ints, which are then compressed using the following simple scheme (assumes that most values are small).

void putint(ucharbuf &p, int n)
{
    if(n<128 && n>-127) p.put(n);
    else if(n<0x8000 && n>=-0x8000) { p.put(0x80); p.put(n); p.put(n>>8); }
    else { p.put(0x81); p.put(n); p.put(n>>8); p.put(n>>16); p.put(n>>24); }
}

int getint(ucharbuf &p)
{
    int c = (char)p.get();
    if(c==-128) { int n = p.get(); n |= char(p.get())<<8; return n; }
    else if(c==-127) { int n = p.get(); n |= p.get()<<8; n |= p.get()<<16; return n|(p.get()<<24); }
    else return c;
}

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
    }
}

int getuint(ucharbuf &p)
{
    int n = p.get();
    if(n & 0x80)
    {
        n += (p.get() << 7) - 0x80;
        if(n & (1<<14)) n += (p.get() << 14) - (1<<14);
        if(n & (1<<21)) n += (p.get() << 21) - (1<<21);
        if(n & (1<<28)) n |= 0xF0000000;
    }
    return n;
}

void sendstring(const char *t, ucharbuf &p)
{
    while(*t) putint(p, *t++);
    putint(p, 0);
}

void getstring(char *text, ucharbuf &p, int len)
{
    char *t = text;
    do
    {
        if(t>=&text[len]) { text[len-1] = 0; return; }
        if(!p.remaining()) { *t = 0; return; }
        *t = getint(p);
    }
    while(*t++);
}


void filtertext(char *dst, const char *src, bool whitespace, int len)
{
    for(int c = *src; c; c = *++src)
    {
        switch(c)
        {
        case '\f': ++src; continue;
        }
        if(isspace(c) ? whitespace : isprint(c))
        {
            *dst++ = c;
            if(!--len) break;
        }
    }
    *dst = '\0';
}

const char *modenames[] =
{
    "team deathmatch", "coopedit", "deathmatch", "survivor",
    "team survivor", "ctf", "pistol frenzy", "bot team deathmatch", "bot deathmatch", "last swiss standing", 
    "one shot, one kill", "team one shot, one kill", "bot one shot, one skill"
};

const char *modestr(int n) { return (n>=0 && (size_t)n < sizeof(modenames)/sizeof(modenames[0])) ? modenames[n] : "unknown"; }

char msgsizesl[] =               // size inclusive message token, 0 for variable or not-checked sizes
{ 
    SV_INITS2C, 5, SV_INITC2S, 0, SV_POS, 0, SV_TEXT, 0, SV_TEAMTEXT, 0, SV_SOUND, 2, SV_CDIS, 2,
    SV_SHOOT, 0, SV_EXPLODE, 0, SV_SUICIDE, 1, SV_AKIMBO, 2, SV_RELOAD, 3,
    SV_GIBDIED, 4, SV_DIED, 4, SV_GIBDAMAGE, 6, SV_DAMAGE, 6, SV_HITPUSH, 6, SV_SHOTFX, 9, SV_THROWNADE, 8,
    SV_TRYSPAWN, 1, SV_SPAWNSTATE, 20, SV_SPAWN, 3, SV_FORCEDEATH, 2, SV_RESUME, 0,
    SV_TIMEUP, 2, SV_EDITENT, 10, SV_MAPRELOAD, 2, SV_ITEMACC, 2,
    SV_MAPCHANGE, 0, SV_ITEMSPAWN, 2, SV_ITEMPICKUP, 2,
    SV_PING, 2, SV_PONG, 2, SV_CLIENTPING, 2, SV_GAMEMODE, 2,
    SV_EDITMODE, 2, SV_EDITH, 7, SV_EDITT, 7, SV_EDITS, 6, SV_EDITD, 6, SV_EDITE, 6,
    SV_SENDMAP, 0, SV_RECVMAP, 1, SV_SERVMSG, 0, SV_ITEMLIST, 0, SV_WEAPCHANGE, 2, SV_PRIMARYWEAP, 2,
    SV_MODELSKIN, 2,
    SV_FLAGPICKUP, 2, SV_FLAGDROP, 2, SV_FLAGRETURN, 2, SV_FLAGSCORE, 2, SV_FLAGRESET, 2, SV_FLAGINFO, 0, SV_FLAGS, 2,
    SV_ARENAWIN, 2,
	SV_SETMASTER, 2, SV_SETADMIN, 0, SV_SERVOPINFO, 3, 
    SV_CALLVOTE, 0, SV_CALLVOTESUC, 1, SV_CALLVOTEERR, 2, SV_VOTE, 2, SV_VOTERESULT, 2,
	SV_FORCETEAM, 2, SV_AUTOTEAM, 2,
    SV_WHOIS, 2, SV_WHOISINFO, 3,
	SV_CONNECT, 0,
    SV_CLIENT, 0,
    -1
};

char msgsizelookup(int msg)
{
    for(char *p = msgsizesl; *p>=0; p += 2) if(*p==msg) return p[1];
    return -1;
}

