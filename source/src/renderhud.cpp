// renderhud.cpp: HUD rendering

#include "cube.h"

void drawicon(Texture *tex, float x, float y, float s, int col, int row, float ts)
{
    if(tex && tex->xs == tex->ys) quad(tex->id, x, y, s, ts*col, ts*row, ts);
}

inline void turn_on_transparency()
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(255, 255, 255, 255);
}

void drawequipicon(float x, float y, int col, int row, float blend)
{
    static Texture *tex = NULL;
    if(!tex) tex = textureload("packages/misc/items.png", 4);
    if(tex)
    {
        //if(blend||transparency) glEnable(GL_BLEND); if(transparency)
        turn_on_transparency();
        drawicon(tex, x, y, 120, col, row, 1/4.0f);
        //if(blend||transparency)
        glDisable(GL_BLEND);
    }
}

void drawradaricon(float x, float y, float s, int col, int row)
{
    static Texture *tex = NULL;
    if(!tex) tex = textureload("packages/misc/radaricons.png", 3);
    if(tex)
    {
        glEnable(GL_BLEND);
        drawicon(tex, x, y, s, col, row, 1/4.0f);
        glDisable(GL_BLEND);
    }
}

void drawctficon(float x, float y, float s, int col, int row, float ts)
{
    static Texture *ctftex = NULL, *htftex = NULL, *ktftex = NULL;
    if(!ctftex) ctftex = textureload("packages/misc/ctficons.png", 3);
    if(!htftex) htftex = textureload("packages/misc/htficons.png", 3);
    if(!ktftex) ktftex = textureload("packages/misc/ktficons.png", 3);
    if(m_htf)
    {
        if(htftex) drawicon(htftex, x, y, s, col, row, ts);
    }
    else if(m_ktf)
    {
        if(ktftex) drawicon(ktftex, x, y, s, col, row, ts);
    }
    else
    {
        if(ctftex) drawicon(ctftex, x, y, s, col, row, ts);
    }
}

void drawvoteicon(float x, float y, int col, int row, bool noblend)
{
    static Texture *tex = NULL;
    if(!tex) tex = textureload("packages/misc/voteicons.png", 3);
    if(tex)
    {
        if(noblend) glDisable(GL_BLEND);
        else turn_on_transparency(); // if(transparency && !noblend)
        drawicon(tex, x, y, 240, col, row, 1/2.0f);
        if(noblend) glEnable(GL_BLEND);
    }
}

VARP(crosshairsize, 0, 15, 50);
VARP(showstats, 0, 1, 2);
VARP(crosshairfx, 0, 1, 1);
VARP(crosshairteamsign, 0, 1, 1);
VARP(hideradar, 0, 0, 1);
VARP(hidecompass, 0, 0, 1);
VARP(hideteam, 0, 0, 1);
VARP(radarentsize, 4, 16, 64);
VARP(hidectfhud, 0, 0, 1);
VARP(hidevote, 0, 0, 2);
VARP(hidehudmsgs, 0, 0, 1);
VARP(hidehudequipment, 0, 0, 1);
VARP(hideconsole, 0, 0, 1);
VARP(hidespecthud, 0, 0, 1);
VAR(showmap, 0, 0, 1);

void drawscope()
{
    static Texture *scopetex = NULL;
    if(!scopetex) scopetex = textureload("packages/misc/scope.png", 3);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, scopetex->id);
    glColor3ub(255, 255, 255);

    // figure out the bounds of the scope given the desired aspect ratio
    float sz = min(VIRTW, VIRTH),
          x1 = VIRTW/2 - sz/2,
          x2 = VIRTW/2 + sz/2,
          y1 = VIRTH/2 - sz/2,
          y2 = VIRTH/2 + sz/2,
          border = (512 - 64*2)/512.0f;

    // draw center viewport
    glBegin(GL_TRIANGLE_FAN);
    glTexCoord2f(0.5f, 0.5f);
    glVertex2f(x1 + 0.5f*sz, y1 + 0.5f*sz);
    loopi(8+1)
    {
        float c = 0.5f*(1 + border*cosf(i*2*M_PI/8.0f)), s = 0.5f*(1 + border*sinf(i*2*M_PI/8.0f));
        glTexCoord2f(c, s);
        glVertex2f(x1 + c*sz, y1 + s*sz);
    }
    glEnd();

    glDisable(GL_BLEND);

    // draw outer scope
    glBegin(GL_TRIANGLE_STRIP);
    loopi(8+1)
    {
        float c = 0.5f*(1 + border*cosf(i*2*M_PI/8.0f)), s = 0.5f*(1 + border*sinf(i*2*M_PI/8.0f));
        glTexCoord2f(c, s);
        glVertex2f(x1 + c*sz, y1 + s*sz);
        c = c < 0.4f ? 0 : (c > 0.6f ? 1 : 0.5f);
        s = s < 0.4f ? 0 : (s > 0.6f ? 1 : 0.5f);
        glTexCoord2f(c, s);
        glVertex2f(x1 + c*sz, y1 + s*sz);
    }
    glEnd();

    // fill unused space with border texels
    if(x1 > 0 || x2 < VIRTW || y1 > 0 || y2 < VIRTH)
    {
        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0, 0); glVertex2f(0,  0);
        glTexCoord2f(0, 0); glVertex2f(x1, y1);
        glTexCoord2f(0, 1); glVertex2f(0,  VIRTH);
        glTexCoord2f(0, 1); glVertex2f(x1, y2);

        glTexCoord2f(1, 1); glVertex2f(VIRTW, VIRTH);
        glTexCoord2f(1, 1); glVertex2f(x2, y2);
        glTexCoord2f(1, 0); glVertex2f(VIRTW, 0);
        glTexCoord2f(1, 0); glVertex2f(x2, y1);

        glTexCoord2f(0, 0); glVertex2f(0,  0);
        glTexCoord2f(0, 0); glVertex2f(x1, y1);
        glEnd();
    }

    glEnable(GL_BLEND);
}

const char *crosshairnames[CROSSHAIR_NUM] = { "default", "teammate", "scope" };
Texture *crosshairs[CROSSHAIR_NUM] = { NULL }; // weapon specific crosshairs

Texture *loadcrosshairtexture(const char *c)
{
    defformatstring(p)("packages/misc/crosshairs/%s", c);
    Texture *crosshair = textureload(p, 3);
    if(crosshair==notexture) crosshair = textureload("packages/misc/crosshairs/default.png", 3);
    return crosshair;
}

void loadcrosshair(char *c, char *name)
{
    int n = -1;
    loopi(CROSSHAIR_NUM) if(!strcmp(crosshairnames[i], name)) { n = i; break; }
    if(n<0)
    {
        n = atoi(name);
        if(n<0 || n>=CROSSHAIR_NUM) return;
    }
    crosshairs[n] = loadcrosshairtexture(c);
}

COMMAND(loadcrosshair, ARG_2STR);

void drawcrosshair(playerent *p, int n, color *c, float size)
{
    Texture *crosshair = crosshairs[n];
    if(!crosshair)
    {
        crosshair = crosshairs[CROSSHAIR_DEFAULT];
        if(!crosshair) crosshair = crosshairs[CROSSHAIR_DEFAULT] = loadcrosshairtexture("default.png");
    }

    if(crosshair->bpp==32) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    else glBlendFunc(GL_ONE, GL_ONE);
	glBindTexture(GL_TEXTURE_2D, crosshair->id);
    glColor3ub(255,255,255);
    if(c) glColor3f(c->r, c->g, c->b);
    else if(crosshairfx || n==CROSSHAIR_TEAMMATE)
    {
        if(n==CROSSHAIR_TEAMMATE) glColor3ub(255, 0, 0);
        else if(!m_osok)
        {
            if(p->health<=25) glColor3ub(255,0,0);
            else if(p->health<=50) glColor3ub(255,128,0);
        }
    }
    float s = size>0 ? size : (float)crosshairsize;
	float chsize = s * (p->weaponsel->type==GUN_ASSAULT && p->weaponsel->shots > 3 ? 1.4f : 1.0f) * (n==CROSSHAIR_TEAMMATE ? 2.0f : 1.0f);
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0, 0); glVertex2f(VIRTW/2 - chsize, VIRTH/2 - chsize);
    glTexCoord2f(1, 0); glVertex2f(VIRTW/2 + chsize, VIRTH/2 - chsize);
    glTexCoord2f(0, 1); glVertex2f(VIRTW/2 - chsize, VIRTH/2 + chsize);
    glTexCoord2f(1, 1); glVertex2f(VIRTW/2 + chsize, VIRTH/2 + chsize);
    glEnd();
}

VARP(hidedamageindicator, 0, 0, 1);
VARP(damageindicatorsize, 0, 200, 10000);
VARP(damageindicatordist, 0, 500, 10000);
VARP(damageindicatortime, 1, 1000, 10000);
VARP(damageindicatoralpha, 1, 50, 100);
int damagedirections[4] = {0};

void updatedmgindicator(vec &attack)
{
    if(hidedamageindicator || !damageindicatorsize) return;
    float bestdist = 0.0f;
    int bestdir = -1;
    loopi(4)
    {
        vec d;
        d.x = (float)(cosf(RAD*(player1->yaw-90+(i*90))));
        d.y = (float)(sinf(RAD*(player1->yaw-90+(i*90))));
        d.z = 0.0f;
        d.add(player1->o);
        float dist = d.dist(attack);
        if(dist < bestdist || bestdir==-1)
        {
            bestdist = dist;
            bestdir = i;
        }
    }
    damagedirections[bestdir] = lastmillis+damageindicatortime;
}

void drawdmgindicator()
{
    if(!damageindicatorsize) return;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    float size = (float)damageindicatorsize;
    loopi(4)
    {
        if(!damagedirections[i] || damagedirections[i] < lastmillis) continue;
        float t = damageindicatorsize/(float)(damagedirections[i]-lastmillis);
        glPushMatrix();
        glColor4f(0.5f, 0.0f, 0.0f, damageindicatoralpha/100.0f);
        glTranslatef(VIRTW/2, VIRTH/2, 0);
        glRotatef(i*90, 0, 0, 1);
        glTranslatef(0, (float)-damageindicatordist, 0);
        glScalef(max(0.0f, 1.0f-t), max(0.0f, 1.0f-t), 0);

        glBegin(GL_TRIANGLES);
        glVertex3f(size/2.0f, size/2.0f, 0.0f);
        glVertex3f(-size/2.0f, size/2.0f, 0.0f);
        glVertex3f(0.0f, 0.0f, 0.0f);
        glEnd();
        glPopMatrix();
    }
    glEnable(GL_TEXTURE_2D);
}

void drawequipicons(playerent *p)
{
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 0.2f+(sinf(lastmillis/100.0f)+1.0f)/2.0f);

    // health & armor
    if(p->armour) drawequipicon(620, 1650, (p->armour-1)/25, 2, false);
    drawequipicon(20, 1650, 2, 3, (p->state!=CS_DEAD && p->health<=20 && !m_osok));

    // weapons
	int c = p->weaponsel->type, r = 0;
    if(c==GUN_AKIMBO || c==GUN_CPISTOL) c = GUN_PISTOL; // same icon for akimb & pistol
    if(c==GUN_GRENADE) c--; // FIXME
    if(c>3) { c -= 4; r = 1; }

	if(p->weaponsel && p->weaponsel->type>=GUN_KNIFE && p->weaponsel->type<NUMGUNS) drawequipicon(1220, 1650, c, r, (!p->weaponsel->mag && p->weaponsel->type != GUN_KNIFE && p->weaponsel->type != GUN_GRENADE));
    glEnable(GL_BLEND);
}

void drawradarent(float x, float y, float yaw, int col, int row, float iconsize, bool pulse, const char *label = NULL, ...)
{
    glPushMatrix();
    if(pulse) glColor4f(1.0f, 1.0f, 1.0f, 0.2f+(sinf(lastmillis/30.0f)+1.0f)/2.0f);
    else glColor4f(1, 1, 1, 1);
    glTranslatef(x, y, 0);
    glRotatef(yaw, 0, 0, 1);
    drawradaricon(-iconsize/2.0f, -iconsize/2.0f, iconsize, col, row);
    glPopMatrix();
    if(label && showmap)
    {
        glPushMatrix();
        glEnable(GL_BLEND);
        glTranslatef(iconsize/2, iconsize/2, 0);
        glScalef(1/2.0f, 1/2.0f, 1/2.0f);
        defvformatstring(lbl, label, label);
        draw_text(lbl, (int)(x*2), (int)(y*2));
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
        glPopMatrix();
    }
}

struct hudline : cline
{
    int type;

    hudline() : type(HUDMSG_INFO) {}
};

struct hudmessages : consolebuffer<hudline>
{
    hudmessages() : consolebuffer<hudline>(20) {}

    void addline(const char *sf)
    {
        if(conlines.length() && conlines[0].type&HUDMSG_OVERWRITE)
        {
            conlines[0].millis = totalmillis;
            conlines[0].type = HUDMSG_INFO;
            copystring(conlines[0].line, sf);
        }
        else consolebuffer<hudline>::addline(sf, totalmillis);
    }
    void editline(int type, const char *sf)
    {
        if(conlines.length() && ((conlines[0].type&HUDMSG_TYPE)==(type&HUDMSG_TYPE) || conlines[0].type&HUDMSG_OVERWRITE))
        {
            conlines[0].millis = totalmillis;
            conlines[0].type = type;
            copystring(conlines[0].line, sf);
        }
        else consolebuffer<hudline>::addline(sf, totalmillis).type = type;
    }
    void render()
    {
        if(!conlines.length()) return;
        glLoadIdentity();
		glOrtho(0, VIRTW*0.9f, VIRTH*0.9f, 0, -1, 1);
        int dispmillis = arenaintermission ? 6000 : 3000;
        loopi(min(conlines.length(), 3)) if(totalmillis-conlines[i].millis<dispmillis)
        {
            cline &c = conlines[i];
            int tw = text_width(c.line);
            draw_text(c.line, int(tw > VIRTW*0.9f ? 0 : (VIRTW*0.9f-tw)/2), int(((VIRTH*0.9f)/4*3)+FONTH*i+pow((totalmillis-c.millis)/(float)dispmillis, 4)*VIRTH*0.9f/4.0f));
        }
    }
};

hudmessages hudmsgs;

void hudoutf(const char *s, ...)
{
    defvformatstring(sf, s, s);
    hudmsgs.addline(sf);
    conoutf("%s", sf);
}

void hudonlyf(const char *s, ...)
{
    defvformatstring(sf, s, s);
    hudmsgs.addline(sf);
}

void hudeditf(int type, const char *s, ...)
{
    defvformatstring(sf, s, s);
    hudmsgs.editline(type, sf);
}

bool insideradar(const vec &centerpos, float radius, const vec &o)
{
    if(showmap) return !o.reject(centerpos, radius);
    return o.distxy(centerpos)<=radius;
}

bool isattacking(playerent *p) { return lastmillis-p->lastaction < 500; }

vec getradarpos()
{
    float radarviewsize = VIRTH/6;
    float overlaysize = radarviewsize*4.0f/3.25f;
    return vec(VIRTW-10-VIRTH/28-overlaysize, 10+VIRTH/52, 0);
}

VARP(showmapbackdrop, 0, 2, 2);
VARP(showmapbackdroptransparency, 0, 75, 100);
VARP(radarheight, 11, 35, 100);
VAR(showradarvalues, 0, 0, 1); // DEBUG
VAR(radarfactor, 0, 0, 2000); // DEBUG
void drawradar(playerent *p, int w, int h)
{
    int gdim = max(mapdims[4], mapdims[5]);
    float radarviewsize = showmap?3*min(VIRTW,VIRTH)/4:min(VIRTW,VIRTH)/5;
    float halfviewsize = radarviewsize/2.0f;
    float iconsize = (showmap?radarentsize/4:radarentsize)/(float)gdim*radarviewsize;
    float coordtrans =  showmap?radarviewsize/gdim:1;
    int offd = (abs(mapdims[5]-mapdims[4])/2)*coordtrans;
    if(!gdim) { gdim = ssize/2; offd = 0; }
    int offx = gdim==mapdims[5]?offd:0;
    int offy = gdim==mapdims[4]?offd:0;
    glColor3f(1.0f, 1.0f, 1.0f);
    glPushMatrix();
    extern GLuint minimaptex;
    if(showmap)
    {
        vec centerpos(VIRTW/2 , VIRTH/2, 0.0f);
        if(showmapbackdrop)
        {
            glDisable(GL_TEXTURE_2D);
            if(showmapbackdrop==2) glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE_MINUS_SRC_COLOR);
            loopi(2)
            {
                int cg = i?(showmapbackdrop==2?((int)(255*(100-showmapbackdroptransparency)/100.0f)):0):(showmapbackdrop==2?((int)(255*(100-showmapbackdroptransparency)/100.0f)):64);
                int co = i?0:4;
                glColor3ub(cg, cg, cg);
                glBegin(GL_QUADS);
                glVertex2f( centerpos.x - halfviewsize - co, centerpos.y + halfviewsize + co);
                glVertex2f( centerpos.x + halfviewsize + co, centerpos.y + halfviewsize + co);
                glVertex2f( centerpos.x + halfviewsize + co, centerpos.y - halfviewsize - co);
                glVertex2f( centerpos.x - halfviewsize - co, centerpos.y - halfviewsize - co);
                glEnd();
            }
            glColor3ub(255,255,255);
            glEnable(GL_TEXTURE_2D);
        }
        glTranslatef(centerpos.x - halfviewsize, centerpos.y - halfviewsize , 0);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
        quad(minimaptex, 0, 0, radarviewsize, 0.0f, 0.0f, 1.0f);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
        vec mdd(mapdims[0], mapdims[1], 0);
        vec cod(offx, offy, 0);
        vec ppv = vec(p->o).sub(mdd).mul(coordtrans).add(cod);
        drawradarent(ppv.x, ppv.y, p->yaw, p->state==CS_ALIVE ? (isattacking(p) ? 2 : 0) : 1, 2, iconsize, isattacking(p), "%s", colorname(p)); // local player
        loopv(players) // other players
        {
            playerent *pl = players[i];
            if(!pl || pl==p) continue;
            if(!isteam(p->team, pl->team)) continue;
            vec rtmp = vec(pl->o).sub(mdd).mul(coordtrans).add(cod);
            drawradarent(rtmp.x, rtmp.y, pl->yaw, pl->state==CS_ALIVE ? (isattacking(pl) ? 2 : 0) : 1, team_base(pl->team), iconsize, isattacking(pl), "%s", colorname(pl));
        }
        if(m_flags)
        {
            glColor4f(1.0f, 1.0f, 1.0f, (sinf(lastmillis / 100.0f) + 1.0f) / 2.0f);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            loopi(2) // flag items
            {
                flaginfo &f = flaginfos[i];
                entity *e = f.flagent;
                if(!e) continue;
                if(e->x == -1 && e-> y == -1) continue; // flagdummies
                vec pos = vec(e->x, e->y, 0).sub(mdd).mul(coordtrans).add(cod);
                vec cpos(pos.x + 16.0f, pos.y - 16.0f, 0);
                drawradarent(pos.x, pos.y, camera1->yaw, m_ktf ? 2 : f.team, 3, iconsize, false); // draw bases
                if(f.state!=CTFF_STOLEN && !(m_ktf && f.state == CTFF_IDLE))
                {
                    drawradarent(cpos.x, cpos.y, 0, 3, m_ktf ? 2 : f.team, iconsize, false); // draw on entitiy pos
                }
                if(m_ktf && f.state == CTFF_IDLE) continue;
                if(f.state==CTFF_STOLEN)
                {
                    vec apos(1.6f, -1.6f, 0);
                    if(f.actor)
                    {
                        apos.add(f.actor->o);
                        bool tm = i != team_base(p->team);
                        if(m_htf) tm = !tm;
                        else if(m_ktf) tm = true;
                        if(tm)
                        {
                            apos.sub(mdd).mul(coordtrans).add(cod);
                            drawradarent(apos.x, apos.y, 0, 3, m_ktf ? 2 : f.team, iconsize, true); // draw near flag thief
                        }
                    }
                }
            }
        }

        glEnable(GL_BLEND);
        glPopMatrix();
    }
    else
    {
        extern int minimaplastsize;
        vec centerpos(VIRTW-halfviewsize-72, halfviewsize+64, 0); // x-72, y+64 is a good position (w/ teamhud on)
        glTranslatef(centerpos.x, centerpos.y, 0);
        glRotatef(-camera1->yaw, 0, 0, 1);
        glTranslatef(-halfviewsize, -halfviewsize, 0);
        vec rtr = vec(offx - mapdims[0], offy - mapdims[1], 0);
        float d2s = (5.0f * (gdim/(minimaplastsize/4.0f)) * (radarheight/radarviewsize)*minimaplastsize)/6.0f;
        float f2m = radarfactor?radarfactor/100.0f:((((radarviewsize*minimaplastsize)/gdim)/radarheight)/10.0f);
        vec usecenter = vec(p->o).add(rtr);
        if(showradarvalues)
        {
            conoutf("gdim: %d : rh: %d", gdim, radarheight);
            conoutf("rvs: %.2f : ics: %.2f : mmls: %d", radarviewsize, iconsize, minimaplastsize);
            conoutf("offd: %d [%d:%d]", offd, offx, offy);
            conoutf("RTR: %.2f %.2f", rtr.x, rtr.y);
            conoutf("D2S: %.2f : F2M: %.2f", d2s, f2m);
            showradarvalues = 0;
        }
        glDisable(GL_BLEND);
        circle(minimaptex, halfviewsize, halfviewsize, halfviewsize, usecenter.x/gdim, usecenter.y/gdim, radarheight/100.0f);
        glTranslatef(halfviewsize, halfviewsize, 0);
        drawradarent(0, 0, p->yaw, p->state==CS_ALIVE ? (isattacking(p) ? 2 : 0) : 1, 2, iconsize, isattacking(p), "%s", colorname(p)); // local player
        loopv(players) // other players
        {
            playerent *pl = players[i];
            if(!pl || pl==p) continue;
            if(!isteam(p->team, pl->team)) continue;
            vec rtmp = vec(pl->o).add(rtr).sub(usecenter);
            bool isok = rtmp.magnitude() < d2s;
            if(isok)
            {
                rtmp.mul(f2m);
                drawradarent(rtmp.x, rtmp.y, pl->yaw, pl->state==CS_ALIVE ? (isattacking(pl) ? 2 : 0) : 1, team_base(pl->team), iconsize, isattacking(pl), "%s", colorname(pl));
            }
        }
        if(m_flags)
        {
            glColor4f(1.0f, 1.0f, 1.0f, (sinf(lastmillis / 100.0f) + 1.0f) / 2.0f);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            loopi(2) // flag items
            {
                flaginfo &f = flaginfos[i];
                entity *e = f.flagent;
                if(!e) continue;
                if(e->x == -1 && e-> y == -1) continue; // flagdummies
                vec pos = vec(e->x, e->y, 0).add(rtr).sub(usecenter);
                vec cpos(radarheight/10.0f, -radarheight/10.0f, 0);
                cpos.rotate_around_z(camera1->yaw*RAD);
                cpos.add(pos);
                bool isok = pos.magnitude() < d2s;
                if(isok)
                {
                    pos.mul(f2m);
                    drawradarent(pos.x, pos.y, 0/*camera1->yaw*/, m_ktf ? 2 : f.team, 3, iconsize, false); // draw bases [circle doesn't need rotating]
                    if(f.state!=CTFF_STOLEN && !(m_ktf && f.state == CTFF_IDLE))
                    {
                        /*
                        // flag icons are drawn a little too soon (depending on camera1->yaw and radarheight) .. ac_arid is the worst (going from RVSF to CLA)
                        // but all these calculations barely improve on it - and mostly cause the flag to be drawn "too late" which is probably MORE irritating
                        bool flag2 = true;
                        if(radarheight<50)
                        {
                            float vf = 0.025f/(radarheight*radarheight/1000.0f);//float vf = 0.5f/(100.0f/(1.0f*radarheight));
                            float nc2s = d2s - d2s*vf;
                            if(cpos.magnitude() >= nc2s) flag2 = false;
                        }
                        if(flag2)
                        {
                        */
                            cpos.mul(f2m); // these can be drawn slightly outside the radar-eyecandy .. only at low radarheight.
                            drawradarent(cpos.x, cpos.y, camera1->yaw, 3, m_ktf ? 2 : f.team, iconsize, false); // draw on entitiy pos
                        //}
                    }
                }
                if(m_ktf && f.state == CTFF_IDLE) continue;
                if(f.state==CTFF_STOLEN)
                {
                    vec apos(0.4f, -0.4f, 0);
                    if(f.actor)
                    {
                        apos.add(f.actor->o);
                        bool tm = i != team_base(p->team);
                        if(m_htf) tm = !tm;
                        else if(m_ktf) tm = true;
                        if(tm)
                        {
                            apos.add(rtr).sub(usecenter);
                            bool isok = apos.magnitude() < d2s;
                            if(isok)
                            {
                                apos.mul(f2m);
                                drawradarent(apos.x, apos.y, camera1->yaw, 3, m_ktf ? 2 : f.team, iconsize, true); // draw near flag thief
                            }
                        }
                    }
                }
            }
        }
        glEnable(GL_BLEND);
        glPopMatrix();
        // eye candy:
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor3f(1, 1, 1);
        static Texture *bordertex = NULL;
        if(!bordertex) bordertex = textureload("packages/misc/compass-base.png", 3);
        quad(bordertex->id, centerpos.x-halfviewsize-16, centerpos.y-halfviewsize-16, radarviewsize+32, 0, 0, 1, 1);
        if(!hidecompass)
        {
            static Texture *compasstex = NULL;
            if(!compasstex) compasstex = textureload("packages/misc/compass-rose.png", 3);
            glPushMatrix();
            glTranslatef(centerpos.x, centerpos.y, 0);
            glRotatef(-camera1->yaw, 0, 0, 1);
            quad(compasstex->id, -halfviewsize-8, -halfviewsize-8, radarviewsize+16, 0, 0, 1, 1);
            glPopMatrix();
        }
    }
}

void drawteamicons(int w, int h)
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3f(1, 1, 1);
    static Texture *icons = NULL;
    if(!icons) icons = textureload("packages/misc/teamicons.png", 3);
    quad(icons->id, VIRTW-VIRTH/12-10, 10, VIRTH/12, team_base(player1->team) ? 0.5f : 0, 0, 0.49f, 1.0f);
}

int damageblendmillis = 0;

VARFP(damagescreen, 0, 1, 1, { if(!damagescreen) damageblendmillis = 0; });
VARP(damagescreenfactor, 1, 7, 100);
VARP(damagescreenalpha, 1, 45, 100);
VARP(damagescreenfade, 0, 125, 1000);

void damageblend(int n)
{
    if(!damagescreen) return;
    if(lastmillis > damageblendmillis) damageblendmillis = lastmillis;
    damageblendmillis += n*damagescreenfactor;
}

void drawmedals(float x, float y, int col, int row, Texture *tex)
{
    if(tex)
    {
        glPushAttrib(GL_COLOR_BUFFER_BIT);
        glDisable(GL_BLEND);
        drawicon(tex, x, y, 120, col, row, 1/4.0f);
        glPopAttrib();
    }
}
const char *medal_str[] =
{
    "Best Fragger", "Dude that dies a lot"
}; //just some medals string tests, nothing serious
extern bool medals_arrived;
extern medalsst a_medals[END_MDS];
void drawscores()
{
    static float time=0;
    if(!medals_arrived) {time=0; return;} else if(time > 5){time=0; medals_arrived=0;}
    static Texture *tex = NULL;
    if(!tex) tex = textureload("packages/misc/nice_medals.png", 4);
    time+=((float)(curtime))/1000;
    float vw=VIRTW*7/4,vh=VIRTH*7/4;
    glPushAttrib(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
	glOrtho(0, vw, vh, 0, -1, 1);
    int left = vw/4, top = vh/4;
    blendbox(left, top, left*3, top*3, true, -1);
    top+=10;left+=10;const float txtdx=160,txtdy=30,medalsdy=130;
    glColor4f(1,1,1,1);
    float desttime=0;
    loopi(END_MDS) {
        if(a_medals[i].assigned) {
            desttime+=0.3;
            if(time < desttime) continue;
            drawmedals(left, top, 0, 0, tex);
            playerent *mpl = a_medals[i].cn == getclientnum() ? player1 : getclient(a_medals[i].cn);
            draw_textf("%s %s: %d", left+txtdx, top+txtdy, medal_str[i], mpl->name, a_medals[i].item); top+=medalsdy;
        }
    }

    glPopAttrib();
}

VARP(dbgpos,0,0,1);
void gl_drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater)
{
    playerent *p = camera1->type<ENT_CAMERA ? (playerent *)camera1 : player1;
    bool spectating = player1->isspectating();

    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
    glEnable(GL_BLEND);

    if(underwater)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4ub(hdr.watercolor[0], hdr.watercolor[1], hdr.watercolor[2], 102);

        glBegin(GL_TRIANGLE_STRIP);
        glVertex2f(0, 0);
        glVertex2f(VIRTW, 0);
        glVertex2f(0, VIRTH);
        glVertex2f(VIRTW, VIRTH);
        glEnd();
    }

    if(lastmillis < damageblendmillis)
    {
        static Texture *damagetex = NULL;
        if(!damagetex) damagetex = textureload("packages/misc/damage.png", 3);

        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, damagetex->id);
        float fade = damagescreenalpha/100.0f;
        if(damageblendmillis - lastmillis < damagescreenfade)
            fade *= float(damageblendmillis - lastmillis)/damagescreenfade;
        glColor4f(fade, fade, fade, fade);

        glBegin(GL_TRIANGLE_STRIP);
        glTexCoord2f(0, 0); glVertex2f(0, 0);
        glTexCoord2f(1, 0); glVertex2f(VIRTW, 0);
        glTexCoord2f(0, 1); glVertex2f(0, VIRTH);
        glTexCoord2f(1, 1); glVertex2f(VIRTW, VIRTH);
        glEnd();
    }

    glEnable(GL_TEXTURE_2D);

    playerent *targetplayer = playerincrosshair();
    bool menu = menuvisible();
    bool command = getcurcommand() ? true : false;
    if((p->state==CS_ALIVE || p->state==CS_EDITING) && !p->weaponsel->reloading)
	{
		bool drawteamwarning = crosshairteamsign && targetplayer && isteam(targetplayer->team, p->team) && targetplayer->state==CS_ALIVE;
		p->weaponsel->renderaimhelp(drawteamwarning);
	}

    drawdmgindicator();

    if(p->state==CS_ALIVE && !hidehudequipment) drawequipicons(p);

    if(!editmode)
    {
        glMatrixMode(GL_MODELVIEW);
        if(/*!menu &&*/ (!hideradar || showmap)) drawradar(p, w, h);
        if(!hideteam && m_teammode) drawteamicons(w, h);
        glMatrixMode(GL_PROJECTION);
    }

    char *infostr = editinfo();
    int commandh = 1570 + FONTH;
    if(command) commandh -= rendercommand(20, 1570, VIRTW);
    else if(infostr) draw_text(infostr, 20, 1570);
    else if(targetplayer) draw_text(colorname(targetplayer), 20, 1570);
//     if(targetplayer && targetplayer->weaponsel->type == GUN_SNIPER) // flowtron: for debugging ATM
//     {
//         sniperrifle *sr = (sniperrifle *)targetplayer->weaponsel;
//         if(sr->scoped) draw_textf("\f2SCOPED", 20, 1570 - 2*FONTH);
//     }

    glLoadIdentity();
    glOrtho(0, VIRTW*2, VIRTH*2, 0, -1, 1);
    extern bool senst;
    if(senst) {
        draw_textf("\fJSensitivity Training (hotkeys):\n\fE1. try High Sens.\n2. try Low Sens.\n\fJafter trying both choose the one you liked most:\fE\n3. choose: High Sens.\n4. choose: Low Sens.\n\fIrepeat the steps above until the training stops.\n\f35. Stop Training.", VIRTW/4, VIRTH/3);
    }

    if(!hideconsole) renderconsole();
    if(showstats)
    {
        if(showstats==2)
        {
            const int left = (VIRTW-225-10)*2, top = (VIRTH*7/8)*2;
            const int ttll = VIRTW*2 - 3*FONTH/2;
            blendbox(left - 24, top - 24, VIRTW*2 - 72, VIRTH*2 - 48, true, -1);
            int c_num;
            int c_r = 255;      int c_g = 255;      int c_b = 255;
            string c_val;
    #define TXTCOLRGB \
            switch(c_num) \
            { \
                case 0: c_r = 120; c_g = 240; c_b = 120; break; \
                case 1: c_r = 120; c_g = 120; c_b = 240; break; \
                case 2: c_r = 230; c_g = 230; c_b = 110; break; \
                case 3: c_r = 250; c_g = 100; c_b = 100; break; \
                default: \
                    c_r = 255; c_g = 255; c_b =  64; \
                break; \
            }

            draw_text("fps", left - (text_width("fps") + FONTH/2), top    );
            draw_text("lod", left - (text_width("lod") + FONTH/2), top+ 80);
            draw_text("wqd", left - (text_width("wqd") + FONTH/2), top+160);
            draw_text("wvt", left - (text_width("wvt") + FONTH/2), top+240);
            draw_text("evt", left - (text_width("evt") + FONTH/2), top+320);

            //ttll -= 3*FONTH/2;

            formatstring(c_val)("%d", curfps);
            c_num = curfps > 150 ? 0 : (curfps > 100 ? 1 : (curfps > 30 ? 2 : 3)); TXTCOLRGB
            draw_text(c_val, ttll - text_width(c_val), top    , c_r, c_g, c_b);

            int lf = lod_factor();
            formatstring(c_val)("%d", lf);
            c_num = lf>199?(lf>299?(lf>399?3:2):1):0; TXTCOLRGB
            draw_text(c_val, ttll - text_width(c_val), top+ 80, c_r, c_g, c_b);

            formatstring(c_val)("%d", nquads);
            c_num = nquads>3999?(nquads>5999?(nquads>7999?3:2):1):0; TXTCOLRGB
            draw_text(c_val, ttll - text_width(c_val), top+160, c_r, c_g, c_b);

            formatstring(c_val)("%d", curvert);
            c_num = curvert>3999?(curvert>5999?(curvert>7999?3:2):1):0; TXTCOLRGB
            draw_text(c_val, ttll - text_width(c_val), top+240, c_r, c_g, c_b);

            formatstring(c_val)("%d", xtraverts);
            c_num = xtraverts>3999?(xtraverts>5999?(xtraverts>7999?3:2):1):0; TXTCOLRGB
            draw_text(c_val, ttll - text_width(c_val), top+320, c_r, c_g, c_b);
        }
        else
        {
            if(dbgpos)
            {
                defformatstring(o_x)("pos x %03.1f", player1->o.x);
                draw_text(o_x, VIRTW*2 - ( text_width(o_x) + FONTH ), VIRTH*2 - 9*FONTH/2);
                defformatstring(o_y)("pos y %03.1f", player1->o.y);
                draw_text(o_y, VIRTW*2 - ( text_width(o_y) + FONTH ), VIRTH*2 - 7*FONTH/2);
                defformatstring(o_z)("pos z %03.1f", player1->o.z);
                draw_text(o_z, VIRTW*2 - ( text_width(o_z) + FONTH ), VIRTH*2 - 5*FONTH/2);
            }
            defformatstring(c_val)("fps %d", curfps);
            draw_text(c_val, VIRTW*2 - ( text_width(c_val) + FONTH ), VIRTH*2 - 3*FONTH/2);
        }
    }

    if(hidevote < 2 && multiplayer(false))
    {
        extern votedisplayinfo *curvote;

        if(curvote && curvote->millis >= totalmillis && !(hidevote == 1 && curvote->localplayervoted && curvote->result == VOTE_NEUTRAL))
        {
            const int left = 20*2, top = VIRTH;
            draw_textf("%s called a vote:", left, top+240, curvote->owner ? colorname(curvote->owner) : "");
            draw_textf("%s", left, top+320, curvote->desc);
            draw_textf("----", left, top+400);
            draw_textf("%d yes vs. %d no", left, top+480, curvote->stats[VOTE_YES], curvote->stats[VOTE_NO]);

            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glColor4f(1.0f, 1.0f, 1.0f, (sinf(lastmillis/100.0f)+1.0f) / 2.0f);
            switch(curvote->result)
            {
                case VOTE_NEUTRAL:
                    drawvoteicon(left, top, 0, 0, true);
                    if(!curvote->localplayervoted)
                        draw_textf("\f3press F1/F2 to vote yes or no", left, top+560);
                    break;
                default:
                    drawvoteicon(left, top, (curvote->result-1)&1, 1, false);
                    draw_textf("\f3vote %s", left, top+560, curvote->result == VOTE_YES ? "PASSED" : "FAILED");
                    break;
            }
        }
    }
    //else draw_textf("%c%d here F1/F2 will be praised during a vote", 20*2, VIRTH+560, '\f', 0); // see position (left/top) setting in block above

    if(menu) rendermenu();
    else if(command) renderdoc(40, VIRTH, max(commandh*2 - VIRTH, 0));

    if(!hidehudmsgs) hudmsgs.render();


    if(!hidespecthud && !menu && p->state==CS_DEAD && p->spectatemode<=SM_DEATHCAM)
    {
        glLoadIdentity();
		glOrtho(0, VIRTW*3/2, VIRTH*3/2, 0, -1, 1);
        const int left = (VIRTW*3/2)*6/8, top = (VIRTH*3/2)*3/4;
        draw_textf("SPACE to change view", left, top);
        draw_textf("SCROLL to change player", left, top+80);
    }

    /* */
    glLoadIdentity();
	glOrtho(0, VIRTW*3/2, VIRTH*3/2, 0, -1, 1);
    const int tbMSGleft = (VIRTW*3/2)*5/6;
    const int tbMSGtop = (VIRTH*3/2)*7/8;
    draw_textf("!TEST BUILD!", tbMSGleft, tbMSGtop);
    /* */
    drawscores();
    if(!hidespecthud && spectating && player1->spectatemode!=SM_DEATHCAM)
    {
        glLoadIdentity();
		glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
		const char *specttext = "GHOST [free]";//SPECTATING";
		if(player1->team == TEAM_SPECT) specttext = "GHOST";
		else if(player1->team == TEAM_CLA_SPECT) specttext = "[CLA]";
		else if(player1->team == TEAM_RVSF_SPECT) specttext = "[RVSF]";
		draw_text(specttext, VIRTW/40, VIRTH/10*7);
        if(player1->spectatemode==SM_FOLLOW1ST || player1->spectatemode==SM_FOLLOW3RD || player1->spectatemode==SM_FOLLOW3RD_TRANSPARENT)
        {
            if(players.inrange(player1->followplayercn) && players[player1->followplayercn])
            {
                defformatstring(name)("Player %s", players[player1->followplayercn]->name);
                draw_text(name, VIRTW/40, VIRTH/10*8);
            }
        }
    }

    if(p->state==CS_ALIVE)
    {
        glLoadIdentity();
        glOrtho(0, VIRTW/2, VIRTH/2, 0, -1, 1);

        if(!hidehudequipment)
        {
            pushfont("huddigits");
            draw_textf("%d",  90, 823, p->health);
            if(p->armour) draw_textf("%d", 390, 823, p->armour);
            if(p->weaponsel && p->weaponsel->type>=GUN_KNIFE && p->weaponsel->type<NUMGUNS)
            {
                glMatrixMode(GL_MODELVIEW);
                p->weaponsel->renderstats();
                glMatrixMode(GL_PROJECTION);
            }
            popfont();
        }

        if(m_flags && !hidectfhud)
        {
            glLoadIdentity();
            glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
            glColor4f(1.0f, 1.0f, 1.0f, 0.2f);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);

            loopi(2) // flag state
            {
                if(flaginfos[i].state == CTFF_INBASE) glDisable(GL_BLEND); else glEnable(GL_BLEND);
                drawctficon(i*120+VIRTW/4.0f*3.0f, 1650, 120, i, 0, 1/4.0f);
            }

            // big flag-stolen icon
            int ft = 0;
            if((flaginfos[0].state==CTFF_STOLEN && flaginfos[0].actor == p && flaginfos[0].ack) ||
               (flaginfos[1].state==CTFF_STOLEN && flaginfos[1].actor == p && flaginfos[1].ack && ++ft))
            {
                glColor4f(1.0f, 1.0f, 1.0f, (sinf(lastmillis/100.0f)+1.0f) / 2.0f);
                glEnable(GL_BLEND);
                drawctficon(VIRTW-225-10, VIRTH*5/8, 225, ft, 1, 1/2.0f);
                glDisable(GL_BLEND);
            }
        }
    }

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
}

void loadingscreen(const char *fmt, ...)
{
    static Texture *logo = NULL;
    if(!logo) logo = textureload("packages/misc/startscreen.png", 3);

    glEnable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, VIRTW, VIRTH, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glClearColor(0, 0, 0, 1);
    glColor3f(1, 1, 1);

    loopi(fmt ? 1 : 2)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        quad(logo->id, (VIRTW-VIRTH)/2, 0, VIRTH, 0, 0, 1);
        if(fmt)
        {
            glEnable(GL_BLEND);
            defvformatstring(str, fmt, fmt);
            int w = text_width(str);
            draw_text(str, w>=VIRTW ? 0 : (VIRTW-w)/2, VIRTH*3/4);
            glDisable(GL_BLEND);
        }
        SDL_GL_SwapBuffers();
    }

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);
}

static void bar(float bar, int o, float r, float g, float b)
{
    int side = 2*FONTH;
    float x1 = side, x2 = bar*(VIRTW*1.2f-2*side)+side;
    float y1 = o*FONTH;
    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_TRIANGLE_STRIP);
    loopk(10)
    {
       float c = 1.2f*cosf(M_PI/2 + k/9.0f*M_PI), s = 1 + 1.2f*sinf(M_PI/2 + k/9.0f*M_PI);
       glVertex2f(x2 - c*FONTH, y1 + s*FONTH);
       glVertex2f(x1 + c*FONTH, y1 + s*FONTH);
    }
    glEnd();

    glColor3f(r, g, b);
    glBegin(GL_TRIANGLE_STRIP);
    loopk(10)
    {
       float c = cosf(M_PI/2 + k/9.0f*M_PI), s = 1 + sinf(M_PI/2 + k/9.0f*M_PI);
       glVertex2f(x2 - c*FONTH, y1 + s*FONTH);
       glVertex2f(x1 + c*FONTH, y1 + s*FONTH);
    }
    glEnd();
}

void show_out_of_renderloop_progress(float bar1, const char *text1, float bar2, const char *text2)   // also used during loading
{
    c2skeepalive();

    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, VIRTW*1.2f, VIRTH*1.2f, 0, -1, 1);

    glLineWidth(3);

    if(text1)
    {
        bar(1, 1, 0.1f, 0.1f, 0.1f);
        if(bar1>0) bar(bar1, 1, 0.2f, 0.2f, 0.2f);
    }

    if(bar2>0)
    {
        bar(1, 3, 0.1f, 0.1f, 0.1f);
        bar(bar2, 3, 0.2f, 0.2f, 0.2f);
    }

    glLineWidth(1);

    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    if(text1) draw_text(text1, 2*FONTH, 1*FONTH + FONTH/2);
    if(bar2>0) draw_text(text2, 2*FONTH, 3*FONTH + FONTH/2);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_DEPTH_TEST);
    SDL_GL_SwapBuffers();
}

