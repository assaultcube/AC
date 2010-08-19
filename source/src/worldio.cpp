// worldio.cpp: loading & saving of maps and savegames

#include "cube.h"

void backup(char *name, char *backupname)
{
    string backupfile;
    copystring(backupfile, findfile(backupname, "wb"));
    remove(backupfile);
    rename(findfile(name, "wb"), backupfile);
}

static string cgzname, ocgzname, bakname, pcfname, mcfname, omcfname;
static string reqmpak; // required media pack

const char *setnames(const char *name)
{
    string pakname, mapname;
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

void checkmapdependencies(bool silent = false, bool details = false) // find required MediaPack (s) for current map
{
    if(!silent) conoutf("checking map dependencies");
    static hashtable<const char *, int> mufpaths;
    hashtable<const char *, int> usedmods;
    string goodname, modname, allmods;
    // well, the "goodname" can still contain the users home-dir
    #define USEFILENAME(fmtp2f,fname,count) \
        { \
            defformatstring(basename)(fmtp2f, fname); \
            copystring(goodname, findfile(path(basename), "r")); \
            int *n = mufpaths.access(goodname); \
            if(!n) { n = &mufpaths.access(newstring(goodname), -1); if(fileexists(goodname, "r")) *n = 0; } \
            if(*n >= 0) *n += count; \
            const char *pt = strstr(goodname, basename); \
            copystring(modname, goodname, pt - goodname + 1); \
            if(goodname != pt && !usedmods.access(modname)) usedmods.access(newstring(modname), 0); \
        }
    // skybox textures
    loopi(6)
    {
        extern Texture *sky[];
        Texture *t = sky[i];
        if(t == notexture) { if(!silent) conoutf("sky texture %d doesn't exist", i); }
        else USEFILENAME("%s", t->name, 1);
    }
    // map textures (cubes & models)
    int texuse[256] = { 0 };
    loopj(ssize - 2)
    {
        loopk(ssize - 2)
        {
            if(SOLID(S(j + 1, k + 1)) && texuse[S(j + 1, k + 1)->wtex] && SOLID(S(j, k + 1)) && SOLID(S(j + 1, k)) && SOLID(S(j + 2, k + 1)) && SOLID(S(j + 1, k + 2))) continue; // no side visible
            sqr *s = S(j + 1, k + 1);
            char ttexuse[256] = { 0 };
            ttexuse[s->wtex] = 1;
            if(!SOLID(s)) ttexuse[s->utex] = ttexuse[s->ftex] = ttexuse[s->ctex] = 1;
            loopi(256) texuse[i] += ttexuse[i];
        }
    }
    extern vector<mapmodelinfo> mapmodels;
    loopv(ents) if(ents[i].type == MAPMODEL && mapmodels.inrange(ents[i].attr2) && ents[i].attr4) texuse[ents[i].attr4]++;
    int used = 0;
    loopi(256) if(texuse[i])
    {
        Texture *t = lookuptexture(i);
        if(t == notexture) { if(!silent) conoutf("texture slot %3d doesn't exist (used %d times)", i, texuse[i]); }
        else
        {
            used++;
            USEFILENAME("%s", t->name, texuse[i]);
        }

    }
    if(!silent) conoutf("used: %d texture slots", used);
    // mapmodels
    int mmuse[256] = { 0 };
    used = 0;
    loopv(ents) if(ents[i].type == MAPMODEL) mmuse[ents[i].attr2]++;
    loopi(256) if(mmuse[i])
    {
        if(!mapmodels.inrange(i)) { if(!silent) conoutf("mapmodel slot %3d doesn't exist (used %d times)", i, mmuse[i]); }
        else
        {
            used++;
            USEFILENAME("packages/models/%s", mapmodels[i].name, mmuse[i]);
        }
    }
    if(!silent) conoutf("used: %d mapmodel slots", used);
    // sounds
    int msuse[128] = { 0 };
    used = 0;
    loopv(ents) if(ents[i].type == SOUND && ents[i].attr1 >= 0) msuse[ents[i].attr1]++;
    loopi(128) if(msuse[i])
    {
        if(!mapsounds.inrange(i)) { if(!silent) conoutf("mapsound slot %3d doesn't exist (used %d times)", i, msuse[i]); }
        else if(mapsounds[i].buf && mapsounds[i].buf->name)
        {
            used++;
            USEFILENAME("packages/audio/sounds/%s", mapsounds[i].buf->name, msuse[i]);
        }
    }
    if(!silent) conoutf("used: %d mapsound slots", used);
    // used mods
    int usedm = 0;
    allmods[0] = '\0';
    enumeratek(usedmods, const char *, key, concatformatstring(allmods, "%s%s",*allmods ? ", " : "", key); delete key; usedm++);
    extern string clientmap;
    if(!silent) conoutf("used: %s%s %d total packages (%s.cfg)", allmods, usedm ? ", " : "", usedm, clientmap);
    // mediapacks
    int usedf = mufpaths.numelems;
    int *packn = new int[usedf];
    loopi(usedf) packn[i] = -2; // -2: unset, -1: local-only, 0: release, 1..n: i in loopv(mpdefs)
    int idx;
    copystring(reqmpak, "");
    reqmpak[0] = '\0';
    // First check releasefiles
    int mpfl;
    char* acrfc = loadfile("packages/misc/releasefiles.txt", &mpfl, "r");
    if(acrfc)
    {
        idx = -1;
        enumeratek(mufpaths, const char *, key,
            idx++;
            if(packn[idx]==-2)
            {
                char* p = strstr(acrfc, key);
                if(p) packn[idx] = 0;
            }
        );
    }
    else conoutf("ERROR: your installation is missing the packages/misc/releasefiles.txt - please do a CLEAN re-install!"); // even if silent == true
    // Now check MediaPack definition files
    vector<char *> mpdefs;
    listfiles("packages/mediapack", "txt", mpdefs);
    int usedmpaks = 0;
    loopvj(mpdefs)
    {
        bool usethis = false;
        int mpfl;
        defformatstring(p2mpdf)("packages/mediapack/%s.txt", mpdefs[j]);
        char* mpfc = loadfile(p2mpdf, &mpfl, "r");
        if(mpfc)
        {
            if(!silent) conoutf("found mediapack %s [%d]", mpdefs[j], mpfl);
            idx = -1;
            enumeratek(mufpaths, const char *, key,
                idx++;
                if(packn[idx]==-2)
                {
                    const char* gp = strstr(key, "packages/");
                    if(gp)
                    {
                        const char* fp = strstr(mpfc, gp);
                        if(fp)
                        {
                            packn[idx] = j + 1;
                            if(!usethis)
                            {
                                formatstring(reqmpak)("%s%s%s", reqmpak, reqmpak[0]=='\0'?"":",", mpdefs[j]);
                                if(++usedmpaks > 0) usethis = true; // these packs are already on the system
                                if(usedmpaks>25) conoutf("this map requires too many mediapacks."); // even if silent == true
                            }
                        }
                    }
                }
            )
        }
    }
    idx = -1;
    if(!silent && details)
    {
        conoutf("mediapack details:");
        // packn[idx]==0?"release":
        enumeratek(mufpaths, const char *, key, idx++; if(packn[idx]!=0) conoutf("%3d: %s : %d : %s", idx, key, packn[idx], packn[idx]<0?"LOCAL":mpdefs[packn[idx]-1]); );
    }
    if(usedmpaks>0) conoutf("this map requires the following %d mediapack%s: %s", usedmpaks, usedmpaks==1?"":"s", reqmpak);
    else
    {
        if(hdr.mediareq[0] && !silent)
        {
            char sep[] = ",";
            char *pch;
            pch = strtok (hdr.mediareq,sep);
            while (pch != NULL)
            {
                conoutf("http://www.26things.com/mrx/get.php?mpid=%s", pch);
                pch = strtok (NULL, sep);
            }
        }
    }
    enumeratek(mufpaths, const char *, key, mufpaths.remove(key)); // don't report false positives next time round
}
void wrapCMD(int i) { checkmapdependencies(false, i!=0); }
//COMMAND(checkmapdependencies, ARG_NONE); // for some reason this still results in silent==true - WTF? It used to work with the _proper_ default.
COMMANDN(checkmapdependencies, wrapCMD, ARG_1INT);

void save_world(char *mname)
{
    if(!*mname) mname = getclientmap();
    if(securemapcheck(mname)) return;
    voptimize();
    toptimize();
    setnames(mname);
    backup(cgzname, bakname);
    // MediaPacks (flowtron) 2009oct07
	//  	- check map dependencies and
	// 		  set the appropriate value (NULL, "-1" or "A,B,C,...")
	//  	  in the header
    checkmapdependencies(true);
    copystring(hdr.mediareq, reqmpak, 128);
	//  	?? output? IIRC I did some somewhere .. 2010apr02
    //  	:: -- - MediaPack - -- --- ----
    stream *f = opengzfile(cgzname, "wb");
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

extern int mapdims[6];     // min/max X/Y and delta X/Y
void showmapdims()
{
    conoutf("  min X|Y: %3d : %3d", mapdims[0], mapdims[1]);
    conoutf("  max X|Y: %3d : %3d", mapdims[2], mapdims[3]);
    conoutf("delta X|Y: %3d : %3d", mapdims[4], mapdims[5]);
}
COMMAND(showmapdims, ARG_NONE);

extern void preparectf(bool cleanonly = false);
int numspawn[3], maploaded = 0, numflagspawn[2];
VAR(curmaprevision, 1, 0, 0);

extern char *mlayout;
extern int Mv, Ma;
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
    stream *f = opengzfile(cgzname, "rb");
    if(!f) { conoutf("\f3could not read map %s", cgzname); return false; }
    header tmp;
    memset(&tmp, 0, sizeof(header));
    if(f->read(&tmp, sizeof(header)-sizeof(int)*16-sizeof(char)*128)!=sizeof(header)-sizeof(int)*16-sizeof(char)*128) { conoutf("\f3while reading map: header malformatted"); delete f; return false; }
    lilswap(&tmp.version, 4);
    if(strncmp(tmp.head, "CUBE", 4)!=0 && strncmp(tmp.head, "ACMP",4)!=0) { conoutf("\f3while reading map: header malformatted"); delete f; return false; }
    if(tmp.version>MAPVERSION) { conoutf("\f3this map requires a newer version of AssaultCube"); delete f; return false; }
    if(tmp.sfactor<SMALLEST_FACTOR || tmp.sfactor>LARGEST_FACTOR || tmp.numents > MAXENTITIES) { conoutf("\f3illegal map size"); delete f; return false; }
    if(tmp.version>=4 && f->read(&tmp.waterlevel, sizeof(int)*16)!=sizeof(int)*16) { conoutf("\f3while reading map: header malformatted"); delete f; return false; }
    if(tmp.version>=7 && f->read(&tmp.mediareq, sizeof(char)*128)!=sizeof(char)*128) { conoutf("\f3while reading map: header malformatted"); delete f; return false; }
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
    if(hdr.version<7) hdr.mediareq[0] = '\0';
    else
    {
        if(hdr.mediareq[0])
        {
            conoutf("this map requires the following mediapacks: %s", hdr.mediareq); // TODO: check for client meeting these requirements - hint: listfiles
            conoutf("to get a set of URLs to download them please run \f2checkmapdependencies\f5 manually.");
        }
        //else conoutf("this map works with the vanilla release");
        // TODO: output the requirements into a file for easy user-retrieval & -action - needs a base-URL where we will hold the mediapacks
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
    char diff = 0;
    Mv = Ma = 0;

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
    loopk(4) mapdims[k] = k < 2 ? ssize : 0;
    loopk(cubicsize) if (world[k].type != SOLID)
    {
        int cwx = k%ssize,
            cwy = k/ssize;
        if(cwx < mapdims[0]) mapdims[0] = cwx;
        if(cwy < mapdims[1]) mapdims[1] = cwy;
        if(cwx > mapdims[2]) mapdims[2] = cwx;
        if(cwy > mapdims[3]) mapdims[3] = cwy;
    }
    loopk(2) mapdims[k+4] = mapdims[k+2] - mapdims[k];
    calclight();
    conoutf("read map %s rev %d (%d milliseconds)", cgzname, hdr.maprevision, watch.stop());
    conoutf("%s", hdr.maptitle);
    pushscontext(IEXC_MAPCFG); // untrusted altogether
    persistidents = false;
    execfile("config/default_map_settings.cfg");
    execfile(pcfname);
    execfile(mcfname);
    persistidents = true;
    popscontext();

    c2skeepalive();
    watch.start();
    loopi(256) if(texuse[i]) lookupworldtexture(i);
    printf("loaded textures (%d milliseconds)\n", watch.stop());
    c2skeepalive();
    watch.start();
    preload_mapmodels();
    printf("loaded mapmodels (%d milliseconds)\n", watch.stop());
    c2skeepalive();
    watch.start();
    audiomgr.preloadmapsounds();
    printf("loaded mapsounds (%d milliseconds)\n", watch.stop());
    c2skeepalive();

    startmap(mname);
    return true;
}

COMMANDN(savemap, save_world, ARG_1STR);

// FIXME - remove this before release
void setmaprevision(int rev) { hdr.maprevision = rev; }
COMMAND(setmaprevision, ARG_1INT);

#define MAPDEPFILENAME "mapdependencies.txt"
void listmapdependencies(char *mapname)  // print map dependencies to file
{
    static hashtable<const char *, int> sumpaths;
    hashtable<const char *, int> usedmods;
    string fullname, modname, allmods;

    #define ADDFILENAME(prefix, fname, count) \
        { \
            defformatstring(basename)(prefix, fname); \
            copystring(fullname, findfile(basename, "r")); \
            int *n = sumpaths.access(fullname); \
            if(!n) { n = &sumpaths.access(newstring(fullname), -1); if(fileexists(fullname, "r")) *n = 0; } \
            if(*n >= 0) *n += count; \
            const char *pt = strstr(fullname, basename); \
            copystring(modname, fullname, pt - fullname + 1); \
            if(fullname != pt && !usedmods.access(modname)) usedmods.access(newstring(modname), 0); \
        }

    if(multiplayer()) return;
    stream *f = openfile(MAPDEPFILENAME, "a");
    if(!f) { conoutf("\f3could not append to %s", MAPDEPFILENAME); return; }
    if(!mapname || !*mapname)
    { // print summary
        vector<const char *> allres;
        enumeratek(sumpaths, const char *, key, allres.add(key));
        allres.sort(stringsort);
        f->printf("used ressources total:\n");
        loopv(allres) f->printf("    used %6d times:  \"%s\"\n", *sumpaths.access(allres[i]), allres[i]);
        enumeratek(sumpaths, const char *, key, delete key);
        sumpaths.clear();
        f->printf("  %d files used\n\n\n", allres.length());
    }
    else if(load_world(mapname))
    { // print map deps
        filtertext(fullname, hdr.maptitle, 1);
        f->printf("--  --:--  --  --  --\n   map: %s\n        %s\nspawns: FFA %d\tCLA %2d RVSF %2d\tflags: %d+%d\n  size: %d : %8d\trevision: %d\n--  --:--  --  --  --\n",
            cgzname, fullname,
            numspawn[2], numspawn[0], numspawn[1],
            numflagspawn[0], numflagspawn[1],
            sfactor, maploaded,
            hdr.maprevision
        );
        loopi(6)
        {
            extern Texture *sky[];
            Texture *t = sky[i];
            if(t == notexture) f->printf("      sky texture %d doesn't exist\n", i);
            else
            {
                ADDFILENAME("%s", t->name, 1);
                f->printf("      sky texture %d, \"%s\"\n", i, fullname);
            }
        }
        int texuse[256] = { 0 };
        loopj(ssize - 2)
        {
            loopk(ssize - 2)
            {
                if(SOLID(S(j + 1, k + 1)) && texuse[S(j + 1, k + 1)->wtex] && SOLID(S(j, k + 1)) && SOLID(S(j + 1, k)) && SOLID(S(j + 2, k + 1)) && SOLID(S(j + 1, k + 2))) continue; // no side visible
                sqr *s = S(j + 1, k + 1);
                char ttexuse[256] = { 0 };
                ttexuse[s->wtex] = 1;
                if(!SOLID(s)) ttexuse[s->utex] = ttexuse[s->ftex] = ttexuse[s->ctex] = 1;
                loopi(256) texuse[i] += ttexuse[i];
            }
        }
        extern vector<mapmodelinfo> mapmodels;
        loopv(ents) if(ents[i].type == MAPMODEL && mapmodels.inrange(ents[i].attr2) && ents[i].attr4) texuse[ents[i].attr4]++;
        int used = 0;
        loopi(256) if(texuse[i])
        {
            Texture *t = lookuptexture(i);
            if(t == notexture) f->printf("      texture slot %3d doesn't exist (used %d times)\n", i, texuse[i]);
            else
            {
                used++;
                ADDFILENAME("%s", t->name, texuse[i]);
                f->printf("      texture slot %3d used %5d times, \"%s\"\n", i, texuse[i], fullname);
            }
        }
        f->printf("  used: %d texture slots\n", used);
        // mapmodels
        int mmuse[256] = { 0 };
        used = 0;
        loopv(ents) if(ents[i].type == MAPMODEL) mmuse[ents[i].attr2]++;
        loopi(256) if(mmuse[i])
        {
            if(!mapmodels.inrange(i)) f->printf("      mapmodel slot %3d doesn't exist (used %d times)\n", i, mmuse[i]);
            else
            {
                used++;
                ADDFILENAME("packages/models/%s", mapmodels[i].name, mmuse[i]);
                f->printf("      mapmodel slot %3d used %4d times, \"%s\"\n", i, mmuse[i], fullname);
            }
        }
        f->printf("  used: %d mapmodel slots\n", used);
        // sounds
        int msuse[128] = { 0 };
        used = 0;
        loopv(ents) if(ents[i].type == SOUND && ents[i].attr1 >= 0) msuse[ents[i].attr1]++;
        loopi(128) if(msuse[i])
        {
            if(!mapsounds.inrange(i)) f->printf("      mapsound slot %3d doesn't exist (used %d times)\n", i, msuse[i]);
            else if(mapsounds[i].buf && mapsounds[i].buf->name)
            {
                used++;
                ADDFILENAME("packages/audio/sounds/%s", mapsounds[i].buf->name, msuse[i]);
                f->printf("      mapsound slot %3d used %4d times, \"%s\"\n", i, msuse[i], fullname);
            }
        }
        f->printf("  used: %d mapsound slots\n", used);

        int usedm = 0;
        allmods[0] = '\0';
        enumeratek(usedmods, const char *, key, concatformatstring(allmods, "%s%s",*allmods ? ", " : "", key); delete key; usedm++);
        pushscontext(IEXC_MAPCFG); // untrusted altogether
        persistidents = false;
        execfile(mcfname);
        f->printf("  used: %s%s %d total packages (%s.cfg)\n", allmods, usedm ? ", " : "", usedm, mapname);
        if(usedm) conoutf("  used: %s (%s.cfg) in %s", allmods, mapname, mapname);
        persistidents = true;
        popscontext();
        f->printf("\n\n");
    }
    delete f;
}

COMMAND(listmapdependencies, ARG_1STR);

void listmapdependencies_all(int sure)
{
    if(sure != 42 || multiplayer()) return;

    stream *f = openfile(MAPDEPFILENAME, "w");
    delete f;
    vector<char *> files;
    listfiles("packages/maps", "cgz", files);
    listfiles("packages/maps/official", "cgz", files);
    files.sort(stringsort);
    conoutf("%d maps to go...", files.length());
    loopv(files) listmapdependencies(files[i]);
    listmapdependencies(NULL);
}
COMMAND(listmapdependencies_all, ARG_1INT);
