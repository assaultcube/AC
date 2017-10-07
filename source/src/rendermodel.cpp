#include "cube.h"

VARP(animationinterpolationtime, 0, 150, 1000);

model *loadingmodel = NULL;
mapmodelattributes loadingattributes;

#include "tristrip.h"
#include "modelcache.h"
#include "vertmodel.h"
#include "md2.h"
#include "md3.h"

#define checkmdl if(!loadingmodel) { conoutf("not loading a model"); flagmapconfigerror(LWW_MODELERR); scripterr(); return; }

void mdlcullface(int *cullface)
{
    checkmdl;
    loadingmodel->cullface = *cullface!=0;
}

COMMAND(mdlcullface, "i");

void mdlvertexlight(int *vertexlight)
{
    checkmdl;
    loadingmodel->vertexlight = *vertexlight!=0;
}

COMMAND(mdlvertexlight, "i");

void mdltranslucent(int *translucency)
{
    checkmdl;
    loadingmodel->translucency = *translucency/100.0f;
}

COMMAND(mdltranslucent, "i");

void mdlalphatest(int *alphatest)
{
    checkmdl;
    loadingmodel->alphatest = *alphatest/100.0f;
}

COMMAND(mdlalphatest, "i");

void mdlalphablend(int *alphablend) //ALX Alpha channel models
{
    checkmdl;
    loadingmodel->alphablend = *alphablend!=0;
}
COMMAND(mdlalphablend, "i");

void mdlscale(int *percent)
{
    checkmdl;
    float scale = 0.3f;
    if(*percent>0) scale = *percent/100.0f;
    else if(*percent<0) scale = 0.0f;
    loadingmodel->scale = scale;
}

COMMAND(mdlscale, "i");

void mdltrans(float *x, float *y, float *z)
{
    checkmdl;
    loadingmodel->translate = vec(*x, *y, *z);
}

COMMAND(mdltrans, "fff");

void mdlshadowdist(int *dist)
{
    checkmdl;
    loadingmodel->shadowdist = *dist;
}

COMMAND(mdlshadowdist, "i");

void mdlcachelimit(int *limit)
{
    checkmdl;
    loadingmodel->cachelimit = *limit;
}

COMMAND(mdlcachelimit, "i");

const char *mdlattrnames[] = { "keywords", "desc", "defaults", "usage", "author", "license", "distribution", "version", "requires", "" };

void mdlattribute(char *attrname, char *val)
{
    checkmdl;
    int i = getlistindex(attrname, mdlattrnames, true, MMA_NUM);
    if(i < MMA_NUM)
    {
        DELSTRING(loadingattributes.n[i]);
        filtertext(val, val, FTXT__MDLATTR);
        if(*val) loadingattributes.n[i] = newstring(val);
    }
}

COMMAND(mdlattribute, "ss");

VAR(mapmodelchanged, 0, 0, 1);

vector<mapmodelinfo> mapmodels;
const char *mmpath = "mapmodels/";
const char *mmshortname(const char *name) { return !strncmp(name, mmpath, strlen(mmpath)) ? name + strlen(mmpath) : name; }

void mapmodel(int *rad, int *h, int *zoff, char *scale, char *name, char *flags)
{
    if(*scale && *name) // ignore "mapmodel" commands with insufficient parameters
    {
        if(!_ignoreillegalpaths && !strchr(name, '/') && !strchr(name, '\\')) { flagmapconfigerror(LWW_CONFIGERR * 2); } // throw errors for unconverted mapmodels (unspecific, because not all get detected)
        intret(mapmodels.length());
        mapmodelinfo &mmi = mapmodels.add();
        mmi.rad = *rad;
        mmi.h = *h;
        mmi.zoff = *zoff;
        mmi.scale = atof(scale);
        mmi.flags = 0;
        if(*flags) mmi.flags = ATOI(flags);
        if(mmi.scale < 0.25f || mmi.scale > 4.0f)
        {
            mmi.scale = 1.0f;
            if(strcmp(scale, "0")) { flagmapconfigerror(LWW_CONFIGERR * 2); scripterr(); }
        }
        mmi.m = NULL;

        filtertext(name, name, FTXT__MEDIAFILEPATH);
        formatstring(mmi.name)("%s%s", mmpath, name[0] == '.' && name[1] == '/' ? name + 2 : name);
        mapmodelchanged = 1;
        flagmapconfigchange();
    }
    else { flagmapconfigerror(LWW_CONFIGERR * 2); scripterr(); }
}
COMMAND(mapmodel, "iiisss");

void mapmodelreset()
{
    if(execcontext==IEXC_MAPCFG)
    {
        mapmodels.shrink(0);
        mapmodelchanged = 1;
    }
}
COMMAND(mapmodelreset, "");

mapmodelinfo *getmminfo(int i) { return mapmodels.inrange(i) ? &mapmodels[i] : NULL; }

COMMANDF(mapmodelslotname, "i", (int *idx) { result(mapmodels.inrange(*idx) ? mmshortname(mapmodels[*idx].name) : ""); }); // returns the model name that is configured in a certain slot
COMMANDF(mapmodelslotbyname, "s", (char *name) // returns the slot(s) that a certain mapmodel is configured in
{
    string res = "";
    loopv(mapmodels) if(!strcmp(name, mapmodels[i].name)) concatformatstring(res, "%s%d", *res ? " " : "", i);
    result(res);
});

void mapmodelslotusage(int *n) // returns all entity indices that contain a mapmodel of slot n
{
    string res = "";
    loopv(ents) if(ents[i].type == MAPMODEL && ents[i].attr2 == *n) concatformatstring(res, "%s%d", i ? " " : "", i);
    result(*res ? res : (mapmodels.inrange(*n) && (mapmodels[*n].flags & MMF_REQUIRED) ? " " : res));
}
COMMAND(mapmodelslotusage, "i");

void deletemapmodelslot(int *n, char *opt) // delete mapmodel slot - only if unused or "purge" is specified
{
    EDITMP("deletemapmodelslot");
    if(!mapmodels.inrange(*n)) return;
    bool purgeall = !strcmp(opt, "purge");
    if(!purgeall) loopv(ents) if(ents[i].type == MAPMODEL && ents[i].attr2 == *n) { conoutf("mapmodel slot #%d is in use: can't delete", *n); return; }
    int deld = 0;
    loopv(ents) if(ents[i].type == MAPMODEL)
    {
        entity &e = ents[i];
        if(e.attr2 == *n)
        { // delete entity
            deleted_ents.add(e);
            memset(&e, 0, sizeof(persistent_entity));
            e.type = NOTUSED;
            deld++;
        }
        else if(e.attr2 > *n) e.attr2--; // adjust models in higher slots
    }
    mapmodels.remove(*n);
    defformatstring(s)(" (%d mapmodels purged)", deld);
    conoutf("mapmodel slot #%d deleted%s", *n, deld ? s : "");
    unsavededits++;
    mapmodelchanged = 1;
    hdr.flags |= MHF_AUTOMAPCONFIG; // requires automapcfg
}
COMMAND(deletemapmodelslot, "is");

void editmapmodelslot(int *n, char *rad, char *h, char *zoff, char *scale, char *name) // edit slot parameters != ""
{
    string res = "";
    if(mapmodels.inrange(*n))
    {
        mapmodelinfo &mmi = mapmodels[*n];
        if((*rad || *h || *zoff || *scale || *name) && !noteditmode("editmapmodelslot") && !multiplayer("editmapmodelslot"))
        { // change attributes
            if(*rad) mmi.rad = strtol(rad, NULL, 0);
            if(*h) mmi.h = strtol(h, NULL, 0);
            if(*zoff) mmi.zoff = strtol(zoff, NULL, 0);
            float s = atof(scale);
            if(s < 0.25f || s > 4.0f) s = 1.0f;
            if(*scale) mmi.scale = s;
            mmi.m = NULL;
            if(*name) formatstring(mmi.name)("%s%s", mmpath, name);
            unsavededits++;
            mapmodelchanged = 1;
            hdr.flags |= MHF_AUTOMAPCONFIG; // requires automapcfg
        }
        formatstring(res)("%d %d %d %s \"%s\"", mmi.rad, mmi.h, mmi.zoff, floatstr(mmi.scale, true), mmshortname(mmi.name)); // give back all current attributes
    }
    result(res);
}
COMMAND(editmapmodelslot, "isssss");

struct tempmmslot { mapmodelinfo m; vector<int> oldslot; bool used; };

int tempmmcmp(tempmmslot *a, tempmmslot *b)
{
    int n = strcmp(a->m.name, b->m.name);
    if(n) return n;
    if(a->m.rad != b->m.rad) return a->m.rad - b->m.rad;
    if(a->m.h != b->m.h) return a->m.h - b->m.h;
    if(a->m.zoff != b->m.zoff) return a->m.zoff - b->m.zoff;
    if(a->m.scale != b->m.scale) return int((a->m.scale - b->m.scale) * 1e6);
    return 0;
}

int tempmmsort(tempmmslot *a, tempmmslot *b)
{
    int n = tempmmcmp(a, b);
    return n ? n : a->oldslot[0] - b->oldslot[0];
}

int tempmmunsort(tempmmslot *a, tempmmslot *b)
{
    return a->oldslot[0] - b->oldslot[0];
}

void sortmapmodelslots(char **args, int numargs)
{
    bool nomerge = false, mergeused = false, nosort = false, unknownarg = false;
    loopi(numargs) if(args[i][0])
    {
        if(!strcasecmp(args[i], "nomerge")) nomerge = true;
        else if(!strcasecmp(args[i], "nosort")) nosort = true;
        else if(!strcasecmp(args[i], "mergeused")) mergeused = true;
        else { conoutf("sortmapmodelslots: unknown argument \"%s\"", args[i]); unknownarg = true; }
    }

    EDITMP("sortmapmodelslots");
    if(unknownarg || mapmodels.length() < 3) return;

    vector<tempmmslot> tempslots;
    loopv(mapmodels)
    {
        tempslots.add().m = mapmodels[i];
        tempslots.last().oldslot.add(i);
        tempslots.last().used = false;
    }
    loopv(ents) if(ents[i].type == MAPMODEL && tempslots.inrange(ents[i].attr2)) tempslots[ents[i].attr2].used = true;
    tempslots.sort(tempmmsort);

    // remove double entries
    if(!nomerge) loopvrev(tempslots) if(i > 0)
    {
        tempmmslot &s1 = tempslots[i], &s0 = tempslots[i - 1];
        if(!tempmmcmp(&s0, &s1) && (mergeused || !s0.used || !s1.used))
        {
            if(s1.used) s0.used = true;
            loopvj(s1.oldslot) s0.oldslot.add(s1.oldslot[j]);
            tempslots.remove(i);
        }
    }
    if(nosort) tempslots.sort(tempmmunsort);

    // create translation table
    uchar newslot[256];
    loopk(256) newslot[k] = k;
    loopv(tempslots)
    {
        tempmmslot &t = tempslots[i];
        loopvj(t.oldslot)
        {
            if(t.oldslot[j] < 256) newslot[t.oldslot[j]] = i;
        }
    }

    // translate all mapmodel entities
    loopv(ents) if(ents[i].type == MAPMODEL) ents[i].attr2 = newslot[ents[i].attr2];

    conoutf("%d mapmodel slots%s, %d %sslots merged", tempslots.length(), nosort ? "" : " sorted", mapmodels.length() - tempslots.length(), mergeused ? "" : "unused ");

    // rewrite mapmodel slot list
    mapmodels.shrink(tempslots.length());
    loopv(tempslots)
    {
        mapmodels[i] = tempslots[i].m;
        mapmodels[i].m = NULL;
    }
    unsavededits++;
    mapmodelchanged = 1;
    hdr.flags |= MHF_AUTOMAPCONFIG; // requires automapcfg
}
COMMAND(sortmapmodelslots, "v");

hashtable<const char *, mapmodelattributes *> mdlregistry;

void setmodelattributes(const char *name, mapmodelattributes &ma)
{
    if(!strchr(name, ' ') && !strncmp(name, mmpath, strlen(mmpath))) name += strlen(mmpath); // for now: ignore mapmodels with spaces in the path (next release: ban them)
    else return; // we only want mapmodels
    mapmodelattributes **mrp = mdlregistry.access(name), *mr = mrp ? *mrp : NULL, *r = mr;
    if(!r)
    {
        r = new mapmodelattributes;
        copystring(r->name, name);
    }
    loopi(MMA_NUM)
    {
        if(ma.n[i])
        {
            if(!r->n[i]) mapmodelchanged = 1;
            DELSTRING(r->n[i]);  // overwrite existing attributes
            r->n[i] = ma.n[i];
            ma.n[i] = NULL;
        }
    }
    if(!mr) mdlregistry.access(r->name, r);
}

void mapmodelregister_(char **args, int numargs)  // read model attributes without loading the model
{
    mapmodelattributes ma;
    if(numargs > 0)  // need a name at least
    {
        defformatstring(p)("%s%s", mmpath, args[0]);
        loopirev(--numargs < MMA_NUM ? numargs : MMA_NUM) ma.n[i] = *args[i + 1] ? newstring(args[i + 1]) : NULL;
        setmodelattributes(p, ma);
    }
}
COMMANDN(mapmodelregister, mapmodelregister_, "v");

void mapmodelregistryclear()
{
    enumerate(mdlregistry, mapmodelattributes *, m,
        loopi(MMA_NUM) DELSTRING(m->n[i]);
        DELETEP(m);
    );
    mdlregistry.clear();
    mapmodelchanged = 1;
}
COMMAND(mapmodelregistryclear, "");

hashtable<const char *, model *> mdllookup;
hashtable<const char *, char> mdlnotfound;
bool silentmodelloaderror = false;

model *loadmodel(const char *name, int i, bool trydl)     // load model by name (optional) or from mapmodels[i]
{
    if(!name)                                   // name == NULL -> get index i from mapmodels[]
    {
        if(!mapmodels.inrange(i)) return NULL;
        mapmodelinfo &mmi = mapmodels[i];
        if(mmi.m) return mmi.m;                 // mapmodels[i] was already loaded
        name = mmi.name;
    }
    if(mdlnotfound.access(name)) return NULL;   // already tried to find that earlier -> not available
    model **mm = mdllookup.access(name);
    model *m;
    if(mm) m = *mm;                             // a model of that name is already loaded
    else
    {
        pushscontext(IEXC_MDLCFG);
        m = new md2(name);                      // try md2
        loadingmodel = m;
        loopi(MMA_NUM) DELSTRING(loadingattributes.n[i]);
        if(!m->load())                          // md2 didn't load
        {
            delete m;
            m = new md3(name);                  // try md3
            loadingmodel = m;
            if(!m->load())                      // md3 didn't load -> we don't have that model
            {
                delete m;
                m = loadingmodel = NULL;
                if(trydl && !strncmp(name, mmpath, strlen(mmpath))) requirepackage(PCK_MAPMODEL, name);
                else
                {
                    if(!silentmodelloaderror) conoutf("\f3failed to load model %s", name);
                    mdlnotfound.access(newstring(name), 0);  // do not search for this name again
                }
            }
        }
        popscontext();
        if(loadingmodel && m)
        {
            mdllookup.access(m->name(), m);
            setmodelattributes(m->name(), loadingattributes);
            if(m->shadowdist && !m->cullface)
            {
                conoutf("\f3mapmodel config error: disabling face culling in combination with shadows will cause visual errors (%s)", m->name());
                flagmapconfigerror(LWW_MODELERR);
            }
        }
        loadingmodel = NULL;
    }
    if(mapmodels.inrange(i) && !mapmodels[i].m) mapmodels[i].m = m;
    return m;
}

void cleanupmodels()
{
    enumerate(mdllookup, model *, m, m->cleanup());
}

void resetmdlnotfound()
{
    enumeratek(mdlnotfound, const char *, m, delstring(m));
    mdlnotfound.clear();
}

void getmapmodelattributes(char *name, char *attr)
{
    const char *res = NULL;
    mapmodelattributes **ap = mdlregistry.access(name), *a = ap ? *ap : NULL;
    if(a)
    {
        int i = getlistindex(attr, mdlattrnames, true, MMA_NUM);
        if(i < MMA_NUM) res = a->n[i];
        else
        {
            string s = "";
            loopi(MMA_NUM) if(a->n[i]) concatformatstring(s, " %s:\"%s\"", mdlattrnames[i], a->n[i]);
            conoutf("%s:%s", a->name, *s ? s : " <no attributes set>");
        }
    }
    result(res ? res : "");
}
COMMAND(getmapmodelattributes, "ss");

void updatemapmodeldependencies()
{
    loopv(mapmodels) mapmodels[i].flags = 0;
    loopv(ents) if(ents[i].type == MAPMODEL && mapmodels.inrange(ents[i].attr2)) mapmodels[ents[i].attr2].flags |= MMF_TEMP_USED;
    bool foundone = true;
    while(foundone)
    {
        foundone = false;
        loopvk(mapmodels) if(mapmodels[k].flags & MMF_TEMP_USED)
        {
            const char *name = mmshortname(mapmodels[k].name);
            mapmodelattributes **ap = mdlregistry.access(name), *a = ap ? *ap : NULL;
            const char *dep = a ? a->n[MMA_REQUIRES] : NULL;
            if(dep && *dep)
            {
                bool found = false;
                loopv(mapmodels) if(!strcmp(mmshortname(mapmodels[i].name), dep))
                {
                    if(!(mapmodels[i].flags & MMF_TEMP_USED)) foundone = true;
                    mapmodels[i].flags |= MMF_REQUIRED | MMF_TEMP_USED;
                    found = true;
                }
                if(!found)
                {
                    defformatstring(cmd)("mapmodel 0 0 0 0 \"%s\" %d", dep, MMF_REQUIRED);
                    execute(cmd);
                    foundone = true;
                    clientlogf(" mapmodel %s added to config, because %s requires it", dep, name);
                }
            }
        }
    }
    loopv(mapmodels) mapmodels[i].flags &= MMF_CONFIGMASK;
}
COMMAND(updatemapmodeldependencies, "");

static int mmasortorder = 0;
int mmasort(mapmodelattributes **a, mapmodelattributes **b) { return strcmp((*a)->name, (*b)->name); }
int mmasort2(mapmodelattributes **a, mapmodelattributes **b)
{
    const char *aa = (*a)->n[mmasortorder], *bb = (*b)->n[mmasortorder], nn[2] = { 127, 0 };
    int i = strcmp(aa ? aa : nn, bb ? bb : nn);
    return i ? i : (*a)->tmp - (*b)->tmp;
}

void writemapmodelattributes()
{
    stream *f = openfile("config" PATHDIVS "mapmodelattributes.cfg", "w");
    if(f)
    {
        vector<mapmodelattributes *> mmas;
        enumerate(mdlregistry, mapmodelattributes *, m, mmas.add(m));
        mmas.sort(mmasort);
        f->printf("// automatically written on exit. this is cached information extracted from model configs. no point in editing it.\n// [path %s]\n", conc(mdlattrnames, MMA_NUM, true)); // memory leak, but w/e
        loopv(mmas)
        {
            f->printf("\nmapmodelregister %s", mmas[i]->name);
            loopj(MMA_NUM) f->printf(" %s", escapestring(mmas[i]->n[j]));  // escapestring can handle NULL
        }
        f->printf("\n\n");
        delete f;
    }
}

void listallmapmodelattributes(char **args, int numargs) // create a list of mapmodel paths and selected attributes
{
    // parse argument list
    vector<const char *> opts;
    loopi(MMA_NUM) opts.add(mdlattrnames[i]);
    opts.add("sortby:"); opts.add("explodekeywords"); opts.add("");
    vector<int> a;
    loopi(numargs) a.add(getlistindex(args[i], (const char **)opts.getbuf(), false, opts.length()));
    bool explodekeywords = false;
    loopi(numargs) if(a[i] == MMA_NUM + 1) explodekeywords = true;

    // build list of mapmodels (explode if necessary)
    vector<mapmodelattributes *> mmas, emmas;
    enumerate(mdlregistry, mapmodelattributes *, m,
    {
        if(explodekeywords && m->n[MMA_KEYWORDS] && listlen(m->n[MMA_KEYWORDS]) > 1)
        {
            vector<char *> keys;
            explodelist(m->n[MMA_KEYWORDS], keys);
            loopv(keys)
            {
                mmas.add(new mapmodelattributes(*m));
                emmas.add(mmas.last())->n[MMA_KEYWORDS] = keys[i];
            }
        }
        else mmas.add(m);
    });

    // sort the list
    mmas.sort(mmasort);
    loopirev(numargs - 1) if(a[i] == MMA_NUM) // sortby:
    {
        loopvj(mmas) mmas[j]->tmp = j; // store last sort order, because quicksort is not stable
        if((mmasortorder = a[i + 1]) < MMA_NUM) mmas.sort(mmasort2);
    }

    // assemble output
    vector<char> res;
    loopv(mmas)
    {
        cvecprintf(res, "%s", mmas[i]->name);
        loopvj(a) if(a[j] < MMA_NUM) cvecprintf(res, " %s", escapestring(mmas[i]->n[a[j]]));
        res.add('\n');
    }
    loopv(emmas) { delstring(emmas[i]->n[MMA_KEYWORDS]); delete emmas[i]; } // only delete what was exploded
    resultcharvector(res, -1);
}
COMMAND(listallmapmodelattributes, "v");

VARP(dynshadow, 0, 40, 100);
VARP(dynshadowdecay, 0, 1000, 3000);

struct batchedmodel
{
    vec o;
    int anim, varseed, tex;
    float roll, yaw, pitch, speed;
    int basetime;
    playerent *d;
    int attached;
    float scale;
};
struct modelbatch
{
    model *m;
    vector<batchedmodel> batched;
};
static vector<modelbatch *> batches;
static vector<modelattach> modelattached;
static int numbatches = -1;

void startmodelbatches()
{
    numbatches = 0;
    modelattached.setsize(0);
}

batchedmodel &addbatchedmodel(model *m)
{
    modelbatch *b = NULL;
    if(m->batch>=0 && m->batch<numbatches && batches[m->batch]->m==m) b = batches[m->batch];
    else
    {
        if(numbatches<batches.length())
        {
            b = batches[numbatches];
            b->batched.setsize(0);
        }
        else b = batches.add(new modelbatch);
        b->m = m;
        m->batch = numbatches++;
    }
    return b->batched.add();
}

void renderbatchedmodel(model *m, batchedmodel &b)
{
    modelattach *a = NULL;
    if(b.attached>=0) a = &modelattached[b.attached];

    if(stenciling)
    {
        m->render(b.anim|ANIM_NOSKIN, b.varseed, b.speed, b.basetime, b.o, b.roll, b.yaw, b.pitch, b.d, a, b.scale);
        return;
    }

    int x = (int)b.o.x, y = (int)b.o.y;
    if(!OUTBORD(x, y))
    {
        sqr *s = S(x, y);
        glColor3ub(s->r, s->g, s->b);
    }
    else glColor3f(1, 1, 1);

    m->setskin(b.tex);

    if(b.anim&ANIM_TRANSLUCENT)
    {
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        m->render(b.anim|ANIM_NOSKIN, b.varseed, b.speed, b.basetime, b.o, b.roll, b.yaw, b.pitch, b.d, a, b.scale);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glDepthFunc(GL_LEQUAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        GLfloat color[4];
        glGetFloatv(GL_CURRENT_COLOR, color);
        glColor4f(color[0], color[1], color[2], m->translucency);
    }

    m->render(b.anim, b.varseed, b.speed, b.basetime, b.o, b.roll, b.yaw, b.pitch, b.d, a, b.scale);

    if(b.anim&ANIM_TRANSLUCENT)
    {
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);
    }
}

void renderbatchedmodelshadow(model *m, batchedmodel &b)
{
    int x = (int)b.o.x, y = (int)b.o.y;
    if(OUTBORD(x, y)) return;
    sqr *s = S(x, y);
    vec center(b.o.x, b.o.y, s->floor);
    if(s->type==FHF) center.z -= s->vdelta/4.0f;
    if(dynshadowquad && center.z-0.1f>b.o.z) return;
    center.z += 0.1f;
    modelattach *a = NULL;
    if(b.attached>=0) a = &modelattached[b.attached];
    float intensity = dynshadow/100.0f;
    if(dynshadowdecay) switch(b.anim&ANIM_INDEX)
    {
        case ANIM_DECAY:
        case ANIM_LYING_DEAD:
            intensity *= max(1.0f - float(lastmillis - b.basetime)/dynshadowdecay, 0.0f);
            break;
    }
    glColor4f(0, 0, 0, intensity);
    m->rendershadow(b.anim, b.varseed, b.speed, b.basetime, dynshadowquad ? center : b.o, b.yaw, a);
}

static int sortbatchedmodels(const batchedmodel *x, const batchedmodel *y)
{
    if(x->tex < y->tex) return -1;
    if(x->tex > y->tex) return 1;
    return 0;
}

struct translucentmodel
{
    model *m;
    batchedmodel *batched;
    float dist;
};

static int sorttranslucentmodels(const translucentmodel *x, const translucentmodel *y)
{
    if(x->dist > y->dist) return -1;
    if(x->dist < y->dist) return 1;
    return 0;
}

void clearmodelbatches()
{
    numbatches = -1;
}

void endmodelbatches(bool flush)
{
    vector<translucentmodel> translucent;
    loopi(numbatches)
    {
        modelbatch &b = *batches[i];
        if(b.batched.empty()) continue;
        loopvj(b.batched) if(b.batched[j].tex) { b.batched.sort(sortbatchedmodels); break; }
        b.m->startrender();
        loopvj(b.batched)
        {
            batchedmodel &bm = b.batched[j];
            if(bm.anim&ANIM_TRANSLUCENT)
            {
                translucentmodel &tm = translucent.add();
                tm.m = b.m;
                tm.batched = &bm;
                tm.dist = camera1->o.dist(bm.o);
                continue;
            }
            renderbatchedmodel(b.m, bm);
        }
        if(dynshadow && b.m->hasshadows() && (!reflecting || refracting) && (!effective_stencilshadow || !hasstencil || stencilbits < 8))
        {
            loopvj(b.batched)
            {
                batchedmodel &bm = b.batched[j];
                if(bm.anim&ANIM_TRANSLUCENT) continue;
                renderbatchedmodelshadow(b.m, bm);
            }
        }
        b.m->endrender();
    }
    if(translucent.length())
    {
        translucent.sort(sorttranslucentmodels);
        model *lastmodel = NULL;
        loopv(translucent)
        {
            translucentmodel &tm = translucent[i];
            if(lastmodel!=tm.m)
            {
                if(lastmodel) lastmodel->endrender();
                (lastmodel = tm.m)->startrender();
            }
            renderbatchedmodel(tm.m, *tm.batched);
        }
        if(lastmodel) lastmodel->endrender();
    }
    if(flush) clearmodelbatches();
}

const int dbgmbatch = 0;
//VAR(dbgmbatch, 0, 0, 1);

VARP(popdeadplayers, 0, 0, 1);
void rendermodel(const char *mdl, int anim, int tex, float rad, const vec &o, float roll, float yaw, float pitch, float speed, int basetime, playerent *d, modelattach *a, float scale)
{
    if(popdeadplayers && d && a)
    {
        int acv = anim&ANIM_INDEX;
        if( acv == ANIM_DECAY || acv == ANIM_LYING_DEAD || acv == ANIM_CROUCH_DEATH || acv == ANIM_DEATH ) return;
    }
    model *m = loadmodel(mdl);
    if(!m || (stenciling && (m->shadowdist <= 0 || anim&ANIM_TRANSLUCENT))) return;

    if(rad >= 0)
    {
        if(!rad)
        {
            rad = m->radius;
            if(roll != 0 || pitch != 0) rad = max(m->radius, m->zradius);  // FIXME: this assumes worst-case even for small angles and should be eased up (especially for lamp posts)
            rad *= scale;
        }
        if(isoccluded(camera1->o.x, camera1->o.y, o.x-rad, o.y-rad, rad*2)) return;
    }

    if(stenciling && d && !raycubelos(camera1->o, o, d->radius))
    {
        vec target(o);
        target.z += d->eyeheight;
        if(!raycubelos(camera1->o, target, d->radius)) return;
    }

    int varseed = 0;
    if(d) switch(anim&ANIM_INDEX)
    {
        case ANIM_DEATH:
        case ANIM_LYING_DEAD: varseed = (int)(size_t)d + d->lastpain; break;
        default: varseed = (int)(size_t)d + d->lastaction; break;
    }

    if(a) for(int i = 0; a[i].tag; i++)
    {
        if(a[i].name) a[i].m = loadmodel(a[i].name);
        //if(a[i].m && a[i].m->type()!=m->type()) a[i].m = NULL;
    }

    if(numbatches>=0 && !dbgmbatch)
    {
        batchedmodel &b = addbatchedmodel(m);
        b.o = o;
        b.anim = anim;
        b.varseed = varseed;
        b.tex = tex;
        b.roll = roll;
        b.yaw = yaw;
        b.pitch = pitch;
        b.speed = speed;
        b.basetime = basetime;
        b.d = d;
        b.attached = a ? modelattached.length() : -1;
        if(a) for(int i = 0;; i++) { modelattached.add(a[i]); if(!a[i].tag) break; }
        b.scale = scale;
        return;
    }

    if(stenciling)
    {
        m->startrender();
        m->render(anim|ANIM_NOSKIN, varseed, speed, basetime, o, 0, yaw, pitch, d, a, scale);
        m->endrender();
        return;
    }

    m->startrender();

    int x = (int)o.x, y = (int)o.y;
    if(!OUTBORD(x, y))
    {
        sqr *s = S(x, y);
        if(!(anim&ANIM_TRANSLUCENT) && dynshadow && m->hasshadows() && (!reflecting || refracting) && (!effective_stencilshadow || !hasstencil || stencilbits < 8))
        {
            vec center(o.x, o.y, s->floor);
            if(s->type==FHF) center.z -= s->vdelta/4.0f;
            if(!dynshadowquad || center.z-0.1f<=o.z)
            {
                center.z += 0.1f;
                float intensity = dynshadow/100.0f;
                if(dynshadowdecay) switch(anim&ANIM_INDEX)
                {
                    case ANIM_DECAY:
                    case ANIM_LYING_DEAD:
                        intensity *= max(1.0f - float(lastmillis - basetime)/dynshadowdecay, 0.0f);
                        break;
                }
                glColor4f(0, 0, 0, intensity);
                m->rendershadow(anim, varseed, speed, basetime, dynshadowquad ? center : o, yaw, a);
            }
        }
        glColor3ub(s->r, s->g, s->b);
    }
    else glColor3f(1, 1, 1);

    m->setskin(tex);

    if(anim&ANIM_TRANSLUCENT)
    {
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        m->render(anim|ANIM_NOSKIN, varseed, speed, basetime, o, 0, yaw, pitch, d, a, scale);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        glDepthFunc(GL_LEQUAL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        GLfloat color[4];
        glGetFloatv(GL_CURRENT_COLOR, color);
        glColor4f(color[0], color[1], color[2], m->translucency);
    }

    m->render(anim, varseed, speed, basetime, o, 0, yaw, pitch, d, a, scale);

    if(anim&ANIM_TRANSLUCENT)
    {
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);
    }

    m->endrender();
}

int findanim(const char *name)
{
    const char *names[] = { "idle", "run", "attack", "pain", "jump", "land", "flipoff", "salute", "taunt", "wave", "point", "crouch idle", "crouch walk", "crouch attack", "crouch pain", "crouch death", "death", "lying dead", "flag", "gun idle", "gun shoot", "gun reload", "gun throw", "mapmodel", "trigger", "decay", "all" };
    loopi(sizeof(names)/sizeof(names[0])) if(!strcmp(name, names[i])) return i;
    return -1;
}

void loadskin(const char *dir, const char *altdir, Texture *&skin) // model skin sharing
{
    bool old_silent_texture_load = silent_texture_load;
    silent_texture_load = true;
    #define ifnoload if((skin = textureload(path))==notexture)
    defformatstring(path)("packages/models/%s/skin.jpg", dir);
    ifnoload
    {
        strcpy(path+strlen(path)-3, "png");
        ifnoload
        {
            formatstring(path)("packages/models/%s/skin.jpg", altdir);
            ifnoload
            {
                strcpy(path+strlen(path)-3, "png");
                ifnoload {};
            }
        }
    }
    silent_texture_load = old_silent_texture_load;
}

void preload_playermodels()
{
    model *playermdl = loadmodel("playermodels");
    if(dynshadow && playermdl) playermdl->genshadows(8.0f, 4.0f);
    loopi(NUMGUNS)
    {
        if(i == GUN_CPISTOL) continue;
        defformatstring(vwep)("weapons/%s/world", guns[i].modelname);
        model *vwepmdl = loadmodel(vwep);
        if(dynshadow && vwepmdl) vwepmdl->genshadows(8.0f, 4.0f);
    }
}

void preload_entmodels()
{
    extern const char *entmdlnames[];
    for(int i = 0; entmdlnames[i][0]; i++)
    {
        model *mdl = loadmodel(entmdlnames[i]);
        if(dynshadow && mdl) mdl->genshadows(8.0f, 2.0f);
    }
    static const char *bouncemdlnames[] = { "misc/gib01", "misc/gib02", "misc/gib03", "weapons/grenade/static" };
    loopi(sizeof(bouncemdlnames)/sizeof(bouncemdlnames[0]))
    {
        model *mdl = loadmodel(bouncemdlnames[i]);
        if(dynshadow && mdl) mdl->genshadows(8.0f, 2.0f);
    }
}

bool preload_mapmodels(bool trydl)
{
    int missing = 0;
    loopv(mapmodels) mapmodels[i].flags &= ~MMF_TEMP_USED;
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type!=MAPMODEL || !mapmodels.inrange(e.attr2)) continue;
        mapmodels[ents[i].attr2].flags |= MMF_TEMP_USED;
        if(e.attr4 && lookuptexture(e.attr4, notexture, trydl) == notexture && (!_ignoreillegalpaths || gettextureslot(e.attr4))) missing++;
    }
    loopv(mapmodels) if((mapmodels[i].flags & (MMF_REQUIRED | MMF_TEMP_USED)) && !loadmodel(NULL, i, trydl)) missing++;
    return !missing;
}

void renderclient(playerent *d, const char *mdlname, const char *vwepname, int tex)
{
    int anim = ANIM_IDLE|ANIM_LOOP;
    float speed = 0.0;
    vec o(d->o);
    o.z -= d->eyeheight;
    int basetime = -((int)(size_t)d&0xFFF);
    if(d->state==CS_DEAD)
    {
        if(d==player1 && d->allowmove()) return;
        if(d->nocorpse) return;
        d->pitch = 0.1f;
        anim = ANIM_DEATH;
        basetime = d->lastpain;
        int t = lastmillis-d->lastpain;
        if(t<0 || t>20000) return;
        if(t>2000)
        {
            anim = ANIM_LYING_DEAD|ANIM_NOINTERP|ANIM_LOOP;
            basetime += 2000;
            t -= 2000;
            o.z -= t*t/10000000000.0f*t;
        }
    }
    else if(d->state==CS_EDITING)                   { anim = ANIM_JUMP|ANIM_END; }
    else if(d->state==CS_LAGGED)                    { anim = ANIM_SALUTE|ANIM_LOOP|ANIM_TRANSLUCENT; }
    else if(lastmillis-d->lastpain<300)             { anim = d->crouching ? ANIM_CROUCH_PAIN : ANIM_PAIN; speed = 300.0f/4; basetime = d->lastpain; }
    else if(d->weaponsel==d->lastattackweapon && lastmillis-d->lastaction<300 && d->lastpain<d->lastaction && !d->weaponsel->reloading && (d->weaponsel->type!=GUN_GRENADE || ((grenades *)d->weaponsel)->cookingmillis>0))
                                                    { anim = d->crouching ? ANIM_CROUCH_ATTACK : ANIM_ATTACK; speed = 300.0f/8; basetime = d->lastaction; }
    else if(!d->onfloor && d->timeinair>50)         { anim = (d->crouching ? ANIM_CROUCH_WALK : ANIM_JUMP)|ANIM_END; }
    else if(!d->move && !d->strafe)                 { anim = (d->crouching ? ANIM_CROUCH_IDLE : ANIM_IDLE)|ANIM_LOOP; }
    else                                            { anim = (d->crouching ? ANIM_CROUCH_WALK : ANIM_RUN)|ANIM_LOOP; speed = 1860/d->maxspeed; }
    if(d->move < 0)                                 { anim |= ANIM_REVERSE; }
    modelattach a[3];
    int numattach = 0;
    if(vwepname)
    {
        a[numattach].name = vwepname;
        a[numattach].tag = "tag_weapon";
        numattach++;
    }

    if(!stenciling && !reflecting && !refracting)
    {
        if(d->weaponsel==d->lastattackweapon && lastmillis-d->lastaction < d->weaponsel->flashtime())
            anim |= ANIM_PARTICLE;
        if(d != player1 && d->state==CS_ALIVE)
        {
            d->head = vec(-1, -1, -1);
            a[numattach].tag = "tag_head";
            a[numattach].pos = &d->head;
            numattach++;
        }
    }
    if(player1->isspectating() && d->clientnum == player1->followplayercn && player1->spectatemode == SM_FOLLOW3RD_TRANSPARENT)
    {
        anim |= ANIM_TRANSLUCENT; // see through followed player
        if(stenciling) return;
    }
    rendermodel(mdlname, anim|ANIM_DYNALLOC, tex, 1.5f, o, 0, d->yaw+90, d->pitch/4, speed, basetime, d, a);
    if(!stenciling && !reflecting && !refracting)
    {
        if(isteam(player1->team, d->team)) renderaboveheadicon(d);
    }
}

VARP(teamdisplaymode, 0, 1, 2);

#define SKINBASE "packages/models/playermodels"
VARP(hidecustomskins, 0, 0, 2);
static vector<char *> playerskinlist;

const char *getclientskin(const char *name, const char *suf)
{
    static string tmp;
    int suflen = (int)strlen(suf), namelen = (int)strlen(name);
    const char *s, *r = NULL;
    loopv(playerskinlist)
    {
        s = playerskinlist[i];
        int sl = (int)strlen(s) - suflen;
        if(sl > 0 && !strcmp(s + sl, suf))
        {
            if(namelen == sl && !strncmp(name, s, namelen)) return s; // exact match
            if(s[sl - 1] == '_')
            {
                copystring(tmp, s);
                tmp[sl - 1] = '\0';
                if(strstr(name, tmp)) r = s; // partial match
            }
        }
    }
    return r;
}

void updateclientname(playerent *d)
{
    static bool gotlist = false;
    if(!gotlist) listfiles(SKINBASE "/custom", "jpg", playerskinlist);
    gotlist = true;
    if(!d || !playerskinlist.length()) return;
    d->skin_noteam = getclientskin(d->name, "_ffa");
    d->skin_cla = getclientskin(d->name, "_cla");
    d->skin_rvsf = getclientskin(d->name, "_rvsf");
}

void renderclientp(playerent *d)
{
    if(!d) return;
    int team = team_base(d->team);
    const char *cs = NULL, *skinbase = SKINBASE, *teamname = team_basestring(team);
    int skinid = 1 + d->skin();
    string skin;
    if(hidecustomskins == 0 || (hidecustomskins == 1 && !m_teammode))
    {
        cs = team ? d->skin_rvsf : d->skin_cla;
        if(!m_teammode && d->skin_noteam) cs = d->skin_noteam;
    }
    if(cs)
        formatstring(skin)("%s/custom/%s.jpg", skinbase, cs);
    else
    {
        if(!m_teammode || !teamdisplaymode) formatstring(skin)("%s/%s/%02d.jpg", skinbase, teamname, skinid);
        else switch(teamdisplaymode)
        {
            case 1: formatstring(skin)("%s/%s/%02d_%svest.jpg", skinbase, teamname, skinid, team ? "blue" : "red"); break;
            case 2: default: formatstring(skin)("%s/%s/%s.jpg", skinbase, teamname, team ? "blue" : "red"); break;
        }
    }
    string vwep;
    if(d->weaponsel) formatstring(vwep)("weapons/%s/world", d->weaponsel->info.modelname);
    else vwep[0] = 0;
    renderclient(d, "playermodels", vwep[0] ? vwep : NULL, -(int)textureload(skin)->id);
}

void renderclients()
{
    playerent *d;
    loopv(players) if((d = players[i]) && d->state!=CS_SPAWNING && d->state!=CS_SPECTATE && (!player1->isspectating() || player1->spectatemode != SM_FOLLOW1ST || player1->followplayercn != i)) renderclientp(d);
    if(player1->state==CS_DEAD || (reflecting && !refracting)) renderclientp(player1);
}

void loadallmapmodels()  // try to find all mapmodels in packages/models/mapmodels
{
    vector<char *> files;
    listdirsrecursive("packages/models/mapmodels", files);
    files.sort(stringsort);
    loopvrev(files) if(files.inrange(i + 1) && !strcmp(files[i], files[i + 1])) delstring(files.remove(i + 1)); // remove doubles
    silentmodelloaderror = true;
    loopv(files)
    {
        const char *mn = files[i] + strlen("packages/models/");
        clientlogf("loadallmaps: %s %s", mn, loadmodel(mn) ? "loaded" : "failed to load");
        delstring(files[i]);
    }
    silentmodelloaderror = false;
}
COMMAND(loadallmapmodels, "");
