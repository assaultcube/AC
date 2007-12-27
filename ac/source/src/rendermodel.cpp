#include "pch.h"
#include "cube.h"

VARP(animationinterpolationtime, 0, 150, 1000);

model *loadingmodel = NULL;

#include "tristrip.h"
#include "vertmodel.h"
#include "md2.h"
#include "md3.h"
    
#define checkmdl if(!loadingmodel) { conoutf("not loading a model"); return; }

void mdlcullface(int cullface)
{
    checkmdl;
    loadingmodel->cullface = cullface!=0;
}

COMMAND(mdlcullface, ARG_1INT);

void mdlscale(int percent)
{
    checkmdl;
    float scale = 0.3f;
    if(percent>0) scale = percent/100.0f;
    else if(percent<0) scale = 0.0f;
    loadingmodel->scale = scale;
}

COMMAND(mdlscale, ARG_1INT);

void mdltrans(char *x, char *y, char *z)
{
    checkmdl;
    loadingmodel->translate = vec(atof(x), atof(y), atof(z));
}

COMMAND(mdltrans, ARG_3STR);

vector<mapmodelinfo> mapmodels;

void mapmodel(char *rad, char *h, char *zoff, char *snap, char *name)
{
    mapmodelinfo &mmi = mapmodels.add();
    mmi.rad = atoi(rad);
    mmi.h = atoi(h);
    mmi.zoff = atoi(zoff);
    s_sprintf(mmi.name)("mapmodels/%s", name);
}

void mapmodelreset() { mapmodels.setsize(0); }

mapmodelinfo &getmminfo(int i) { return mapmodels.inrange(i) ? mapmodels[i] : *(mapmodelinfo *)0; }

COMMAND(mapmodel, ARG_5STR);
COMMAND(mapmodelreset, ARG_NONE);

hashtable<const char *, model *> mdllookup;

model *loadmodel(const char *name, int i)
{
    if(!name)
    {
        if(!mapmodels.inrange(i)) return NULL;
        mapmodelinfo &mmi = mapmodels[i];
        if(mmi.m) return mmi.m;
        name = mmi.name;
    }
    model **mm = mdllookup.access(name);
    model *m;
    if(mm) m = *mm;
    else
    {
        m = new md2(name);
        loadingmodel = m;
        if(!m->load())
        {
            delete m;
            m = new md3(name);
            loadingmodel = m;
            if(!m->load())
            {
                delete m;
                loadingmodel = NULL;
                return NULL;
            }
        }
        loadingmodel = NULL;
        mdllookup.access(m->name(), &m);
    }
    if(mapmodels.inrange(i) && !mapmodels[i].m) mapmodels[i].m = m;
    return m;
}

VARP(dynshadow, 0, 40, 100);

struct batchedmodel
{
    vec o;
    int anim, varseed, tex;
    float yaw, pitch, speed;
    int basetime;
    playerent *d;
    model *vwep;
    float scale;
};
struct modelbatch
{
    model *m;
    vector<batchedmodel> batched;
};
static vector<modelbatch *> batches;
static int numbatches = -1;

void startmodelbatches()
{   
    numbatches = 0;
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
            b->batched.setsizenodelete(0);
        }
        else b = batches.add(new modelbatch);
        b->m = m;
        m->batch = numbatches++;
    }
    return b->batched.add();
}

void renderbatchedmodel(model *m, batchedmodel &b)
{
    int x = (int)b.o.x, y = (int)b.o.y;
    if(!OUTBORD(x, y))
    {
        sqr *s = S(x, y);
        glColor3ub(s->r, s->g, s->b);
    }
    else glColor3f(1, 1, 1);

    m->setskin(b.tex);
    m->render(b.anim, b.varseed, b.speed, b.basetime, b.o, b.yaw, b.pitch, b.d, b.vwep, b.scale);
}

void renderbatchedmodelshadow(model *m, batchedmodel &b)
{
    int x = (int)b.o.x, y = (int)b.o.y;
    if(OUTBORD(x, y)) return;
    sqr *s = S(x, y);
    vec center(b.o.x, b.o.y, s->floor);
    if(s->type==FHF) center.z -= s->vdelta/4.0f;
    if(center.z-0.1f>b.o.z) return;
    center.z += 0.1f;
    m->rendershadow(b.anim, b.varseed, b.speed, b.basetime, center, b.yaw, b.vwep);
}

static int sortbatchedmodels(const batchedmodel *x, const batchedmodel *y)
{
    if(x->tex < y->tex) return -1;
    if(x->tex > y->tex) return 1;
    return 0;
}

void endmodelbatches()
{
    loopi(numbatches)
    {
        modelbatch &b = *batches[i];
        if(b.batched.empty()) continue;
        loopvj(b.batched) if(b.batched[j].tex) { b.batched.sort(sortbatchedmodels); break; }
        b.m->startrender();
        loopvj(b.batched)
        {
            batchedmodel &bm = b.batched[j];
            renderbatchedmodel(b.m, bm);
        }
        if(dynshadow && b.m->hasshadows() && (!reflecting || refracting))
        {
            glColor4f(0, 0, 0, dynshadow/100.0f);
            loopvj(b.batched)
            {
                batchedmodel &bm = b.batched[j];
                renderbatchedmodelshadow(b.m, bm);
            }
        }
        b.m->endrender();
    }
    numbatches = -1;
}

void rendermodel(const char *mdl, int anim, int tex, float rad, const vec &o, float yaw, float pitch, float speed, int basetime, playerent *d, const char *vwepmdl, float scale)
{
    model *m = loadmodel(mdl);
    if(!m) return;

    if(rad > 0 && isoccluded(camera1->o.x, camera1->o.y, o.x-rad, o.y-rad, rad*2)) return;

    int varseed = 0;
    if(d) switch(anim&ANIM_INDEX)
    {
        case ANIM_DEATH:
        case ANIM_LYING_DEAD: varseed = (int)(size_t)d + d->lastpain; break;
        default: varseed = (int)(size_t)d + d->lastaction; break;
    }

    model *vwep = NULL;
    if(vwepmdl)
    {
        vwep = loadmodel(vwepmdl);
        if(vwep->type()!=m->type()) vwep = NULL;
    }

    if(numbatches>=0)
    {
        batchedmodel &b = addbatchedmodel(m);
        b.o = o;
        b.anim = anim;
        b.varseed = varseed;
        b.tex = tex;
        b.yaw = yaw;
        b.pitch = pitch;
        b.speed = speed;
        b.basetime = basetime;
        b.d = d;
        b.vwep = vwep;
        b.scale = scale;
        return;
    }

    m->startrender();

    int x = (int)o.x, y = (int)o.y;
    if(!OUTBORD(x, y))
    {
        sqr *s = S(x, y);
        if(dynshadow && m->hasshadows() && (!reflecting || refracting))
        {
            vec center(o.x, o.y, s->floor);
            if(s->type==FHF) center.z -= s->vdelta/4.0f;
            if(center.z-0.1f<=o.z)
            {
                center.z += 0.1f;
                glColor4f(0, 0, 0, dynshadow/100.0f);
                m->rendershadow(anim, varseed, speed, basetime, center, yaw, vwep); 
            }
        } 
        glColor3ub(s->r, s->g, s->b);
    }
    else glColor3f(1, 1, 1);

    m->setskin(tex);
    m->render(anim, varseed, speed, basetime, o, yaw, pitch, d, vwep, scale);

    m->endrender();
}

int findanim(const char *name)
{
    const char *names[] = { "idle", "run", "attack", "pain", "jump", "land", "flipoff", "salute", "taunt", "wave", "point", "crouch idle", "crouch walk", "crouch attack", "crouch pain", "crouch death", "death", "lying dead", "flag", "gun idle", "gun shoot", "gun reload", "gun throw", "mapmodel", "trigger", "all" };
    loopi(sizeof(names)/sizeof(names[0])) if(!strcmp(name, names[i])) return i;
    return -1;
}

void loadskin(const char *dir, const char *altdir, Texture *&skin) // model skin sharing
{
    #define ifnoload if((skin = textureload(path))==notexture)
    s_sprintfd(path)("packages/models/%s/skin.jpg", dir);
    ifnoload
    {
        strcpy(path+strlen(path)-3, "png");
        ifnoload
        {
            s_sprintf(path)("packages/models/%s/skin.jpg", altdir);
            ifnoload
            {
                strcpy(path+strlen(path)-3, "png");
                ifnoload return;
            } 
        } 
    } 
}

void preload_playermodels()
{
    model *playermdl = loadmodel("playermodels");
    if(dynshadow && playermdl) playermdl->genshadows(8.0f, 4.0f);
    loopi(NUMGUNS)
    {
        s_sprintfd(vwep)("weapons/%s/world", guns[i].modelname);
        model *vwepmdl = loadmodel(vwep);
        if(dynshadow && vwepmdl) vwepmdl->genshadows(8.0f, 4.0f);
    }
}

void preload_entmodels()
{
    extern const char *entmdlnames[];
    loopi(I_AKIMBO-I_CLIPS+1)
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
            
void preload_mapmodels()
{
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type!=MAPMODEL || !mapmodels.inrange(e.attr2)) continue;
        if(!loadmodel(NULL, e.attr2)) continue;
        if(e.attr4) lookuptexture(e.attr4);
    }
}

void renderclient(playerent *d, const char *mdlname, const char *vwepname, int tex)
{
    int varseed = (int)(size_t)d;
    int anim = ANIM_IDLE|ANIM_LOOP;
    float speed = 0.0;
    vec o(d->o);
    o.z -= d->dyneyeheight();
    int basetime = -((int)(size_t)d&0xFFF);
    if(d->state==CS_DEAD)
    {
        if(d==player1 && d->allowmove()) return;
        loopv(bounceents) if(bounceents[i]->bouncestate==GIB && bounceents[i]->owner==d) return;
        d->pitch = 0.1f;
        int r = 6;
        anim = ANIM_DEATH;
        varseed += d->lastpain;
        basetime = d->lastpain;
        int t = lastmillis-d->lastpain;
        if(t<0 || t>20000) return;
        if(t>(r-1)*100-50)
        {
            anim = ANIM_LYING_DEAD|ANIM_NOINTERP|ANIM_LOOP;
            if(t>(r+10)*100)
            {
                t -= (r+10)*100;
                o.z -= t*t/10000000000.0f*t;
            }
        }
    }
    else if(d->state==CS_EDITING)                   { anim = ANIM_JUMP|ANIM_END; }
    else if(d->state==CS_LAGGED)                    { anim = ANIM_SALUTE|ANIM_LOOP; }
    else if(lastmillis-d->lastpain<300)             { anim = d->crouching ? ANIM_CROUCH_PAIN : ANIM_PAIN; speed = 300.0f/4; varseed += d->lastpain; basetime = d->lastpain; }
    else if(!d->onfloor && d->timeinair>50)         { anim = ANIM_JUMP|ANIM_END; }
    else if(d->weaponsel==d->lastattackweapon && lastmillis-d->lastaction<300) { anim = d->crouching ? ANIM_CROUCH_ATTACK : ANIM_ATTACK; speed = 300.0f/8; basetime = d->lastaction; }
    else if(!d->move && !d->strafe)                 { anim = (d->crouching ? ANIM_CROUCH_IDLE : ANIM_IDLE)|ANIM_LOOP; }
    else                                            { anim = (d->crouching ? ANIM_CROUCH_WALK : ANIM_RUN)|ANIM_LOOP; speed = 1860/d->maxspeed; }
    
    rendermodel(mdlname, anim, tex, 1.5f, o, d->yaw+90, d->pitch/4, speed, basetime, d, vwepname);
}

VAR(teamdisplaymode, 0, 1, 2);

void renderclient(playerent *d)
{
    if(!d) return;
    int team = team_int(d->team);
    int skinid = 1 + max(0, min(d->skin, (team==TEAM_CLA ? 3 : 5)));
    string skinbase = "packages/models/playermodels";
    string skin;
    if(!m_teammode || !teamdisplaymode) s_sprintf(skin)("%s/%s/%02i.jpg", skinbase, team_string(team), skinid);
    else switch(teamdisplaymode)
    {
        case 1: s_sprintf(skin)("%s/%s/%02i_%svest.jpg", skinbase, team_string(team), skinid, team ? "blue" : "red"); break;
        case 2: default: s_sprintf(skin)("%s/%s/%s.jpg", skinbase, team_string(team), team ? "blue" : "red"); break;
    }
    string vwep;
    if(d->weaponsel) s_sprintf(vwep)("weapons/%s/world", d->weaponsel->info.modelname);
    else vwep[0] = 0;
    renderclient(d, "playermodels", vwep[0] ? vwep : NULL, -(int)textureload(skin)->id);
}

void renderclients()
{   
    playerent *d;
    loopv(players) if((d = players[i]) && d->state!=CS_SPAWNING) renderclient(d);
    if(player1->state==CS_DEAD || (reflecting && !refracting)) renderclient(player1);
}

