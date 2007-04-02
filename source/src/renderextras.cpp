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

void circle(GLuint tex, float x, float y, float r, float tx, float ty, float tr, int subdiv = 16)
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
				glTexCoord2f(0, 0);			        glVertex2f((float)x1+texw*i, (float)y1+texh*j);
				glTexCoord2f(1-xtexcut, 0);			glVertex2f(x1+texw*(i+1)-xboxcut, (float)y1+texh*j);
				glTexCoord2f(1-xtexcut, 1-ytexcut);	glVertex2f(x1+texw*(i+1)-xboxcut, (float)y1+texh*(j+1)-yboxcut); 
				glTexCoord2f(0, 1-ytexcut);			glVertex2f((float)x1+texw*i, y1+texh*(j+1)-yboxcut);
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
		glTexCoord2f(0, 0);	glVertex2i(x1, y1);
		glTexCoord2f(1, 0);	glVertex2i(x2, y1);
		glTexCoord2f(1, 1); glVertex2i(x2, y2); 
		glTexCoord2f(0, 1);	glVertex2i(x1, y2);
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
        vec v(e.x, e.y, e.z);
        particle_splash(2, 2, 40, v);
    }
    int e = closestent();
    if(e>=0)
    {
        entity &c = ents[e];
        s_sprintf(closeent)("closest entity = %s (%d, %d, %d, %d), selection = (%d, %d)", entnames[c.type], c.attr1, c.attr2, c.attr3, c.attr4, getvar("selxs"), getvar("selys"));
    }
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

void drawicon(Texture *tex, float x, float y, float s, int col, int row, float ts)
{
    if(tex && tex->xs == tex->ys) quad(tex->id, x, y, s, ts*col, ts*row, ts);
}

void drawequipicon(float x, float y, int col, int row, float blend)
{
    static Texture *tex = NULL;
    if(!tex) tex = textureload("packages/misc/items.png");
    if(tex)
    {
        if(blend) glEnable(GL_BLEND);
        drawicon(tex, x, y, 120, col, row, 1/3.0f);
        if(blend) glDisable(GL_BLEND);
    }
}

void drawradaricon(float x, float y, float s, int col, int row, bool blend)
{
    static Texture *tex = NULL;
    if(!tex) tex = textureload("packages/misc/radaricons.png");
    if(tex)
    {
        if(blend) glEnable(GL_BLEND);
        drawicon(tex, x, y, s, col, row, 1/4.0f);
        if(blend) glDisable(GL_BLEND);
    }
}

void drawctficon(float x, float y, float s, int col, int row, float ts)
{
    static Texture *tex = NULL;
    if(!tex) tex = textureload("packages/misc/teamicons.png");
    if(tex) drawicon(tex, x, y, s, col, row, ts);
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

VARP(crosshairsize, 0, 15, 50);

int dblend = 0;
void damageblend(int n) { dblend += n; }

VARP(hidestats, 0, 1, 1);
VARP(crosshairfx, 0, 1, 1);
VARP(hideradar, 0, 0, 1);
VARP(radarres, 1, 64, 1024);
VARP(radarentsize, 1, 4, 64);
VARP(hidectfhud, 0, 0, 1);
VARP(hideteamhud, 0, 0, 1);

bool showmap = false;
void toggleshowmap() { showmap = !showmap; }
COMMAND(toggleshowmap, ARG_NONE);

void drawscope()
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.9f);
    static Texture *scopetex = NULL;
    if(!scopetex) scopetex = textureload("packages/misc/scope.png");
    glBindTexture(GL_TEXTURE_2D, scopetex->id);
    glBegin(GL_QUADS);
    glColor3ub(255,255,255);

    glTexCoord2i(0, 0); glVertex2i(0, 0);
    glTexCoord2i(1, 0); glVertex2i(VIRTW, 0);
    glTexCoord2i(1, 1); glVertex2i(VIRTW, VIRTH);
    glTexCoord2i(0, 1); glVertex2i(0, VIRTH);

    glEnd();
    glDisable(GL_ALPHA_TEST);
}

void drawcrosshair(bool showteamwarning)
{
    glBlendFunc(GL_ONE, GL_ONE);
    static Texture *teammatetex = NULL;
    if(!teammatetex) teammatetex = textureload("packages/misc/teammate.png");
	glBindTexture(GL_TEXTURE_2D, showteamwarning ? teammatetex->id : crosshair->id);
    glBegin(GL_QUADS);
    glColor3ub(255,255,255);
    if(crosshairfx)
    {
        if(showteamwarning) glColor3ub(255, 0, 0);
        else if(player1->gunwait) glColor3ub(128,128,128);
        else if(!m_osok)
        {
            if(player1->health<=25) glColor3ub(255,0,0);
            else if(player1->health<=50) glColor3ub(255,128,0);
        }
    }
	float chsize = (float)crosshairsize * (player1->gunselect==GUN_ASSAULT && player1->shots > 3 ? 1.4f : 1.0f) * (showteamwarning ? 2.0f : 1.0f);
    glTexCoord2i(0, 0); glVertex2f(VIRTW/2 - chsize, VIRTH/2 - chsize);
    glTexCoord2i(1, 0); glVertex2f(VIRTW/2 + chsize, VIRTH/2 - chsize);
    glTexCoord2i(1, 1); glVertex2f(VIRTW/2 + chsize, VIRTH/2 + chsize);
    glTexCoord2i(0, 1); glVertex2f(VIRTW/2 - chsize, VIRTH/2 + chsize);
    glEnd();
}

void drawequipicons()
{   
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 0.2f+(sinf(lastmillis/100.0f)+1.0f)/2.0f);

    // health & armor
    if(player1->armour) drawequipicon(620, 1650, 2, 0, false);
    drawequipicon(20, 1650, 1, 0, (player1->health <= 20 && !m_osok));
    
    // weapons
    int c = player1->gunselect, r = 1;
    if(c>2) { c -= 3; r = 2; }
    if(c==GUN_GRENADE) c = r = 0;

    drawequipicon(1220, 1650, c, r, (!player1->mag[player1->gunselect] && player1->gunselect != GUN_KNIFE && player1->gunselect != GUN_GRENADE));
    glEnable(GL_BLEND);
}

void drawradarent(float x, float y, float yaw, int col, int row, float iconsize, bool blend)
{
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(yaw, 0, 0, 1);
    drawradaricon(-iconsize/2.0f, -iconsize/2.0f, iconsize, col, row, blend);
    glPopMatrix();
}

bool insideradar(const vec &centerpos, float radius, const vec &o)
{
    // return o.reject(centerpos, radius);
    return o.distxy(centerpos)<=radius;
}

void drawradar(const vec &center, float angle, int radarres, int w, int h, bool fullscreen)
{
    glPushMatrix();
    glDisable(GL_BLEND);

    const float worldsize = (float)ssize;
    const float radarviewsize = fullscreen ? VIRTH : VIRTH/6;
    const float radarsize = worldsize/radarres*radarviewsize;

    if(fullscreen) glTranslatef(VIRTW/2-radarviewsize/2, 0, 0);
    else glTranslatef(VIRTW-radarviewsize-10, 10, 0);
    glTranslatef(radarviewsize/2, radarviewsize/2, 0);
    glRotatef(angle, 0, 0, 1);
    glTranslatef(-radarviewsize/2, -radarviewsize/2, 0);

    extern GLuint minimaptex;

    vec centerpos(min(max(center.x, radarres/2), worldsize-radarres/2), min(max(center.y, radarres/2), worldsize-radarres/2), 0);
    circle(minimaptex, radarviewsize/2, radarviewsize/2, radarviewsize/2, centerpos.x/worldsize, centerpos.y/worldsize, radarres/2/worldsize);
    //quad(minimaptex, 0, 0, radarviewsize, (centerpos.x-radarres/2)/worldsize, (centerpos.y-radarres/2)/worldsize, radarres/worldsize);
    glTranslatef(-(centerpos.x-radarres/2)/worldsize*radarsize, -(centerpos.y-radarres/2)/worldsize*radarsize, 0);

    const float iconsize = radarentsize/(float)radarres*radarviewsize;
    const float coordtrans = radarsize/worldsize;

    drawradarent(player1->o.x*coordtrans, player1->o.y*coordtrans, player1->yaw, player1->state==CS_ALIVE ? (player1->attacking ? 2 : 0) : 1, 2, iconsize, false); // local player
    loopv(players) // other players
    {
        playerent *pl = players[i];
        if(!pl || !isteam(player1->team, pl->team) || !insideradar(centerpos, radarres/2, pl->o)) continue;
        drawradarent(pl->o.x*coordtrans, pl->o.y*coordtrans, pl->yaw, pl->state==CS_ALIVE ? (pl->attacking ? 2 : 0) : 1, team_int(pl->team), iconsize, false);
    }
    if(m_ctf)
    {
        glColor4f(1.0f, 1.0f, 1.0f, (sinf(lastmillis / 100.0f) + 1.0f) / 2.0f);
        loopi(2) // flag items
        {
            flaginfo &f = flaginfos[i];
            entity *e = f.flag;
            if(!e) continue;
            if(f.state==CTFF_STOLEN && f.actor && insideradar(centerpos, radarres/2, f.actor->o))
                drawradarent(f.actor->o.x*coordtrans+iconsize/2, f.actor->o.y*coordtrans+iconsize/2, 0, 3, f.team, iconsize, true); // draw near flag thief
            else if(insideradar(centerpos, radarres/2, vec(e->x, e->y, centerpos.z))) drawradarent(e->x*coordtrans, e->y*coordtrans, 0, 3, f.team, iconsize, false); // draw on entitiy pos
        }
    }

    glEnable(GL_BLEND);
    glPopMatrix();
}

void gl_drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater)
{
    readmatrices();
    if(editmode)
    {
        if(cursordepth==1.0f) worldpos = camera1->o;
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        cursorupdate();
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

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
        if(dblend) glColor3f(0.0f, 0.9f, 0.9f);
        else glColor3f(0.9f, 0.5f, 0.0f);
        glVertex2i(0, 0);
        glVertex2i(VIRTW, 0);
        glVertex2i(VIRTW, VIRTH);
        glVertex2i(0, VIRTH);
        glEnd();
        glDepthMask(GL_TRUE);
        dblend -= min(1, curtime/3);
        if(dblend<0) dblend = 0;
    }

    glEnable(GL_TEXTURE_2D);

    playerent *targetplayer = playerincrosshair();
    bool didteamkill = player1->lastteamkill && player1->lastteamkill + 5000 > lastmillis;
    bool menuvisible = rendermenu();

    if(player1->state==CS_ALIVE && !player1->reloading && !didteamkill && !menuvisible)
    {
        bool drawteamwarning = targetplayer ? (isteam(targetplayer->team, player1->team) && targetplayer->state!=CS_DEAD) : false;
        if(player1->gunselect==GUN_SNIPER && scoped) drawscope();
        else if((player1->gunselect!=GUN_SNIPER || drawteamwarning)) drawcrosshair(drawteamwarning);
    }

    drawequipicons();

    if(!hideradar)
    {
        if(showmap) drawradar(vec(ssize/2, ssize/2, 0), 0, ssize, w, h, true);
        else drawradar(player1->o, -player1->yaw, radarres, w, h, false);
    }

    if(getcurcommand()) rendercommand(20, 1570);
    else if(closeent[0]) draw_text(closeent, 20, 1570);
    else if(targetplayer) draw_text(targetplayer->name, 20, 1570);

    glPopMatrix();

    glPushMatrix();
    glOrtho(0, VIRTW*2, VIRTH*2, 0, -1, 1);
    renderconsole();
    if(!hidestats)
    {
        const int left = (VIRTW-225-10)*2, top = (VIRTH*7/8)*2;
        draw_textf("fps %d", left, top, curfps);
        draw_textf("lod %d", left, top+80, lod_factor());
        draw_textf("wqd %d", left, top+160, nquads); 
        draw_textf("wvt %d", left, top+240, curvert);
        draw_textf("evt %d", left, top+320, xtraverts);
    }
    glPopMatrix();

    if(player1->state==CS_ALIVE)
    {
        glPushMatrix();
        glOrtho(0, VIRTW/2, VIRTH/2, 0, -1, 1);
        draw_textf("%d",  90, 827, player1->health);
        if(player1->armour) draw_textf("%d", 390, 827, player1->armour);
        if(player1->gunselect!=GUN_KNIFE)
        {
            char gunstats[64];
            if(player1->gunselect!=GUN_GRENADE) sprintf(gunstats,"%i/%i",player1->mag[player1->gunselect],player1->ammo[player1->gunselect]);
            else sprintf(gunstats,"%i",player1->mag[player1->gunselect]);
            draw_text(gunstats, 690, 827);
        }
		glPopMatrix();

        if(m_teammode && !hideteamhud)
        {
            glPushMatrix();
            glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
            glDisable(GL_BLEND);
            drawctficon(VIRTW-120-10, VIRTH/6+10+10, 120, team_int(player1->team), 1, 1/4.0f); // local players team
            glEnable(GL_BLEND);
            draw_textf(player1->team, VIRTW-VIRTH/6-10, VIRTH/6+10+10+120/2);
            glPopMatrix();
        }

		if(didteamkill)
		{
			glPushMatrix();
			glOrtho(0, VIRTW/3, VIRTH/3, 0, -1, 1);
			draw_text("\f3you killed a teammate", VIRTW/3/40, VIRTH/3/2);
			glPopMatrix();
		}

        if(m_ctf && !hidectfhud)
        {
            glPushMatrix();
            glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
            glColor4f(1.0f, 1.0f, 1.0f, 0.2f);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);

            loopi(2) // flag state
            {
                if(flaginfos[i].state == CTFF_INBASE) glDisable(GL_BLEND); else glEnable(GL_BLEND);
                drawctficon(i*120+VIRTW/4.0f*3.0f, 1650, 120, i, 0, 1/4.0f);
            }
            
            // big flag-stolen icon
            flaginfo &f = flaginfos[team_opposite(team_int(player1->team))];
            if(f.state==CTFF_STOLEN && f.actor == player1 && f.ack)
            {
                glColor4f(1.0f, 1.0f, 1.0f, (sinf(lastmillis/100.0f)+1.0f) / 2.0f);
                glEnable(GL_BLEND);
                drawctficon(VIRTW-225-10, VIRTH*5/8, 225, team_opposite(team_int(player1->team)), 1, 1/2.0f);
                glDisable(GL_BLEND);
            }
            glPopMatrix();
        }
    }

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
}

void renderbounceents()
{
    loopv(bounceents)
    {
        bounceent *p = bounceents[i];
		if(!p) continue;
		string model;
		float z = p->o.z;
		
		switch(p->bouncestate)
		{
			case NADE_THROWED:
				s_strcpy(model, "weapons/grenade/static");
				break;
			case GIB:
			default:
            {    
                uint n = (((4*(uint)(size_t)p)+(uint)p->timetolife)%3)+1;
				s_sprintf(model)("misc/gib0%u", n);
				int t = lastmillis-p->millis;
				if(t>p->timetolife-2000)
				{
					t -= p->timetolife-2000;
					z -= t*t/4000000000.0f*t;
				}
				break;
            }
		}
		path(model);
		rendermodel(model, ANIM_MAPMODEL|ANIM_LOOP, 0, 1.1f, p->o.x, z, p->o.y, p->yaw, p->pitch, 10.0f);
    }
}

VARP(gibnum, 0, 6, 1000);
VARP(gibttl, 0, 5000, 15000);
VARP(gibspeed, 1, 30, 100);

void addgib(playerent *d)
{
	if(!d) return;
	playsound(S_GIB, &d->o);
	
	loopi(gibnum)
	{
		bounceent *p = newbounceent();
		p->owner = d;
		p->millis = lastmillis;
		p->timetolife = gibttl+rnd(10)*100;
		p->bouncestate = GIB;

		p->o = d->o;
		p->o.z -= d->aboveeye;

        p->yaw = (float)rnd(360);
        p->pitch = (float)rnd(360);

        p->maxspeed = 30.0f;
        p->rotspeed = 3.0f;
        
        const float angle = (float)rnd(360);
        const float speed = (float)gibspeed;

        p->vel.x = sinf(RAD*angle)*rnd(1000)/1000.0f;
        p->vel.y = cosf(RAD*angle)*rnd(1000)/1000.0f;
        p->vel.z = rnd(1000)/1000.0f;
        p->vel.mul(speed/100.0f);
	}
}

void loadingscreen()
{
    static Texture *logo = NULL;
    if(!logo) logo = textureload("packages/misc/startscreen.png");

    glEnable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, VIRTW, VIRTH, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(0, 0, 0, 1);
    glColor3f(1, 1, 1);

    loopi(2)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        quad(logo->id, (VIRTW-VIRTH)/2, 0, VIRTH, 0, 0, 1);
        SDL_GL_SwapBuffers();
    }

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
}

