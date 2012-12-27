// misc useful functions used by the server

#include "cube.h"

#ifdef _DEBUG
bool protocoldbg = false;
void protocoldebug(bool enable) { protocoldbg = enable; }
#define DEBUGCOND (protocoldbg)
#endif

// all network traffic is in 32bit ints, which are then compressed using the following simple scheme (assumes that most values are small).

template<class T>
static inline void putint_(T &p, int n)
{
    DEBUGVAR(n);
    if(n<128 && n>-127) p.put(n);
    else if(n<0x8000 && n>=-0x8000) { p.put(0x80); p.put(n); p.put(n>>8); }
    else { p.put(0x81); p.put(n); p.put(n>>8); p.put(n>>16); p.put(n>>24); }
}
void putint(ucharbuf &p, int n) { putint_(p, n); }
void putint(packetbuf &p, int n) { putint_(p, n); }
void putint(vector<uchar> &p, int n) { putint_(p, n); }

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
template<class T>
static inline void putuint_(T &p, int n)
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
void putuint(ucharbuf &p, int n) { putuint_(p, n); }
void putuint(packetbuf &p, int n) { putuint_(p, n); }
void putuint(vector<uchar> &p, int n) { putuint_(p, n); }

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

template<class T>
static inline void putfloat_(T &p, float f)
{
    lilswap(&f, 1);
    p.put((uchar *)&f, sizeof(float));
}
void putfloat(ucharbuf &p, float f) { putfloat_(p, f); }
void putfloat(packetbuf &p, float f) { putfloat_(p, f); }
void putfloat(vector<uchar> &p, float f) { putfloat_(p, f); }

float getfloat(ucharbuf &p)
{
    float f;
    p.get((uchar *)&f, sizeof(float));
    return lilswap(f);
}

template<class T>
static inline void sendstring_(const char *text, T &p)
{
    const char *t = text;
    if(t) { while(*t) putint(p, *t++); }
    putint(p, 0);
    DEBUGVAR(text);
}
void sendstring(const char *t, ucharbuf &p) { sendstring_(t, p); }
void sendstring(const char *t, packetbuf &p) { sendstring_(t, p); }
void sendstring(const char *t, vector<uchar> &p) { sendstring_(t, p); }

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

#define follower(x) (((x) & 0xc0) == 0x80)

int getutf8char(const uchar *&s)
{
    int res = *s++;
    if(res < 0x80) return res;
    if((res & 0xe0) == 0xc0)
    { // 2-Byte
        if(follower(s[0]))
        {
            res &= 0x1f;
            res <<= 6;
            res |= *s++ & 0x3f;
            return res;
        }
        return -1;
    }
    if((res & 0xf0) == 0xe0)
    { // 3-Byte
        if(follower(s[0]) && follower(s[1]))
        {
            res &= 0x0f;
            loopi(2)
            {
                res <<= 6;
                res |= *s++ & 0x3f;
            }
            return res;
        }
        return -1;
    }
    if((res & 0xf8) == 0xf0)
    { // 4-Byte
        if(follower(s[0]) && follower(s[1]) && follower(s[2]))
        {
            res &= 0x07;
            loopi(3)
            {
                res <<= 6;
                res |= *s++ & 0x3f;
            }
            return res;
        }
        return -1;
    }
    return -1;
}

int pututf8char(uchar *&d, int s)
{
    if(s < 0 || s > 0x1fffff) return 0;
    if(s < 0x80)
    {
        *d++ = s;
        return 1;
    }
    if(s < 0x800)
    {
        *d++ = ((s >> 6) & 0x1f) | 0xc0;
        *d++ = (s & 0x3f) | 0x80;
        return 2;
    }
    if(s < 0x10000)
    {
        *d++ = ((s >> 12) & 0x0f) | 0xe0;
        *d++ = ((s >> 6) & 0x3f) | 0x80;
        *d++ = (s & 0x3f) | 0x80;
        return 3;
    }
    *d++ = ((s >> 18) & 0x07) | 0xf0;
    *d++ = ((s >> 12) & 0x3f) | 0x80;
    *d++ = ((s >> 6) & 0x3f) | 0x80;
    *d++ = (s & 0x3f) | 0x80;
    return 4;
}

void filtertext(char *dst, const char *src, int whitespace, int len)
{ // whitespace: no whitespace at all (0), blanks only (1), blanks & newline (2), spaces for underlines (-1)
    for(int c = *src; c; c = *++src)
    {
        c &= 0x7F; // 7-bit ascii
        if(c == '\f')
        {
            if(!*++src) break;
            continue;
        }
        if(isspace(c) ? whitespace && (whitespace>1 || c == ' '): isprint(c))
        {
            if ( whitespace < 0 && c == ' ' ) c = '_';
            *dst++ = c;
            if(!--len) break;
        }
    }
    *dst = '\0';
}

void filterrichtext(char *dst, const char *src, int len)
{
    int b, c;
    unsigned long ul;
    for(c = *src; c; c = *++src)
    {
        c &= 0x7F; // 7-bit ascii
        if(c == '\\')
        {
            b = 0;
            c = *++src;
            switch(c)
            {
                case '\0': --src; continue;
                case 'f': c = '\f'; break;
                case 'n': c = '\n'; break;
                case 'x':
                    b = 16;
                    c = *++src;
                default:
                    if(isspace(c)) continue;
                    if(b == 0 && !isdigit(c)) break;
                    ul = strtoul(src, (char **) &src, b);
                    --src;
                    c = (int) ul;
                    if(!c) continue; // number conversion failed
                    break;
            }
        }
        *dst++ = c;
        if(!--len || !*src) break;
    }
    *dst = '\0';
}

void filterservdesc(char *dst, const char *src, int len)
{ // only colors and spaces allowed
    for(int c = *src; c; c = *++src)
    {
        c &= 0x7F; // 7-bit ascii
        if((!isspace(c) && isprint(c)) || c == ' ' || c == '\f')
        {
            *dst++ = c;
            if(!--len) break;
        }
    }
    *dst = '\0';
}

void filterlang(char *d, const char *s)
{
    if(strlen(s) == 2)
    {
        loopi(2) d[i] = tolower(s[i]);
        d[2] = '\0';
        if(islower(d[0]) && islower(d[1])) return;
    }
    *d = '\0';
}

void trimtrailingwhitespace(char *s)
{
    for(int n = (int)strlen(s) - 1; n >= 0 && isspace(s[n]); n--)
        s[n] = '\0';
}

void cutcolorstring(char *text, int len)
{ // limit string length, ignore color codes
    while(*text)
    {
        if(*text == '\f' && text[1]) text++;
        else len--;
        if(len < 0) { *text = '\0'; break; }
        text++;
    }
}

const char *modefullnames[] =
{
    "demo playback",
    "team deathmatch", "coopedit", "deathmatch", "survivor",
    "team survivor", "ctf", "pistol frenzy", "bot team deathmatch", "bot deathmatch", "last swiss standing",
    "one shot, one kill", "team one shot, one kill", "bot one shot, one kill", "hunt the flag", "team keep the flag",
    "keep the flag", "team pistol frenzy", "team last swiss standing", "bot pistol frenzy", "bot last swiss standing", "bot team survivor", "bot team one shot, one kill"
};

const char *modeacronymnames[] =
{
    "DEMO",
    "TDM", "coop", "DM", "SURV", "TSURV", "CTF", "PF", "BTDM", "BDM", "LSS",
    "OSOK", "TOSOK", "BOSOK", "HTF", "TKTF", "KTF", "TPF", "TLSS", "BPF", "BLSS", "BTSURV", "BTOSOK"
};

const char *voteerrors[] = { "voting is currently disabled", "there is already a vote pending", "already voted", "can't vote that often", "this vote is not allowed in the current environment (singleplayer/multiplayer)", "no permission", "invalid vote", "server denied your call", "the next map/mode is already set" };
const char *mmfullnames[] = { "open", "private", "match" };

const char *fullmodestr(int n) { return (n>=-1 && size_t(n+1) < sizeof(modefullnames)/sizeof(modefullnames[0])) ? modefullnames[n+1] : "unknown"; }
const char *acronymmodestr(int n) { return (n>=-1 && size_t(n+1) < sizeof(modeacronymnames)/sizeof(modeacronymnames[0])) ? modeacronymnames[n+1] : "UNK"; } // 'n/a' bad on *nix filesystem (demonameformat)
const char *modestr(int n, bool acronyms) { return acronyms ? acronymmodestr (n) : fullmodestr(n); }
const char *voteerrorstr(int n) { return (n>=0 && (size_t)n < sizeof(voteerrors)/sizeof(voteerrors[0])) ? voteerrors[n] : "unknown"; }
const char *mmfullname(int n) { return (n>=0 && n < MM_NUM) ? mmfullnames[n] : "unknown"; }

int defaultgamelimit(int gamemode) { return m_teammode ? 15 : 10; }

static const int msgsizes[] =               // size inclusive message token, 0 for variable or not-checked sizes
{
    SV_SERVINFO, 5, SV_WELCOME, 2, SV_INITCLIENT, 0, SV_POS, 0, SV_POSC, 0, SV_POSN, 0, SV_TEXT, 0, SV_TEAMTEXT, 0, SV_TEXTME, 0, SV_TEAMTEXTME, 0, SV_TEXTPRIVATE, 0,
    SV_SOUND, 2, SV_VOICECOM, 2, SV_VOICECOMTEAM, 2, SV_CDIS, 2,
    SV_SHOOT, 0, SV_EXPLODE, 0, SV_SUICIDE, 1, SV_AKIMBO, 2, SV_RELOAD, 3, SV_AUTHT, 0, SV_AUTHREQ, 0, SV_AUTHTRY, 0, SV_AUTHANS, 0, SV_AUTHCHAL, 0,
    SV_GIBDIED, 5, SV_DIED, 5, SV_GIBDAMAGE, 7, SV_DAMAGE, 7, SV_HITPUSH, 6, SV_SHOTFX, 6, SV_THROWNADE, 8,
    SV_TRYSPAWN, 1, SV_SPAWNSTATE, 23, SV_SPAWN, 3, SV_SPAWNDENY, 2, SV_FORCEDEATH, 2, SV_RESUME, 0,
    SV_DISCSCORES, 0, SV_TIMEUP, 3, SV_EDITENT, 10, SV_ITEMACC, 2,
    SV_MAPCHANGE, 0, SV_ITEMSPAWN, 2, SV_ITEMPICKUP, 2,
    SV_PING, 2, SV_PONG, 2, SV_CLIENTPING, 2, SV_GAMEMODE, 2,
    SV_EDITMODE, 2, SV_EDITH, 7, SV_EDITT, 7, SV_EDITS, 6, SV_EDITD, 6, SV_EDITE, 6, SV_NEWMAP, 2,
    SV_SENDMAP, 0, SV_RECVMAP, 1, SV_REMOVEMAP, 0,
    SV_SERVMSG, 0, SV_ITEMLIST, 0, SV_WEAPCHANGE, 2, SV_PRIMARYWEAP, 2,
    SV_FLAGACTION, 3, SV_FLAGINFO, 0, SV_FLAGMSG, 0, SV_FLAGCNT, 3,
    SV_ARENAWIN, 2,
    SV_SETADMIN, 0, SV_SERVOPINFO, 3,
    SV_CALLVOTE, 0, SV_CALLVOTESUC, 1, SV_CALLVOTEERR, 2, SV_VOTE, 2, SV_VOTERESULT, 2,
    SV_SETTEAM, 3, SV_TEAMDENY, 2, SV_SERVERMODE, 2,
    SV_IPLIST, 0,
    SV_LISTDEMOS, 1, SV_SENDDEMOLIST, 0, SV_GETDEMO, 2, SV_SENDDEMO, 0, SV_DEMOPLAYBACK, 3,
    SV_CONNECT, 0,
    SV_SWITCHNAME, 0, SV_SWITCHSKIN, 0, SV_SWITCHTEAM, 0,
    SV_CLIENT, 0,
    SV_EXTENSION, 0,
    SV_MAPIDENT, 3, SV_HUDEXTRAS, 2, SV_POINTS, 0,
    -1
};

int msgsizelookup(int msg)
{
    static int sizetable[SV_NUM] = { -1 };
    if(sizetable[0] < 0)
    {
        memset(sizetable, -1, sizeof(sizetable));
        for(const int *p = msgsizes; *p >= 0; p += 2) sizetable[p[0]] = p[1];
    }
    return msg >= 0 && msg < SV_NUM ? sizetable[msg] : -1;
}

