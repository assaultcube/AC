#include "cube.h"

VARP(animationinterpolationtime, 0, 150, 1000);

model *loadingmodel = NULL;

#include "tristrip.h"
#include "vertmodel.h"
#include "md2.h"
#include "md3.h"
    
#define checkmdl if(!loadingmodel) { conoutf("not loading a model"); return; }

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

void rendermodel(char *mdl, int anim, int tex, float rad, float x, float y, float z, float yaw, float pitch, float speed, int basetime, playerent *d, char *vwepmdl, float scale)
{
    model *m = loadmodel(mdl);
    if(!m) return;

    if(rad > 0 && isoccluded(camera1->o.x, camera1->o.y, x-rad, y-rad, rad*2)) return;

    int ix = (int)x;
    int iy = (int)y;
    vec light(1, 1, 1);
    int varseed = (int)(size_t)d + (d ? d->lastaction : 0);

    model *vwep = NULL;
    if(vwepmdl)
    {
        vwep = loadmodel(vwepmdl);
        if(vwep->type()!=m->type()) vwep = NULL;
    }

    if(!OUTBORD(ix, iy))
    {
        sqr *s = S(ix, iy);
        float ll = 256.0f; // 0.96f;
        float of = 0.0f; // 0.1f;      
        light.x = s->r/ll+of;
        light.y = s->g/ll+of;
        light.z = s->b/ll+of;

        if(dynshadow && m->hasshadows() && (!reflecting || refracting))
        {
            vec center(x, y, s->floor);
            if(s->type==FHF) center.z -= s->vdelta/4.0f;
            if(center.z-0.1f<=z)
            {
                center.z += 0.1f;
                glColor4f(1, 1, 1, dynshadow/100.0f);
                m->rendershadow(anim, varseed, speed, basetime, center, yaw, vwep); 
            }
        } 
    }

    glColor3fv(&light.x);
    m->setskin(tex);

    if(anim&ANIM_MIRROR) glCullFace(GL_BACK);
    m->render(anim, varseed, speed, basetime, x, y, z, yaw, pitch, d, vwep, scale);
    if(anim&ANIM_MIRROR) glCullFace(GL_FRONT);
}

int findanim(const char *name)
{
    const char *names[] = { "idle", "run", "attack", "pain", "jump", "land", "flipoff", "salute", "taunt", "wave", "point", "crouch idle", "crouch walk", "crouch attack", "crouch pain", "crouch death", "death", "lying dead", "flag", "gun idle", "gun shoot", "gun reload", "gun throw", "mapmodel", "trigger", "all" };
    loopi(sizeof(names)/sizeof(names[0])) if(!strcmp(name, names[i])) return i;
    return -1;
}

void loadskin(const char *dir, const char *altdir, Texture *&skin, model *m) // model skin sharing
{
    #define ifnoload if((skin = textureload(path))==crosshair)
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
        s_sprintfd(vwep)("weapons/%s/world", hudgunnames[i]);
        model *vwepmdl = loadmodel(vwep);
        if(dynshadow && vwepmdl) vwepmdl->genshadows(8.0f, 4.0f);
    }
}

void preload_entmodels()
{
    extern char *entmdlnames[];
    loopi(I_AKIMBO-I_CLIPS+1)
    {
        model *mdl = loadmodel(entmdlnames[i]);
        if(dynshadow && mdl) mdl->genshadows(8.0f, 2.0f);
    }
    static char *bouncemdlnames[] = { "misc/gib01", "misc/gib02", "misc/gib03", "weapons/grenade/static" };
    loopi(sizeof(bouncemdlnames)/sizeof(bouncemdlnames[0]))
    {
        model *mdl = loadmodel(bouncemdlnames[i]);
        if(dynshadow && mdl) mdl->genshadows(8.0f, 2.0f);
    }
}
            
void preload_mapmodels()
{
    int xs, ys;
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type!=MAPMODEL || !mapmodels.inrange(e.attr2)) continue;
        if(!loadmodel(NULL, e.attr2)) continue;
        if(e.attr4) lookuptexture(e.attr4, xs, ys);
    }
}

void renderclient(playerent *d, char *mdlname, char *vwepname, int tex)
{
    int varseed = (int)(size_t)d;
    int anim = ANIM_IDLE|ANIM_LOOP;
    float speed = 0.0;
    float mz = d->o.z-d->eyeheight;
    int basetime = -((int)(size_t)d&0xFFF);
    if(d->state==CS_DEAD)
    {
        loopv(bounceents) if(bounceents[i]->bouncestate==GIB && bounceents[i]->owner==d) return;
        d->pitch = 0.1f;
        int r = 6;
        anim = ANIM_DEATH;
        varseed += d->lastaction;
        basetime = d->lastaction;
        int t = lastmillis-d->lastaction;
        if(t<0 || t>20000) return;
        if(t>(r-1)*100-50)
        {
            anim = ANIM_LYING_DEAD|ANIM_NOINTERP|ANIM_LOOP;
            if(t>(r+10)*100)
            {
                t -= (r+10)*100;
                mz -= t*t/10000000000.0f*t;
            }
        }
        //if(mz<-1000) return;
    }
    else if(d->state==CS_EDITING)                   { anim = ANIM_JUMP|ANIM_END; }
    else if(d->state==CS_LAGGED)                    { anim = ANIM_SALUTE|ANIM_LOOP; }
    else if(lastmillis-d->lastpain<300)             { anim = ANIM_PAIN; speed = 300.0f/4; varseed += d->lastpain; basetime = d->lastpain; }
    else if(!d->onfloor && d->timeinair>50)         { anim = ANIM_JUMP|ANIM_END; }
    else if(d->gunselect==d->lastattackgun && lastmillis-d->lastaction<300)
                                                    { anim = ANIM_ATTACK; speed = 300.0f/8; basetime = d->lastaction; }
    else if(!d->move && !d->strafe)                 { anim = ANIM_IDLE|ANIM_LOOP; }
    else                                            { anim = ANIM_RUN|ANIM_LOOP; speed = 1860/d->maxspeed; }
    rendermodel(mdlname, anim, tex, 1.5f, d->o.x, d->o.y, mz, d->yaw+90, d->pitch/4, speed, basetime, d, vwepname);
}

extern int democlientnum;

void renderclient(playerent *d)
{
    if(!d) return;

    int team = team_int(d->team);
    s_sprintfd(skin)("packages/models/playermodels/%s/0%i.jpg", team_string(team), 1 + max(0, min(d->skin, (team==TEAM_CLA ? 3 : 5))));
    string vwep;
    if(d->gunselect>=0 && d->gunselect<NUMGUNS) s_sprintf(vwep)("weapons/%s/world", hudgunnames[d->gunselect]);
    else vwep[0] = 0;
    renderclient(d, "playermodels", vwep[0] ? vwep : NULL, -(int)textureload(skin)->id);
}

void renderclients()
{   
    playerent *d;
    loopv(players) if((d = players[i]) && (!demoplayback || i!=democlientnum)) renderclient(d);
    if(player1->state==CS_DEAD || (reflecting && !refracting)) renderclient(player1);
}

