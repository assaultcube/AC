// misc useful functions used by the server

#include "pch.h"
#include "cube.h"
#include "hash.h"

#ifdef _DEBUG
bool protocoldbg = false;
void protocoldebug(bool enable) { protocoldbg = enable; }
#define DEBUGCOND (protocoldbg)
#endif

// all network traffic is in 32bit ints, which are then compressed using the following simple scheme (assumes that most values are small).

void putint(ucharbuf &p, int n)
{
    DEBUGVAR(n);
    if(n<128 && n>-127) p.put(n);
    else if(n<0x8000 && n>=-0x8000) { p.put(0x80); p.put(n); p.put(n>>8); }
    else { p.put(0x81); p.put(n); p.put(n>>8); p.put(n>>16); p.put(n>>24); }
}

int getint(ucharbuf &p)
{
    int c = (char)p.get();
    if(c==-128) { int n = p.get(); n |= char(p.get())<<8; DEBUGVAR(n); return n; }
    else if(c==-127) { int n = p.get(); n |= p.get()<<8; n |= p.get()<<16; n |= (p.get()<<24); DEBUGVAR(n); return n; }
    else
    {
        DEBUGVAR(c);
        return c;
    }
}

// much smaller encoding for unsigned integers up to 28 bits, but can handle signed
void putuint(ucharbuf &p, int n)
{
    DEBUGVAR(n);
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
    DEBUGVAR(n);
    return n;
}

void sendstring(const char *text, ucharbuf &p)
{
    const char *t = text;
    while(*t) putint(p, *t++);
    putint(p, 0);
    DEBUGVAR(text);
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
    DEBUGVAR(text);
}


void filtertext(char *dst, const char *src, int whitespace, int len)
{
    for(int c = *src; c; c = *++src)
    {
        c &= 0x7F; // 7-bit ascii
        switch(c)
        {
        case '\f': ++src; continue;
        }
        if(isspace(c) ? whitespace && (whitespace>1 || c == ' ') : isprint(c))
        {
            *dst++ = c;
            if(!--len) break;
        }
    }
    *dst = '\0';
}

const char *modefullnames[] =
{
    "team deathmatch", "coopedit", "deathmatch", "survivor",
    "team survivor", "ctf", "pistol frenzy", "bot team deathmatch", "bot deathmatch", "last swiss standing",
    "one shot, one kill", "team one shot, one kill", "bot one shot, one kill", "hunt the flag", "team keep the flag", "keep the flag"
};

const char *modeacronymnames[] =
{
    "TDM", "coop", "DM", "SURV", "TSURV", "CTF", "PF", "BTDM", "BDM", "LSS",
    "OSOK", "TOSOK", "BOSOK", "HTF", "TKTF", "KTF"
};

const char *voteerrors[] = { "voting is currently disabled", "there is already a vote pending", "already voted", "can't vote that often", "this vote is not allowed in the current environment (singleplayer/multiplayer)", "no permission", "invalid vote" };

const char *fullmodestr(int n) { return (n>=0 && (size_t)n < sizeof(modefullnames)/sizeof(modefullnames[0])) ? modefullnames[n] : "unknown"; }
const char *acronymmodestr(int n) { return (n>=0 && (size_t)n < sizeof(modeacronymnames)/sizeof(modeacronymnames[0])) ? modeacronymnames[n] : "n/a"; }
const char *modestr(int n, bool acronyms) { return acronyms ? acronymmodestr (n) : fullmodestr(n); }
const char *voteerrorstr(int n) { return (n>=0 && (size_t)n < sizeof(voteerrors)/sizeof(voteerrors[0])) ? voteerrors[n] : "unknown"; }

char msgsizesl[] =               // size inclusive message token, 0 for variable or not-checked sizes
{
    SV_INITS2C, 5, SV_WELCOME, 2, SV_INITC2S, 0, SV_POS, 0, SV_TEXT, 0, SV_TEAMTEXT, 0, SV_SOUND, 2, SV_VOICECOM, 2, SV_VOICECOMTEAM, 2, SV_CDIS, 2,
    SV_SHOOT, 0, SV_EXPLODE, 0, SV_SUICIDE, 1, SV_AKIMBO, 2, SV_RELOAD, 3,
    SV_GIBDIED, 4, SV_DIED, 4, SV_GIBDAMAGE, 6, SV_DAMAGE, 6, SV_HITPUSH, 6, SV_SHOTFX, 9, SV_THROWNADE, 8,
    SV_TRYSPAWN, 1, SV_SPAWNSTATE, 23, SV_SPAWN, 3, SV_FORCEDEATH, 2, SV_RESUME, 0,
    SV_TIMEUP, 2, SV_EDITENT, 10, SV_MAPRELOAD, 2, SV_NEXTMAP, 0, SV_ITEMACC, 2,
    SV_MAPCHANGE, 0, SV_ITEMSPAWN, 2, SV_ITEMPICKUP, 2,
    SV_PING, 2, SV_PONG, 2, SV_CLIENTPING, 2, SV_GAMEMODE, 2,
    SV_EDITMODE, 2, SV_EDITH, 7, SV_EDITT, 7, SV_EDITS, 6, SV_EDITD, 6, SV_EDITE, 6, SV_NEWMAP, 2,
    SV_SENDMAP, 0, SV_RECVMAP, 1, SV_SERVMSG, 0, SV_ITEMLIST, 0, SV_WEAPCHANGE, 2, SV_PRIMARYWEAP, 2,
    SV_MODELSKIN, 2,
    SV_FLAGACTION, 3, SV_FLAGINFO, 0, SV_FLAGMSG, 0, SV_FLAGCNT, 3,
    SV_ARENAWIN, 2,
    SV_SETADMIN, 0, SV_SERVOPINFO, 3,
    SV_CALLVOTE, 0, SV_CALLVOTESUC, 1, SV_CALLVOTEERR, 2, SV_VOTE, 2, SV_VOTERESULT, 2,
    SV_FORCETEAM, 3, SV_AUTOTEAM, 2, SV_CHANGETEAM, 1,
    SV_WHOIS, 2, SV_WHOISINFO, 3,
    SV_LISTDEMOS, 1, SV_SENDDEMOLIST, 0, SV_GETDEMO, 2, SV_SENDDEMO, 0, SV_DEMOPLAYBACK, 3,
    SV_CONNECT, 0,
    SV_CLIENT, 0,
    SV_EXTENSION, 0,
    SV_SPAWNLIST, 0, SV_FORCENOTIFY, 3,
    -1
};

char msgsizelookup(int msg)
{
    for(char *p = msgsizesl; *p>=0; p += 2) if(*p==msg) return p[1];
    return -1;
}

const char *genpwdhash(const char *name, const char *pwd, int salt)
{
    static string temp;
    s_sprintf(temp)("%s %d %s %s %d", pwd, salt, name, pwd, PROTOCOL_VERSION);
    tiger::hashval hash;
    tiger::hash((uchar *)temp, strlen(temp), hash);
    s_sprintf(temp)("%llx %llx %llx", hash.chunks[0], hash.chunks[1], hash.chunks[2]);
    return temp;
}


