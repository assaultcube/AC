// worldrender.cpp: goes through all cubes in top down quad tree fashion, determines what has to
// be rendered and how (depending on neighbouring cubes), then calls functions in rendercubes.cpp

#include "cube.h"

void render_wall(sqr *o, sqr *s, int x1, int y1, int x2, int y2, int mip, sqr *d1, sqr *d2, bool topleft, int dir)
{
    if(minimap) return;
    if(SOLID(o) || o->type==SEMISOLID)
    {
        float c1 = s->floor;
        float c2 = s->floor;
        if(s->type==FHF) { c1 -= d1->vdelta/4.0f; c2 -= d2->vdelta/4.0f; }
        float f1 = s->ceil;
        float f2 = s->ceil;
        if(s->type==CHF) { f1 += d1->vdelta/4.0f; f2 += d2->vdelta/4.0f; }
        //if(f1-c1<=0 && f2-c2<=0) return;
        render_square(o->wtex, c1, c2, f1, f2, x1<<mip, y1<<mip, x2<<mip, y2<<mip, 1<<mip, d1, d2, topleft, dir);
        return;
    }
    {
        float f1 = s->floor;
        float f2 = s->floor;
        float c1 = o->floor;
        float c2 = o->floor;
        if(o->type==FHF && s->type!=FHF)
        {
            c1 -= d1->vdelta/4.0f;
            c2 -= d2->vdelta/4.0f;
        }
        if(s->type==FHF && o->type!=FHF)
        {
            f1 -= d1->vdelta/4.0f;
            f2 -= d2->vdelta/4.0f;
        }
        if(f1>=c1 && f2>=c2) goto skip;
        render_square(o->wtex, f1, f2, c1, c2, x1<<mip, y1<<mip, x2<<mip, y2<<mip, 1<<mip, d1, d2, topleft, dir);
    }
    skip:
    {
        float f1 = o->ceil;
        float f2 = o->ceil;
        float c1 = s->ceil;
        float c2 = s->ceil;
        if(o->type==CHF && s->type!=CHF)
        {
            f1 += d1->vdelta/4.0f;
            f2 += d2->vdelta/4.0f;
        }
        else if(s->type==CHF && o->type!=CHF)
        {
            c1 += d1->vdelta/4.0f;
            c2 += d2->vdelta/4.0f;
        }
        if(c1<=f1 && c2<=f2) return;
        render_square(o->utex, f1, f2, c1, c2, x1<<mip, y1<<mip, x2<<mip, y2<<mip, 1<<mip, d1, d2, topleft, dir);
    }
}

const int MAX_MIP = 5;   // 32x32 unit blocks
const int MIN_LOD = 2;
const int LOW_LOD = 25;
const int MAX_LOD = 250;

int lod = 40, lodtop, lodbot, lodleft, lodright;
int min_lod;

int lod_factor() { return lod; }

VARP(minlod, LOW_LOD, 60, MAX_LOD);

int stats[LARGEST_FACTOR];

// detect those cases where a higher mip solid has a visible wall next to lower mip cubes
// (used for wall rendering below)

bool issemi(int mip, int x, int y, int x1, int y1, int x2, int y2)      
{
    if(!(mip--)) return true;
    sqr *w = wmip[mip];
    int mfactor = sfactor - mip;
    x *= 2;
    y *= 2;
    switch(SWS(w, x+x1, y+y1, mfactor)->type)
    {
        case SEMISOLID: if(issemi(mip, x+x1, y+y1, x1, y1, x2, y2)) return true;
        case CORNER:
        case SOLID: break;
        default: return true;
    }
    switch(SWS(w, x+x2, y+y2, mfactor)->type)
    {
        case SEMISOLID: if(issemi(mip, x+x2, y+y2, x1, y1, x2, y2)) return true;
        case CORNER:
        case SOLID: break;
        default: return true;
    }
    return false;
}

bool render_floor, render_ceil;

// the core recursive function, renders a rect of cubes at a certain mip level from a viewer perspective
// call itself for lower mip levels, on most modern machines however this function will use the higher
// mip levels only for perfect mips.

void render_seg_new(float vx, float vy, float vh, int mip, int x, int y, int xs, int ys)
{
    sqr *w = wmip[mip];
    int mfactor = sfactor - mip;
    int sz = 1<<mfactor;
    int vxx = ((int)vx+(1<<mip)/2)>>mip;
    int vyy = ((int)vy+(1<<mip)/2)>>mip;
    int lx = vxx-lodleft;   // these mark the rect inside the current rest that we want to render using a lower mip level
    int ly = vyy-lodtop;
    int rx = vxx+lodright;
    int ry = vyy+lodbot;

    float fsize = (float)(1<<mip);
    for(int oy = y; oy<ys; oy++) for(int ox = x; ox<xs; ox++)       // first collect occlusion information for this block
    {
        SWS(w,ox,oy,mfactor)->occluded = isoccluded(camera1->o.x, camera1->o.y, (float)(ox<<mip), (float)(oy<<mip), fsize);
    }
    
    int pvx = (int)vx>>mip;
    int pvy = (int)vy>>mip;
    if(pvx>=0 && pvy>=0 && pvx<sz && pvy<sz)
    {
        //SWS(w,vxx,vyy,mfactor)->occluded = 0; 
        SWS(w, pvx, pvy, mfactor)->occluded = 0;  // player cell never occluded
    }

    #define df(x) s->floor-(x->vdelta/4.0f)
    #define dc(x) s->ceil+(x->vdelta/4.0f)
    
    // loop through the rect 3 times (for floor/ceil/walls seperately, to facilitate dynamic stripify)
    // for each we skip occluded cubes (occlusion at higher mip levels is a big time saver!).
    // during the first loop (ceil) we collect cubes that lie within the lower mip rect and are
    // also deferred, and render them recursively. Anything left (perfect mips and higher lods) we
    // render here.

    #define LOOPH {for(int yy = y; yy<ys; yy++) for(int xx = x; xx<xs; xx++) { \
                  sqr *s = SWS(w,xx,yy,mfactor); if(s->occluded) continue; \
                  if(s->defer && mip && xx>=lx && xx<rx && yy>=ly && yy<ry)
    #define LOOPD sqr *t = SWS(s,1,0,mfactor); \
                  sqr *u = SWS(s,1,1,mfactor); \
                  sqr *v = SWS(s,0,1,mfactor);

    int rendered = 0;
    LOOPH // floors
        {
            int start = xx;
            sqr *next;
            while(xx<xs-1 && (next = SWS(w,xx+1,yy,mfactor))->defer && !next->occluded) xx++;    // collect 2xN rect of lower mip
            render_seg_new(vx, vy, vh, mip-1, start*2, yy*2, xx*2+2, yy*2+2);
            continue;
        }
        rendered++;
        LOOPD
        switch(s->type)
        {
            case SPACE:
            case CHF:
                if(s->floor<=vh && render_floor)
                {
                    render_flat(s->ftex, xx<<mip, yy<<mip, 1<<mip, s->floor, s, t, u, v, false);
                    if(s->floor<hdr.waterlevel && !reflecting) addwaterquad(xx<<mip, yy<<mip, 1<<mip);
                }
                break;
            case FHF:
                render_flatdelta(s->ftex, xx<<mip, yy<<mip, 1<<mip, df(s), df(t), df(u), df(v), s, t, u, v, false);
                if(s->floor-s->vdelta/4.0f<hdr.waterlevel && !reflecting) addwaterquad(xx<<mip, yy<<mip, 1<<mip);
                break;
        }
    }}

    if(!rendered) return;
    stats[mip] += rendered;

    if(!minimap) LOOPH continue; // ceils
        LOOPD
        switch(s->type)
        {
            case SPACE:
            case FHF:
                if(s->ceil>=vh && render_ceil)
                    render_flat(s->ctex, xx<<mip, yy<<mip, 1<<mip, s->ceil, s, t, u, v, true);
                break;
            case CHF:
                render_flatdelta(s->ctex, xx<<mip, yy<<mip, 1<<mip, dc(s), dc(t), dc(u), dc(v), s, t, u, v, true);
                break;
        }
    }}

    LOOPH continue; // walls
        LOOPD
        //  w
        // zSt
        //  vu

        sqr *w = SWS(s,0,-1,mfactor);
        sqr *z = SWS(s,-1,0,mfactor);
        bool normalwall = true;

        if(s->type==CORNER)
        {
            // cull also
            bool topleft = true;
            sqr *h1 = NULL;
            sqr *h2 = NULL;
            if(SOLID(z))
            {
                if(SOLID(w))      { render_wall(w, h2 = s, xx+1, yy, xx, yy+1, mip, t, v, false, 4); topleft = false; }
                else if(SOLID(v)) { render_wall(v, h2 = s, xx, yy, xx+1, yy+1, mip, s, u, false, 5); }
            }
            else if(SOLID(t))
            {
                if(SOLID(w))      { render_wall(w, h1 = s, xx+1, yy+1, xx, yy, mip, u, s, false, 6); }
                else if(SOLID(v)) { render_wall(v, h1 = s, xx, yy+1, xx+1, yy, mip, v, t, false, 7); topleft = false; }
            }
            else
            {
                normalwall = false;
                bool wv = w->ceil-w->floor < v->ceil-v->floor;
                if(z->ceil-z->floor < t->ceil-t->floor)
                {
                    if(wv) { render_wall(h1 = s, h2 = v, xx+1, yy, xx, yy+1, mip, t, v, false, 4); topleft = false; }
                    else   { render_wall(h1 = s, h2 = w, xx, yy, xx+1, yy+1, mip, s, u, false, 5); }
                }
                else
                {
                    if(wv) { render_wall(h2 = s, h1 = v, xx+1, yy+1, xx, yy, mip, u, s, false, 6); }
                    else   { render_wall(h2 = s, h1 = w, xx, yy+1, xx+1, yy, mip, v, t, false, 7); topleft = false; }
                }
            }
            render_tris(xx<<mip, yy<<mip, 1<<mip, topleft, h1, h2, s, t, u, v);
        }

        if(normalwall)
        {
            bool inner = xx!=sz-1 && yy!=sz-1;

            if(xx>=vxx && xx!=0 && yy!=sz-1 && !SOLID(z) && (!SOLID(s) || z->type!=CORNER)
                && (z->type!=SEMISOLID || issemi(mip, xx-1, yy, 1, 0, 1, 1)))
                render_wall(s, z, xx,   yy,   xx,   yy+1, mip, s, v, true, 0);
            if(xx<=vxx && inner && !SOLID(t) && (!SOLID(s) || t->type!=CORNER)
                && (t->type!=SEMISOLID || issemi(mip, xx+1, yy, 0, 0, 0, 1)))
                render_wall(s, t, xx+1, yy,   xx+1, yy+1, mip, t, u, false, 1);
            if(yy>=vyy && yy!=0 && xx!=sz-1 && !SOLID(w) && (!SOLID(s) || w->type!=CORNER)
                && (w->type!=SEMISOLID || issemi(mip, xx, yy-1, 0, 1, 1, 1)))
                render_wall(s, w, xx,   yy,   xx+1, yy,   mip, s, t, false, 2);
            if(yy<=vyy && inner && !SOLID(v) && (!SOLID(s) || v->type!=CORNER)
                && (v->type!=SEMISOLID || issemi(mip, xx, yy+1, 0, 0, 1, 0)))
                render_wall(s, v, xx,   yy+1, xx+1, yy+1, mip, v, u, true, 3);
        }
    }}
}

void distlod(int &low, int &high, int angle, float widef)
{
    float f = 90.0f/lod/widef;
    low = (int)((90-angle)/f);
    high = (int)(angle/f);
    if(low<min_lod) low = min_lod;
    if(high<min_lod) high = min_lod;
}

// does some out of date view frustrum optimisation that doesn't contribute much anymore

void render_world(float vx, float vy, float vh, float changelod, int yaw, int pitch, float fov, float fovy, int w, int h)
{
    loopi(LARGEST_FACTOR) stats[i] = 0;
    min_lod = minimap || (player1->isspectating() && player1->spectatemode == SM_OVERVIEW) ? MAX_LOD : MIN_LOD+abs(pitch)/12;
    yaw = 360-yaw;
    float widef = fov/75.0f;
    int cdist = iabs(yaw%90-45);
    if(cdist<7)    // hack to avoid popup at high fovs at 45 yaw
    {
        min_lod = max(min_lod, (int)(MIN_LOD+(10-cdist)/1.0f*widef)); // less if lod worked better
        widef = 1.0f;
    }
    lod = (int)(lod*changelod);
    if(lod<minlod) lod = minlod;
    if(lod>MAX_LOD) lod = MAX_LOD;
    lodtop = lodbot = lodleft = lodright = min_lod;
    if(yaw>45 && yaw<=135)
    {
        lodleft = lod;
        distlod(lodtop, lodbot, yaw-45, widef);
    }
    else if(yaw>135 && yaw<=225)
    {
        lodbot = lod;
        distlod(lodleft, lodright, yaw-135, widef);
    }
    else if(yaw>225 && yaw<=315)
    {
        lodright = lod;
        distlod(lodbot, lodtop, yaw-225, widef);
    }
    else
    {
        lodtop = lod;
        distlod(lodright, lodleft, yaw<=45 ? yaw+45 : yaw-315, widef);
    }
    render_floor = pitch<0.5f*fovy;
    render_ceil  = -pitch<0.5f*fovy;

    render_seg_new(vx, vy, vh, MAX_MIP, 0, 0, ssize>>MAX_MIP, ssize>>MAX_MIP);
    mipstats(stats[0], stats[1], stats[2]);
}

