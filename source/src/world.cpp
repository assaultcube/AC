// world.cpp: core map management stuff

#include "cube.h"
#include "bot/bot.h"


sqr *world = NULL;
int sfactor, ssize, cubicsize, mipsize;

header hdr;
_mapconfigdata mapconfigdata;

// main geometric mipmapping routine, recursively rebuild mipmaps within block b.
// tries to produce cube out of 4 lower level mips as well as possible,
// sets defer to 0 if mipped cube is a perfect mip, i.e. can be rendered at this
// mip level indistinguishable from its constituent cubes (saves considerable
// rendering time if this is possible).

void remip(const block &b, int level)
{
    if(level>=SMALLEST_FACTOR) return;
    int lighterr = getvar("lighterror")*3;
    sqr *w = wmip[level];
    sqr *v = wmip[level+1];
    int wfactor = sfactor - level;
    int vfactor = sfactor - (level+1);
    block s = b;
    if(s.x&1) { s.x--; s.xs++; }
    if(s.y&1) { s.y--; s.ys++; }
    s.xs = (s.xs+1)&~1;
    s.ys = (s.ys+1)&~1;
    for(int y = s.y; y<s.y+s.ys; y+=2)
    {
        sqr *o[4];
        o[0] = SWS(w,s.x,y,wfactor);                               // the 4 constituent cubes
        o[1] = SWS(o[0],1,0,wfactor);
        o[3] = SWS(o[0],0,1,wfactor);
        o[2] = SWS(o[3],1,0,wfactor);
        sqr *r = SWS(v,s.x/2,y/2,vfactor);                         // the target cube in the higher mip level

        for(int x = s.x; x<s.x+s.xs; x+=2)
        {
            *r = *o[0];
            r->type = o[0]->type == o[1]->type && o[0]->type == o[2]->type && o[0]->type == o[3]->type ? o[0]->type : SEMISOLID;
            if(!SOLID(r))
            {
                int floor = 127, ceil = -128;
                loopi(4) if(!SOLID(o[i]))
                {
                    int fh = o[i]->floor;
                    int ch = o[i]->ceil;
                    if(r->type==SEMISOLID)
                    {
                        if(o[i]->type==FHF) fh -= o[i]->vdelta/4+2;     // crap hack, needed for rendering large mips next to hfs
                        if(o[i]->type==CHF) ch += o[i]->vdelta/4+2;     // FIXME: needs to somehow take into account middle vertices on higher mips
                    }
                    if(fh<floor) floor = fh;  // take lowest floor and highest ceil, so we never have to see missing lower/upper from the side
                    if(ch>ceil) ceil = ch;
                }
                r->floor = floor;
                r->ceil = ceil;
            }
            if(r->type==CORNER) goto mip;                       // special case: don't ever split even if textures etc are different
            r->defer = 1;
            if(level) { loopi(4) if(o[i]->defer) goto c; }        // if any of the constituents is not perfect, then this one isn't either
            if(SOLID(r))
            {
                loopi(3)
                {
                    if(o[i]->wtex != o[3]->wtex) goto c;          // on an all solid cube, only thing that needs to be equal for a perfect mip is the wall texture
                }
            }
            else
            {
                loopi(3)
                {
                    sqr *q = o[i + 1];
                    if(*((uint32_t *)q) != *((uint32_t *)o[0])  // type, floor, ceil, wtex
                    || abs(q->b - o[0]->b) > lighterr
                    || q->ftex != o[0]->ftex
                    || q->ctex != o[0]->ctex
                    || abs(q->r - o[0]->r) > lighterr           // perfect mip even if light is not exactly equal
                    || abs(q->g - o[0]->g) > lighterr
                    || q->utex != o[0]->utex) goto c;
                }
                if(r->type==CHF || r->type==FHF)                // can make a perfect mip out of a hf if slopes lie on one line
                {
                    if(o[0]->vdelta-o[1]->vdelta != o[1]->vdelta-SWS(w,x+2,y,wfactor)->vdelta
                    || o[0]->vdelta-o[2]->vdelta != o[2]->vdelta-SWS(w,x+2,y+2,wfactor)->vdelta
                    || o[0]->vdelta-o[3]->vdelta != o[3]->vdelta-SWS(w,x,y+2,wfactor)->vdelta
                    || o[3]->vdelta-o[2]->vdelta != o[2]->vdelta-SWS(w,x+2,y+1,wfactor)->vdelta
                    || o[1]->vdelta-o[2]->vdelta != o[2]->vdelta-SWS(w,x+1,y+2,wfactor)->vdelta) goto c;
                }
            }
            mip:
            r->defer = 0;
            c:;
            loopi(4) o[i] += 2;
            r++;
        }
    }
    s.x  /= 2;
    s.y  /= 2;
    s.xs /= 2;
    s.ys /= 2;
    remip(s, level+1);
}

void remipmore(const block &b, int level)
{
    block bb = b;
    if(bb.x>1) bb.x--;
    if(bb.y>1) bb.y--;
    if(bb.xs<ssize-3) bb.xs++;
    if(bb.ys<ssize-3) bb.ys++;
    remip(bb, level);
}

static int clentsel = 0, clenttype = NOTUSED;

void nextclosestent(void) { clentsel++; }

void closestenttype(char *what)
{
    clenttype = what[0] ? findtype(what) : NOTUSED;
}

COMMAND(nextclosestent, "");
COMMAND(closestenttype, "s");

bool pinnedclosestent = false;
static int pinnedent = -1;
COMMANDF(toggleclosestentpin, "", () { pinnedclosestent = ents.inrange((pinnedent = pinnedclosestent ? -1 : closestent())); });

int closestent()        // used for delent and edit mode ent display
{
    if(noteditmode("closestent")) return -1;
    if(pinnedclosestent)
    {
        if(ents.inrange(pinnedent) && ents[pinnedent].type != NOTUSED) return pinnedent;
        pinnedclosestent = false; // release lock when ent is deleted
    }
    int best = -1, bcnt = 0;
    float bdist = 99999;
    loopj(3)
    {
        bcnt = 0;
        loopv(ents)
        {
            entity &e = ents[i];
            if(e.type==NOTUSED) continue;
            if(clenttype != NOTUSED && e.type != clenttype) continue;
            bool ice = false;
            loopk(eh_ents.length()) if(eh_ents[k]==e.type) ice = true;
            if(ice) continue;
            vec v(e.x, e.y, e.z);
            float dist = v.dist(camera1->o);
            if(j)
            {
                if(ents[best].x == e.x && ents[best].y == e.y && ents[best].z == e.z)
                {
                    if(j == 2 && bcnt == clentsel) return i;
                    bcnt++;
                }
            }
            else if(dist<bdist)
            {
                best = i;
                bdist = dist;
            }
        }
        if(best < 0 || bcnt == 1) break;
        if(bcnt) clentsel %= bcnt;
    }
    return best;
}

void entproperty(int *prop, float *famount, int *unscaled)
{
    int n = closestent();
    if(n < 0) return;
    entity &e = ents[n];
    int old_a1 = e.attr1;
    int t = e.type < MAXENTTYPES ? e.type : 0;
    int amount = int(*famount * (*prop >= 0 && *prop < 7 && !*unscaled ? entscale[t][*prop] : 1));

    switch(*prop)
    {
        case 0: e.attr1 += amount; break;
        case 1: e.attr2 += amount; break;
        case 2: e.attr3 += amount; break;
        case 3: e.attr4 += amount; break;
        case 4: e.attr5 += amount; break;
        case 5: e.attr6 += amount; break;
        case 6: e.attr7 += amount; break;
        case 11: e.x += amount; break;
        case 12: e.y += amount; break;
        case 13: e.z += amount; break;
    }
    clampentityattributes(e);
    switch(e.type)
    {
        case LIGHT: calclight(); break;
        case SOUND:
            audiomgr.preloadmapsound(e);
            entityreference entref(&e);
            location *loc = audiomgr.locations.find(old_a1, &entref, mapsounds);
            if(loc)
                loc->drop();
    }
    if(changedents.find(n) == -1) changedents.add(n);   // apply ent changes later (reduces network traffic)
    unsavededits++;
}
COMMAND(entproperty, "ifi");

hashtable<char *, enet_uint32> mapinfo, &resdata = mapinfo;

void getenttype()
{
    int e = closestent();
    if(e<0) return;
    int type = ents[e].type;
    if(type < 0 || type >= MAXENTTYPES) return;
    result(entnames[type]);
}
COMMAND(getenttype, "");

void getentattr(int *attr, int *unscaled)
{
    int n = closestent();
    const char *res = "0";
    if(n >= 0)
    {
        entity &e = ents[n];
        int t = e.type < MAXENTTYPES ? e.type : 0;
        int scale = *attr >= 0 && *attr < 7 && !*unscaled ? entscale[t][*attr] : 1;
        switch(*attr)
        {
            #define CHKATTR(x) case x - 1: res = floatstr(float(e.attr##x) / scale, true); break
            CHKATTR(1);
            CHKATTR(2);
            CHKATTR(3);
            CHKATTR(4);
            CHKATTR(5);
            CHKATTR(6);
            CHKATTR(7);
            #undef CHKATTR
        }
    }
    result(res);
}
COMMAND(getentattr, "ii");

void deletesoundentity(entity &e)
{
    entityreference entref(&e);
    location *loc = audiomgr.locations.find(e.attr1, &entref, mapsounds);
    if(loc) loc->drop();
}

vector<persistent_entity> deleted_ents;

void delent()
{
    int n = closestent();
    if(n<0) { conoutf("no more entities"); return; }
    syncentchanges(true);
    int t = ents[n].type;
    conoutf("%s entity deleted", entnames[t % MAXENTTYPES]);

    entity &e = ents[n];
    if(t == SOUND) deletesoundentity(e); //stop playing sound
    deleted_ents.add(e);
    ents[n].type = NOTUSED;
    addmsg(SV_EDITENT, "ri9i3", n, NOTUSED, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

    switch(t)
    {
        case LIGHT: calclight(); break;
    }
    unsavededits++;
}
COMMAND(delent, "");

void undelent(char *index)
{
    if(noteditmode("undelent")) return;
    if(!deleted_ents.length()) { conoutf("no more entities to undelete"); return; }
    int n = isdigit(*index) ? strtol(index, NULL, 0) : deleted_ents.length() - 1;
    if(deleted_ents.inrange(n))
    {
        persistent_entity e = deleted_ents.remove(n);
        int t = e.type < MAXENTTYPES ? e.type : NOTUSED;
        if(OUTBORD(e.x, e.y)) conoutf("failed to undelete %s entity (coordinates outside map borders)", entnames[t]);
        else
        {
            newentity(-1, e.x, e.y, e.z, entnames[t], float(e.attr1) / entscale[t][0], float(e.attr2) / entscale[t][1], float(e.attr3) / entscale[t][2], float(e.attr4) / entscale[t][3]);
            *((persistent_entity *) &ents.last()) = e;
            conoutf("%s entity undeleted", entnames[t]);
        }
    }
}
COMMAND(undelent, "s");

void getdeletedentities()
{
    vector<char> res;
    loopv(deleted_ents)
    {
        persistent_entity &e = deleted_ents[i];
        int t = e.type < MAXENTTYPES ? e.type : NOTUSED;
        cvecprintf(res,"%s %d %d %d  %s\n", entnames[t], e.x, e.y, e.z, formatentityattributes(e));
    }
    if(res.length()) res.last() = '\0';
    else res.add('\0');
    result(res.getbuf());
}
COMMAND(getdeletedentities, "");

void unlistdeletedentity(char *which)
{
    if(!strcasecmp(which, "all")) deleted_ents.setsize(0);
    else if(!strcasecmp(which, "last")) deleted_ents.pop();
    else if(isdigit(*which))
    {
        int n = strtol(which, NULL, 0);
        if(deleted_ents.inrange(n)) deleted_ents.remove(n);
    }
    intret(deleted_ents.length());
}
COMMAND(unlistdeletedentity, "s");

int findtype(const char *what)
{
    int t = getlistindex(what, entnames, true, NOTUSED);
    if(t == NOTUSED) conoutf("unknown entity type \"%s\"", what);
    return t;
}

void newentity(int index, int x, int y, int z, const char *what, float v1f, float v2f, float v3f, float v4f) // add an entity or overwrite an existing one
{
    int type = findtype(what);
    if(type == NOTUSED) return;
    if (index >= 0 && ents[index].type == SOUND) deletesoundentity(ents[index]); // overwriting sound entity
    switch(type)
    { // MAPMODEL, PLAYERSTART and CTF-FLAG use the current camera direction as value for attr1, so attr234 need to be moved
        case MAPMODEL:
            v4f = v3f;
            v3f = v2f;
        case PLAYERSTART:
        case CTF_FLAG:
            v2f = v1f;
            int y = camera1->yaw;
            if(type != PLAYERSTART) y = y + 7 - (y + 7) % 15;
            v1f = y;
            break;
    }

    int v1 = v1f * entscale[type][0], v2 = v2f * entscale[type][1], v3 = v3f * entscale[type][2], v4 = v4f * entscale[type][3];
    entity e(x, y, z, type, v1, v2, v3, v4);

    switch(type)
    {
        case LIGHT:
            if(v1 > 64) e.attr1 = 64;
            if(!v1) e.attr1 = 16;
            if(!v2 && !v3 && !v4) e.attr2 = 255;
            break;
    }
    clampentityattributes(e);
    syncentchanges(true);
    addmsg(SV_EDITENT, "ri9i3", index<0 ? ents.length() : index, type, e.x, e.y, e.z, e.attr1, e.attr2, e.attr3, e.attr4, e.attr5, e.attr6, e.attr7);
    e.spawned = true;
    int oldtype = type;
    if(index<0) ents.add(e);
    else
    {
        oldtype = ents[index].type;
        ents[index] = e;
    }
    if(oldtype!=type) switch(oldtype)
    {
        case LIGHT: calclight(); break;
    }
    switch(type)
    {
        case LIGHT: calclight(); break;
        case SOUND: audiomgr.preloadmapsound(e); break;
    }
    unsavededits++;
}

void entset(char *what, float *a1, float *a2, float *a3, float *a4)
{
    int n = closestent();
    if(n>=0)
    {
        entity &e = ents[n];
        newentity(n, e.x, e.y, e.z, what, *a1, *a2, *a3, *a4);
    }
}

COMMAND(entset, "sffff");

void clearents(char *name)
{
    int type = findtype(name);
    if(noteditmode("clearents") || multiplayer()) return;
    bool found = false;
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type == type)
        {
            if(type == SOUND) deletesoundentity(e);
            deleted_ents.add(e);
            e.type = NOTUSED;
            found = true;
        }
    }
    switch(type)
    {
        case LIGHT: calclight(); break;
    }
    if(found) unsavededits++;
}

COMMAND(clearents, "s");

// entity commands based on entity number (for scripts, singleplayer only)

void deleteentity(char *ns)
{
    int n = ATOI(ns);
    if(noteditmode("deleteentity") || multiplayer(true) || !*ns || !ents.inrange(n)) return;
    entity &e = ents[n];
    int t = e.type;
    if(t == SOUND) deletesoundentity(e);
    conoutf("deleted entity #%d (%s)", n, entnames[e.type]);
    deleted_ents.add(e);
    memset(&e, 0, sizeof(persistent_entity));
    e.type = NOTUSED;
    if(t == LIGHT) calclight();
    unsavededits++;
}
COMMAND(deleteentity, "s");

void editentity(char **args, int numargs) // index x y z a1 a2 a3 a4 ...
{
    string res = "";
    if(numargs > 0)
    {
        int n = ATOI(args[0]);
        if(noteditmode("editentity") || multiplayer(true) || !*args[0] || !ents.inrange(n)) return;
        entity &e = ents[n];
        bool edit = false;
        for(int i = 1; i < numargs; i++) if(*args[i]) edit = true; // only arguments other than empty strings can edit anything - otherwise we're just browsing
        int t = e.type < MAXENTTYPES ? e.type : NOTUSED;
        if(edit)
        {
            if(e.type == SOUND)
            { // disable sound /before/ changing it
                entityreference entref(&e);
                location *loc = audiomgr.locations.find(e.attr1, &entref, mapsounds);
                if(loc) loc->drop();
            }
            for(int i = 1; i < numargs; i++) if(*args[i])
            {
                int v = atof(args[i]) * (i < 4 || i > 10 ? 1 : entscale[t][i - 4]);
                switch(i)
                {
                    case 1: e.x = v; break;
                    case 2: e.y = v; break;
                    case 3: e.z = v; break;
                    case 4: e.attr1 = v; break;
                    case 5: e.attr2 = v; break;
                    case 6: e.attr3 = v; break;
                    case 7: e.attr4 = v; break;
                    case 8: e.attr5 = v; break;
                    case 9: e.attr6 = v; break;
                    case 10: e.attr7 = v; break;
                }
            }
            clampentityattributes(e);
            switch(e.type)
            {
                case LIGHT: calclight(); break;
                case SOUND: audiomgr.preloadmapsound(e); break;
            }
            unsavededits++;
        }
        // give back unchanged or new entity properties
        formatstring(res)("%s %d %d %d  %s", entnames[t], e.x, e.y, e.z, formatentityattributes(e));
    }
    result(res);
}
COMMAND(editentity, "v");

void enumentities(char *type)
{
    vector<char> res;
    int t = getlistindex(type, entnames, false, -1);
    loopv(ents) if(ents[i].type == t) cvecprintf(res, "%d ", i);
    if(!res.length()) res.add('\0');
    else res.last() = '\0';
    result(res.getbuf());
}
COMMAND(enumentities, "s");

void scalecomp(uchar &c, int intens)
{
    int n = c*intens/100;
    if(n>255) n = 255;
    c = n;
}

void scalelights(int f, int intens)
{
    if(multiplayer()) return;
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
        }
        unsavededits++;
    }
    calclight();
}

COMMANDF(scalelights, "ii", (int *f, int *i) { scalelights(*f, *i); });

int findentity(int type, int index)
{
    for(int i = index; i<ents.length(); i++) if(ents[i].type==type) return i;
    loopj(index) if(ents[j].type==type) return j;
    return -1;
}

int findentity(int type, int index, uchar attr2)
{
    for(int i = index; i<ents.length(); i++) if(ents[i].type==type && ents[i].attr2==attr2) return i;
    loopj(index) if(ents[j].type==type && ents[j].attr2==attr2) return j;
    return -1;
}

void nextplayerstart(int *type)
{
    static int cycle = -1;

    if(noteditmode("nextplayerstart")) return;
    cycle = findentity(PLAYERSTART, cycle + 1, *type);
    if(cycle >= 0)
    {
        gotoplayerstart(player1, &ents[cycle]);
        entinmap(player1);
    }
}

COMMAND(nextplayerstart, "i");

sqr *wmip[LARGEST_FACTOR*2];

void setupworld(int factor)
{
    ssize = 1<<(sfactor = factor);
    cubicsize = ssize*ssize;
    mipsize = cubicsize*134/100;
    sqr *w = world = new sqr[mipsize];
    memset(world, 0, mipsize*sizeof(sqr));
    loopi(LARGEST_FACTOR*2) { wmip[i] = w; w += cubicsize>>(i*2); }
}

void sqrdefault(sqr *s)
{
    if(!s) return;
    s->r = s->g = s->b = 150;
    s->ftex = DEFAULT_FLOOR;
    s->ctex = DEFAULT_CEIL;
    s->wtex = s->utex = DEFAULT_WALL;
    s->type = SOLID;
    s->floor = 0;
    s->ceil = 16;
    s->vdelta = s->defer = s->tag = 0;
}

bool worldbordercheck(int x1, int x2, int y1, int y2, int z1, int z2)  // check for solid world border
{
    loop(x, ssize) loop(y, ssize)
    {
        if(x >= x1 && x < ssize - x2 && y >= y1 && y < ssize - y2)
        {
            if(S(x,y)->type != SOLID && (S(x,y)->ceil > 126 - z1 || S(x,y)->floor < -127 + z2)) return false;
        }
        else
        {
            if(S(x,y)->type != SOLID) return false;
        }
    }
    return true;
}

void calcmapdims()
{
    mapdims.x1 = mapdims.y1 = ssize;
    mapdims.x2 = mapdims.y2 = 0;
    mapdims.minfloor = 127; mapdims.maxceil = -128;   // flow initialised those two with "0", but i'd guess, that was by mistake
    sqr *s = world;
    for(int y = 0; y < ssize; y++) for(int x = 0; x < ssize; x++)
    {
        if(!SOLID(s))
        {
            if(OUTBORD(x, y)) conoutf("non-solid cube in world border found: x %d, y %d", x, y); // will also go fatal() below
            if(x < mapdims.x1) mapdims.x1 = x;
            if(x > mapdims.x2) mapdims.x2 = x;
            if(y < mapdims.y1) mapdims.y1 = y;
            mapdims.y2 = y;
            if(s->floor < mapdims.minfloor) mapdims.minfloor = s->floor;   // for some reason, flow excluded cubes with floor == -128 here -  let's try without that...
            if(s->ceil > mapdims.maxceil) mapdims.maxceil = s->ceil;
        }
        s++;
    }
    if(mapdims.x2 < mapdims.x1 || mapdims.y2 < mapdims.y1)
    { // map is completely solid -> default to empty map values
        mapdims.x1 = mapdims.y1 = 2;
        mapdims.x2 = mapdims.y2 = ssize - 3;
        mapdims.minfloor = 0;
        mapdims.maxceil = 16;
    }
    if(mapdims.x1 < MINBORD || mapdims.y1 < MINBORD || mapdims.x2 >= ssize - MINBORD || mapdims.y2 >= ssize - MINBORD)
        fatal("calcmapdims(): world border violation (x:%d..%d, y:%d..%d)", mapdims.x1, mapdims.x2, mapdims.y1, mapdims.y2);
    mapdims.xspan = mapdims.x2 - mapdims.x1 + 1;
    mapdims.yspan = mapdims.y2 - mapdims.y1 + 1;
    mapdims.xm = mapdims.x1 + mapdims.xspan / 2.0f;
    mapdims.ym = mapdims.y1 + mapdims.yspan / 2.0f;
}

bool empty_world(int factor, bool force)    // main empty world creation routine, if passed factor -1 will enlarge old world by 1, factor -2 will shrink old world by 1
{
    if(!force && noteditmode("empty world")) return false;
    if(factor == -2 && !worldbordercheck(ssize/4 + MINBORD, ssize/4 + MINBORD, ssize/4 + MINBORD, ssize/4 + MINBORD, 0, 0)) { conoutf("map does not fit into smaller world"); return false; }
    block *ow = NULL, be = { 0, 0, ssize, ssize }, bs = { ssize/4, ssize/4, ssize/2, ssize/2 };
    int oldfactor = sfactor;
    bool copy = false;
    if(world && factor<0) { factor = sfactor + (factor == -2 ? -1 : 1); copy = true; }
    if(factor<SMALLEST_FACTOR) factor = SMALLEST_FACTOR;
    if(factor>LARGEST_FACTOR) factor = LARGEST_FACTOR;
    if(copy && oldfactor == factor) return false;
    bool shrink = factor < oldfactor;
    bool clearmap = !copy && world;
    if(clearmap && unsavededits) { xmapbackup("newmap", ""); unsavededits = 0; }
    if(copy) ow = blockcopy(shrink ? bs : be);

    extern char *mlayout;
    DELETEA(world);
    DELETEA(mlayout);

    setupworld(factor);
    loop(x,ssize) loop(y,ssize)
    {
        sqrdefault(S(x,y));
    }

    checkselections(); // assert no selection became invalid

    if(copy)
    {
        ow->x = ow->y = shrink ? 0 : ssize/4;
        blockpaste(*ow);
        int moveents = shrink ? -ssize/2 : ssize/4;
        loopv(ents)
        {
            ents[i].x += moveents;
            ents[i].y += moveents;
            if(OUTBORD(ents[i].x, ents[i].y)) ents[i].type = NOTUSED;
        }
        player1->o.x += moveents;
        player1->o.y += moveents;
        entinmap(player1);
    }
    else
    {   // all-new map
        int oldunsavededits = unsavededits;
        memset(&hdr, 0, sizeof(header));
        mapconfigdata.clear();
        formatstring(hdr.maptitle)("Untitled Map by %s", player1->name);
        setfvar("waterlevel", -10000);
        hdr.waterlevel = waterlevel * WATERLEVELSCALING;
        setwatercolor();
        loopk(3) loopi(256) hdr.texlists[k][i] = i;
        ents.shrink(0);
        block b = { 8, 8, ssize-16, ssize - 16};
        edittypexy(SPACE, b);
        clearheaderextras();
        unsavededits = oldunsavededits;
    }
    strncpy(hdr.head, "ACMP", 4);
    hdr.version = MAPVERSION;
    hdr.headersize = sizeof(header);
    hdr.sfactor = sfactor;

    calcmapdims();
    calclight();
    resetmap(!copy);
    if(clearmap)
    {
        pushscontext(IEXC_MAPCFG);
        per_idents = false;
        neverpersist = true;
        execfile("config/default_map_settings.cfg");
        neverpersist = false;
        per_idents = true;
        popscontext();
        setvar("fullbright", 1);
        fullbrightlight();
    }
    if(!copy)
    {
        findplayerstart(player1, true);
        startmap("newmap", false);
    }
    else conoutf("new map size: %d", sfactor);
    freeblock(ow);
    return true;
}

void mapsize(void)
{
    intret(sfactor);
}

COMMAND(mapsize, "");

void mapenlarge()  { if(empty_world(-1, false)) addmsg(SV_NEWMAP, "ri", -1); }
void mapshrink()   { if(empty_world(-2, false)) addmsg(SV_NEWMAP, "ri", -2); }
void newmap(int *i)
{
    if(m_botmode) { conoutf("newmap not supported in bot mode"); return; }
    if(empty_world(*i, false))
    {
        addmsg(SV_NEWMAP, "ri", max(*i, 0));
        exechook(HOOK_SP_MP, "onNewMap", "");
    }
    defformatstring(startmillis)("%d", millis_());
    alias("gametimestart", startmillis, true);
}

COMMAND(mapenlarge, "");
COMMAND(mapshrink, "");
COMMAND(newmap, "i");
COMMANDN(recalc, calclight, "");

void countperfectmips(int mip, int xx, int yy, int bs, int *stats)
{
    int mfactor = sfactor - mip;
    if(!bs) bs = 1 << mfactor;
    for(int y = yy; y < yy + bs; y++) for(int x = xx; x < xx + bs; x++)
    {
        sqr *r = SWS(wmip[mip], x, y, mfactor);
        if(r->defer && mip > 0) countperfectmips(mip - 1, x * 2, y * 2, 2, stats);
        else stats[mip]++;
    }
}

COMMANDF(calcmipstats, "", ()
{
    int st[SMALLEST_FACTOR + 1];
    countperfectmips(SMALLEST_FACTOR, 0, 0, 0, st);
    conoutf("current mips: %d / %d / %d / %d / %d / %d / %d", st[0], st[1], st[2], st[3], st[4], st[5], st[6]);
});

#define INVISWTEX (1<<0)
#define INVISUTEX (1<<1)
#define INVISIBLE (1<<2)

void mapmrproper(bool manual)
{
    int sta[SMALLEST_FACTOR + 1], stb[SMALLEST_FACTOR + 1], stc[SMALLEST_FACTOR + 1];
    sqr *s, *r, *o[4];
    block b = { 0, 0, ssize, ssize };
    if(manual)
    {
        if(noteditmode("mapmrproper") || multiplayer()) return;
        makeundo(b);
    }
    remip(b);
    if(manual) countperfectmips(SMALLEST_FACTOR, 0, 0, 0, sta);

    // basic corner cleanup: do that now
    // corners are rendered at the highest possible mip - squash data of lower mips (except vdelta and tag)
    for(int mip = SMALLEST_FACTOR; mip > 0; mip--)
    {
        int mfactor = sfactor - mip, bs = 1 << mfactor;
        for(int y = 1; y < bs - 1; y++) for(int x = 1; x < bs - 1; x++)
        {
            r = SWS(wmip[mip], x, y, mfactor);
            if(r->type == CORNER) loopi(4)
            {
                #define cc(p) s->p = r->p
                s = SWS(wmip[mip - 1], x * 2 + (i & 1), y * 2 + i / 2, mfactor + 1);
                cc(floor); cc(ceil); cc(wtex); cc(ftex); cc(ctex); cc(utex);
                #undef cc
            }
        }
    }

    // generate special mipmap without considering light or utex/wtex -> we'll try to reach this by optimization...
    sqr *worldbackup = (sqr *) memcpy(new sqr[ssize*ssize], world, ssize*ssize*sizeof(sqr));
    r = world; loopirev(ssize * ssize) { r->wtex = r->utex = r->r = r->g = r->b = 0; r++; }
    remip(b);
    if(manual) countperfectmips(SMALLEST_FACTOR, 0, 0, 0, stb);
    memcpy(world, worldbackup, ssize*ssize*sizeof(sqr));
    delete[] worldbackup;

    // default to invisible, we'll mark all visible afterwards
    r = world; loopirev(ssize * ssize) (r++)->reserved[0] = INVISUTEX|INVISWTEX;

    // check visibility of wall & upper wall in mip 0
    for(int y = 0; y < ssize; y++) for(int x = 0; x < ssize; x++)
    {
        s = S(x,y); uchar &rs = s->reserved[0];
        if(x < 1 || x > ssize - 2 || y < 1 || y > ssize - 2) continue; // we need 1 cube safety-space - all outside is invisible anyway
        o[0] = s - ssize;
        o[1] = s + ssize;
        o[2] = s + 1;
        o[3] = s - 1;
        loopi(4) if(!SOLID(o[i]))
        {
            if(SOLID(s) || s->floor > o[i]->floor || (o[i]->type == FHF && (s->type != FHF || s->floor != o[i]->floor))) rs &= ~INVISWTEX; // wall texture visible
            if(!SOLID(s) && (s->ceil < o[i]->ceil || (o[i]->type == CHF && (s->type != CHF || s->ceil != o[i]->ceil)))) rs &= ~INVISUTEX; // upper wall texture visible
        }
        if(SOLID(s) && SOLID(o[0]) && SOLID(o[1]) && SOLID(o[2]) && SOLID(o[3])) rs |= INVISIBLE;
    }

    // corners are rendered at the highest possible mip - make sure, the right helper cubes are marked visible (including actually hidden ones)
    // (this is a lot of effort, but there's no easier way... savemap's toptimize() does it wrong, for example
    for(int mip = SMALLEST_FACTOR; mip > 0; mip--)
    {
        int mfactor = sfactor - mip, bs = 1 << mfactor;
        for(int y = 1; y < bs - 1; y++) for(int x = 1; x < bs - 1; x++)
        {
            s = SWS(wmip[mip], x, y, mfactor);
            if(s->type == CORNER && (mip == SMALLEST_FACTOR || SWS(wmip[mip + 1], x / 2, y / 2, mfactor - 1)->type != CORNER))
            { // highest corner mip (this one is rendered) - mark helper cubes
                sqr *z = SWS(s,-1,0,mfactor);
                sqr *t = SWS(s,1,0,mfactor);
                sqr *v = SWS(s,0,1,mfactor);
                sqr *w = SWS(s,0,-1,mfactor);
                if(SOLID(z) || SOLID(t))
                {
                    if(SOLID(w))      S(x << mip, (y - 1) << mip)->reserved[0] &= ~(INVISWTEX|INVISIBLE);
                    else if(SOLID(v)) S(x << mip, (y + 1) << mip)->reserved[0] &= ~(INVISWTEX|INVISIBLE);
                }
            }
        }
    }

    // propagate visibility and visible textures into higher mips
    for(int mip = 0; mip < SMALLEST_FACTOR; mip++)
    {
        int mfactor = sfactor - mip, bs = 1 << mfactor;
        for(int y = 0; y < bs; y++) for(int x = 0; x < bs; x++)
        {
            s = SWS(wmip[mip], x, y, mfactor);
            r = SWS(wmip[mip + 1], x / 2, y / 2, mfactor - 1);
            uchar &rs = s->reserved[0], &rr = r->reserved[0];
            if(!((x | y) & 1)) rr = rs;
            else rr &= rs;
            if(!(rs & INVISWTEX)) r->wtex = s->wtex;
            if(!(rs & INVISUTEX)) r->utex = s->utex;
        }
    }

    // corners are rendered at the highest possible mip - make sure, the right cubes of the lot is marked visible (including actually hidden ones)
    for(int mip = SMALLEST_FACTOR; mip > 0; mip--)
    {
        int mfactor = sfactor - mip, bs = 1 << mfactor;
        for(int y = 1; y < bs - 1; y++) for(int x = 1; x < bs - 1; x++)
        {
            s = SWS(wmip[mip], x, y, mfactor);
            if(s->type == CORNER && (mip == SMALLEST_FACTOR || SWS(wmip[mip + 1], x / 2, y / 2, mfactor - 1)->type != CORNER))
            { // highest mip - mark the important cube like the highest mip
                S(x << mip, y << mip)->reserved[0] = s->reserved[0];
            }
        }
    }           // visibility flags complete ;)
    //r = world; loopirev(ssize * ssize) { r->tag = (r->tag & 0xc0) + r->reserved[0]; r++; }  // easy debugging

    // optimize utex & wtex according to mips & visibility
    #define domip0(b) for(int yy = y << mip; yy < (y + 1) << mip; yy++) for(int xx = x << mip; xx < (x + 1) << mip; xx++) { s = S(xx,yy); uchar &rs = s->reserved[0]; b ; }
    for(int mip = SMALLEST_FACTOR; mip > 0; mip--)
    {
        int mfactor = sfactor - mip, bs = 1 << mfactor;
        for(int y = 0; y < bs; y++) for(int x = 0; x < bs; x++)
        {
            r = SWS(wmip[mip], x, y, mfactor);
            if(mip > 1) loopi(4)
            {
                s = SWS(wmip[mip - 1], x * 2 + (i & 1), y * 2 + i / 2, mfactor + 1);
                uchar &rs = s->reserved[0];
                if(rs & INVISWTEX) s->wtex = r->wtex;
                if(rs & INVISUTEX) s->utex = r->utex;
            }
            if(r->defer) continue;
            if(r->reserved[0] & INVISIBLE)
            {
                domip0(rs &= INVISIBLE;);
                continue; // do not touch hidden solid blocks... for some reason it messes up filesize
            }
            int utex = -1, wtex = -1, udiff = 0, wdiff = 0;
            domip0(
                if(!(rs & INVISWTEX) && s->wtex != wtex)
                {
                    wtex = s->wtex;
                    wdiff++;
                }
                if(!(rs & INVISUTEX) && s->utex != utex)
                {
                    utex = s->utex;
                    udiff++;
                }
            );
            if(!wdiff) wtex = r->wtex;
            if(!udiff) utex = r->utex;
            if(wdiff < 2) domip0(if(rs & INVISWTEX) s->wtex = wtex; rs &= ~(INVISWTEX|INVISIBLE); );
            if(udiff < 2) domip0(if(rs & INVISUTEX) s->utex = utex; rs &= ~INVISUTEX; );
        }
    }

    loopirev(ssize * ssize) world[i].reserved[0] = 0;  // cleanup

    // recalc everything with the optimized data
    calclight();
    if(manual)
    {
        countperfectmips(SMALLEST_FACTOR, 0, 0, 0, stc);
        conoutf("before: %d / %d / %d / %d / %d / %d / %d", sta[0], sta[1], sta[2], sta[3], sta[4], sta[5], sta[6]);
        conoutf("idealized: %d / %d / %d / %d / %d / %d / %d", stb[0], stb[1], stb[2], stb[3], stb[4], stb[5], stb[6]);
        conoutf("final count: %d / %d / %d / %d / %d / %d / %d", stc[0], stc[1], stc[2], stc[3], stc[4], stc[5], stc[6]);
        unsavededits++;
    }
}
COMMANDF(mapmrproper, "", () { mapmrproper(true); });
