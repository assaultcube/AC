
#include "cube.h"
#include "tools.h"

VARP(usenewworldrenderer, 0, 0, 1);

//TODO ogl functions need to go to a better place.
PFNGLBINDBUFFERPROC glBindBuffer = NULL;
PFNGLBUFFERDATAPROC glBufferData = NULL;
PFNGLGENBUFFERSPROC glGenBuffers = NULL;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = NULL;

#define GETPROCADDR(type, name) reinterpret_cast<type>(SDL_GL_GetProcAddress(name))

bool glprocsloaded = false;

/**
 * @brief set up pointers to OpenGL functions if necessary.
 */
bool loadglprocs()
{
    if(glprocsloaded) return true;
    glBindBuffer = GETPROCADDR(PFNGLBINDBUFFERPROC, "glBindBuffer");
    glBufferData = GETPROCADDR(PFNGLBUFFERDATAPROC, "glBufferData");
    glGenBuffers = GETPROCADDR(PFNGLGENBUFFERSPROC, "glGenBuffers");
    glDeleteBuffers = GETPROCADDR(PFNGLDELETEBUFFERSPROC, "glDeleteBuffers");
    return glprocsloaded = glBindBuffer && glBufferData && glGenBuffers && glDeleteBuffers;
}

#define DEBUGCOND (1) // TODO allow debugging to be turned off

enum { FLAT = 0, WALL_LEFTTORIGHT, WALL_TOPTOBOTTOM };

struct vertexpoint
{
    ushort x;
    ushort y;
    short height; // quarter-cube units to accommodate vdeltas
    short wall;

    vertexpoint(int x=0, int y=0, int h=0, int wall=FLAT):
        x(x), y(y), height(h), wall(wall)
    {  }

    bool operator==(const vertexpoint &o) const
    {
        return x == o.x && y == o.y && height == o.height && (!wall == !o.wall);
    }

};

// TODO this hash looks shit, steal one from somewhere...
uint hthash(const vertexpoint &vp)
{
    uint x = vp.x, y = vp.y, h = vp.height, c = vp.wall;
    uint hash = x | y << 12 | h << 24;
    hash *= (1 + vp.wall);
    return hash;
}

bool htcmp(const vertexpoint &vp1, const vertexpoint &vp2)
{
    return vp1 == vp2;
}

#define VERTCOLOURS(v, s) { (v).r = (s).r; (v).g = (s).g; (v).b = (s).b; (v).a = 255; }


#define GLBUFOFF(n) static_cast<GLvoid *>(static_cast<char *>(0) + (n))

int checkglerrors();

int intcmpf(int *i1, int *i2)
{
    if(*i1 < *i2) return -1;
    if(*i1 > *i2) return 1;
    return 0;
}


int intcmpfr(int *i1, int *i2)
{
    if(*i1 > *i2) return -1;
    if(*i1 < *i2) return 1;
    return 0;
}

struct texbatch;
double blockdist(int);

/**
 * @brief A sequence of adjacent render blocks that can be rendered in one go.
 */
struct visibleblockspan
{

    visibleblockspan(int start=-1);

    /// First render block of this span
    int start;

    /// Number of blocks spanned
    int count;

    /// Block closest to the current player (measured from centre of the block)
    int nearestblock;

    /// Distance to the nearest block
    float nearestdist;

    /**
     * @brief Update nearest render block and distance if necessary
     */
    void updatenearest(int block)
    {
        float dist = blockdist(block);
        if(dist < nearestdist)
        {
            nearestblock = block;
            nearestdist = dist;
        }
    }

    /**
     * @brief Find the render block in this span closest to the camera
     */
    void findnearest()
    {
        for(int i = start, end = start+count; i < end; ++i)
        {
            updatenearest(i);
        }
    }

    /**
     * @brief Get the number of vertices in the given batch in this span.
     */
    int vertcount(texbatch const* t);

};


visibleblockspan::visibleblockspan(int start) :
    start(start), count(1), nearestblock(-1), nearestdist(1e16)
{
    if(start >= 0)
    {
        updatenearest(start);
    }
}

/**
 * @brief Compare visible blocks, sorting nearest first.
 */
int nearestvisibleblockspancmp(visibleblockspan *s1, visibleblockspan *s2)
{
    if(s1->nearestdist > s2->nearestdist) return 1;
    if(s1->nearestdist < s2->nearestdist) return -1;
    return 0;
}

vector<int> visibleblocks;
vector<visibleblockspan> visibleblockspans;

int wdrawcalls = 0; /// Number of draw calls made to render world geometry

DEBUGCODE(VAR(cyclevbs, 0, 0, 1));

class texbatch
{

    // Private list of visible block spans
    vector<visibleblockspan> spans;

public:

    GLuint vertexbo;
    GLuint elembo;
    int elemtype;
    int tex;
    int elemcount;
    vector<int> blockstarts;


    texbatch() : vertexbo(0), elembo(0), elemtype(0), tex(0), elemcount(0) {}

    void deinit()
    {
        if(vertexbo) glDeleteBuffers(1, &vertexbo);
        if(elembo) glDeleteBuffers(1, &elembo);
        vertexbo = 0;
        elembo = 0;
        blockstarts.setsize(0);
    }

    ~texbatch()
    {
        if(vertexbo) glDeleteBuffers(1, &vertexbo);
        if(elembo) glDeleteBuffers(1, &elembo);
    }

    void init(int tex)
    {
        this->tex = tex;
        if(!vertexbo) glGenBuffers(1, &vertexbo);
        if(!elembo) glGenBuffers(1, &elembo);
    }

    void bind_buffers() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, vertexbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elembo);
    }

    void pre() const
    {
        bind_buffers();
        glVertexPointer(3, GL_FLOAT, sizeof(vertex), GLBUFOFF(offsetof(vertex, x)));
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(vertex), GLBUFOFF(offsetof(vertex, r)));
        if(tex)
        {
            glTexCoordPointer(2, GL_FLOAT, sizeof(vertex), GLBUFOFF(offsetof(vertex, u)));
            glBindTexture(GL_TEXTURE_2D, lookupworldtexture(tex)->id);
        }
    }

    int blocklen(int i) const
    {
        return blockstarts[i+1] - blockstarts[i];
    }

    int visibleblocklen(int i) const
    {
        return blocklen(visibleblocks[i]);
    }

    int rangelen(int first, int last) const
    {
        return blockstarts[last] - blockstarts[first];
    }

    int indexsize() const
    {
        return (elemtype == GL_UNSIGNED_INT) ? 4 : 2;
    }

    void drawall() const
    {
        glDrawElements(GL_TRIANGLES, elemcount, elemtype, 0);
        wdrawcalls += 1;
    }

    /**
     * @brief Render all geometry in visible blocks.
     */
    void batchdraw()
    {
        int spancount = visibleblockspans.length();
        bool ready = false;

        spans = visibleblockspans;
        spans.sort<visibleblockspan>(nearestvisibleblockspancmp);
        
        DEBUGCODE(int curspan = (totalmillis / 1000) % spancount);
        
        loopi(spancount)
        {
            DEBUGCODE(if(cyclevbs && (i != curspan)) continue);
            visibleblockspan *span = &spans[i];
            int len = span->vertcount(this);
            if(!len) continue;
            if(!ready)
            {
                pre(); // Only bind buffers if there's something to render
                if(elemcount < 200)
                {
                    // For small numbers of verts, just render everything
                    // in one go and be done with it. Saves a few drawcalls.
                    drawall();
                    return;
                }
                ready = true;
            }
            glDrawElements(GL_TRIANGLES, len, elemtype, GLBUFOFF(blockstarts[span->start]*indexsize()));
            wdrawcalls += 1;
        }
    }

};

int visibleblockspan::vertcount(const texbatch* t)
{
    return t->rangelen(start, start+count);
}


struct coord2d
{
    int x, y;
    //coord2d(int x=0, int y=0) : x(x), y(y) {}
};

enum { TOP = 0, RIGHT, BOTTOM, LEFT, NUM_SIDES };
const coord2d NEIGHBOUR_OFFSETS[] = { {0, -1}, {1, 0}, {0, 1}, {-1, 0} };



int opposingside(int side)
{
    switch(side)
    {
        case TOP: return BOTTOM;
        case RIGHT: return LEFT;
        case BOTTOM: return TOP;
        case LEFT: return RIGHT;
    }
    return -1;
}

int modulo(int a, int b)
{
    while(a < 0) a += b;
    return a % b;
}

/**
 * @brief Return the next side in the given direction.
 *
 * Clockwise by default.
 */
int nextside(int side, bool ccw=false)
{
    return modulo(side + (ccw?-1:1), NUM_SIDES);
}

enum { TOP_LEFT = 0, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT, NUM_CORNERS };
const coord2d CORNER_OFFSETS[] = { {0, 0}, {1, 0}, {0, 1}, {1, 1} };


// Set x and y to the corner offsets from the topleft corner of the cube
#define COFFSET(c, x, y) { x = (c)&1; y = ((c)&2) >> 1; }

// Check if corner is along the Left, Right, Top, Bottom edges.
#define CL(c) (((c)&1) == 0)
#define CR(c) (((c)&1) == 1)
#define CT(c) (((c)&2) == 0)
#define CB(c) (((c)&2) == 2)

/// Check if the two corners form a diagonal
#define CDIAG(c1, c2) ((CL(c1) != CL(c2)) && (CT(c1) != CT(c2)))

void corner_offsets(int c, int &x, int &y)
{
    x = c&1;
    y = (c&2) >> 1;
}

int nextcorner(int c, bool ccw=false)
{
    static int order[4] = { TOP_LEFT, TOP_RIGHT, BOTTOM_RIGHT, BOTTOM_LEFT };
    c %= NUM_CORNERS;
    int idx = -1;
    loopi(NUM_CORNERS) if(order[i] == c) { idx = i; break; }
    ASSERT(idx != -1);
    return order[modulo(idx + (ccw ? -1 : 1), NUM_CORNERS)];
}

int opposingcorner(int c)
{
    //DEBUG(c << ':' << (CT(c)?2:0) | (CL(c)?1:0));
    return (CT(c)?2:0) | (CL(c)?1:0);
}

/**
 * @brief Get the two corners that hug the given side.
 */
void sidecorners(int side, int *corners, bool flip=false)
{
    switch(side)
    {
    case TOP:
        corners[0] = TOP_RIGHT;
        corners[1] = TOP_LEFT;
        break;
    case RIGHT:
        corners[0] = BOTTOM_RIGHT;
        corners[1] = TOP_RIGHT;
        break;
    case BOTTOM:
        corners[0] = BOTTOM_LEFT;
        corners[1] = BOTTOM_RIGHT;
        break;
    case LEFT:
        corners[0] = TOP_LEFT;
        corners[1] = BOTTOM_LEFT;
    }
    if(flip)
    {
        swap(corners[0], corners[1]);
    }
}

/**
 * @brief Get the cube adjacent to the cube at (x,y) along the given side.
 */
sqr *adjcube(int x, int y, int side, int stride=1)
{
    coord2d o = NEIGHBOUR_OFFSETS[side];
    return S(x + stride*o.x, y + stride*o.y);
}

/**
 * @brief Get the corner that joints the two sides.
 *
 * @returns the corner common to the given sides, or -1 if non-neighbouring
 */
int commoncorner(int side1, int side2)
{
    int side1corners[2], side2corners[2];
    sidecorners(side1, side1corners);
    sidecorners(side2, side2corners);
    loopi(2) loopk(2) if(side1corners[i] == side2corners[k]) return side1corners[i];
    return -1;
}

/**
 * @brief Find the vdeltas for the cube at (x, y).
 *
 * Returns 0 if the given surface (floor/ceil) is not a heightfield.
 *
 * (xo,yo) are corners, (0,0) for topleft, (1,1) for bottomright.
 *
 */
int cornervdelta(int x, int y, int xo, int yo, bool ceil)
{
    int type = S(x,y)->type;
    int vdelta = S(x+xo,y+yo)->vdelta;
    switch(type)
    {
        case FHF: return !ceil ? -vdelta : 0;
        case CHF: return ceil ? vdelta : 0;
        default: return 0;
    }
}

int cornervdelta(int x, int y, int corner, bool ceil)
{
    coord2d o = CORNER_OFFSETS[corner];
    return cornervdelta(x, y, o.x, o.y, ceil);
}

int cornerheight(int x, int y, int xo, int yo, bool ceil)
{
    sqr const *s = S(x,y);
    return 4*(ceil ? s->ceil : s->floor) + cornervdelta(x, y, xo, yo, ceil);
}

int cornerheight(int x, int y, int corner, bool ceil)
{
    coord2d o = CORNER_OFFSETS[corner];
    return cornerheight(x, y, o.x, o.y, ceil);
}

// The order in which floor/corner vertices are to be drawn for a triangle pair
const int FCORNERX[] = { 0, 1, 0, 1, 1, 0 };
const int FCORNERY[] = { 0, 0, 1, 0, 1, 1 };
const int CCORNERX[] = { 0, 0, 1, 0, 1, 1 };
const int CCORNERY[] = { 0, 1, 0, 1, 1, 0 };

typedef unsigned int arrayindex;

/**
 * Helper class for constructing the world mesh.
 *
 * Not used for actual rendering, but generating the VBOs.
 */
struct worldmesh
{

    hashtable<vertexpoint, int> elemindexmap[256];
    vector<vertex> verts[256];
    vector<arrayindex> vertindices[256];

    int vertex_index(int x, int y, int h, int tex, int xo, int yo, int wall=FLAT)
    {
        int ax = x + xo, ay = y + yo;
        vertexpoint vp(ax, ay, h, wall);
        int *idx = elemindexmap[tex].access(vp);
        if(idx) return *idx;
        vector<vertex> *vs = verts + tex;
        int newidx = vs->length();

        Texture const *texture = lookupworldtexture(tex);
        float texscale = 32.0f * ((uniformtexres && texture->scale>1.0f) ? 1.0f : texture->scale);
        float xs = texscale / texture->xs;
        float ys = texscale / texture->ys;

        vertex v;
        v.x = ax;
        v.y = ay;
        v.z = h * 0.25f;
        if(wall)
        {
            v.u = (wall == WALL_LEFTTORIGHT ? ay : ax) * xs;
            v.v = -v.z * ys;
        }
        else
        {
            v.u = xs * ax;
            v.v = ys * ay;
        }
        VERTCOLOURS(v, *S(ax, ay));

        vs->add(v);
        return elemindexmap[tex].access(vp, newidx);
    }

    bool hasdata() const
    {
        loopi(256) if(verts[i].length()) return true;
        return false;
    }

    void addvert(int x, int y, int h, int tex, int xo=0, int yo=0, int wall=FLAT)
    {
        vertindices[tex].add(vertex_index(x, y, h, tex, xo, yo, wall));
    }

    void flattri(int x, int y, int h, int tex, int corner, bool ceil, int size=1)
    {
        int corners[3];
        corners[0] = nextcorner(corner, !ceil);
        loopi(2) corners[i+1] = nextcorner(corners[i], ceil);

        loopi(3)
        {
            int xo, yo;
            corner_offsets(corners[i], xo, yo);
            addvert(x+xo*size, y+yo*size, h, tex);
        }
        //int xo, yo;
        //corner_offsets(corner, xo, yo);



        /*
        switch(corner)
        {
            case TOP_LEFT:
                addvert(x,      y,      h, tex);
                addvert(x+size, y,      h, tex);
                addvert(x,      y+size, h, tex);
                break;
            case TOP_RIGHT:
                addvert(x+size, y,      h, tex);
                addvert(x+size, y+size, h, tex);
                addvert(x,      y+size, h, tex);
                break;
            case BOTTOM_LEFT:
                addvert(x,      y+size, h, tex);
                addvert(x+size, y,      h, tex);
                addvert(x+size, y+size, h, tex);
                break;
            case BOTTOM_RIGHT:
                addvert(x+size, y+size, h, tex);
                addvert(x,      y+size, h, tex);
                addvert(x+size, y,      h, tex);
                break;
        }
        if(ceil)
        {
            int i = vertindices[tex].length() - 1;
            swap(vertindices[tex][i], vertindices[tex][i-1]);
        }
        */
    }

    /**
     * @brief Add a wall face.
     *
     * x,y coordinates of the cube this wall belongs to
     * bh1, bh2 bottom height (elevation) of the bottom of the wall
     * uh1, uh2 upper height of the top of the wall
     * corner1, corner2 corners this wall spans
     * tex texture id
     */
    void wallquad(int x, int y, int bh1, int bh2, int uh1, int uh2, int corner1, int corner2, int tex, int size=1)
    {

        int xo1, yo1, xo2, yo2;
        corner_offsets(corner1, xo1, yo1);
        corner_offsets(corner2, xo2, yo2);

        int wall = WALL_LEFTTORIGHT;
        if(xo1 != xo2) wall = WALL_TOPTOBOTTOM;

        xo1 *= size; yo1 *= size; xo2 *= size; yo2 *= size;

        addvert(x+xo1, y+yo1, uh1, tex, 0, 0, wall);
        addvert(x+xo2, y+yo2, uh2, tex, 0, 0, wall);
        addvert(x+xo1, y+yo1, bh1, tex, 0, 0, wall);
        addvert(x+xo1, y+yo1, bh1, tex, 0, 0, wall);
        addvert(x+xo2, y+yo2, uh2, tex, 0, 0, wall);
        addvert(x+xo2, y+yo2, bh2, tex, 0, 0, wall);

    }

    void cubewalls(int x, int y, bool ceil, bool skipcorners=true, int size=1)
    {
        sqr const *s = S(x, y), *o;
        if(skipcorners && s->type == CORNER) return;
        if(ceil && SOLID(s)) return;

        // corner heights - elevation (of the bottom) at the starting and
        // finishing corner
        int ch1, ch2;

        int corners[2];
        // TODO try to get rid of duplicated code in wall/floor sections
        if(!ceil)
        {
            loopi(NUM_SIDES)
            {
                sidecorners(i, corners);
                coord2d no = NEIGHBOUR_OFFSETS[i];
                int xo = x + no.x*size;
                int yo = y + no.y*size;
                if(OUTBORD(xo, yo) || OUTBORD(x, y)) continue;
                o = S(xo, yo);
                {
                    // Check if all the 1 x 1 cubes along the side of
                    // this size x size cube are solid.
                    // If so, no need to draw the wall on this side.
                    bool solidneighbour = true;
                    int xbeg = x-1, ybeg = y-1, xinc = 0, yinc = 0;
                    if(no.x) { ybeg = y; yinc = 1; if(no.x == 1) xbeg = xo; }
                    if(no.y) { xbeg = x; xinc = 1; if(no.y == 1) ybeg = yo; }
                    loopj(size) if(!SOLID(S(xbeg+j*xinc, ybeg+j*yinc))) { solidneighbour = false; break; }
                    if(solidneighbour) continue;
                }
                int ch1, ch2;
                ch1 = ch2 = 4*s->floor;
                if(!SOLID(s))
                {
                    ch1 = cornerheight(x, y, corners[0], ceil);
                    ch2 = cornerheight(x, y, corners[1], ceil);
                }
                else
                {
                    for(int yo = -1; yo <= size+1; ++yo) for(int xo = -1; xo <= size+1; ++xo)
                    {
                        if(SOLID(S(x+xo, y+yo))) continue;
                        loopk(NUM_CORNERS) ch1 = ch2 = max(ch1, cornerheight(x+xo, y+yo, k, true));
                    }
                }
                int lowheight = 4*min(s->floor, o->floor);
                if(s->type == FHF || o->type == FHF)
                {
                    for(int yo = -1; yo <= size+1; ++yo) for(int xo = -1; xo <= size+1; ++xo)
                    {
                        loopk(NUM_CORNERS) lowheight = min(lowheight, cornerheight(x+xo, y+yo, k, ceil));
                    }
                }

                {
                    // If the adjacent corners of this and the adjacent cube
                    // are at the same height, then there is obviously no wall
                    // between them.
                    int adjcorners[2];
                    sidecorners(opposingside(i), adjcorners, true);
                    if(ch1 == cornerheight(xo, yo, adjcorners[0], ceil)
                        && ch2 == cornerheight(xo, yo, adjcorners[1], ceil))
                    {
                        continue;
                    }
                }
                if((ch1 > lowheight || ch2 > lowheight))
                    wallquad(x, y, lowheight, lowheight, ch1, ch2, corners[0], corners[1], s->wtex, size);
            }
        }
        else
        {
            loopi(NUM_SIDES)
            {
                sidecorners(i, corners);
                coord2d no = NEIGHBOUR_OFFSETS[i];
                int xo = x + no.x*size;
                int yo = y + no.y*size;
                o = S(xo, yo);
                {
                    // Check if all the 1 x 1 cubes along the side of
                    // this size x size cube are solid.
                    // If so, no need to draw the wall on this side.
                    bool solidneighbour = true;
                    int xbeg = x-1, ybeg = y-1, xinc = 0, yinc = 0;
                    if(no.x) { ybeg = y; yinc = 1; if(no.x == 1) xbeg = xo; }
                    if(no.y) { xbeg = x; xinc = 1; if(no.y == 1) ybeg = yo; }
                    loopj(size) if(!SOLID(S(xbeg+j*xinc, ybeg+j*yinc))) { solidneighbour = false; break; }
                    if(solidneighbour) continue;
                }
                if(SOLID(o)) continue;
                ch1 = cornerheight(x, y, corners[0], ceil);
                ch2 = cornerheight(x, y, corners[1], ceil);
                int highheight = 4*max(s->ceil, o->ceil);
                if(s->type == CHF || o->type == CHF)
                {
                    for(int yo = -1; yo <= size+1; ++yo) for(int xo = -1; xo <= size+1; ++xo)
                    {
                        if(SOLID(S(x+xo, y+yo))) continue;
                        loopk(NUM_CORNERS)
                        {
                            highheight = max(highheight, cornerheight(x+xo, y+yo, k, ceil));
                        }
                    }
                }
                {
                    // If the adjacent corners of this and the adjacent cube
                    // are at the same height, then there is obviously no wall
                    // between them.
                    int adjcorners[2];
                    sidecorners(opposingside(i), adjcorners, true);
                    if(ch1 == cornerheight(xo, yo, adjcorners[0], ceil)
                        && ch2 == cornerheight(xo, yo, adjcorners[1], ceil))
                    {
                        continue;
                    }
                }
                if((ch1 < highheight || ch2 < highheight))
                    wallquad(x, y, ch1, ch2, highheight, highheight, corners[0], corners[1], s->utex, size);
            }

        }


    }

    void flat(int x, int y, bool ceil, int size=1)
    {
        sqr const *s = S(x, y);
        if(SOLID(s)) return;
        int tex = ceil ? s->ctex : s->ftex;

        int h = ceil ? s->ceil : s->floor;
        h *= 4;

        const int *cxs = FCORNERX;
        const int *cys = FCORNERY;
        if(ceil)
        {
            cxs = CCORNERX;
            cys = CCORNERY;
        }
        loopi(6) // 6 verts per flat face (2 triangles to make a quad)
        {
            int cx = cxs[i];
            int cy = cys[i];
            int xoff = cx * (size-1);
            int yoff = cy * (size-1);
            addvert(x+xoff, y+yoff, h + cornervdelta(x+xoff, y+yoff, cx, cy, ceil), tex, cx, cy);
            //addvert(x, y, h + cornervdelta(x, y, cx, cy, ceil), tex, cx*size, cy*size);
        }
    }

    vector<int> startindices[256];

    /**
     * @brief Mark the beginning of each render block.
     */
    void mark()
    {
        loopi(256)
        {
            startindices[i].add(vertindices[i].length());
        }
    }

    /**
     * @brief Shove world geometry to the GPU.
     */
    void loadtogpu(texbatch &b)
    {
        checkglerrors();
        b.bind_buffers();
        vector<vertex> *vs = verts + b.tex;
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertex) * vs->length(), vs->getbuf(), GL_STATIC_DRAW);
        vector<arrayindex> *evec = vertindices + b.tex;
        b.elemcount = evec->length();
        startindices[b.tex].add(b.elemcount);
        b.blockstarts = startindices[b.tex];
        if(b.elemcount <= 65535)
        {
            b.elemtype = GL_UNSIGNED_SHORT;
            ushort *shorts = new ushort[b.elemcount];
            loopi(b.elemcount) shorts[i] = (*evec)[i];
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(ushort) * b.elemcount, shorts, GL_STATIC_DRAW);
            delete[] shorts;
        }
        else
        {
            b.elemtype = GL_UNSIGNED_INT;
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint) * b.elemcount, evec->getbuf(), GL_STATIC_DRAW);
        }
        DEBUG("updated buffer data for texture " << b.tex << ": "
            << vs->length() << " verts, "
            << b.elemcount << " indices (" << ((b.elemtype == GL_UNSIGNED_INT) ? "uint" : "ushort") << ")");
    }

    void loadtogpu(texbatch *b, int n=1)
    {
        DEBUGS(int startmillis = SDL_GetTicks());
        loopi(n) loadtogpu(b[i]);
        DEBUGS(int endmillis = SDL_GetTicks());
        DEBUG("Updated all world geometry buffer objects, time taken: "
            << endmillis - startmillis
            << "ms\n");
    }

};

texbatch texbatches[256];

#define wloop(i) for(int i = 1, end = ssize - 1; i < end; ++i)
#define wbloop(start, end, var) for(int var = max(1, static_cast<int>(start)), __end = max(ssize-1, static_cast<int>(end)); var < __end; ++var)

// Map geometry has been updated and static geometry on the GPU needs updating.
bool regenworldbuffers = true;

/**
 * @brief Update GPU-side world geometry before next frame is rendered.
 */
void postregenworldvbos()
{
    if(!usenewworldrenderer)
    {
        // Should this go somewhere else?
        loopi(256) texbatches[i].deinit();
    }
    visibleblocks.setsize(0);
    regenworldbuffers = true;
}


int checkglerrors()
{
    int errcount = 0;
    GLenum error;
    while((error = glGetError()) != GL_NO_ERROR)
    {
        ++errcount;
        const char *str = "unknown";
        switch(error)
        {
        case GL_NO_ERROR: str = "No error"; break;
        case GL_INVALID_ENUM: str = "Invalid enum"; break;
        case GL_INVALID_VALUE: str = "Invalid value"; break;
        case GL_INVALID_OPERATION: str = "Invalid operation"; break;
        case GL_STACK_OVERFLOW: str = "Stack overflow"; break;

        }
        printf("OpenGL error: %s\n", str);
    }
    return errcount;
}

extern void recalc();
void checkbssize();
VARFP(bssize, 8, 32, 128, checkbssize(); postregenworldvbos());

/**
 * @brief Make sure bssize is a power of two.
 */
void checkbssize()
{
    int m = 1 << 16;
    while((m&bssize) == 0) m >>= 1;
    if(m != bssize)
    {
        conoutf("bssize must be a power of two, using %d", m);
    }
    bssize = m;
}


#define bloopxy(xb, yb) wbloop(yb*bssize, (yb+1)*bssize, y) wbloop(xb*bssize, (xb+1)*bssize, x)

/**
 * @brief Do you even flat bro?
 */
bool isevenflat(sqr const *s, bool ceil)
{
    return (s->type == SPACE) || (s->type == (ceil ? FHF : CHF));
}

/**
 * @brief Check if two cubes are identical enough to be merged into a mip.
 *
 * If a given set of power-of-two aligned cubes are all unifiable,
 * they can be rendered as one cube is basically what this means. Basically.
 */
bool unifiable(sqr const *s1, sqr const *s2, bool ceil)
{
    //if((s1->type == CORNER) != (s2->type == CORNER)) return false;
    if(s1->type == CORNER && s2->type == CORNER) return true;
    if(!isevenflat(s1, ceil) || !isevenflat(s2, ceil)) return false;
    if(ceil)
    {
        if(s1->ctex != s2->ctex) return false;
        if(s1->ceil != s2->ceil) return false;
    }
    else
    {
        if(s1->ftex != s2->ftex) return false;
        if(s1->floor != s2->floor) return false;
    }
    return true;
}

/**
 * @brief Get the corner corresponding to the given side
 *
 * Gets the corner opposing the corner which joins the given side
 * and the side next to it in counterclockwise direction.
 *
 * Helper function for cornertris.
 */
int cornerfromside(int s)
{
    switch(s)
    {
        case LEFT:   return BOTTOM_RIGHT;
        case TOP:    return BOTTOM_LEFT;
        case RIGHT:  return TOP_LEFT;
        case BOTTOM: return TOP_RIGHT;
    }
}

/**
 * @brief Generate both walls and flats for corners.
 */
void cornertris(worldmesh *wm, int x1, int y1, int bsize)
{
    // Check if the bsize*bsize block at x1,y1 (topleft corner)
    // is a complete corner
    if(bsize == 0) return;
    for(int y = y1, yend = y1+bsize; y < yend; ++y)
    {
        for(int x = x1, xend = x1+bsize; x < xend; ++x)
        {
            if(S(x, y)->type != CORNER)
            {
                // Non-corner cube found - split the block into four
                // and continue search recursively.
                int hbsize = bsize / 2;
                cornertris(wm, x1,        y1,        hbsize);
                cornertris(wm, x1+hbsize, y1,        hbsize);
                cornertris(wm, x1,        y1+hbsize, hbsize);
                cornertris(wm, x1+hbsize, y1+hbsize, hbsize);
                return;
            }
        }
    }
    // No corners that hug the very edges of the map. Thanks MINBORD!
    if(x1 == 0 || y1 == 0 || x1+bsize >= ssize || y1+bsize == ssize) return;

    // Do it like the original
    //  w
    // zSt
    //  vu

    sqr const *s = S(x1,       y1      );
    sqr const *w = S(x1,       y1-bsize);
    sqr const *t = S(x1+bsize, y1      );
    sqr const *u = S(x1+bsize, y1+bsize);
    sqr const *v = S(x1,       y1+bsize);
    sqr const *z = S(x1-bsize, y1      );

    int floor = 4 * s->floor;
    int ceil = 4 * s->ceil;

    bool normalwall = true;

    if(SOLID(z))
    {
        // Form corner with solids and w/v
        if(SOLID(w))
        {
            wm->wallquad(x1, y1, floor, floor, ceil, ceil, BOTTOM_LEFT, TOP_RIGHT, w->wtex, bsize);
            wm->flattri(x1, y1, floor, s->ftex, BOTTOM_RIGHT, false, bsize);
            wm->flattri(x1, y1, ceil, s->ctex, BOTTOM_RIGHT, true, bsize);
        }
        else if(SOLID(v))
        {
            wm->wallquad(x1, y1, floor, floor, ceil, ceil, BOTTOM_RIGHT, TOP_LEFT, v->wtex, bsize);
            wm->flattri(x1, y1, floor, s->ftex, TOP_RIGHT, false, bsize);
            wm->flattri(x1, y1, ceil, s->ctex, TOP_RIGHT, true, bsize);
        }
    }
    else if(SOLID(t))
    {
        // Form corner between solids t and w/v
        if(SOLID(w))
        {
            wm->wallquad(x1, y1, floor, floor, ceil, ceil, TOP_LEFT, BOTTOM_RIGHT, w->wtex, bsize);
            wm->flattri(x1, y1, floor, s->ftex, BOTTOM_LEFT, false, bsize);
            wm->flattri(x1, y1, ceil, s->ctex, BOTTOM_LEFT, true, bsize);
        }
        else if(SOLID(v))
        {
            wm->wallquad(x1, y1, floor, floor, ceil, ceil, TOP_RIGHT, BOTTOM_LEFT, v->wtex, bsize);
            wm->flattri(x1, y1, floor, s->ftex, TOP_LEFT, false, bsize);
            wm->flattri(x1, y1, ceil, s->ctex, TOP_LEFT, true, bsize);
        }
    }
    else
    {
        // No properly placed solids around - form a non-solid corner
        normalwall = false;
        bool wv = w->ceil-w->floor < v->ceil-v->floor;

        int floor2, ceil2, ftex2, ctex2, wcorner1, wcorner2, fcorner1, fcorner2;
        sqr const *s2 = NULL;

        if(z->ceil-z->floor < t->ceil-t->floor)
        {
            if(wv) { s2 = v; wcorner1 = BOTTOM_LEFT; fcorner1 = TOP_LEFT; }
            else { s2 = w; wcorner1 = BOTTOM_RIGHT; fcorner1 = BOTTOM_LEFT; }
        }
        else
        {
            if(wv) { s2 = v; wcorner1 = TOP_LEFT; fcorner1 = TOP_RIGHT; }
            else { s2 = w; wcorner1 = TOP_RIGHT; fcorner1 = BOTTOM_RIGHT; }
        }
        ctex2 = s2->ctex;
        ftex2 = s2->ftex;
        floor2 = 4 * s2->floor;
        ceil2 = 4 * s2->ceil;
        fcorner2 = opposingcorner(fcorner1);
        wcorner2 = opposingcorner(wcorner1);

        wm->wallquad(x1, y1, floor, floor, floor2, floor2, wcorner2, wcorner1, s->wtex, bsize); // Lower wall
        wm->wallquad(x1, y1, ceil2, ceil2, ceil, ceil, wcorner2, wcorner1, s->utex, bsize); // Upper wall
        wm->flattri(x1, y1, floor, s->ftex, fcorner1, false, bsize); // Floor tri 1
        wm->flattri(x1, y1, floor2, ftex2, fcorner2, false, bsize); // Floor tri 2
        wm->flattri(x1, y1, ceil, s->ctex, fcorner1, true, bsize); // Ceil tri 1
        wm->flattri(x1, y1, ceil2, ctex2, fcorner2, true, bsize); // Ceil tri 2
    }
    if(normalwall)
    {
        loopi(2) wm->cubewalls(x1, y1, i, false, bsize);
    }
}

/**
 * @brief Generate floor and ceiling geometry quadtree style.
 *
 * QUADTREE STYLE!
 *
 * Takes into account lighterror but current lighterror code is a bit..
 * primitive.
 */
void flatmeshqt(worldmesh *wm, int x1, int y1, int bsize, bool ceil)
{

    int xbeg = max(1, x1);
    int ybeg = max(1, y1);
    int xend = min(ssize-1, x1+bsize);
    int yend = min(ssize-1, y1+bsize);

    sqr const *prev = S(xbeg, ybeg);
    bool hf = (ceil && prev->type == CHF) || (!ceil && prev->type == FHF);

    int lighterr = 0;
    extern int lighterror;
    int maxlighterr = lighterror * lighterror;
    if(bsize == 1) goto end;
    for(int y = ybeg; y < yend; ++y)
    {
        for(int x = xbeg; x < xend; ++x)
        {
            sqr const *s = S(x, y);
            lighterr += abs(s->r - prev->r) + abs(s->g - prev->g) + abs(s->b - prev->b);
            // Check if lighterror is exceeded - never for skymap
            bool lightfail = (ceil ? prev->ctex : prev->ftex) != 0 && lighterr > maxlighterr;

            if(lightfail || !unifiable(prev, s, ceil))
            {
                int hbsize = bsize / 2;
                flatmeshqt(wm, x1,        y1,        hbsize, ceil);
                flatmeshqt(wm, x1+hbsize, y1,        hbsize, ceil);
                flatmeshqt(wm, x1+hbsize, y1+hbsize, hbsize, ceil);
                flatmeshqt(wm, x1,        y1+hbsize, hbsize, ceil);
                return;
            }
        }
    }
    end:
    if(prev->type != CORNER) wm->flat(x1, y1, ceil, bsize);
    //if(!SOLID(prev) && waterlevel > prev->floor) addwaterquad(x1, y1, bssize);
}

void wallmeshqt(worldmesh *wm, int x1, int y1, int bsize, bool ceil)
{

    int xbeg = max(1, x1);
    int ybeg = max(1, y1);
    int xend = min(ssize-1, x1+bsize);
    int yend = min(ssize-1, y1+bsize);

    sqr const *prev = S(xbeg, ybeg);
    //bool hf = (ceil && prev->type == CHF) || (!ceil && prev->type == FHF);

    //int lighterr = 0;
    //extern int lighterror;
    //int maxlighterr = lighterror * lighterror;
    if(bsize == 1) goto end;
    for(int y = ybeg; y < yend; ++y)
    {
        for(int x = xbeg; x < xend; ++x)
        {
            bool edge = x == xbeg || x == xend-1 || y == ybeg || y == yend-1;
            sqr const *s = S(x, y);
            //lighterr += abs(s->r - prev->r) + abs(s->g - prev->g) + abs(s->b - prev->b);
            // Check if lighterror is exceeded - never for skymap
            //bool lightfail = (ceil ? prev->ctex : prev->ftex) != 0 && lighterr > maxlighterr;
            //bool wtexfail = edge && !ceil && (s->wtex != prev->wtex);
            //bool utexfail = edge && ceil && (s->utex != prev->utex);

            bool texfail = false;
            bool typefail = false;
            bool heightfail = false;

            if(edge)
            {
                texfail = ceil ? (s->utex != prev->utex) : (s->wtex != prev->wtex);
                typefail = s->type != prev->type || (s->type != SPACE || s->type != SOLID);
                if(!SOLID(s)) heightfail = ceil ? (s->ceil != prev->ceil) : (s->floor != prev->floor);
            }

            if(typefail || texfail || heightfail)
            {
                int hbsize = bsize / 2;
                wallmeshqt(wm, x1,        y1,        hbsize, ceil);
                wallmeshqt(wm, x1+hbsize, y1,        hbsize, ceil);
                wallmeshqt(wm, x1+hbsize, y1+hbsize, hbsize, ceil);
                wallmeshqt(wm, x1,        y1+hbsize, hbsize, ceil);
                return;
            }
        }
    }
    end:
    if(prev->type != CORNER) loopi(2) wm->cubewalls(x1, y1, i, true, bsize);
}

/**
 * @brief Generate wall mesh
 */
void wallmesh(worldmesh *wm, int x1, int y1, int bsize, bool ceil)
{
    int xbeg = max(1, x1);
    int ybeg = max(1, y1);
    int xend = min(ssize-1, x1+bsize);
    int yend = min(ssize-1, y1+bsize);

    //wmaux(wm, x1, y1, bsize, ceil, false, false);

    // Generate left-to-right walls
    for(int i = 0; i < 2; ++i)
    {
        bool south = !i;
        int off = i ? 1 : -1;
        for(int y = ybeg; y < yend; ++y)
        {
            sqr const *prev = S(1, y+off);
            int prevx = 1;
            for(int x = xbeg; x < xend; ++x)
            {
                sqr const *s = S(x,y);
                sqr const *u = S(x, y+off);
                if(SOLID(u))
                {
                    if(SOLID(s)) continue;
                    int nx = x;
                    int lelevation = 4 * s->floor;
                    int uelevation = 4 * s->ceil;
                    for(; nx < xend; ++nx)
                    {
                        lelevation = min(lelevation, cornerheight(x+1, y, TOP_LEFT, false));
                        uelevation = max(uelevation, cornerheight(x+1, y, TOP_LEFT, true));
                        if(!SOLID(S(nx, y+off))) break;
                        if(S(nx, y+off)->wtex != S(nx-1, y+off)->wtex) break;
                        if(S(nx, y)->type != SPACE) break;
                    }
                    int corner1 = TOP_LEFT, corner2 = TOP_RIGHT;
                    if(i == 1)
                    {
                        corner1 = BOTTOM_RIGHT;
                        corner2 = BOTTOM_LEFT;
                    }
                    if(s->type == FHF || s->type == CHF)
                    {
                        // Small optimisation for heightfields
                        // Wall tris along walls next to heightfields
                        // have their upper and lower verts connect
                        wm->wallquad(x, y,
                                     cornerheight(x, y, corner1, false),
                                     cornerheight(x, y, corner2, false),
                                     cornerheight(x, y, corner1, true),
                                     cornerheight(x, y, corner2, true),
                                     corner1, corner2, u->wtex, nx - x);
                    }
                    else
                    {
                        wm->wallquad(x, y,
                                     lelevation, lelevation,
                                     uelevation, uelevation,
                                     corner1, corner2, u->wtex, nx - x);
                    }
                    x = nx;
                }
            }
        }

    }
}

#define BI(x,y) (y*(ssize/bssize)+x)
#define BX(i) (i/bssize)
#define BY(i) (i%bssize)


int getbscount()
{
    return ssize / bssize;
}

/**
 * @brief Get the (x,y) coordinates of the given block
 */
coord2d blockpos(int b, bool center=false)
{
    int bscount = getbscount();
    coord2d bpos;
    bpos.x = (b % bscount)*bssize;
    bpos.y = (b / bscount)*bssize;
    if(center)
    {
        int hbssize = bssize / 2;
        bpos.x += hbssize;
        bpos.y += hbssize;
    }
    return bpos;
}

/**
 * @brief Calculate distance from camera to the render block
 */
double blockdist(int b)
{
    coord2d bp = blockpos(b, true);
    return sqrt(pow(bp.x-camera1->o.x, 2) + pow(bp.y-camera1->o.y, 2));
}

void findvisibleblocks()
{
    visibleblocks.setsize(0);
    int bscount = ssize / bssize;

    loopj(bscount) loopk(bscount)
    {
        if(!isoccluded(camera1->o.x, camera1->o.y, k*bssize, j*bssize, bssize))
        {
            visibleblocks.add(BI(k, j));
        }
    }

    visibleblocks.sort<int>(intcmpf);

    visibleblockspans.setsize(0);
    if(visibleblocks.length() == 0) return;

    visibleblockspan *span = &(visibleblockspans.add(visibleblockspan(visibleblocks[0])));
    for(int i = 1; i < visibleblocks.length(); ++i)
    {
        int vb = visibleblocks[i];
        if((visibleblocks[i-1]+1 == vb))
        {
            ++span->count;
            span->updatenearest(vb);
        }
        else
        {
            span = &(visibleblockspans.add(visibleblockspan(vb)));
        }
    }
}

bool iswatercube(sqr const *s)
{
    return !SOLID(s) && (waterlevel >= s->floor);
}

/**
 * @brief Find visible water quads in the given area.
 */
void findwaterquadsqt(int x, int y, int size, int minsize)
{
    bool iswaterquad = iswatercube(S(x, y));
    loopi(size) loopk(size)
    {
        sqr const *s = S(x+k, y+i);
        if(iswatercube(s) != iswaterquad)
        {
            if(size <= minsize)
            {
                iswaterquad = true;
                goto done;
            }
            int hsize = size / 2;
            findwaterquadsqt(x,       y,       hsize, minsize);
            findwaterquadsqt(x+hsize, y,       hsize, minsize);
            findwaterquadsqt(x,       y+hsize, hsize, minsize);
            findwaterquadsqt(x+hsize, y+hsize, hsize, minsize);
            return;
        }
    }
    done:
    if(iswaterquad && !isoccluded(camera1->o.x, camera1->o.y, x, y, size))
    {
        addwaterquad(x, y, size);
    }
}

/**
 * @brief Find all visible water quads.
 */
void findwaterquads()
{
    // FIXME this function is an unacceptable fps drainer, optimise or
    // come up with a better way of dealing with water quads.
    //findwaterquadsqt(0, 0, ssize, 4);

    // I optimised it! Only look for water quads in visible render blocks.
    loopv(visibleblocks)
    {
        coord2d bp = blockpos(visibleblocks[i]);
        findwaterquadsqt(bp.x, bp.y, bssize, 2);
    }
}

void prepgpudata()
{
    if(!regenworldbuffers) return;

    // TODO separate world buffer generation and renderer initialisation
    if(!loadglprocs())
    {
        conoutf("Failed to load OpenGL entry points required for new world renderer");
        usenewworldrenderer = 0;
        return;
    }
    loopi(256) texbatches[i].init(i);
    worldmesh wm;
    wm.mark();
    int bscount = ssize / bssize;
    loopi(bscount) loopk(bscount)
    {
        flatmeshqt(&wm, k*bssize, i*bssize, bssize, false);
        flatmeshqt(&wm, k*bssize, i*bssize, bssize, true);
        //wallmesh(&wm, k*bssize, i*bssize, bssize, false);
        //wallmesh(&wm, k*bssize, i*bssize, bssize, true);
        //loop(y, bssize) loop(x, bssize) wm.cubewalls(k*bssize+x, i*bssize+y, false);
        //loop(y, bssize) loop(x, bssize) wm.cubewalls(k*bssize+x, i*bssize+y, true);
        wallmeshqt(&wm, k*bssize, i*bssize, bssize, false);
        wallmeshqt(&wm, k*bssize, i*bssize, bssize, true);
        cornertris(&wm, k*bssize, i*bssize, bssize);
        wm.mark();
    }
    wm.loadtogpu(texbatches, 256);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    regenworldbuffers = false;
}

void rendertrissky()
{
    if(texbatches[0].elemcount)
    {
        skyfloor = -128.0f; // No idea what this does
        glDisable(GL_TEXTURE_2D);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_ARRAY_BUFFER);
        glEnableClientState(GL_COLOR_ARRAY);
        texbatches[0].pre();
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        texbatches[0].batchdraw();
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glEnable(GL_TEXTURE_2D);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

// TODO git rid of unused arguments
void render_world_new(float vx, float vy, float vh, float changelod, int yaw, int pitch, float fov, float fovy, int w, int h)
{
    wdrawcalls = 0;

    if(!visibleblocks.length()) goto done;

    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_ARRAY_BUFFER);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    for(int i = 1; i < 256; ++i)
    {
        if(texbatches[i].elemcount == 0) continue;
        texbatches[i].batchdraw();
    }

    done:
    // Is this really necessary?
    glDisableClientState(GL_ARRAY_BUFFER);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    // This is really necessary.
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

#undef VERTCOLOURS
