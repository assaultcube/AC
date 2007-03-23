// rendergl.cpp: core opengl rendering stuff

#include "cube.h"
#include "bot/bot.h"

#ifdef __APPLE__
	#define GL_COMBINE_EXT GL_COMBINE_ARB
	#define GL_COMBINE_RGB_EXT GL_COMBINE_RGB_ARB
	#define GL_SOURCE0_RGB_EXT GL_SOURCE0_RGB_ARB
	#define GL_SOURCE1_RGB_EXT GL_SOURCE1_RGB_ARB
	#define GL_RGB_SCALE_EXT GL_RGB_SCALE_ARB
	#define GL_PRIMARY_COLOR_EXT GL_PRIMARY_COLOR_ARB
#endif

bool hasoverbright = false;

VAR(maxtexsize, 0, 0, 4096);

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
	else if(strstr(exts, "GL_ARB_texture_env_combine")) hasoverbright = true;
    else conoutf("WARNING: cannot use overbright lighting, using old lighting model!");
        
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtexsize);

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

    camera1 = player1;
};

Texture *crosshair = NULL;
hashtable<char *, Texture> textures;

void cleangl()
{
};

void createtexture(int tnum, int w, int h, void *pixels, int clamp, GLenum format)
{
    glBindTexture(GL_TEXTURE_2D, tnum);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp&1 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp&2 ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    
    int tw = w, th = h;
    if(maxtexsize) while(tw>maxtexsize || th>maxtexsize) { tw /= 2; th /= 2; };
    if(tw!=w)
    { 
        if(gluScaleImage(format, w, h, GL_UNSIGNED_BYTE, pixels, tw, th, GL_UNSIGNED_BYTE, pixels))
        {
            tw = w;
            th = h;
        };
    };
    if(gluBuild2DMipmaps(GL_TEXTURE_2D, format, tw, th, format, GL_UNSIGNED_BYTE, pixels)) fatal("could not build mipmaps");
};

GLuint loadsurface(const char *texname, int &xs, int &ys, int clamp)
{
    SDL_Surface *s = IMG_Load(texname);
    if(!s) { conoutf("couldn't load texture %s", texname); return 0; };
    if(s->format->BitsPerPixel!=24 && s->format->BitsPerPixel!=32) 
    { 
        SDL_FreeSurface(s); 
        conoutf("texture must be 24bpp or 32bpp: %s", texname); 
        return 0; 
    };
    GLuint tnum;
    glGenTextures(1, &tnum);
    createtexture(tnum, s->w, s->h, s->pixels, clamp, s->format->BitsPerPixel==24 ? GL_RGB : GL_RGBA);
    xs = s->w;
    ys = s->h;
    SDL_FreeSurface(s);
    return tnum;
};

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
            s_sprintfd(pname)("packages/textures/%s", s.name);
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

struct strip { int type, start, num; };

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
    loopv(skystrips) glDrawArrays(skystrips[i].type, skystrips[i].start, skystrips[i].num);
    skystrips.setsizenodelete(0);
};

void renderstrips()
{
    int xs, ys;
    loopj(renderedtexs)
    {
        stripbatch &sb = stripbatches[j];
        glBindTexture(GL_TEXTURE_2D, lookuptexture(sb.tex, xs, ys));
        loopv(sb.strips) glDrawArrays(sb.strips[i].type, sb.strips[i].start, sb.strips[i].num);
        sb.strips.setsizenodelete(0);
    };
    renderedtexs = 0;
};

void overbright(float amount) { if(hasoverbright) glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, amount ); };

void addstrip(int type, int tex, int start, int n)
{
    vector<strip> *strips;

    if(tex==DEFAULT_SKY) strips = &skystrips;
    else
    {
        stripbatch *sb = &stripbatches[renderedtex[tex]];
        if(sb->tex!=tex || sb>=&stripbatches[renderedtexs]) 
        {
            sb = &stripbatches[renderedtex[tex] = renderedtexs++];
            sb->tex = tex;
        };
        strips = &sb->strips;
    };
    if(type!=GL_TRIANGLE_STRIP && !strips->empty())
    {
        strip &last = strips->last();
        if(last.type==type && last.start+last.num==start)
        {
            last.num += n;
            return;
        };
    };
    strip &s = strips->add();
    s.type = type;
    s.start = start;
    s.num = n;
};

VARFP(gamma, 30, 100, 300,
{
    float f = gamma/100.0f;
    if(SDL_SetGamma(f,f,f)==-1)
    {
        conoutf("Could not set gamma (card/driver doesn't support it?)");
        conoutf("sdl: %s", SDL_GetError());
    };
});

physent *camera1 = NULL;

void trypitch(int i) { camera1->pitch = (float)i; }
COMMAND(trypitch, ARG_1INT);

void recomputecamera()
{
    if(editmode || player1->state!=CS_DEAD)
    {
        camera1 = player1;
    }
    else
    {
        static physent deathcam;
        if(camera1==&deathcam) return;
        deathcam = *(physent *)player1;
        deathcam.reset();
        deathcam.type = ENT_CAMERA;
        deathcam.roll = 0;
        deathcam.move = -1;
        camera1 = &deathcam;

        loopi(10) moveplayer(camera1, 10, true, 50);
    };
};

void transplayer()
{
    glLoadIdentity();
   
    glRotatef(camera1->roll, 0, 0, 1);
    glRotatef(camera1->pitch, -1, 0, 0);
    glRotatef(camera1->yaw, 0, 1, 0);

    glTranslatef(-camera1->o.x,  -camera1->o.z, -camera1->o.y); 
};

VARP(fov, 90, 100, 120);

int xtraverts;

VAR(fog, 64, 180, 1024);
VAR(fogcolour, 0, 0x8099B3, 0xFFFFFF);

VARP(hudgun,0,1,1);

char *hudgunnames[] = { "knife", "pistol", "shotgun", "subgun", "sniper", "assault", "grenade"};

void drawhudgun(int w, int h, float aspect, int farplane)
{
    if(scoped && player1->gunselect==GUN_SNIPER) return;
    
    glEnable(GL_CULL_FACE);
   
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective((float)100.0f*h/w, aspect, 0.3f, farplane); // fov fixed at 100°
    glMatrixMode(GL_MODELVIEW);

    if(hudgun && player1->state!=CS_DEAD) renderhudgun();
    rendermenumdl();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective((float)fov*h/w, aspect, 0.15f, farplane);
    glMatrixMode(GL_MODELVIEW);

    glDisable(GL_CULL_FACE);
};

bool outsidemap(physent *pl)
{
    if(pl->o.x < 0 || pl->o.x >= ssize || pl->o.y <0 || pl->o.y > ssize) return true;
    sqr *s = S((int)pl->o.x, (int)pl->o.y);
    return SOLID(s)
        || pl->o.z < s->floor - (s->type==FHF ? s->vdelta/4 : 0)
        || pl->o.z > s->ceil  + (s->type==CHF ? s->vdelta/4 : 0);
};

void gl_drawframe(int w, int h, float changelod, float curfps)
{
    recomputecamera();

    float hf = hdr.waterlevel-0.3f;
    float fovy = (float)fov*h/w;
    float aspect = w/(float)h;
    bool underwater = camera1->o.z<hf;
   
    glFogi(GL_FOG_START, (fog+64)/8);
    glFogi(GL_FOG_END, fog);
    float fogc[4] = { (fogcolour>>16)/256.0f, ((fogcolour>>8)&255)/256.0f, (fogcolour&255)/256.0f, 1.0f };
    glFogfv(GL_FOG_COLOR, fogc);
    glClearColor(fogc[0], fogc[1], fogc[2], 1.0f);

    if(underwater)
    {
        fovy += sinf(lastmillis/1000.0f)*2.0f;
        aspect += sinf(lastmillis/1000.0f+PI)*0.1f;
        glFogi(GL_FOG_START, 0);
        glFogi(GL_FOG_END, (fog+96)/8);
    };
    
    glClear((outsidemap(camera1) ? GL_COLOR_BUFFER_BIT : 0) | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    int farplane = fog*5/2;
    gluPerspective(fovy, aspect, 0.15f, farplane);
    glMatrixMode(GL_MODELVIEW);

    transplayer();

    glEnable(GL_TEXTURE_2D);
    
    resetcubes();
            
    render_world(camera1->o.x, camera1->o.y, camera1->o.z, changelod,
            (int)camera1->yaw, (int)camera1->pitch, (float)fov, w, h);
    finishstrips();

    setupworld();

    renderstripssky();

    glLoadIdentity();
    glRotatef(camera1->pitch, -1, 0, 0);
    glRotatef(camera1->yaw,   0, 1, 0);
    glRotatef(90, 1, 0, 0);
    glColor3f(1, 1, 1);
    glDisable(GL_FOG);
    glDepthFunc(GL_GREATER);
    draw_envbox(fog*4/3);
    glDepthFunc(GL_LESS);
    glEnable(GL_FOG);

    transplayer();
        
    overbright(2);
    
    renderstrips();

    xtraverts = 0;

    renderentities();

    readdepth(w, h);

    renderclients();

    if(player1->state==CS_ALIVE) readdepth(w, h, hitpos);

    renderspheres(curtime);
    renderents();

    renderbounceents();
    
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
