// implementation of generic tools

#include "cube.h"

extern bool isdedicated;

#ifdef NO_POSIX_R
char *strtok_r(char *s, const char *delim, char **b)
{
    if(s) *b = s;
    *b += strspn(*b, delim);
    if(!**b) return NULL;
    s = *b;
    *b += strcspn(s, delim);
    if(**b) *(*b)++ = '\0';
    return s;
}
#endif

string _timestringbuffer = "";

const char *timestring(time_t t, bool local, const char *fmt, char *buf)
{
#ifdef NO_POSIX_R
    struct tm *timeinfo;
    timeinfo = local ? localtime(&t) : gmtime (&t);
#else
    struct tm *timeinfo, b;
    timeinfo = local ? localtime_r(&t, &b) : gmtime_r(&t, &b);
#endif
    strftime(buf, sizeof(string) - 1, fmt && *fmt ? fmt : "%Y%m%d_%H.%M.%S", timeinfo); // sortable time for filenames
    return buf;
}

const char *asctimestr()
{
    return timestring(true, "%c");
}

const char *numtime()
{
    static string numt;
    time_t t = time(NULL);
    int t1 = t / 1000000, t2 = t % 1000000;
    formatstring(numt)("%d%06d", t1, t2);
    return numt + strspn(numt, "0");
}

static const uchar transformenttab[] = {
            /* mapformat     1..5                               6..7     */
            /*  0 */   /* NOTUSED        */ NOTUSED,      /* NOTUSED     */ NOTUSED,
            /*  1 */   /* LIGHT          */ LIGHT,        /* LIGHT       */ LIGHT,
            /*  2 */   /* PLAYERSTART    */ PLAYERSTART,  /* PLAYERSTART */ PLAYERSTART,
            /*  3 */   /* I_SHELLS       */ I_AMMO,       /* I_CLIPS     */ I_CLIPS,
            /*  4 */   /* I_BULLETS      */ I_AMMO,       /* I_AMMO      */ I_AMMO,
            /*  5 */   /* I_ROCKETS      */ I_AMMO,       /* I_GRENADE   */ I_GRENADE,
            /*  6 */   /* I_ROUNDS       */ I_AMMO,       /* I_HEALTH    */ I_HEALTH,
            /*  7 */   /* I_HEALTH       */ I_HEALTH,     /* I_ARMOUR    */ I_ARMOUR,
            /*  8 */   /* I_BOOST        */ I_HEALTH,     /* I_AKIMBO    */ I_AKIMBO,
            /*  9 */   /* I_GREENARMOUR  */ I_HELMET,     /* MAPMODEL    */ MAPMODEL,
            /* 10 */   /* I_YELLOWARMOUR */ I_ARMOUR,     /* CARROT      */ CARROT,
            /* 11 */   /* I_QUAD         */ I_AKIMBO,     /* LADDER      */ LADDER,
            /* 12 */   /* TELEPORT       */ NOTUSED,      /* CTF_FLAG    */ CTF_FLAG,
            /* 13 */   /* TELEDEST       */ NOTUSED,      /* SOUND       */ SOUND,
            /* 14 */   /* MAPMODEL       */ MAPMODEL,     /* CLIP        */ CLIP,
            /* 15 */   /* MONSTER        */ NOTUSED,      /* PLCLIP      */ PLCLIP,
            /* 16 */   /* CARROT         */ NOTUSED,                        16,
            /* 17 */   /* JUMPPAD        */ NOTUSED,                        17      };

void transformoldentitytypes(int mapversion, uchar &enttype)
{
    const uchar *usetab = transformenttab + (mapversion > 5 ? 1 : 0);
    if(mapversion < 8 && enttype < 18) enttype = usetab[enttype * 2];
}

int fixmapheadersize(int version, int headersize)   // we can't trust hdr.headersize for file versions < 10 (thx flow)
{
    if(version < 4) return sizeof(header) - sizeof(int) * 16;
    else if(version == 7 || version == 8) return sizeof(header) + sizeof(char) * 128;  // mediareq
    else if(version < 10 || headersize < int(sizeof(header))) return sizeof(header);
    return headersize;
}

header *peekmapheader(uchar *data, int len) // extract the header from an in-memory mapfile (doesn't process all values: only used to get the map revision and timestamp)
{
    static header h;
    uLongf rawsize = (int)sizeof(header);
    if(uncompress((Bytef*)&h, &rawsize, data, len) == Z_BUF_ERROR && (!strncmp(h.head, "CUBE", 4) || !strncmp(h.head, "ACMP",4)))
    {
        lilswap(&h.version, 4); // version, headersize, sfactor, numents
        if(h.version < 4) memset(&h.waterlevel, 0, sizeof(int) * 16);
        else lilswap(&h.maprevision, 4); // maprevision, ambient, flags, timestamp
        return &h;
    }
    return NULL;
}

// map geometry statistics

servsqr *createservworld(const sqr *s, int _cubicsize) // create a server-style floorplan on the client, to use same statistics functions on client and server
{
    servsqr *res = new servsqr[_cubicsize], *d = res;
    loopirev(_cubicsize)
    {
        d->type = s->type == SOLID ? SOLID : ((s->type & TAGTRIGGERMASK) | (s->tag & ~TAGTRIGGERMASK)); // guaranteed to not have tagclips on SOLID cubes - so we can check for SOLID without masks
        d->ceil = s->ceil;
        d->floor = s->floor;
        d->vdelta = s->vdelta;
        d++;
        s++;
    }
    return res;
}

int calcmapdims(mapdim_s &md, const servsqr *s, int _ssize)
{
    int res = 0;
    md.x1 = md.y1 = _ssize;
    md.x2 = md.y2 = 0;
    md.minfloor = 127; md.maxceil = -128;
    for(int y = 0; y < _ssize; y++) for(int x = 0; x < _ssize; x++)
    {
        if((s->type & TAGTRIGGERMASK) != SOLID)
        {
            if(x < md.x1) md.x1 = x;
            if(x > md.x2) md.x2 = x;
            if(y < md.y1) md.y1 = y;
            md.y2 = y;
            if(s->floor < md.minfloor) md.minfloor = s->floor;
            if(s->ceil > md.maxceil) md.maxceil = s->ceil;
        }
        s++;
    }
    if(md.x2 < md.x1 || md.y2 < md.y1)
    { // map is completely solid -> default to empty map values
        md.x1 = md.y1 = 2;
        md.x2 = md.y2 = _ssize - 3;
        md.minfloor = 0;
        md.maxceil = 16;
        res = -1; // reject map on servers
    }
    if(md.x1 < MINBORD || md.y1 < MINBORD || md.x2 >= _ssize - MINBORD || md.y2 >= _ssize - MINBORD) res = -2; // reject map because of world border violation
    md.xspan = md.x2 - md.x1 + 1;
    md.yspan = md.y2 - md.y1 + 1;
    md.xm = md.x1 + md.xspan / 2.0f;
    md.ym = md.y1 + md.yspan / 2.0f;
    return res;
}

int calcmapareastats(mapareastats_s &ms, servsqr *_servworld, int _ssize, const mapdim_s &md)
{
    memset(&ms, 0, sizeof(ms));

    // count steep FHF and CHF
    int linegap = _ssize - md.xspan;
    #ifndef STANDALONE
    int totalmax = 0;
    #endif
    servsqr *bb = _servworld + _ssize * md.y1 + md.x1, *ss = bb;
    for(int j = md.yspan; j > 0; j--, ss += linegap) loopirev(md.xspan)
    {
        int type = ss->type & TAGTRIGGERMASK;
        if(type == FHF || type == CHF) // only CHF and FHF use vdelta
        {
            servsqr *r[3] = { ss + 1, ss + _ssize, ss + _ssize + 1 };
            int min = ss->vdelta, max = min;
            loopi(3)
            {
                if(r[i]->vdelta < min) min = r[i]->vdelta;
                else if(r[i]->vdelta > max) max = r[i]->vdelta;
            }
            max -= min;
            #ifndef STANDALONE
            if(max > totalmax)
            {
                ms.steepest = int(ss - _servworld);
                totalmax = max;
            }
            #endif
            int d = max / MAS_VDELTA_QUANT;
            ASSERT(d >= 0);
            if(d >= MAS_VDELTA_TABSIZE) d = MAS_VDELTA_TABSIZE - 1;
            ms.vdd[d]++;
        }
        ss++;
    }
    loopirev(MAS_VDELTA_TABSIZE) ms.vdds = (ms.vdds << 2) | ((ms.vdd[i] > MAS_VDELTA_THRES) << 1) | (ms.vdd[i] > 0);

    // check occlusion by SOLIDs (destroys vdelta values)
    ss = bb;
    for(int j = md.yspan; j > 0; j--, ss += linegap) loopirev(md.xspan) (ss++)->vdelta = 0; // reset all vdelta values
    int xc = (md.xspan + MAS_GRID / 2) / (MAS_GRID + 1), yc = (md.yspan + MAS_GRID / 2) / (MAS_GRID + 1);
    if(md.x1 + MAS_GRID * xc >= _ssize || md.y1 + MAS_GRID * yc >= _ssize) return -1; // malformed map
    int xb = min(md.x1 + xc, _ssize - MAS_GRID * xc - md.x1 - 1) / 2, yb = min(md.y1 + yc, _ssize - MAS_GRID * yc - md.y1 - 1) / 2; // make sure, all tp points are inside the map area
    int tgx = min(xb, max(3, xc / 3)), tgy = min(yb, max(3, yc / 3)) * _ssize, tp[8] = { tgx, -tgx, tgy, -tgy, 2 * tgx + 2 * tgy, -2 * tgx - 2 * tgy, -2 * tgx + 2 * tgy, 2 * tgx - 2 * tgy }; // 8 positions to try, if the stariing point is solid
    uchar epoch = 1;
    servsqr *s = bb + xc + yc * _ssize; ss = s;
    vector<threeint> tab;
    threeint pp;
    for(int y = 0; y < MAS_GRID; y++, ss = s += _ssize * yc) for(int x = 0; x < MAS_GRID; x++, ss += xc) // measure visible area in MAS_GRID x MAS_GRID probe points
    { // calculate MAS_GRID^2 probe points (with a low-res version of computeraytable()
        servsqr *r = ss, *rr;
        if(SOLID(r)) loopi(8)
        { // if initial position is SOLID, we try 8 points around that spot
            if(!SOLID(r + tp[i]))
            {
                r += tp[i];
                break;
            }
        }
        pp.val2 = int(r - _servworld);
        int area = 0, volume = 0, frac;
        loopk(MAS_RESOLUTION)
        {
            #define RAYS(da, db) \
                rr = r; frac = 0; \
                for(;;) \
                { \
                    if((frac += k) >= MAS_RESOLUTION) rr += da, frac -= MAS_RESOLUTION; \
                    rr += db; \
                    if(SOLID(rr)) break; \
                    if(rr->vdelta != epoch) { area += 1; volume += rr->ceil - rr->floor; } \
                    rr->vdelta = epoch; \
                }
            RAYS(_ssize, 1);
            RAYS(_ssize, -1);
            RAYS(-_ssize, 1);
            RAYS(-_ssize, -1);
            RAYS(1, _ssize);
            RAYS(1, -_ssize);
            RAYS(-1, _ssize);
            RAYS(-1, -_ssize);
            #undef RAYS
        }
        pp.val1 = volume;
        pp.key = area;
        tab.add(pp);
        if(area) epoch++;
        if(!epoch) epoch++;
    }
    ss = bb;
    for(int j = md.yspan; j > 0; j--, ss += linegap) loopirev(md.xspan)
    {
        if(!SOLID(ss))
        {
            ms.total++;
            if(!ss->vdelta) ms.rest++; // count all cubes not in view of one of the probe points
        }
        ss++;
    }
    ASSERT(tab.length() == MAS_GRID2);
    tab.sort(cmpintdesc);
    loopv(tab)
    { // sort probe poiints in descending order of area
        ms.ppv[i] = tab[i].val1;
        ms.ppa[i] = tab[i].key;
        #ifndef STANDALONE
        ms.ppp[i] = tab[i].val2;
        #endif
    }
    return 0;
}

int lastflagdistancewarning = 0;
void calcentitystats(entitystats_s &es, const persistent_entity *pents, int pentsize)
{
#ifndef STANDALONE
    vector<persistent_entity> _pents;
    if(!pents)
    { // use regular ents list
        loopv(ents) _pents.add() = ents[i];
        pentsize = _pents.length();
        if(pentsize) pents = &_pents[0];
    }
#endif
    memset(&es, 0, sizeof(es));
    loopi(MAXENTTYPES) es.first[i] = pentsize;
    vector<int> picks;
    loopi(pentsize)
    {
        const persistent_entity &e = pents[i];
        if(e.type < MAXENTTYPES)
        {
            es.entcnt[e.type]++;
            if(es.first[e.type] == pentsize) es.first[e.type] = i;
            es.last[e.type] = i;
            if(e.type == PLAYERSTART) switch(e.attr2)
            {
                case 0:   es.spawns[0]++; break;
                case 1:   es.spawns[1]++; break;
                case 100: es.spawns[2]++; break;
                default: es.unknownspawns++; break;
            }
            if(e.type == CTF_FLAG) switch(e.attr2)
            {
                case 0:
                case 1:
                    es.flags[e.attr2]++;
                    es.flagents[e.attr2] = i;
                    break;
                default:
                    es.unknownflags++;
                    break;
            }
            if(isitem(e.type)) picks.add(i);
        }
    }
    es.hasffaspawns = es.spawns[2] >= MINSPAWNS;
    es.hasteamspawns = es.spawns[0] >= MINSPAWNS && es.spawns[1] >= MINSPAWNS;
    if(es.flags[0] == 1 && es.flags[1] == 1)
    {
        vec fd(pents[es.flagents[0]].x - pents[es.flagents[1]].x, pents[es.flagents[0]].y - pents[es.flagents[1]].y, 0);
        es.flagentdistance = fd.magnitude();
        #ifndef STANDALONE
        // the clientgame:server calls this too, so we need to check we didn't just warn already.
        if(es.flagentdistance < MINFLAGDISTANCE && (lastflagdistancewarning==0 || (lastmillis - lastflagdistancewarning) > 1000))
        {
            hudoutf("\f2flags \f3too close\f5! \f4[%d<%d]", es.flagentdistance, MINFLAGDISTANCE);
            lastflagdistancewarning = lastmillis;
        }
        #endif // STANDALONE
        es.hasflags = es.flagentdistance >= MINFLAGDISTANCE;
    }
    int r = 0;
    loopvjrev(picks) loopirev(j)
    { // calculate the distances between all pickups (takes n^2/2 calculations - which should be fine for any sane map)
        double d = pow2(pents[picks[i]].x - pents[picks[j]].x) + pow2(pents[picks[i]].y - pents[picks[j]].y); // deltaX^2 + deltaY^2
        frexp(d, &r);
        r /= 2; // sqrt ;)
        if(r > LARGEST_FACTOR) r = LARGEST_FACTOR;
        es.pickupdistance[r]++;
    }
    es.pickups = picks.length();
    es.modes_possible = gmode_possible(es.hasffaspawns, es.hasteamspawns, es.hasflags);
}
#if 0
const char *rateentitystats(entitystats_s &es)
{
    static char res[LARGEST_FACTOR + 1];


}
#endif

int cmpintasc(const int *a, const int *b) { return *a - *b; } // leads to ascending sort order
int cmpintdesc(const int *a, const int *b) { return *b - *a; } // leads to descending sort order
int stringsort(const char **a, const char **b) { return strcmp(*a, *b); }
int stringsortrev(const char **a, const char **b) { return strcmp(*b, *a); }
int stringsortignorecase(const char **a, const char **b) { return strcasecmp(*a, *b); }
int stringsortignorecaserev(const char **a, const char **b) { return strcasecmp(*b, *a); }

///////////////////////// debugging ///////////////////////

#if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
void stackdumper(unsigned int type, EXCEPTION_POINTERS *ep)
{
    if(!ep) fatal("unknown type");
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    CONTEXT *context = ep->ContextRecord;
    string out, t;
    formatstring(out)("Win32 Exception: 0x%x [0x%x]\n\n", er->ExceptionCode, er->ExceptionCode==EXCEPTION_ACCESS_VIOLATION ? er->ExceptionInformation[1] : -1);
    STACKFRAME sf = {{context->Eip, 0, AddrModeFlat}, {}, {context->Ebp, 0, AddrModeFlat}, {context->Esp, 0, AddrModeFlat}, 0};
    SymInitialize(GetCurrentProcess(), NULL, TRUE);

    while(::StackWalk(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(), GetCurrentThread(), &sf, context, NULL, ::SymFunctionTableAccess, ::SymGetModuleBase, NULL))
    {
        struct { IMAGEHLP_SYMBOL sym; string n; } si = { { sizeof( IMAGEHLP_SYMBOL ), 0, 0, 0, sizeof(string) } };
        IMAGEHLP_LINE li = { sizeof( IMAGEHLP_LINE ) };
        DWORD off;
        if(SymGetSymFromAddr(GetCurrentProcess(), (DWORD)sf.AddrPC.Offset, &off, &si.sym) && SymGetLineFromAddr(GetCurrentProcess(), (DWORD)sf.AddrPC.Offset, &off, &li))
        {
            char *del = strrchr(li.FileName, '\\');
            formatstring(t)("%s - %s [%d]\n", si.sym.Name, del ? del + 1 : li.FileName, li.LineNumber);
            concatstring(out, t);
        }
    }
#if !defined(STANDALONE)
    if(clientlogfile) clientlogfile->printf("%s\n", out);
#endif
    fatal("%s", out);
}
#elif defined(linux) || defined(__linux) || defined(__linux__)

#include <execinfo.h>

// stack dumping on linux, inspired by Sachin Agrawal's sample code

struct signalbinder
{
    static void stackdumper(int sig)
    {
        printf("stacktrace:\n");
#if !defined(STANDALONE)
        if(clientlogfile) clientlogfile->printf("no stacktrace\n");
#endif
        const int BTSIZE = 25;
        void *array[BTSIZE];
        int n = backtrace(array, BTSIZE);
        backtrace_symbols_fd(array, n, 1);

        fatal("AssaultCube error (%d)", sig);

    }

    signalbinder()
    {
        // register signals to dump the stack if they are raised,
        // use constructor for early registering
        signal(SIGSEGV, stackdumper);
        signal(SIGFPE, stackdumper);
        signal(SIGILL, stackdumper);
        signal(SIGBUS, stackdumper);
        signal(SIGSYS, stackdumper);
        signal(SIGABRT, stackdumper);
    }
};

signalbinder sigbinder;

#endif


///////////////////////// misc tools ///////////////////////

bool cmpb(void *b, int n, enet_uint32 c)
{
    ENetBuffer buf;
    buf.data = b;
    buf.dataLength = n;
    return enet_crc32(&buf, 1)==c;
}

bool cmpf(char *fn, enet_uint32 c)
{
    int n = 0;
    char *b = loadfile(fn, &n);
    bool r = cmpb(b, n, c);
    delete[] b;
    return r;
}

enet_uint32 adler(unsigned char *data, size_t len)
{
    enet_uint32 a = 1, b = 0;
    while (len--)
    {
        a += *data++;
        b += a;
    }
    return b;
}

bool isbigendian()
{
    return !*(const uchar *)&islittleendian;
}

void strtoupper(char *t, const char *s)
{
    if(!s) s = t;
    while(*s)
    {
        *t = toupper(*s);
        t++; s++;
    }
    *t = '\0';
}

const char *atoip(const char *s, enet_uint32 *ip)
{
    unsigned int d[4];
    int n;
    if(!s) return NULL;
    if(sscanf(s, "%u.%u.%u.%u%n", d, d + 1, d + 2, d + 3, &n) != 4)
    {
        *ip = strtoul(s, (char **)&s, 0); // try single-integer IPs
        return *ip > 0xffffff ? s : NULL; // require first octet to be non-zero to avoid misinterpreting faulty dotted IPs
    }
    *ip = 0;
    loopi(4)
    {
        if(d[i] > 255) return NULL;
        *ip = (*ip << 8) + d[i];
    }
    return s + n;
}

const char *atoipr(const char *s, iprange *ir)
{
    if((s = atoip(s, &ir->lr)) == NULL) return NULL;
    ir->ur = ir->lr;
    s += strspn(s, " \t");
    if(*s == '-')
    {
        if(!(s = atoip(s + 1, &ir->ur)) || ir->lr > ir->ur) return NULL;
    }
    else if(*s == '/')
    {
        int m, n;
        if(sscanf(s + 1, "%d%n", &m, &n) != 1 || m < 0 || m > 32) return NULL;
        unsigned long bm = (1 << (32 - m)) - 1;
        ir->lr &= ~bm;
        ir->ur |= bm;
        s += 1 + n;
    }
    return s;
}

const char *iptoa(const enet_uint32 ip, char *b)
{
    formatstring(b)("%d.%d.%d.%d", (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
    return b;
}

const char *iprtoa(const struct iprange &ipr, char *b)
{
    if(ipr.lr == ipr.ur) return iptoa(ipr.lr, b);
    else formatstring(b)("%d.%d.%d.%d-%d.%d.%d.%d", (ipr.lr >> 24) & 255, (ipr.lr >> 16) & 255, (ipr.lr >> 8) & 255, ipr.lr & 255, (ipr.ur >> 24) & 255, (ipr.ur >> 16) & 255, (ipr.ur >> 8) & 255, ipr.ur & 255);
    return b;
}

char *formatdemofilename(const char *demoformat, const char *timestampformat, const char *map, int mode, int srvclock, int secondsplayed, int secondsremaining, enet_uint32 ip, char *buf)
{
    vector<char> d;
    // we use the following internal mapping of formatchars:
    // %g : gamemode (int)      %G : gamemode (chr)             %F : gamemode (full)
    // %m : minutes remaining   %M : minutes played
    // %s : seconds remaining   %S : seconds played
    // %h : IP of server        %H : hostname of server (client only)
    // %n : mapName
    // %w : timestamp "when"
    bool did_map = false, did_mode = false, did_ts = false;
    for(const char *s = demoformat; *s; s++)
    {
        if(*s == '%')
        {
            switch(*++s)
            {
                case 'F': cvecconcat(d, fullmodestr(mode)); did_mode = true; break;
                case 'g': cvecprintf(d, "%d", mode); did_mode = true; break;
                case 'G': cvecconcat(d, acronymmodestr(mode)); did_mode = true; break;
                case 'H':
                    if(!isdedicated)
                    {
#ifndef STANDALONE
                        ENetAddress a = { htonl(ip), 0 };
                        cvecconcat(d, !enet_address_get_host(&a, buf, MAXSTRLEN) ? buf : "unknown");
                        break;
#endif
                    } // fallthrough to "IP" on dedicated servers
                case 'h': cvecconcat(d, ip ? iptoa(ip, buf) : "local"); break;
                case 'm': cvecprintf(d, "%d", secondsremaining / 60); break;
                case 'M': cvecprintf(d, "%d", secondsplayed / 60); break;
                case 'n': cvecconcat(d, map); did_map = true; break;
                case 's': cvecprintf(d, "%d", secondsremaining); break;
                case 'S': cvecprintf(d, "%d", secondsplayed); break;
                case 'w':
                {
                    time_t t = ((time_t) srvclock) * 60;
                    bool utc = *timestampformat == 'U';
                    cvecconcat(d, timestring(t, !utc, timestampformat + int(utc), buf));
                    did_ts = true;
                    break;
                }
                default:
                    if(*s) d.add(*s);
                    break;
            }
        }
        else d.add(*s);
    }
    d.add('\0');
    filtertext(buf, d.getbuf(), FTXT__DEMONAME);
    return did_map && did_mode && did_ts ? buf : formatdemofilename("%w_%h_%n_%G", "%Y%m%d_%H%M", map, mode, srvclock, secondsplayed, secondsremaining, ip, buf);
}

int cmpiprange(const struct iprange *a, const struct iprange *b)
{
    if(a->lr < b->lr) return -1;
    if(a->lr > b->lr) return 1;
    return 0;
}

int cmpipmatch(const struct iprange *a, const struct iprange *b)
{
    return - (a->lr < b->lr) + (a->lr > b->ur);
}

char *concatformatstring(char *d, const char *s, ...)
{
    static defvformatstring(temp, s, s);
    return concatstring(d, temp);
}

int cvecprintf(vector<char> &v, const char *s, ...)
{
    defvformatstring(temp, s, s);
    int len = strlen(temp);
    if(len) v.put(temp, len);
    return len;
}

int cvecconcat(vector<char> &v, const char *s)
{
    int len = strlen(s);
    if(len) v.put(s, len);
    return len;
}

const char *hiddenpwd(const char *pwd, int showchars)
{
    static int sc = 3;
    static string text;
    copystring(text, pwd);
    if(showchars > 0) sc = showchars;
    for(int i = (int)strlen(text) - 1; i >= sc; i--) text[i] = '*';
    return text;
}

int getlistindex(const char *key, const char *list[], bool acceptnumeric, int deflt)
{
    int max = 0;
    while(list[max] && list[max][0]) if(!strcasecmp(key, list[max])) return max; else max++;
    if(acceptnumeric && isdigit(key[0]))
    {
        int i = (int)strtol(key, NULL, 0);
        if(i >= 0 && i < max) return i;
    }
#if !defined(STANDALONE) && defined(_DEBUG)
    char *opts = conc(list, -1, true);
    if(!isdedicated && *key) clientlogf("warning: unknown token \"%s\" (not in list [%s])", key, opts);
    delstring(opts);
#endif
    return deflt;
}

//////////////// geometry utils ////////////////

static inline float det2x2(float a, float b, float c, float d) { return a*d - b*c; }
static inline float det3x3(float a1, float a2, float a3,
                           float b1, float b2, float b3,
                           float c1, float c2, float c3)
{
    return a1 * det2x2(b2, b3, c2, c3)
         - b1 * det2x2(a2, a3, c2, c3)
         + c1 * det2x2(a2, a3, b2, b3);
}

float glmatrixf::determinant() const
{
    float a1 = v[0], a2 = v[1], a3 = v[2], a4 = v[3],
          b1 = v[4], b2 = v[5], b3 = v[6], b4 = v[7],
          c1 = v[8], c2 = v[9], c3 = v[10], c4 = v[11],
          d1 = v[12], d2 = v[13], d3 = v[14], d4 = v[15];

    return a1 * det3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4)
         - b1 * det3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4)
         + c1 * det3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4)
         - d1 * det3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4);
}

void glmatrixf::adjoint(const glmatrixf &m)
{
    float a1 = m.v[0], a2 = m.v[1], a3 = m.v[2], a4 = m.v[3],
          b1 = m.v[4], b2 = m.v[5], b3 = m.v[6], b4 = m.v[7],
          c1 = m.v[8], c2 = m.v[9], c3 = m.v[10], c4 = m.v[11],
          d1 = m.v[12], d2 = m.v[13], d3 = m.v[14], d4 = m.v[15];

    v[0]  =  det3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4);
    v[1]  = -det3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4);
    v[2]  =  det3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4);
    v[3]  = -det3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4);

    v[4]  = -det3x3(b1, b3, b4, c1, c3, c4, d1, d3, d4);
    v[5]  =  det3x3(a1, a3, a4, c1, c3, c4, d1, d3, d4);
    v[6]  = -det3x3(a1, a3, a4, b1, b3, b4, d1, d3, d4);
    v[7]  =  det3x3(a1, a3, a4, b1, b3, b4, c1, c3, c4);

    v[8]  =  det3x3(b1, b2, b4, c1, c2, c4, d1, d2, d4);
    v[9]  = -det3x3(a1, a2, a4, c1, c2, c4, d1, d2, d4);
    v[10] =  det3x3(a1, a2, a4, b1, b2, b4, d1, d2, d4);
    v[11] = -det3x3(a1, a2, a4, b1, b2, b4, c1, c2, c4);

    v[12] = -det3x3(b1, b2, b3, c1, c2, c3, d1, d2, d3);
    v[13] =  det3x3(a1, a2, a3, c1, c2, c3, d1, d2, d3);
    v[14] = -det3x3(a1, a2, a3, b1, b2, b3, d1, d2, d3);
    v[15] =  det3x3(a1, a2, a3, b1, b2, b3, c1, c2, c3);
}

bool glmatrixf::invert(const glmatrixf &m, float mindet)
{
    float a1 = m.v[0], b1 = m.v[4], c1 = m.v[8], d1 = m.v[12];
    adjoint(m);
    float det = a1*v[0] + b1*v[1] + c1*v[2] + d1*v[3]; // float det = m.determinant();
    if(fabs(det) < mindet) return false;
    float invdet = 1/det;
    loopi(16) v[i] *= invdet;
    return true;
}

// multithreading and ipc tools wrapper for the server
// all embedded servers and all standalone servers except on linux use SDL
// the standalone linux version uses native linux libraries - and also makes use of shared memory

#ifdef AC_USE_SDL_THREADS
    #include "SDL_timer.h"
    #include "SDL_thread.h"      // also fetches SDL_mutex.h
#else
    #include <pthread.h>
    #include <semaphore.h>
    #include <sys/shm.h>
#endif

static int sl_sem_errorcountdummy = 0;

#ifdef AC_USE_SDL_THREADS
sl_semaphore::sl_semaphore(int init, int *ecnt)
{
    data = (void *)SDL_CreateSemaphore(init);
    errorcount = ecnt ? ecnt : &sl_sem_errorcountdummy;
    if(data == NULL) (*errorcount)++;
}

sl_semaphore::~sl_semaphore()
{
    if(data) SDL_DestroySemaphore((SDL_sem *) data);
}

void sl_semaphore::wait()
{
    if(SDL_SemWait((SDL_sem *) data) != 0) (*errorcount)++;
}

int sl_semaphore::trywait()
{
    return SDL_SemTryWait((SDL_sem *) data);
}

int sl_semaphore::timedwait(int howlongmillis)
{
    return SDL_SemWaitTimeout((SDL_sem *) data, howlongmillis);
}

int sl_semaphore::getvalue()
{
    return SDL_SemValue((SDL_sem *) data);
}

void sl_semaphore::post()
{
    if(SDL_SemPost((SDL_sem *) data) != 0) (*errorcount)++;
}
#else
sl_semaphore::sl_semaphore(int init, int *ecnt)
{
    errorcount = ecnt ? ecnt : &sl_sem_errorcountdummy;
    data = (void *) new sem_t;
    if(data == NULL || sem_init((sem_t *) data, 0, init) != 0) (*errorcount)++;
}

sl_semaphore::~sl_semaphore()
{
    if(data)
    {
        if(sem_destroy((sem_t *) data) != 0) (*errorcount)++;
        delete (sem_t *)data;
    }
}

void sl_semaphore::wait()
{
    if(sem_wait((sem_t *) data) != 0) (*errorcount)++;
}

int sl_semaphore::trywait()
{
    return sem_trywait((sem_t *) data);
}

int sl_semaphore::timedwait(int howlongmillis)
{
    struct timespec t;
    if(clock_gettime(CLOCK_REALTIME, &t))
    {
        (*errorcount)++;
        return sem_trywait((sem_t *) data);
    }
    howlongmillis += t.tv_nsec / 1000000;
    t.tv_nsec = (howlongmillis % 1000) * 1000000;
    t.tv_sec += howlongmillis / 1000;
    return sem_timedwait((sem_t *) data, &t);
}

int sl_semaphore::getvalue()
{
    int ret;
    if(sem_getvalue((sem_t *) data, &ret) != 0) (*errorcount)++;
    return ret;
}

void sl_semaphore::post()
{
    if(sem_post((sem_t *) data) != 0) (*errorcount)++;
}
#endif

// (wrapping threads is slightly ugly, since SDL threads use a different return value (int) than pthreads (void *) - and that can't be solved with a typecast)
#ifdef AC_USE_SDL_THREADS
struct sl_threadinfo { int (*fn)(void *); void *data; SDL_Thread *handle; volatile char done; };

static int sl_thread_indir(void *info)
{
    int res = (*((sl_threadinfo*)info)->fn)(((sl_threadinfo*)info)->data);
    ((sl_threadinfo*)info)->done = 1;
    return res;
}

void *sl_createthread(int (*fn)(void *), void *data, const char *name)
{
    sl_threadinfo *ti = new sl_threadinfo;
    ti->data = data;
    ti->fn = fn;
    ti->done = 0;
    ti->handle = SDL_CreateThread(sl_thread_indir, name, ti);
    return (void *) ti;
}

int sl_waitthread(void *ti)
{
    int res;
    SDL_WaitThread(((sl_threadinfo *)ti)->handle, &res);
    delete (sl_threadinfo *) ti;
    return res;
}

static vector<sl_threadinfo *> oldthreads;
static sl_semaphore sem_oldthreads(1, NULL);

void sl_detachthread(void *ti) // SDL can't actually detach threads, so this is manual cleanup
{
    if(ti && sl_pollthread(ti))
    {
        SDL_WaitThread(((sl_threadinfo *)ti)->handle, NULL);
        delete (sl_threadinfo *) ti;
        ti = NULL;
    }
    if(ti || sem_oldthreads.getvalue() > 0)
    {
        sem_oldthreads.wait();
        if(ti) oldthreads.add((sl_threadinfo *)ti);
        loopvrev(oldthreads)
        {
            if(oldthreads[i]->done)
            {
                SDL_WaitThread(oldthreads[i]->handle, NULL);
                delete oldthreads.remove(i);
            }
        }
        sem_oldthreads.post();
    }
}

static SDL_threadID mainthreadid = SDL_ThreadID();
bool ismainthread() { return mainthreadid == SDL_ThreadID(); }

#else
struct sl_threadinfo { int (*fn)(void *); void *data; pthread_t handle; int res; volatile char done; };

static void *sl_thread_indir(void *info)
{
    sl_threadinfo *ti = (sl_threadinfo*) info;
    ti->res = (ti->fn)(ti->data);
    ti->done = 1;
    return &ti->res;
}

void *sl_createthread(int (*fn)(void *), void *data, const char *name)
{
    sl_threadinfo *ti = new sl_threadinfo;
    ti->data = data;
    ti->fn = fn;
    ti->done = 0;
    pthread_create(&(ti->handle), NULL, sl_thread_indir, ti);
    if(name) pthread_setname_np(ti->handle, name);
    return (void *) ti;
}

int sl_waitthread(void *ti)
{
    void *res;
    pthread_join(((sl_threadinfo *)ti)->handle, &res);
    int ires = *((int *)res);
    delete (sl_threadinfo *) ti;
    return ires;
}

static vector<sl_threadinfo *> oldthreads;
static sl_semaphore sem_oldthreads(1, NULL);

void sl_detachthread(void *ti)
{
    if(ti) pthread_detach(((sl_threadinfo *)ti)->handle);
    if(ti || sem_oldthreads.getvalue() > 0)
    {
        sem_oldthreads.wait();
        if(ti) oldthreads.add((sl_threadinfo *)ti);
        loopvrev(oldthreads) if(oldthreads[i]->done) delete oldthreads.remove(i);
        sem_oldthreads.post();
    }
}

static pthread_t mainthreadid = pthread_self();
bool ismainthread() { return pthread_equal(mainthreadid, pthread_self()) != 0; }

#endif

bool sl_pollthread(void *ti)
{
    return ((sl_threadinfo*)ti)->done != 0;
}

// platform dependent stuff not covered by enet (use POSIX or, if possible, SDL)
#ifdef AC_USE_SDL_THREADS
void sl_sleep(int duration)
{
    SDL_Delay(duration);
}
#else
void sl_sleep(int duration)
{
    struct timespec t = { duration / 1000, (duration % 1000) * 1000000 };
    nanosleep(&t, NULL);
}
#endif

void parseupdatelist(hashtable<const char *, int> &ht, char *buf, const char *prefix, const char *suffix)
{
    for(char *d = buf; *d; d++) if(!isalnum(*d) && !strchr("._-() /\n", *d)) *d = ' '; // allowed chars in media path strings (except ' ')
    char *p, *l = buf, *m;
    int rev, *revp, plen = prefix ? (int)strlen(prefix) : 0, slen = suffix ? (int)strlen(suffix) : 0;
    do
    {
        if((p = strchr(l, '\n'))) *p = '\0'; // break into single lines
        l += strspn(l, " "); // skip leading spaces
        if((m = strchr(l, ' ')) && (rev = atoi(m + 1))) // string has a space and a number != 0 after it
        {
            *m = '\0';
            if((!plen || !strncmp(l, prefix, plen)) && // prefix is either not required or present
               (!slen || (m - l > slen && !strcmp(m - slen, suffix)))) // suffix is either not required or present
            {
                l += plen; // skip prefix
                m[-slen] = '\0'; // cut suffix
                revp = ht.access(l);
                if(revp) *revp = rev;
                else ht.access(newstring(l), rev);
            }
        }
        l = p + 1;
    }
    while(p);
}
