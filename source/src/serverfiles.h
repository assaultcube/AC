// serverfiles.h

// map management

#define SERVERMAP_PATH          "packages/maps/servermaps/"
#define SERVERMAP_PATH_BUILTIN  "packages/maps/official/"
#define SERVERMAP_PATH_INCOMING "packages/maps/servermaps/incoming/"

#define GZBUFSIZE ((MAXCFGFILESIZE * 11) / 10)

struct servermapbuffer  // sending of maps between clients
{
    string mapname;
    int cgzsize, cfgsize, cfgsizegz, revision, datasize;
    uchar *data, *gzbuf;

    servermapbuffer() : data(NULL) { gzbuf = new uchar[GZBUFSIZE]; }
    ~servermapbuffer() { delete[] gzbuf; }

    void clear() { DELETEA(data); revision = 0; }

    int available()
    {
        if(data && !strcmp(mapname, behindpath(smapname)) && cgzsize == smapstats.cgzsize)
        {
            if( !revision || (revision == smapstats.hdr.maprevision)) return cgzsize;
        }
        return 0;
    }

    void setrevision()
    {
        if(available() && !revision) revision = smapstats.hdr.maprevision;
    }

    void load(void)  // load currently played map into the buffer (if distributable), clear buffer otherwise
    {
        string cgzname, cfgname;
        const char *name = behindpath(smapname);   // no paths allowed here

        clear();
        formatstring(cgzname)(SERVERMAP_PATH "%s.cgz", name);
        path(cgzname);
        if(fileexists(cgzname, "r"))
        {
            formatstring(cfgname)(SERVERMAP_PATH "%s.cfg", name);
        }
        else
        {
            formatstring(cgzname)(SERVERMAP_PATH_INCOMING "%s.cgz", name);
            path(cgzname);
            formatstring(cfgname)(SERVERMAP_PATH_INCOMING "%s.cfg", name);
        }
        path(cfgname);
        uchar *cgzdata = (uchar *)loadfile(cgzname, &cgzsize);
        uchar *cfgdata = (uchar *)loadfile(cfgname, &cfgsize);
        if(cgzdata && (!cfgdata || cfgsize < MAXCFGFILESIZE))
        {
            uLongf gzbufsize = GZBUFSIZE;
            if(!cfgdata || compress2(gzbuf, &gzbufsize, cfgdata, cfgsize, 9) != Z_OK)
            {
                cfgsize = 0;
                gzbufsize = 0;
            }
            cfgsizegz = (int) gzbufsize;
            if(cgzsize + cfgsizegz < MAXMAPSENDSIZE)
            { // map is ok, fill buffer
                copystring(mapname, name);
                datasize = cgzsize + cfgsizegz;
                data = new uchar[datasize];
                memcpy(data, cgzdata, cgzsize);
                memcpy(data + cgzsize, gzbuf, cfgsizegz);
                logline(ACLOG_INFO,"loaded map %s, %d + %d(%d) bytes.", cgzname, cgzsize, cfgsize, cfgsizegz);
            }
        }
        DELETEA(cgzdata);
        DELETEA(cfgdata);
    }

    bool sendmap(const char *nmapname, int nmapsize, int ncfgsize, int ncfgsizegz, uchar *ndata)
    {
        FILE *fp;
        bool written = false;

        if(!nmapname[0] || nmapsize <= 0 || ncfgsizegz < 0 || nmapsize + ncfgsizegz > MAXMAPSENDSIZE || ncfgsize > MAXCFGFILESIZE) return false;  // malformed: probably modded client
        int cfgsize = ncfgsize;
        if(smode == GMODE_COOPEDIT && !strcmp(nmapname, behindpath(smapname)))
        { // update mapbuffer only in coopedit mode (and on same map)
            copystring(mapname, nmapname);
            datasize = nmapsize + ncfgsizegz;
            revision = 0;
            DELETEA(data);
            data = new uchar[datasize];
            memcpy(data, ndata, datasize);
        }

        defformatstring(name)(SERVERMAP_PATH_INCOMING "%s.cgz", nmapname);
        path(name);
        fp = fopen(name, "wb");
        if(fp)
        {
            fwrite(ndata, 1, nmapsize, fp);
            fclose(fp);
            formatstring(name)(SERVERMAP_PATH_INCOMING "%s.cfg", nmapname);
            path(name);
            fp = fopen(name, "wb");
            if(fp)
            {
                uLongf rawsize = ncfgsize;
                if(uncompress(gzbuf, &rawsize, ndata + nmapsize, ncfgsizegz) == Z_OK && rawsize - ncfgsize == 0)
                    fwrite(gzbuf, 1, cfgsize, fp);
                fclose(fp);
                written = true;
            }
        }
        return written;
    }

    void sendmap(client *cl, int chan)
    {
        if(!available()) return;
        packetbuf p(MAXTRANS + datasize, ENET_PACKET_FLAG_RELIABLE);
        putint(p, SV_RECVMAP);
        sendstring(mapname, p);
        putint(p, cgzsize);
        putint(p, cfgsize);
        putint(p, cfgsizegz);
        putint(p, revision);
        p.put(data, datasize);
        sendpacket(cl->clientnum, chan, p.finalize());
    }
};


// provide maps by the server

enum { MAP_NOTFOUND = 0, MAP_TEMP, MAP_CUSTOM, MAP_LOCAL, MAP_OFFICIAL, MAP_VOID };
static const char * const maplocstr[] = { "not found", "temporary", "custom", "local", "official", "void" };
#define readonlymap(x) ((x) >= MAP_CUSTOM)
#define distributablemap(x) ((x) == MAP_TEMP || (x) == MAP_CUSTOM)

int findmappath(const char *mapname, char *filename)
{
    if(!mapname[0]) return MAP_NOTFOUND;
    string tempname;
    if(!filename) filename = tempname;
    const char *name = behindpath(mapname);
    formatstring(filename)(SERVERMAP_PATH_BUILTIN "%s.cgz", name);
    path(filename);
    int loc = MAP_NOTFOUND;
    if(getfilesize(filename) > 10) loc = MAP_OFFICIAL;
    else
    {
#ifndef STANDALONE
        copystring(filename, setnames(name));
        if(!isdedicated && getfilesize(filename) > 10) loc = MAP_LOCAL;
        else
        {
#endif
            formatstring(filename)(SERVERMAP_PATH "%s.cgz", name);
            path(filename);
            if(isdedicated && getfilesize(filename) > 10) loc = MAP_CUSTOM;
            else
            {
                formatstring(filename)(SERVERMAP_PATH_INCOMING "%s.cgz", name);
                path(filename);
                if(isdedicated && getfilesize(filename) > 10) loc = MAP_TEMP;
            }
#ifndef STANDALONE
        }
#endif
    }
    return loc;
}

mapstats *getservermapstats(const char *mapname, bool getlayout, int *maploc)
{
    string filename;
    int ml;
    if(!maploc) maploc = &ml;
    *maploc = findmappath(mapname, filename);
    if(getlayout) DELETEA(maplayout);
    return *maploc == MAP_NOTFOUND ? NULL : loadmapstats(filename, getlayout);
}


// server config files

void serverconfigfile::init(const char *name)
{
    copystring(filename, name);
    path(filename);
    read();
}

bool serverconfigfile::load()
{
    DELETEA(buf);
    buf = loadfile(filename, &filelen);
    if(!buf)
    {
        logline(ACLOG_INFO,"could not read config file '%s'", filename);
        return false;
    }
    char *p;
    if('\r' != '\n') // this is not a joke!
    {
        char c = strchr(buf, '\n') ? ' ' : '\n'; // in files without /n substitute /r with /n, otherwise remove /r
        for(p = buf; (p = strchr(p, '\r')); p++) *p = c;
    }
    for(p = buf; (p = strstr(p, "//")); ) // remove comments
    {
        while(*p != '\n' && *p != '\0') p++[0] = ' ';
    }
    for(p = buf; (p = strchr(p, '\t')); p++) *p = ' ';
    for(p = buf; (p = strchr(p, '\n')); p++) *p = '\0'; // one string per line
    return true;
}

// maprot.cfg

#define CONFIG_MAXPAR 6

struct configset
{
    string mapname;
    union
    {
        struct { int mode, time, vote, minplayer, maxplayer, skiplines; };
        int par[CONFIG_MAXPAR];
    };
};

int FlagFlag = MINFF * 1000;
int Mvolume, Marea, SHhits, Mopen = 0;
float Mheight = 0;

bool mapisok(mapstats *ms)
{
    if ( Mheight > MAXMHEIGHT ) { logline(ACLOG_INFO, "MAP CHECK FAIL: The overall ceil height is too high (%.1f cubes)", Mheight); return false; }
    if ( Mopen > MAXMAREA ) { logline(ACLOG_INFO, "MAP CHECK FAIL: There is a big open area in this (hint: use more solid walls)", Mheight); return false; }
    if ( SHhits > MAXHHITS ) { logline(ACLOG_INFO, "MAP CHECK FAIL: Too high height in some parts of the map (%d hits)", SHhits); return false; }

    if ( ms->hasflags ) // Check if flags are ok
    {
        struct { short x, y; } fl[2];
        loopi(2)
        {
            if(ms->flags[i] == 1)
            {
                short *fe = ms->entposs + ms->flagents[i] * 3;
                fl[i].x = *fe; fe++; fl[i].y = *fe;
            }
            else fl[i].x = fl[i].y = 0; // the map has no valid flags
        }
        FlagFlag = pow2(fl[0].x - fl[1].x) + pow2(fl[0].y - fl[1].y);
    }
    else FlagFlag = MINFF * 1000; // the map has no flags

    if ( FlagFlag < MINFF ) { logline(ACLOG_INFO, "MAP CHECK FAIL: The flags are too close to each other"); return false; }

    for (int i = 0; i < ms->hdr.numents; i++)
    {
        int v = ms->enttypes[i];
        if (v < I_CLIPS || v > I_AKIMBO) continue;
        short *p = &ms->entposs[i*3];
        float density = 0, hdensity = 0;
        for(int j = 0; j < ms->hdr.numents; j++)
        {
            int w = ms->enttypes[j];
            if (w < I_CLIPS || w > I_AKIMBO || i == j) continue;
            short *q = &ms->entposs[j*3];
            float r2 = 0;
            loopk(3){ r2 += (p[k]-q[k])*(p[k]-q[k]); }
            if ( r2 == 0.0f ) { logline(ACLOG_INFO, "MAP CHECK FAIL: Items too close %s %s (%hd,%hd)", entnames[v], entnames[w],p[0],p[1]); return false; }
            r2 = 1/r2;
            if (r2 < 0.0025f) continue;
            if (w != v)
            {
                hdensity += r2;
                continue;
            }
            density += r2;
        }
/*        if (hdensity > 0.0f) { logline(ACLOG_INFO, "ITEM CHECK H %s %f", entnames[v], hdensity); }
        if (density > 0.0f) { logline(ACLOG_INFO, "ITEM CHECK D %s %f", entnames[v], density); }*/
        if ( hdensity > 0.5f ) { logline(ACLOG_INFO, "MAP CHECK FAIL: Items too close %s %.2f (%hd,%hd)", entnames[v],hdensity,p[0],p[1]); return false; }
        switch(v)
        {
#define LOGTHISSWITCH(X) if( density > X ) { logline(ACLOG_INFO, "MAP CHECK FAIL: Items too close %s %.2f (%hd,%hd)", entnames[v],density,p[0],p[1]); return false; }
            case I_CLIPS:
            case I_HEALTH: LOGTHISSWITCH(0.24f); break;
            case I_AMMO: LOGTHISSWITCH(0.04f); break;
            case I_HELMET: LOGTHISSWITCH(0.02f); break;
            case I_ARMOUR:
            case I_GRENADE:
            case I_AKIMBO: LOGTHISSWITCH(0.005f); break;
            default: break;
#undef LOGTHISSWITCH
        }
    }
    return true;
}

struct servermaprot : serverconfigfile
{
    vector<configset> configsets;
    int curcfgset;

    servermaprot() : curcfgset(-1) {}

    void read()
    {
        if(getfilesize(filename) == filelen) return;
        configsets.shrink(0);
        if(!load()) return;

        const char *sep = ": ";
        configset c;
        int i, line = 0;
        char *l, *p = buf;
        logline(ACLOG_VERBOSE,"reading map rotation '%s'", filename);
        while(p < buf + filelen)
        {
            l = p; p += strlen(p) + 1; line++;
            l = strtok(l, sep);
            if(l)
            {
                copystring(c.mapname, behindpath(l));
                for(i = 3; i < CONFIG_MAXPAR; i++) c.par[i] = 0;  // default values
                for(i = 0; i < CONFIG_MAXPAR; i++)
                {
                    if((l = strtok(NULL, sep)) != NULL) c.par[i] = atoi(l);
                    else break;
                }
                if(i > 2)
                {
                    configsets.add(c);
                    logline(ACLOG_VERBOSE," %s, %s, %d minutes, vote:%d, minplayer:%d, maxplayer:%d, skiplines:%d", c.mapname, modestr(c.mode, false), c.time, c.vote, c.minplayer, c.maxplayer, c.skiplines);
                }
                else logline(ACLOG_INFO," error in line %d, file %s", line, filename);
            }
        }
        DELETEA(buf);
        logline(ACLOG_INFO,"read %d map rotation entries from '%s'", configsets.length(), filename);
        return;
    }

    int next(bool notify = true, bool nochange = false) // load next maprotation set
    {
#ifndef STANDALONE
        if(!isdedicated)
        {
            defformatstring(nextmapalias)("nextmap_%s", getclientmap());
            const char *map = getalias(nextmapalias);     // look up map in the cycle
            startgame(map && notify ? map : getclientmap(), getclientmode(), -1, notify);
            return -1;
        }
#endif
        if(configsets.empty()) fatal("maprot unavailable");
        int n = numclients();
        int csl = configsets.length();
        int ccs = curcfgset;
        if(ccs >= 0 && ccs < csl) ccs += configsets[ccs].skiplines;
        configset *c = NULL;
        loopi(3 * csl + 1)
        {
            ccs++;
            if(ccs >= csl || ccs < 0) ccs = 0;
            c = &configsets[ccs];
            if((n >= c->minplayer || i >= csl) && (!c->maxplayer || n <= c->maxplayer || i >= 2 * csl))
            {
                mapstats *ms = NULL;
                if((ms = getservermapstats(c->mapname)) && mapisok(ms)) break;
                else logline(ACLOG_INFO, "maprot error: map '%s' %s", c->mapname, (ms ? "does not satisfy some basic requirements" : "not found"));
            }
            if(i >= 3 * csl) fatal("maprot unusable"); // not a single map in rotation can be found...
        }
        if(!nochange)
        {
            curcfgset = ccs;
            startgame(c->mapname, c->mode, c->time, notify);
        }
        return ccs;
    }

    void restart(bool notify = true) // restart current map
    {
#ifndef STANDALONE
        if(!isdedicated)
        {
            startgame(getclientmap(), getclientmode(), -1, notify);
            return;
        }
#endif
    startgame(smapname, smode, -1, notify);
    }

    configset *current() { return configsets.inrange(curcfgset) ? &configsets[curcfgset] : NULL; }
    configset *get(int ccs) { return configsets.inrange(ccs) ? &configsets[ccs] : NULL; }
    int get_next()
    {
        int ccs = curcfgset;
        while(!strcmp(configsets[curcfgset].mapname,configsets[ccs].mapname))
        {
            ccs++;
            if(!configsets.inrange(ccs)) ccs=0;
            if (ccs == curcfgset) break;
        }
        curcfgset = ccs;
        return ccs;
    }
};

// serverblacklist.cfg

struct serveripblacklist : serverconfigfile
{
    vector<iprange> ipranges;

    void read()
    {
        if(getfilesize(filename) == filelen) return;
        ipranges.shrink(0);
        if(!load()) return;

        iprange ir;
        int line = 0, errors = 0;
        char *l, *r, *p = buf;
        logline(ACLOG_VERBOSE,"reading ip blacklist '%s'", filename);
        while(p < buf + filelen)
        {
            l = p; p += strlen(p) + 1; line++;
            if((r = (char *) atoipr(l, &ir)))
            {
                ipranges.add(ir);
                l = r;
            }
            if(l[strspn(l, " ")])
            {
                for(int i = (int)strlen(l) - 1; i > 0 && l[i] == ' '; i--) l[i] = '\0';
                logline(ACLOG_INFO," error in line %d, file %s: ignored '%s'", line, filename, l);
                errors++;
            }
        }
        DELETEA(buf);
        ipranges.sort(cmpiprange);
        int orglength = ipranges.length();
        loopv(ipranges)
        {
            if(!i) continue;
            if(ipranges[i].ur <= ipranges[i - 1].ur)
            {
                if(ipranges[i].lr == ipranges[i - 1].lr && ipranges[i].ur == ipranges[i - 1].ur)
                    logline(ACLOG_VERBOSE," blacklist entry %s got dropped (double entry)", iprtoa(ipranges[i]));
                else
                    logline(ACLOG_VERBOSE," blacklist entry %s got dropped (already covered by %s)", iprtoa(ipranges[i]), iprtoa(ipranges[i - 1]));
                ipranges.remove(i--); continue;
            }
            if(ipranges[i].lr <= ipranges[i - 1].ur)
            {
                logline(ACLOG_VERBOSE," blacklist entries %s and %s are joined due to overlap", iprtoa(ipranges[i - 1]), iprtoa(ipranges[i]));
                ipranges[i - 1].ur = ipranges[i].ur;
                ipranges.remove(i--); continue;
            }
        }
        loopv(ipranges) logline(ACLOG_VERBOSE," %s", iprtoa(ipranges[i]));
        logline(ACLOG_INFO,"read %d (%d) blacklist entries from '%s', %d errors", ipranges.length(), orglength, filename, errors);
    }

    bool check(enet_uint32 ip) // ip: network byte order
    {
        iprange t;
        t.lr = ENET_NET_TO_HOST_32(ip); // blacklist uses host byte order
        t.ur = 0;
        return ipranges.search(&t, cmpipmatch) != NULL;
    }
};

// nicknameblacklist.cfg

#define MAXNICKFRAGMENTS 5
enum { NWL_UNLISTED = 0, NWL_PASS, NWL_PWDFAIL, NWL_IPFAIL };

struct servernickblacklist : serverconfigfile
{
    struct iprchain     { struct iprange ipr; const char *pwd; int next; };
    struct blackline    { int frag[MAXNICKFRAGMENTS]; bool ignorecase; int line; void clear() { loopi(MAXNICKFRAGMENTS) frag[i] = -1; } };
    hashtable<const char *, int> whitelist;
    vector<iprchain> whitelistranges;
    vector<blackline> blacklines;
    vector<const char *> blfraglist;

    void destroylists()
    {
        whitelistranges.setsize(0);
        enumeratek(whitelist, const char *, key, delete key);
        whitelist.clear(false);
        blfraglist.deletecontents();
        blacklines.setsize(0);
    }

    void read()
    {
        if(getfilesize(filename) == filelen) return;
        destroylists();
        if(!load()) return;

        const char *sep = " ";
        int line = 1, errors = 0;
        iprchain iprc;
        blackline bl;
        char *l, *s, *r, *p = buf;
        logline(ACLOG_VERBOSE,"reading nickname blacklist '%s'", filename);
        while(p < buf + filelen)
        {
            l = p; p += strlen(p) + 1;
            l = strtok(l, sep);
            if(l)
            {
                s = strtok(NULL, sep);
                int ic = 0;
                if(s && (!strcmp(l, "accept") || !strcmp(l, "a")))
                { // accept nickname IP-range
                    int *i = whitelist.access(s);
                    if(!i) i = &whitelist.access(newstring(s), -1);
                    s += strlen(s) + 1;
                    while(s < p)
                    {
                        r = (char *) atoipr(s, &iprc.ipr);
                        s += strspn(s, sep);
                        iprc.pwd = r && *s ? NULL : newstring(s, strcspn(s, sep));
                        if(r || *s)
                        {
                            iprc.next = *i;
                            *i = whitelistranges.length();
                            whitelistranges.add(iprc);
                            s = r ? r : s + strlen(iprc.pwd);
                        }
                        else break;
                    }
                    s = NULL;
                }
                else if(s && (!strcmp(l, "block") || !strcmp(l, "b") || ic++ || !strcmp(l, "blocki") || !strcmp(l, "bi")))
                { // block nickname fragments (ic == ignore case)
                    bl.clear();
                    loopi(MAXNICKFRAGMENTS)
                    {
                        if(ic) strtoupper(s);
                        loopvj(blfraglist)
                        {
                            if(!strcmp(s, blfraglist[j])) { bl.frag[i] = j; break; }
                        }
                        if(bl.frag[i] < 0)
                        {
                            bl.frag[i] = blfraglist.length();
                            blfraglist.add(newstring(s));
                        }
                        s = strtok(NULL, sep);
                        if(!s) break;
                    }
                    bl.ignorecase = ic > 0;
                    bl.line = line;
                    blacklines.add(bl);
                }
                else { logline(ACLOG_INFO," error in line %d, file %s: unknown keyword '%s'", line, filename, l); errors++; }
                if(s && s[strspn(s, " ")]) { logline(ACLOG_INFO," error in line %d, file %s: ignored '%s'", line, filename, s); errors++; }
            }
            line++;
        }
        DELETEA(buf);
        logline(ACLOG_VERBOSE," nickname whitelist (%d entries):", whitelist.numelems);
        string text;
        enumeratekt(whitelist, const char *, key, int, idx,
        {
            text[0] = '\0';
            for(int i = idx; i >= 0; i = whitelistranges[i].next)
            {
                iprchain &ic = whitelistranges[i];
                if(ic.pwd) concatformatstring(text, "  pwd:\"%s\"", hiddenpwd(ic.pwd));
                else concatformatstring(text, "  %s", iprtoa(ic.ipr));
            }
            logline(ACLOG_VERBOSE, "  accept %s%s", key, text);
        });
        logline(ACLOG_VERBOSE," nickname blacklist (%d entries):", blacklines.length());
        loopv(blacklines)
        {
            text[0] = '\0';
            loopj(MAXNICKFRAGMENTS)
            {
                int k = blacklines[i].frag[j];
                if(k >= 0) { concatstring(text, " "); concatstring(text, blfraglist[k]); }
            }
            logline(ACLOG_VERBOSE, "  %2d block%s%s", blacklines[i].line, blacklines[i].ignorecase ? "i" : "", text);
        }
        logline(ACLOG_INFO,"read %d + %d entries from nickname blacklist file '%s', %d errors", whitelist.numelems, blacklines.length(), filename, errors);
    }

    int checkwhitelist(const client &c)
    {
        if(c.type != ST_TCPIP) return NWL_PASS;
        iprange ipr;
        ipr.lr = ENET_NET_TO_HOST_32(c.peer->address.host); // blacklist uses host byte order
        int *idx = whitelist.access(c.name);
        if(!idx) return NWL_UNLISTED; // no matching entry
        int i = *idx;
        bool needipr = false, iprok = false, needpwd = false, pwdok = false;
        while(i >= 0)
        {
            iprchain &ic = whitelistranges[i];
            if(ic.pwd)
            { // check pwd
                needpwd = true;
                if(pwdok || !strcmp(genpwdhash(c.name, ic.pwd, c.salt), c.pwd)) pwdok = true;
            }
            else
            { // check IP
                needipr = true;
                if(!cmpipmatch(&ipr, &ic.ipr)) iprok = true; // range match found
            }
            i = whitelistranges[i].next;
        }
        if(needpwd && !pwdok) return NWL_PWDFAIL; // wrong PWD
        if(needipr && !iprok) return NWL_IPFAIL; // wrong IP
        return NWL_PASS;
    }

    int checkblacklist(const char *name)
    {
        if(blacklines.empty()) return -2;  // no nickname blacklist loaded
        string nameuc;
        copystring(nameuc, name);
        strtoupper(nameuc);
        loopv(blacklines)
        {
            loopj(MAXNICKFRAGMENTS)
            {
                int k = blacklines[i].frag[j];
                if(k < 0) return blacklines[i].line; // no more fragments to check
                if(strstr(blacklines[i].ignorecase ? nameuc : name, blfraglist[k]))
                {
                    if(j == MAXNICKFRAGMENTS - 1) return blacklines[i].line; // all fragments match
                }
                else break; // this line no match
            }
        }
        return -1; // no match
    }
};

#define FORBIDDENSIZE 15
struct serverforbiddenlist : serverconfigfile
{
    int num;
    char entries[100][2][FORBIDDENSIZE+1]; // 100 entries and 2 words per entry is more than enough

    void initlist()
    {
        num = 0;
        memset(entries,'\0',2*100*(FORBIDDENSIZE+1));
    }

    void addentry(char *s)
    {
        int len = strlen(s);
        if ( len > 128 || len < 3 ) return;
        int n = 0;
        string s1, s2;
        char *c1 = s1, *c2 = s2;
        if (num < 100 && (n = sscanf(s,"%s %s",s1,s2)) > 0 ) // no warnings
        {
            strncpy(entries[num][0],c1,FORBIDDENSIZE);
            if ( n > 1 ) strncpy(entries[num][1],c2,FORBIDDENSIZE);
            else entries[num][1][0]='\0';
            num++;
        }
    }

    void read()
    {
        if(getfilesize(filename) == filelen) return;
        initlist();
        if(!load()) return;

        char *l, *p = buf;
        logline(ACLOG_VERBOSE,"reading forbidden list '%s'", filename);
        while(p < buf + filelen)
        {
            l = p; p += strlen(p) + 1;
            addentry(l);
        }
        DELETEA(buf);
    }

    bool canspeech(char *s)
    {
        for (int i=0; i<num; i++){
            if ( !findpattern(s,entries[i][0]) ) continue;
            else if ( entries[i][1][0] == '\0' || findpattern(s,entries[i][1]) ) return false;
        }
        return true;
    }
};

// serverpwd.cfg

#define ADMINPWD_MAXPAR 1
struct pwddetail
{
    string pwd;
    int line;
    bool denyadmin;    // true: connect only
};

int passtime = 0, passtries = 0;
enet_uint32 passguy = 0;

struct serverpasswords : serverconfigfile
{
    vector<pwddetail> adminpwds;
    int staticpasses;

    serverpasswords() : staticpasses(0) {}

    void init(const char *name, const char *cmdlinepass)
    {
        if(cmdlinepass[0])
        {
            pwddetail c;
            copystring(c.pwd, cmdlinepass);
            c.line = 0;   // commandline is 'line 0'
            c.denyadmin = false;
            adminpwds.add(c);
        }
        staticpasses = adminpwds.length();
        serverconfigfile::init(name);
    }

    void read()
    {
        if(getfilesize(filename) == filelen) return;
        adminpwds.shrink(staticpasses);
        if(!load()) return;

        pwddetail c;
        const char *sep = " ";
        int i, line = 1, par[ADMINPWD_MAXPAR];
        char *l, *p = buf;
        logline(ACLOG_VERBOSE,"reading admin passwords '%s'", filename);
        while(p < buf + filelen)
        {
            l = p; p += strlen(p) + 1;
            l = strtok(l, sep);
            if(l)
            {
                copystring(c.pwd, l);
                par[0] = 0;  // default values
                for(i = 0; i < ADMINPWD_MAXPAR; i++)
                {
                    if((l = strtok(NULL, sep)) != NULL) par[i] = atoi(l);
                    else break;
                }
                //if(i > 0)
                {
                    c.line = line;
                    c.denyadmin = par[0] > 0;
                    adminpwds.add(c);
                    logline(ACLOG_VERBOSE,"line%4d: %s %d", c.line, hiddenpwd(c.pwd), c.denyadmin ? 1 : 0);
                }
            }
            line++;
        }
        DELETEA(buf);
        logline(ACLOG_INFO,"read %d admin passwords from '%s'", adminpwds.length() - staticpasses, filename);
    }

    bool check(const char *name, const char *pwd, int salt, pwddetail *detail = NULL, enet_uint32 address = 0)
    {
        bool found = false;
        if (address && passguy == address)
        {
            if (passtime + 3000 > servmillis || ( passtries > 5 && passtime + 10000 > servmillis ))
            {
                passtries++;
                passtime = servmillis;
                return false;
            }
            else
            {
                if ( passtime + 60000 < servmillis ) passtries = 0;
            }
            passtries++;
        }
        else
        {
            passtries = 0;
        }
        passguy = address;
        passtime = servmillis;
        loopv(adminpwds)
        {
            if(!strcmp(genpwdhash(name, adminpwds[i].pwd, salt), pwd))
            {
                if(detail) *detail = adminpwds[i];
                found = true;
                break;
            }
        }
        return found;
    }
};

// serverinfo_en.txt, motd_en.txt

#define MAXINFOLINELEN 100  // including color codes

struct serverinfofile
{
    struct serverinfotext { const char *type; char lang[3]; char *info; int lastcheck; };
    vector<serverinfotext> serverinfotexts;
    const char *infobase, *motdbase;

    void init(const char *info, const char *motd) { infobase = info; motdbase = motd; }

    char *readinfofile(const char *fnbase, const char *lang)
    {
        defformatstring(fname)("%s_%s.txt", fnbase, lang);
        path(fname);
        int len, n;
        char *c, *s, *t, *buf = loadfile(fname, &len);
        if(!buf) return NULL;
        char *nbuf = new char[len + 2];
        for(t = nbuf, s = strtok(buf, "\n\r"); s; s = strtok(NULL, "\n\r"))
        {
            c = strstr(s, "//");
            if(c) *c = '\0'; // strip comments
            for(n = (int)strlen(s) - 1; n >= 0 && s[n] == ' '; n--) s[n] = '\0'; // strip trailing blanks
            filterrichtext(t, s + strspn(s, " "), MAXINFOLINELEN); // skip leading blanks
            n = (int)strlen(t);
            if(n) t += n + 1;
        }
        *t = '\0';
        DELETEA(buf);
        if(!*nbuf) DELETEA(nbuf);
        logline(ACLOG_DEBUG,"read file \"%s\"", fname);
        return nbuf;
    }

    const char *getinfocache(const char *fnbase, const char *lang)
    {
        serverinfotext sn = { fnbase, { 0, 0, 0 }, NULL, 0} , *s = &sn;
        filterlang(sn.lang, lang);
        if(!sn.lang[0]) return NULL;
        loopv(serverinfotexts)
        {
            serverinfotext &si = serverinfotexts[i];
            if(si.type == s->type && !strcmp(si.lang, s->lang))
            {
                if(servmillis - si.lastcheck > (si.info ? 15 : 45) * 60 * 1000)
                { // re-read existing files after 15 minutes; search missing again after 45 minutes
                    DELETEA(si.info);
                    s = &si;
                }
                else return si.info;
            }
        }
        s->info = readinfofile(fnbase, lang);
        s->lastcheck = servmillis;
        if(s == &sn) serverinfotexts.add(*s);
        if(fnbase == motdbase && s->info)
        {
            char *c = s->info;
            while(*c) { c += strlen(c); if(c[1]) *c++ = '\n'; }
            if(strlen(s->info) > MAXSTRLEN) s->info[MAXSTRLEN] = '\0'; // keep MOTD at sane lengths
        }
        return s->info;
    }

    const char *getinfo(const char *lang)
    {
        return getinfocache(infobase, lang);
    }

    const char *getmotd(const char *lang)
    {
        const char *motd;
        if(*lang && (motd = getinfocache(motdbase, lang))) return motd;
        return getinfocache(motdbase, "en");
    }
};

struct killmessagesfile : serverconfigfile
{
    void init(const char *name) { serverconfigfile::init(name); }
    void read()
    {
        if(getfilesize(filename) == filelen) return;
        if(!load()) return;

        char *l, *s, *p = buf;
        const char *sep = " \"";
        int line = 0;
        logline(ACLOG_VERBOSE,"reading kill messages file '%s'", filename);
        while(p < buf + filelen)
        {
            l = p; p += strlen(p) + 1;
            l = strtok(l, sep);
            
            char *message;
            if(l)
            {
                s = strtok(NULL, sep);
                bool fragmsg = !strcmp(l, "fragmessage");
                bool gibmsg = !strcmp(l, "gibmessage");
                if(s && (fragmsg || gibmsg))
                {
                    int errors = 0;
                    int gun = atoi(s);
                    
                    s += strlen(s) + 1;
                    while(s[0] == ' ') s++;
                    int hasquotes = strspn(s, "\"");
                    s += hasquotes;
                    message = s;
                    const char *seps = "\" \n", *end = NULL;
                    char cursep;
                    while( (cursep = *seps++) != '\0')
                    {
                        if(cursep == '"' && !hasquotes) continue;
                        end = strchr(message, cursep);
                        if(end) break;
                    }
                    if(end) message[end-message] = '\0';
                    
                    if(gun < 0 || gun >= NUMGUNS)
                    {
                        logline(ACLOG_INFO, " error in line %i, invalid gun : %i", line, gun);
                        errors++;
                    }
                    if(strlen(message)>MAXKILLMSGLEN)
                    {
                        logline(ACLOG_INFO, " error in line %i, too long message : string length is %i, max. allowed is %i", line, strlen(message), MAXKILLMSGLEN);
                        errors++;
                    }
                    if(!errors)
                    {
                        if(fragmsg)
                        {
                            copystring(killmessages[0][gun], message);
                            logline(ACLOG_VERBOSE, " added msg '%s' for frags with weapon %i ", message, gun);
                        }
                        else
                        {
                            copystring(killmessages[1][gun], message);
                            logline(ACLOG_VERBOSE, " added msg '%s' for gibs with weapon %i ", message, gun);
                        }
                    }
                    s = NULL;
                    line++;
                }
            }
        }
    }
};
