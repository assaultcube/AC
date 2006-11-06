// world.cpp: core map management stuff

#include "cube.h"

extern char *entnames[];                // lookup from map entities above to strings

sqr *world = NULL;
int sfactor, ssize, cubicsize, mipsize;

header hdr;

void settag(int tag, int type)          // set all cubes with "tag" to space, if tag is 0 then reset ALL tagged cubes according to type
{
    int maxx = 0, maxy = 0, minx = ssize, miny = ssize;
    loop(x, ssize) loop(y, ssize)
    {
        sqr *s = S(x, y);
        if(s->tag)
        {
            if(tag)
            {
                if(tag==s->tag) s->type = SPACE;
                else continue;
            }
            else
            {
                s->type = type ? SOLID : SPACE;
            };
            if(x>maxx) maxx = x;
            if(y>maxy) maxy = y;
            if(x<minx) minx = x;
            if(y<miny) miny = y;
        };
    };
    block b = { minx, miny, maxx-minx+1, maxy-miny+1 };
    if(maxx) remip(b);      // remip minimal area of changed geometry
};

void resettagareas() { settag(0, 0); };                                                         // reset for editing or map saving
// Modified by Rick
//void settagareas() { settag(0, 1); loopv(ents) if(ents[i].type==TRIGGER) setspawn(i, true); };   // set for playing

void settagareas() // set for playing
{
     settag(0, 1);
     loopv(ents) if(ents[i].type==CARROT) setspawn(i, true);
     if(ishost()) BotManager.PickNextTrigger();
};
// End mod

void trigger(int tag, int type, bool savegame)
{
    if(!tag) return;
    settag(tag, type);
    if(!savegame && type!=3) playsound(S_RUMBLE);
    s_sprintfd(aliasname)("level_trigger_%d", tag);
    if(identexists(aliasname)) execute(aliasname);
};

COMMAND(trigger, ARG_2INT);

// main geometric mipmapping routine, recursively rebuild mipmaps within block b.
// tries to produce cube out of 4 lower level mips as well as possible,
// sets defer to 0 if mipped cube is a perfect mip, i.e. can be rendered at this
// mip level indistinguishable from its constituent cubes (saves considerable
// rendering time if this is possible).

void remip(block &b, int level)
{
    if(level>=SMALLEST_FACTOR) return;
    int lighterr = getvar("lighterror")*3;
    sqr *w = wmip[level];
    sqr *v = wmip[level+1];
    int ws = ssize>>level;
    int vs = ssize>>(level+1);
    block s = b;
    if(s.x&1) { s.x--; s.xs++; };
    if(s.y&1) { s.y--; s.ys++; };
    s.xs = (s.xs+1)&~1;
    s.ys = (s.ys+1)&~1;
    for(int x = s.x; x<s.x+s.xs; x+=2) for(int y = s.y; y<s.y+s.ys; y+=2)
    {
        sqr *o[4];
        o[0] = SWS(w,x,y,ws);                               // the 4 constituent cubes
        o[1] = SWS(w,x+1,y,ws);
        o[2] = SWS(w,x+1,y+1,ws);
        o[3] = SWS(w,x,y+1,ws);
        sqr *r = SWS(v,x/2,y/2,vs);                         // the target cube in the higher mip level
        *r = *o[0];
        uchar nums[MAXTYPE];
        loopi(MAXTYPE) nums[i] = 0;
        loopj(4) nums[o[j]->type]++;
        r->type = SEMISOLID;                                // cube contains both solid and space, treated specially in the renderer
        loopk(MAXTYPE) if(nums[k]==4) r->type = k;
        if(!SOLID(r))
        {
            int floor = 127, ceil = -128, num = 0;
            loopi(4) if(!SOLID(o[i]))
            {
                num++;
                int fh = o[i]->floor;
                int ch = o[i]->ceil;
                if(r->type==SEMISOLID)
                {
                    if(o[i]->type==FHF) fh -= o[i]->vdelta/4+2;     // crap hack, needed for rendering large mips next to hfs
                    if(o[i]->type==CHF) ch += o[i]->vdelta/4+2;     // FIXME: needs to somehow take into account middle vertices on higher mips
                };
                if(fh<floor) floor = fh;  // take lowest floor and highest ceil, so we never have to see missing lower/upper from the side
                if(ch>ceil) ceil = ch;
            };
            r->floor = floor;
            r->ceil = ceil;
        };       
        if(r->type==CORNER) goto mip;                       // special case: don't ever split even if textures etc are different
        r->defer = 1;
        if(SOLID(r))
        {
            loopi(3)
            {
                if(o[i]->wtex!=o[3]->wtex) goto c;          // on an all solid cube, only thing that needs to be equal for a perfect mip is the wall texture
            };
        }
        else
        {
            loopi(3)
            {
                if(o[i]->type!=o[3]->type
                || o[i]->floor!=o[3]->floor
                || o[i]->ceil!=o[3]->ceil
                || o[i]->ftex!=o[3]->ftex
                || o[i]->ctex!=o[3]->ctex
                || abs(o[i+1]->r-o[0]->r)>lighterr          // perfect mip even if light is not exactly equal
                || abs(o[i+1]->g-o[0]->g)>lighterr
                || abs(o[i+1]->b-o[0]->b)>lighterr
                || o[i]->utex!=o[3]->utex
                || o[i]->wtex!=o[3]->wtex) goto c;
            };
            if(r->type==CHF || r->type==FHF)                // can make a perfect mip out of a hf if slopes lie on one line
            {
                if(o[0]->vdelta-o[1]->vdelta != o[1]->vdelta-SWS(w,x+2,y,ws)->vdelta
                || o[0]->vdelta-o[2]->vdelta != o[2]->vdelta-SWS(w,x+2,y+2,ws)->vdelta
                || o[0]->vdelta-o[3]->vdelta != o[3]->vdelta-SWS(w,x,y+2,ws)->vdelta
                || o[3]->vdelta-o[2]->vdelta != o[2]->vdelta-SWS(w,x+2,y+1,ws)->vdelta
                || o[1]->vdelta-o[2]->vdelta != o[2]->vdelta-SWS(w,x+1,y+2,ws)->vdelta) goto c;
            };
        };
        { loopi(4) if(o[i]->defer) goto c; };               // if any of the constituents is not perfect, then this one isn't either
        mip:
        r->defer = 0;
        c:;
    };
    s.x  /= 2;
    s.y  /= 2;
    s.xs /= 2;
    s.ys /= 2;
    remip(s, level+1);
};

void remipmore(block &b, int level)
{
    block bb = b;
    if(bb.x>1) bb.x--;
    if(bb.y>1) bb.y--;
    if(bb.xs<ssize-3) bb.xs++;
    if(bb.ys<ssize-3) bb.ys++;
    remip(bb, level);
};

int closestent()        // used for delent and edit mode ent display
{
    if(noteditmode()) return -1;
    int best = -1;
    float bdist = 99999;
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==NOTUSED) continue;
        vec v = { e.x, e.y, e.z };
        vdist(dist, t, player1->o, v);
        if(dist<bdist)
        {
            best = i;
            bdist = dist;
        };
    };
    return bdist==99999 ? -1 : best; 
};

void entproperty(int prop, int amount)
{
    int e = closestent();
    if(e<0) return;
    switch(prop)
    {
        case 0: ents[e].attr1 += amount; break;
        case 1: ents[e].attr2 += amount; break;
        case 2: ents[e].attr3 += amount; break;
        case 3: ents[e].attr4 += amount; break;
    };
};

void delent()
{
    int e = closestent();
    if(e<0) { conoutf("no more entities"); return; };
    int t = ents[e].type;
    conoutf("%s entity deleted", entnames[t]);
    ents[e].type = NOTUSED;
    addmsg(SV_EDITENT, "ri9", e, NOTUSED, 0, 0, 0, 0, 0, 0, 0);
    if(t==LIGHT) calclight();
};

int findtype(char *what)
{
    loopi(MAXENTTYPES) if(strcmp(what, entnames[i])==0) return i;
    conoutf("unknown entity type \"%s\"", what);
    return NOTUSED;
}

entity *newentity(int x, int y, int z, char *what, int v1, int v2, int v3, int v4)
{
    int type = findtype(what);
    persistent_entity e = { x, y, z, v1, type, v2, v3, v4 };
    switch(type)
    {
        case LIGHT:
            if(v1>64) v1 = 64;//if(v1>32) v1=32;
            if(!v1) e.attr1 = 16;
            if(!v2 && !v3 && !v4) e.attr2 = 255;          
            break;
            
        case MAPMODEL:
            e.attr4 = e.attr3;
            e.attr3 = e.attr2;
            e.attr2 = (uchar)e.attr1;
        case PLAYERSTART:
        case CTF_FLAG:
            e.attr2 = v1;
            e.attr1 = (int)player1->yaw;
            break;
    };
    addmsg(SV_EDITENT, "ri9", ents.length(), type, e.x, e.y, e.z, e.attr1, e.attr2, e.attr3, e.attr4);
    ents.add(*((entity *)&e)); // unsafe!
    if(type==LIGHT) calclight();
    return &ents.last();
};

void clearents(char *name)
{  
    int type = findtype(name);
    if(noteditmode() || multiplayer()) return;
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==type) e.type = NOTUSED;
    };
    if(type==LIGHT) calclight();
};

COMMAND(clearents, ARG_1STR);

void scalecomp(uchar &c, int intens)
{
    int n = c*intens/100;
    if(n>255) n = 255;
    c = n;
};

void scalelights(int f, int intens)
{
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type!=LIGHT) continue;
        e.attr1 = e.attr1*f/100;
        if(e.attr1<2) e.attr1 = 2;
        if(e.attr1>32) e.attr1 = 32;
        if(intens)
        {
            scalecomp(e.attr2, intens);
            scalecomp(e.attr3, intens);
            scalecomp(e.attr4, intens);
        };
    };
    calclight();
};

COMMAND(scalelights, ARG_2INT);

int findentity(int type, int index)
{
    for(int i = index; i<ents.length(); i++) if(ents[i].type==type) return i;
    loopj(index) if(ents[j].type==type) return j;
    return -1;
};

int findplayerstart(int team, int index)
{
    for(int i = index; i<ents.length(); i++) if(ents[i].type==PLAYERSTART && ents[i].attr2==team) return i;
    loopj(index) if(ents[j].type==PLAYERSTART && ents[j].attr2==team) return j;
    for(int i = index; i<ents.length(); i++) if(ents[i].type==PLAYERSTART) return i;
    return -1;
};

sqr *wmip[LARGEST_FACTOR*2];

void setupworld(int factor)
{
    ssize = 1<<(sfactor = factor);
    cubicsize = ssize*ssize;
    mipsize = cubicsize*134/100;
    sqr *w = world = new sqr[mipsize];
    loopi(LARGEST_FACTOR*2) { wmip[i] = w; w += cubicsize>>(i*2); };
};

void empty_world(int factor, bool force)    // main empty world creation routine, if passed factor -1 will enlarge old world by 1
{
    if(!force && noteditmode()) return; 
    cleardlights();
    pruneundos();
    sqr *oldworld = world;
    bool copy = false;
    if(oldworld && factor<0) { factor = sfactor+1; copy = true; };
    if(factor<SMALLEST_FACTOR) factor = SMALLEST_FACTOR;
    if(factor>LARGEST_FACTOR) factor = LARGEST_FACTOR;
    setupworld(factor);
    
    loop(x,ssize) loop(y,ssize)
    {
        sqr *s = S(x,y);
        s->r = s->g = s->b = 150;
        s->ftex = DEFAULT_FLOOR;
        s->ctex = DEFAULT_CEIL;
        s->wtex = s->utex = DEFAULT_WALL;
        s->type = SOLID;
        s->floor = 0;
        s->ceil = 16;
        s->vdelta = 0;
        s->defer = 0;
    };
    
    strncpy(hdr.head, "ACMP", 4);
    hdr.version = MAPVERSION;
    hdr.headersize = sizeof(header);
    hdr.sfactor = sfactor;

    if(copy)
    {
        loop(x,ssize/2) loop(y,ssize/2)
        {
            *S(x+ssize/4, y+ssize/4) = *SWS(oldworld, x, y, ssize/2);
        };
        loopv(ents)
        {
            ents[i].x += ssize/4;
            ents[i].y += ssize/4;
        };
    }
    else
    {
        s_strncpy(hdr.maptitle, "Untitled Map by Unknown", 128);
        hdr.waterlevel = -100000;
        loopi(15) hdr.reserved[i] = 0;
        loopk(3) loopi(256) hdr.texlists[k][i] = i;
        ents.setsize(0);
        block b = { 8, 8, ssize-16, ssize-16 }; 
        edittypexy(SPACE, b);
    };
    
    calclight();
    startmap("maps/unnamed");
    if(oldworld)
    {
        delete[] oldworld;
        toggleedit();
        execfile("config/default_map_settings.cfg");
        execute("fullbright 1");
    };
};

void mapenlarge()  { empty_world(-1, false); };
void newmap(int i) { empty_world(i, false); };

COMMAND(mapenlarge, ARG_NONE);
COMMAND(newmap, ARG_1INT);
COMMANDN(recalc, calclight, ARG_NONE);
COMMAND(delent, ARG_NONE);
COMMAND(entproperty, ARG_2INT);

