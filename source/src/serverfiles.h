// serverfiles.h

// abuse globals to register server parameters
#ifdef _DEBUG
#define SERVPAR(name, min, cur, max, desc) int last_##name, name = addservparint(#name, min, cur, max, NULL, &name, &last_##name, NULL, false, true, desc)
#define SERVPARLIST(name, min, cur, max, list, desc) int last_##name, name = addservparint(#name, min, cur, max, list, &name, &last_##name, NULL, false, true, desc)
#define SERVPARF(name, min, cur, max, body, desc) extern int last_##name, name; void var_##name() { body; } int last_##name, name = addservparint(#name, min, cur, max, NULL,&name, &last_##name, var_##name, false, true, desc)
#define SERVSTR(name, cur, min, max, filt, desc) char last_##name[max+1], name[max+1]; bool __sdummy_##name = addservparstr(#name, min, max, filt, cur, name, last_##name, NULL, false, true, desc)
#else
#define SERVPAR(name, min, cur, max, desc) int last_##name, name = addservparint(#name, min, cur, max, NULL, &name, &last_##name, NULL, false, true, NULL)
#define SERVPARLIST(name, min, cur, max, list, desc) int last_##name, name = addservparint(#name, min, cur, max, list, &name, &last_##name, NULL, false, true, NULL)
#define SERVPARF(name, min, cur, max, body, desc) extern int last_##name, name; void var_##name() { body; } int last_##name, name = addservparint(#name, min, cur, max, NULL, &name, &last_##name, var_##name, false, true, NULL)
#define SERVSTR(name, cur, min, max, filt, desc) char last_##name[max+1], name[max+1]; bool __sdummy_##name = addservparstr(#name, min, max, filt, cur, name, last_##name, NULL, false, true, NULL)
#endif

// map management

#define SERVERMAXMAPFACTOR 10       // 10 is huge... if you're running low on RAM, consider 9 as maximum (don't go higher than 10, or players will name their ugly dog after you)

#define SERVERMAP_PATH_BUILTIN  "packages" PATHDIVS "maps" PATHDIVS "official" PATHDIVS
#define SERVERMAP_PATH_LOCAL    "packages" PATHDIVS "maps" PATHDIVS
#define SERVERMAP_PATH          "packages" PATHDIVS "maps" PATHDIVS "servermaps" PATHDIVS
#define SERVERMAP_PATH_INCOMING "packages" PATHDIVS "maps" PATHDIVS "servermaps" PATHDIVS "incoming" PATHDIVS

const char *servermappath_off = SERVERMAP_PATH_BUILTIN;
const char *servermappath_local = SERVERMAP_PATH_LOCAL;
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

    const char *err;
    bool isok;                      // definitive flag!
    #ifdef _DEBUG
    char maptitle[129];
    #endif

    servermap(const char *mname, const char *mpath) { memset(&fname, 0, sizeof(struct servermap)); fname = newstring(mname); fpath = mpath; }
    ~servermap() { delstring(fname); DELETEA(cgzraw); DELETEA(cfgrawgz); DELETEA(enttypes); DELETEA(entpos_x); DELETEA(entpos_y); DELETEA(layoutgz); }

    bool isro() { return fpath == servermappath_off || fpath == servermappath_serv; }
    bool isofficial() { return fpath == servermappath_off; }
    bool isdistributable() { return fpath == servermappath_serv || fpath == servermappath_incom; }

    int getmemusage() { return sizeof(struct servermap) + cgzlen + cfggzlen + layoutgzlen + numents * (sizeof(uchar) + sizeof(short) * 3); }

    char *getlayout() // uncompress prefabricated floorplan
    {
        if(isok && layoutgz && layoutgzlen > 0 && layoutgzlen < layoutlen)
        {
            char *lo = new char[layoutlen];
            uLongf rawsize = layoutlen;
            if(lo && uncompress((Bytef*)lo, &rawsize, layoutgz, layoutgzlen) == Z_OK && rawsize - layoutlen == 0) return lo;
            delete[] lo;
        }
        return NULL;
    }

    const char *getpathdesc()
    {
        if(fpath == servermappath_off) return "official";
        if(fpath == servermappath_local) return "local";
        if(fpath == servermappath_serv) return "custom";
        if(fpath == servermappath_incom) return "temporary";
        ASSERT(0); return "unknown";
    }

    void load(void)  // load map into memory and extract everything important about it  (assumes struct to be zeroed: can only be called once)
    {
        static uchar *staticbuffer = NULL;
        if(!staticbuffer) staticbuffer = new uchar[FLOORPLANBUFSIZE];     // this buffer gets reused for every map load several times (also: because of this, load() is not thread safe)

        stream *f = NULL;
        int restofhead;
        string filename;

        if(!validmapname(fname)) err = "illegal filename";
        if(err) goto loadfailed;

        // load map files, prepare sendmap buffer
        {
            formatstring(filename)("%s%s.cfg", fpath, fname);
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
        }
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
        // TODO :)


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

int readmapsthread(void *logfilename)
{
    static int readmaps_epoch = 0;

    while(1)
    {
        while(!startnewservermapsepoch) readmapsthread_sem->wait();  // wait without using the cpu

        if(logfilename) readmaplog = openfile((const char *)logfilename, "a");

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

#ifndef STANDALONE
const char *checklocalmap(const char *mapname) // check, if a map file exists in packages/maps/official or packages/maps - return proper servermap-path, if found
{
    const char **fn = setnames(behindpath(mapname)), *locs[2] = { servermappath_off, servermappath_local };
    int l = 0;
    if(!isdedicated && (getfilesize(fn[l]) > 10 || getfilesize(fn[++l]) > 10)) return locs[l];
    return NULL;
}
#endif

bool serverwritemap(const char *mapname, int mapsize, int cfgsize, int cfgsizegz, uchar *data)
{
    FILE *fp;
    bool written = false;
    if(!mapname[0] || mapsize <= 0 || cfgsizegz < 0 || mapsize + cfgsizegz > MAXMAPSENDSIZE || cfgsize > MAXCFGFILESIZE) return false;  // malformed: probably modded client
    defformatstring(name)(SERVERMAP_PATH_INCOMING "%s.cgz", mapname);
    path(name);
    fp = fopen(name, "wb");
    if(fp)
    {
        fwrite(data, 1, mapsize, fp);
        fclose(fp);
        formatstring(name)(SERVERMAP_PATH_INCOMING "%s.cfg", mapname);
        path(name);
        fp = fopen(name, "wb");
        if(fp)
        {
            uLongf rawsize = cfgsize;
            uchar *gzbuf = new uchar[cfgsize];
            if(uncompress(gzbuf, &rawsize, data + mapsize, cfgsizegz) == Z_OK && rawsize - cfgsize == 0)
                fwrite(gzbuf, 1, cfgsize, fp);
            fclose(fp);
            delete[] gzbuf;
            written = true;
        }
    }
    return written;
}

// server config files

vector<serverconfigfile *> serverconfigs;                 // list of server config files, automatically polled in the background
volatile bool recheckallserverconfigs = false;            // signal readserverconfigsthread to poll all files
sl_semaphore *readserverconfigsthread_sem = NULL;         // sync readmapsthread with main thread

void serverconfigfile::init(const char *name, bool tracking)
{
    copystring(filename, name);
    path(filename);
    clear();
    if(tracking) serverconfigs.add(this);
}

bool serverconfigfile::load()
{
    DELETEA(buf);
    buf = loadfile(filename, &filelen);
    if((readfailed = !buf))
    {
        logline(ACLOG_INFO,"could not read config file '%s'", filename);
        return false;
    }
    entropy_add_block((const uchar *)buf, filelen);
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

bool serverconfigfile::isbusy()
{
    return busy.getvalue() < 1;
}

void serverconfigfile::trypreload() // in readserverconfigsthread: load changed files into RAM
{
    if(!buf && !isbusy())
    {
        int newfilelen;
        bool contentchanged = false;
        char *newbuf = loadfile(filename, &newfilelen);
        readfailed = !newbuf;
        if(newbuf)
        {
            uint32_t newhash;
            uchar *p = (uchar*)newbuf;
            fnv1a_init(newhash);
            loopirev(newfilelen) fnv1a_add(newhash, *p++);
            if(newhash != filehash)
            {
                contentchanged = true;
                filehash = newhash;
            }
            delete newbuf;
        }
        if(contentchanged) load();
    }
}

void serverconfigfile::process() // in main thread: refill data structures with preloaded file content
{
    updated = false;
    if((readfailed || buf) && !isbusy() && !busy.trywait())
    {
        clear();
        if(buf && strlen(buf)) read();
        DELETEA(buf);
        busy.post();
        updated = true;
    }
}

int readserverconfigsthread(void *data)
{
    while(1)
    {
        while(!recheckallserverconfigs) readserverconfigsthread_sem->wait();  // wait without using the cpu

        loopv(serverconfigs) serverconfigs[i]->trypreload();

        entropy_save();
        recheckallserverconfigs = false;
    }
    return 0;
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

servermap *getservermap(const char *mapname);

struct servermaprot : serverconfigfile
{
    vector<configset> configsets;
    int curcfgset;

    servermaprot() : curcfgset(-1) {}

    void clear()
    {
        configsets.shrink(0);
    }

    void read()
    {
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
        logline(ACLOG_INFO,"read %d map rotation entries from '%s'", configsets.length(), filename);
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
                if(getservermap(c->mapname)) break;
                else logline(ACLOG_INFO, "maprot error: map '%s' not found", c->mapname);
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
    startgame("ac_desert", 0, -1, notify);
    }

    configset *current() { return configsets.inrange(curcfgset) ? &configsets[curcfgset] : NULL; }
    configset *get(int ccs) { return configsets.inrange(ccs) ? &configsets[ccs] : NULL; }
};

// generic commented IP list

struct serveripcclist : serverconfigfile
{
    vector<iprangecc> ipranges;

    bool parsecomment(iprangecc &ipr, const char *l)
    {
        memset(ipr.cc, 0, 4);
        l += strspn(l, " ");
        loopi(3) if(isprint(l[i])) ipr.cc[i] = l[i]; else break;
        return ipr.cc[0] && ipr.cc[1];
    }

    const char *printcomment(iprangecc &ipr)
    {
        return ipr.cc[3] == '\0' ? ipr.cc : "ERR";
    }

    int cmpcomment(iprangecc &ipr1, iprangecc &ipr2)
    {
        return ipr1.ci - ipr2.ci;
    }

    void clear()
    {
        ipranges.shrink(0);
    }

    void read()
    {
        iprangecc ir;
        int line = 0, errors = 0, concatd = 0;
        char *l, *r, *p = buf;
        logline(ACLOG_VERBOSE,"reading ip list '%s'", filename);
        while(p < buf + filelen)
        {
            l = p + strspn(p, " "); p += strlen(p) + 1; line++;
            if(!l[0]) continue;
            if((r = (char *) atoipr(l, &ir)) && parsecomment(ir, r))
            {
                if(ipranges.length() && ipranges.last().ur == ir.lr - 1 && !cmpcomment(ipranges.last(), ir))
                {
                    ipranges.last().ur = ir.ur; // direct concatenation (for pre-sorted lists)
                    concatd++;
                }
                else
                    ipranges.add(ir);
            }
            else
                logline(ACLOG_INFO," error in line %d, file %s: failed to parse '%s'", line, filename, r ? r : l), errors++;
        }
        ipranges.sort(cmpiprange);
        int orglength = ipranges.length();
        loopv(ipranges)
        { // make sure, ranges don't overlap - otherwise bsearch gets unpredictable
            if(!i) continue;
            if(ipranges[i].ur <= ipranges[i - 1].ur)
            {
                if(cmpcomment(ipranges[i], ipranges[i - 1]))
                    logline(ACLOG_INFO," error: IP list entry %s|%s deleted because of overlapping %s|%s", iprtoa(ipranges[i]), printcomment(ipranges[i]), iprtoa(ipranges[i - 1]), printcomment(ipranges[i - 1]));
                else
                {
                    if(ipranges[i].lr == ipranges[i - 1].lr && ipranges[i].ur == ipranges[i - 1].ur)
                        logline(ACLOG_VERBOSE," IP list entry %s got dropped (double entry)", iprtoa(ipranges[i]));
                    else
                        logline(ACLOG_VERBOSE," IP list entry %s got dropped (already covered by %s)", iprtoa(ipranges[i]), iprtoa(ipranges[i - 1]));
                }
                ipranges.remove(i--); continue;
            }
            if(ipranges[i].lr <= ipranges[i - 1].ur)
            {
                if(!cmpcomment(ipranges[i], ipranges[i - 1])) // same comment
                {
                    logline(ACLOG_VERBOSE," IP list entries %s and %s are joined due to overlap (both %s)", iprtoa(ipranges[i - 1]), iprtoa(ipranges[i]), printcomment(ipranges[i]));
                    ipranges[i - 1].ur = ipranges[i].ur;
                    ipranges.remove(i--); continue;
                }
                else
                {
                    logline(ACLOG_INFO," error: IP list entries %s|%s and %s|%s are overlapping - dropping %s|%s", iprtoa(ipranges[i - 1]), printcomment(ipranges[i - 1]),
                         iprtoa(ipranges[i]), printcomment(ipranges[i]), iprtoa(ipranges[i]), printcomment(ipranges[i]));
                    errors++;
                    ipranges.remove(i--); continue;
                }
            }
            if(ipranges[i].lr - 1 == ipranges[i - 1].ur && !cmpcomment(ipranges[i], ipranges[i - 1]))
            {
                logline(ACLOG_VERBOSE," concatenating IP list entries %s and %s (both %s)", iprtoa(ipranges[i - 1]), iprtoa(ipranges[i]), printcomment(ipranges[i]));
                ipranges[i - 1].ur = ipranges[i].ur;
                ipranges.remove(i--); continue;
            }
        }
        if(logcheck(ACLOG_VERBOSE)) loopv(ipranges) logline(ACLOG_VERBOSE," %s %s", iprtoa(ipranges[i]), printcomment(ipranges[i]));
        logline(ACLOG_INFO,"read %d (%d) IP list entries from '%s', %d errors, %d concatenated", ipranges.length(), orglength, filename, errors, concatd);
    }

    const char *check(enet_uint32 ip, bool lock = false) // ip: host byte order
    {
        if(lock) busy.wait();
        iprangecc t, *res;
        t.lr = ip;
        t.ur = 0;
        res = ipranges.search(&t, cmpipmatch);
        const char *cc = res ? printcomment(*res) : NULL;
        if(lock) busy.post();
        return cc;
    }
};

// serverblacklist.cfg

struct serveripblacklist : serverconfigfile
{
    vector<iprange> ipranges;

    void clear()
    {
        ipranges.shrink(0);
    }

    void read()
    {
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
        if(logcheck(ACLOG_VERBOSE)) loopv(ipranges) logline(ACLOG_VERBOSE," %s", iprtoa(ipranges[i]));
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

    void clear()
    {
        whitelistranges.setsize(0);
        enumeratek(whitelist, const char *, key, delete key);
        whitelist.clear(false);
        blfraglist.deletecontents();
        blacklines.setsize(0);
    }

    void read()
    {
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
        if(logcheck(ACLOG_VERBOSE))
        {
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

    void clear()
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
        char *l, *p = buf;
        logline(ACLOG_VERBOSE,"reading forbidden list '%s'", filename);
        while(p < buf + filelen)
        {
            l = p; p += strlen(p) + 1;
            addentry(l);
        }
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

    void clear()
    {
        adminpwds.shrink(staticpasses);
    }

    void read()
    {
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

struct serverinfofile : serverconfigfile  // plaintext info file, used for serverinfo and motd
{
    vector<char> msg;
    int maxlen;

    void init(const char *fnbase, int ml)
    {
        maxlen = ml;
        defformatstring(fname)("%s_en.txt", fnbase);
        serverconfigfile::init(fname);
    }

    void clear()
    {
        msg.shrink(0);
    }

    void read()
    {
        string s;
        char *l, *p = buf;
        logline(ACLOG_VERBOSE,"reading info text file '%s'", filename);
        while(p < buf + filelen)
        {
            l = p; p += strlen(p) + 1;
            ASSERT(MAXINFOLINELEN < MAXSTRLEN);
            filtertext(s, l, FTXT__SERVERINFOLINE, MAXINFOLINELEN);
            if(*s) cvecprintf(msg, "%s\n", s);
        }
        if(msg.length() > maxlen)
        {
            logline(ACLOG_VERBOSE,"info text file '%s' truncated: %d -> %d", filename, msg.length(), maxlen);
            msg.shrink(maxlen);
        }
    }

    const char *getmsg()
    {
        if(!msg.length() || msg.last()) msg.add('\0'); // make sure, msg is terminated
        return msg.getbuf();
    }

    char *getmsgline(string s, int *pos)
    {
        s[0] = '\0';
        if(msg.inrange(*pos))
        {
            const char *p = getmsg() + *pos;
            int len = strcspn(p, "\n") + 1;
            copystring(s, p, len);
            *pos += len;
        }
        return s;
    }
};

// realtime server parameters

enum { SID_INT, SID_STR };

struct servpar
{
    int type;           // one of SPAR_* above
    const char *name;
    union
    {
        int minval;    // SID_INT
        int minlen;    // SID_STR
    };
    union
    {
        int maxval;    // SID_INT
        int maxlen;    // SID_STR
    };
    union
    {
        int *i;        // SID_INT
        char *s;       // SID_STR
    };
    union
    {
        int *shadow_i;  // SID_INT
        char *shadow_s; // SID_STR
    };
    void (*fun)();
    union
    {
        const char **list; // SID_INT
        const char *defaultstr;  // SID_STR
    };
    union
    {
        int defaultint; // SID_INT
        int filter;     // SID_STR
    };
#ifdef _DEBUG
    const char *desc;
    int chapter;
#endif
    bool log;           // log value changes (careful!)
    bool fromfile;      // true: value can be set by config file, false: value is set by server

    servpar() {}

    // SID_INT
    servpar(int type, const char *name, int minval, int maxval, const char *list[], int *i, int *shadow_i, int defval, void (*fun)(), bool log, bool fromfile, const char *_desc)
        : type(type), name(name), minval(minval), maxval(maxval), i(i), shadow_i(shadow_i), fun(fun), list(list), defaultint(defval), log(log), fromfile(fromfile)
        { DEBUGCODE(desc = _desc); }

    // SID_STR
    servpar(int type, const char *name, int minlen, int maxlen, int filter, char *s, char *shadow_s, const char *defaultstr, void (*fun)(), bool log, int fromfile, const char *_desc)
        : type(type), name(name), minlen(minlen), maxlen(maxlen), s(s), shadow_s(shadow_s), fun(fun), defaultstr(defaultstr), filter(filter), log(log), fromfile(fromfile)
        { DEBUGCODE(desc = _desc;) }
};

hashtable<const char *, servpar> *servpars = NULL;

int addservparint(const char *name, int minval, int cur, int maxval, const char *list[], int *storage, int *shadowstorage, void (*fun)(), bool log, bool fromfile, const char *desc)
{
    if(!servpars) servpars = new hashtable<const char *, servpar>;
    if(list && minval == maxval) while(list[maxval - minval + 1][0]) maxval++; // get list length
    ASSERT(cur >= minval && cur <= maxval);
    servpar v(SID_INT, name, minval, maxval, list, storage, shadowstorage, cur, fun, log, fromfile, desc);
    servpars->access(name, v);
    *shadowstorage = cur;
    return cur;
}

bool addservparstr(const char *name, int minlen, int maxlen, int filt, const char *cur, char *storage, char *shadowstorage, void (*fun)(), bool log, bool fromfile, const char *desc)
{
    if(!servpars) servpars = new hashtable<const char *, servpar>;
    ASSERT(maxlen > minlen && minlen >= 0 && int(strlen(cur)) >= minlen && int(strlen(cur)) <= maxlen);
    servpar v(SID_STR, name, minlen, maxlen, filt, storage, shadowstorage, cur, fun, log, fromfile, desc);
    servpars->access(name, v);
    strncpy(storage, cur, maxlen + 1);
    strncpy(shadowstorage, cur, maxlen + 1);
    storage[maxlen] = shadowstorage[maxlen] = '\0';
    return true;
}

struct serverparameter : serverconfigfile
{
    bool load()
    {
        if(serverconfigfile::load())
        {
            int line = 0, idx;
            char *b, *l, *p = buf, *kb, *k, *v;
            servpar *id;
            while(p < buf + filelen)
            {
                l = p; p += strlen(p) + 1; line++;
                l = strtok_r(l, " ", &b);
                if(l && (k = strtok_r(l, ":", &kb)) && (v = strtok_r(NULL, ":", &kb)) && (id = servpars->access(k)) && id->fromfile) switch(id->type)
                {
                    case SID_INT:
                    {
                        int i = ATOI(v);
                        if(id->list && (idx = getlistindex(v, id->list, false, -1)) >= 0) i = idx + id->minval;  // multiple choice
                        if(i >= id->minval && i <= id->maxval) *(id->shadow_i) = i;
                        break;
                    }
                    case SID_STR:
                    {
                        if((v - k) - strlen(k) > 1) filterrichtext(v, v);
                        filtertext(v, v, id->filter);
                        int i = (int)strlen(v);
                        if(i >= id->minlen && i <= id->maxlen) strncpy(id->shadow_s, v, id->maxlen + 1);
                        break;
                    }
                }
            }
            return true;
        }
        return false;
    }

    void read()
    { // in mainloop: update values from shadow values
        enumerate(*servpars, servpar, id,
            switch(id.type)
            {
                case SID_INT:
                {
                    if(*id.i != *id.shadow_i)
                    {
                        if(id.fun) ((void (__cdecl *)())id.fun)();
                        *id.i = *id.shadow_i;
                    }
                    break;
                }
                case SID_STR:
                {
                    if(strcmp(id.s, id.shadow_s))
                    {
                        if(id.fun) ((void (__cdecl *)())id.fun)();
                        strncpy(id.s, id.shadow_s, id.maxlen + 1);
                    }
                    break;
                }
            }
        );
    }

    void dump(stream *f)
    {
        enumerate(*servpars, servpar, id,
            switch(id.type)
            {
                case SID_INT: f->printf("%s:%d // default %d\n", id.name, *id.i, id.defaultint); break;
                case SID_STR: f->printf("%s:%s // default %s\n", id.name, id.s, id.defaultstr); break;
            }
        );
    }
};

#ifdef _DEBUG
int siddocsort(servpar **a, servpar **b) { return (*a)->chapter == (*b)->chapter ? strcmp((*a)->name, (*b)->name) : (*a)->chapter - (*b)->chapter; }

const char *siddocchapters[] = { "dDebug switches", "mMisc settings", "vVote settings", "" };
const char *siddocchaptersorting = "vmd";

void serverparameters_dumpdocu(char *fname)
{
    vector<servpar *> sp;
    enumerate(*servpars, servpar, id, if(id.fromfile && id.desc && id.desc[0]) sp.add(&id));
    loopv(sp)
    {
        const char *e = strchr(siddocchaptersorting, sp[i]->desc[0]);
        sp[i]->chapter = e ? (int) (e - siddocchaptersorting) : sp[i]->desc[0];
    }
    sp.sort(siddocsort);
    stream *f = openfile(path(fname), "w");
    int lastchap = 0;
    if(f)
    {
        f->printf("// serverparameters.cfg\n//\n// parameters from this file are read once per minute by the server\n// and are effective immediately\n\n"
                  "// each setting consists of a keyword and a value, written like this: keyword:value\n// there are no spaces allowed between keyword and value\n"
                  "// using two colons (keyword::value) activates rich-text filtering for the value\n\n\n");
        loopv(sp)
        {
            servpar &id = *sp[i];
            if(lastchap != id.desc[0])
            {
                int ch = -1;
                for(int k = 0; siddocchapters[k][0]; k++) if(siddocchapters[k][0] == id.desc[0]) ch = k;
                lastchap = id.desc[0];
                if(ch < 0) f->printf("\n// **** untitled chapter %c ****\n\n", lastchap);
                else f->printf("\n// **** %s ****\n\n", siddocchapters[ch] + 1);
            }
            switch(id.type)
            {
                case SID_INT:
                {
                    f->printf("// %s    %s\n//   integer [%d..%d], default %d", id.name, id.desc + 1, id.minval, id.maxval, id.defaultint);
                    if(id.list)
                    {
                        for(int k = 0; id.list[k] && id.list[k][0]; k++) f->printf(", %d:%s", id.minval + k, id.list[k]);
                    }
                    f->printf("\n%s:%d\n\n", id.name, id.defaultint);
                    break;
                }
                case SID_STR:
                {
                    f->printf("// %s    %s\n//   string [%d..%d chars], default \"%s\"\n%s:%s\n\n", id.name, id.desc + 1, id.minlen, id.maxlen, id.defaultstr, id.name, id.defaultstr);
                    break;
                }
            }
        }
        f->printf("\n\n");
        delete f;
    }
}
#endif


