// rendergl.cpp: core opengl rendering stuff

#include "cube.h"
#include "bot/bot.h"

bool hasoverbright = false, hasmultitexture = false;

// GL_ARB_multitexture
PFNGLACTIVETEXTUREARBPROC   glActiveTexture_   = NULL;
PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2f_ = NULL;
PFNGLMULTITEXCOORD3FARBPROC glMultiTexCoord3f_ = NULL;

void *getprocaddress(const char *name)
{
    return SDL_GL_GetProcAddress(name);
}

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

    const char *vendor = (const char *)glGetString(GL_VENDOR);
    const char *exts = (const char *)glGetString(GL_EXTENSIONS);
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    const char *version = (const char *)glGetString(GL_VERSION);
    conoutf("Renderer: %s (%s)", renderer, vendor);
    conoutf("Driver: %s", version);

    if(strstr(exts, "GL_EXT_texture_env_combine")) hasoverbright = true;
	else if(strstr(exts, "GL_ARB_texture_env_combine")) hasoverbright = true;
    else conoutf("WARNING: cannot use overbright lighting, using old lighting model!");

    if(strstr(exts, "GL_ARB_multitexture"))
    {
        hasmultitexture = true;
        glActiveTexture_       = (PFNGLACTIVETEXTUREARBPROC)  getprocaddress("glActiveTextureARB");
        glMultiTexCoord2f_     = (PFNGLMULTITEXCOORD2FARBPROC)getprocaddress("glMultiTexCoord2fARB");
        glMultiTexCoord3f_     = (PFNGLMULTITEXCOORD3FARBPROC)getprocaddress("glMultiTexCoord3fARB");
    }

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtexsize);

    if(fsaa) glEnable(GL_MULTISAMPLE);

    camera1 = player1;
}

void cleangl()
{
}

void line(int x1, int y1, float z1, int x2, int y2, float z2)
{
    glBegin(GL_POLYGON);
    glVertex3f((float)x1, (float)y1, z1);
    glVertex3f((float)x1, y1+0.01f, z1);
    glVertex3f((float)x2, y2+0.01f, z2);
    glVertex3f((float)x2, (float)y2, z2);
    glEnd();
    xtraverts += 4;
}

void linestyle(float width, int r, int g, int b)
{   
    glLineWidth(width);
    glColor3ub(r,g,b);
}   
    
void box(block &b, float z1, float z2, float z3, float z4)
{   
    glBegin(GL_POLYGON);
    glVertex3f((float)b.x,      (float)b.y,      z1);
    glVertex3f((float)b.x+b.xs, (float)b.y,      z2);
    glVertex3f((float)b.x+b.xs, (float)b.y+b.ys, z3);
    glVertex3f((float)b.x,      (float)b.y+b.ys, z4);
    glEnd();
    xtraverts += 4;
}   

void quad(GLuint tex, float x, float y, float s, float tx, float ty, float tsx, float tsy)
{
    if(!tsy) tsy = tsx;
    glBindTexture(GL_TEXTURE_2D, tex);
    glBegin(GL_QUADS);
    glTexCoord2f(tx,     ty);     glVertex2f(x,   y);
    glTexCoord2f(tx+tsx, ty);     glVertex2f(x+s, y);
    glTexCoord2f(tx+tsx, ty+tsy); glVertex2f(x+s, y+s);
    glTexCoord2f(tx,     ty+tsy); glVertex2f(x,   y+s);
    glEnd();
    xtraverts += 4;
}

void circle(GLuint tex, float x, float y, float r, float tx, float ty, float tr, int subdiv)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(tx, ty); 
    glVertex2f(x, y);
    loopi(subdiv+1)
    {
        float c = cosf(2*M_PI*i/float(subdiv)), s = sinf(2*M_PI*i/float(subdiv));
        glTexCoord2f(tx + tr*c, ty + tr*s);
        glVertex2f(x + r*c, y + r*s);
    }
    glEnd();
    xtraverts += subdiv+2;
}

void dot(int x, int y, float z)
{
    const float DOF = 0.1f;
    glBegin(GL_POLYGON);
    glVertex3f(x-DOF, y-DOF, z);
    glVertex3f(x+DOF, y-DOF, z);
    glVertex3f(x+DOF, y+DOF, z);
    glVertex3f(x-DOF, y+DOF, z);
    glEnd();
    xtraverts += 4;
}

void blendbox(int x1, int y1, int x2, int y2, bool border, int tex)
{   
    glDepthMask(GL_FALSE);
    glDisable(GL_TEXTURE_2D);
    if(tex>=0)
    {
        glBindTexture(GL_TEXTURE_2D, tex);
        glEnable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glColor3f(1, 1, 1);

        int texw = 512;
        int texh = texw;
        int cols = (int)((x2-x1)/texw+1);
        int rows = (int)((y2-y1)/texh+1);
        xtraverts += cols*rows*4;
            
        loopj(rows)
        {
            float ytexcut = 0.0f;
            float yboxcut = 0.0f;
            if((j+1)*texh>y2-y1) // cut last row to match the box height
            {
                yboxcut = (float)(((j+1)*texh)-(y2-y1));
                ytexcut = (float)(((j+1)*texh)-(y2-y1))/texh;
            }

            loopi(cols)
            {
                float xtexcut = 0.0f;
                float xboxcut = 0.0f;
                if((i+1)*texw>x2-x1)
                {
                    xboxcut = (float)(((i+1)*texw)-(x2-x1));
                    xtexcut = (float)(((i+1)*texw)-(x2-x1))/texw;
                }

                glBegin(GL_QUADS);
                glTexCoord2f(0, 0);                 glVertex2f((float)x1+texw*i, (float)y1+texh*j);
                glTexCoord2f(1-xtexcut, 0);         glVertex2f(x1+texw*(i+1)-xboxcut, (float)y1+texh*j);
                glTexCoord2f(1-xtexcut, 1-ytexcut); glVertex2f(x1+texw*(i+1)-xboxcut, (float)y1+texh*(j+1)-yboxcut);
                glTexCoord2f(0, 1-ytexcut);         glVertex2f((float)x1+texw*i, y1+texh*(j+1)-yboxcut);
                glEnd();
            }
        }
    }
    else
    {
        if(border) glColor3f(0.7f, 0.7f, 0.7f); //glColor3d(0.5, 0.3, 0.4); 
        else glColor3f(1, 1, 1);
        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2i(x1, y1);
        glTexCoord2f(1, 0); glVertex2i(x2, y1);
        glTexCoord2f(1, 1); glVertex2i(x2, y2);
        glTexCoord2f(0, 1); glVertex2i(x1, y2);
        glEnd();
        xtraverts += 4;
    }

    glDisable(GL_BLEND);
    if(tex>=0) glDisable(GL_TEXTURE_2D);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_POLYGON);
    glColor3f(0.6f, 0.6f, 0.6f); //glColor3d(0.2, 0.7, 0.4);
    glVertex2i(x1, y1);
    glVertex2i(x2, y1); 
    glVertex2i(x2, y2);
    glVertex2i(x1, y2);
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_BLEND); 
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
}

VARP(fov, 90, 100, 120);
VAR(fog, 64, 180, 1024);
VAR(fogcolour, 0, 0x8099B3, 0xFFFFFF);

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
    }
}

void transplayer()
{
    glLoadIdentity();
   
    glRotatef(camera1->roll, 0, 0, 1);
    glRotatef(camera1->pitch, -1, 0, 0);
    glRotatef(camera1->yaw, 0, 1, 0);

    // move from RH to Z-up LH quake style worldspace
    glRotatef(-90, 1, 0, 0);
    glScalef(1, -1, 1);

    glTranslatef(-camera1->o.x, -camera1->o.y, -camera1->o.z); 
}

void genclipmatrix(float a, float b, float c, float d, GLfloat matrix[16])
{
    // transform the clip plane into camera space
    GLdouble clip[4] = {a, b, c, d};
    glClipPlane(GL_CLIP_PLANE0, clip);
    glGetClipPlane(GL_CLIP_PLANE0, clip);

    glGetFloatv(GL_PROJECTION_MATRIX, matrix);
    float x = ((clip[0]<0 ? -1 : (clip[0]>0 ? 1 : 0)) + matrix[8]) / matrix[0],
          y = ((clip[1]<0 ? -1 : (clip[1]>0 ? 1 : 0)) + matrix[9]) / matrix[5],
          w = (1 + matrix[10]) / matrix[14],
          scale = 2 / (x*clip[0] + y*clip[1] - clip[2] + w*clip[3]);
    matrix[2] = clip[0]*scale;
    matrix[6] = clip[1]*scale;
    matrix[10] = clip[2]*scale + 1.0f;
    matrix[14] = clip[3]*scale;
}

bool reflecting = false, refracting = false;
GLuint reflecttex = 0, refracttex = 0;
int reflectlastsize = 0;

VARP(reflectres, 6, 8, 10);
VAR(reflectclip, 0, 3, 100);
VARP(waterreflect, 0, 1, 1);
VARP(waterrefract, 0, 0, 1);

void drawreflection(float hf, int w, int h, float changelod, bool refract)
{
    reflecting = true;
    refracting = refract;

    int size = 1<<reflectres;
    while(size > w || size > h) size /= 2;
    if(size!=reflectlastsize)
    {
        if(reflecttex) glDeleteTextures(1, &reflecttex);
        if(refracttex) glDeleteTextures(1, &refracttex);
        reflecttex = refracttex = 0;
    }
    if(!reflecttex || (waterrefract && !refracttex))
    {
        if(!reflecttex)
        {
            glGenTextures(1, &reflecttex);
            createtexture(reflecttex, size, size, NULL, 3, false, GL_RGB);
        }
        if(!refracttex)
        {
            glGenTextures(1, &refracttex);
            createtexture(refracttex, size, size, NULL, 3, false, GL_RGB);
        }
        reflectlastsize = size;
    }

    resetcubes();

    render_world(camera1->o.x, camera1->o.y, refract ? camera1->o.z : hf, changelod,
            (int)camera1->yaw, (refract ? 1 : -1)*(int)camera1->pitch, (float)fov, size, size);

    setupstrips();

    if(!refract) glCullFace(GL_BACK);
    glViewport(0, 0, size, size);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);

    transplayer();
    if(!refract)
    {
        glTranslatef(0, 0, 2*hf);
        glScalef(1, 1, -1);
    }

    GLfloat clipmat[16];
    genclipmatrix(0, 0, refract ? -1 : 1, refract ? 0.1f*reflectclip+hf : 0.1f*reflectclip-hf, clipmat);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadMatrixf(clipmat);
    glMatrixMode(GL_MODELVIEW);

    renderstripssky();

    glPushMatrix();
    glLoadIdentity();
    glRotatef(camera1->pitch, -1, 0, 0);
    glRotatef(camera1->yaw,   0, 1, 0);
    glRotatef(90, 1, 0, 0);   
    if(!refract) glScalef(1, 1, -1);
    glColor3f(1, 1, 1); 
    glDisable(GL_FOG);
    glDepthFunc(GL_GREATER);
    draw_envbox(fog*4/3);
    glDepthFunc(GL_LESS);
    glEnable(GL_FOG);
    glPopMatrix();

    overbright(2);

    renderstrips();
    renderentities();
    renderclients();

    overbright(1);

    render_particles(0);

    if(refract) glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    extern int mtwater;
    if(refract && !mtwater)
    {
        glLoadIdentity();
        glOrtho(0, 1, 0, 1, -1, 1);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4ubv(hdr.watercolor);
        glBegin(GL_QUADS);
        glVertex2i(0, 1);
        glVertex2i(1, 1);
        glVertex2i(1, 0);
        glVertex2i(0, 0);
        glEnd();
        glDisable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_DEPTH_TEST);
    }
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    if(!refract) glCullFace(GL_FRONT);
    glViewport(0, 0, w, h);

    glBindTexture(GL_TEXTURE_2D, refract ? refracttex : reflecttex);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, size, size);

    glDisable(GL_TEXTURE_2D);

    reflecting = refracting = false;
}

bool minimap = false, minimapdirty = true;
int minimaplastsize = 0;
GLuint minimaptex = 0;

void clearminimap()
{
    minimapdirty = true;
}

COMMAND(clearminimap, ARG_NONE);

VARFP(minimapres, 7, 9, 10, clearminimap());

void drawminimap(int w, int h)
{
    if(!minimapdirty) return;

    int size = 1<<minimapres;
    while(size > w || size > h) size /= 2;
    if(size!=minimaplastsize)
    {
        glDeleteTextures(1, &minimaptex);
        minimaptex = 0;
    }
    if(!minimaptex)
    {
        glGenTextures(1, &minimaptex);
        createtexture(minimaptex, size, size, NULL, 3, false, GL_RGB);
        minimaplastsize = size;
    }

    minimap = true;

    physent minicam;
    camera1 = &minicam;
    camera1->o.x = camera1->o.y = ssize/2;
    camera1->o.z = 128;
    camera1->pitch = -90;
    camera1->yaw = 0;

    glViewport(0, 0, size, size);
    glClearDepth(0.0);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-ssize/2, ssize/2, -ssize/2, ssize/2, 0, 256);
    glScalef(1, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    glCullFace(GL_BACK);
    glDisable(GL_FOG);
    glEnable(GL_TEXTURE_2D);
   
    transplayer();

    resetcubes();

    render_world(camera1->o.x, camera1->o.y, camera1->o.z, 1.0f,
            (int)camera1->yaw, (int)camera1->pitch, 90.0f, size, size);

    setupstrips();

    overbright(2);
    glDepthFunc(GL_ALWAYS);
    renderstrips();
    glDepthFunc(GL_LESS);
    renderentities();
    overbright(1);

    float hf = hdr.waterlevel-0.3f;
    renderwater(hf, 0, 0);

    minimap = false;

    glBindTexture(GL_TEXTURE_2D, minimaptex);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, size, size);
    minimapdirty = false;

    glCullFace(GL_FRONT);
    glEnable(GL_FOG);
    glDisable(GL_TEXTURE_2D);

    glViewport(0, 0, w, h);
    glClearDepth(1.0);
}

int xtraverts;

void drawhudgun(int w, int h, float aspect, int farplane)
{
    if(scoped && player1->gunselect==GUN_SNIPER) return;
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective((float)100.0f*h/w, aspect, 0.3f, farplane); // fov fixed at 100°
    glMatrixMode(GL_MODELVIEW);

    if(player1->state!=CS_DEAD) renderhudgun();
    rendermenumdl();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective((float)fov*h/w, aspect, 0.15f, farplane);
    glMatrixMode(GL_MODELVIEW);
}

bool outsidemap(physent *pl)
{
    if(pl->o.x < 0 || pl->o.x >= ssize || pl->o.y <0 || pl->o.y > ssize) return true;
    sqr *s = S((int)pl->o.x, (int)pl->o.y);
    return SOLID(s)
        || pl->o.z < s->floor - (s->type==FHF ? s->vdelta/4 : 0)
        || pl->o.z > s->ceil  + (s->type==CHF ? s->vdelta/4 : 0);
}

float cursordepth = 0.9f;
GLint viewport[4];
GLdouble mm[16], pm[16];
vec worldpos, camup, camright;

void readmatrices()
{
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, mm);
    glGetDoublev(GL_PROJECTION_MATRIX, pm);
    camright = vec(float(mm[0]), float(mm[4]), float(mm[8]));
    camup = vec(float(mm[1]), float(mm[5]), float(mm[9]));
}

// stupid function to cater for stupid ATI linux drivers that return incorrect depth values

float depthcorrect(float d)
{
    return (d<=1/256.0f) ? d*256 : d;
}

// find out the 3d target of the crosshair in the world easily and very acurately.
// sadly many very old cards and drivers appear to fuck up on glReadPixels() and give false
// coordinates, making shooting and such impossible.
// also hits map entities which is unwanted.
// could be replaced by a more acurate version of monster.cpp los() if needed

void readdepth(int w, int h, vec &pos)
{
    glReadPixels(w/2, h/2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &cursordepth);
    double worldx = 0, worldy = 0, worldz = 0;
    gluUnProject(w/2, h/2, depthcorrect(cursordepth), mm, pm, viewport, &worldx, &worldy, &worldz);
    pos.x = (float)worldx;
    pos.y = (float)worldy;
    pos.z = (float)worldz;
}

void gl_drawframe(int w, int h, float changelod, float curfps)
{
    drawminimap(w, h);

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

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    int farplane = fog*5/2;
    gluPerspective(fovy, aspect, 0.15f, farplane);
    glMatrixMode(GL_MODELVIEW);

    if(underwater)
    {
        fovy += sinf(lastmillis/1000.0f)*2.0f;
        aspect += sinf(lastmillis/1000.0f+PI)*0.1f;
        float wfogc[4] = { hdr.watercolor[0]/255.0f, hdr.watercolor[1]/255.0f, hdr.watercolor[2]/255.0f, 1.0f };
        glFogfv(GL_FOG_COLOR, wfogc);
        glFogi(GL_FOG_START, 0);
        glFogi(GL_FOG_END, (fog+96)/8);
    }
    else if(waterreflect)
    {
        extern int wx1;
        if(wx1>=0) 
        {
            drawreflection(hf, w, h, changelod, false);
            if(waterrefract) drawreflection(hf, w, h, changelod, true);
        }
    }
    
    glClear((outsidemap(camera1) ? GL_COLOR_BUFFER_BIT : 0) | GL_DEPTH_BUFFER_BIT);

    transplayer();

    glEnable(GL_TEXTURE_2D);
    
    resetcubes();
            
    render_world(camera1->o.x, camera1->o.y, camera1->o.z, changelod,
            (int)camera1->yaw, (int)camera1->pitch, (float)fov, w, h);

    setupstrips();

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

    readmatrices();
    readdepth(w, h, worldpos);

    renderclients();

    if(player1->state==CS_ALIVE) readdepth(w, h, hitpos);

    renderbounceents();
    
    // Added by Rick: Need todo here because of drawing the waypoints
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    WaypointClass.Think();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    // end add
    
    drawhudgun(w, h, aspect, farplane);

    overbright(1);

    render_particles(curtime);

    glDisable(GL_CULL_FACE);
    int nquads = renderwater(hf, !waterreflect || underwater ? 0 : reflecttex, !waterreflect || !waterrefract || underwater ? 0 : refracttex);

    glDisable(GL_FOG);
    glDisable(GL_TEXTURE_2D);

    if(editmode)
    {
        if(cursordepth==1.0f) worldpos = camera1->o;
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        cursorupdate();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    extern vector<vertex> verts;
    gl_drawhud(w, h, (int)curfps, nquads, verts.length(), underwater);

    glEnable(GL_CULL_FACE);
    glEnable(GL_FOG);
}

