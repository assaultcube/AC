// renderparticles.cpp

#include "cube.h"

static GLushort *hemiindices = NULL;
static vec *hemiverts = NULL;
static int heminumverts = 0, heminumindices = 0;

static void subdivide(int depth, int face);

static void genface(int depth, int i1, int i2, int i3)
{
    int face = heminumindices; heminumindices += 3;
    hemiindices[face]   = i1;
    hemiindices[face+1] = i2;
    hemiindices[face+2] = i3;
    subdivide(depth, face);
}

static void subdivide(int depth, int face)
{
    if(depth-- <= 0) return;
    int idx[6];
    loopi(3) idx[i] = hemiindices[face+i];
    loopi(3)
    {
        int vert = heminumverts++;
        hemiverts[vert] = vec(hemiverts[idx[i]]).add(hemiverts[idx[(i+1)%3]]).normalize(); //push on to unit sphere
        idx[3+i] = vert;
        hemiindices[face+i] = vert;
    }
    subdivide(depth, face);
    loopi(3) genface(depth, idx[i], idx[3+i], idx[3+(i+2)%3]);
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
    loopi(hres) genface(depth, 0, i+1, 1+(i+1)%hres);
}

GLuint createexpmodtex(int size, float minval)
{
    uchar *data = new uchar[size*size], *dst = data;
    loop(y, size) loop(x, size)
    {
        float dx = 2*float(x)/(size-1) - 1, dy = 2*float(y)/(size-1) - 1;
        float z = max(0.0f, 1.0f - dx*dx - dy*dy);
        if(minval) z = sqrtf(z);
        else loopk(2) z *= z;
        *dst++ = uchar(max(z, minval)*255);
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    createtexture(tex, size, size, data, 3, true, false, GL_ALPHA);
    delete[] data;
    return tex;
}

static struct expvert
{
    vec pos;
    float u, v, s, t;
} *expverts = NULL;

static GLuint expmodtex[2] = {0, 0};
static GLuint lastexpmodtex = 0;

VARP(mtexplosion, 0, 1, 1);

void setupexplosion()
{
    if(!hemiindices) inithemisphere(5, 2);

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
            e.s = v.x*0.5f + 0.5f;
            e.t = v.y*0.5f + 0.5f;
            float wobble = v.dot(center) + 0.002f*lastmillis;
            wobble -= floor(wobble);
            wobble = 1.0f + fabs(wobble - 0.5f)*0.5f;
            e.pos = vec(v).mul(wobble);
        }
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(expvert), &expverts->pos);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, sizeof(expvert), &expverts->u);

    if(mtexplosion && maxtmus>=2)
    {
        setuptmu(0, "C * T", "= Ca");

        glActiveTexture_(GL_TEXTURE1_ARB);
        glClientActiveTexture_(GL_TEXTURE1_ARB);

        glEnable(GL_TEXTURE_2D);
        setuptmu(1, "P * Ta x 4", "Pa * Ta x 4");
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, sizeof(expvert), &expverts->s);

        glActiveTexture_(GL_TEXTURE0_ARB);
        glClientActiveTexture_(GL_TEXTURE0_ARB);

        if(!expmodtex[0]) expmodtex[0] = createexpmodtex(64, 0);
        if(!expmodtex[1]) expmodtex[1] = createexpmodtex(64, 0.25f);
        lastexpmodtex = 0;
    }
}

void drawexplosion(bool inside, float r, float g, float b, float a)
{
    if(mtexplosion && maxtmus>=2 && lastexpmodtex != expmodtex[inside ? 1 : 0])
    {
        glActiveTexture_(GL_TEXTURE1_ARB);
        lastexpmodtex = expmodtex[inside ? 1 :0];
        glBindTexture(GL_TEXTURE_2D, lastexpmodtex);
        glActiveTexture_(GL_TEXTURE0_ARB);
    }
    loopi(!reflecting && inside ? 2 : 1)
    {
        glColor4f(r, g, b, i ? a/2 : a);
        if(i)
        {
            glScalef(1, 1, -1);
            glDepthFunc(GL_GEQUAL);
        }
        if(inside)
        {
            if(!reflecting)
            {
                glCullFace(GL_BACK);
                glDrawElements(GL_TRIANGLES, heminumindices, GL_UNSIGNED_SHORT, hemiindices);
                glCullFace(GL_FRONT);
            }
            glScalef(1, 1, -1);
        }
        glDrawElements(GL_TRIANGLES, heminumindices, GL_UNSIGNED_SHORT, hemiindices);
        if(i) glDepthFunc(GL_LESS);
    }
}

void cleanupexplosion()
{
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    if(mtexplosion && maxtmus>=2)
    {
        resettmu(0);

        glActiveTexture_(GL_TEXTURE1_ARB);
        glClientActiveTexture_(GL_TEXTURE1_ARB);

        glDisable(GL_TEXTURE_2D);
        resettmu(1);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);

        glActiveTexture_(GL_TEXTURE0_ARB);
        glClientActiveTexture_(GL_TEXTURE0_ARB);
    }
}

#define MAXPARTYPES 22

struct particle { vec o, d; int fade, type; int millis; particle *next; };
particle *parlist[MAXPARTYPES], *parempty = NULL;

static Texture *parttex[7];

void particleinit()
{
    loopi(MAXPARTYPES) parlist[i] = NULL;

    parttex[0] = textureload("packages/misc/base.png");
    parttex[1] = textureload("packages/misc/smoke.png");
    parttex[2] = textureload("packages/misc/explosion.png");
    parttex[3] = textureload("<decal>packages/misc/bullethole.png");
    parttex[4] = textureload("packages/misc/blood.png");
    parttex[5] = textureload("packages/misc/scorch.png");
    parttex[6] = textureload("packages/misc/muzzleflash.jpg");
}

void cleanupparticles()
{
    loopi(2) if(expmodtex[i]) { glDeleteTextures(1, &expmodtex[i]); expmodtex[i] = 0; }
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
    if(OUTBORD((int)o.x, (int)o.y)) return;

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

static struct parttype { int type; float r, g, b; int gr, tex; float sz; } parttypes[] =
{
    { PT_PART,       0.4f, 0.4f, 0.4f, 2,  0, 0.06f }, // yellow: sparks
    { PT_PART,       1.0f, 1.0f, 1.0f, 20, 1, 0.15f }, // grey:   small smoke
    { PT_PART,       0.0f, 0.0f, 1.0f, 20, 0, 0.08f }, // blue:   edit mode closest ent
    { PT_BLOOD,      0.5f, 0.0f, 0.0f, 1,  4, 0.3f  }, // red:    blood spats
    { PT_PART,       1.0f, 0.1f, 0.1f, 0,  1, 0.2f  }, // red:    demotrack
    { PT_FIREBALL,   1.0f, 0.5f, 0.5f, 0,  2, 7.0f  }, // explosion fireball
    { PT_SHOTLINE,   1.0f, 1.0f, 0.7f, 0, -1, 0.0f  }, // yellow: shotline
    { PT_BULLETHOLE, 1.0f, 1.0f, 1.0f, 0,  3, 0.3f  }, // hole decal
    
    { PT_STAIN,      0.5f, 0.0f, 0.0f, 0,  4, 0.6f  }, // red:    blood stain
    { PT_DECAL,      1.0f, 1.0f, 1.0f, 0,  5, 1.5f  }, // scorch decal
    { PT_HUDFLASH,   1.0f, 1.0f, 1.0f, 0,  6, 0.7f  }, // hudgun muzzle flash
    { PT_FLASH,      1.0f, 1.0f, 1.0f, 0,  6, 0.7f  }, // muzzle flash
    { PT_PART,       1.0f, 1.0f, 1.0f, 20, 0, 0.08f }, // white: edit mode ent type : light
    { PT_PART,       0.0f, 1.0f, 0.0f, 20, 0, 0.08f }, // green: edit mode ent type : spawn
    { PT_PART,       1.0f, 0.0f, 0.0f, 20, 0, 0.08f }, // red: edit mode ent type : ammo
    { PT_PART,       1.0f, 1.0f, 0.0f, 20, 0, 0.08f }, // yellow: edit mode ent type : pickup
    { PT_PART,       1.0f, 0.0f, 1.0f, 20, 0, 0.08f }, // magenta: edit mode ent type : model, sound
    { PT_PART,       1.0f, 0.5f, 0.2f, 20, 0, 0.08f }, // orange: edit mode ent type : "carrot"
    { PT_PART,       0.5f, 0.5f, 0.5f, 20, 0, 0.08f }, // grey: edit mode ent type : ladder, (pl)clip
    { PT_PART,       0.0f, 1.0f, 1.0f, 20, 0, 0.08f }, // turquoise: edit mode ent type : CTF-flag
    // 2011jun18 : shotty decals
    { PT_BULLETHOLE, 0.2f, 0.2f, 1.0f, 0,  3, 0.1f  }, // hole decal M
    { PT_BULLETHOLE, 0.2f, 1.0f, 0.2f, 0,  3, 0.1f  }, // hole decal C
};

VAR(particlesize, 20, 100, 500);

VARP(blood, 0, 1, 1);
VARP(bloodttl, 0, 10000, 30000);

void render_particles(int time, int typemask)
{
    bool rendered = false;
    for(int i = MAXPARTYPES-1; i>=0; i--) if(typemask&(1<<parttypes[i].type) && parlist[i])
    {
        if(!rendered)
        {
            rendered = true;
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glDisable(GL_FOG);
        }

        parttype &pt = parttypes[i];
        float sz = pt.sz*particlesize/100.0f;

        if(pt.tex>=0) glBindTexture(GL_TEXTURE_2D, parttex[pt.tex]->id);
        else glDisable(GL_TEXTURE_2D);
        switch(pt.type)
        {
            case PT_HUDFLASH:
                sethudgunperspective(true);
                // fall through
            case PT_FLASH:
                glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
                glColor3f(pt.r, pt.g, pt.b);
                glBegin(GL_QUADS);
                break;

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

            case PT_BULLETHOLE:
                glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
                glBegin(GL_QUADS);
                break;

            case PT_BLOOD:
                glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
                glColor3f(1-pt.r, 1-pt.g, 1-pt.b);
                glBegin(GL_QUADS);
                break;

            case PT_STAIN:
                glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
                glBegin(GL_QUADS);
                break;
        }

        for(particle *p, **pp = &parlist[i]; (p = *pp);)
        {
            switch(pt.type)
            {
                case PT_HUDFLASH:
                case PT_FLASH:
                {
                    vec corners[4] =
                    {
                        vec(-camright.x+camup.x, -camright.y+camup.y, -camright.z+camup.z),
                        vec( camright.x+camup.x,  camright.y+camup.y,  camright.z+camup.z),
                        vec( camright.x-camup.x,  camright.y-camup.y,  camright.z-camup.z),
                        vec(-camright.x-camup.x, -camright.y-camup.y, -camright.z-camup.z)
                    };
                    loopk(4) corners[k].rotate(p->d.x, p->d.y, camdir).mul(sz*p->d.z).add(p->o);

                    glTexCoord2i(0, 1); glVertex3fv(corners[0].v);
                    glTexCoord2i(1, 1); glVertex3fv(corners[1].v);
                    glTexCoord2i(1, 0); glVertex3fv(corners[2].v);
                    glTexCoord2i(0, 0); glVertex3fv(corners[3].v);

                    xtraverts += 4;
                    break;
                }

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
                    if(reflecting && !refracting) oc.z = p->o.z - (hdr.waterlevel-0.3f);
                    glRotatef(inside ? camera1->yaw - 180 : atan2(oc.y, oc.x)/RAD - 90, 0, 0, 1);
                    glRotatef((inside ? camera1->pitch : asin(oc.z/oc.magnitude())/RAD) - 90, 1, 0, 0);

                    glRotatef(lastmillis/7.0f, 0, 0, 1);
                    glScalef(-sz, sz, -sz);
                    drawexplosion(inside, pt.r, pt.g, pt.b, 1.0f-sz/pt.sz);
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
                    glColor4f(s->r/127.5f, s->g/127.5f, s->b/127.5, max(0.0f, min((p->millis+p->fade - lastmillis)/1000.0f, 0.7f)));
                    vec dx(0, 0, 0), dy(0, 0, 0);
                    loopk(3) if(p->d[k]) { dx[(k+1)%3] = -1; dy[(k+2)%3] = p->d[k]; break; }
                    int o = detrnd((size_t)p, 11);
                    static const int tc[4][2] = { {0, 1}, {1, 1}, {1, 0}, {0, 0} };
                    glTexCoord2i(tc[o%4][0], tc[o%4][1]); glVertex3f(p->o.x+(-dx.x+dy.x)*pt.sz, p->o.y+(-dx.y+dy.y)*pt.sz, p->o.z+(-dx.z+dy.z)*pt.sz);
                    glTexCoord2i(tc[(o+1)%4][0], tc[(o+1)%4][1]); glVertex3f(p->o.x+( dx.x+dy.x)*pt.sz, p->o.y+( dx.y+dy.y)*pt.sz, p->o.z+( dx.z+dy.z)*pt.sz);
                    glTexCoord2i(tc[(o+2)%4][0], tc[(o+2)%4][1]); glVertex3f(p->o.x+( dx.x-dy.x)*pt.sz, p->o.y+( dx.y-dy.y)*pt.sz, p->o.z+( dx.z-dy.z)*pt.sz);
                    glTexCoord2i(tc[(o+3)%4][0], tc[(o+3)%4][1]); glVertex3f(p->o.x+(-dx.x-dy.x)*pt.sz, p->o.y+(-dx.y-dy.y)*pt.sz, p->o.z+(-dx.z-dy.z)*pt.sz);
                    xtraverts += 4;
                    break;
                }

                case PT_BULLETHOLE:
                {
                    float blend = max(0.0f, min((p->millis+p->fade - lastmillis)/1000.0f, 1.0f));
                    glColor4f(pt.r*blend, pt.g*blend, pt.b*blend, blend);
                    vec dx(0, 0, 0), dy(0, 0, 0);
                    int tx = 0, ty = 1;
                    loopk(3) if(p->d[k])
                    {
                        dx[(k+1)%3] = -1; dy[(k+2)%3] = p->d[k];
                        if(k<2) { tx = k^1; ty = 2; }
                        break;
                    }
                    glTexCoord2f(0.5f+0.5f*(-dx[tx]+dy[tx]), 0.5f+0.5f*(-dx[ty]+dy[ty])); glVertex3f(p->o.x+(-dx.x+dy.x)*pt.sz, p->o.y+(-dx.y+dy.y)*pt.sz, p->o.z+(-dx.z+dy.z)*pt.sz);
                    glTexCoord2f(0.5f+0.5f*( dx[tx]+dy[tx]), 0.5f+0.5f*( dx[ty]+dy[ty])); glVertex3f(p->o.x+( dx.x+dy.x)*pt.sz, p->o.y+( dx.y+dy.y)*pt.sz, p->o.z+( dx.z+dy.z)*pt.sz);
                    glTexCoord2f(0.5f+0.5f*( dx[tx]-dy[tx]), 0.5f+0.5f*( dx[ty]-dy[ty])); glVertex3f(p->o.x+( dx.x-dy.x)*pt.sz, p->o.y+( dx.y-dy.y)*pt.sz, p->o.z+( dx.z-dy.z)*pt.sz);
                    glTexCoord2f(0.5f+0.5f*(-dx[tx]-dy[tx]), 0.5f+0.5f*(-dx[ty]-dy[ty])); glVertex3f(p->o.x+(-dx.x-dy.x)*pt.sz, p->o.y+(-dx.y-dy.y)*pt.sz, p->o.z+(-dx.z-dy.z)*pt.sz);
                    xtraverts += 4;
                    break;
                }

                case PT_BLOOD:
                {
                    int n = detrnd((size_t)p, 4), o = detrnd((size_t)p, 11);;
                    float tx = 0.5f*(n&1), ty = 0.5f*((n>>1)&1), tsz = 0.5f;
                    static const int tc[4][2] = { {0, 1}, {1, 1}, {1, 0}, {0, 0} };
                    glTexCoord2f(tx+tsz*tc[o%4][0], ty+tsz*tc[o%4][1]); glVertex3f(p->o.x+(-camright.x+camup.x)*sz, p->o.y+(-camright.y+camup.y)*sz, p->o.z+(-camright.z+camup.z)*sz);
                    glTexCoord2f(tx+tsz*tc[(o+1)%4][0], ty+tsz*tc[(o+1)%4][1]); glVertex3f(p->o.x+( camright.x+camup.x)*sz, p->o.y+( camright.y+camup.y)*sz, p->o.z+( camright.z+camup.z)*sz);
                    glTexCoord2f(tx+tsz*tc[(o+2)%4][0], ty+tsz*tc[(o+2)%4][1]); glVertex3f(p->o.x+( camright.x-camup.x)*sz, p->o.y+( camright.y-camup.y)*sz, p->o.z+( camright.z-camup.z)*sz);
                    glTexCoord2f(tx+tsz*tc[(o+3)%4][0], ty+tsz*tc[(o+3)%4][1]); glVertex3f(p->o.x+(-camright.x-camup.x)*sz, p->o.y+(-camright.y-camup.y)*sz, p->o.z+(-camright.z-camup.z)*sz);
                    xtraverts += 4;
                    break;
                }

                case PT_STAIN:
                {
                    float blend = max(0.0f, min((p->millis+p->fade - lastmillis)/1000.0f, 1.0f));
                    glColor3f(blend*(1-pt.r), blend*(1-pt.g), blend*(1-pt.b));
                    int n = detrnd((size_t)p, 4), o = detrnd((size_t)p, 11);
                    float tx = 0.5f*(n&1), ty = 0.5f*((n>>1)&1), tsz = 0.5f;
                    static const int tc[4][2] = { {0, 1}, {1, 1}, {1, 0}, {0, 0} };
                    vec dx(0, 0, 0), dy(0, 0, 0);
                    loopk(3) if(p->d[k]) { dx[(k+1)%3] = -1; dy[(k+2)%3] = p->d[k]; break; }
                    glTexCoord2f(tx+tsz*tc[o%4][0], ty+tsz*tc[o%4][1]); glVertex3f(p->o.x+(-dx.x+dy.x)*pt.sz, p->o.y+(-dx.y+dy.y)*pt.sz, p->o.z+(-dx.z+dy.z)*pt.sz);
                    glTexCoord2f(tx+tsz*tc[(o+1)%4][0], ty+tsz*tc[(o+1)%4][1]); glVertex3f(p->o.x+( dx.x+dy.x)*pt.sz, p->o.y+( dx.y+dy.y)*pt.sz, p->o.z+( dx.z+dy.z)*pt.sz);
                    glTexCoord2f(tx+tsz*tc[(o+2)%4][0], ty+tsz*tc[(o+2)%4][1]); glVertex3f(p->o.x+( dx.x-dy.x)*pt.sz, p->o.y+( dx.y-dy.y)*pt.sz, p->o.z+( dx.z-dy.z)*pt.sz);
                    glTexCoord2f(tx+tsz*tc[(o+3)%4][0], ty+tsz*tc[(o+3)%4][1]); glVertex3f(p->o.x+(-dx.x-dy.x)*pt.sz, p->o.y+(-dx.y-dy.y)*pt.sz, p->o.z+(-dx.z-dy.z)*pt.sz);
                    xtraverts += 4;
                    break;
                }
            }

            if(time < 0) pp = &p->next;
            else if(!p->fade || lastmillis-p->millis>p->fade)
            {
                *pp = p->next;
                p->next = parempty;
                parempty = p;
            }
            else
            {
                if(pt.gr) p->o.z -= ((lastmillis-p->millis)/3.0f)*time/(pt.gr*10000);
                if(pt.type==PT_PART || pt.type==PT_BLOOD)
                {
                    p->o.add(vec(p->d).mul(time/20000.0f));
                    if(OUTBORD((int)p->o.x, (int)p->o.y))
                    {
                        *pp = p->next;
                        p->next = parempty;
                        parempty = p;
                        continue;
                    }
                }
                if(pt.type==PT_BLOOD)
                {
                    sqr *s = S((int)p->o.x, (int)p->o.y);
                    if(s->type==SPACE && p->o.z<=s->floor)
                    {
                        *pp = p->next;
                        p->next = parempty;
                        parempty = p;
                        newparticle(vec(p->o.x, p->o.y, s->floor+0.005f), vec(0, 0, 1), bloodttl, 8);
                        continue;
                    }
                }
                pp = &p->next;
            }
        }

        switch(pt.type)
        {
            case PT_HUDFLASH:
                glEnd();
                sethudgunperspective(false);
                break;

            case PT_PART:
            case PT_SHOTLINE:
            case PT_DECAL:
            case PT_BULLETHOLE:
            case PT_BLOOD:
            case PT_STAIN:
            case PT_FLASH:
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

void particle_emit(int type, int *args, int basetime, int seed, const vec &p)
{
    if(type<0 || type>=MAXPARTYPES) return;
    parttype &pt = parttypes[type];
    if(pt.type==PT_FIREBALL) particle_fireball(type, p);
    else if(pt.type==PT_FLASH || pt.type==PT_HUDFLASH)
    {
        if(lastmillis - basetime < args[0])
            particle_flash(type, args[1]>0 ? args[1]/100.0f : 1.0f, seed%360, p);
    }
    else particle_splash(type, args[0], args[1], p);
}

void particle_flash(int type, float scale, float angle, const vec &p)
{
    angle *= RAD;
    newparticle(p, vec(cosf(angle), sinf(angle), scale), 0, type);
}

void particle_splash(int type, int num, int fade, const vec &p)
{
    if(parttypes[type].type==PT_BLOOD && !blood) return;
    loopi(num)
    {
        const int radius = 150;
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

VARP(maxtrail, 1, 500, 10000);

void particle_trail(int type, int fade, const vec &s, const vec &e)
{
    vec v;
    float d = e.dist(s, v);
    int steps = clamp(int(d*2), 1, maxtrail);
    v.div(steps);
    vec p = s;
    loopi(steps)
    {
        p.add(v);
        vec tmp((float)(rnd(11)-5), (float)(rnd(11)-5), (float)(rnd(11)-5));
        newparticle(p, tmp, rnd(fade)+fade, type);
    }
}

void particle_fireball(int type, const vec &o)
{
    newparticle(o, vec(0, 0, 0), (int)((parttypes[type].sz-1.0f)*100.0f), type);
}

VARP(bulletbouncesound, 0, 1, 1);
VARP(bullethole, 0, 1, 1);
VARP(bulletholettl, 0, 10000, 30000);
VARP(bulletbouncesoundrad, 0, 15, 1000);

// 2011jun18: shotty decals
//bool addbullethole(dynent *d, const vec &from, const vec &to, float radius, bool noisy)
bool addbullethole(dynent *d, const vec &from, const vec &to, float radius, bool noisy, int type)
{
    if(!bulletholettl || !bullethole) return false;
    vec surface, ray(to);
    ray.sub(from);
    ray.normalize();
    float dist = raycube(from, ray, surface), mag = to.dist(from);
    if(surface.iszero() || (radius>0 && (dist < mag-radius || dist > mag+radius))) return false;
    vec o(from);
    o.add(ray.mul(dist));
    o.add(vec(surface).normalize().mul(0.01f));
    // 2011jun18: shotty decals
    int tf = type > 0 ? ( type > 1 ? 21 : 20 ) : 7;
    newparticle(o, surface, bulletholettl, tf);
    //newparticle(o, surface, bulletholettl, 7);
    if(noisy && bulletbouncesound && bulletbouncesoundrad && d!=player1 && o.dist(camera1->o) <= bulletbouncesoundrad)
    {
        audiomgr.playsound(o.z<hdr.waterlevel ? S_BULLETWATERHIT : S_BULLETHIT, &o, SP_LOW);
    }
    return true;
}


VARP(scorch, 0, 1, 1);
VARP(scorchttl, 0, 10000, 30000);

bool addscorchmark(vec &o, float radius)
{
    if(!scorchttl || !scorch) return false;
    sqr *s = S((int)o.x, (int)o.y);
    if(s->type!=SPACE || o.z-s->floor>radius) return false;
    newparticle(vec(o.x, o.y, s->floor+0.02f), vec(0, 0, 1), scorchttl, 9);
    return true;
}

VARP(shotline, 0, 1, 1);
VARP(shotlinettl, 0, 75, 10000);
VARP(bulletairsound, 0, 1, 1);
VARP(bulletairsoundrad, 0, 15, 1000);
VARP(bulletairsoundsourcerad, 0, 8, 1000);
VARP(bulletairsounddestrad, 0, 8, 1000);

void addshotline(dynent *pl, const vec &from, const vec &to)
{
    if(pl == player1 || !shotlinettl || !shotline) return;
    bool fx = shotlinettl > 0 && (player1->isspectating() || !multiplayer(false)); // allow fx only in spect mode and locally
    if(rnd(3) && !fx) return; // show all shotlines when fx enabled

    int start = (camera1->o.dist(to) <= 10.0f) ? 8 : 5;
    vec unitv;
    float dist = to.dist(from, unitv);
    unitv.div(dist);

    // shotline visuals
    vec o = unitv;
    o.mul(dist/10+start).add(from);
    vec d = unitv;
    d.mul(dist/10*-(10-start-2)).add(to);
    newparticle(o, d, fx ? shotlinettl : min(75, shotlinettl), 6);

    // shotline sound fx
    if(!bulletairsoundrad) return;
    vec fromuv, touv;
    float fd = camera1->o.dist(from, fromuv);
    float td = camera1->o.dist(to, touv);
    if(fd <= (float)bulletairsoundsourcerad || td <= (float)bulletairsounddestrad) return; // ignore nearly fired/detonated shots
    vec soundpos(unitv);
    soundpos.mul(fd/(fd+td)*dist);
    soundpos.add(from);
    if(!bulletairsound || soundpos.dist(camera1->o) > bulletairsoundrad) return; // outside player radius
    audiomgr.playsound(S_BULLETAIR1 + rnd(2), &soundpos, SP_LOW);
}

