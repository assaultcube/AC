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

#define TEXSCALEPREFIXSIZE 8
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

int fixcl(SDL_Surface *s, int threshold)
{
    if(!s || !s->h || !s->w) return 0;
    int sum = 0, w, a0 = *((uchar *) &s->format->Amask) ? 1 : 0;
    if(s->format->palette || s->format->BytesPerPixel < 3) s = (SDL_Surface *)s->pixels;
    for(int y = 0; y < s->h; y++)
    {
        uchar *pix = ((uchar *)s->pixels) + y * s->pitch + a0;
        w = 0;
        for(int x = 0; x < s->w; x++)
        {
            w += iabs(((pix[0] * pix[0]) | (pix[1] * pix[1]) | (pix[2] * pix[2])) - ((pix[0] * pix[1] * pix[2]) / 222));
            pix += s->format->BytesPerPixel;
        }
        sum += w / s->w;
    }
    sum = sqrtf(sum / s->h);
    int t = sum / threshold + 1;
    if(t > 1) for(int y = 0; y < s->h; y++)
    {
        uchar *pix = ((uchar *)s->pixels) + y * s->pitch + a0;
        for(int x = 0; x < s->w; x++)
        {
            loopi(3) pix[i] /= t;
            pix += s->format->BytesPerPixel;
        }
    }
    return t;
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
    const char *ffile = findfile(file, "rb");
    if(findfilelocation == FFL_ZIP)
    {
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
    }
    else if(!s) s = IMG_Load(ffile);
    if(!s)
    {
        if(trydl) requirepackage(PCK_TEXTURE, file);
        else if(!silent_texture_load) conoutf("couldn't load texture %s", texname);
        return 0;
    }
    s = fixsurfaceformat(s);
    if(strstr(texname,"playermodel")) { fixcl(s, 45); }
    else if(strstr(texname,"skin") && strstr(texname,"weapon")) { fixcl(s, 44); }

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
    defformatstring(pname)("%.7g        ", scale);
    copystring(pname + TEXSCALEPREFIXSIZE, name, MAXSTRLEN - TEXSCALEPREFIXSIZE);
    path(pname + TEXSCALEPREFIXSIZE);
    Texture *t = textures.access(pname);
    if(t) return t;
    int xs, ys, bpp;
    GLuint id = loadsurface(pname + TEXSCALEPREFIXSIZE, xs, ys, bpp, clamp, mipmap, canreduce, scale, trydl);
    if(!id) return notexture;
    char *key = newstring(pname);
    t = &textures[key];
    t->name = key + TEXSCALEPREFIXSIZE;
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

struct Slot
{
    string name;
    float scale, orgscale;
    Texture *tex;
    bool loaded;
};

vector<Slot> slots;

COMMANDF(texturereset, "", () { if(execcontext==IEXC_MAPCFG) slots.setsize(0); });

void _texture(Slot &s, float *scale, char *name)
{
    if(name)
    {
        filtertext(s.name, parentdir(name), FTXT__MEDIAFILEPATH); // filter parts separately, because filename may (legally) contain "<decal>"
        if(s.name[0] == '.' && s.name[1] == '/') memmove(s.name, s.name + 2, sizeof(s.name) - 2);
        if(*s.name) concatstring(s.name, "/");
        name = (char *)behindpath(name);
        if(*name == '<')
        {
            char *endcmd = strchr(name, '>');
            if(endcmd)
            {
                *endcmd = '\0';
                concatstring(s.name, name);
                concatstring(s.name, ">");
                name = endcmd + 1;
            }
        }
        filtertext(name, name, FTXT__MEDIAFILENAME);
        concatstring(s.name, name);
    }
    s.tex = NULL;
    s.loaded = false;
    if(scale)
    {
        s.orgscale = *scale;
        s.scale = (*scale > 0 && *scale <= 4.0f) ? *scale : 1.0f;
    }
}

void checktexturefilename(const char *name)
{
    if(execcontext == IEXC_MAPCFG && strstr(name, "map_editor"))
    {
        conoutf("\f3bad texture: %s", name);
        if(!_ignoreillegalpaths) { flagmapconfigerror(LWW_CONFIGERR * 4); scripterr(); }
    }
}

COMMANDF(texture, "fs", (float *scale, char *name)
{
    intret(slots.length());
    Slot &s = slots.add();
    _texture(s, scale, name);
    if(*scale < 0.0f || *scale > 4.0f)
    {
        conoutf("\f3texture slot #%d \"%s\" error: scale factor %.7g out of range 0..4", slots.length() - 1, name, *scale);
        flagmapconfigerror(LWW_CONFIGERR * 4);
        scripterr();
    }
    checktexturefilename(name);
    flagmapconfigchange();
});


const char *gettextureslot(int i)
{
    static string res;
    if(slots.inrange(i))
    {
        Slot &s = slots[i];
        formatstring(res)("texture %s \"%s\"", floatstr(s.orgscale, true), s.name);
        return res;
    }
    else return NULL;
}

Texture *lookuptexture(int tex, Texture *failtex, bool trydl)
{
    Texture *t = failtex;
    if(slots.inrange(tex))
    {
        Slot &s = slots[tex];
        if(!s.loaded)
        {
            defformatstring(pname)("packages/textures/%s", path(s.name, true));
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
    if(t.id) glDeleteTextures(1, &t.id);
    int xs = 1, ys = 1, bpp = 0;
    t.id = loadsurface(t.name, xs, ys, bpp, t.clamp, t.mipmap, t.canreduce, t.scale);
    t.xs = xs;
    t.ys = ys;
    t.bpp = bpp;
    return t.id!=0;
}

void reloadtextures()
{
    enumerate(textures, Texture, t, reloadtexture(t));
}

Texture *sky[6] = {NULL, NULL, NULL, NULL, NULL, NULL};

SVARF(loadsky, "", { loadskymap(false); });

bool loadskymap(bool reload)
{
    const char *side[] = { "lf", "rt", "ft", "bk", "dn", "up" };
    const char *legacyprefix = "textures/skymaps/";
    if(!reload)
    {
        int n = strlen(legacyprefix);
        filtertext(loadsky, loadsky, FTXT__MEDIAFILEPATH);
        checktexturefilename(loadsky);
        if(!strncmp(loadsky, legacyprefix, n))
        { // remove the prefix
            char *t = loadsky;
            loadsky = newstring(loadsky + n);
            delstring(t);
        }
        flagmapconfigchange();
    }
    bool old_silent_texture_load = silent_texture_load, success = true;
    if(autodownload && execcontext == IEXC_MAPCFG) silent_texture_load = true;
    loopi(6)
    {
        defformatstring(name)("packages/%s%s_%s.jpg", legacyprefix, loadsky, side[i]);
        sky[i] = textureload(name, 3);
        if(sky[i] == notexture && !reload && autodownload && execcontext == IEXC_MAPCFG)
        {
            requirepackage(PCK_SKYBOX, loadsky);
            break;
        }
        if(sky[i] == notexture && reload) success = false;
    }
    silent_texture_load = old_silent_texture_load;
    return success;
}

void loadnotexture(char *c)
{
    noworldtexture = notexture; // reset to default
    *mapconfigdata.notexturename = '\0';
    if(c[0])
    {
        checktexturefilename(c);
        filtertext(mapconfigdata.notexturename, c, FTXT__MEDIAFILEPATH);
        defformatstring(p)("packages/textures/%s", mapconfigdata.notexturename);
        noworldtexture = textureload(p);
        if(noworldtexture==notexture) { conoutf("could not load alternative texture '%s'.", p); flagmapconfigerror(LWW_CONFIGERR * 8); scripterr(); }
    }
    flagmapconfigchange();
}

COMMAND(loadnotexture, "s");
COMMANDF(getnotexture, "", () { result(mapconfigdata.notexturename); });

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
        conoutf("\fBGuide view is only available when editing.");
    }
}

COMMAND(guidetoggle, "");

void gettexturelist(char *_filter, char *_exclude, char *_exts) // create a list of texture paths and filenames
{
    vector<char *> filter, exclude, exts, files;
    vector<int> filter_n, exclude_n, exts_n;
    explodelist(_filter, filter);       // path has to start with one of these
    explodelist(_exclude, exclude);     // path may not start with one of these
    explodelist(_exts, exts);           // file needs to have one of those extensions (or ".jpg" by default)
    if(!exts.length()) exts.add(newstring(".jpg"));
    loopv(filter) filter_n.add((int) strlen(filter[i]));
    loopv(exclude) exclude_n.add((int) strlen(exclude[i]));
    loopv(exts) exts_n.add((int) strlen(exts[i]));
    listfilesrecursive("packages/textures", files);
    files.sort(stringsort);
    loopvrev(files) if(files.inrange(i + 1) && !strcmp(files[i], files[i + 1])) delstring(files.remove(i + 1)); // remove doubles
    int pn = strlen("packages/textures/");
    loopvrev(files)
    {
        bool filtered = filter.length() == 0, excluded = false, extmatch = false;
        if(!strncmp(files[i], "packages/textures/", pn))
        {
            const char *s = files[i] + pn;
            loopvj(filter) if(!strncmp(s, filter[j], filter_n[j])) filtered = true;
            loopvj(exclude) if(!strncmp(s, exclude[j], exclude_n[j])) excluded = true;
            int n = strlen(s);
            loopvj(exts) if(n > exts_n[j] && !strcmp(s + n - exts_n[j], exts[j])) extmatch = true;
        }
        if(!extmatch || !filtered || excluded) delstring(files.remove(i));
    }
    vector<char> res;
    bool cutprefix = filter.length() == 1;
    bool cutext = exts.length() == 1 && exts[0][0] != '.';
    loopv(files)
    {
        char *f = files[i] + pn;
        const char *p = parentdir(f), *b = behindpath(f);
        cvecprintf(res, "\"%s\" \"%s\"", p, b);  // always add columns for path (without "packages/textures/") and filename
        if(cutprefix) cvecprintf(res, " \"%s\"", int(strlen(p)) > filter_n[0] ? p + filter_n[0] : "");  // if there's exactly one filter string, add column with that string omitted
        if(cutext)
        {
            int n = strlen(b);
            if(n > exts_n[0]) ((char *)b)[n - exts_n[0]] = '\0';
            cvecprintf(res, " \"%s\"", b);  // if there's only one allowed extension and it does not start with '.', add column of filenames without extension
        }
        res.add('\n');
    }
    filter.deletearrays();
    exclude.deletearrays();
    exts.deletearrays();
    files.deletearrays();
    resultcharvector(res, -1);
}
COMMAND(gettexturelist, "sss");

void textureslotusagemapmodels(int *used)
{
    int old0 = used[0];
    loopv(ents) if(ents[i].type == MAPMODEL) used[ents[i].attr4]++;
    used[0] = old0;
}

void textureslotusagegeometry(int *used, int *visible = NULL, int *mostvisible = NULL)
{
    sqr *s = world;
    int *mostw = mostvisible ? new int[1024] : NULL, *mostc = mostvisible ? mostw + 256 : NULL, *mostf = mostvisible ? mostw + 512 : NULL, *mostu = mostvisible ? mostw + 768 : NULL;
    if(mostvisible) loopi(1024) mostw[i] = 0;
    if(visible) calcworldvisibility();
    loopirev(cubicsize)
    {
        used[s->wtex]++;
        if(s->type != SOLID)
        {
            used[s->ctex]++;
            used[s->ftex]++;
            used[s->utex]++;
        }
        if(visible && !(s->visible & INVISIBLE))
        { // cube not invisible
            if(!(s->visible & INVISWTEX))
            { // lower wall visible
                visible[s->wtex]++;
                if(mostw) mostw[s->wtex]++;
            }
            if(s->type != SOLID)
            {
                if(!(s->visible & INVISUTEX))
                { // upper wall visible
                    visible[s->utex]++;
                    if(mostu) mostu[s->utex]++;
                }
                visible[s->ctex]++;
                if(mostc) mostc[s->ctex]++;
                visible[s->ftex]++;
                if(mostf) mostf[s->ftex]++;
            }
        }
        s++;
    }
    if(visible) clearworldvisibility();
    if(mostvisible) loopk(4)
    { // find most visible texures (for all four geometry uses)
        int *mx[4] = { mostw, mostf, mostc, mostu }, *m = mx[k], maxval = 0;
        mostvisible[k] = -1;
        loopi(256) if(m[i] > maxval)
        {
            maxval = m[i];
            mostvisible[k] = i;
        }
    }
    if(mostw) delete[] mostw;
}

void textureslotusagelist(char *what)
{
    int used[256] =  { 0 }, visible[256];
    int mostvisible[4] = { 0 };
    bool onlymostvisible = !strcmp(what, "onlymostvisible");
    if(strcmp(what, "onlygeometry"))
    { // if not only geometry, count map model usage
        textureslotusagemapmodels(used);
    }
    loopi(256) visible[i] = used[i]; // all mapmodel uses count as "visible" as well
    if(strcmp(what, "onlymodels"))
    { // if not only models, count map geometry
        textureslotusagegeometry(used, visible, onlymostvisible ? mostvisible : NULL);
    }
    vector<char> res;
    if(onlymostvisible) cvecprintf(res, "wall %d floor %d ceiling %d \"upper wall\" %d ", mostvisible[0], mostvisible[1], mostvisible[2], mostvisible[3]);
    else loopi(256) cvecprintf(res, "%d %d ", used[i], visible[i]);
    resultcharvector(res, -1);
}
COMMAND(textureslotusagelist, "s");

bool testworldtexusage(int n)
{
    sqr *s = world;
    loopirev(cubicsize)
    {
        if(s->wtex == n || s->ctex == n || s->ftex == n || s->utex == n) return true;
        s++;
    }
    return false;
}

void textureslotusage(int *n) // returns all mapmodel entity indices that use a certain texture to skin the model
                              // if the texture is used in map geometry, at least a space is returned
                              // if the texture is unused, an empty string is returned
{
    string res = "";
    loopv(ents) if(ents[i].type == MAPMODEL && ents[i].attr4 == *n) concatformatstring(res, "%s%d", i ? " " : "", i);
    if(!*res)
    { // not used to skin a mapmodel: check world usage...
        if(testworldtexusage(*n)) copystring(res, " ");
    }
    result(res);
}
COMMAND(textureslotusage, "i");

void deletetextureslot(int *n, char *opt, char *_replace) // delete texture slot - only if unused or "purge" is specified
{
    EDITMP("deletetextureslot");
    if(!slots.inrange(*n)) return;
    bool purgeall = !strcmp(opt, "purge");
    bool mmused = false;
    loopv(ents) if(ents[i].type == MAPMODEL && ents[i].attr4 == *n) mmused = true;
    if(!purgeall && *n <= DEFAULT_CEIL) { conoutf("texture slots below #%d should usually not be deleted", DEFAULT_CEIL + 1); return; }
    if(!purgeall && (mmused || testworldtexusage(*n))) { conoutf("texture slot #%d is in use: can't delete", *n); return; }
    int deld = 0, replace = 256;
    if(purgeall && isdigit(*_replace)) replace = strtol(_replace, NULL, 0) & 255;
    if(replace > *n) replace--;
    sqr *s = world;
    loopirev(cubicsize)
    { // check, if cubes use the texture
        if(s->wtex == *n) { s->wtex = replace; deld++; }
        else if(s->wtex > *n) s->wtex--;
        if(s->ctex == *n) { s->ctex = replace; deld++; }
        else if(s->ctex > *n) s->ctex--;
        if(s->ftex == *n) { s->ftex = replace; deld++; }
        else if(s->ftex > *n) s->ftex--;
        if(s->utex == *n) { s->utex = replace; deld++; }
        else if(s->utex > *n) s->utex--;
        s++;
    }
    block bb = { clmapdims.x1 - 1, clmapdims.y1 - 1, clmapdims.xspan + 2, clmapdims.yspan + 2 };
    remip(bb);
    loopv(ents) if(ents[i].type == MAPMODEL)
    { // check, if mapmodels use the texture
        entity &e = ents[i];
        if(e.attr4 == *n)
        { // use default model texture instead
            e.attr4 = 0;
            deld++;
        }
        else if(e.attr4 > *n) e.attr4--; // adjust models in higher slots
    }
    loopk(3) // adjust texlists
    {
        int j = 255;
        loopi(256)
        {
            if(hdr.texlists[k][i] == *n) j = i;
            if(hdr.texlists[k][i] > *n) hdr.texlists[k][i]--;
        }
        if(j < 255) memmove(&hdr.texlists[k][j], &hdr.texlists[k][j + 1], 255 - j);
        hdr.texlists[k][255] = 255;
    }
    slots.remove(*n);
    defformatstring(m)(" (%d uses removed)", deld);
    if(replace != 255) formatstring(m)(" (%d uses changed to use slot #%d)", deld, replace);
    conoutf("texture slot #%d deleted%s", *n, deld ? m : "");
    unsavededits++;
    hdr.flags |= MHF_AUTOMAPCONFIG; // requires automapcfg
}
COMMAND(deletetextureslot, "iss");

void edittextureslot(int *n, char *scale, char *name) // edit slot parameters != ""
{
    string res = "";
    if(slots.inrange(*n))
    {
        Slot &s = slots[*n];
        if((*scale || *name) && !noteditmode("edittextureslot") && !multiplayer("edittextureslot"))
        { // change attributes
            float newscale = atof(scale);
            _texture(s, *scale ? &newscale : NULL, *name ? name : NULL);
            unsavededits++;
            hdr.flags |= MHF_AUTOMAPCONFIG; // requires automapcfg
        }
        formatstring(res)("%s \"%s\"", floatstr(s.orgscale, true), s.name);
    }
    result(res);
}
COMMAND(edittextureslot, "iss");

void gettextureorigin(char *fname)
{
    if(!*fname) return;
    defformatstring(s)("packages/textures/%s", fname);
    findfile(path(s), "r");
    const char *res = s;
    switch(findfilelocation)
    {
        case FFL_ZIP:     res = "zip";                                                break;
        case FFL_WORKDIR: res = fileexists(s, "r") ? "official" : "<file not found>"; break;
        case FFL_HOME:    res = "custom";                                             break;
        default:          formatstring(s)("package dir #%d", findfilelocation);       break;
    }
    result(res);
}
COMMAND(gettextureorigin, "s");

void textureslotbyname(char *name) // returns the slot(s) that a certain texture file is configured in
{
    string res = "";
    loopv(slots) if(!strcmp(name, slots[i].name)) concatformatstring(res, "%s%d", *res ? " " : "", i);
    result(res);
}
COMMAND(textureslotbyname, "s");


// helper functions to allow copy&paste between different maps while keeping the proper textures assigned
// (pasting will add any missing texture slots)

void *texconfig_copy()
{
    vector<Slot> *s = new vector<Slot>;
    loopv(slots) s->add(slots[i]);
    return (void *)s;
}

void texconfig_delete(void *s)
{
    delete (vector<Slot> *)s;
}

uchar *texconfig_paste(void *_s, uchar *usedslots) // create mapping table to translate pasted geometry to this maps texture list - add any missing textures
{
    vector<Slot> *s = (vector<Slot> *)_s;
    static uchar res[256];
    loopi(256) res[i] = i;
    loopi(256) if(usedslots[i] && s->inrange(i))
    {
        Slot &os = (*s)[i];
        bool found = false;
        loopvj(slots)
        {
            if(slots[j].scale == os.scale && !strcmp(slots[j].name, os.name))
            {
                res[i] = j;
                found = true;
                break;
            }
        }
        if(!found)
        {
            if(slots.length() < 255)
            {
                res[i] = slots.length();
                _texture(slots.add(), &os.scale, os.name);
                conoutf("added texture \"%s\" (scale %.7g) in slot #%d", os.name, os.scale, res[i]);
                if(!(hdr.flags & MHF_AUTOMAPCONFIG)) automapconfig(); // make sure, the new texture slots will be saved with the map
            }
            else conoutf("\f3failed to add texture \"%s\" (scale %.7g): no texture slot available", os.name, os.scale);
        }
    }
    return res;
}

struct temptexslot { Slot s; vector<int> oldslot; bool used; };

int tempslotsort(temptexslot *a, temptexslot *b)
{
    int n = strcmp(a->s.name, b->s.name);
    if(n) return n;
    if(a->s.scale < b->s.scale) return -1;
    if(a->s.scale > b->s.scale) return 1;
    return a->oldslot[0] - b->oldslot[0];
}

int tempslotunsort(temptexslot *a, temptexslot *b)
{
    return a->oldslot[0] - b->oldslot[0];
}

void sorttextureslots(char **args, int numargs)
{
    bool nomerge = false, mergeused = false, nosort = false, unknownarg = false;
    vector<int> presort;
    presort.add(0); // skymap needs to stay on #0
    loopi(numargs) if(args[i][0])
    {
        if(!strcasecmp(args[i], "nomerge")) nomerge = true;
        else if(!strcasecmp(args[i], "nosort")) nosort = true;
        else if(!strcasecmp(args[i], "mergeused")) mergeused = true;
        else if(isdigit(args[i][0])) presort.add(ATOI(args[i]));
        else { conoutf("sorttextureslots: unknown argument \"%s\"", args[i]); unknownarg = true; }
    }
    while(presort.length() < 5) presort.add(presort.length());

    EDITMP("sorttextureslots");
    if(unknownarg || slots.length() < 5) return;

    int used[256] = { 0 };
    textureslotusagemapmodels(used);
    textureslotusagegeometry(used);

    vector<temptexslot> tempslots;
    loopv(slots)
    {
        tempslots.add().s = slots[i];
        tempslots.last().oldslot.add(i);
        tempslots.last().used = used[i] > 0;
    }
    tempslots.sort(tempslotsort);

    // remove double entries (if requested)
    if(!nomerge) loopvrev(tempslots) if(i > 0)
    {
        temptexslot &s1 = tempslots[i], &s0 = tempslots[i - 1];
        if(s1.oldslot[0] > 4 && s0.oldslot[0] > 0 && !strcmp(s0.s.name, s1.s.name) && s0.s.scale == s1.s.scale && (mergeused || !s0.used || !s1.used))
        {
            if(s1.used) s0.used = true;
            loopvj(s1.oldslot) s0.oldslot.add(s1.oldslot[j]);
            tempslots.remove(i);
        }
    }

    // sort special textures (like skymap) back front - also, revert sorting, if it's unwanted
    if(nosort) tempslots.sort(tempslotunsort);
    loopv(presort) for(int n = i + 1; n < presort.length(); n++) if(presort[i] == presort[n]) presort.remove(n); // first occurrence is kept
    loopvk(presort)
    {
        loopv(tempslots)
        {
            bool yep = false;
            loopvj(tempslots[i].oldslot) if(tempslots[i].oldslot[j] == presort[k]) yep = true;
            if(yep && i > k)
            {
                temptexslot t = tempslots.remove(i);
                tempslots.insert(k, t);
                break;
            }
        }
    }

    // create translation table
    uchar newslot[256];
    loopk(256) newslot[k] = k;
    loopv(tempslots)
    {
        temptexslot &t = tempslots[i];
        loopvj(t.oldslot)
        {
            if(t.oldslot[j] < 256) newslot[t.oldslot[j]] = i;
        }
    }

    // translate all texture entries
    sqr *s = world;
    loopirev(cubicsize)
    {
        s->wtex = newslot[s->wtex];
        s->ctex = newslot[s->ctex];
        s->ftex = newslot[s->ftex];
        s->utex = newslot[s->utex];
        s++;
    }
    block bb = { clmapdims.x1 - 1, clmapdims.y1 - 1, clmapdims.xspan + 2, clmapdims.yspan + 2 };
    remip(bb);
    loopv(ents) if(ents[i].type == MAPMODEL)
    {
        entity &e = ents[i];
        if(e.attr4) e.attr4 = newslot[e.attr4];
    }

    // translate texture lists
    loopk(3)
    {
        uchar *ps = hdr.texlists[k], *pd = ps, d[256], x;
        loopi(256) d[i] = 1;
        loopi(256)
        {
            x = newslot[*ps++];
            if(d[x] && x < tempslots.length())
            {
                d[x] = 0;
                *pd++ = x;
            }
        }
        loopi(256) if(d[i]) *pd++ = i;
    }

    conoutf("%d texture slots%s, %d %sslots merged", tempslots.length(), nosort ? "" : " sorted", slots.length() - tempslots.length(), mergeused ? "" : "unused ");

    // rewrite texture slot list
    slots.shrink(tempslots.length());
    loopv(tempslots)
    {
        temptexslot &t = tempslots[i];
        _texture(slots[i], &t.s.orgscale, t.s.name);
    }
    unsavededits++;
    hdr.flags |= MHF_AUTOMAPCONFIG; // requires automapcfg
}
COMMAND(sorttextureslots, "v");


#ifdef _DEBUG
void resettexturelists()
{
    loopk(3) loopi(256) hdr.texlists[k][i] = i;
}
COMMAND(resettexturelists, "");
#endif
