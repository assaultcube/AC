// worldio.cpp: loading & saving of maps and savegames

#include "pch.h"
#include "cube.h"

void backup(char *name, char *backupname)
{
    string backupfile;
    s_strcpy(backupfile, findfile(backupname, "wb"));
    remove(backupfile);
    rename(findfile(name, "wb"), backupfile);
}

static string cgzname, ocgzname, bakname, pcfname, mcfname, omcfname;

const char *setnames(const char *name)
{
    string pakname, mapname;
    char *slash = strpbrk(name, "/\\");
    if(slash)
    {
        s_strncpy(pakname, name, slash-name+1);
        s_strcpy(mapname, slash+1);
    }
    else
    {
        s_strcpy(pakname, "maps");
        s_strcpy(mapname, name);
    }
    s_sprintf(cgzname)("packages/%s/%s.cgz",      pakname, mapname);
    s_sprintf(ocgzname)("packages/maps/official/%s.cgz",   mapname);
    s_sprintf(bakname)("packages/%s/%s_%s.BAK",   pakname, mapname, numtime());
    s_sprintf(pcfname)("packages/%s/package.cfg", pakname);
    s_sprintf(mcfname)("packages/%s/%s.cfg",      pakname, mapname);
    s_sprintf(omcfname)("packages/maps/official/%s.cfg",   mapname);

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
            if(o[i]->floor<s->floor) { wt = s->wtex; wf = false; }
            if(o[i]->ceil>s->ceil)   { ut = s->utex; uf = false; }
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
    FILE *f = openfile(cgzname, "wb");
    if(!f) { conoutf("\f3could not write map to %s", cgzname); return; }
    fwrite(mdata, 1, msize, f);
    fclose(f);
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
        FILE *f = openfile(mcfname, "w");
        if(f)
        {
            fwrite(rawcfg, 1, size, f);
            fclose(f);
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

void save_world(char *mname)
{
    if(!*mname) mname = getclientmap();
    if(securemapcheck(mname)) return;
    voptimize();
    toptimize();
    setnames(mname);
    backup(cgzname, bakname);
    gzFile f = opengzfile(cgzname, "wb9");
    if(!f) { conoutf("could not write map to %s", cgzname); return; }
    hdr.version = MAPVERSION;
    hdr.numents = 0;
    loopv(ents) if(ents[i].type!=NOTUSED) hdr.numents++;
    if(hdr.numents > MAXENTITIES)
    {
        conoutf("too many map entities (%d), only %d will be written to file", hdr.numents, MAXENTITIES);
        hdr.numents = MAXENTITIES;
    }
    header tmp = hdr;
    endianswap(&tmp.version, sizeof(int), 4);
    endianswap(&tmp.waterlevel, sizeof(int), 1);
    tmp.maprevision += advancemaprevision;
    endianswap(&tmp.maprevision, sizeof(int), 1);
    gzwrite(f, &tmp, sizeof(header));
    int ne = hdr.numents;
    loopv(ents)
    {
        if(ents[i].type!=NOTUSED)
        {
            if(!ne--) break;
            entity tmp = ents[i];
            endianswap(&tmp, sizeof(short), 4);
            gzwrite(f, &tmp, sizeof(persistent_entity));
        }
    }
    sqr *t = NULL;
    int sc = 0;
    #define spurge while(sc) { gzputc(f, 255); if(sc>255) { gzputc(f, 255); sc -= 255; } else { gzputc(f, sc); sc = 0; } }
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
                gzputc(f, s->type);
                gzputc(f, s->wtex);
                gzputc(f, s->vdelta);
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
                gzputc(f, s->type);
                gzputc(f, s->floor);
                gzputc(f, s->ceil);
                gzputc(f, s->wtex);
                gzputc(f, s->ftex);
                gzputc(f, s->ctex);
                gzputc(f, s->vdelta);
                gzputc(f, s->utex);
                gzputc(f, s->tag);
            }
        }
        t = s;
    }
    spurge;
    gzclose(f);
    conoutf("wrote map file %s", cgzname);
}

extern void preparectf(bool cleanonly = false);
int numspawn[3], maploaded = 0, numflagspawn[2];
VAR(curmaprevision, 1, 0, 0);

bool load_world(char *mname)        // still supports all map formats that have existed since the earliest cube betas!
{
    int loadmillis = SDL_GetTicks();
    advancemaprevision = 1;
    setnames(mname);
    maploaded = getfilesize(ocgzname);
    if(maploaded > 0)
    {
        s_strcpy(cgzname, ocgzname);
        s_strcpy(mcfname, omcfname);
    }
    else maploaded = getfilesize(cgzname);
    gzFile f = opengzfile(cgzname, "rb9");
    if(!f) { conoutf("\f3could not read map %s", cgzname); return false; }
    header tmp;
    memset(&tmp, 0, sizeof(header));
    if(gzread(f, &tmp, sizeof(header)-sizeof(int)*16)!=sizeof(header)-sizeof(int)*16) { conoutf("\f3while reading map: header malformatted"); gzclose(f); return false; }
    endianswap(&tmp.version, sizeof(int), 4);
    if(strncmp(tmp.head, "CUBE", 4)!=0 && strncmp(tmp.head, "ACMP",4)!=0) { conoutf("\f3while reading map: header malformatted"); gzclose(f); return false; }
    if(tmp.version>MAPVERSION) { conoutf("\f3this map requires a newer version of cube"); gzclose(f); return false; }
    if(tmp.sfactor<SMALLEST_FACTOR || tmp.sfactor>LARGEST_FACTOR || tmp.numents > MAXENTITIES) { conoutf("\f3illegal map size"); gzclose(f); return false; }
    if(tmp.version>=4 && gzread(f, &tmp.waterlevel, sizeof(int)*16)!=sizeof(int)*16) { conoutf("\f3while reading map: header malformatted"); gzclose(f); return false; }
    hdr = tmp;
    loadingscreen("%s", hdr.maptitle);
    resetmap();
    if(hdr.version>=4)
    {
        endianswap(&hdr.waterlevel, sizeof(int), 1);
        if(!hdr.watercolor[3]) setwatercolor();
        endianswap(&hdr.maprevision, sizeof(int), 1);
        curmaprevision = hdr.maprevision;
    }
    else
    {
        hdr.waterlevel = -100000;
    }
    ents.setsize(0);
    loopi(3) numspawn[i] = 0;
    loopi(2) numflagspawn[i] = 0;
    loopi(hdr.numents)
    {
        entity &e = ents.add();
        gzread(f, &e, sizeof(persistent_entity));
        endianswap(&e, sizeof(short), 4);
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
    if(!mapinfo.numelems || (mapinfo.access(mname) && !cmpf(cgzname, mapinfo[mname]))) world = (sqr *)ents.getbuf();
	c2skeepalive();
	char texuse[256];
	loopi(256) texuse[i] = 0;
    sqr *t = NULL;
    loopk(cubicsize)
    {
        sqr *s = &world[k];
        int type = f ? gzgetc(f) : -1;
        switch(type)
        {
            case -1:
            {
                if(f)
                {
                    conoutf("while reading map at %d: type %d out of range", k, type);
                    gzclose(f);
                    f = NULL;
                }
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
                if(!t) { gzclose(f); f = NULL; k--; continue; }
                int n = gzgetc(f);
                for(int i = 0; i<n; i++, k++) memcpy(&world[k], t, sizeof(sqr));
                k--;
                break;
            }
            case 254: // only in MAPVERSION<=2
            {
                if(!t) { gzclose(f); f = NULL; k--; continue; }
                memcpy(s, t, sizeof(sqr));
                s->r = s->g = s->b = gzgetc(f);
                gzgetc(f);
                break;
            }
            case SOLID:
            {
                s->type = SOLID;
                s->wtex = gzgetc(f);
                s->vdelta = gzgetc(f);
                if(hdr.version<=2) { gzgetc(f); gzgetc(f); }
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
                    gzclose(f);
                    f = NULL;
                    k--;
                    continue;
                }
                s->type = type;
                s->floor = gzgetc(f);
                s->ceil = gzgetc(f);
                if(s->floor>=s->ceil) s->floor = s->ceil-1;  // for pre 12_13
                s->wtex = gzgetc(f);
                s->ftex = gzgetc(f);
                s->ctex = gzgetc(f);
                if(hdr.version<=2) { gzgetc(f); gzgetc(f); }
                s->vdelta = gzgetc(f);
                s->utex = (hdr.version>=2) ? gzgetc(f) : s->wtex;
                s->tag = (hdr.version>=5) ? gzgetc(f) : 0;
            }
        }
        s->defer = 0;
        t = s;
        texuse[s->wtex] = 1;
        if(!SOLID(s)) texuse[s->utex] = texuse[s->ftex] = texuse[s->ctex] = 1;
    }
    if(f) gzclose(f);
	c2skeepalive();
    calclight();
    conoutf("read map %s rev %d (%d milliseconds)", cgzname, hdr.maprevision, SDL_GetTicks()-loadmillis);
    conoutf("%s", hdr.maptitle);

    pushscontext(IEXC_MAPCFG); // untrusted altogether
    persistidents = false;
    execfile("config/default_map_settings.cfg");
    execfile(pcfname);
    execfile(mcfname);
    const char *pack = getalias("required_mappack");
    int reqpack = pack ? atoi(pack) : 0;
    persistidents = true;
    popscontext();

	c2skeepalive();
    loopi(256) if(texuse[i]) lookupworldtexture(i);
	c2skeepalive();
	preload_mapmodels();
	c2skeepalive();
    audiomgr.preloadmapsounds();
    c2skeepalive();

    startmap(mname);

    pack = getalias("mappack_version");
    if(pack && atoi(pack) < reqpack) conoutf("\f3this map needs mappack version %d or newer", reqpack);

    return true;
}

COMMANDN(savemap, save_world, ARG_1STR);

// FIXME - remove this before release
void setmaprevision(int rev) { hdr.maprevision = rev; }
COMMAND(setmaprevision, ARG_1INT);
