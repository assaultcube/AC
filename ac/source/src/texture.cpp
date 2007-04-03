// texture.cpp: texture management

#include "cube.h"

Texture *crosshair = NULL;
hashtable<char *, Texture> textures;

void createtexture(int tnum, int w, int h, void *pixels, int clamp, bool mipmap, GLenum format)
{
    glBindTexture(GL_TEXTURE_2D, tnum);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp&1 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp&2 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    extern int maxtexsize;
    int tw = w, th = h;
    if(maxtexsize) while(tw>maxtexsize || th>maxtexsize) { tw /= 2; th /= 2; }
    if(tw!=w)
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
    SDL_Surface *s = IMG_Load(texname);
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

