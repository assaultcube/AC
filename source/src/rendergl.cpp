// rendergl.cpp: core opengl rendering stuff

#include "cube.h"
#include "bot/bot.h"

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

    if(fsaa) glEnable(GL_MULTISAMPLE);

    camera1 = player1;
}

void cleangl()
{
}

void line(int x1, int y1, float z1, int x2, int y2, float z2)
{
    glBegin(GL_POLYGON);
    glVertex3f((float)x1, z1, (float)y1);
    glVertex3f((float)x1, z1, y1+0.01f);
    glVertex3f((float)x2, z2, y2+0.01f);
    glVertex3f((float)x2, z2, (float)y2);
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
    glVertex3f((float)b.x,      z1, (float)b.y);
    glVertex3f((float)b.x+b.xs, z2, (float)b.y);
    glVertex3f((float)b.x+b.xs, z3, (float)b.y+b.ys);
    glVertex3f((float)b.x,      z4, (float)b.y+b.ys);
    glEnd();
    xtraverts += 4;
}   

void quad(GLuint tex, float x, float y, float s, float tx, float ty, float ts)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glBegin(GL_QUADS);
    glTexCoord2f(tx,    ty);    glVertex2f(x,   y);
    glTexCoord2f(tx+ts, ty);    glVertex2f(x+s, y);
    glTexCoord2f(tx+ts, ty+ts); glVertex2f(x+s, y+s);
    glTexCoord2f(tx,    ty+ts); glVertex2f(x,   y+s);
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
    glVertex3f(x-DOF, (float)z, y-DOF);
    glVertex3f(x+DOF, (float)z, y-DOF);
    glVertex3f(x+DOF, (float)z, y+DOF);
    glVertex3f(x-DOF, (float)z, y+DOF);
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

    glTranslatef(-camera1->o.x,  -camera1->o.z, -camera1->o.y); 
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

void drawminimap()
{
    if(!minimapdirty) return;

    extern int scr_w, scr_h;
    int size = 1<<minimapres;
    while(size > scr_w || size > scr_h) size /= 2;
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
    renderwater(hf);

    minimap = false;

    glBindTexture(GL_TEXTURE_2D, minimaptex);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, size, size);
    minimapdirty = false;

    glCullFace(GL_FRONT);
    glEnable(GL_FOG);
    glDisable(GL_TEXTURE_2D);

    glViewport(0, 0, scr_w, scr_h);
    glClearDepth(1.0);
}

VARP(fov, 90, 100, 120);

int xtraverts;

VAR(fog, 64, 180, 1024);
VAR(fogcolour, 0, 0x8099B3, 0xFFFFFF);

void drawhudgun(int w, int h, float aspect, int farplane)
{
    if(scoped && player1->gunselect==GUN_SNIPER) return;
    
    glEnable(GL_CULL_FACE);
   
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

    glDisable(GL_CULL_FACE);
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
    gluUnProject(w/2, h/2, depthcorrect(cursordepth), mm, pm, viewport, &worldx, &worldz, &worldy);
    pos.x = (float)worldx;
    pos.y = (float)worldy;
    pos.z = (float)worldz;
}

void invertperspective()
{
    // This only generates a valid inverse matrix for matrices generated by gluPerspective()
    GLdouble inv[16];
    memset(inv, 0, sizeof(inv));

    inv[0*4+0] = 1.0/pm[0*4+0];
    inv[1*4+1] = 1.0/pm[1*4+1];
    inv[2*4+3] = 1.0/pm[3*4+2];
    inv[3*4+2] = -1.0;
    inv[3*4+3] = pm[2*4+2]/pm[3*4+2];

    glLoadMatrixd(inv);
}

void gl_drawframe(int w, int h, float changelod, float curfps)
{
    drawminimap();

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
    }
    
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
    int nquads = renderwater(hf);

    glDisable(GL_FOG);
    glDisable(GL_TEXTURE_2D);

    if(editmode)
    {
        if(cursordepth==1.0f) worldpos = camera1->o;
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        cursorupdate();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    invertperspective();
    extern vector<vertex> verts;
    gl_drawhud(w, h, (int)curfps, nquads, verts.length(), underwater);

    glEnable(GL_CULL_FACE);
    glEnable(GL_FOG);
}

