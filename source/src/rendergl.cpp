// rendergl.cpp: core opengl rendering stuff

#include "cube.h"

#ifdef DARWIN
#define GL_COMBINE_EXT GL_COMBINE_ARB
#define GL_COMBINE_RGB_EXT GL_COMBINE_RGB_ARB
#define GL_SOURCE0_RBG_EXT GL_SOURCE0_RGB_ARB
#define GL_SOURCE1_RBG_EXT GL_SOURCE1_RGB_ARB
#define GL_RGB_SCALE_EXT GL_RGB_SCALE_ARB
#endif

bool hasoverbright = false;

int glmaxtexsize = 256;

void gl_init(int w, int h, int bpp, int depth, int fsaa)
{
    //#define fogvalues 0.5f, 0.6f, 0.7f, 1.0f

    glViewport(0, 0, w, h);
    glClearDepth(1.0);
    glDepthFunc(GL_LESS);
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    
    
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_DENSITY, 0.25);
    glHint(GL_FOG_HINT, GL_NICEST);
    

    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-3.0, -3.0);

    glCullFace(GL_FRONT);
    glEnable(GL_CULL_FACE);

    char *exts = (char *)glGetString(GL_EXTENSIONS);
    
    if(strstr(exts, "GL_EXT_texture_env_combine")) hasoverbright = true;
    else conoutf("WARNING: cannot use overbright lighting, using old lighting model!");
        
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &glmaxtexsize);

    GLUquadricObj *qsphere = gluNewQuadric();
    if(!qsphere) fatal("glu sphere");
    gluQuadricDrawStyle(qsphere, GLU_FILL);
    gluQuadricOrientation(qsphere, GLU_INSIDE);
    gluQuadricTexture(qsphere, GL_TRUE);
    glNewList(1, GL_COMPILE);
    gluSphere(qsphere, 1, 12, 6);
    glEndList();
    gluDeleteQuadric(qsphere);

    if(fsaa) glEnable(GL_MULTISAMPLE);
};

Texture *crosshair = NULL;
hashtable<char *, Texture> textures;

void cleangl()
{
};

GLuint installtex(const char *texname, int &xs, int &ys, bool clamp, bool highqual)
{
    SDL_Surface *s = IMG_Load(texname);
    if(!s) { conoutf("couldn't load texture %s", texname); return 0; };
    if(s->format->BitsPerPixel!=24 && s->format->BitsPerPixel!=32) { conoutf("texture must be 24bpp or 32bpp: %s", texname); return 0; };
    // loopi(s->w*s->h*3) { uchar *p = (uchar *)s->pixels+i; *p = 255-*p; };  
    GLuint tnum;
    glGenTextures(1, &tnum);
    glBindTexture(GL_TEXTURE_2D, tnum);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, highqual ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR); //NEAREST);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE); 
    xs = s->w;
    ys = s->h;
    while(xs>glmaxtexsize || ys>glmaxtexsize) { xs /= 2; ys /= 2; };
    int mode = s->format->BitsPerPixel==24 ? GL_RGB : GL_RGBA;
    uchar *scaledimg = (uchar *)s->pixels;
    if(xs!=s->w)
    {
        conoutf("warning: quality loss: scaling %s", texname);     // for voodoo cards under linux
        scaledimg = new uchar[xs*ys*s->format->BitsPerPixel/8];
        if(gluScaleImage(mode, s->w, s->h, GL_UNSIGNED_BYTE, s->pixels, xs, ys, GL_UNSIGNED_BYTE, scaledimg))
        {
            xs = s->w;
            ys = s->h;
        };
    };
    if(gluBuild2DMipmaps(GL_TEXTURE_2D, mode, xs, ys, mode, GL_UNSIGNED_BYTE, scaledimg)) fatal("could not build mipmaps");
    if(xs!=s->w) delete[] scaledimg;
    SDL_FreeSurface(s);
    return tnum;
};

// management of texture slots
// each texture slot can have multople texture frames, of which currently only the first is used
// additional frames can be used for various shaders

Texture *textureload(const char *name, bool clamp, bool highqual)
{
    string pname;
    s_strcpy(pname, name);
    path(pname);
    Texture *t = textures.access(pname);
    if(t) return t;
    int xs, ys;
    GLuint id = installtex(pname, xs, ys, clamp, highqual);
    if(!id) return crosshair;
    char *key = newstring(pname);
    t = &textures[key];
    t->name = key;
    t->xs = xs;
    t->ys = ys;
    t->id = id;
    return t;
};  
    
struct Slot
{   
    string name;
    Texture *tex; 
    bool loaded;
};
    
vector<Slot> slots;
    
void texturereset() { slots.setsizenodelete(0); };

void texture(char *aframe, char *name)
{   
    Slot &s = slots.add();
    s_strcpy(s.name, name);
    path(s.name);
    s.tex = NULL;
    s.loaded = false;
};

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
            s_sprintfd(pname)("packages%ctextures%c%s", PATHDIV, PATHDIV, s.name);
            s.tex = textureload(pname);
            s.loaded = true;
        };
        if(s.tex) t = s.tex;
    };

    xs = t->xs;
    ys = t->ys;
    return t->id;
};

void setupworld()
{
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY); 
    setarraypointers();

    if(hasoverbright)
    {
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT); 
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT);
    };
};

struct strip { int start, num; };

struct stripbatch
{
    int tex;
    vector<strip> strips;
};

vector<strip> skystrips;
stripbatch stripbatches[256];
uchar renderedtex[256];
int renderedtexs = 0;

void renderstripssky()
{
    if(skystrips.empty()) return;
    int xs, ys;
    glBindTexture(GL_TEXTURE_2D, lookuptexture(DEFAULT_SKY, xs, ys));
    loopv(skystrips) glDrawArrays(GL_TRIANGLE_STRIP, skystrips[i].start, skystrips[i].num);
    skystrips.setsizenodelete(0);
};

void renderstrips()
{
    int xs, ys;
    loopj(renderedtexs)
    {
        stripbatch &sb = stripbatches[j];
        glBindTexture(GL_TEXTURE_2D, lookuptexture(sb.tex, xs, ys));
        loopv(sb.strips) glDrawArrays(GL_TRIANGLE_STRIP, sb.strips[i].start, sb.strips[i].num);
        sb.strips.setsizenodelete(0);
    };
    renderedtexs = 0;
};

void overbright(float amount) { if(hasoverbright) glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, amount ); };

void addstrip(int tex, int start, int n)
{
    if(tex==DEFAULT_SKY)
    {
        strip &s = skystrips.add();
        s.start = start;
        s.num = n;
        return;
    };
    stripbatch *sb = &stripbatches[renderedtex[tex]];
    if(sb->tex!=tex || sb>=&stripbatches[renderedtexs]) 
    {
        sb = &stripbatches[renderedtex[tex] = renderedtexs++];
        sb->tex = tex;
    };
    strip &s = sb->strips.add();
    s.start = start;
    s.num = n;
};

VARF(gamma, 30, 100, 300,
{
    float f = gamma/100.0f;
    if(SDL_SetGamma(f,f,f)==-1)
    {
        conoutf("Could not set gamma (card/driver doesn't support it?)");
        conoutf("sdl: %s", SDL_GetError());
    };
});

void trypitch(int i) { player1->pitch = (float)i; }
COMMAND(trypitch, ARG_1INT);

void transplayer()
{
    glLoadIdentity();
    
    if(player1->state==CS_DEAD)
    {
        float t = (lastmillis-player1->lastaction)/1000.0f;
        if(t >= 1.6f) t = 1.6f;
        
        player1->pitch = (float) sin(t)*(-70.0f-player1->oldpitch)+player1->oldpitch;
        glRotated(player1->pitch,-1.0,0.0,0.0);
        glRotated(player1->yaw,0.0,1.0,0.0);

        glTranslated(-player1->o.x, player1->eyeheight-sin(t)*7.0f-player1->o.z, -player1->o.y);
		//glTranslated(-player1->o.x,  -player1->o.z, -player1->o.y); 
    }
    else
    {
        glRotated(player1->roll,0.0,0.0,1.0);
        glRotated(player1->pitch,-1.0,0.0,0.0);
        glRotated(player1->yaw,0.0,1.0,0.0);

        glTranslated(-player1->o.x,  -player1->o.z, -player1->o.y); 
    };  
};

VAR(fov, 90, 105, 120);

int xtraverts;

VAR(fog, 64, 180, 1024);
VAR(fogcolour, 0, 0x8099B3, 0xFFFFFF);

VAR(hudgun,0,1,1);

char *hudgunnames[] = { "knife", "pistol", "shotgun", "subgun", "sniper", "assault", "grenade"};

void drawhudmodel(int start, int end, float speed, int base)
{
    s_sprintfd(mdl)("hudguns/%s", hudgunnames[player1->gunselect]);
    rendermodel(mdl, start, end, 0, 1.0f, player1->o.x, player1->o.z, player1->o.y, player1->yaw+90, player1->pitch, false, 1.0f, speed, 0, base);
};


void drawhudgun(int w, int h, float aspect, int farplane)
{
    if(scoped && player1->gunselect==GUN_SNIPER) return;
    
    glEnable(GL_CULL_FACE);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective((float)100.0f*h/w, aspect, 0.3f, farplane); // fov fixed at 100°
    glMatrixMode(GL_MODELVIEW);
   
    if(hudgun && player1->state!=CS_DEAD)rendermd3gun();
    rendermenumdl();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective((float)fov*h/w, aspect, 0.15f, farplane);
    glMatrixMode(GL_MODELVIEW);

    glDisable(GL_CULL_FACE);
};

void gl_drawframe(int w, int h, float changelod, float curfps)
{
    float hf = hdr.waterlevel-0.3f;
    float fovy = (float)fov*h/w;
    float aspect = w/(float)h;
    bool underwater = player1->o.z<hf;
    
    glFogi(GL_FOG_START, (fog+64)/8);
    glFogi(GL_FOG_END, fog);
    float fogc[4] = { (fogcolour>>16)/256.0f, ((fogcolour>>8)&255)/256.0f, (fogcolour&255)/256.0f, 1.0f };
    glFogfv(GL_FOG_COLOR, fogc);
    glClearColor(fogc[0], fogc[1], fogc[2], 1.0f);

    if(underwater)
    {
        fovy += (float)sin(lastmillis/1000.0)*2.0f;
        aspect += (float)sin(lastmillis/1000.0+PI)*0.1f;
        glFogi(GL_FOG_START, 0);
        glFogi(GL_FOG_END, (fog+96)/8);
    };
    
    glClear((player1->outsidemap ? GL_COLOR_BUFFER_BIT : 0) | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    int farplane = fog*5/2;
    gluPerspective(fovy, aspect, 0.15f, farplane);
    glMatrixMode(GL_MODELVIEW);

    transplayer();

    glEnable(GL_TEXTURE_2D);
    
    resetcubes();
            
    render_world(player1->o.x, player1->o.y, player1->o.z, changelod,
            (int)player1->yaw, (int)player1->pitch, (float)fov, w, h);
    finishstrips();

    setupworld();

    renderstripssky();

    glLoadIdentity();
    glRotated(player1->pitch, -1.0, 0.0, 0.0);
    glRotated(player1->yaw,   0.0, 1.0, 0.0);
    glRotated(90.0, 1.0, 0.0, 0.0);
    glColor3f(1.0f, 1.0f, 1.0f);
    glDisable(GL_FOG);
    glDepthFunc(GL_GREATER);
    draw_envbox(fog*4/3);
    glDepthFunc(GL_LESS);
    glEnable(GL_FOG);

    transplayer();
        
    overbright(2);
    
    renderstrips();

    xtraverts = 0;

    renderclients();
    //monsterrender();
    BotManager.RenderBots(); // Added by Rick

    renderentities();

    renderspheres(curtime);
    renderents();

    renderphysents();
    
    rendershotlines();

    glDisable(GL_CULL_FACE);

    // Added by Rick: Need todo here because of drawing the waypoints
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    WaypointClass.Think();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    // end add
    
    drawhudgun(w, h, aspect, farplane);

    overbright(1);
    int nquads = renderwater(hf);
    
    overbright(2);
    render_particles(curtime);
    overbright(1);

    glDisable(GL_FOG);

    glDisable(GL_TEXTURE_2D);

    extern vector<vertex> verts;
    gl_drawhud(w, h, (int)curfps, nquads, verts.length(), underwater);

    glEnable(GL_CULL_FACE);
    glEnable(GL_FOG);
};
