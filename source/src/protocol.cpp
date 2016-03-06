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

// special encoding for asymmetric small integers: -1..252 go into one byte, -256..65279 go into three bytes and any other int goes into five bytes
// (the network encoding is compatible with basic getint)
template<class T>
static inline void putaint_(T &p, int n)
{
    DEBUGVAR(n);
    if(n<253 && n>-2) p.put(n - 125);
    else if(n<0xFF00 && n>=-256) { n += 256; p.put(0x80); p.put(n); p.put(n>>8); }
    else { p.put(0x81); p.put(n); p.put(n>>8); p.put(n>>16); p.put(n>>24); }
}
void putaint(ucharbuf &p, int n) { putaint_(p, n); }
void putaint(packetbuf &p, int n) { putaint_(p, n); }
void putaint(vector<uchar> &p, int n) { putaint_(p, n); }

int getaint(ucharbuf &p)
{
    int c = ((char)p.get()) + 125;
    if(c==-128) { int n = p.get(); n |= p.get()<<8; n -= 256; DEBUGVAR(n); return n; }
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

#define GZMSGBUFSIZE ((MAXGZMSGSIZE * 11) / 10)
static uchar *gzbuf = new uchar[GZMSGBUFSIZE];          // not thread-safe, but nesting is no problem

void putgzbuf(vector<uchar> &d, vector<uchar> &s) // zips a vector into a stream, stored in another vector; adds a header to simplify decompression
{
    int size = s.length();
    if(size < 1 || size > MAXGZMSGSIZE) size = 0;
    uLongf gzbufsize = GZMSGBUFSIZE;
    if(size && compress2(gzbuf, &gzbufsize, s.getbuf(), size, 9) == Z_OK)
    {
        if(gzbufsize > (uint)size)
        { // use zipped
            putuint(d, size);
            putuint(d, (int) gzbufsize);
            d.put(gzbuf, (int) gzbufsize);
            return;
        }
    }
    // use raw because it's smaller or because zipping failed
    putuint(d, size);
    putuint(d, 0);
    if(size) d.put(s.getbuf(), size);
}

ucharbuf *getgzbuf(ucharbuf &p) // fetch a gzipped buffer; needs to be freed properly later
{
    int size = getuint(p);
    int sizegz = getuint(p);
    if(sizegz == 0) sizegz = size; // uncompressed was smaller
    if(sizegz > 0 && p.remaining() >= sizegz && sizegz <= MAXGZMSGSIZE && size >= 0 && size <= MAXGZMSGSIZE)
    {
        uchar *data = new uchar[size + 1];
        uLongf rawsize = size;
        if(data)
        {
            if(size == sizegz)
            {
                memcpy(data, &p.buf[p.len], size);
            }
            else if(uncompress(data, &rawsize, &p.buf[p.len], sizegz) != Z_OK || rawsize - size != 0)
            {
                p.forceoverread();
                delete[] data;
                return NULL;
            }
            p.len += sizegz;
            return new ucharbuf(data, size);
        }
    }
    p.forceoverread();
    return NULL;
}

void freegzbuf(ucharbuf *p)  // free a ucharbuf created by getgzbuf()
{
    if(p)
    {
        delete[] p->buf;
        delete p;
    }
}

//#define FILENAMESALLLOWERCASE

bool validmapname(const char *s) // checks for length, allowed chars and special DOS filenames
{
    int len = strlen(s);
    if(len > MAXMAPNAMELEN) return false;
    if(len == 3 || len == 4)
    {
        char uc[4];
        loopi(3) uc[i] = toupper(s[i]);
        uc[3] = '\0';
        const char *resd = "COMLPTCONPRNAUXNUL", *fnd = strstr(resd, uc);
        if(fnd)
        {
            int pos = (int) (fnd - resd);
            if(pos == 0 || pos == 3)
            {
                if(isdigit(s[3])) return false; // COMx, LPTx
            }
            else if(pos % 3 == 0) return false; // CON, PRN, AUX, NUL
        }
    }
    while(*s != '\0')
    {
#ifdef FILENAMESALLLOWERCASE
        if(!islower(*s) && !isdigit(*s) && *s != '_' && *s != '-' && *s != '.') return false;
#else
        if(!isalnum(*s) && *s != '_' && *s != '-' && *s != '.') return false;
#endif
        ++s;
    }
    return true;
}


// filter text according to rules
// dst can be identical to src; dst needs to be of size "min(len, strlen(s)) + 1"
// returns dst

char *filtertext(char *dst, const char *src, int flags, int len)
{
    char *res = dst;
    bool nowhite = (flags & FTXT_NOWHITE) != 0,             // removes all whitespace; adding ALLOWBLANKS, ALLOWNL or ALLOWTAB enables exceptions
         allowblanks = (flags & FTXT_ALLOWBLANKS) != 0,     // only in combination with FTXT_NOWHITE
         allownl = (flags & FTXT_ALLOWNL) != 0,             // only in combination with FTXT_NOWHITE
         allowtab = (flags & FTXT_ALLOWTAB) != 0,           // only in combination with FTXT_NOWHITE
         nocolor = (flags & FTXT_NOCOLOR) != 0,             // removes all '\f' + following char
         tabtoblank = (flags & FTXT_TABTOBLANK) != 0,       // replaces \t with single blanks
         fillblanks = (flags & FTXT_FILLBLANKS) != 0,       // convert ' ' to '_'
         safecs = (flags & FTXT_SAFECS) != 0,               // removes all special chars that may execute commands when evaluate arguments; the argument still requires encapsulation by ""
         leet = (flags & FTXT_LEET) != 0,                   // translates leetspeak
         toupp = (flags & FTXT_TOUPPER) != 0,               // translates to all-uppercase
         tolow = (flags & FTXT_TOLOWER) != 0,               // translates to all-lowercase
         filename = (flags & FTXT_FILENAME) != 0,           // strict a-z, 0-9 and "-_.()" (also translates "[]" and "{}" to "()"), removes everything between '<' and '>'
         allowslash = (flags & FTXT_ALLOWSLASH) != 0,       // only in combination with FTXT_FILENAME
         mapname = (flags & FTXT_MAPNAME) != 0,             // only allows lowercase chars, digits, '_', '-' and '.'; probably should be used in combination with TOLOWER
         cropwhite = (flags & FTXT_CROPWHITE) != 0,         // removes leading and trailing whitespace
         pass = false;
    if(leet || mapname) nocolor = true;
#ifdef FILENAMESALLLOWERCASE
    if(filename) tolow = true;
#endif
    bool trans = toupp || tolow || leet || filename || fillblanks;
    bool leadingwhite = cropwhite;
    char *lastwhite = NULL;
    bool insidepointybrackets = false;
    for(int c = *src; c; c = *++src)
    {
        c &= 0x7F; // 7-bit ascii. not negotiable.
        pass = false;
        if(trans)
        {
            if(leet)
            {
                const char *org = "1374@5$2!*#%80|+",
                           *asc = "letaassziohzhoit",
                           *a = strchr(org, c);
                if(a) c = asc[a - org];
            }
            if(filename)
            {
                if(c == '>') insidepointybrackets = false;
                else if(c == '<') insidepointybrackets = true;
                if(insidepointybrackets || c == '>') continue; // filter anything between '<' and '>' as this may contain commands for texture loading
                const char *org = "[]{}\\",
                           *asc = "()()/",
                           *a = strchr(org, c);
                if(a) c = asc[a - org];
            }
            if(tolow) c = tolower(c);
            else if(toupp) c = toupper(c);
            if(fillblanks && c == ' ') c = '_';
        }
        if(filename && !(islower(c)
#ifndef FILENAMESALLLOWERCASE
                    || isupper(c)
#endif
                    || isdigit(c) || strchr("._-()", c) || (allowslash && c == '/'))) continue;
        if(safecs && strchr("($)\"", c)) continue;
        if(c == '\t')
        {
            if(tabtoblank) c = ' ';
            else if(allowtab) pass = true;
        }
        if(c == '\f')
        {
            if(nocolor)
            {
                if(src[1]) ++src;
                continue;
            }
            pass = true;
        }
        if(c == '\1')
        { // always filter encoded igraphs
            if(src[1]) ++src;
            continue;
        }
        if(mapname && !isalnum(c) && !strchr("_-./\\", c)) continue;
        if(isspace(c))
        {
            if(leadingwhite) continue;
            if(nowhite && !((c == ' ' && allowblanks) || (c == '\n' && allownl)) && !pass) continue;
            if(!lastwhite) lastwhite = dst;
        }
        else
        {
            lastwhite = NULL;
            if(!pass && !isprint(c)) continue;
        }
        leadingwhite = false;
        *dst++ = c;
        if(!--len || !*src) break;
    }
    if(cropwhite && lastwhite) *lastwhite = '\0';
    *dst = '\0';
    return res;
}

// translate certain escape sequences
// dst can be identical to src; dst needs to be of size "min(len, strlen(s)) + 1"

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
                case 'i': c = '\1'; break;
                case 'a': c = '\a'; break;
                case 't': c = '\t'; break;
                case 'n': c = '\n'; break;
                case 'x':
                    b = 16;
                    c = *++src;
                default:
                    if(isspace(c)) continue;
                    if(b == 0 && !isdigit(c)) break;
                    ul = strtoul(src, (char **) &src, b);
                    --src;
                    c = (int) ul & 0x7f;
                    if(!c) continue; // number conversion failed
                    break;
            }
        }
        *dst++ = c;
        if(!--len || !*src) break;
    }
    *dst = '\0';
}

// ensures, that d is either two lowercase chars or ""

void filterlang(char *d, const char *s)
{
    if(strlen(s) == 2)
    {
        filtertext(d, s, FTXT_TOLOWER, 2);
        if(islower(d[0]) && islower(d[1])) return;
    }
    *d = '\0';
}

void trimtrailingwhitespace(char *s)
{
    for(int n = (int)strlen(s) - 1; n >= 0 && isspace(s[n]); n--)
        s[n] = '\0';
}

const char *modefullnames[] =
{
    "demo playback",
    "team deathmatch", "coopedit", "deathmatch", "survivor",
    "team survivor", "capture the flag", "pistol frenzy", "bot team deathmatch", "bot deathmatch", "last swiss standing",
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

int gmode_possible(bool hasffaspawns, bool hasteamspawns, bool hasflags)  // return bitmask of playable modes, according to existing spawn and flag entities
{
    return ((hasffaspawns ? GMMASK__FFASPAWN : 0) | (hasteamspawns ? GMMASK__TEAMSPAWN : 0)) & ~(hasflags ? 0 : GMMASK__FLAGENTS);
}

int gmode_parse(const char *list) // convert a list of mode acronyms to a bitmask
{
    char *buf = newstring(list), *b;
    int res = 0;
    for(char *p = strtok_r(buf, "|", &b); p; p = strtok_r(NULL, "|", &b))
    {
        loopi(GMODE_NUM) if(!strcasecmp(p, modeacronymnames[i + 1])) res |= 1 << i;
    }
    delete buf;
    return res;
}

char *gmode_enum(int gm, char *buf) // convert mode bitmask to string with sorted list of mode acronyms
{
    vector<const char *> mas;
    loopi(GMODE_NUM) if(gm & (1 << i)) mas.add(modeacronymnames[i + 1]);
    mas.sort(stringsortignorecase);
    buf[0] = '\0';
    loopv(mas) concatformatstring(buf, "%s%s", i ? "|" : "", mas[i]);
    filtertext(buf, buf, FTXT_TOLOWER);
    return buf;
}

static const int msgsizes[] =               // size inclusive message token, 0 for variable or not-checked sizes
{
    SV_SERVINFO, 5, SV_WELCOME, 2, SV_INITCLIENT, 0, SV_POS, 0, SV_POSC, 0, SV_POSN, 0, SV_TEXT, 0, SV_TEAMTEXT, 0, SV_TEXTME, 0, SV_TEAMTEXTME, 0, SV_TEXTPRIVATE, 0,
    SV_SOUND, 2, SV_VOICECOM, 2, SV_VOICECOMTEAM, 2, SV_CDIS, 2,
    SV_SHOOT, 0, SV_EXPLODE, 0, SV_SUICIDE, 1, SV_AKIMBO, 2, SV_RELOAD, 3, SV_AUTHT, 0, SV_AUTHREQ, 0, SV_AUTHTRY, 0, SV_AUTHANS, 0, SV_AUTHCHAL, 0,
    SV_GIBDIED, 5, SV_DIED, 5, SV_GIBDAMAGE, 7, SV_DAMAGE, 7, SV_HITPUSH, 6, SV_SHOTFX, 6, SV_THROWNADE, 8,
    SV_TRYSPAWN, 1, SV_SPAWNSTATE, 23, SV_SPAWN, 3, SV_SPAWNDENY, 2, SV_FORCEDEATH, 2, SV_RESUME, 0,
    SV_DISCSCORES, 0, SV_TIMEUP, 3, SV_EDITENT, 13, SV_ITEMACC, 2,
    SV_MAPCHANGE, 0, SV_ITEMSPAWN, 2, SV_ITEMPICKUP, 2,
    SV_PING, 2, SV_PONG, 2, SV_CLIENTPING, 2, SV_GAMEMODE, 2,
    SV_EDITMODE, 2, SV_EDITXY, 8, SV_EDITARCH, 56, SV_EDITBLOCK, 0, SV_EDITD, 6, SV_EDITE, 6, SV_NEWMAP, 2,
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

const char *disc_reason(int reason)
{
    static const char *prot[] = { "terminated by enet", "end of packet", "client num", "tag type", "duplicate connection", "overflow", "random", "voodoo", "sync" },
        *refused[] = { "connection refused", "due to ban", "due to blacklisting", "wrong password", "server FULL", "server mastermode is \"private\"" },
        *removed[] = { "removed from server", "failed admin login", "inappropriate nickname", "annoyance limit exceeded", "SPAM", "DOS", "ludicrous speed", "fishslapped", "afk" },
        *kick[] = { "kicked", "banned", "kicked by vote", "banned by vote", "kicked by server operator", "banned by server operator",
                    "auto kicked - teamkilling", "auto banned - teamkilling", "auto kicked - friendly fire", "auto banned - friendly fire", "auto kick", "auto ban" };
    static string res;
    const int prot_n = constarraysize(prot), ref_n = constarraysize(refused), rem_n = constarraysize(removed), kick_n = constarraysize(kick);
    if(reason >= DISC_PROTOCOL && reason < DISC_REFUSED)
    { // protocol related errors
        if(reason < DISC_PROTOCOL + prot_n) return prot[reason];
        else formatstring(res)("network protocol error '%d'", reason);
    }
    else if(reason >= DISC_REFUSED && reason < DISC_REMOVED)
    { // connection refused
        if(reason < DISC_REFUSED + ref_n) return refused[reason - DISC_REFUSED];
        else formatstring(res)("connection refused '%d'", reason);
    }
    else if(reason >= DISC_REMOVED && reason < DISC_KICK)
    { // player removed from server
        if(reason < DISC_REMOVED + rem_n) return removed[reason - DISC_REMOVED];
        else formatstring(res)("removed from server '%d'", reason);
    }
    else if(reason >= DISC_KICK && reason < DISC_UNKNOWN)
    { // kicks & bans
        reason -= DISC_KICK;
        if(reason < kick_n) return kick[reason];
        else formatstring(res)("%s '%d'", reason & 1 ? "banned" : "kicked", reason + DISC_KICK);
    }
    else
    { // unknown reasons
        formatstring(res)("unknown '%d'", reason);
    }
    return res;
}

