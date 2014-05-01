// worldio.cpp: loading & saving of maps and savegames

#include "cube.h"

#define DEBUGCOND (worldiodebug==1)
VARP(worldiodebug, 0, 0, 1);

void backup(char *name, char *backupname)
{
    string backupfile;
    copystring(backupfile, findfile(backupname, "wb"));
    remove(backupfile);
    rename(findfile(name, "wb"), backupfile);
}

static string cgzname, ocgzname, bakname, cbakname, pcfname, mcfname, omcfname, mapname;

const char *setnames(const char *name)
{
    string pakname;
    const char *slash = strpbrk(name, "/\\");
    if(slash)
    {
        copystring(pakname, name, slash-name+1);
        copystring(mapname, slash+1);
    }
    else
    {
        copystring(pakname, "maps");
        copystring(mapname, name);
    }
    formatstring(cgzname)("packages/%s/%s.cgz",      pakname, mapname);
    formatstring(ocgzname)("packages/maps/official/%s.cgz",   mapname);
    formatstring(bakname)("packages/%s/%s_%s.BAK",   pakname, mapname, numtime());
    formatstring(cbakname)("packages/%s/%s.cfg_%s.BAK",   pakname, mapname, numtime());
    formatstring(pcfname)("packages/%s/package.cfg", pakname);
    formatstring(mcfname)("packages/%s/%s.cfg",      pakname, mapname);
    formatstring(omcfname)("packages/maps/official/%s.cfg",   mapname);

    path(cgzname);
    path(bakname);
    path(cbakname);
    return cgzname;
}

// the optimize routines below are here to reduce the detrimental effects of messy mapping by
// setting certain properties (vdeltas and textures) to neighbouring values wherever there is no
// visible difference. This allows the mipmapper to generate more efficient mips.
// the reason it is done on save is to reduce the amount spend in the mipmapper (as that is done
// in realtime).

inline bool nhf(sqr *s) { return s->type!=FHF && s->type!=CHF; }

void voptimize()        // reset vdeltas on non-hf cubes
{
    loop(y, ssize) loop(x, ssize)
    {
        sqr *s = S(x, y);
        if(x && y) { if(nhf(s) && nhf(S(x-1, y)) && nhf(S(x-1, y-1)) && nhf(S(x, y-1))) s->vdelta = 0; }
        else s->vdelta = 0;
    }
}

// these two are used by getmap/sendmap.. transfers compressed maps directly

void writemap(char *name, int msize, uchar *mdata)
{
    setnames(name);
    backup(cgzname, bakname);
    stream *f = openfile(cgzname, "wb");
    if(!f) { conoutf("\f3could not write map to %s", cgzname); return; }
    f->write(mdata, msize);
    delete f;
    conoutf("wrote map %s as file %s", name, cgzname);
}

uchar *readmap(char *name, int *size, int *revision)
{
    setnames(name);
    uchar *data = (uchar *)loadfile(cgzname, size);
    if(!data) { conoutf("\f3could not read map %s", cgzname); return NULL; }
    mapstats *ms = loadmapstats(cgzname, false);
    if(revision) *revision = ms->hdr.maprevision;
    return data;
}

void writecfggz(char *name, int size, int sizegz, uchar *data)
{
    if(size < 1 || !sizegz || size > MAXCFGFILESIZE) return;
    setnames(name);

    uchar *rawcfg = new uchar[size];
    uLongf rawsize = size;
    if(rawcfg && uncompress(rawcfg, &rawsize, data, sizegz) == Z_OK && rawsize - size == 0)
    {
        stream *f = openfile(mcfname, "w");
        if(f)
        {
            f->write(rawcfg, size);
            delete f;
            conoutf("wrote map config to %s", mcfname);
        }
        else
        {
            conoutf("\f3could not write config to %s", mcfname);
        }
    }
    DELETEA(rawcfg);
}

#define GZBUFSIZE ((MAXCFGFILESIZE * 11) / 10)

uchar *readmcfggz(char *name, int *size, int *sizegz)
{
    setnames(name);
    uchar *gzbuf = new uchar[GZBUFSIZE];
    uchar *data = (uchar*)loadfile(mcfname, size, "r");
    if(data && *size < MAXCFGFILESIZE)
    {
        uLongf gzbufsize = GZBUFSIZE;
        if(compress2(gzbuf, &gzbufsize, data, *size, 9) != Z_OK)
        {
            *size = 0;
            gzbufsize = 0;
            DELETEA(gzbuf);
        }
        *sizegz = (int) gzbufsize;
    }
    else
    {
        DELETEA(gzbuf);
    }
    DELETEA(data);
    return gzbuf;
}

// save map as .cgz file. uses 2 layers of compression: first does simple run-length
// encoding and leaves out data for certain kinds of cubes, then zlib removes the
// last bits of redundancy. Both passes contribute greatly to the miniscule map sizes.

void rlencodecubes(vector<uchar> &f, sqr *s, int len, bool preservesolids) // run-length encoding and serialisation of a series of cubes
{
    sqr *t = NULL;
    int sc = 0;
    #define spurge while(sc) { f.add(255); if(sc>255) { f.add(255); sc -= 255; } else { f.add(sc); sc = 0; } }
    #define c(f) (s->f==t->f)
    while(len-- > 0)
    {
        // 4 types of blocks, to compress a bit:
        // 255 (2): same as previous block + count
        // 254 (3): same as previous, except light // deprecated
        // SOLID (5)
        // anything else (9)

        if(SOLID(s) && !preservesolids)
        {
            if(t && c(type) && c(wtex) && c(vdelta))
            {
                sc++;
            }
            else
            {
                spurge;
                f.add(s->type);
                f.add(s->wtex);
                f.add(s->vdelta);
            }
        }
        else
        {
            if(t && c(type) && c(floor) && c(ceil) && c(ctex) && c(ftex) && c(utex) && c(wtex) && c(vdelta) && c(tag))
            {
                sc++;
            }
            else
            {
                spurge;
                f.add(s->type == SOLID ? 253 : s->type);
                f.add(s->floor);
                f.add(s->ceil);
                f.add(s->wtex);
                f.add(s->ftex);
                f.add(s->ctex);
                f.add(s->vdelta);
                f.add(s->utex);
                f.add(s->tag);
            }
        }
        t = s;
        s++;
    }
    spurge;
}

void rldecodecubes(ucharbuf &f, sqr *s, int len, int version, bool silent) // run-length decoding of a series of cubes (version is only relevant, if < 6)
{
    sqr *t = NULL, *e = s + len;
    while(s < e)
    {
        int type = f.overread() ? -1 : f.get();
        switch(type)
        {
            case -1:
            {
                if(!silent) conoutf("while reading map at %d: unexpected end of file", cubicsize - (e - s));
                f.forceoverread();
                silent = true;
                sqrdefault(s);
                break;
            }
            case 255:
            {
                if(!t) { f.forceoverread(); continue; }
                int n = f.get();
                loopi(n) memcpy(s++, t, sizeof(sqr));
                s--;
                break;
            }
            case 254: // only in MAPVERSION<=2
            {
                if(!t) { f.forceoverread(); continue; }
                memcpy(s, t, sizeof(sqr));
                f.get(); f.get();
                break;
            }
            case SOLID:
            {
                sqrdefault(s);                  // takes care of ftex, ctex, floor, ceil and tag
                s->type = SOLID;
                s->utex = s->wtex = f.get();
                s->vdelta = f.get();
                if(version<=2) { f.get(); f.get(); }
                break;
            }
            case 253: // SOLID with all textures during editing (undo)
                type = SOLID;
            default:
            {
                if(type<0 || type>=MAXTYPE)
                {
                    if(!silent) conoutf("while reading map at %d: type %d out of range", cubicsize - (e - s), type);
                    f.overread();
                    continue;
                }
                sqrdefault(s);
                s->type = type;
                s->floor = f.get();
                s->ceil = f.get();
                if(s->floor>=s->ceil) s->floor = s->ceil-1;  // for pre 12_13
                s->wtex = f.get();
                s->ftex = f.get();
                s->ctex = f.get();
                if(version<=2) { f.get(); f.get(); }
                s->vdelta = f.get();
                s->utex = (version>=2) ? f.get() : s->wtex;
                s->tag = (version>=5) ? f.get() : 0;
            }
        }
        s->defer = 0;
        t = s;
        s++;
    }
}

// headerextra stores additional data in a map file (support since format 10)
// data can be persistent or oneway
// the format and handling is explicitly designed to handle yet unknown header types to avoid further format version bumps

struct headerextra
{
    int len, flags;
    uchar *data;
    headerextra() : len(0), flags(0), data(NULL) {}
    headerextra(int l, int f, uchar *d) : len(l), flags(f), data(NULL) { if(d) { data = new uchar[len]; memcpy(data, d, len); } }
    ~headerextra() { DELETEA(data); }
};
vector<headerextra *> headerextras;

enum { HX_UNUSED = 0, HX_MAPINFO, HX_MODEINFO, HX_ARTIST, HX_EDITUNDO, HX_CONFIG, HX_NUM, HX_TYPEMASK = 0x3f, HX_FLAG_PERSIST = 0x40 };
const char *hx_names[] = { "unused", "mapinfo", "modeinfo", "artist", "editundo", "config", "unknown" };
#define addhxpacket(p, len, flags, buffer) { if(p.length() + len < MAXHEADEREXTRA) { putuint(p, len); putuint(p, flags); p.put(buffer, len); } }
#define hx_name(t) hx_names[min((t) & HX_TYPEMASK, int(HX_NUM))]

void clearheaderextras() { loopvrev(headerextras) delete headerextras.remove(i); }

void deleteheaderextra(int n) { if(headerextras.inrange(n)) delete headerextras.remove(n); }

COMMANDF(listheaderextras, "", ()
{
    loopv(headerextras) conoutf("extra header record %d: %s, %d bytes", i, hx_name(headerextras[i]->flags), headerextras[i]->len);
    if(!headerextras.length()) conoutf("no extra header records found");
});

int findheaderextra(int type)
{
    loopv(headerextras) if((headerextras[i]->flags & HX_TYPEMASK) == type) return i;
    return -1;
}

void unpackheaderextra(uchar *buf, int len)  // break the extra data from the mapheader into its pieces
{
    ucharbuf p(buf, len);
    DEBUG("unpacking " << len << " bytes");
    while(1)
    {
        int len = getuint(p), flags = getuint(p), type = flags & HX_TYPEMASK;
        if(p.overread() || len > p.remaining()) break;
        clientlogf(" found headerextra \"%s\", %d bytes%s", hx_name(type), len, flags & HX_FLAG_PERSIST ? "persistent" : "");  // debug info
        headerextras.add(new headerextra(len, flags, p.subbuf(len).buf));
    }
}

void parseheaderextra(bool clearnonpersist = true, int ignoretypes = 0)  // parse all headerextra packets, delete the nonpersistent ones (like editundos)
{
    DEBUG("parsing " << headerextras.length() << " packets");
    loopv(headerextras)
    {
        ucharbuf q(headerextras[i]->data, headerextras[i]->len);
        int type = headerextras[i]->flags & HX_TYPEMASK;
        DEBUG("packet " << i << " type " << type);
        bool deletethis = false;                               // (set deletethis for headers that persist outside headerextras and get reinserted by packheaderextras())
        if(!(ignoretypes & (1 << type))) switch(type)
        {
            case HX_EDITUNDO:
                restoreeditundo(q);
                break;

            case HX_CONFIG:
                setcontext("map", "embedded");
                execute((const char *)q.buf);
                resetcontext();
                break;

            case HX_MAPINFO:
            case HX_MODEINFO:
            case HX_ARTIST:
            default:
                break;
        }
        if(deletethis || (clearnonpersist && !(headerextras[i]->flags & HX_FLAG_PERSIST))) delete headerextras.remove(i--);
    }
}

ucharbuf packheaderextras(int ignoretypes = 0)  // serialise all extra data packets to save with the map header
{
    vector<uchar> buf, tmp;
    loopv(headerextras) if(!(ignoretypes & (1 << (headerextras[i]->flags & HX_TYPEMASK))))
    { // copy existing persistent hx packets
        addhxpacket(buf, headerextras[i]->len, headerextras[i]->flags, headerextras[i]->data);
    }
    // create non-persistent ones, if wanted
    if(!(ignoretypes & (1 << HX_EDITUNDO)))
    {
        int limit = (MAXHEADEREXTRA - buf.length()) / 3;
        backupeditundo(tmp, limit, limit);
        addhxpacket(buf, tmp.length(), HX_EDITUNDO, tmp.getbuf());
    }
    ucharbuf q(new uchar[buf.length()], buf.length());
    memcpy(q.buf, buf.getbuf(), q.maxlen);
    DEBUG("packed " << q.maxlen << " bytes");
    return q;
}

bool embedconfigfile()
{
    if(securemapcheck(getclientmap())) return false;
    setnames(getclientmap());
    int len;
    uchar *buf = (uchar *)loadfile(path(mcfname), &len);
    if(buf)
    {
        for(int n; (n = findheaderextra(HX_CONFIG)) >= 0; ) deleteheaderextra(n);
        headerextras.add(new headerextra(len + 1, HX_CONFIG|HX_FLAG_PERSIST, NULL))->data = buf;
        backup(mcfname, cbakname); // don't delete the config file, just rename
        conoutf("embedded map config file \"%s\"", mcfname);
    }
    else conoutf("\f3failed to load config file %s", mcfname);
    return buf != NULL;
}
COMMANDF(embedconfigfile, "", () { if(embedconfigfile()) save_world(getclientmap()); });

void extractconfigfile()
{
    if(securemapcheck(getclientmap())) return;
    setnames(getclientmap());
    int n = findheaderextra(HX_CONFIG);
    if(n < 0) conoutf("no embedded config found");
    else
    {
        backup(mcfname, cbakname); // don't overwrite an existing config file, just rename it
        stream *f = openfile(mcfname, "w");
        if(f)
        {
            f->write(headerextras[n]->data, headerextras[n]->len);
            delete f;
            conoutf("extracted embedded map config to file %s", mcfname);
            deleteheaderextra(n);
        }
        else conoutf("\f3failed to write config to %s", mcfname);
    }
}
COMMAND(extractconfigfile, "");

VAR(advancemaprevision, 1, 1, 100);

VARP(mapbackupsonsave, 0, 1, 1);

void save_world(char *mname, bool skipoptimise, bool addcomfort)
{
    if(!*mname) mname = getclientmap();
    if(securemapcheck(mname)) return;
    DEBUG("writing map \"" << mname << "\"");
    if(!validmapname(mname))
    {
        conoutf("\f3Invalid map name. It must only contain letters, digits, '-', '_' and be less than %d characters long", MAXMAPNAMELEN);
        return;
    }
    if(!skipoptimise)
    {
        voptimize();
        mapmrproper(false);
        addcomfort = false; // "optimized + undos" is not useful
    }
    setnames(mname);
    if(mapbackupsonsave) backup(cgzname, bakname);
    stream *f = opengzfile(cgzname, "wb");
    if(!f) { conoutf("could not write map to %s", cgzname); return; }
    strncpy(hdr.head, "ACMP", 4); // ensure map now declares itself as an AssaultCube map, even if imported as CUBE
    hdr.version = MAPVERSION;
    hdr.headersize = sizeof(header);
    hdr.numents = 0;
    loopv(ents) if(ents[i].type!=NOTUSED) hdr.numents++;
    if(hdr.numents > MAXENTITIES)
    {
        conoutf("too many map entities (%d), only %d will be written to file", hdr.numents, MAXENTITIES);
        hdr.numents = MAXENTITIES;
    }
    header tmp = hdr;
    ucharbuf hx = packheaderextras(addcomfort ? 0 : (1 << HX_EDITUNDO));   // if addcomfort -> add undos/redos
    int writeextra = 0;
    if(hx.maxlen) tmp.headersize += writeextra = clamp(hx.maxlen, 0, MAXHEADEREXTRA);
    if(writeextra || skipoptimise) tmp.version = 10;   // 9 and 10 are the same, but in 10 the headersize is reliable - if we don't need it, stick to 9
    tmp.maprevision += advancemaprevision;
    DEBUG("version " << tmp.version << " headersize " << tmp.headersize << " entities " << tmp.numents << " factor " << tmp.sfactor << " revision " << tmp.maprevision);
    lilswap(&tmp.version, 4);
    lilswap(&tmp.waterlevel, 1);
    lilswap(&tmp.maprevision, 2);
    f->write(&tmp, sizeof(header));
    if(writeextra) f->write(hx.buf, writeextra);
    delete[] hx.buf;
    int ne = hdr.numents;
    loopv(ents)
    {
        if(ents[i].type!=NOTUSED)
        {
            if(!ne--) break;
            persistent_entity tmp = ents[i];
            lilswap((short *)&tmp, 4);
            f->write(&tmp, sizeof(persistent_entity));
        }
    }

    vector<uchar> rawcubes;
    rlencodecubes(rawcubes, world, cubicsize, skipoptimise);  // if skipoptimize -> keep properties of solid cubes (forces format 10)
    f->write(rawcubes.getbuf(), rawcubes.length());
    delete f;
    conoutf("wrote map file %s %s%s", cgzname, skipoptimise ? "without optimisation" : "(optimised)", addcomfort ? " (including editing history)" : "");
}
VARP(preserveundosonsave, 0, 0, 1);
COMMANDF(savemap, "s", (char *name) { save_world(name, preserveundosonsave && editmode, preserveundosonsave && editmode); } );
COMMANDF(savemapoptimised, "s", (char *name) { save_world(name, false, false); } );

extern int mapdims[8];     // min/max X/Y and delta X/Y and min-floor/max-ceil
void showmapdims()
{
    conoutf("  min X|Y|Z: %3d : %3d : %3d", mapdims[0], mapdims[1], mapdims[6]);
    conoutf("  max X|Y|Z: %3d : %3d : %3d", mapdims[2], mapdims[3], mapdims[7]);
    conoutf("delta X|Y|Z: %3d : %3d : %3d", mapdims[4], mapdims[5], mapdims[7]-mapdims[6]);
}
COMMAND(showmapdims, "");

extern void preparectf(bool cleanonly = false);
int numspawn[3], maploaded = 0, numflagspawn[2];
VAR(curmaprevision, 1, 0, 0);

extern char *mlayout;
extern int Mv, Ma, Hhits;
extern float Mh;

void rebuildtexlists()  // checks the texlists, if they still contain all possible textures
{
    short h[256];
    vector<uchar> missing;
    loopk(3)
    {
        missing.setsize(0);
        uchar *p = hdr.texlists[k];
        loopi(256) h[i] = 0;
        loopi(256) h[p[i]]++;
        loopi(256) if(h[i] == 0) missing.add(i);
        loopi(256) if(h[p[i]] > 1)
        {
            h[p[i]]--;
            p[i] = missing.pop();
        }
    }
}

bool load_world(char *mname)        // still supports all map formats that have existed since the earliest cube betas!
{
    const int sizeof_header = sizeof(header), sizeof_baseheader = sizeof_header - sizeof(int) * 16;
    stopwatch watch;
    watch.start();

    advancemaprevision = 1;
    setnames(mname);
    maploaded = getfilesize(ocgzname);
    if(maploaded > 0)
    {
        copystring(cgzname, ocgzname);
        copystring(mcfname, omcfname);
    }
    else maploaded = getfilesize(cgzname);
    if(!validmapname(mapname))
    {
        conoutf("\f3Invalid map name. It must only contain letters, digits, '-', '_' and be less than %d characters long", MAXMAPNAMELEN);
        return false;
    }
    stream *f = opengzfile(cgzname, "rb");
    if(!f) { conoutf("\f3could not read map %s", cgzname); return false; }
    DEBUG("reading map \"" << cgzname << "\"");
    header tmp;
    memset(&tmp, 0, sizeof_header);
    if(f->read(&tmp, sizeof_baseheader) != sizeof_baseheader ||
       (strncmp(tmp.head, "CUBE", 4)!=0 && strncmp(tmp.head, "ACMP",4)!=0)) { conoutf("\f3while reading map: header malformatted (1)"); delete f; return false; }
    lilswap(&tmp.version, 4); // version, headersize, sfactor, numents
    if(tmp.version > MAXMAPVERSION) { conoutf("\f3this map requires a newer version of AssaultCube"); delete f; return false; }
    if(tmp.sfactor<SMALLEST_FACTOR || tmp.sfactor>LARGEST_FACTOR || tmp.numents > MAXENTITIES) { conoutf("\f3illegal map size"); delete f; return false; }
    tmp.headersize = fixmapheadersize(tmp.version, tmp.headersize);
    int restofhead = min(tmp.headersize, sizeof_header) - sizeof_baseheader;
    if(f->read(&tmp.waterlevel, restofhead) != restofhead) { conoutf("\f3while reading map: header malformatted (2)"); delete f; return false; }
    DEBUG("version " << tmp.version << " headersize " << tmp.headersize << " headerextrasize " << tmp.headersize - int(sizeof(header)) << " entities " << tmp.numents << " factor " << tmp.sfactor << " revision " << tmp.maprevision);
    clearheaderextras();
    if(tmp.headersize > sizeof_header)
    {
        int extrasize = tmp.headersize - sizeof_header;
        if(tmp.version < 9) extrasize = 0;  // throw away mediareq...
        else if(extrasize > MAXHEADEREXTRA) extrasize = MAXHEADEREXTRA;
        if(extrasize)
        { // map file actually has extra header data that we want too preserve
            uchar *extrabuf = new uchar[extrasize];
            if(f->read(extrabuf, extrasize) != extrasize) { conoutf("\f3while reading map: header malformatted (3)"); delete f; return false; }
            unpackheaderextra(extrabuf, extrasize);
            delete[] extrabuf;
        }
        f->seek(tmp.headersize, SEEK_SET);
    }
    hdr = tmp;
    rebuildtexlists();
    loadingscreen("%s", hdr.maptitle);
    resetmap();
    if(hdr.version>=4)
    {
        lilswap(&hdr.waterlevel, 1);
        if(!hdr.watercolor[3]) setwatercolor();
        lilswap(&hdr.maprevision, 2);
        curmaprevision = hdr.maprevision;
    }
    else
    {
        hdr.waterlevel = -100000;
        hdr.ambient = 0;
    }
    setvar("waterlevel", hdr.waterlevel);
    ents.shrink(0);
    loopi(3) numspawn[i] = 0;
    loopi(2) numflagspawn[i] = 0;
    loopi(hdr.numents)
    {
        entity &e = ents.add();
        f->read(&e, sizeof(persistent_entity));
        lilswap((short *)&e, 4);
        e.spawned = false;
        if(e.type == LIGHT)
        {
            if(!e.attr2) e.attr2 = 255; // needed for MAPVERSION<=2
            if(e.attr1>32) e.attr1 = 32; // 12_03 and below
        }
        transformoldentities(hdr.version, e.type);
        if(e.type == PLAYERSTART && (e.attr2 == 0 || e.attr2 == 1 || e.attr2 == 100))
        {
            if(e.attr2 == 100)
                numspawn[2]++;
            else
                numspawn[e.attr2]++;
        }
        if(e.type == CTF_FLAG && (e.attr2 == 0 || e.attr2 == 1)) numflagspawn[e.attr2]++;
    }
    delete[] world;
    setupworld(hdr.sfactor);
    if(!mapinfo.numelems || (mapinfo.access(mname) && !cmpf(cgzname, mapinfo[mname]))) world = (sqr *)ents.getbuf();
    c2skeepalive();

    vector<uchar> rawcubes; // fetch whole file into buffer
    loopi(9)
    {
        ucharbuf q = rawcubes.reserve(cubicsize);
        q.len = f->read(q.buf, cubicsize);
        rawcubes.addbuf(q);
        if(q.len < cubicsize) break;
    }
    ucharbuf uf(rawcubes.getbuf(), rawcubes.length());
    rldecodecubes(uf, world, cubicsize, hdr.version, false); // decode file
    c2skeepalive();

    // calculate map statistics
    DELETEA(mlayout);
    mlayout = new char[cubicsize + 256];
    memset(mlayout, 0, cubicsize * sizeof(char));
    Mv = Ma = Hhits = 0;
    char texuse[256];
    loopi(256) texuse[i] = 0;
    loopk(8) mapdims[k] = k < 2 ? ssize : 0;
    loopk(cubicsize)
    {
        sqr *s = &world[k];
        if(SOLID(s)) mlayout[k] = 127;
        else
        {
            mlayout[k] = s->floor; // FIXME
            int diff = s->ceil - s->floor;
            if(diff > 6)
            {
                if(diff > MAXMHEIGHT) Hhits += diff - MAXMHEIGHT;
                Ma += 1;
                Mv += diff;
            }
            texuse[s->utex] = texuse[s->ftex] = texuse[s->ctex] = 1;
            int cwx = k%ssize,
                cwy = k/ssize;
            if(cwx < mapdims[0]) mapdims[0] = cwx;
            if(cwy < mapdims[1]) mapdims[1] = cwy;
            if(cwx > mapdims[2]) mapdims[2] = cwx;
            if(cwy > mapdims[3]) mapdims[3] = cwy;
            if(s->floor != -128 && s->floor < mapdims[6]) mapdims[6] = s->floor;
            if(s->ceil  > mapdims[7]) mapdims[7] = s->ceil;
        }
        texuse[s->wtex] = 1;
    }
    Mh = Ma ? (float)Mv/Ma : 0;
    loopk(2) mapdims[k+4] = mapdims[k+2] + 1 - mapdims[k]; // 8..15 ^= 8 cubes - minimal X/Y == 2 - != 0 !!
    c2skeepalive();

    calclight();
    conoutf("read map %s rev %d (%d milliseconds)", cgzname, hdr.maprevision, watch.stop());
    conoutf("%s", hdr.maptitle);
    pushscontext(IEXC_MAPCFG); // untrusted altogether
    per_idents = false;
    neverpersist = true;
    execfile("config/default_map_settings.cfg");
    execfile(pcfname);
    execfile(mcfname);
    parseheaderextra();
    neverpersist = false;
    per_idents = true;
    popscontext();

    c2skeepalive();

    watch.start();
    loopi(256) if(texuse[i]) lookupworldtexture(i, autodownload ? true : false);
    int texloadtime = watch.stop();

    c2skeepalive();

    watch.start();
    preload_mapmodels(autodownload ? true : false);
    int mdlloadtime = watch.stop();

    c2skeepalive();

    watch.start();
    audiomgr.preloadmapsounds(autodownload ? true : false);
    int audioloadtime = watch.stop();

    c2skeepalive();

    watch.start();
    int downloaded = downloadpackages();
    if(downloaded > 0) clientlogf("downloaded content (%d KB in %d seconds)", downloaded/1000, watch.stop()/1000);

    c2skeepalive();

    loadsky(NULL, true);

    watch.start();
    loopi(256) if(texuse[i]) lookupworldtexture(i, false);
    clientlogf("loaded textures (%d milliseconds)", texloadtime+watch.stop());
    c2skeepalive();
    watch.start();
    preload_mapmodels(false);
    clientlogf("loaded mapmodels (%d milliseconds)", mdlloadtime+watch.stop());
    c2skeepalive();
    watch.start();
    audiomgr.preloadmapsounds(false);
    clientlogf("loaded mapsounds (%d milliseconds)", audioloadtime+watch.stop());
    c2skeepalive();

    defformatstring(startmillis)("%d", millis_());
    alias("gametimestart", startmillis);
    startmap(mname);
    return true;
}
