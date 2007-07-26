// texture.cpp: texture management

#include "cube.h"

Texture *crosshair = NULL;
hashtable<char *, Texture> textures;

VAR(maxtexsize, 0, -1, 4096);

void createtexture(int tnum, int w, int h, void *pixels, int clamp, bool mipmap, GLenum format)
{
    glBindTexture(GL_TEXTURE_2D, tnum);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp&1 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp&2 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);

    int tw = w, th = h;
    if(maxtexsize<0) glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint *)&maxtexsize);
    if(maxtexsize) while(tw>maxtexsize || th>maxtexsize) { tw /= 2; th /= 2; }
    if(tw!=w || th!=h)
    {
        if(gluScaleImage(format, w, h, GL_UNSIGNED_BYTE, pixels, tw, th, GL_UNSIGNED_BYTE, pixels))
        {
            tw = w;
            th = h;
        }
    }
    if(mipmap)
    {
        if(gluBuild2DMipmaps(GL_TEXTURE_2D, format, tw, th, format, GL_UNSIGNED_BYTE, pixels)) fatal("could not build mipmaps");
    }
    else glTexImage2D(GL_TEXTURE_2D, 0, format, tw, th, 0, format, GL_UNSIGNED_BYTE, pixels);
}

GLuint loadsurface(const char *texname, int &xs, int &ys, int clamp)
{
    SDL_Surface *s = IMG_Load(findfile(texname, "rb"));
    if(!s) { conoutf("couldn't load texture %s", texname); return 0; }
    if(s->format->BitsPerPixel!=24 && s->format->BitsPerPixel!=32)
    {
        SDL_FreeSurface(s);
        conoutf("texture must be 24bpp or 32bpp: %s", texname);
        return 0;
    }
    GLuint tnum;
    glGenTextures(1, &tnum);
    createtexture(tnum, s->w, s->h, s->pixels, clamp, true, s->format->BitsPerPixel==24 ? GL_RGB : GL_RGBA);
    xs = s->w;
    ys = s->h;
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
    int xs, ys; 
    GLuint id = loadsurface(pname, xs, ys, clamp);
    if(!id) return crosshair;
    char *key = newstring(pname);
    t = &textures[key];
    t->name = key;
    t->xs = xs;
    t->ys = ys;
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

int lookuptexture(int tex, int &xs, int &ys)
{
    Texture *t = crosshair;
    if(slots.inrange(tex))
    {
        Slot &s = slots[tex];
        if(!s.loaded)
        {
            s_sprintfd(pname)("packages/textures/%s", s.name);
            s.tex = textureload(pname);
            s.loaded = true;
        }
        if(s.tex) t = s.tex;
    }

    xs = t->xs;
    ys = t->ys;
    return t->id;
}

Texture *sky[6] = {NULL, NULL, NULL, NULL, NULL, NULL};

void loadsky(char *basename)
{
    char *side[] = { "ft", "bk", "lf", "rt", "dn", "up" };
    loopi(6)
    {
        s_sprintfd(name)("packages/%s_%s.jpg", basename, side[i]);
        sky[i] = textureload(name, 3);
        if(!sky[i]) conoutf("could not load sky texture: %s", name);
    }
}   

COMMAND(loadsky, ARG_1STR);

void draw_envbox_face(float s0, float t0, int x0, int y0, int z0,
                      float s1, float t1, int x1, int y1, int z1,
                      float s2, float t2, int x2, int y2, int z2,
                      float s3, float t3, int x3, int y3, int z3,
                      Texture *tex)
{
    glBindTexture(GL_TEXTURE_2D, tex->id);
    glBegin(GL_QUADS);
    glTexCoord2f(s3, t3); glVertex3i(x3, y3, z3);
    glTexCoord2f(s2, t2); glVertex3i(x2, y2, z2);
    glTexCoord2f(s1, t1); glVertex3i(x1, y1, z1);
    glTexCoord2f(s0, t0); glVertex3i(x0, y0, z0);
    glEnd();
    xtraverts += 4;
}

void draw_envbox(int w)
{
    if(!sky[0]) return;

    glDepthMask(GL_FALSE);

    draw_envbox_face(1.0f, 1.0f, -w, -w,  w,
                     0.0f, 1.0f,  w, -w,  w,
                     0.0f, 0.0f,  w, -w, -w, 
                     1.0f, 0.0f, -w, -w, -w, sky[0]);

    draw_envbox_face(1.0f, 1.0f, +w,  w,  w,
                     0.0f, 1.0f, -w,  w,  w,
                     0.0f, 0.0f, -w,  w, -w, 
                     1.0f, 0.0f, +w,  w, -w, sky[1]);

    draw_envbox_face(0.0f, 0.0f, -w, -w, -w,
                     1.0f, 0.0f, -w,  w, -w,
                     1.0f, 1.0f, -w,  w,  w, 
                     0.0f, 1.0f, -w, -w,  w, sky[2]);

    draw_envbox_face(1.0f, 1.0f, +w, -w,  w,
                     0.0f, 1.0f, +w,  w,  w,
                     0.0f, 0.0f, +w,  w, -w, 
                     1.0f, 0.0f, +w, -w, -w, sky[3]);

    draw_envbox_face(0.0f, 1.0f, -w,  w,  w,
                     0.0f, 0.0f, +w,  w,  w,
                     1.0f, 0.0f, +w, -w,  w, 
                     1.0f, 1.0f, -w, -w,  w, sky[4]);

    draw_envbox_face(0.0f, 1.0f, +w,  w, -w,
                     0.0f, 0.0f, -w,  w, -w,
                     1.0f, 0.0f, -w, -w, -w, 
                     1.0f, 1.0f, +w, -w, -w, sky[5]);

    glDepthMask(GL_TRUE);
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
    { 0, { 0, 0, 0, }, { 0, 0, 0 }, 0 }, \
    { 0, { 0, 0, 0, }, { 0, 0, 0 }, 0 } \
}

#define INITTMU \
{ \
    GL_MODULATE, \
    { 0, 0, 0, 0 }, \
    { GL_MODULATE, { GL_TEXTURE, GL_PREVIOUS_ARB, GL_CONSTANT_ARB }, { GL_SRC_COLOR, GL_SRC_COLOR, GL_SRC_ALPHA }, 1 }, \
    { GL_MODULATE, { GL_TEXTURE, GL_PREVIOUS_ARB, GL_CONSTANT_ARB }, { GL_SRC_ALPHA, GL_SRC_ALPHA, GL_SRC_ALPHA }, 1 } \
}

tmu tmus[4] =
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
    if(hasTE && !hasMT) maxtmus = 1;
    else if(hasTE && hasMT)
    {
        glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, (GLint *)&maxtmus);
        maxtmus = max(1, min(4, maxtmus));
        loopi(maxtmus)
        {
            glActiveTexture_(GL_TEXTURE0_ARB+i);
            resettmu(i);
        }
        glActiveTexture_(GL_TEXTURE0_ARB);
    }
}

