// rendercubes.cpp: sits in between worldrender.cpp and rendergl.cpp and fills the vertex array for different cube surfaces.

#include "cube.h"

vector<vertex> verts;

void finishstrips();

void setupstrips()
{
    finishstrips();

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    vertex *buf = verts.getbuf();
    glVertexPointer(3, GL_FLOAT, sizeof(vertex), &buf->x);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(vertex), &buf->r);
    glTexCoordPointer(2, GL_FLOAT, sizeof(vertex), &buf->u);
}

struct strips { vector<GLint> first; vector<GLsizei> count; };

struct stripbatch
{
    int tex;
    strips tris, tristrips, quads;
};

stripbatch skystrips = { DEFAULT_SKY };
stripbatch stripbatches[256];
uchar renderedtex[256];
int renderedtexs = 0;

extern int ati_mda_bug;

#define RENDERSTRIPS(strips, type) \
    if(strips.first.length()) \
    { \
        if(hasMDA && !ati_mda_bug) glMultiDrawArrays_(type, strips.first.getbuf(), strips.count.getbuf(), strips.first.length()); \
        else loopv(strips.first) glDrawArrays(type, strips.first[i], strips.count[i]); \
        strips.first.setsize(0); \
        strips.count.setsize(0); \
    }

void renderstripssky()
{
    if(skystrips.tris.first.empty() && skystrips.tristrips.first.empty() && skystrips.quads.first.empty()) return;
    glDisable(GL_TEXTURE_2D);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    RENDERSTRIPS(skystrips.tris, GL_TRIANGLES);
    RENDERSTRIPS(skystrips.tristrips, GL_TRIANGLE_STRIP);
    RENDERSTRIPS(skystrips.quads, GL_QUADS);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_TEXTURE_2D);
}

void renderstrips()
{
    loopj(renderedtexs)
    {
        stripbatch &sb = stripbatches[j];
        glBindTexture(GL_TEXTURE_2D, lookupworldtexture(sb.tex)->id);
        RENDERSTRIPS(sb.tris, GL_TRIANGLES);
        RENDERSTRIPS(sb.tristrips, GL_TRIANGLE_STRIP);
        RENDERSTRIPS(sb.quads, GL_QUADS);
    }
    renderedtexs = 0;

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void addstrip(int type, int tex, int start, int n)
{
    stripbatch *sb = NULL;
    if(tex==DEFAULT_SKY)
    {
        if(minimap) return;
        sb = &skystrips;
        loopi(n) skyfloor = min(skyfloor, verts[start + i].z);
    }
    else
    {
        sb = &stripbatches[renderedtex[tex]];
        if(sb->tex!=tex || sb>=&stripbatches[renderedtexs])
        {
            sb = &stripbatches[renderedtex[tex] = renderedtexs++];
            sb->tex = tex;
        }
    }
    strips &s = (type==GL_QUADS ? sb->quads : (type==GL_TRIANGLES ? sb->tris : sb->tristrips));
    if(type!=GL_TRIANGLE_STRIP && s.first.length() && s.first.last()+s.count.last() == start)
    {
        s.count.last() += n;
        return;
    }
    s.first.add(start);
    s.count.add(n);
}

// generating the actual vertices is done dynamically every frame and sits at the
// leaves of all these functions, and are part of the cpu bottleneck on really slow
// machines, hence the macros.

#define vert(v1, v2, v3, ls, t1, t2) \
{ \
    vertex &v = verts.add(); \
    v.u = t1; v.v = t2; \
    v.x = (float)(v1); v.y = (float)(v2); v.z = (float)(v3); \
    v.r = ls->r; v.g = ls->g; v.b = ls->b; v.a = 255; \
}

enum
{
    STRIP_FLOOR = 1,
    STRIP_DELTA,
    STRIP_WALL
};

int nquads;

// for testing purpose. UNDOME on release.
const float texturescale = 32.0f;
//VARP(texturescale, 16, 32, 64);

#define TEXTURESCALE (float(texturescale) * ((uniformtexres && t->scale>1.0f) ? 1.0f : t->scale))

int striptype = 0, striptex, oh, oy, ox, odir;                         // the o* vars are used by the stripification
int ol1r, ol1g, ol1b, ol2r, ol2g, ol2b;
float ofloor, oceil;
bool ohf;
int firstindex;
bool showm = false;

void showmip() { showm = !showm; }
void mipstats(int a, int b, int c) { if(showm) conoutf("1x1/2x2/4x4: %d / %d / %d", a, b, c); }

COMMAND(showmip, "");

VAR(mergestrips, 0, 1, 1);

#define stripend(verts) \
    if(striptype) \
    { \
        int type = GL_TRIANGLE_STRIP, len = verts.length()-firstindex; \
        if(mergestrips) switch(len) \
        { \
            case 3: type = GL_TRIANGLES; break; \
            case 4: type = GL_QUADS; swap(verts.last(), verts[verts.length()-2]); break; \
        } \
        addstrip(type, striptex, firstindex, len); \
        striptype = 0; \
    }

void finishstrips() { stripend(verts); }

sqr sbright, sdark;
VARP(lighterror, 1, 4, 100);

void render_flat(int wtex, int x, int y, int size, int h, sqr *l1, sqr *l4, sqr *l3, sqr *l2, bool isceil)  // floor/ceil quads
{
    if(showm) { l3 = l1 = &sbright; l4 = l2 = &sdark; }

    Texture *t = lookupworldtexture(wtex);
    float xf = TEXTURESCALE/t->xs;
    float yf = TEXTURESCALE/t->ys;
    float xs = size*xf;
    float ys = size*yf;
    float xo = xf*x;
    float yo = yf*y;

    bool first = striptype!=STRIP_FLOOR || x!=ox+size || striptex!=wtex || h!=oh || y!=oy;

    if(first)       // start strip here
    {
        stripend(verts);
        firstindex = verts.length();
        striptex = wtex;
        oh = h;
        oy = y;
        striptype = STRIP_FLOOR;
        if(isceil)
        {
            vert(x, y,      h, l1, xo, yo);
            vert(x, y+size, h, l2, xo, yo+ys);
        }
        else
        {
            vert(x, y+size, h, l2, xo, yo+ys);
            vert(x, y,      h, l1, xo, yo);
        }
        ol1r = l1->r;
        ol1g = l1->g;
        ol1b = l1->b;
        ol2r = l2->r;
        ol2g = l2->g;
        ol2b = l2->b;
    }
    else        // continue strip
    {
        int lighterr = lighterror*2;
        if((iabs(ol1r-l3->r)<lighterr && iabs(ol2r-l4->r)<lighterr        // skip vertices if light values are close enough
        &&  iabs(ol1g-l3->g)<lighterr && iabs(ol2g-l4->g)<lighterr
        &&  iabs(ol1b-l3->b)<lighterr && iabs(ol2b-l4->b)<lighterr) || !wtex)
        {
            verts.setsize(verts.length()-2);
            nquads--;
        }
        else
        {
            uchar *p1 = (uchar *)(&verts[verts.length()-1].r);
            ol1r = p1[0];
            ol1g = p1[1];
            ol1b = p1[2];
            uchar *p2 = (uchar *)(&verts[verts.length()-2].r);
            ol2r = p2[0];
            ol2g = p2[1];
            ol2b = p2[2];
        }
    }

    if(isceil)
    {
        vert(x+size, y,      h, l4, xo+xs, yo);
        vert(x+size, y+size, h, l3, xo+xs, yo+ys);
    }
    else
    {
        vert(x+size, y+size, h, l3, xo+xs, yo+ys);
        vert(x+size, y,      h, l4, xo+xs, yo);
    }

    ox = x;
    nquads++;
}

void render_flatdelta(int wtex, int x, int y, int size, float h1, float h4, float h3, float h2, sqr *l1, sqr *l4, sqr *l3, sqr *l2, bool isceil)  // floor/ceil quads on a slope
{
    if(showm) { l3 = l1 = &sbright; l4 = l2 = &sdark; }

    Texture *t = lookupworldtexture(wtex);
    float xf = TEXTURESCALE/t->xs;
    float yf = TEXTURESCALE/t->ys;
    float xs = size*xf;
    float ys = size*yf;
    float xo = xf*x;
    float yo = yf*y;

    bool first = striptype!=STRIP_DELTA || x!=ox+size || striptex!=wtex || y!=oy;

    if(first)
    {
        stripend(verts);
        firstindex = verts.length();
        striptex = wtex;
        oy = y;
        striptype = STRIP_DELTA;
        if(isceil)
        {
            vert(x, y,      h1, l1, xo, yo);
            vert(x, y+size, h2, l2, xo, yo+ys);
        }
        else
        {
            vert(x, y+size, h2, l2, xo, yo+ys);
            vert(x, y,      h1, l1, xo, yo);
        }
    }

    if(isceil)
    {
        vert(x+size, y,      h4, l4, xo+xs, yo);
        vert(x+size, y+size, h3, l3, xo+xs, yo+ys);
    }
    else
    {
        vert(x+size, y+size, h3, l3, xo+xs, yo+ys);
        vert(x+size, y,      h4, l4, xo+xs, yo);
    }

    ox = x;
    nquads++;
}

void render_2tris(sqr *h, sqr *s, int x1, int y1, int x2, int y2, int x3, int y3, sqr *l1, sqr *l2, sqr *l3)   // floor/ceil tris on a corner cube
{
    stripend(verts);

    Texture *t = lookupworldtexture(h->ftex);
    float xf = TEXTURESCALE/t->xs;
    float yf = TEXTURESCALE/t->ys;
    vert(x1, y1, h->floor, l1, xf*x1, yf*y1);
    vert(x2, y2, h->floor, l2, xf*x2, yf*y2);
    vert(x3, y3, h->floor, l3, xf*x3, yf*y3);
    addstrip(mergestrips ? GL_TRIANGLES : GL_TRIANGLE_STRIP, h->ftex, verts.length()-3, 3);

    t = lookupworldtexture(h->ctex);
    xf = TEXTURESCALE/t->xs;
    yf = TEXTURESCALE/t->ys;

    vert(x3, y3, h->ceil, l3, xf*x3, yf*y3);
    vert(x2, y2, h->ceil, l2, xf*x2, yf*y2);
    vert(x1, y1, h->ceil, l1, xf*x1, yf*y1);
    addstrip(mergestrips ? GL_TRIANGLES : GL_TRIANGLE_STRIP, h->ctex, verts.length()-3, 3);
    nquads++;
}

void render_tris(int x, int y, int size, bool topleft,
                 sqr *h1, sqr *h2, sqr *s, sqr *t, sqr *u, sqr *v)
{
    if(topleft)
    {
        if(h1) render_2tris(h1, s, x+size, y+size, x, y+size, x, y, u, v, s);
        if(h2) render_2tris(h2, s, x, y, x+size, y, x+size, y+size, s, t, v);
    }
    else
    {
        if(h1) render_2tris(h1, s, x, y, x+size, y, x, y+size, s, t, u);
        if(h2) render_2tris(h2, s, x+size, y, x+size, y+size, x, y+size, t, u, v);
    }
}

void render_square(int wtex, float floor1, float floor2, float ceil1, float ceil2, int x1, int y1, int x2, int y2, int size, sqr *l1, sqr *l2, bool flip, int dir)   // wall quads
{
    if(showm) { l1 = &sbright; l2 = &sdark; }

    Texture *t = lookupworldtexture(wtex);
    float xf = TEXTURESCALE/t->xs;
    float yf = -TEXTURESCALE/t->ys;
    float xs = size*xf;
    float xo = xf*(x1==x2 ? min(y1,y2) : min(x1,x2));

    bool first = striptype!=STRIP_WALL || striptex!=wtex || ox!=x1 || oy!=y1 || ofloor!=floor1 || oceil!=ceil1 || odir!=dir,
         hf = floor1!=floor2 || ceil1!=ceil2;

    if(first)
    {
        stripend(verts);
        firstindex = verts.length();
        striptex = wtex;
        striptype = STRIP_WALL;

        if(!flip)
        {
            vert(x1, y1, ceil1,  l1, xo, yf*ceil1);
            vert(x1, y1, floor1, l1, xo, yf*floor1);
        }
        else
        {
            vert(x1, y1, floor1, l1, xo, yf*floor1);
            vert(x1, y1, ceil1,  l1, xo, yf*ceil1);
        }
        ol1r = l1->r;
        ol1g = l1->g;
        ol1b = l1->b;
    }
    else        // continue strip
    {
        int lighterr = lighterror*2;
        if((!hf && !ohf)
        && ((iabs(ol1r-l2->r)<lighterr && iabs(ol1g-l2->g)<lighterr && iabs(ol1b-l2->b)<lighterr) || !wtex))       // skip vertices if light values are close enough
        {
            verts.setsize(verts.length()-2);
            nquads--;
        }
        else
        {
            uchar *p1 = (uchar *)(&verts[verts.length()-1].r);
            ol1r = p1[0];
            ol1g = p1[1];
            ol1b = p1[2];
        }
    }

    if(!flip)
    {
        vert(x2, y2, ceil2,  l2, xo+xs, yf*ceil2);
        vert(x2, y2, floor2, l2, xo+xs, yf*floor2);
    }
    else
    {
        vert(x2, y2, floor2, l2, xo+xs, yf*floor2);
        vert(x2, y2, ceil2,  l2, xo+xs, yf*ceil2);
    }

    ox = x2;
    oy = y2;
    ofloor = floor2;
    oceil = ceil2;
    odir = dir;
    ohf = hf;
    nquads++;
}

void resetcubes()
{
    verts.setsize(0);

    striptype = 0;
    nquads = 0;

    sbright.r = sbright.g = sbright.b = 255;
    sdark.r = sdark.g = sdark.b = 0;

    resetwater();
}

struct shadowvertex { float u, v, x, y, z; };
vector<shadowvertex> shadowverts;

static void resetshadowverts()
{
    shadowverts.setsize(0);

    striptype = 0;
}

static void rendershadowstrips()
{
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    shadowvertex *buf = shadowverts.getbuf();
    glVertexPointer(3, GL_FLOAT, sizeof(shadowvertex), &buf->x);
    glTexCoordPointer(2, GL_FLOAT, sizeof(shadowvertex), &buf->u);

    loopj(renderedtexs)
    {
        stripbatch &sb = stripbatches[j];
        RENDERSTRIPS(sb.tris, GL_TRIANGLES);
        RENDERSTRIPS(sb.tristrips, GL_TRIANGLE_STRIP);
        RENDERSTRIPS(sb.quads, GL_QUADS);
    }
    renderedtexs = 0;

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    xtraverts += shadowverts.length();
}

#define shadowvert(v1, v2, v3) \
{ \
    shadowvertex &v = shadowverts.add(); \
    v.x = (float)(v1); v.y = (float)(v2); v.z = (float)(v3); \
}

void rendershadow_tri(sqr *h, int x1, int y1, int x2, int y2, int x3, int y3)   // floor tris on a corner cube
{
    stripend(shadowverts);

    shadowvert(x1, y1, h->floor);
    shadowvert(x2, y2, h->floor);
    shadowvert(x3, y3, h->floor);
    addstrip(mergestrips ? GL_TRIANGLES : GL_TRIANGLE_STRIP, DEFAULT_FLOOR, shadowverts.length()-3, 3);
}

void rendershadow_tris(int x, int y, bool topleft, sqr *h1, sqr *h2)
{
    if(topleft)
    {
        if(h1) rendershadow_tri(h1, x+1, y+1, x, y+1, x, y);
        if(h2) rendershadow_tri(h2, x, y, x+1, y, x+1, y+1);
    }
    else
    {
        if(h1) rendershadow_tri(h1, x, y, x+1, y, x, y+1);
        if(h2) rendershadow_tri(h2, x+1, y, x+1, y+1, x, y+1);
    }
}

static void rendershadow_flat(int x, int y, int h) // floor quads
{
    bool first = striptype!=STRIP_FLOOR || x!=ox+1 || h!=oh || y!=oy;

    if(first)       // start strip here
    {
        stripend(shadowverts);
        firstindex = shadowverts.length();
        striptex = DEFAULT_FLOOR;
        oh = h;
        oy = y;
        striptype = STRIP_FLOOR;
        shadowvert(x, y+1, h);
        shadowvert(x, y,   h);
    }
    else        // continue strip
    {
        shadowverts.setsize(shadowverts.length()-2);
    }

    shadowvert(x+1, y+1, h);
    shadowvert(x+1, y,   h);

    ox = x;
}

static void rendershadow_flatdelta(int x, int y, float h1, float h4, float h3, float h2)  // floor quads on a slope
{
    bool first = striptype!=STRIP_DELTA || x!=ox+1 || y!=oy;

    if(first)
    {
        stripend(shadowverts);
        firstindex = shadowverts.length();
        striptex = DEFAULT_FLOOR;
        oy = y;
        striptype = STRIP_DELTA;
        shadowvert(x, y+1, h2);
        shadowvert(x, y,   h1);
    }

    shadowvert(x+1, y+1, h3);
    shadowvert(x+1, y,   h4);

    ox = x;
}

void rendershadow(int x, int y, int xs, int ys, const vec &texgenS, const vec &texgenT)
{
    x = max(x, 1);
    y = max(y, 1);
    xs = min(xs, ssize-1);
    ys = min(ys, ssize-1);

    resetshadowverts();

    #define df(x) s->floor-(x->vdelta/4.0f)

    sqr *w = wmip[0];
    for(int yy = y; yy<ys; yy++) for(int xx = x; xx<xs; xx++)
    {
        sqr *s = SW(w,xx,yy);
        if(s->type==SPACE || s->type==CHF)
        {
            rendershadow_flat(xx, yy, s->floor);
        }
        else if(s->type==FHF)
        {
            sqr *t = SW(s,1,0), *u = SW(s,1,1), *v = SW(s,0,1);
            rendershadow_flatdelta(xx, yy, df(s), df(t), df(u), df(v));
        }
        else if(s->type==CORNER)
        {
            sqr *t = SW(s,1,0), *v = SW(s,0,1), *w = SW(s,0,-1), *z = SW(s,-1,0);
            bool topleft = true;
            sqr *h1 = NULL, *h2 = NULL;
            if(SOLID(z))
            {
                if(SOLID(w))      { h2 = s; topleft = false; }
                else if(SOLID(v)) { h2 = s; }
            }
            else if(SOLID(t))
            {
                if(SOLID(w))      { h1 = s; }
                else if(SOLID(v)) { h1 = s; topleft = false; }
            }
            else
            {
                bool wv = w->ceil-w->floor < v->ceil-v->floor;
                if(z->ceil-z->floor < t->ceil-t->floor)
                {
                    if(wv) { h1 = s; h2 = v; topleft = false; }
                    else   { h1 = s; h2 = w; }
                }
                else
                {
                    if(wv) { h2 = s; h1 = v; }
                    else   { h2 = s; h1 = w; topleft = false; }
                }
            }
            rendershadow_tris(xx, yy, topleft, h1, h2);
        }
    }

    stripend(shadowverts);

    for(shadowvertex *v = shadowverts.getbuf(), *end = &v[shadowverts.length()]; v < end; v++)
    {
        float vx = v->x, vy = v->y;
        v->u = vx*texgenS.x + vy*texgenS.y + texgenS.z;
        v->v = vx*texgenT.x + vy*texgenT.y + texgenT.z;
    }

    rendershadowstrips();
}

