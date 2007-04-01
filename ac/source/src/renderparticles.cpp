// renderparticles.cpp

#include "cube.h"

#define MAXPARTYPES 7

struct particle { vec o, d; int fade, type; int millis; particle *next; };
particle *parlist[MAXPARTYPES], *parempty = NULL;

static Texture *parttex[3];

void particleinit()
{
    loopi(MAXPARTYPES) parlist[i] = NULL;

    parttex[0] = textureload("packages/misc/base.png");
    parttex[1] = textureload("packages/misc/smoke.png");
    parttex[2] = textureload("packages/misc/explosion.jpg");
   
    GLUquadricObj *qsphere = gluNewQuadric();
    if(!qsphere) fatal("glu sphere");
    gluQuadricDrawStyle(qsphere, GLU_FILL);
    gluQuadricOrientation(qsphere, GLU_INSIDE);
    gluQuadricTexture(qsphere, GL_TRUE);
    glNewList(1, GL_COMPILE);
    gluSphere(qsphere, 1, 12, 6);
    glEndList();
    gluDeleteQuadric(qsphere);
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
    PT_SHOTLINE
};

static struct parttype { int type; float r, g, b; int gr, tex; float sz; } parttypes[] =
{
    { 0,           0.4f, 0.4f, 0.4f, 2,  0, 0.06f }, // yellow: sparks 
    { 0,           1.0f, 1.0f, 1.0f, 20, 1, 0.15f }, // grey:   small smoke
    { 0,           0.2f, 0.2f, 1.0f, 20, 0, 0.08f }, // blue:   edit mode entities
    { 0,           1.0f, 0.1f, 0.1f, 1,  1, 0.06f }, // red:    blood spats
    { 0,           1.0f, 0.1f, 0.1f, 0,  1, 0.2f  }, // red:    demotrack
    { PT_FIREBALL, 1.0f, 1.0f, 1.0f, 0,  2, 7.0f  }, // explosion fireball
    { PT_SHOTLINE, 1.0f, 1.0f, 0.7f, 0, -1, 0.0f  }  // yellow: shotline
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
    loopi(MAXPARTYPES) if(parlist[i])
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
            case PT_PART:
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glColor3f(pt.r, pt.g, pt.b);
                glBegin(GL_QUADS);
                break;

            case PT_FIREBALL:
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                break;

            case PT_SHOTLINE:
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColor4f(pt.r, pt.g, pt.b, 0.5f);
                glBegin(GL_LINES);
                break;
        }
         
        for(particle *p, **pp = &parlist[i]; (p = *pp);)
        {       
            switch(pt.type)
            {
                case PT_PART:
                    glTexCoord2i(0, 1); glVertex3f(p->o.x+(-camright.x+camup.x)*sz, p->o.z+(-camright.y+camup.y)*sz, p->o.y+(-camright.z+camup.z)*sz);
                    glTexCoord2i(1, 1); glVertex3f(p->o.x+( camright.x+camup.x)*sz, p->o.z+( camright.y+camup.y)*sz, p->o.y+( camright.z+camup.z)*sz);
                    glTexCoord2i(1, 0); glVertex3f(p->o.x+( camright.x-camup.x)*sz, p->o.z+( camright.y-camup.y)*sz, p->o.y+( camright.z-camup.z)*sz);
                    glTexCoord2i(0, 0); glVertex3f(p->o.x+(-camright.x-camup.x)*sz, p->o.z+(-camright.y-camup.y)*sz, p->o.y+(-camright.z-camup.z)*sz);
                    xtraverts += 4;
                    break;
                
                case PT_FIREBALL:
                    sz = 1.0f + (pt.sz-1.0f)*min(p->fade, lastmillis-p->millis)/p->fade;
                    glColor4f(pt.r, pt.g, pt.b, 1.0f-sz/pt.sz);
                    glPushMatrix();
                    glTranslatef(p->o.x, p->o.z, p->o.y);
                    glRotatef(lastmillis/5.0f, 1, 1, 1);
                    glScalef(sz, sz, sz);
                    glCallList(1); 
                    glScalef(0.8f, 0.8f, 0.8f);
                    glCallList(1);
                    glPopMatrix(); 
                    xtraverts += 12*6*2;
                    break;
                
                case PT_SHOTLINE:
                    glVertex3f(p->o.x, p->o.z, p->o.y);
                    glVertex3f(p->d.x, p->d.z, p->d.y);
                    xtraverts += 2;
                    break;
            }

            if(lastmillis-p->millis>p->fade)
            {
                *pp = p->next;
                p->next = parempty;
                parempty = p;
            }
            else
            {
			    if(pt.gr) p->o.z -= ((lastmillis-p->millis)/3.0f)*time/(pt.gr*10000);
                if(pt.type!=PT_SHOTLINE) p->o.add(vec(p->d).mul(time/20000.0f));
                pp = &p->next;
            }
        }
           
        if(pt.type==PT_PART || pt.type==PT_SHOTLINE) glEnd();
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

