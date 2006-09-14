// renderextras.cpp: misc gl render code and the HUD

#include "cube.h"

void line(int x1, int y1, float z1, int x2, int y2, float z2)
{
    glBegin(GL_POLYGON);
    glVertex3f((float)x1, z1, (float)y1);
    glVertex3f((float)x1, z1, y1+0.01f);
    glVertex3f((float)x2, z2, y2+0.01f);
    glVertex3f((float)x2, z2, (float)y2);
    glEnd();
    xtraverts += 4;
};

void linestyle(float width, int r, int g, int b)
{
    glLineWidth(width);
    glColor3ub(r,g,b);
};

void box(block &b, float z1, float z2, float z3, float z4)
{
    glBegin(GL_POLYGON);
    glVertex3f((float)b.x,      z1, (float)b.y);
    glVertex3f((float)b.x+b.xs, z2, (float)b.y);
    glVertex3f((float)b.x+b.xs, z3, (float)b.y+b.ys);
    glVertex3f((float)b.x,      z4, (float)b.y+b.ys);
    glEnd();
    xtraverts += 4;
};

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
};

void blendbox(int x1, int y1, int x2, int y2, bool border, int tex)
{
    glDepthMask(GL_FALSE);
    glDisable(GL_TEXTURE_2D);
    if(tex>=0) 
    {
        glBindTexture(GL_TEXTURE_2D, tex);
        glEnable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
		glColor3d(1.0, 1.0, 1.0);

		float texw = 512.0f;
		float texh = texw;
		int cols = (x2-x1)/texw+1;
		int rows = (y2-y1)/texh+1;
		xtraverts += cols*rows*4;
			
		loopj(rows)
		{
			float ytexcut = 0.0f;
			float yboxcut = 0.0f;
			if((j+1)*texh>y2-y1) // cut last row to match the box height*/
			{
				yboxcut = (((j+1)*texh)-(y2-y1));
				ytexcut = (((j+1)*texh)-(y2-y1))/texh;
			}

			loopi(cols)
			{
				float xtexcut = 0.0f;
				float xboxcut = 0.0f;
				if((i+1)*texw>x2-x1)
				{
					xboxcut = (((i+1)*texw)-(x2-x1));
					xtexcut = (((i+1)*texw)-(x2-x1))/texw;
				}

				glBegin(GL_QUADS);
				glTexCoord2f(0, 0);					glVertex2i(x1+texw*i, y1+texh*j);
				glTexCoord2f(1-xtexcut, 0);			glVertex2i(x1+texw*(i+1)-xboxcut, y1+texh*j);
				glTexCoord2f(1-xtexcut, 1-ytexcut);	glVertex2i(x1+texw*(i+1)-xboxcut, y1+texh*(j+1)-yboxcut); 
				glTexCoord2f(0, 1-ytexcut);			glVertex2i(x1+texw*i, y1+texh*(j+1)-yboxcut);
				glEnd();
			}
		}
    }
    else
    {
        if(border) glColor3d(0.7, 0.7, 0.7); //glColor3d(0.5, 0.3, 0.4); 
        else glColor3d(1.0, 1.0, 1.0);
        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);	glVertex2i(x1, y1);
		glTexCoord2f(1, 0);	glVertex2i(x2, y1);
		glTexCoord2f(1, 1); glVertex2i(x2, y2); 
		glTexCoord2f(0, 1);	glVertex2i(x1, y2);
		glEnd();
		xtraverts += 4;
    };

    glDisable(GL_BLEND);
    if(tex>=0) glDisable(GL_TEXTURE_2D);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_POLYGON);
    glColor3d(0.6, 0.6, 0.6); //glColor3d(0.2, 0.7, 0.4); 
    glVertex2i(x1, y1);
    glVertex2i(x2, y1); 
    glVertex2i(x2, y2);
    glVertex2i(x1, y2);
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
};

const int MAXSPHERES = 50;
struct sphere { vec o; float size, max; int type; sphere *next; };
sphere spheres[MAXSPHERES], *slist = NULL, *sempty = NULL;
bool sinit = false;

void newsphere(vec &o, float max, int type)
{
    if(!sinit)
    {
        loopi(MAXSPHERES)
        {
            spheres[i].next = sempty;
            sempty = &spheres[i];
        };
        sinit = true;
    };
    if(sempty)
    {
        sphere *p = sempty;
        sempty = p->next;
        p->o = o;
        p->max = max;
        p->size = 1;
        p->type = type;
        p->next = slist;
        slist = p;
    };
};

void renderspheres(int time)
{
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBindTexture(GL_TEXTURE_2D, 4);  

    for(sphere *p, **pp = &slist; p = *pp;)
    {
        glPushMatrix();
        float size = p->size/p->max;
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f-size);
        glTranslatef(p->o.x, p->o.z, p->o.y);
        glRotatef(lastmillis/5.0f, 1, 1, 1);
        glScalef(p->size, p->size, p->size);
        glCallList(1);
        glScalef(0.8f, 0.8f, 0.8f);
        glCallList(1);
        glPopMatrix();
        xtraverts += 12*6*2;

        if(p->size>p->max)
        {
            *pp = p->next;
            p->next = sempty;
            sempty = p;
        }
        else
        {
            p->size += time/100.0f;   
            pp = &p->next;
        };
    };

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
};

string closeent;
char *entnames[] =
{
    "none?", "light", "playerstart",
    "clips", "ammobox","grenades",
    "health", "armour", "akimbo", 
    "mapmodel", "trigger", 
    "ladder", "ctf-flag", "?", "?", "?", 
};

void renderents()       // show sparkly thingies for map entities in edit mode
{
    closeent[0] = 0;
    if(!editmode) return;
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==NOTUSED) continue;
        vec v = { e.x, e.y, e.z };
        particle_splash(2, 2, 40, v);
    };
    int e = closestent();
    if(e>=0)
    {
        entity &c = ents[e];
        sprintf_s(closeent)("closest entity = %s (%d, %d, %d, %d), selection = (%d, %d)", entnames[c.type], c.attr1, c.attr2, c.attr3, c.attr4, getvar("selxs"), getvar("selys"));
    };
};

void loadsky(char *basename)
{
    static string lastsky = "";
    if(strcmp(lastsky, basename)==0) return;
    char *side[] = { "ft", "bk", "lf", "rt", "dn", "up" };
    int texnum = 14;
    loopi(6)
    {
        sprintf_sd(name)("packages/%s_%s.jpg", basename, side[i]);
        int xs, ys;
        if(!installtex(texnum+i, path(name), xs, ys, true)) conoutf("could not load sky textures");
    };
    strcpy_s(lastsky, basename);
};

COMMAND(loadsky, ARG_1STR);

float cursordepth = 0.9f;
GLint viewport[4];
GLdouble mm[16], pm[16];
vec worldpos;

void readmatrices()
{
    glGetIntegerv(GL_VIEWPORT, viewport);
    glGetDoublev(GL_MODELVIEW_MATRIX, mm);
    glGetDoublev(GL_PROJECTION_MATRIX, pm);
};

// stupid function to cater for stupid ATI linux drivers that return incorrect depth values

float depthcorrect(float d)
{
	return (d<=1/256.0f) ? d*256 : d;
};

// find out the 3d target of the crosshair in the world easily and very acurately.
// sadly many very old cards and drivers appear to fuck up on glReadPixels() and give false
// coordinates, making shooting and such impossible.
// also hits map entities which is unwanted.
// could be replaced by a more acurate version of monster.cpp los() if needed

void readdepth(int w, int h)
{
    glReadPixels(w/2, h/2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &cursordepth);
    double worldx = 0, worldy = 0, worldz = 0;
    gluUnProject(w/2, h/2, depthcorrect(cursordepth), mm, pm, viewport, &worldx, &worldz, &worldy);
    worldpos.x = (float)worldx;
    worldpos.y = (float)worldy;
    worldpos.z = (float)worldz;
    vec r = { (float)mm[0], (float)mm[4], (float)mm[8] };
    vec u = { (float)mm[1], (float)mm[5], (float)mm[9] };
    setorient(r, u);
};

void drawicon(float tx, float ty, int x, int y, int tex = 5, float scaling = 1/3.0f) // EDIT: AH
{
    glBindTexture(GL_TEXTURE_2D, tex);
    glBegin(GL_QUADS);
    tx /= 192;
    ty /= 192;
    float o = scaling;
    int s = 120;
    glTexCoord2f(tx,   ty);   glVertex2i(x,   y);
    glTexCoord2f(tx+o, ty);   glVertex2i(x+s, y);
    glTexCoord2f(tx+o, ty+o); glVertex2i(x+s, y+s);
    glTexCoord2f(tx,   ty+o); glVertex2i(x,   y+s);
    glEnd();
    xtraverts += 4;
};

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
};

VAR(crosshairsize, 0, 15, 50);

int dblend = 0;
void damageblend(int n) { dblend += n; };

VAR(hidestats, 0, 0, 1);
VAR(crosshairfx, 0, 1, 1);

VAR(scopefov, 5, 50, 50);
bool scoped = false;
int oldfov = 100;

void togglescope(bool deactivate)
{
    if(player1->gunselect==GUN_SNIPER)
    {
        if(!scoped) oldfov = getvar("fov");
        scoped = !scoped;
        setvar("fov", scoped ? scopefov : oldfov);
    }
}

void gl_drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater)
{
    readmatrices();
    if(editmode)
    {
        if(cursordepth==1.0f) worldpos = player1->o;
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        cursorupdate();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    };

    glDisable(GL_DEPTH_TEST);
    invertperspective();
    glPushMatrix();  
    glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
    glEnable(GL_BLEND);

    if(dblend || underwater)
    {
        glDepthMask(GL_FALSE);
        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
        glBegin(GL_QUADS);
        if(dblend) glColor3d(0.0f, 0.9f, 0.9f);
        else glColor3d(0.9f, 0.5f, 0.0f);
        glVertex2i(0, 0);
        glVertex2i(VIRTW, 0);
        glVertex2i(VIRTW, VIRTH);
        glVertex2i(0, VIRTH);
        glEnd();
        glDepthMask(GL_TRUE);
        dblend -= min(1, curtime/3);
        if(dblend<0) dblend = 0;
    };

    glEnable(GL_TEXTURE_2D);

    char *command = getcurcommand();
    dynent *player = playerincrosshair();
	dynent *bot = botincrosshair();
    if(command) draw_textf("> %s_", 20, 1570, 2, (int)command);
    else if(closeent[0]) draw_text(closeent, 20, 1570, 2);
    else if(player || (bot && (player=bot))) draw_text(player->name, 20, 1570, 2);

    renderscores();
    if(!rendermenu())
    {
        if(scoped && player1->gunselect == GUN_SNIPER)
        {
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_ALPHA_TEST);
            glAlphaFunc(GL_GREATER, 0.9f);
			glBindTexture(GL_TEXTURE_2D, 10);
            glBegin(GL_QUADS);
            glColor3ub(255,255,255);
            
			glTexCoord2d(0.0, 0.0); glVertex2f(0, 0);
            glTexCoord2d(1.0, 0.0); glVertex2f(VIRTW, 0);
            glTexCoord2d(1.0, 1.0); glVertex2f(VIRTW, VIRTH);
            glTexCoord2d(0.0, 1.0); glVertex2f(0, VIRTH);
			
            glEnd();
            glDisable(GL_ALPHA_TEST);
        }
		else if(player1->gunselect != GUN_SNIPER && !player1->reloading && !player1->state==CS_DEAD)
        {
            glBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);
			bool teammate_in_xhair = player ? (isteam(player->team, player1->team) && player->state!=CS_DEAD) : false;
			glBindTexture(GL_TEXTURE_2D, teammate_in_xhair ? 9 : 1);
            glBegin(GL_QUADS);
            glColor3ub(255,255,255);
            if(crosshairfx)
            {
                if(player1->gunwait && !teammate_in_xhair) glColor3ub(128,128,128);
                else if(player1->health<=25 || teammate_in_xhair) glColor3ub(255,0,0);
                else if(player1->health<=50) glColor3ub(255,128,0);
            };
			float chsize = (float)crosshairsize * (player1->gunselect==GUN_ASSAULT && player1->shots > 3 ? 1.4f : 1.0f) * (teammate_in_xhair ? 2.0f : 1.0f);
            glTexCoord2d(0.0, 0.0); glVertex2f(VIRTW/2 - chsize, VIRTH/2 - chsize);
            glTexCoord2d(1.0, 0.0); glVertex2f(VIRTW/2 + chsize, VIRTH/2 - chsize);
            glTexCoord2d(1.0, 1.0); glVertex2f(VIRTW/2 + chsize, VIRTH/2 + chsize);
            glTexCoord2d(0.0, 1.0); glVertex2f(VIRTW/2 - chsize, VIRTH/2 + chsize);
            glEnd();
        }
    };

    glPopMatrix();

    glPushMatrix();    
    glOrtho(0, VIRTW*4/3, VIRTH*4/3, 0, -1, 1);
    renderconsole();

    if(!hidestats)
    {
        glPopMatrix();
        glPushMatrix();
        glOrtho(0, VIRTW*3/2, VIRTH*3/2, 0, -1, 1);
        draw_textf("fps %d", 3200, 2320, 2, curfps);
        draw_textf("lod %d", 3200, 2390, 2, lod_factor());
        draw_textf("wqd %d", 3200, 2460, 2, nquads); 
        draw_textf("wvt %d", 3200, 2530, 2, curvert);
        draw_textf("evt %d", 3200, 2600, 2, xtraverts);
    };
    
    glPopMatrix();

    if(player1->state==CS_ALIVE)
    {
        glPushMatrix();
        glOrtho(0, VIRTW/2, VIRTH/2, 0, -1, 1);
        draw_textf("%d",  90, 827, 2, player1->health);
        if(player1->armour) draw_textf("%d", 390, 827, 2, player1->armour);
        //draw_textf("%d", 690, 827, 2, player1->mag[player1->gunselect]);
        if (player1->gunselect!=GUN_KNIFE)
        {
            //draw_textf("%d", 690, 827, 2, player1->mag[player1->gunselect]); 
            char gunstats  [64];
            if (player1->gunselect!=GUN_GRENADE)
		sprintf(gunstats,"%i/%i",player1->mag[player1->gunselect],player1->ammo[player1->gunselect]);
	    else sprintf(gunstats,"%i",player1->mag[player1->gunselect]);
            draw_text(gunstats, 690, 827, 2);
            //clean up pointer?
            //delete gunstats;
        }

        glPopMatrix();

        // EDIT: AH - ctf
        #define ctf_scl 0.75
        if(m_ctf)
        {
            glPushMatrix();
            glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glColor4f(1.0f, 1.0f, 1.0f, 0.2f);            
            if(flaginfos[TEAM_CLA].state == CTFF_INBASE) glDisable(GL_BLEND); else glEnable(GL_BLEND);
            drawicon(64*ctf_scl, 0, 1940, 1650, 11, 1.0f/4.0f);    
            if(flaginfos[TEAM_RVSF].state == CTFF_INBASE) glDisable(GL_BLEND); else glEnable(GL_BLEND);
            drawicon(0, 0, 1820, 1650, 11, 1.0f/4.0f); 
            glDisable(GL_BLEND);
            drawicon(rb_team_int(player1->team)*64*ctf_scl, 64*ctf_scl, VIRTW-128-10, 10, 11, 1.0f/4.0f); // shows which team you are
            glPopMatrix();
            
            // big flag icon
            flaginfo &f = flaginfos[rb_opposite(rb_team_int(player1->team))];
            if(f.state==CTFF_STOLEN && f.thief == player1 && f.pick_ack )
            {
                glPushMatrix();
                glOrtho(0, VIRTW/2, VIRTH/2, 0, -1, 1);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                glColor4f(1.0f, 1.0f, 1.0f, (float)(sin(lastmillis / 100.0) + 1) / 2);
                glEnable(GL_BLEND);
                drawicon(rb_opposite(rb_team_int(player1->team))*128*ctf_scl, 128*ctf_scl, 1065, VIRTH/2/3*2, 11, 1.0f/2.0f);
                glPopMatrix();
            };
        };

        glPushMatrix();
        glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
        glDisable(GL_BLEND);
        
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(1.0f, 1.0f, 1.0f, (float) 0.2f+(sin(lastmillis/100)+1)/2);
        
        if(player1->health <= 20)        
        {
            glEnable(GL_BLEND);
            drawicon(64, 0, 20, 1650); //drawicon(128, 128, 20, 1650);
            glDisable(GL_BLEND);
        }
        else drawicon(64, 0, 20, 1650); //drawicon(128, 128, 20, 1650);
        if(player1->armour) drawicon((float)(128), 0, 620, 1650); //if(player1->armour) drawicon((float)(player1->armourtype*64), 0, 620, 1650); 
        int g = player1->gunselect;
        int r = 64;
        if(g>2) { g -= 3; r = 128; };
        
        if(!player1->mag[player1->gunselect] && player1->gunselect != GUN_KNIFE && player1->gunselect != GUN_GRENADE)
        {
            glEnable(GL_BLEND);
            drawicon((float)(g*64), (float)r, 1220, 1650);
            glDisable(GL_BLEND);
        }
        else 
	{
	    if (player1->gunselect==GUN_GRENADE) drawicon(0, 0, 1220, 1650);
	    else drawicon((float)(g*64), (float)r, 1220, 1650);   
        }
	glPopMatrix();
    };

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
};

#define MAXSHOTLINES 100
struct shotline 
{ 
    vec from, to;
    bool inuse;
    int millis;
};
shotline shotlines[MAXSHOTLINES];

void shotlinereset() { loopi(MAXSHOTLINES) shotlines[i].inuse = false; };

void addshotline(dynent *d, vec &from, vec &to)
{
    if(d == player1) return;
    if(rnd(3) != 0) return;
    loopi(MAXSHOTLINES)
    {
        shotline *s = &shotlines[i];
        if(s->inuse) continue;
        int start = 10;
        vdist(_dist, _unitv, player1->o, to);
        if(_dist <= 10.0f) start = 8;
        else start = 5;
        
        vdist(dist, unitv, from, to);
        vdiv(unitv, dist);
        s->inuse = true;
        s->from = unitv;
        vmul(s->from, dist/10*start);
        vadd(s->from, from);
        s->to = unitv;
        vmul(s->to, dist/10*-(10-start-2));
        vadd(s->to, to);
        s->millis = lastmillis;
        return;
    };
};

VAR(shotline_duration, 0, 75, 10000);

void rendershotlines()
{
    loopi(MAXSHOTLINES)
    {
        shotline *s = &shotlines[i];
        if(!s->inuse) continue;
        if(lastmillis-s->millis > shotline_duration) { s->inuse = false; continue; }
        glPushMatrix();
        glDisable(GL_TEXTURE_2D);
        
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_SRC_ALPHA);
        glDisable(GL_FOG);
        
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glBegin(GL_LINES);
            glColor4f(1.0f, 1.0f, 0.7f, 0.5f);
            glVertex3f(s->from.x, s->from.z, s->from.y);
            glVertex3f(s->to.x, s->to.z, s->to.y);
        glEnd();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        
        glEnable(GL_FOG);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        
        glEnable(GL_TEXTURE_2D);
        glPopMatrix();
    };
};

void renderphysents()
{
    loopv(physents)
    {
        physent *p = physents[i];
		if(!p) continue;
		string model;
		float z = p->o.z;
		
		switch(p->state)
		{
			case NADE_THROWED:
				strcpy_s(model, "weapons/grenade/static");
				break;
			case GIB:
			default:
				sprintf_s(model)("misc/gib0%i", (((4*(int)p)+p->timetolife)%3)+1);
				int t = lastmillis-p->millis;
				if(t>p->timetolife-2000)
				{
					t -= p->timetolife-2000;
					z -= t*t/4000000000.0f*t;
				}
				break;
		}
		path(model);
		rendermodel(model, 0, 1, 0, 0, p->o.x, z, p->o.y, p->yaw, p->pitch, false, 2.0f, 100.0f, 0, 0);
    };
};

VAR(gibnum, 0, 6, 1000);
VAR(gibttl, 0, 0, 15000);
VAR(gibspeed, 1, 7, 20);

int numlivingenemies()
{
	int num = 0;
	loopv(players) if(!isteam(players[i]->team, player1->team)) num++;
	loopv(bots) if(!isteam(bots[i]->team, player1->team)) num++;
	return num;
}

void addgib(dynent *d)
{
	if(!d) return;
	playsound(S_GIB, &d->o);

	bool massivegib = gamemode == 9 && numlivingenemies() == 1; 
	
	loopi((massivegib ? 1000 : gibnum))
	{
		physent *p = new_physent();
		p->owner = d;
		p->millis = lastmillis;
		if(massivegib) p->timetolife = 15000;
		else if(gibttl>0) p->timetolife = gibttl;
		else p->timetolife = 5000+rnd(10)*100;

		p->state = GIB;

		p->isphysent = true;
		p->gravity = 20;
		p->timeinair = 0;
		p->onfloor = false;

		p->o = d->o;
		p->o.z -= d->aboveeye;

		float gibvelangle = rnd(360);
		float speed = massivegib ? 20 : gibspeed;
		p->vel.x = sin(gibvelangle)*speed*rnd(1000)/1000.0f;
		p->vel.y = cos(gibvelangle)*speed*rnd(1000)/1000.0f;
		p->vel.z = speed*rnd(1000)/1000.0f;
		vdiv(p->vel, 10.0f);

		p->yaw = (float)rnd(360);
		p->pitch = (float)rnd(360);
		p->rotspeed = 3.0f;
	}
};

void loadingscreen()
{
	glPushMatrix();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
	
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);

	glBindTexture(GL_TEXTURE_2D, 8);
    glBegin(GL_QUADS);

	glTexCoord2f(0, 0); glVertex2i(0,		0);
    glTexCoord2f(1, 0); glVertex2i(VIRTW,	0);
    glTexCoord2f(1, 1); glVertex2i(VIRTW,	VIRTH);
    glTexCoord2f(0, 1); glVertex2i(0,		VIRTH);

	glEnd();

	SDL_GL_SwapBuffers();

	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	
	glPopMatrix();
}
