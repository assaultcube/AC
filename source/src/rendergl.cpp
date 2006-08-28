// rendergl.cpp: core opengl rendering stuff

#include "cube.h"

#ifdef DARWIN
#define GL_COMBINE_EXT GL_COMBINE_ARB
#define GL_COMBINE_RGB_EXT GL_COMBINE_RGB_ARB
#define GL_SOURCE0_RBG_EXT GL_SOURCE0_RGB_ARB
#define GL_SOURCE1_RBG_EXT GL_SOURCE1_RGB_ARB
#define GL_RGB_SCALE_EXT GL_RGB_SCALE_ARB
#endif

extern int curvert;

bool hasoverbright = false;

void purgetextures();

GLUquadricObj *qsphere = NULL;
int glmaxtexsize = 256;

void gl_init(int w, int h)
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
        
    purgetextures();

    if(!(qsphere = gluNewQuadric())) fatal("glu sphere");
    gluQuadricDrawStyle(qsphere, GLU_FILL);
    gluQuadricOrientation(qsphere, GLU_INSIDE);
    gluQuadricTexture(qsphere, GL_TRUE);
    glNewList(1, GL_COMPILE);
    gluSphere(qsphere, 1, 12, 6);
    glEndList();
};

void cleangl()
{
    if(qsphere) gluDeleteQuadric(qsphere);
};

bool installtex(int tnum, char *texname, int &xs, int &ys, bool clamp, bool highqual)
{
    SDL_Surface *s = IMG_Load(texname);
    if(!s) { conoutf("couldn't load texture %s", (int)texname); return false; };
    if(s->format->BitsPerPixel!=24 && s->format->BitsPerPixel!=32) { conoutf("texture must be 24bpp or 32bpp: %s", (int)texname); return false; };
    // loopi(s->w*s->h*3) { uchar *p = (uchar *)s->pixels+i; *p = 255-*p; };  
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
    void *scaledimg = s->pixels;
    if(xs!=s->w)
    {
        conoutf("warning: quality loss: scaling %s", (int)texname);     // for voodoo cards under linux
        scaledimg = alloc(xs*ys*3);
        gluScaleImage(GL_RGB, s->w, s->h, GL_UNSIGNED_BYTE, s->pixels, xs, ys, GL_UNSIGNED_BYTE, scaledimg);
    };
    int mode = s->format->BitsPerPixel==24 ? GL_RGB : GL_RGBA;
    if(gluBuild2DMipmaps(GL_TEXTURE_2D, mode, xs, ys, mode, GL_UNSIGNED_BYTE, scaledimg)) fatal("could not build mipmaps");
    if(xs!=s->w) free(scaledimg);
    SDL_FreeSurface(s);
    return true;
};

// management of texture slots
// each texture slot can have multople texture frames, of which currently only the first is used
// additional frames can be used for various shaders

const int MAXTEX = 1000;
int texx[MAXTEX];                           // ( loaded texture ) -> ( name, size )
int texy[MAXTEX];                           
string texname[MAXTEX];
int curtex = 0;
const int FIRSTTEX = 160;                   // opengl id = loaded id + FIRSTTEX
// std 1+, sky 14+, md3's 30+,md2's 40+

const int MAXFRAMES = 2;                    // increase to allow more complex shader defs
int mapping[256][MAXFRAMES];                // ( cube texture, frame ) -> ( opengl id, name )
string mapname[256][MAXFRAMES];

void purgetextures()
{
    loopi(256) loop(j,MAXFRAMES) mapping[i][j] = 0;
};

int curtexnum = 0;

void texturereset() { curtexnum = 0; };

void texture(char *aframe, char *name)
{
    int num = curtexnum++, frame = atoi(aframe);
    if(num<0 || num>=256 || frame<0 || frame>=MAXFRAMES) return;
    mapping[num][frame] = 1;
    char *n = mapname[num][frame];
    strcpy_s(n, name);
    path(n);
};

COMMAND(texturereset, ARG_NONE);
COMMAND(texture, ARG_2STR);

int lookuptexture(int tex, int &xs, int &ys)
{
    int frame = 0;                      // other frames?
    int tid = mapping[tex][frame];

    if(tid>=FIRSTTEX)
    {
        xs = texx[tid-FIRSTTEX];
        ys = texy[tid-FIRSTTEX];
        return tid;
    };

    xs = ys = 16;
    if(!tid) return 1;                  // crosshair :)

    loopi(curtex)       // lazily happens once per "texture" command, basically
    {
        if(strcmp(mapname[tex][frame], texname[i])==0)
        {
            mapping[tex][frame] = tid = i+FIRSTTEX;
            xs = texx[i];
            ys = texy[i];
            return tid;
        };
    };

    if(curtex==MAXTEX) fatal("loaded too many textures");

    int tnum = curtex+FIRSTTEX;
    strcpy_s(texname[curtex], mapname[tex][frame]);

    sprintf_sd(name)("packages%ctextures%c%s", PATHDIV, PATHDIV, texname[curtex]);

    if(installtex(tnum, name, xs, ys))
    {
        mapping[tex][frame] = tnum;
        texx[curtex] = xs;
        texy[curtex] = ys;
        curtex++;
        return tnum;
    }
    else
    {
        return mapping[tex][frame] = FIRSTTEX;  // temp fix
    };
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

int skyoglid;

struct strip { int tex, start, num; };
vector<strip> strips;

void renderstripssky()
{
    glBindTexture(GL_TEXTURE_2D, skyoglid);
    loopv(strips) if(strips[i].tex==skyoglid) glDrawArrays(GL_TRIANGLE_STRIP, strips[i].start, strips[i].num);
};

void renderstrips()
{
    int lasttex = -1;
    loopv(strips) if(strips[i].tex!=skyoglid)
    {
        if(strips[i].tex!=lasttex)
        {
            glBindTexture(GL_TEXTURE_2D, strips[i].tex); 
            lasttex = strips[i].tex;
        };
        glDrawArrays(GL_TRIANGLE_STRIP, strips[i].start, strips[i].num);  
    };   
};

void overbright(float amount) { if(hasoverbright) glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, amount ); };

void addstrip(int tex, int start, int n)
{
    strip &s = strips.add();
    s.tex = tex;
    s.start = start;
    s.num = n;
};

VARF(gamma, 30, 100, 300,
{
    float f = gamma/100.0f;
    if(SDL_SetGamma(f,f,f)==-1)
    {
        conoutf("Could not set gamma (card/driver doesn't support it?)");
        conoutf("sdl: %s", (int)SDL_GetError());
    };
});

void trypitch(int i) { player1->pitch=i; }
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
    sprintf_sd(mdl)("hudguns/%s", hudgunnames[player1->gunselect]);
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
    
    int xs, ys;
    skyoglid = lookuptexture(DEFAULT_SKY, xs, ys);
   
    resetcubes();
            
    curvert = 0;
    strips.setsize(0);
  
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
    draw_envbox(14, fog*4/3);
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

    gl_drawhud(w, h, (int)curfps, nquads, curvert, underwater);

    glEnable(GL_CULL_FACE);
    glEnable(GL_FOG);
};
