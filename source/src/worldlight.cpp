// worldlight.cpp

#include "cube.h"

VAR(lightscale,1,4,100);

void lightray(float bx, float by, const persistent_entity &light, float fade = 1, bool flicker = false)     // done in realtime, needs to be fast
{
    float lx = light.x+(flicker ? (rnd(21)-10)*0.1f : 0);
    float ly = light.y+(flicker ? (rnd(21)-10)*0.1f : 0);
    float dx = bx-lx;
    float dy = by-ly;
    float dist = (float)sqrt(dx*dx+dy*dy);
    if(dist<1.0f) return;
    int reach = light.attr1;
    int steps = (int)(reach*reach*1.6f/dist); // can change this for speedup/quality?
    const int PRECBITS = 12;
    const float PRECF = 4096.0f;
    int x = (int)(lx*PRECF);
    int y = (int)(ly*PRECF);
    int fadescale = (int)(fade*PRECF);
    int l = light.attr2*fadescale;
    int stepx = (int)(dx/(float)steps*PRECF);
    int stepy = (int)(dy/(float)steps*PRECF);
    int stepl = (int)(l/(float)steps); // incorrect: light will fade quicker if near edge of the world

    if(maxtmus)
    {
        l /= lightscale;
        stepl /= lightscale;

        if(light.attr3 || light.attr4)      // coloured light version, special case because most lights are white
        {
            if(flicker)
            {
                int dimness = rnd((((255<<PRECBITS)-(int(light.attr2)+int(light.attr3)+int(light.attr4))*fadescale/3)>>(PRECBITS+4))+1);
                x += stepx*dimness;
                y += stepy*dimness;
            }

            if(OUTBORD(x>>PRECBITS, y>>PRECBITS)) return;

            int g = light.attr3*fadescale;
            int stepg = (int)(g/(float)steps);
            int b = light.attr4*fadescale;
            int stepb = (int)(b/(float)steps);
            g /= lightscale;
            stepg /= lightscale;
            b /= lightscale;
            stepb /= lightscale;
            loopi(steps)
            {
                sqr *s = S(x>>PRECBITS, y>>PRECBITS);
                s->r = min((l>>PRECBITS)+s->r, 255);
                s->g = min((g>>PRECBITS)+s->g, 255);
                s->b = min((b>>PRECBITS)+s->b, 255);
                if(SOLID(s)) return;
                x += stepx;
                y += stepy;
                l -= stepl;
                g -= stepg;
                b -= stepb;
                stepl -= 25;
                stepg -= 25;
                stepb -= 25;
            }
        }
        else        // white light, special optimized version
        {
            if(flicker)
            {
                int dimness = rnd((((255<<PRECBITS)-(light.attr2*fadescale))>>(PRECBITS+4))+1);
                x += stepx*dimness;
                y += stepy*dimness;
            }

            if(OUTBORD(x>>PRECBITS, y>>PRECBITS)) return;

            if(hdr.ambient > 0xFF) loopi(steps)
            {
                sqr *s = S(x>>PRECBITS, y>>PRECBITS);
                s->r = min((l>>PRECBITS)+s->r, 255);
                s->g = min((l>>PRECBITS)+s->g, 255);
                s->b = min((l>>PRECBITS)+s->b, 255);
                if(SOLID(s)) return;
                x += stepx;
                y += stepy;
                l -= stepl;
                stepl -= 25;
            }
            else loopi(steps)
            {
                sqr *s = S(x>>PRECBITS, y>>PRECBITS);
                s->r = s->g = s->b = min((l>>PRECBITS)+s->r, 255);
                if(SOLID(s)) return;
                x += stepx;
                y += stepy;
                l -= stepl;
                stepl -= 25;
            }
        }
    }
    else        // the old (white) light code, here for the few people with old video cards that don't support overbright
    {
        loopi(steps)
        {
            sqr *s = S(x>>PRECBITS, y>>PRECBITS);
            int light = l>>PRECBITS;
            if(light>s->r) s->r = s->g = s->b = (uchar)light;
            if(SOLID(s)) return;
            x += stepx;
            y += stepy;
            l -= stepl;
        }
    }
}

void calclightsource(const persistent_entity &l, float fade = 1, bool flicker = true)
{
    int reach = l.attr1;
    int sx = l.x-reach;
    int ex = l.x+reach;
    int sy = l.y-reach;
    int ey = l.y+reach;

    const float s = 0.8f;

    for(float sx2 = (float)sx; sx2<=ex; sx2+=s*2) { lightray(sx2, (float)sy, l, fade, flicker); lightray(sx2, (float)ey, l, fade, flicker); }
    for(float sy2 = sy+s; sy2<=ey-s; sy2+=s*2)    { lightray((float)sx, sy2, l, fade, flicker); lightray((float)ex, sy2, l, fade, flicker); }
}

void postlightarea(const block &a)    // median filter, smooths out random noise in light and makes it more mipable
{
    loop(x,a.xs) loop(y,a.ys)   // assumes area not on edge of world
    {
        sqr *s = S(x+a.x,y+a.y);
        #define median(m) s->m = (s->m*2 + SW(s,1,0)->m*2  + SW(s,0,1)->m*2 \
                                         + SW(s,-1,0)->m*2 + SW(s,0,-1)->m*2 \
                                         + SW(s,1,1)->m    + SW(s,1,-1)->m \
                                         + SW(s,-1,1)->m   + SW(s,-1,-1)->m)/14;  // median is 4/2/1 instead
        median(r);
        median(g);
        median(b);
    }

    remip(a);
}

int lastcalclight = 0;

VARP(fullbrightlevel, 0, 176, 255);

void fullbrightlight(int level)
{
    if(level < 0) level = fullbrightlevel;

    loopi(mipsize) world[i].r = world[i].g = world[i].b = level;
    lastcalclight = totalmillis;
}

VARF(ambient, 0, 0, 0xFFFFFF, if(!noteditmode("ambient")) { hdr.ambient = ambient; calclight(); });

void calclight()
{
    bvec acol((hdr.ambient>>16)&0xFF, (hdr.ambient>>8)&0xFF, hdr.ambient&0xFF);
    if(!acol.x && !acol.y)
    {
        if(!acol.z) acol.z = 10;
        acol.x = acol.y = acol.z;
    }
    else if(!maxtmus) acol.x = acol.y = acol.z = max(max(acol.x, acol.y), acol.z); // the old (white) light code, here for the few people with old video cards that don't support overbright
    loop(x,ssize) loop(y,ssize)
    {
        sqr *s = S(x,y);
        s->r = acol.x;
        s->g = acol.y;
        s->b = acol.z;
    }

    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==LIGHT) calclightsource(e);
    }

    block b = { 1, 1, ssize-2, ssize-2 };
    postlightarea(b);
    setvar("fullbright", 0);
    lastcalclight = totalmillis;
}

struct dlight
{
    physent *owner;
    vec offset, o;
    block *area;
    int reach, fade, expire;
    uchar r, g, b;

    float calcintensity() const
    {
        if(!fade || lastmillis < expire - fade) return 1.0f;
        return max(float(expire - lastmillis)/fade, 0.0f);
    }
};

vector<dlight> dlights;

VARP(dynlight, 0, 1, 1);

static inline bool insidearea(const block &a, const block &b)
{
    return b.x >= a.x && b.y >= a.y && b.x+b.xs <= a.x+a.xs && b.y+b.ys <= a.y+a.ys;
}

void preparedynlight(dlight &d)
{
    block area = { (int)d.o.x-d.reach, (int)d.o.y-d.reach, d.reach*2+1, d.reach*2+1 };

    if(area.x<1) { area.xs = max(area.xs - (1 - area.x), 0); area.x = 1; }
    else if(area.x>ssize-2) { area.x = ssize-2; area.xs = 0; }
    if(area.y<1) { area.ys = max(area.ys - (1 - area.y), 0); area.y = 1; }
    else if(area.y>ssize-2) { area.y = ssize-2; area.ys = 0; }
    if(area.x+area.xs>ssize-2) area.xs = ssize-2-area.x;
    if(area.y+area.ys>ssize-2) area.ys = ssize-2-area.y;

    if(d.area)
    {
        if(insidearea(*d.area, area)) return;

        freeblock(d.area);
    }
    d.area = blockcopy(area);      // backup area before rendering in dynlight
}

void adddynlight(physent *owner, const vec &o, int reach, int expire, int fade, uchar r, uchar g, uchar b)
{
    if(!dynlight) return;

    dlight &d = dlights.add();
    d.owner = owner;
    d.o = o;
    if(d.owner)
    {
        d.offset = d.o;
        d.offset.sub(d.owner->o);
    }
    else d.offset = vec(0, 0, 0);
    d.reach = reach;
    d.fade = fade;
    d.expire = lastmillis + expire;
    d.r = r;
    d.g = g;
    d.b = b;

    d.area = NULL;
    preparedynlight(d);
}

void cleardynlights()
{
    loopv(dlights) freeblock(dlights[i].area);
    dlights.shrink(0);
}

void removedynlights(physent *owner)
{
    loopv(dlights) if(dlights[i].owner==owner)
    {
        freeblock(dlights[i].area);
        dlights.remove(i--);
    }
}

void dodynlights()
{
    if(dlights.empty()) return;
    const block *area = NULL;
    loopv(dlights)
    {
        dlight &d = dlights[i];
        if(lastmillis >= d.expire)
        {
            freeblock(d.area);
            dlights.remove(i--);
            continue;
        }
        if(d.owner)
        {
            vec oldo(d.o);
            d.o = d.owner->o;
            d.o.add(d.offset);
            if(d.o != oldo) preparedynlight(dlights[i]);
        }
    }
    loopv(dlights)
    {
        dlight &d = dlights[i];
        persistent_entity l((int)d.o.x, (int)d.o.y, (int)d.o.z, LIGHT, d.reach, d.r, d.g, d.b);
        calclightsource(l, d.calcintensity(), false);
        if(area)
        {
            if(insidearea(*area, *d.area)) continue;
            if(!insidearea(*d.area, *area)) postlightarea(*area);
        }
        area = d.area;
    }
    if(area) postlightarea(*area);
    lastcalclight = totalmillis;
}

void undodynlights()
{
    if(dlights.empty()) return;
    const block *area = NULL;
    loopvrev(dlights)
    {
        const dlight &d = dlights[i];
        if(area)
        {
            if(insidearea(*area, *d.area)) continue;
            if(!insidearea(*d.area, *area)) blockpaste(*area);
        }
        area = d.area;
    }
    if(area) blockpaste(*area);
}

// utility functions also used by editing code

block *blockcopy(const block &s)
{
    block *b = (block *)new uchar[sizeof(block)+s.xs*s.ys*sizeof(sqr)];
    *b = s;
    sqr *q = (sqr *)(b+1);
    for(int y = s.y; y<s.ys+s.y; y++) for(int x = s.x; x<s.xs+s.x; x++) *q++ = *S(x,y);
    return b;
}

void blockpaste(const block &b, int bx, int by, bool light)
{
    const sqr *q = (const sqr *)((&b)+1);
    sqr *dest = 0;
    uchar tr, tg, tb;

    for(int y = by; y<b.ys+by; y++)
    for(int x = bx; x<b.xs+bx; x++)
    {
        dest = S(x,y);

        // retain light info for edit mode paste
        tr = dest->r;
        tg = dest->g;
        tb = dest->b;

        *dest = *q++;

        if (light) //edit mode paste
        {
            dest->r = tr;
            dest->g = tg;
            dest->b = tb;
        }
    }
    remipmore(b);
}

void blockpaste(const block &b)
{
    blockpaste(b, b.x, b.y, false);
}

void freeblock(block *&b)
{
    if(b) { delete[] (uchar *)b; b = NULL; }
}

