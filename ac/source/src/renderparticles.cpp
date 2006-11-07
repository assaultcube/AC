// renderparticles.cpp

#include "cube.h"

const int MAXPARTICLES = 10500;
const int NUMPARTCUTOFF = 20;
struct particle { vec o, d; int fade, type; int millis; particle *next; int tex; }; // EDIT: AH
particle particles[MAXPARTICLES], *parlist = NULL, *parempty = NULL;
bool parinit = false;

VAR(maxparticles, 100, 2000, MAXPARTICLES-500);

void newparticle(vec &o, vec &d, int fade, int type, int tex = -1)
{
    if(!parinit)
    {
        loopi(MAXPARTICLES)
        {
            particles[i].next = parempty;
            parempty = &particles[i];
        };
        parinit = true;
    };
    if(parempty)
    {
        particle *p = parempty;
        parempty = p->next;
        p->o = o;
        p->d = d;
        p->fade = fade;
        p->type = type;
        p->millis = lastmillis;
        p->next = parlist;
        p->tex = tex;
        parlist = p;
    };
};

VAR(demotracking, 0, 0, 1);
VAR(particlesize, 20, 100, 500); 

vec right, up;

void setorient(vec &r, vec &u) { right = r; up = u; };

void render_particles(int time)
{
	if(demoplayback && demotracking)
	{
		vec nom = { 0, 0, 0 };
		newparticle(player1->o, nom, 100000000, 4);
	};

    if(!parlist) return;

    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);
    glDisable(GL_FOG);

    static Texture *parttex[2] = {NULL, NULL};
    if(!parttex[0]) parttex[0] = textureload("packages/misc/base.png");
    if(!parttex[1]) parttex[1] = textureload("packages/misc/smoke.png");
    static struct parttype { float r, g, b; int gr, tex; float sz; } parttypes[] =
    {
        { 0.2f, 0.2f, 0.2f, 2,  0, 0.06f }, // yellow: sparks 
        { 0.5f, 0.5f, 0.5f, 20, 1, 0.15f }, // grey:   small smoke
        { 0.2f, 0.2f, 1.0f, 20, 0, 0.08f }, // blue:   edit mode entities
        { 1.0f, 0.1f, 0.1f, 1,  1, 0.06f }, // red:    blood spats
        { 1.0f, 0.1f, 0.1f, 0,  1, 0.2f  }, // red:    demotrack

//        { 1.0f, 0.8f, 0.8f, 20, 6, 1.2f  }, // yellow: fireball1
//        { 0.5f, 0.5f, 0.5f, 20, 1, 0.6f  }, // grey:   big smoke   
//        { 1.0f, 1.0f, 1.0f, 20, 8, 1.2f  }, // blue:   fireball2
//        { 1.0f, 1.0f, 1.0f, 20, 9, 1.2f  }, // green:  fireball3
//        { 1.0f, 1.0f, 1.0f, 2,  3, 0.06f }, // 
    };
    
    int numrender = 0;
    
    parttype *lastpt = NULL;
    for(particle *p, **pp = &parlist; p = *pp;)
    {       
//        if(p->type==9 && p->tex!=-1) parttypes[9].tex = p->tex; // hack, AH
        parttype &pt = parttypes[p->type];

        if(&pt!=lastpt)
        {
            if(!lastpt || pt.tex!=lastpt->tex)
            {
                if(lastpt) glEnd();
                glBindTexture(GL_TEXTURE_2D, parttex[pt.tex]->id);
                glBegin(GL_QUADS);
            };
            if(!lastpt || pt.r!=lastpt->r || pt.g!=lastpt->g || pt.b!=lastpt->b)
                glColor3f(pt.r, pt.g, pt.b);
            lastpt = &pt;
        };
        
        float sz = pt.sz*particlesize/100.0f; 
        // perf varray?
        glTexCoord2i(0, 1); glVertex3f(p->o.x+(-right.x+up.x)*sz, p->o.z+(-right.y+up.y)*sz, p->o.y+(-right.z+up.z)*sz);
        glTexCoord2i(1, 1); glVertex3f(p->o.x+( right.x+up.x)*sz, p->o.z+( right.y+up.y)*sz, p->o.y+( right.z+up.z)*sz);
        glTexCoord2i(1, 0); glVertex3f(p->o.x+( right.x-up.x)*sz, p->o.z+( right.y-up.y)*sz, p->o.y+( right.z-up.z)*sz);
        glTexCoord2i(0, 0); glVertex3f(p->o.x+(-right.x-up.x)*sz, p->o.z+(-right.y-up.y)*sz, p->o.y+(-right.z-up.z)*sz);
        xtraverts += 4;

        if(numrender++>maxparticles || (p->fade -= time)<0)
        {
            *pp = p->next;
            p->next = parempty;
            parempty = p;
        }
        else
        {
			if(pt.gr) p->o.z -= ((lastmillis-p->millis)/3.0f)*curtime/(pt.gr*10000);
            vec a = p->d;
            vmul(a, time);
            vdiv(a, 20000.0f);
            vadd(p->o, a);
            pp = &p->next;
        };
    };

    if(lastpt) glEnd();

    glEnable(GL_FOG);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
};

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
        vec d = { (float)x, (float)y, (float)z };
        newparticle(p, d, rnd(fade*3), type);
    };
};

void particle_trail(int type, int fade, vec &s, vec &e)
{
    vdist(d, v, s, e);
    vdiv(v, d*2+0.1f);
    vec p = s;
    loopi((int)d*2)
    {
        vadd(p, v);
        vec d = { float(rnd(11)-5), float(rnd(11)-5), float(rnd(11)-5) };
        newparticle(p, d, rnd(fade)+fade, type);
    };
};
