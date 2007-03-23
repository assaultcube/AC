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
};

COMMAND(mdlscale, ARG_1INT);

void mdltrans(char *x, char *y, char *z)
{
    checkmdl;
    loadingmodel->translate = vec(atof(x), atof(y), atof(z));
};

COMMAND(mdltrans, ARG_3STR);

vector<mapmodelinfo> mapmodels;

void mapmodel(char *rad, char *h, char *zoff, char *snap, char *name)
{
    mapmodelinfo &mmi = mapmodels.add();
    mmi.rad = atoi(rad);
    mmi.h = atoi(h);
    mmi.zoff = atoi(zoff);
    s_sprintf(mmi.name)("mapmodels/%s", name);
};

void mapmodelreset() { mapmodels.setsize(0); };

mapmodelinfo &getmminfo(int i) { return mapmodels.inrange(i) ? mapmodels[i] : *(mapmodelinfo *)0; };

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
    };
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
            };
        };
        loadingmodel = NULL;
        mdllookup.access(m->name(), &m);
    };
    if(mapmodels.inrange(i) && !mapmodels[i].m) mapmodels[i].m = m;
    return m;
};

void rendermodel(char *mdl, int anim, int tex, float rad, float x, float y, float z, float yaw, float pitch, float speed, int basetime, playerent *d, char *vwepmdl, float scale)
{
    model *m = loadmodel(mdl);
    if(!m) return;

    if(rad > 0 && isoccluded(camera1->o.x, camera1->o.y, x-rad, z-rad, rad*2)) return;

    int ix = (int)x;
    int iy = (int)z;
    vec light(1, 1, 1);

    if(!OUTBORD(ix, iy))
    {
         sqr *s = S(ix,iy);
         float ll = 256.0f; // 0.96f;
         float of = 0.0f; // 0.1f;      
         light.x = s->r/ll+of;
         light.y = s->g/ll+of;
         light.z = s->b/ll+of;
    };

    glColor3fv(&light.x);
    m->setskin(tex);

    model *vwep = NULL;
    if(vwepmdl)
    {
        vwep = loadmodel(vwepmdl);
        if(vwep->type()!=m->type()) vwep = NULL;
    };

    if(anim&ANIM_MIRROR) glCullFace(GL_BACK);
    m->render(anim, (int)(size_t)d + (d ? d->lastaction : 0), speed, basetime, x, y, z, yaw, pitch, d, vwep, scale);
    if(anim&ANIM_MIRROR) glCullFace(GL_FRONT);
};

VAR(dynshadow, 0, 40, 100);

void rendershadow(playerent *d)
{
    if(OUTBORD((int)d->o.x, (int)d->o.y)) return;
    sqr *s = S((int)d->o.x, (int)d->o.y);
    float floor = s->floor;
    if(s->type==FHF) floor -= s->vdelta/4.0f;
    
    float radius = 1.25f*d->radius;
    radius *= 1.0f + min(1.0f, max(0.0f, 0.5f*(d->o.z - d->eyeheight - floor)/(d->aboveeye + d->eyeheight)));

    static Texture *shadowtex = NULL;
    if(!shadowtex) shadowtex = textureload("packages/misc/shadow.png", 3);

    glColor4f(1, 1, 1, dynshadow/100.0f);

    glBindTexture(GL_TEXTURE_2D, shadowtex->id);

    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    floor += 0.1f;

    glBegin(GL_POLYGON);
    glTexCoord2f(0, 0); glVertex3f(d->o.x-radius, floor, d->o.y-radius);
    glTexCoord2f(1, 0); glVertex3f(d->o.x+radius, floor, d->o.y-radius);
    glTexCoord2f(1, 1); glVertex3f(d->o.x+radius, floor, d->o.y+radius);
    glTexCoord2f(0, 1); glVertex3f(d->o.x-radius, floor, d->o.y+radius);
    glEnd();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
};

int findanim(const char *name)
{
    const char *names[] = { "idle", "run", "attack", "pain", "jump", "land", "flipoff", "salute", "taunt", "wave", "point", "crouch idle", "crouch walk", "crouch attack", "crouch pain", "crouch death", "death", "lying dead", "flag", "gun idle", "gun shoot", "gun reload", "gun throw", "mapmodel", "trigger", "all" };
    loopi(sizeof(names)/sizeof(names[0])) if(!strcmp(name, names[i])) return i;
    return -1;
};

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
            }; 
        }; 
    }; 
};

void preload_mapmodels()
{
    int xs, ys;
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type!=MAPMODEL || !mapmodels.inrange(e.attr2)) continue;
        if(!loadmodel(NULL, e.attr2)) continue;
        if(e.attr4) lookuptexture(e.attr4, xs, ys);
    };
};

