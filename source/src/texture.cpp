// texture.cpp: texture management

#include "cube.h"

#define FUNCNAME(name) name##1
#define DEFPIXEL uint OP(r, 0);
#define PIXELOP OP(r, 0);
#define BPP 1
#include "scale.h"

#define FUNCNAME(name) name##2
#define DEFPIXEL uint OP(r, 0), OP(g, 1);
#define PIXELOP OP(r, 0); OP(g, 1);
#define BPP 2
#include "scale.h"

#define FUNCNAME(name) name##3
#define DEFPIXEL uint OP(r, 0), OP(g, 1), OP(b, 2);
#define PIXELOP OP(r, 0); OP(g, 1); OP(b, 2);
#define BPP 3
#include "scale.h"

#define FUNCNAME(name) name##4
#define DEFPIXEL uint OP(r, 0), OP(g, 1), OP(b, 2), OP(a, 3);
#define PIXELOP OP(r, 0); OP(g, 1); OP(b, 2); OP(a, 3);
#define BPP 4
#include "scale.h"

void scaletexture(uchar *src, uint sw, uint sh, uint bpp, uchar *dst, uint dw, uint dh)
{
    if(sw == dw*2 && sh == dh*2)
    {
        switch(bpp)
        {
            case 1: return halvetexture1(src, sw, sh, dst);
            case 2: return halvetexture2(src, sw, sh, dst);
            case 3: return halvetexture3(src, sw, sh, dst);
            case 4: return halvetexture4(src, sw, sh, dst);
        }
    }
    else if(sw < dw || sh < dh || sw&(sw-1) || sh&(sh-1))
    {
        switch(bpp)
        {
            case 1: return scaletexture1(src, sw, sh, dst, dw, dh);
            case 2: return scaletexture2(src, sw, sh, dst, dw, dh);
            case 3: return scaletexture3(src, sw, sh, dst, dw, dh);
            case 4: return scaletexture4(src, sw, sh, dst, dw, dh);
        }
    }
    else
    {
        switch(bpp)
        {
            case 1: return shifttexture1(src, sw, sh, dst, dw, dh);
            case 2: return shifttexture2(src, sw, sh, dst, dw, dh);
            case 3: return shifttexture3(src, sw, sh, dst, dw, dh);
            case 4: return shifttexture4(src, sw, sh, dst, dw, dh);
        }
    }
}

Texture *notexture = NULL, *noworldtexture = NULL;

hashtable<char *, Texture> textures;

VAR(hwtexsize, 1, 0, 0);
VAR(hwmaxaniso, 1, 0, 0);
VARFP(maxtexsize, 0, 0, 1<<12, initwarning("texture quality", INIT_LOAD));
VARFP(texreduce, -1, 0, 3, initwarning("texture quality", INIT_LOAD));
VARFP(trilinear, 0, 1, 1, initwarning("texture filtering", INIT_LOAD));
VARFP(bilinear, 0, 1, 1, initwarning("texture filtering", INIT_LOAD));
VARFP(aniso, 0, 0, 16, initwarning("texture filtering", INIT_LOAD));

int formatsize(GLenum format)
{
    switch(format)
    {
        case GL_LUMINANCE:
        case GL_ALPHA: return 1;
        case GL_LUMINANCE_ALPHA: return 2;
        case GL_RGB: return 3;
        case GL_RGBA: return 4;
        default: return 4;
    }
}

void resizetexture(int w, int h, bool mipmap, bool canreduce, GLenum target, int &tw, int &th)
{
    int hwlimit = hwtexsize,
        sizelimit = mipmap && maxtexsize ? min(maxtexsize, hwlimit) : hwlimit;
    if(canreduce && texreduce)
    {
        if(texreduce==-1)
        {
            w = 2;
            h = 2;
        }
        else
        {
            w = max(w>>texreduce, 2); // 1);
            h = max(h>>texreduce, 2); // 1);
        }
    }
    w = min(w, sizelimit);
    h = min(h, sizelimit);
    if(mipmap || w&(w-1) || h&(h-1))
    {
        tw = th = 1;
        while(tw < w) tw *= 2;
        while(th < h) th *= 2;
        if(w < tw - tw/4) tw /= 2;
        if(h < th - th/4) th /= 2;
    }
    else
    {
        tw = w;
        th = h;
    }
}

void uploadtexture(GLenum target, GLenum internal, int tw, int th, GLenum format, GLenum type, void *pixels, int pw, int ph, bool mipmap)
{
    int bpp = formatsize(format);
    uchar *buf = NULL;
    if(pw!=tw || ph!=th)
    {
        buf = new uchar[tw*th*bpp];
        scaletexture((uchar *)pixels, pw, ph, bpp, buf, tw, th);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for(int level = 0;; level++)
    {
        uchar *src = buf ? buf : (uchar *)pixels;
        glTexImage2D(target, level, internal, tw, th, 0, format, type, src);
        if(!mipmap || max(tw, th) <= 1) break;
        int srcw = tw, srch = th;
        if(tw > 1) tw /= 2;
        if(th > 1) th /= 2;
        if(!buf) buf = new uchar[tw*th*bpp];
        scaletexture(src, srcw, srch, bpp, buf, tw, th);
    }
    if(buf) delete[] buf;
}

void createtexture(int tnum, int w, int h, void *pixels, int clamp, bool mipmap, bool canreduce, GLenum format)
{
    glBindTexture(GL_TEXTURE_2D, tnum);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp&1 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp&2 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    if(hasAF && min(aniso, hwmaxaniso) > 0 && mipmap) glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, min(aniso, hwmaxaniso));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
        mipmap ?
            (trilinear ?
                (bilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR) :
                (bilinear ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST)) :
            (bilinear ? GL_LINEAR : GL_NEAREST));

    int tw = w, th = h;
    if(pixels) resizetexture(w, h, mipmap, canreduce, GL_TEXTURE_2D, tw, th);
    uploadtexture(GL_TEXTURE_2D, format, tw, th, format, GL_UNSIGNED_BYTE, pixels, w, h, mipmap);
}

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define RGBAMASKS 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff
#define RGBMASKS  0xff0000, 0x00ff00, 0x0000ff, 0
#else
#define RGBAMASKS 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000
#define RGBMASKS  0x0000ff, 0x00ff00, 0xff0000, 0
#endif

SDL_Surface *wrapsurface(void *data, int width, int height, int bpp)
{
    switch(bpp)
    {
        case 3: return SDL_CreateRGBSurfaceFrom(data, width, height, 8*bpp, bpp*width, RGBMASKS);
        case 4: return SDL_CreateRGBSurfaceFrom(data, width, height, 8*bpp, bpp*width, RGBAMASKS);
    }
    return NULL;
}

SDL_Surface *creatergbsurface(int width, int height)
{
    return SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 24, RGBMASKS);
}

SDL_Surface *creatergbasurface(int width, int height)
{
    return SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 32, RGBAMASKS);
}

SDL_Surface *forcergbsurface(SDL_Surface *os)
{
    SDL_Surface *ns = SDL_CreateRGBSurface(SDL_SWSURFACE, os->w, os->h, 24, RGBMASKS);
    if(ns) SDL_BlitSurface(os, NULL, ns, NULL);
    SDL_FreeSurface(os);
    return ns;
}

SDL_Surface *forcergbasurface(SDL_Surface *os)
{
    SDL_Surface *ns = SDL_CreateRGBSurface(SDL_SWSURFACE, os->w, os->h, 32, RGBAMASKS);
    if(ns)
    {
        SDL_SetAlpha(os, 0, 0);
        SDL_BlitSurface(os, NULL, ns, NULL);
    }
    SDL_FreeSurface(os);
    return ns;
}

bool checkgrayscale(SDL_Surface *s)
{
    // gray scale images have 256 levels, no colorkey, and the palette is a ramp
    if(s->format->palette)
    {
        if(s->format->palette->ncolors != 256 || s->format->colorkey) return false;
        const SDL_Color *colors = s->format->palette->colors;
        loopi(256) if(colors[i].r != i || colors[i].g != i || colors[i].b != i) return false;
    }
    return true;
}

int fixcl(SDL_Surface *s, bool check = true, Uint8 value = 0, Uint8 mlimit = 255)
{
    Uint32 pixel = 0;
    int bpp = s->format->BytesPerPixel;
    int F = 0, N = 0, t = 0;
    while ( value > mlimit ) {t++; value >>= 1;}
    int tmp = s->w * bpp;
    for (int i = 0; i < tmp; i+=bpp)
    {
        for (int j = 0; j < s->h; j++)
        {
            Uint8 *p = (Uint8 *)s->pixels + j * s->w * bpp + i;
            switch (bpp)
            {
                case 1:
                {
                    if (check) pixel = *p;
                    else *p >>= t;
                    break;
                }
                case 2:
                {
                    if (check) pixel = *(Uint16 *)p;
                    else { p[0] >>= t; p[1] >>= t; }
                    break;
                }
                case 3:
                {
                    if (check)
                    {
                        if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
                            pixel = p[0] << 16 | p[1] << 8 | p[2];
                        else
                            pixel = p[0] | p[1] << 8 | p[2] << 16;
                    }
                    else { loopk(3) p[k] >>= t; }
                    break;
                }
               case 4:
               {
                   if (check) pixel = *(Uint32 *)p;
                   else {
                       Uint8 r = 0, g = 0, b = 0, a = 0;
                       SDL_GetRGBA(pixel, s->format, &r, &g, &b, &a);
                       r >>= t; g >>= t; b >>= t;
                       Uint32 *q = (Uint32 *)p;
                       *q = SDL_MapRGBA(s->format, r,g,b,a);
                   }
                   break;
               }
                default: break;
            }
            if (check) {
                Uint8 r = 0, g = 0, b = 0, a = 0;
                SDL_GetRGBA(pixel, s->format, &r, &g, &b, &a);
                F += r > g ? ( r > b ? r : b ) : ( g > b ? g : b ); N++;
            }
        }
    }
    if (!N) return 0;
    return F/N;
}

SDL_Surface *fixsurfaceformat(SDL_Surface *s)
{
    if(!s) return NULL;
    if(!s->pixels || min(s->w, s->h) <= 0 || s->format->BytesPerPixel <= 0)
    {
        SDL_FreeSurface(s);
        return NULL;
    }
    static const uint rgbmasks[] = { RGBMASKS }, rgbamasks[] = { RGBAMASKS };
    switch(s->format->BytesPerPixel)
    {
        case 1:
            if(!checkgrayscale(s)) return s->format->colorkey ? forcergbasurface(s) : forcergbsurface(s);
            break;
        case 3:
            if(s->format->Rmask != rgbmasks[0] || s->format->Gmask != rgbmasks[1] || s->format->Bmask != rgbmasks[2])
                return forcergbsurface(s);
            break;
        case 4:
            if(s->format->Rmask != rgbamasks[0] || s->format->Gmask != rgbamasks[1] || s->format->Bmask != rgbamasks[2] || s->format->Amask != rgbamasks[3])
                return s->format->Amask ? forcergbasurface(s) : forcergbsurface(s);
            break;
    }
    return s;
}

GLenum texformat(int bpp)
{
    switch(bpp)
    {
        case 8: return GL_LUMINANCE;
        case 16: return GL_LUMINANCE_ALPHA;
        case 24: return GL_RGB;
        case 32: return GL_RGBA;
        default: return 0;
    }
}

SDL_Surface *texdecal(SDL_Surface *s)
{
    SDL_Surface *m = SDL_CreateRGBSurface(SDL_SWSURFACE, s->w, s->h, 16, 0, 0, 0, 0);
    if(!m) fatal("create surface");
    uchar *dst = (uchar *)m->pixels, *src = (uchar *)s->pixels;
    loopi(s->h*s->w)
    {
        *dst++ = *src;
        *dst++ = 255 - *src;
        src += s->format->BytesPerPixel;
    }
    SDL_FreeSurface(s);
    return m;
}

void scalesurface(SDL_Surface *s, float scale)
{
    uint dw = s->w*scale, dh = s->h*scale;
    uchar *buf = new uchar[dw*dh*s->format->BytesPerPixel];
    scaletexture((uchar *)s->pixels, s->w, s->h, s->format->BytesPerPixel, buf, dw, dh);
    delete[] (uchar *)s->pixels;
    s->w = dw;
    s->h = dh;
    s->pixels = buf;
}

bool silent_texture_load = false;

VARFP(hirestextures, 0, 1, 1, initwarning("texture resolution", INIT_LOAD));
bool uniformtexres = !hirestextures;

GLuint loadsurface(const char *texname, int &xs, int &ys, int &bpp, int clamp = 0, bool mipmap = true, bool canreduce = false, float scale = 1.0f, bool trydl = false)
{
    const char *file = texname;
    if(texname[0]=='<')
    {
        file = strchr(texname, '>');
        if(!file) { if(!silent_texture_load) conoutf("could not load texture %s", texname); return 0; }
        file++;
    }

    SDL_Surface *s = NULL;
    stream *z = openzipfile(file, "rb");
    if(z)
    {
        SDL_RWops *rw = z->rwops();
        if(rw)
        {
            s = IMG_Load_RW(rw, 0);
            SDL_FreeRW(rw);
        }
        delete z;
    }
    if(!s) s = IMG_Load(findfile(file, "rb"));
    if(!s)
    {
        if(trydl)
            requirepackage(PCK_TEXTURE, file);
        else if(!silent_texture_load) conoutf("couldn't load texture %s", texname);
        return 0;
    }
    s = fixsurfaceformat(s);
    Uint8 x = 0;
    if(strstr(texname,"playermodel") && (x = fixcl(s)) > 35) { fixcl(s,false,x,35); }
    else if(strstr(texname,"skin") && strstr(texname,"weapon") && (x = fixcl(s)) > 40 ) { fixcl(s,false,x,40); }

    GLenum format = texformat(s->format->BitsPerPixel);
    if(!format)
    {
        SDL_FreeSurface(s);
        conoutf("texture must be 8, 16, 24, or 32 bpp: %s", texname);
        return 0;
    }
    if(max(s->w, s->h) > (1<<12))
    {
        SDL_FreeSurface(s);
        conoutf("texture size exceeded %dx%d pixels: %s", 1<<12, 1<<12, texname);
        return 0;
    }

    if(texname[0]=='<')
    {
        const char *cmd = &texname[1], *arg1 = strchr(cmd, ':');//, *arg2 = arg1 ? strchr(arg1, ',') : NULL;
        if(!arg1) arg1 = strchr(cmd, '>');
        if(!strncmp(cmd, "decal", arg1-cmd)) { s = texdecal(s); format = texformat(s->format->BitsPerPixel); }
    }

    if(uniformtexres && scale > 1.0f) scalesurface(s, 1.0f/scale);

    GLuint tnum;
    glGenTextures(1, &tnum);
    createtexture(tnum, s->w, s->h, s->pixels, clamp, mipmap, canreduce, format);
    xs = s->w;
    ys = s->h;
    bpp = s->format->BitsPerPixel;
    SDL_FreeSurface(s);
    return tnum;
}

// management of texture slots
// each texture slot can have multiple texture frames, of which currently only the first is used
// additional frames can be used for various shaders

Texture *textureload(const char *name, int clamp, bool mipmap, bool canreduce, float scale, bool trydl)
{
    string pname;
    copystring(pname, name);
    path(pname);
    Texture *t = textures.access(pname);
    if(t) return t;
    int xs, ys, bpp;
    GLuint id = loadsurface(pname, xs, ys, bpp, clamp, mipmap, canreduce, scale, trydl);
    if(!id) return notexture;
    char *key = newstring(pname);
    t = &textures[key];
    t->name = key;
    t->xs = xs;
    t->ys = ys;
    t->bpp = bpp;
    t->clamp = clamp;
    t->mipmap = mipmap;
    t->canreduce = canreduce;
    t->id = id;
    t->scale = scale;
    return t;
}

Texture *createtexturefromsurface(const char *name, SDL_Surface *s)
{
    string pname;
    copystring(pname, name);
    path(pname);
    Texture *t = textures.access(pname);
    if(!t)
    {
        char *key = newstring(pname);
        t = &textures[key];
        t->name = key;
    }

    GLuint tnum;
    glGenTextures(1, &tnum);
    GLenum format = texformat(s->format->BitsPerPixel);
    createtexture(tnum, s->w, s->h, s->pixels, 0, true, false, format);

    t->xs = s->w;
    t->ys = s->h;
    t->bpp = s->format->BitsPerPixel;
    t->clamp = 0;
    t->mipmap = true;
    t->canreduce = false;
    t->id = tnum;
    return t;
}

struct Slot
{
    string name;
    float scale;
    Texture *tex;
    bool loaded;
};

vector<Slot> slots;

void texturereset() { if(execcontext==IEXC_MAPCFG) slots.setsize(0); }

void texture(float *scale, char *name)
{
    Slot &s = slots.add();
    copystring(s.name, name);
    path(s.name);
    s.tex = NULL;
    s.loaded = false;
    s.scale = (*scale > 0 && *scale <= 2.0f) ? *scale : 1.0f;
}

COMMAND(texturereset, "");
COMMAND(texture, "fs");

Texture *lookuptexture(int tex, Texture *failtex, bool trydl)
{
    Texture *t = failtex;
    if(slots.inrange(tex))
    {
        Slot &s = slots[tex];
        if(!s.loaded)
        {
            defformatstring(pname)("packages/textures/%s", s.name);
            s.tex = textureload(pname, 0, true, true, s.scale, trydl);
            if(!trydl)
            {
            if(s.tex==notexture) s.tex = failtex;
            s.loaded = true;
        }
        }
        if(s.tex) t = s.tex;
    }
    return t;
}

void cleanuptextures()
{
    enumerate(textures, Texture, t,
        if(t.id) { glDeleteTextures(1, &t.id); t.id = 0; }
    );
}

bool reloadtexture(Texture &t)
{
    if(t.id) return true;
    int xs = 1, ys = 1, bpp = 0;
    t.id = loadsurface(t.name, xs, ys, bpp, t.clamp, t.mipmap, t.canreduce, t.scale);
    t.xs = xs;
    t.ys = ys;
    t.bpp = bpp;
    return t.id!=0;
}

bool reloadtexture(const char *name)
{
    Texture *t = textures.access(path(name, true));
    if(t) return reloadtexture(*t);
    return false;
}

void reloadtextures()
{
    enumerate(textures, Texture, t, reloadtexture(t));
}

Texture *sky[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
static string skybox;

void loadsky(char *basename, bool reload)
{
    const char *side[] = { "lf", "rt", "ft", "bk", "dn", "up" };
    if(reload) basename = skybox;
    else copystring(skybox, basename);
    loopi(6)
    {
        defformatstring(name)("packages/%s_%s.jpg", basename, side[i]);
        sky[i] = textureload(name, 3);
        if(sky[i] == notexture && !reload && autodownload)
        {
            defformatstring(dl)("packages/%s", basename);
            requirepackage(PCK_SKYBOX, dl);
            break;
        }
    }
}

COMMANDF(loadsky, "s", (char *name) { loadsky(name, false); intret(0); });
void loadnotexture(char *c)
{
    noworldtexture = notexture; // reset to default
    if(c[0])
    {
        defformatstring(p)("packages/textures/%s", c);
        noworldtexture = textureload(p);
        if(noworldtexture==notexture) conoutf("could not load alternative texture '%s'.", p);
    }
}
COMMAND(loadnotexture, "s");

void draw_envbox_face(float s0, float t0, float x0, float y0, float z0,
                      float s1, float t1, float x1, float y1, float z1,
                      float s2, float t2, float x2, float y2, float z2,
                      float s3, float t3, float x3, float y3, float z3,
                      Texture *tex)
{
    glBindTexture(GL_TEXTURE_2D, tex->id);
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(s3, t3); glVertex3f(x3, y3, z3);
    glTexCoord2f(s2, t2); glVertex3f(x2, y2, z2);
    glTexCoord2f(s0, t0); glVertex3f(x0, y0, z0);
    glTexCoord2f(s1, t1); glVertex3f(x1, y1, z1);
    glEnd();
    xtraverts += 4;
}

VAR(skyclip, 0, 1, 1);

float skyfloor = 1e16f;

void draw_envbox(int w)
{
    extern float skyfloor;

    float zclip = skyclip && skyfloor >= camera1->o.z ? 0.5f + float(skyfloor-camera1->o.z)/w : 0.0f,
          vclip = 1-zclip,
          z = 2*w*(vclip-0.5f);

    if(vclip < 0) return;

    glDepthMask(GL_FALSE);

    draw_envbox_face(0.0f, 0.0f, -w, -w, -w,
                     1.0f, 0.0f, -w,  w, -w,
                     1.0f, vclip, -w,  w,  z,
                     0.0f, vclip, -w, -w,  z, sky[0] ? sky[0] : notexture);

    draw_envbox_face(1.0f, vclip, +w, -w,  z,
                     0.0f, vclip, +w,  w,  z,
                     0.0f, 0.0f, +w,  w, -w,
                     1.0f, 0.0f, +w, -w, -w, sky[1] ? sky[1] : notexture);

    draw_envbox_face(1.0f, vclip, -w, -w,  z,
                     0.0f, vclip,  w, -w,  z,
                     0.0f, 0.0f,  w, -w, -w,
                     1.0f, 0.0f, -w, -w, -w, sky[2] ? sky[2] : notexture);

    draw_envbox_face(1.0f, vclip, +w,  w,  z,
                     0.0f, vclip, -w,  w,  z,
                     0.0f, 0.0f, -w,  w, -w,
                     1.0f, 0.0f, +w,  w, -w, sky[3] ? sky[3] : notexture);

    if(!zclip)
        draw_envbox_face(0.0f, 1.0f, -w,  w,  w,
                         0.0f, 0.0f, +w,  w,  w,
                         1.0f, 0.0f, +w, -w,  w,
                         1.0f, 1.0f, -w, -w,  w, sky[4] ? sky[4] : notexture);

    draw_envbox_face(0.0f, 1.0f, +w,  w, -w,
                     0.0f, 0.0f, -w,  w, -w,
                     1.0f, 0.0f, -w, -w, -w,
                     1.0f, 1.0f, +w, -w, -w, sky[5] ? sky[5] : notexture);

    glDepthMask(GL_TRUE);

    skyfloor = 1e16f;
}

struct tmufunc
{
    GLenum combine, sources[3], ops[3];
    int scale;
};

struct tmu
{
    GLenum mode;
    GLfloat color[4];
    tmufunc rgb, alpha;
};

#define INVALIDTMU \
{ \
    0, \
    { -1, -1, -1, -1 }, \
    { 0, { 0, 0, 0 }, { 0, 0, 0 }, 0 }, \
    { 0, { 0, 0, 0 }, { 0, 0, 0 }, 0 } \
}

#define INITTMU \
{ \
    GL_MODULATE, \
    { 0, 0, 0, 0 }, \
    { GL_MODULATE, { GL_TEXTURE, GL_PREVIOUS_ARB, GL_CONSTANT_ARB }, { GL_SRC_COLOR, GL_SRC_COLOR, GL_SRC_ALPHA }, 1 }, \
    { GL_MODULATE, { GL_TEXTURE, GL_PREVIOUS_ARB, GL_CONSTANT_ARB }, { GL_SRC_ALPHA, GL_SRC_ALPHA, GL_SRC_ALPHA }, 1 } \
}

#define MAXTMUS 4

tmu tmus[MAXTMUS] =
{
    INVALIDTMU,
    INVALIDTMU,
    INVALIDTMU,
    INVALIDTMU
};

VAR(maxtmus, 1, 0, 0);

void parsetmufunc(tmufunc &f, const char *s)
{
    int arg = -1;
    while(*s) switch(tolower(*s++))
    {
        case 't': f.sources[++arg] = GL_TEXTURE; f.ops[arg] = GL_SRC_COLOR; break;
        case 'p': f.sources[++arg] = GL_PREVIOUS_ARB; f.ops[arg] = GL_SRC_COLOR; break;
        case 'k': f.sources[++arg] = GL_CONSTANT_ARB; f.ops[arg] = GL_SRC_COLOR; break;
        case 'c': f.sources[++arg] = GL_PRIMARY_COLOR_ARB; f.ops[arg] = GL_SRC_COLOR; break;
        case '~': f.ops[arg] = GL_ONE_MINUS_SRC_COLOR; break;
        case 'a': f.ops[arg] = f.ops[arg]==GL_ONE_MINUS_SRC_COLOR ? GL_ONE_MINUS_SRC_ALPHA : GL_SRC_ALPHA; break;
        case '=': f.combine = GL_REPLACE; break;
        case '*': f.combine = GL_MODULATE; break;
        case '+': f.combine = GL_ADD; break;
        case '-': f.combine = GL_SUBTRACT_ARB; break;
        case ',':
        case '@': f.combine = GL_INTERPOLATE_ARB; break;
        case '.': f.combine = GL_DOT3_RGB_ARB; break;
        case 'x': while(!isdigit(*s)) s++; f.scale = *s++-'0'; break;
    }
}

void committmufunc(bool rgb, tmufunc &dst, tmufunc &src)
{
    if(dst.combine!=src.combine) glTexEnvi(GL_TEXTURE_ENV, rgb ? GL_COMBINE_RGB_ARB : GL_COMBINE_ALPHA_ARB, src.combine);
    loopi(3)
    {
        if(dst.sources[i]!=src.sources[i]) glTexEnvi(GL_TEXTURE_ENV, (rgb ? GL_SOURCE0_RGB_ARB : GL_SOURCE0_ALPHA_ARB)+i, src.sources[i]);
        if(dst.ops[i]!=src.ops[i]) glTexEnvi(GL_TEXTURE_ENV, (rgb ? GL_OPERAND0_RGB_ARB : GL_OPERAND0_ALPHA_ARB)+i, src.ops[i]);
    }
    if(dst.scale!=src.scale) glTexEnvi(GL_TEXTURE_ENV, rgb ? GL_RGB_SCALE_ARB : GL_ALPHA_SCALE, src.scale);
}

void committmu(int n, tmu &f)
{
    if(n>=maxtmus) return;
    if(tmus[n].mode!=f.mode) glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, f.mode);
    if(memcmp(tmus[n].color, f.color, sizeof(f.color))) glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, f.color);
    committmufunc(true, tmus[n].rgb, f.rgb);
    committmufunc(false, tmus[n].alpha, f.alpha);
    tmus[n] = f;
}

void resettmu(int n)
{
    tmu f = tmus[n];
    f.mode = GL_MODULATE;
    f.rgb.scale = 1;
    f.alpha.scale = 1;
    committmu(n, f);
}

void scaletmu(int n, int rgbscale, int alphascale)
{
    tmu f = tmus[n];
    if(rgbscale) f.rgb.scale = rgbscale;
    if(alphascale) f.alpha.scale = alphascale;
    committmu(n, f);
}

void colortmu(int n, float r, float g, float b, float a)
{
    tmu f = tmus[n];
    f.color[0] = r;
    f.color[1] = g;
    f.color[2] = b;
    f.color[3] = a;
    committmu(n, f);
}

void setuptmu(int n, const char *rgbfunc, const char *alphafunc)
{
    static tmu init = INITTMU;
    tmu f = tmus[n];

    f.mode = GL_COMBINE_ARB;
    if(rgbfunc) parsetmufunc(f.rgb, rgbfunc);
    else f.rgb = init.rgb;
    if(alphafunc) parsetmufunc(f.alpha, alphafunc);
    else f.alpha = init.alpha;

    committmu(n, f);
}

void inittmus()
{
    GLint val;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &val);
    hwtexsize = val;

    if(hasTE && !hasMT) maxtmus = 1;
    else if(hasTE && hasMT)
    {
        glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &val);
        maxtmus = max(1, min(MAXTMUS, int(val)));
        loopi(maxtmus)
        {
            glActiveTexture_(GL_TEXTURE0_ARB+i);
            resettmu(i);
        }
        glActiveTexture_(GL_TEXTURE0_ARB);
    }
}

void cleanuptmus()
{
    tmu invalidtmu = INVALIDTMU;
    loopi(MAXTMUS) tmus[i] = invalidtmu;
}


// only works on 32 bit surfaces with alpha in 4th byte!
void blitsurface(SDL_Surface *dst, SDL_Surface *src, int x, int y)
{
    uchar *dstp = (uchar *)dst->pixels + y*dst->pitch + x*4,
          *srcp = (uchar *)src->pixels;
    int dstpitch = dst->pitch - 4*src->w,
        srcpitch = src->pitch - 4*src->w;
    loop(dy, src->h)
    {
        loop(dx, src->w)
        {
            uint k1 = (255U - srcp[3]) * dstp[3], k2 = srcp[3] * 255U, kmax = max(dstp[3], srcp[3]), kscale = max(kmax * 255U, 1U);
            dstp[0] = (dstp[0]*k1 + srcp[0]*k2) / kscale;
            dstp[1] = (dstp[1]*k1 + srcp[1]*k2) / kscale;
            dstp[2] = (dstp[2]*k1 + srcp[2]*k2) / kscale;
            dstp[3] = kmax;
            dstp += 4;
            srcp += 4;
        }
        dstp += dstpitch;
        srcp += srcpitch;
    }
}

Texture *e_wall = NULL, *e_floor = NULL, *e_ceil = NULL;

void guidetoggle()
{
    if(player1->state == CS_EDITING)
    {
        Slot *sw = &slots[DEFAULT_WALL];
        Slot *sf = &slots[DEFAULT_FLOOR];
        Slot *sc = &slots[DEFAULT_CEIL];

        //if textures match original texture
        if(e_wall == NULL || e_floor == NULL || e_ceil == NULL)
        {
            //replace defaults with grid texures
            e_wall = sw->tex;
            e_floor = sf->tex;
            e_ceil = sc->tex;
            sw->tex = textureload("packages/textures/map_editor/wall.png");
            sf->tex = textureload("packages/textures/map_editor/floor.png");
            sc->tex = textureload("packages/textures/map_editor/ceil.png");
            conoutf("Guide: \f0on");
        }
        else
        {
            // restore textures
            if(e_wall) sw->tex = e_wall;
            if(e_floor) sf->tex = e_floor;
            if(e_ceil) sc->tex = e_ceil;
            e_wall = NULL;
            e_floor = NULL;
            e_ceil = NULL;
            conoutf("Guide: \fBoff");
        }
    }
    else
    {
        conoutf("\fBGuide view is only avaiable when editing.");
    }
}

COMMAND(guidetoggle, "");