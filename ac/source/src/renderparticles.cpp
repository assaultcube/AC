// renderparticles.cpp

#include "cube.h"

static GLushort *hemiindices = NULL;
static vec *hemiverts = NULL;
static int heminumverts = 0, heminumindices = 0;

static void subdivide(int &nIndices, int &nVerts, int depth, int face);

static void genface(int &nIndices, int &nVerts, int depth, int i1, int i2, int i3)
{   
    int face = nIndices; nIndices += 3;
    hemiindices[face]   = i1;
    hemiindices[face+1] = i2;
    hemiindices[face+2] = i3;
    subdivide(nIndices, nVerts, depth, face);
}

static void subdivide(int &nIndices, int &nVerts, int depth, int face)
{   
    if(depth-- <= 0) return;
    int idx[6];
    loopi(3) idx[i] = hemiindices[face+i];
    loopi(3)
    {
        int vert = nVerts++;
        hemiverts[vert] = vec(hemiverts[idx[i]]).add(hemiverts[idx[(i+1)%3]]).normalize(); //push on to unit sphere
        idx[3+i] = vert;
        hemiindices[face+i] = vert;
    }
    subdivide(nIndices, nVerts, depth, face); 
    loopi(3) genface(nIndices, nVerts, depth, idx[i], idx[3+i], idx[3+(i+2)%3]);
}

//subdiv version wobble much more nicely than a lat/longitude version
static void inithemisphere(int hres, int depth)
{   
    const int tris = hres << (2*depth);
    heminumverts = heminumindices = 0;
    DELETEA(hemiverts);
    DELETEA(hemiindices);
    hemiverts = new vec[tris+1];
    hemiindices = new GLushort[tris*3];
    hemiverts[heminumverts++] = vec(0.0f, 0.0f, 1.0f); //build initial 'hres' sided pyramid
    loopi(hres)
    {
        float a = PI2*float(i)/hres;
        hemiverts[heminumverts++] = vec(cosf(a), sinf(a), 0.0f);
    }
    loopi(hres) genface(heminumindices, heminumverts, depth, 0, i+1, 1+(i+1)%hres);
}

GLuint createexpmodtex(int size, float minval)
{   
    uchar *data = new uchar[size*size], *dst = data;
    loop(y, size) loop(x, size)
    {
        float dx = 2*float(x)/(size-1) - 1, dy = 2*float(y)/(size-1) - 1;
        float z = 1 - dx*dx - dy*dy;
        if(minval) z = sqrtf(max(z, 0));
        else loopk(2) z *= z; 
        *dst++ = uchar(max(z, minval)*255);
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    createtexture(tex, size, size, data, 3, true, GL_ALPHA);
    delete[] data;
    return tex;
}

static struct expvert
{   
    vec pos; 
    float u, v;
} *expverts = NULL;

static GLuint expmodtex[2] = {0, 0};
static GLuint lastexpmodtex = 0;

VARP(mtexplosion, 0, 1, 1);

void setupexplosion()
{   
    const int hres = 5;
    const int depth = 2;
    if(!hemiindices) inithemisphere(hres, depth);

    static int lastexpmillis = 0;
    if(lastexpmillis != lastmillis || !expverts)
    {
        vec center = vec(13.0f, 2.3f, 7.1f);
        lastexpmillis = lastmillis;
        if(!expverts) expverts = new expvert[heminumverts];
        loopi(heminumverts)
        {
            expvert &e = expverts[i];
            vec &v = hemiverts[i];
            e.u = v.x*0.5f + 0.001f*lastmillis;
            e.v = v.y*0.5f + 0.001f*lastmillis;
            float wobble = v.dot(center) + 0.002f*lastmillis;
            wobble -= floor(wobble);
            wobble = 1.0f + fabs(wobble - 0.5f)*0.5f;
            e.pos = vec(v).mul(wobble);
        }
    }

    if(mtexplosion && maxtmus>=2)
    {
        setuptmu(0, "C * T", "= Ca");
        glActiveTexture_(GL_TEXTURE1_ARB);
        glEnable(GL_TEXTURE_2D);

        GLfloat s[4] = { 0.5f, 0, 0, 0.5f }, t[4] = { 0, 0.5f, 0, 0.5f };
        glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
        glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
        glTexGenfv(GL_S, GL_OBJECT_PLANE, s);
        glTexGenfv(GL_T, GL_OBJECT_PLANE, t);
        glEnable(GL_TEXTURE_GEN_S);
        glEnable(GL_TEXTURE_GEN_T);

        setuptmu(1, "P * Ta x 4", "Pa * Ta x 4");

        glActiveTexture_(GL_TEXTURE0_ARB);
        
        if(!expmodtex[0]) expmodtex[0] = createexpmodtex(64, 0);
        if(!expmodtex[1]) expmodtex[1] = createexpmodtex(64, 0.25f);
        lastexpmodtex = 0;
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(expvert), &expverts->pos);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, sizeof(expvert), &expverts->u);
}

void drawexplosion(bool inside)
{
    if(mtexplosion && maxtmus>=2 && lastexpmodtex != expmodtex[inside ? 1 : 0])
    {
        glActiveTexture_(GL_TEXTURE1_ARB);
        lastexpmodtex = expmodtex[inside ? 1 :0];
        glBindTexture(GL_TEXTURE_2D, lastexpmodtex);
        glActiveTexture_(GL_TEXTURE0_ARB);
    }
    if(inside)
    {
        glDisable(GL_DEPTH_TEST);
        glCullFace(GL_BACK);
        glDrawElements(GL_TRIANGLES, heminumindices, GL_UNSIGNED_SHORT, hemiindices);
        glCullFace(GL_FRONT);
        glScalef(1, 1, -1);
    }
    glDrawElements(GL_TRIANGLES, heminumindices, GL_UNSIGNED_SHORT, hemiindices);
    if(inside) glEnable(GL_DEPTH_TEST);
}

void cleanupexplosion()
{   
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    if(mtexplosion && maxtmus>=2)
    {
        resettmu(0);
        glActiveTexture_(GL_TEXTURE1_ARB);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_TEXTURE_GEN_S);
        glDisable(GL_TEXTURE_GEN_T);
        resettmu(1);
        glActiveTexture_(GL_TEXTURE0_ARB);
    }
}

#define MAXPARTYPES 8

struct particle { vec o, d; int fade, type; int millis; particle *next; };
particle *parlist[MAXPARTYPES], *parempty = NULL;

static Texture *parttex[4];

void particleinit()
{
    loopi(MAXPARTYPES) parlist[i] = NULL;

    parttex[0] = textureload("packages/misc/base.png");
    parttex[1] = textureload("packages/misc/smoke.png");
    parttex[2] = textureload("packages/misc/explosion.jpg");
    parttex[3] = textureload("packages/misc/hole.png");
}

void particlereset()
{
    loopi(MAXPARTYPES)
    {
        while(parlist[i])
        {
            particle *p = parlist[i];
            parlist[i] = p->next;
            p->next = parempty;
            parempty = p;
        }
    }
}

void newparticle(const vec &o, const vec &d, int fade, int type)
{
    if(!parempty)
    {
        particle *ps = new particle[256];
        loopi(256)
        {
            ps[i].next = parempty;
            parempty = &ps[i];
        }
    }

    particle *p = parempty;
    parempty = p->next;
    p->o = o;
    p->d = d;
    p->fade = fade;
    p->type = type;
    p->millis = lastmillis;
    p->next = parlist[type];
    parlist[type] = p;
}

enum
{
    PT_PART = 0,
    PT_FIREBALL,
    PT_SHOTLINE,
    PT_DECAL
};

static struct parttype { int type; float r, g, b; int gr, tex; float sz; } parttypes[] =
{
    { 0,           0.4f, 0.4f, 0.4f, 2,  0, 0.06f }, // yellow: sparks 
    { 0,           1.0f, 1.0f, 1.0f, 20, 1, 0.15f }, // grey:   small smoke
    { 0,           0.2f, 0.2f, 1.0f, 20, 0, 0.08f }, // blue:   edit mode entities
    { 0,           1.0f, 0.1f, 0.1f, 1,  1, 0.06f }, // red:    blood spats
    { 0,           1.0f, 0.1f, 0.1f, 0,  1, 0.2f  }, // red:    demotrack
    { PT_FIREBALL, 1.0f, 0.5f, 0.5f, 0,  2, 7.0f  }, // explosion fireball
    { PT_SHOTLINE, 1.0f, 1.0f, 0.7f, 0, -1, 0.0f  }, // yellow: shotline
    { PT_DECAL,    1.0f, 1.0f, 1.0f, 0,  3, 0.07f }, // hole decal     
};

VAR(demotracking, 0, 0, 1);
VAR(particlesize, 20, 100, 500);

void render_particles(int time)
{
	if(demoplayback && demotracking)
	{
		vec nom(0, 0, 0);
		newparticle(player1->o, nom, 100000000, 4);
	}

    bool rendered = false;
    for(int i = MAXPARTYPES-1; i>=0; i--) if(parlist[i])
    {
        if(!rendered)
        {
            rendered = true;
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glDisable(GL_FOG);
        }

        parttype &pt = parttypes[i];
        if(pt.type!=PT_FIREBALL) continue;
        float sz = pt.sz*particlesize/100.0f;

        if(pt.tex>=0) glBindTexture(GL_TEXTURE_2D, parttex[pt.tex]->id);
        else glDisable(GL_TEXTURE_2D);
        switch(pt.type)
        {
            case PT_PART:
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glColor3f(pt.r, pt.g, pt.b);
                glBegin(GL_QUADS);
                break;

            case PT_FIREBALL:
                setupexplosion();
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                break;

            case PT_SHOTLINE:
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColor4f(pt.r, pt.g, pt.b, 0.5f);
                glBegin(GL_LINES);
                break;

            case PT_DECAL:
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glBegin(GL_QUADS);
                break;
        }
         
        for(particle *p, **pp = &parlist[i]; (p = *pp);)
        {       
            switch(pt.type)
            {
                case PT_PART:
                    glTexCoord2i(0, 1); glVertex3f(p->o.x+(-camright.x+camup.x)*sz, p->o.y+(-camright.y+camup.y)*sz, p->o.z+(-camright.z+camup.z)*sz);
                    glTexCoord2i(1, 1); glVertex3f(p->o.x+( camright.x+camup.x)*sz, p->o.y+( camright.y+camup.y)*sz, p->o.z+( camright.z+camup.z)*sz);
                    glTexCoord2i(1, 0); glVertex3f(p->o.x+( camright.x-camup.x)*sz, p->o.y+( camright.y-camup.y)*sz, p->o.z+( camright.z-camup.z)*sz);
                    glTexCoord2i(0, 0); glVertex3f(p->o.x+(-camright.x-camup.x)*sz, p->o.y+(-camright.y-camup.y)*sz, p->o.z+(-camright.z-camup.z)*sz);
                    xtraverts += 4;
                    break;
                
                case PT_FIREBALL:
                {
                    sz = 1.0f + (pt.sz-1.0f)*min(p->fade, lastmillis-p->millis)/p->fade;
                    glPushMatrix();
                    glTranslatef(p->o.x, p->o.y, p->o.z);
                    bool inside = p->o.dist(camera1->o) <= sz*1.25f; //1.25 is max wobble scale
                    vec oc(p->o);
                    oc.sub(camera1->o);
                    glRotatef(inside ? camera1->yaw - 180 : atan2(oc.y, oc.x)/RAD - 90, 0, 0, 1);
                    glRotatef((inside ? camera1->pitch : asin(oc.z/oc.magnitude())/RAD) - 90, 1, 0, 0);
                    glColor4f(pt.r, pt.g, pt.b, 1.0f-sz/pt.sz);

                    glRotatef(lastmillis/7.0f, 0, 0, 1);
                    glScalef(-sz, sz, -sz);
                    drawexplosion(inside);
                    glPopMatrix(); 
                    xtraverts += heminumverts;
                    break;
                }
               
                case PT_SHOTLINE:
                    glVertex3f(p->o.x, p->o.y, p->o.z);
                    glVertex3f(p->d.x, p->d.y, p->d.z);
                    xtraverts += 2;
                    break;

                case PT_DECAL:
                {
                    sqr *s = S((int)p->o.x, (int)p->o.y);
                    glColor4f(s->r/127.5f, s->g/127.5f, s->b/127.5, max(0, min((p->millis+p->fade - lastmillis)/1000.0f, 0.7f)));
                    vec dx(0, 0, 0), dy(0, 0, 0);
                    loopk(3) if(p->d[k]) { dx[(k+1)%3] = -1; dy[(k+2)%3] = p->d[k]; break; } 
                    glTexCoord2i(0, 1); glVertex3f(p->o.x+(-dx.x+dy.x)*pt.sz, p->o.y+(-dx.y+dy.y)*pt.sz, p->o.z+(-dx.z+dy.z)*pt.sz);
                    glTexCoord2i(1, 1); glVertex3f(p->o.x+( dx.x+dy.x)*pt.sz, p->o.y+( dx.y+dy.y)*pt.sz, p->o.z+( dx.z+dy.z)*pt.sz);
                    glTexCoord2i(1, 0); glVertex3f(p->o.x+( dx.x-dy.x)*pt.sz, p->o.y+( dx.y-dy.y)*pt.sz, p->o.z+( dx.z-dy.z)*pt.sz);
                    glTexCoord2i(0, 0); glVertex3f(p->o.x+(-dx.x-dy.x)*pt.sz, p->o.y+(-dx.y-dy.y)*pt.sz, p->o.z+(-dx.z-dy.z)*pt.sz);
                    xtraverts += 4;
                    break;
                }
            }
   
            if(!time) pp = &p->next;
            else if(lastmillis-p->millis>p->fade)
            {
                *pp = p->next;
                p->next = parempty;
                parempty = p;
            }
            else
            {
			    if(pt.gr) p->o.z -= ((lastmillis-p->millis)/3.0f)*time/(pt.gr*10000);
                if(pt.type==PT_PART) p->o.add(vec(p->d).mul(time/20000.0f));
                pp = &p->next;
            }
        }
     
        switch(pt.type)
        {
            case PT_PART:
            case PT_SHOTLINE:
            case PT_DECAL:
                glEnd();
                break;

            case PT_FIREBALL:
                cleanupexplosion();
                break;
        }      
        if(pt.tex<0) glEnable(GL_TEXTURE_2D);
    }

    if(rendered)
    {
        glEnable(GL_FOG);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
}


void particle_splash(int type, int num, int fade, vec &p)
{
    loopi(num)
    {
        const int radius = type==5 ? 50 : 150;
        int x, y, z;
        do
        {
            x = rnd(radius*2)-radius;
            y = rnd(radius*2)-radius;
            z = rnd(radius*2)-radius;
        }
        while(x*x+y*y+z*z>radius*radius);
        vec d((float)x, (float)y, (float)z);
        newparticle(p, d, rnd(fade*3), type);
    }
}

void particle_trail(int type, int fade, vec &s, vec &e)
{
    vec v;
    float d = e.dist(s, v); 
    v.div(d*2+0.1f);
    vec p = s;
    loopi((int)d*2)
    {
        p.add(v);
        vec d((float)(rnd(11)-5), (float)(rnd(11)-5), (float)(rnd(11)-5));
        newparticle(p, d, rnd(fade)+fade, type);
    }
}

void particle_fireball(int type, vec &o)
{
    newparticle(o, vec(0, 0, 0), (int)((parttypes[type].sz-1.0f)*100.0f), type);
}

VARP(holettl, 0, 10000, 30000);

bool addbullethole(vec &from, vec &to, float radius)
{
    if(!holettl) return false;
    vec surface, ray(to);
    ray.sub(from);
    ray.normalize();
    float dist = raycube(from, ray, surface), mag = to.dist(from);
    if(surface.iszero() || (radius>0 && (dist < mag-radius || dist > mag+radius))) return false;
    vec o(from);
    o.add(ray.mul(dist));
    o.add(vec(surface).mul(0.005f));
    newparticle(o, surface, holettl, 7);
    return true;
}

void addshotline(dynent *pl, vec &from, vec &to)
{
    if(pl == player1) return;
    if(rnd(3)) return;
       
    int start = 10;
    if(camera1->o.dist(to) <= 10.0f) start = 8;
    else start = 5;

    vec unitv;
    float dist = to.dist(from, unitv);
    unitv.div(dist);

    vec o = unitv;
    o.mul(dist/10+start).add(from);
    vec d = unitv;
    d.mul(dist/10*-(10-start-2)).add(to);
    newparticle(o, d, 75, 6);
}   

