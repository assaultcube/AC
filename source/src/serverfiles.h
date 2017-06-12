// serverfiles.h

// map management

#define SERVERMAXMAPFACTOR 10       // 10 is huge... if you're running low on RAM, consider 9 as maximum (don't go higher than 10, or players will name their ugly dog after you)

#define SERVERMAP_PATH_BUILTIN  "packages" PATHDIVS "maps" PATHDIVS "official" PATHDIVS
#define SERVERMAP_PATH          "packages" PATHDIVS "maps" PATHDIVS "servermaps" PATHDIVS
#define SERVERMAP_PATH_INCOMING "packages" PATHDIVS "maps" PATHDIVS "servermaps" PATHDIVS "incoming" PATHDIVS

const char *servermappath_off = SERVERMAP_PATH_BUILTIN;
const char *servermappath_serv = SERVERMAP_PATH;
const char *servermappath_incom = SERVERMAP_PATH_INCOMING;

#define GZBUFSIZE ((MAXCFGFILESIZE * 11) / 10)
#define FLOORPLANBUFSIZE  (sizeof(struct servsqr) << ((SERVERMAXMAPFACTOR) * 2))              // that's 4MB for size 10 maps (or 1 MB for size 9 maps)

stream *readmaplog = NULL;   // the readmaps thread always logs directly to file

struct servermap  // in-memory version of a map file on a server
{
    const char *fname, *fpath;      // map name and path (fname has to be first member of this struct! hardcoded!)
    uchar *cgzraw, *cfgrawgz;       // direct copies of the cgz and cfg files (cfg is already gzipped)
    uchar cgzhash[TIGERHASHSIZE], cfghash[TIGERHASHSIZE];
    int cgzlen, cfglen, cfggzlen;   // file lengths and cfg-gz length

    int version, headersize, sfactor, numents, maprevision, waterlevel;  // from map header

    uchar *layoutgz;                // gzipped precompiled floorplan
    int layoutlen, layoutgzlen;

    mapdim_s mapdims;
    entitystats_s entstats;
    mapareastats_s areastats;

    int x1, x2, y1, y2, zmin, zmax; // bounding box for player-reachable areas

    uchar *enttypes;                //             table of entity types
    short *entpos_x, *entpos_y;

    bool isok;                      // definitive flag!
    #ifdef _DEBUG
    char maptitle[129];
    #endif

    servermap(const char *mname, const char *mpath) { memset(&fname, 0, sizeof(struct servermap)); fname = newstring(mname); fpath = mpath; }
    ~servermap() { delstring(fname); DELETEA(cgzraw); DELETEA(cfgrawgz); DELETEA(enttypes); DELETEA(entpos_x); DELETEA(entpos_y); DELETEA(layoutgz); }

    bool isro() { return fpath == servermappath_off || fpath == servermappath_serv; }
    bool isofficial() { return fpath == servermappath_off; }

    int getmemusage() { return sizeof(struct servermap) + cgzlen + cfggzlen + layoutgzlen + numents * (sizeof(uchar) + sizeof(short) * 3); }

    void load(void)  // load map into memory and extract everything important about it  (assumes struct to be zeroed: can only be called once)
    {
        static uchar *staticbuffer = NULL;
        if(!staticbuffer) staticbuffer = new uchar[FLOORPLANBUFSIZE];     // this buffer gets reused for every map load several times (also: because of this, load() is not thread safe)

        const char *err = NULL;
        stream *f = NULL;
        int restofhead;

        // load map files, prepare sendmap buffer
        defformatstring(filename)("%s%s.cfg", fpath, fname);
        path(filename);
        uchar *cfgraw = (uchar *)loadfile(filename, &cfglen);
        if(cfgraw)
        {
            tigerhash(cfghash, cfgraw, cfglen);
            loopk(cfglen) if(cfgraw[k] > 0x7f || (cfgraw[k] < 0x20 && !isspace(cfgraw[k]))) err = "illegal chars in cfg file";
        }
        formatstring(filename)("%s%s.cgz", fpath, fname);
        path(filename);
        cgzraw = (uchar *)loadfile(filename, &cgzlen);
        if(cgzraw) tigerhash(cgzhash, cgzraw, cgzlen);
        if(!cgzraw) err = "loading cgz failed";
        else if(cfglen > MAXCFGFILESIZE) err = "cfg file too big";
        else if(cgzlen >= MAXMAPSENDSIZE) err = "cgz file too big";
        else if(cfgraw)
        {
            uLongf gzbufsize = GZBUFSIZE;
            ASSERT(GZBUFSIZE < FLOORPLANBUFSIZE);
            if(compress2(staticbuffer, &gzbufsize, cfgraw, cfglen, 9) != Z_OK) gzbufsize = 0;
            cfggzlen = (int) gzbufsize;
            if(cgzlen + cfggzlen < MAXMAPSENDSIZE)
            { // map is small enough to be sent
                cfgrawgz = new uchar[cfggzlen];
                memcpy(cfgrawgz, staticbuffer, cfggzlen);
            }
            else err = "cgz + cfg.gz too big to send";
        }
        DELETEA(cfgraw);
        if(err) goto loadfailed;

        // extract entity data and header info; compile map statistics; create floorplan
        {
            const int sizeof_header = sizeof(header), sizeof_baseheader = sizeof(header) - sizeof(int) * 16;
            f = opengzfile(filename, "rb");
            header *h = (header *)staticbuffer;
            if(!f) err = "can't open map file";
            else if(f->read(h, sizeof_baseheader) != sizeof_baseheader || (strncmp(h->head, "CUBE", 4) && strncmp(h->head, "ACMP",4))) err = "bad map file";
            if(err) goto loadfailed;

            lilswap(&h->version, 4); // version, headersize, sfactor, numents
            version = h->version;
            headersize = fixmapheadersize(h->version, h->headersize);
            sfactor = h->sfactor;
            numents = h->numents;
            #ifdef _DEBUG
            memcpy(maptitle, h->maptitle, 128);
            #endif
            restofhead = min(headersize, sizeof_header) - sizeof_baseheader;
            ASSERT(SERVERMAXMAPFACTOR <= LARGEST_FACTOR);
            if(version > MAPVERSION || numents > MAXENTITIES || sfactor < SMALLEST_FACTOR || sfactor > SERVERMAXMAPFACTOR ||
                f->read(&h->waterlevel, restofhead) != restofhead) err = "incompatible map file";
            else
            {
                lilswap(&h->maprevision, 4);  // maprevision, ambient, flags, timestamp
                maprevision = h->maprevision;
                lilswap(&h->waterlevel, 1);
                waterlevel = version >= 4 ? h->waterlevel : -100000;
                restofhead = clamp(headersize - sizeof_header, 0, MAXHEADEREXTRA);
                if(f->read(staticbuffer, restofhead) != restofhead) err = "map file truncated";
            }
        }
        if(err) goto loadfailed;

        // parse header extras
        {
            ucharbuf p(staticbuffer, restofhead);
            while(1)
            {
                int len = getuint(p), flags = getuint(p), type = flags & HX_TYPEMASK;
                if(p.overread() || len > p.remaining()) break;
                ucharbuf q(p.subbuf(len).buf, len);
                switch(type)
                {
                    case HX_EDITUNDO:
                        break;

                    case HX_CONFIG:
                        break;

                    case HX_MAPINFO:
                    case HX_MODEINFO:       // for new maprot...
                    case HX_ARTIST:
                    default:
                        break;
                }
            }
        }
        if(err) goto loadfailed;

        // read and convert map entities
        {
            bool oldentityformat = version < 10; // version < 10 have only 4 attributes and no scaling
            ASSERT(MAXENTITIES * sizeof(persistent_entity) < FLOORPLANBUFSIZE);
            persistent_entity *es = (persistent_entity *) staticbuffer;
            loopi(numents)
            {
                persistent_entity &e = es[i];
                f->read(&e, oldentityformat ? 12 : sizeof(persistent_entity));
                lilswap((short *)&e, 4);
                if(oldentityformat) e.attr5 = e.attr6 = e.attr7 = 0;
                else lilswap(&e.attr5, 1);
                #if 0
                if(e.type == LIGHT && e.attr1 >= 0)
                {
                    if(!e.attr2) e.attr2 = 255;
                    if(e.attr1 > 32) e.attr1 = 32;
                }
                #endif
                transformoldentitytypes(version, e.type);
                if(oldentityformat && e.type < MAXENTTYPES)
                {
                    if(e.type == CTF_FLAG || e.type == MAPMODEL) e.attr1 = e.attr1 + 7 - (e.attr1 + 7) % 15;  // round the angle to the nearest 15-degree-step, like old versions did during rendering
                    if(e.type == LIGHT && e.attr1 < 0) e.attr1 = 0; // negative lights had no meaning before version 10
                    int ov, ss;
                    #define SCALEATTR(x) \
                    if((ss = abs(entwraparound[e.type][x - 1] / entscale[e.type][x - 1]))) e.attr##x = (int(e.attr##x) % ss + ss) % ss; \
                    e.attr##x = ov = e.attr##x * entscale[e.type][x - 1]; \
                    if(ov != e.attr##x) err = "overflow during conversion of entity attribute";
                    SCALEATTR(1);
                    SCALEATTR(2);
                    SCALEATTR(3);
                    SCALEATTR(4);
                    #undef SCALEATTR
                }
            }
        }
        if(err) goto loadfailed;

        // convert and count entities for server use
        {
            persistent_entity *es = (persistent_entity *) staticbuffer;
            calcentitystats(entstats, es, numents);
            enttypes = new uchar[numents];  // FIXME: cut this down to useful entities
            entpos_x = new short[numents];
            entpos_y = new short[numents];
            loopi(numents)
            {
                persistent_entity &e = es[i];
                enttypes[i] = e.type >= MAXENTTYPES ? NOTUSED : e.type;
                entpos_x[i] = e.x;
                entpos_y[i] = e.y;
            }
        }
        if(err) goto loadfailed;

        // read full map geometry (without textures)
        {
            layoutlen = 1 << (sfactor * 2);
            servsqr *ss = (servsqr *)staticbuffer, *tt = NULL, *ee = ss + layoutlen;
            while(ss < ee && !err)
            {
                int type = f->getchar(), n;
                if(!tt && type > MAXTYPE) err = "map file broken";
                else switch(type)
                {
                    case -1:
                        err = "while reading map: unexpected eof";
                        break;

                    case 255:
                        n = f->getchar();
                        loopi(n) memcpy(ss++, tt, sizeof(servsqr));
                        ss--;
                        break;

                    case 254: // only in MAPVERSION<=2
                        memcpy(ss, tt, sizeof(servsqr));
                        f->getchar(); f->getchar();
                        break;

                    case SOLID:
                        ss->type = SOLID;
                        f->getchar();
                        ss->vdelta = f->getchar();
                        if(version <= 2) { f->getchar(); f->getchar(); }
                        break;

                    case 253: // SOLID with all textures during editing (undo)
                        err = "unoptimised map file not allowed on server";
                        break;

                    default:
                        if(type < 0 || type >= MAXTYPE) err = "illegal type";
                        else
                        {
                            ASSERT((int)MAXTYPE < (int)TAGTRIGGERMASK);
                            ss->type = type;
                            ss->floor = f->getchar();
                            ss->ceil = f->getchar();
                            if(ss->floor >= ss->ceil) ss->floor = ss->ceil - 1;  // for pre 12_13
                            f->getchar(); f->getchar(); f->getchar();
                            if(version <= 2) { f->getchar(); f->getchar(); }
                            ss->vdelta = f->getchar();
                            if(version >= 2) f->getchar();
                            if(version >= 5) ss->type |= TAGANYCLIP & f->getchar();
                        }
                        break;
                }
                tt = ss;
                ss++;
            }
        }
        if(err) goto loadfailed;

        // collect geometry stats (exactly the same as calculated by the client)
        {
            if(calcmapdims(mapdims, (servsqr *)staticbuffer, 1 << sfactor) < 0) err = "world geometry error";
        }
        if(err) goto loadfailed;

        // merge vdelta into floor & ceil
        {
            servsqr *ss = (servsqr *)staticbuffer;
            int linelen = 1 << sfactor, linegap = linelen - mapdims.xspan;
            ss += linelen * mapdims.y1 + mapdims.x1;
            for(int j = mapdims.yspan; j > 0; j--, ss += linegap) loopirev(mapdims.xspan)
            {
                int type = ss->type & TAGTRIGGERMASK;
                if(type == FHF || type == CHF)
                {
                    int diff = (ss->vdelta + ss[1].vdelta + ss[linelen].vdelta + ss[linelen + 1].vdelta + 15) / 16;  // round up to full cubes
                    if(type == FHF) ss->floor = max(-128, ss->floor - diff);
                    else ss->ceil = min(127, ss->ceil + diff);
                }
                ss++;
            }
        }
        if(err) goto loadfailed;

        // run mipmapper (to get corners right)


        // calculate area statistics from type & vdelta values (destroys vdelta!)
        {
            if(calcmapareastats(areastats, (servsqr *)staticbuffer, 1 << sfactor, mapdims) < 0) err = "world layout malformed"; // should be a quite fringe error
        }
        if(err) goto loadfailed;

        // work "player accessibility" into the floorplan, calculate map bounding box for player-accessible areas only
        {
            servsqr *ss = (servsqr *)staticbuffer;
            int linelen = 1 << sfactor, linegap = linelen - mapdims.xspan;
            ss += linelen * mapdims.y1 + mapdims.x1;
            x1 = y1 = linelen; zmin = 127; zmax = -128;
            for(int j = 0; j < mapdims.yspan; j++, ss += linegap) loopi(mapdims.xspan)
            {
                if((ss->type & TAGANYCLIP) > 0 || ss->ceil - ss->floor < 5) ss->type = SOLID;  // treat any cube that is not accessible to a player as SOLID
                if(!SOLID(ss))
                {
                    if(i < x1) x1 = i;
                    if(i > x2) x2 = i;
                    if(j < y1) y1 = j;
                    if(j > y2) y2 = j;
                    if(ss->floor < zmin) zmin = ss->floor;
                    if(ss->ceil > zmax) zmax = ss->ceil;
                }
                ss++;
            }
            x1 += mapdims.x1; x2 += mapdims.x1;
            y1 += mapdims.y1; y2 += mapdims.y1;

            if(x2 - x1 < 10 || y2 - y1 < 10 || zmax - zmin < 10) err = "world layout unplayable";

            x1 -= 2; x2 += 2; y1 -= 2; y2 += 2; // make sure, the bounding box is big enough
        }
        if(err) goto loadfailed;

        // create compact floorplan
        {
            char *layout = (char *)staticbuffer;
            servsqr *ss = (servsqr *)staticbuffer;
            loopirev(layoutlen)
            {
                switch(ss->type & TAGTRIGGERMASK)
                {
                    case SOLID:  *layout = 127;       break;
                    case CORNER: *layout = zmin;      break;    // basically: ignore corner areas (otherwise, we'd need a mipmapper to sort it out properly), maybe, this should be fixed at some point...
                    default:     *layout = ss->floor; break;
                }
                layout++; ss++;
            }
            ASSERT(layoutlen * 3 <= (int)FLOORPLANBUFSIZE);
            uLongf gzbufsize = layoutlen * 2;   // valid for sizeof(struct servsqr) >= 3
            if(compress2(staticbuffer + layoutlen, &gzbufsize, staticbuffer, layoutlen, 9) != Z_OK) gzbufsize = 0;
            layoutgzlen = (int) gzbufsize;
            if(layoutgzlen > 0 && layoutgzlen < layoutlen)
            { // gzipping went well -> keep it
                layoutgz = new uchar[layoutgzlen];
                memcpy(layoutgz, staticbuffer + layoutlen, layoutgzlen);
            }
            else err = "gzipping the floorplan failed";
        }

        loadfailed:
        DELETEP(f);
        if(err)
        {   // fail
            if(readmaplog) readmaplog->printf("reading map '%s%s' failed: %s.\n", fpath, fname, err);
        }
        else
        {   // success
            if(readmaplog) readmaplog->printf("read map '%s%s': cgz %d bytes, cfg %d bytes (%d gz), version %d, size %d, rev %d, "
                                              "ents %d, x %d:%d, y %d:%d, z %d:%d, layout %d bytes, spawns %d:%d:%d, flags %d:%d\n",
                                               fpath, fname, cgzlen, cfglen, cfggzlen, version, sfactor, maprevision,
                                               numents, x1, x2, y1, y2, zmin, zmax, layoutgzlen, entstats.spawns[0], entstats.spawns[1], entstats.spawns[2], entstats.flags[0], entstats.flags[1]);
            isok = true;
        }
    }
};

// data structures to sync data flow between main thread and readmapsthread
volatile servermap *servermapdropbox = NULL;     // changed servermap entry back to the main thread
volatile bool startnewservermapsepoch = false;    // signal readmapsthread to start an new full search
sl_semaphore *readmapsthread_sem = NULL;         // sync readmapsthread with main thread

// readmapsthread
//
// * checks all servermaps directories for map files, prioritises and reads them all into memory
// * compiles floorplans for every map and keeps them in memory (gzipped)
// * fetches extended attributes from the map header
// * basically extracts everything from the map file, that the server needs to run a game on it
// (may take a while, but we're not in a hurry)
//
// the thread processes one map at a time and waits for the main thread to store and enlist the findings
//

struct mapfilename { const char *fname, *fpath; int cgzlen, cfglen, epoch; }; // for tracking map/cfg file (-size) changes
vector<mapfilename> mapfilenames; // this list only grows

void updateservermap(int index, bool deletethis)
{
    mapfilename &m = mapfilenames[index];
    servermap *sm = new servermap(m.fname, m.fpath);
    if(!deletethis) sm->load();
    if(sm->isok)
    {
        m.cgzlen = sm->cgzlen;
        m.cfglen = sm->cfglen;
    }
    else m.cgzlen = m.cfglen = 0;

    // pipe sm to main thread....
    // we're handing a pointer to a new servermap to the main thread (who manages the array for all servermaps)
    // if the new servermap is not loaded properly, that's the signal for the main thread to delete the entry from the array
    while(servermapdropbox) readmapsthread_sem->wait(); // wait for the main thread to process the last sent servermap
    servermapdropbox = sm;
}

int getmapfilenameindex(const char *fname, const char *fpath)
{
    loopv(mapfilenames) if(fpath == mapfilenames[i].fpath && !strcmp(fname, mapfilenames[i].fname)) return i;
    return -1;
}

void trymapfiles(const char *fpath, const char *fname, int epoch)  //check, if map files were added or altered (detecting alteration requires filesizes to be changed as well, as usual)
{
    bool updatethis = false;
    int mapfileindex = getmapfilenameindex(fname, fpath);
    if(mapfileindex < 0)
    { // new map file
        mapfileindex = mapfilenames.length();
        mapfilename &m = mapfilenames.add();
        m.fname = newstring(fname);
        m.fpath = fpath;
        m.cgzlen = m.cfglen = 0;
        m.epoch = epoch;
        updatethis = true;
    }
    else
    {
        defformatstring(fcgz)("%s%s.cgz", fpath, fname);
        defformatstring(fcfg)("%s%s.cfg", fpath, fname);
        if(mapfilenames[mapfileindex].cgzlen != getfilesize(path(fcgz)) || mapfilenames[mapfileindex].cfglen != getfilesize(path(fcfg)))
        {
            updatethis = true;  // changed filesize detected
        }
    }
    if(updatethis) updateservermap(mapfileindex, false);
}

void tagmapfile(const char *fpath, const char *fname, int epoch)  // tag list entry, if file (+path) is already in it
{
    int mapfileindex = getmapfilenameindex(fname, fpath);
    if(mapfileindex >= 0) mapfilenames[mapfileindex].epoch = epoch;
}

int readmapsthread(void *logfileprefix)
{
    static int readmaps_epoch = 0;

    while(1)
    {
        while(!startnewservermapsepoch) readmapsthread_sem->wait();  // wait without using the cpu

        if(logfileprefix)
        {
            defformatstring(logfilename)("%sreadmaps_log.txt", (const char *)logfileprefix);
            path(logfilename);
            readmaplog = openfile(logfilename, "a");
        }

        vector<char *> maps_off, maps_serv, maps_incom;

        // get all map file names
        listfiles(servermappath_off, "cgz", maps_off, stringsort);
        listfiles(servermappath_serv, "cgz", maps_serv, stringsort);
        listfiles(servermappath_incom, "cgz", maps_incom, stringsort);
        if(readmaplog) readmaplog->printf("\n#### %s #### scanning all map directories... found %d official maps, %d servermaps and %d maps in 'incoming'\n", timestring(false), maps_off.length(), maps_serv.length(), maps_incom.length());

        // enforce priority: official > servermaps > incoming
        // (every map name is only allowed once among the paths)
        loopv(maps_off)
        {
            loopvjrev(maps_serv) if(!strcmp(maps_off[i], maps_serv[j])) delstring(maps_serv.remove(j));
            loopvjrev(maps_incom) if(!strcmp(maps_off[i], maps_incom[j])) delstring(maps_incom.remove(j));
        }
        loopv(maps_serv)
        {
            loopvjrev(maps_incom) if(!strcmp(maps_serv[i], maps_incom[j])) delstring(maps_incom.remove(j));
        }

        int lastepoch = readmaps_epoch;
        readmaps_epoch++;
        // tag all files in the list that we have found again in this round by giving it the new epoch number
        loopv(maps_off) tagmapfile(servermappath_off, maps_off[i], readmaps_epoch);
        loopv(maps_serv) tagmapfile(servermappath_serv, maps_serv[i], readmaps_epoch);
        loopv(maps_incom) tagmapfile(servermappath_incom, maps_incom[i], readmaps_epoch);

        // signal deletion of all files in the list that are no longer found to the main thread
        loopvrev(mapfilenames) if(mapfilenames[i].epoch == lastepoch) updateservermap(i, true);

        // process all currently available files: load new or changed files and pass them to the main thread
        loopv(maps_off) trymapfiles(servermappath_off, maps_off[i], readmaps_epoch);
        loopv(maps_serv) trymapfiles(servermappath_serv, maps_serv[i], readmaps_epoch);
        loopv(maps_incom) trymapfiles(servermappath_incom, maps_incom[i], readmaps_epoch);

        DELETEP(readmaplog);    // always close the logfile when done, so we can create a new one, if someone removed or renamed the old one
        startnewservermapsepoch = false;
    }
    return 0;
}








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
    if ( Mopen > MAXMAREA ) { logline(ACLOG_INFO, "MAP CHECK FAIL: There is a big open area in this (hint: use more solid walls)"); return false; }
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
        char *b, *l, *p = buf;
        logline(ACLOG_VERBOSE,"reading map rotation '%s'", filename);
        while(p < buf + filelen)
        {
            l = p; p += strlen(p) + 1; line++;
            l = strtok_r(l, sep, &b);
            if(l)
            {
                copystring(c.mapname, behindpath(l));
                for(i = 3; i < CONFIG_MAXPAR; i++) c.par[i] = 0;  // default values
                for(i = 0; i < CONFIG_MAXPAR; i++)
                {
                    if((l = strtok_r(NULL, sep, &b)) != NULL) c.par[i] = atoi(l);
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
        char *b, *l, *s, *r, *p = buf;
        logline(ACLOG_VERBOSE,"reading nickname blacklist '%s'", filename);
        while(p < buf + filelen)
        {
            l = p; p += strlen(p) + 1;
            l = strtok_r(l, sep, &b);
            if(l)
            {
                s = strtok_r(NULL, sep, &b);
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
                        s = strtok_r(NULL, sep, &b);
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

bool issimilar (char s, char d)
{
    s = tolower(s); d = tolower(d);
    if ( s == d ) return true;
    switch (d)
    {
        case 'a': if ( s == '@' || s == '4' ) return true; break;
        case 'c': if ( s == 'k' ) return true; break;
        case 'e': if ( s == '3' ) return true; break;
        case 'i': if ( s == '!' || s == '1' ) return true; break;
        case 'o': if ( s == '0' ) return true; break;
        case 's': if ( s == '$' || s == '5' ) return true; break;
        case 't': if ( s == '7' ) return true; break;
        case 'u': if ( s == '#' ) return true; break;
    }
    return false;
}

bool findpattern (char *s, char *d) // returns true if there is more than 80% of similarity
{
    int len, hit = 0;
    if (!d || (len = strlen(d)) < 1) return false;
    char *dp = d, *s_end = s + strlen(s);
    while (s != s_end)
    {
        if ( *s == ' ' )                                                         // spaces separate words
        {
            if ( !issimilar(*(s+1),*dp) ) { dp = d; hit = 0; }                   // d e t e c t  i t
        }
        else if ( issimilar(*s,*dp) ) { dp++; hit++; }                           // hit!
        else if ( hit > 0 )                                                      // this is not a pair, but there is a previous pattern
        {
            if (*s == '.' || *s == *(s-1) || issimilar(*(s+1),*dp) );            // separator or typo (do nothing)
            else if ( issimilar(*(s+1),*(dp+1)) || *s == '*' ) dp++;             // wild card or typo
            else hit--;                                                          // maybe this is nothing
        }
        else dp = d;                                                             // nothing here
        s++;                                  // walk on the string
        if ( hit && 5 * hit > 4 * len ) return true;                             // found it!
    }
    return false;
}

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
        char *b, *l, *p = buf;
        logline(ACLOG_VERBOSE,"reading admin passwords '%s'", filename);
        while(p < buf + filelen)
        {
            l = p; p += strlen(p) + 1;
            l = strtok_r(l, sep, &b);
            if(l)
            {
                copystring(c.pwd, l);
                par[0] = 0;  // default values
                for(i = 0; i < ADMINPWD_MAXPAR; i++)
                {
                    if((l = strtok_r(NULL, sep, &b)) != NULL) par[i] = atoi(l);
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
        char *c, *b, *s, *t, *buf = loadfile(fname, &len);
        if(!buf) return NULL;
        char *nbuf = new char[len + 2];
        for(t = nbuf, s = strtok_r(buf, "\n\r", &b); s; s = strtok_r(NULL, "\n\r", &b))
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

