// worldlight.cpp

#include "cube.h"

extern bool hasoverbright;

VAR(lightscale,1,4,100);

void lightray(float bx, float by, persistent_entity &light)     // done in realtime, needs to be fast
{
    float lx = light.x+(rnd(21)-10)*0.1f;
    float ly = light.y+(rnd(21)-10)*0.1f;
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
    int l = light.attr2<<PRECBITS;
    int stepx = (int)(dx/(float)steps*PRECF);
    int stepy = (int)(dy/(float)steps*PRECF);
    int stepl = fast_f2nat(l/(float)steps); // incorrect: light will fade quicker if near edge of the world

    if(hasoverbright)
    {
        l /= lightscale;
        stepl /= lightscale;
        
        if(light.attr3 || light.attr4)      // coloured light version, special case because most lights are white
        {
            int dimness = rnd((255-(light.attr2+light.attr3+light.attr4)/3)/16+1);  
            x += stepx*dimness;
            y += stepy*dimness;

            if(OUTBORD(x>>PRECBITS, y>>PRECBITS)) return;

            int g = light.attr3<<PRECBITS;
            int stepg = fast_f2nat(g/(float)steps);
            int b = light.attr4<<PRECBITS;
            int stepb = fast_f2nat(b/(float)steps);
            g /= lightscale;
            stepg /= lightscale;
            b /= lightscale;
            stepb /= lightscale;
            loopi(steps)
            {
                sqr *s = S(x>>PRECBITS, y>>PRECBITS); 
                int tl = (l>>PRECBITS)+s->r;
                s->r = tl>255 ? 255 : tl;
                tl = (g>>PRECBITS)+s->g;
                s->g = tl>255 ? 255 : tl;
                tl = (b>>PRECBITS)+s->b;
                s->b = tl>255 ? 255 : tl;
                if(SOLID(s)) return;
                x += stepx;
                y += stepy;
                l -= stepl;
                g -= stepg;
                b -= stepb;
                stepl -= 25;
                stepg -= 25;
                stepb -= 25;
            };
        }
        else        // white light, special optimized version
        {
            int dimness = rnd((255-light.attr2)/16+1);  
            x += stepx*dimness;
            y += stepy*dimness;

            if(OUTBORD(x>>PRECBITS, y>>PRECBITS)) return;

            loopi(steps)
            {
                sqr *s = S(x>>PRECBITS, y>>PRECBITS); 
                int tl = (l>>PRECBITS)+s->r;
                s->r = s->g = s->b = tl>255 ? 255 : tl;       
                if(SOLID(s)) return;
                x += stepx;
                y += stepy;
                l -= stepl;
                stepl -= 25;
            };
        };
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
        };
    };
    
};

void calclightsource(persistent_entity &l)
{
    int reach = l.attr1;
    int sx = l.x-reach;
    int ex = l.x+reach;
    int sy = l.y-reach;
    int ey = l.y+reach;
    
    rndreset();
    
    const float s = 0.8f;

    for(float sx2 = (float)sx; sx2<=ex; sx2+=s*2) { lightray(sx2, (float)sy, l); lightray(sx2, (float)ey, l); };
    for(float sy2 = sy+s; sy2<=ey-s; sy2+=s*2)    { lightray((float)sx, sy2, l); lightray((float)ex, sy2, l); };
    
    rndtime();
};

void postlightarea(block &a)    // median filter, smooths out random noise in light and makes it more mipable
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
    };

    remip(a);
};

void calclight()
{
    loop(x,ssize) loop(y,ssize)
    {
        sqr *s = S(x,y);
        s->r = s->g = s->b = 10;
    };

    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==LIGHT) calclightsource(e);
    };
    
    block b = { 1, 1, ssize-2, ssize-2 };
    postlightarea(b);
    setvar("fullbright", 0);
};

VAR(dynlight, 0, 16, 32);

vector<block *> dlights;

void cleardlights()
{
    while(!dlights.empty())
    {
        block *backup = dlights.pop();
        blockpaste(*backup);
        free(backup);    
    };
};

void dodynlight(vec &vold, vec &v, int reach, int strength, dynent *owner)
{
    if(!reach) reach = dynlight;
    if(owner->monsterstate) reach = reach/2;
    if(!reach) return;
    
    int creach = reach+16;  // dependant on lightray random offsets!
    block b = { (int)v.x-creach, (int)v.y-creach, creach*2+1, creach*2+1 };

    if(b.x<1) b.x = 1;   
    if(b.y<1) b.y = 1;
    if(b.xs+b.x>ssize-2) b.xs = ssize-2-b.x;
    if(b.ys+b.y>ssize-2) b.ys = ssize-2-b.y;

    dlights.add(blockcopy(b));      // backup area before rendering in dynlight

    persistent_entity l = { (int)v.x, (int)v.y, (int)v.z, reach, LIGHT, strength, 0, 0 };
    calclightsource(l);
    postlightarea(b);
};

// utility functions also used by editing code

block *blockcopy(block &s)
{
    block *b = (block *)alloc(sizeof(block)+s.xs*s.ys*sizeof(sqr));
    *b = s;
    sqr *q = (sqr *)(b+1);
    for(int x = s.x; x<s.xs+s.x; x++) for(int y = s.y; y<s.ys+s.y; y++) *q++ = *S(x,y);
    return b;
};

void blockpaste(block &b)
{
    sqr *q = (sqr *)((&b)+1);
    for(int x = b.x; x<b.xs+b.x; x++) for(int y = b.y; y<b.ys+b.y; y++) *S(x,y) = *q++;
    remip(b);
};


