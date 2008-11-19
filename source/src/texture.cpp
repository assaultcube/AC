// texture.cpp: texture management

#include "pch.h"
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
VARFP(maxtexsize, 0, 0, 1<<12, initwarning("texture quality", INIT_LOAD));
VARFP(trilinear, 0, 1, 1, initwarning("texture filtering", INIT_LOAD));
VARFP(bilinear, 0, 1, 1, initwarning("texture filtering", INIT_LOAD));

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

void resizetexture(int w, int h, bool mipmap, GLenum target, int &tw, int &th)
{
    int hwlimit = hwtexsize,
        sizelimit = mipmap && maxtexsize ? min(maxtexsize, hwlimit) : hwlimit;
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

void createtexture(int tnum, int w, int h, void *pixels, int clamp, bool mipmap, GLenum format)
{
    glBindTexture(GL_TEXTURE_2D, tnum);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp&1 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp&2 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
        mipmap ?
            (trilinear ?
                (bilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR) :
                (bilinear ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST)) :
            (bilinear ? GL_LINEAR : GL_NEAREST));

    int tw = w, th = h;
    if(pixels) resizetexture(w, h, mipmap, GL_TEXTURE_2D, tw, th);
    uploadtexture(GL_TEXTURE_2D, format, tw, th, format, GL_UNSIGNED_BYTE, pixels, w, h, mipmap);
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

GLuint loadsurface(const char *texname, int &xs, int &ys, int &bpp, int clamp)
{
    const char *file = texname;
    if(texname[0]=='<')
    {
        file = strchr(texname, '>');
        if(!file) { conoutf("could not load texture %s", texname); return 0; }
        file++;
    }

    SDL_Surface *s = IMG_Load(findfile(file, "rb"));
    if(!s) { conoutf("couldn't load texture %s", texname); return 0; }
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

    GLuint tnum;
    glGenTextures(1, &tnum);
    createtexture(tnum, s->w, s->h, s->pixels, clamp, true, format);
    xs = s->w;
    ys = s->h;
    bpp = s->format->BitsPerPixel;
    SDL_FreeSurface(s);
    return tnum;
}

// management of texture slots
// each texture slot can have multople texture frames, of which currently only the first is used
// additional frames can be used for various shaders

Texture *textureload(const char *name, int clamp)
{
    string pname;
    s_strcpy(pname, name);
    path(pname);
    Texture *t = textures.access(pname);
    if(t) return t;
    int xs, ys, bpp;
    GLuint id = loadsurface(pname, xs, ys, bpp, clamp);
    if(!id) return notexture;
    char *key = newstring(pname);
    t = &textures[key];
    t->name = key;
    t->xs = xs;
    t->ys = ys;
    t->bpp = bpp;
    t->clamp = clamp;
    t->id = id;
    return t;
}

struct Slot
{
    string name;
    Texture *tex;
    bool loaded;
};

vector<Slot> slots;

void texturereset() { slots.setsizenodelete(0); }

void texture(char *aframe, char *name)
{
    Slot &s = slots.add();
    s_strcpy(s.name, name);
    path(s.name);
    s.tex = NULL;
    s.loaded = false;
}

COMMAND(texturereset, ARG_NONE);
COMMAND(texture, ARG_2STR);

Texture *lookuptexture(int tex, Texture *failtex)
{
    Texture *t = failtex;
    if(slots.inrange(tex))
    {
        Slot &s = slots[tex];
        if(!s.loaded)
        {
            s_sprintfd(pname)("packages/textures/%s", s.name);
            s.tex = textureload(pname);
            if(s.tex==notexture) s.tex = failtex;
            s.loaded = true;
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
    t.id = loadsurface(t.name, xs, ys, bpp, t.clamp);
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

void loadsky(char *basename)
{
    const char *side[] = { "lf", "rt", "ft", "bk", "dn", "up" };
    loopi(6)
    {
        s_sprintfd(name)("packages/%s_%s.jpg", basename, side[i]);
        sky[i] = textureload(name, 3);
        if(!sky[i]) conoutf("could not load sky texture: %s", name);
    }
}

COMMAND(loadsky, ARG_1STR);

void loadnotexture(char *c)
{
    noworldtexture = notexture; // reset to default
    if(c[0])
    {
        s_sprintfd(p)("packages/textures/%s", c);
        noworldtexture = textureload(p);
        if(noworldtexture==notexture) conoutf("could not load alternative texture '%s'.", p);
    }
}
COMMAND(loadnotexture, ARG_1STR);

void draw_envbox_face(float s0, float t0, float x0, float y0, float z0,
                      float s1, float t1, float x1, float y1, float z1,
                      float s2, float t2, float x2, float y2, float z2,
                      float s3, float t3, float x3, float y3, float z3,
                      Texture *tex)
{
    glBindTexture(GL_TEXTURE_2D, tex->id);
    glBegin(GL_QUADS);
    glTexCoord2f(s3, t3); glVertex3f(x3, y3, z3);
    glTexCoord2f(s2, t2); glVertex3f(x2, y2, z2);
    glTexCoord2f(s1, t1); glVertex3f(x1, y1, z1);
    glTexCoord2f(s0, t0); glVertex3f(x0, y0, z0);
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

