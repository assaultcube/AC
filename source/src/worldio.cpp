// worldio.cpp: loading & saving of maps and savegames

#include "cube.h"

void backup(char *name, char *backupname)
{
    string backupfile;
    copystring(backupfile, findfile(backupname, "wb"));
    remove(backupfile);
    rename(findfile(name, "wb"), backupfile);
}

static string cgzname, ocgzname, bakname, pcfname, mcfname, omcfname, mapname;

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
    formatstring(pcfname)("packages/%s/package.cfg", pakname);
    formatstring(mcfname)("packages/%s/%s.cfg",      pakname, mapname);
    formatstring(omcfname)("packages/maps/official/%s.cfg",   mapname);

    path(cgzname);
    path(bakname);
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

void topt(sqr *s, bool &wf, bool &uf, int &wt, int &ut)
{
    sqr *o[4];
    o[0] = SWS(s,0,-1,sfactor);
    o[1] = SWS(s,0,1,sfactor);
    o[2] = SWS(s,1,0,sfactor);
    o[3] = SWS(s,-1,0,sfactor);
     wf = true;
    uf = true;
    if(SOLID(s))
    {
        loopi(4) if(!SOLID(o[i]))
        {
            wf = false;
            wt = s->wtex;
            ut = s->utex;
            return;
         }
    }
    else
    {
        loopi(4) if(!SOLID(o[i]))
        {
            //don't corrupt non-matching cube types
            if (o[i]->type != s->type)
            {
                wf = false;
                uf = false;
                wt = s->wtex;
                ut = s->utex;
                return;
            }

            //wall
            if(o[i]->floor < s->floor)
            { wt = s->wtex; wf = false; }

            //upper wall
            if(o[i]->ceil > s->ceil)
             { ut = s->utex; uf = false; }
        }
    }
}

void toptimize() // FIXME: only does 2x2, make atleast for 4x4 also
{
    bool wf[4], uf[4];
    sqr *s[4];
    for(int y = 2; y<ssize-4; y += 2) for(int x = 2; x<ssize-4; x += 2)
    {
        s[0] = S(x,y);
        int wt = s[0]->wtex, ut = s[0]->utex;
        topt(s[0], wf[0], uf[0], wt, ut);
        topt(s[1] = SWS(s[0],0,1,sfactor), wf[1], uf[1], wt, ut);
        topt(s[2] = SWS(s[0],1,1,sfactor), wf[2], uf[2], wt, ut);
        topt(s[3] = SWS(s[0],1,0,sfactor), wf[3], uf[3], wt, ut);
        loopi(4)
        {
            if(wf[i]) s[i]->wtex = wt;
            if(uf[i]) s[i]->utex = ut;
        }
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

VAR(advancemaprevision, 1, 1, 100);

VARP(mapbackupsonsave, 0, 1, 1);

void save_world(char *mname)
{
    if(!*mname) mname = getclientmap();
    if(securemapcheck(mname)) return;
    if(!validmapname(mname))
    {
        conoutf("\f3Invalid map name. It must only contain letters, digits, '-', '_' and be less than %d characters long", MAXMAPNAMELEN);
        return;
    }
    voptimize();
    toptimize();
    setnames(mname);
    if(mapbackupsonsave) backup(cgzname, bakname);
    stream *f = opengzfile(cgzname, "wb");
    if(!f) { conoutf("could not write map to %s", cgzname); return; }
    strncpy(hdr.head, "ACMP", 4); // ensure map now declares itself as an AssaultCube map, even if imported as CUBE
    hdr.version = MAPVERSION;
    hdr.numents = 0;
    loopv(ents) if(ents[i].type!=NOTUSED) hdr.numents++;
    if(hdr.numents > MAXENTITIES)
    {
        conoutf("too many map entities (%d), only %d will be written to file", hdr.numents, MAXENTITIES);
        hdr.numents = MAXENTITIES;
    }
    header tmp = hdr;
    lilswap(&tmp.version, 4);
    lilswap(&tmp.waterlevel, 1);
    tmp.maprevision += advancemaprevision;
    lilswap(&tmp.maprevision, 2);
    f->write(&tmp, sizeof(header));
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
    sqr *t = NULL;
    int sc = 0;
    #define spurge while(sc) { f->putchar(255); if(sc>255) { f->putchar(255); sc -= 255; } else { f->putchar(sc); sc = 0; } }
    loopk(cubicsize)
    {
        sqr *s = &world[k];
        #define c(f) (s->f==t->f)
        // 4 types of blocks, to compress a bit:
        // 255 (2): same as previous block + count
        // 254 (3): same as previous, except light // deprecated
        // SOLID (5)
        // anything else (9)

        if(SOLID(s))
        {
            if(t && c(type) && c(wtex) && c(vdelta))
            {
                sc++;
            }
            else
            {
                spurge;
                f->putchar(s->type);
                f->putchar(s->wtex);
                f->putchar(s->vdelta);
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
                f->putchar(s->type);
                f->putchar(s->floor);
                f->putchar(s->ceil);
                f->putchar(s->wtex);
                f->putchar(s->ftex);
                f->putchar(s->ctex);
                f->putchar(s->vdelta);
                f->putchar(s->utex);
                f->putchar(s->tag);
            }
        }
        t = s;
    }
    spurge;
    delete f;
    conoutf("wrote map file %s", cgzname);
}

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

bool load_world(char *mname)        // still supports all map formats that have existed since the earliest cube betas!
{
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
    header tmp;
    memset(&tmp, 0, sizeof(header));
    if(f->read(&tmp, sizeof(header)-sizeof(int)*16)!=sizeof(header)-sizeof(int)*16) { conoutf("\f3while reading map: header malformatted"); delete f; return false; }
    lilswap(&tmp.version, 4);
    if(strncmp(tmp.head, "CUBE", 4)!=0 && strncmp(tmp.head, "ACMP",4)!=0) { conoutf("\f3while reading map: header malformatted"); delete f; return false; }
    if(tmp.version>MAPVERSION) { conoutf("\f3this map requires a newer version of AssaultCube"); delete f; return false; }
    if(tmp.sfactor<SMALLEST_FACTOR || tmp.sfactor>LARGEST_FACTOR || tmp.numents > MAXENTITIES) { conoutf("\f3illegal map size"); delete f; return false; }
    if(tmp.version>=4 && f->read(&tmp.waterlevel, sizeof(int)*16)!=sizeof(int)*16) { conoutf("\f3while reading map: header malformatted"); delete f; return false; }
    if((tmp.version==7 || tmp.version==8) && !f->seek(sizeof(char)*128, SEEK_CUR)) { conoutf("\f3while reading map: header malformatted"); delete f; return false; }
    hdr = tmp;
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
    ents.shrink(0);
    loopi(3) numspawn[i] = 0;
    loopi(2) numflagspawn[i] = 0;
    loopi(hdr.numents)
    {
        entity &e = ents.add();
        f->read(&e, sizeof(persistent_entity));
        lilswap((short *)&e, 4);
        e.spawned = false;
        TRANSFORMOLDENTITIES(hdr)
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

    DELETEA(mlayout);
    mlayout = new char[cubicsize + 256];
    memset(mlayout, 0, cubicsize * sizeof(char));
    int diff = 0;
    Mv = Ma = Hhits = 0;

    if(!mapinfo.numelems || (mapinfo.access(mname) && !cmpf(cgzname, mapinfo[mname]))) world = (sqr *)ents.getbuf();
    c2skeepalive();
    char texuse[256];
    loopi(256) texuse[i] = 0;
    sqr *t = NULL;
    loopk(cubicsize)
    {
        char *c = mlayout + k;
        sqr *s = &world[k];
        int type = f ? f->getchar() : -1;
        int n = 1;
        switch(type)
        {
            case -1:
            {
                if(f)
                {
                    conoutf("while reading map at %d: type %d out of range", k, type);
                    delete f;
                    f = NULL;
                }
                *c = 127;
                s->type = SOLID;
                s->ftex = DEFAULT_FLOOR;
                s->ctex = DEFAULT_CEIL;
                s->wtex = s->utex = DEFAULT_WALL;
                s->tag = 0;
                s->floor = 0;
                s->ceil = 16;
                s->vdelta = 0;
                break;
            }
            case 255:
            {
                if(!t || (n = f->getchar()) < 0) { delete f; f = NULL; k--; continue; }
                char tmp = *(c-1);
                memset(c, tmp, n);
                for(int i = 0; i<n; i++, k++) memcpy(&world[k], t, sizeof(sqr));
                k--;
                break;
            }
            case 254: // only in MAPVERSION<=2
            {
                if(!t) { delete f; f = NULL; k--; continue; }
                *c = *(c-1);
                memcpy(s, t, sizeof(sqr));
                s->r = s->g = s->b = f->getchar();
                f->getchar();
                break;
            }
            case SOLID:
            {
                *c = 127;
                s->type = SOLID;
                s->wtex = f->getchar();
                s->vdelta = f->getchar();
                if(hdr.version<=2) { f->getchar(); f->getchar(); }
                s->ftex = DEFAULT_FLOOR;
                s->ctex = DEFAULT_CEIL;
                s->utex = s->wtex;
                s->tag = 0;
                s->floor = 0;
                s->ceil = 16;
                break;
            }
            default:
            {
                if(type<0 || type>=MAXTYPE)
                {
                    conoutf("while reading map at %d: type %d out of range", k, type);
                    delete f;
                    f = NULL;
                    k--;
                    continue;
                }
                s->type = type;
                s->floor = f->getchar();
                s->ceil = f->getchar();
                if(s->floor>=s->ceil) s->floor = s->ceil-1;  // for pre 12_13
                diff = s->ceil - s->floor;
                *c = s->floor; // FIXME
                s->wtex = f->getchar();
                s->ftex = f->getchar();
                s->ctex = f->getchar();
                if(hdr.version<=2) { f->getchar(); f->getchar(); }
                s->vdelta = f->getchar();
                s->utex = (hdr.version>=2) ? f->getchar() : s->wtex;
                s->tag = (hdr.version>=5) ? f->getchar() : 0;
            }
        }
        if ( type != SOLID && diff > 6 )
        {
            // Lucas (10mar2013): Removed "pow2" because it was too strict
            if (diff > MAXMHEIGHT) Hhits += (diff-MAXMHEIGHT)*n;
            Ma += n;
            Mv += diff * n;
        }
        s->defer = 0;
        t = s;
        texuse[s->wtex] = 1;
        if(!SOLID(s)) texuse[s->utex] = texuse[s->ftex] = texuse[s->ctex] = 1;
    }
    Mh = Ma ? (float)Mv/Ma : 0;
    if(f) delete f;
    c2skeepalive();
    loopk(8) mapdims[k] = k < 2 ? ssize : 0;
    loopk(cubicsize) if (world[k].type != SOLID)
    {
        int cwx = k%ssize,
            cwy = k/ssize;
        if(cwx < mapdims[0]) mapdims[0] = cwx;
        if(cwy < mapdims[1]) mapdims[1] = cwy;
        if(cwx > mapdims[2]) mapdims[2] = cwx;
        if(cwy > mapdims[3]) mapdims[3] = cwy;
        if(world[k].floor != -128 && world[k].floor < mapdims[6]) mapdims[6] = world[k].floor;
        if(world[k].ceil  > mapdims[7]) mapdims[7] = world[k].ceil;

    }
    loopk(2) mapdims[k+4] = mapdims[k+2] + 1 - mapdims[k]; // 8..15 ^= 8 cubes - minimal X/Y == 2 - != 0 !!
    calclight();
    conoutf("read map %s rev %d (%d milliseconds)", cgzname, hdr.maprevision, watch.stop());
    conoutf("%s", hdr.maptitle);
    pushscontext(IEXC_MAPCFG); // untrusted altogether
    per_idents = false;
    neverpersist = true;
    execfile("config/default_map_settings.cfg");
    execfile(pcfname);
    execfile(mcfname);
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
    if(downloaded > 0) printf("downloaded content (%d KB in %d seconds)\n", downloaded/1000, watch.stop()/1000);

    c2skeepalive();

    loadsky(NULL, true);

    watch.start();
    loopi(256) if(texuse[i]) lookupworldtexture(i, false);
    printf("loaded textures (%d milliseconds)\n", texloadtime+watch.stop());
    c2skeepalive();
    watch.start();
    preload_mapmodels(false);
    printf("loaded mapmodels (%d milliseconds)\n", mdlloadtime+watch.stop());
    c2skeepalive();
    watch.start();
    audiomgr.preloadmapsounds(false);
    printf("loaded mapsounds (%d milliseconds)\n", audioloadtime+watch.stop());
    c2skeepalive();

    defformatstring(startmillis)("%d", millis_());
    alias("gametimestart", startmillis);
    startmap(mname);
    return true;
}

COMMANDN(savemap, save_world, "s");