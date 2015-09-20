// renderhud.cpp: HUD rendering

#include "cube.h"

void drawicon(Texture *tex, float x, float y, float s, int col, int row, float ts)
{
    if(tex && tex->xs == tex->ys) quad(tex->id, x, y, s, ts*col, ts*row, ts);
}

inline void turn_on_transparency(int alpha = 255)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4ub(255, 255, 255, alpha);
}

void drawequipicon(float x, float y, int col, int row)
{
    static Texture *tex = NULL;
    if(!tex) tex = textureload("packages/misc/items.png", 3);
    if(tex)
    {
        turn_on_transparency();
        drawicon(tex, x, y, 120, col, row, 1/4.0f);
        glDisable(GL_BLEND);
    }
}

VARP(radarentsize, 4, 12, 64);

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

void drawctficon(float x, float y, float s, int col, int row, float ts, int alpha)
{
    static Texture *ctftex = NULL, *htftex = NULL, *ktftex = NULL;
    if(!ctftex) ctftex = textureload("packages/misc/ctficons.png", 3);
    if(!htftex) htftex = textureload("packages/misc/htficons.png", 3);
    if(!ktftex) ktftex = textureload("packages/misc/ktficons.png", 3);
    glColor4ub(255, 255, 255, alpha);
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

VARP(votealpha, 0, 255, 255);
void drawvoteicon(float x, float y, int col, int row, bool noblend)
{
    static Texture *tex = NULL;
    if(!tex) tex = textureload("packages/misc/voteicons.png", 3);
    if(tex)
    {
        if(noblend) glDisable(GL_BLEND);
        else turn_on_transparency(votealpha); // if(transparency && !noblend)
        drawicon(tex, x, y, 240, col, row, 1/2.0f);
        if(noblend) glEnable(GL_BLEND);
    }
}

VARP(crosshairsize, 0, 15, 50);
VARP(showstats, 0, 1, 2);
VARP(crosshairfx, 0, 1, 3);
VARP(crosshairteamsign, 0, 1, 1);
VARP(hideradar, 0, 0, 1);
VARP(hidecompass, 0, 0, 1);
VARP(hideteam, 0, 0, 1);
VARP(hidectfhud, 0, 0, 1);
VARP(hidevote, 0, 0, 2);
VARP(hidehudmsgs, 0, 0, 1);
VARP(hidehudequipment, 0, 0, 1);
VARP(hideconsole, 0, 0, 1);
VARP(hidespecthud, 0, 0, 1);
VAR(showmap, 0, 0, 1);

void drawscope(bool preload)
{
    static Texture *scopetex = NULL;
    if(!scopetex) scopetex = textureload("packages/misc/scope.png", 3);
    if(preload) return;
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

const char *crosshairnames[CROSSHAIR_NUM + 1];  // filled in main.cpp
Texture *crosshairs[CROSSHAIR_NUM] = { NULL }; // weapon specific crosshairs

Texture *loadcrosshairtexture(const char *c)
{
    defformatstring(p)("packages/crosshairs/%s", behindpath(c));
    Texture *crosshair = textureload(p, 3);
    if(crosshair==notexture) crosshair = textureload("packages/crosshairs/default.png", 3);
    return crosshair;
}

void loadcrosshair(char *type, char *filename)
{
    int index = CROSSHAIR_DEFAULT;
    if(!*filename)
    {   // special form: loadcrosshair filename   // short for "loadcrosshair default filename"
        filename = type;
    }
    else if(strchr(type, '.'))
    {   // old syntax "loadcrosshair filename type", remove this in 2020
        const char *oldcrosshairnames[CROSSHAIR_NUM + 1] = { "default", "teammate", "scope", "knife", "pistol", "carbine", "shotgun", "smg", "sniper", "ar", "cpistol", "grenades", "akimbo", "" };
        index = getlistindex(filename, oldcrosshairnames, false, 0);
        if(index > 2) index -= 3;
        else index += NUMGUNS;
        filename = type;
    }
    else
    {   // new syntax with proper gun names
        index = getlistindex(type, crosshairnames, false, CROSSHAIR_DEFAULT);
    }
    if(index == CROSSHAIR_DEFAULT)
    {
        loopi(CROSSHAIR_NUM) if(i != CROSSHAIR_TEAMMATE && i != CROSSHAIR_SCOPE) crosshairs[i] = loadcrosshairtexture(filename);
    }
    else
    {
        crosshairs[index] = loadcrosshairtexture(filename);
    }
}

COMMAND(loadcrosshair, "ss");

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
    else if(crosshairfx==1 || crosshairfx==2 || n==CROSSHAIR_TEAMMATE)
    {
        if(n==CROSSHAIR_TEAMMATE) glColor3ub(255, 0, 0);
        else if(!m_osok)
        {
            if(p->health<=25) glColor3ub(255,0,0);
            else if(p->health<=50) glColor3ub(255,128,0);
        }
    }
    float s = size>0 ? size : (float)crosshairsize;
    float chsize = s * ((p->weaponsel->type==GUN_ASSAULT && p->weaponsel->shots > 3) && (crosshairfx==1 || crosshairfx==3) ? 1.4f : 1.0f) * (n==CROSSHAIR_TEAMMATE ? 2.0f : 1.0f);
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

extern int oldfashionedgunstats;

void drawequipicons(playerent *p)
{
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 0.2f+(sinf(lastmillis/100.0f)+1.0f)/2.0f);

    // health & armor
    if(p->armour) drawequipicon(HUDPOS_ARMOUR*2, 1650, (p->armour-1)/25, 2);
    drawequipicon(HUDPOS_HEALTH*2, 1650, 2, 3);
    if(p->mag[GUN_GRENADE]) drawequipicon(oldfashionedgunstats ? (HUDPOS_GRENADE + 25)*2 : HUDPOS_GRENADE*2, 1650, 3, 1);

    // weapons
    int c = p->weaponsel->type != GUN_GRENADE ? p->weaponsel->type : p->prevweaponsel->type, r = 0;
    if(c==GUN_AKIMBO || c==GUN_CPISTOL) c = GUN_PISTOL; // same icon for akimb & pistol
    if(c>3) { c -= 4; r = 1; }

    if(p->weaponsel && p->weaponsel->type>=GUN_KNIFE && p->weaponsel->type<NUMGUNS)
        drawequipicon(HUDPOS_WEAPON*2, 1650, c, r);
    glEnable(GL_BLEND);
}

void drawradarent(float x, float y, float yaw, int col, int row, float iconsize, bool pulse, const char *label = NULL, ...) PRINTFARGS(8, 9);

void drawradarent(float x, float y, float yaw, int col, int row, float iconsize, bool pulse, const char *label, ...)
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
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, VIRTW*0.9f, VIRTH*0.9f, 0, -1, 1);
        int dispmillis = arenaintermission ? 6000 : 3000;
        loopi(min(conlines.length(), 3)) if(totalmillis-conlines[i].millis<dispmillis)
        {
            cline &c = conlines[i];
            int tw = text_width(c.line);
            draw_text(c.line, int(tw > VIRTW*0.9f ? 0 : (VIRTW*0.9f-tw)/2), int(((VIRTH*0.9f)/4*3)+FONTH*i+pow((totalmillis-c.millis)/(float)dispmillis, 4)*VIRTH*0.9f/4.0f));
        }
        glPopMatrix();
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

VARP(showmapbackdrop, 0, 0, 2);
VARP(showmapbackdroptransparency, 0, 75, 100);
VARP(radarheight, 5, 150, 500);

void drawradar_showmap(playerent *p, int w, int h)
{
    float minimapviewsize = 3*min(VIRTW,VIRTH)/4; //minimap default size
    float halfviewsize = minimapviewsize/2.0f;
    float iconsize = radarentsize/0.2f;
    glColor3f(1.0f, 1.0f, 1.0f);
    glPushMatrix();
    bool spect3rd = p->spectatemode > SM_FOLLOW1ST && p->spectatemode <= SM_FOLLOW3RD_TRANSPARENT;
    playerent *d = spect3rd ? players[p->followplayercn] : p;
    int p_baseteam = p->team == TEAM_SPECT && spect3rd ? team_base(players[p->followplayercn]->team) : team_base(p->team);
    extern GLuint minimaptex;
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
    quad(minimaptex, 0, 0, minimapviewsize, 0.0f, 0.0f, 1.0f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);

    float gdim = max(mapdims.xspan, mapdims.yspan); //no border
    float offd = fabs((mapdims.yspan - mapdims.xspan) / 2.0f);
    if(!gdim) { gdim = ssize/2.0f; offd = 0; }
    float coordtrans = minimapviewsize / gdim;
    float offx = gdim == mapdims.yspan ? offd : 0;
    float offy = gdim == mapdims.xspan ? offd : 0;

    vec mdd = vec(mapdims.x1 - offx, mapdims.y1 - offy, 0);
    vec ppv = vec(p->o).sub(mdd).mul(coordtrans);

    if(!(p->isspectating() && spect3rd)) drawradarent(ppv.x, ppv.y, p->yaw, (p->state==CS_ALIVE || p->state==CS_EDITING) ? (isattacking(p) ? 2 : 0) : 1, 2, iconsize, isattacking(p), "%s", colorname(p)); // local player
    loopv(players) // other players
    {
        playerent *pl = players[i];
        if(!pl || pl == p || !team_isactive(pl->team)) continue;
        if(OUTBORD(pl->o.x, pl->o.y)) continue;
        int pl_baseteam = team_base(pl->team);
        if(p->team < TEAM_SPECT && ((m_teammode && !isteam(p_baseteam, pl_baseteam)) || (!m_teammode && !(spect3rd && d == pl)))) continue;
        if(p->team == TEAM_SPECT && !(spect3rd && (isteam(p_baseteam, pl_baseteam) || d == pl))) continue;
        vec rtmp = vec(pl->o).sub(mdd).mul(coordtrans);
        drawradarent(rtmp.x, rtmp.y, pl->yaw, pl->state==CS_ALIVE ? (isattacking(pl) ? 2 : 0) : 1, spect3rd && d == pl ? 2 : pl_baseteam, iconsize, isattacking(pl), "%s", colorname(pl));
    }
    if(m_flags)
    {
        glColor4f(1.0f, 1.0f, 1.0f, (sinf(lastmillis / 100.0f) + 1.0f) / 2.0f);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        loopi(2) // flag items
        {
            flaginfo &f = flaginfos[i];
            entity *e = f.flagent;
            bool hasflagent = e && e->x != -1 && e->y != -1; // no base for flagentitydummies (HTF on maps without bases)
            if(hasflagent)
            {
                vec pos = vec(e->x, e->y, 0).sub(mdd).mul(coordtrans);
                drawradarent(pos.x, pos.y, 0, m_ktf ? 2 : f.team, 3, iconsize, false); // draw bases
            }
            if((f.state == CTFF_INBASE && hasflagent) || f.state == CTFF_DROPPED)
            {
                vec fltxoff = vec(8, -8, 0);
                vec cpos = vec(f.pos.x, f.pos.y, f.pos.z).sub(mdd).mul(coordtrans).add(fltxoff);
                float flgoff=fabs((radarentsize*2.1f)-8);
                drawradarent(cpos.x+flgoff, cpos.y-flgoff, 0, 3, m_ktf ? 2 : f.team, iconsize, false); // draw on entity pos or whereever dropped
            }
            if(f.state == CTFF_STOLEN)
            {
                if(m_teammode && !m_ktf && player1->team == TEAM_SPECT && p->spectatemode > SM_FOLLOW3RD_TRANSPARENT) continue;
                float d2c = 1.6f * radarentsize/16.0f;
                vec apos(d2c, -d2c, 0);
                if(f.actor)
                {
                    apos.add(f.actor->o);
                    bool tm = i != p_baseteam;
                    if(m_htf) tm = !tm;
                    else if(m_ktf) tm = true;
                    if(tm)
                    {
                        apos.sub(mdd).mul(coordtrans);
                        drawradarent(apos.x, apos.y, 0, 3, m_ktf ? 2 : f.team, iconsize, true); // draw near flag thief
                    }
                }
            }
        }
    }
    glEnable(GL_BLEND);
    glPopMatrix();
}

void drawradar_vicinity(playerent *p, int w, int h)
{
    bool spect3rd = p->spectatemode > SM_FOLLOW1ST && p->spectatemode <= SM_FOLLOW3RD_TRANSPARENT;
    playerent *d = spect3rd ? players[p->followplayercn] : p;
    int p_baseteam = p->team == TEAM_SPECT && spect3rd ? team_base(players[p->followplayercn]->team) : team_base(p->team);
    extern GLuint minimaptex;
    int gdim = max(mapdims.xspan, mapdims.yspan);
    float radarviewsize = min(VIRTW,VIRTH)/5;
    float halfviewsize = radarviewsize/2.0f;
    float iconsize = radarentsize/0.4f;
    float scaleh = radarheight/(2.0f*gdim);
    float scaled = radarviewsize/float(radarheight);
    float offd = fabs((mapdims.yspan - mapdims.xspan) / 2.0f);
    if(gdim < 1) { gdim = ssize/2; offd = 0; }
    float offx = gdim == mapdims.yspan ? offd : 0;
    float offy = gdim==mapdims.xspan ? offd : 0;
    vec rtr = vec(mapdims.x1 - offx, mapdims.y1 - offy, 0);
    vec rsd = vec(mapdims.xm, mapdims.ym, 0);
    float d2s = radarheight/2.0f;
    glColor3f(1.0f, 1.0f, 1.0f);
    glPushMatrix();
    vec centerpos(VIRTW-halfviewsize-72, halfviewsize+64, 0);
    glTranslatef(centerpos.x, centerpos.y, 0);
    glRotatef(-camera1->yaw, 0, 0, 1);
    glTranslatef(-halfviewsize, -halfviewsize, 0);
    vec d4rc = vec(d->o).sub(rsd).normalize().mul(0);
    vec usecenter = vec(d->o).sub(rtr).sub(d4rc);
    glDisable(GL_BLEND);
    circle(minimaptex, halfviewsize, halfviewsize, halfviewsize, usecenter.x/(float)gdim, usecenter.y/(float)gdim, scaleh, 31); //Draw mimimaptext as radar background
    glTranslatef(halfviewsize, halfviewsize, 0);

    if(!(p->isspectating() && spect3rd)) drawradarent(0, 0, p->yaw, (p->state==CS_ALIVE || p->state==CS_EDITING) ? (isattacking(p) ? 2 : 0) : 1, 2, iconsize, isattacking(p), "%s", colorname(p)); // local player
    loopv(players) // other players
    {
        playerent *pl = players[i];
        if(!pl || pl == p || !team_isactive(pl->team)) continue;
        if(OUTBORD(pl->o.x, pl->o.y)) continue;
        int pl_baseteam = team_base(pl->team);
        if(p->team < TEAM_SPECT && ((m_teammode && !isteam(p_baseteam, pl_baseteam)) || (!m_teammode && !(spect3rd && d == pl)))) continue;
        if(p->team == TEAM_SPECT && !(spect3rd && (isteam(p_baseteam, pl_baseteam) || d == pl))) continue;
        vec rtmp = vec(pl->o).sub(d->o);
        bool isok = rtmp.magnitude() < d2s;
        if(isok)
        {
            rtmp.mul(scaled);
            drawradarent(rtmp.x, rtmp.y, pl->yaw, pl->state==CS_ALIVE ? (isattacking(pl) ? 2 : 0) : 1, spect3rd && d == pl ? 2 : pl_baseteam, iconsize, isattacking(pl), "%s", colorname(pl));
        }
    }
    if(m_flags)
    {
        glColor4f(1.0f, 1.0f, 1.0f, (sinf(lastmillis / 100.0f) + 1.0f) / 2.0f);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        float d2c = 1.6f * radarentsize/16.0f;
        loopi(2) // flag items
        {
            flaginfo &f = flaginfos[i];
            entity *e = f.flagent;
            bool hasflagent = e && e->x != -1 && e->y != -1; // no base for flagentitydummies (HTF on maps without bases)
            if(hasflagent)
            {
                vec pos = vec(e->x, e->y, 0).sub(d->o);
                if(pos.magnitude() < d2s)
                {
                    pos.mul(scaled);
                    drawradarent(pos.x, pos.y, 0, m_ktf ? 2 : f.team, 3, iconsize, false); // draw bases [circle doesn't need rotating]
                }
            }
            if((f.state == CTFF_INBASE && hasflagent) || f.state == CTFF_DROPPED)
            {
                vec cpos = vec(f.pos.x, f.pos.y, f.pos.z).sub(d->o);
                if(cpos.magnitude() < d2s)
                {
                    cpos.mul(scaled);
                    float flgoff=radarentsize/0.68f;
                    float ryaw=(camera1->yaw-45)*(2*PI/360);
                    float offx=flgoff*cosf(-ryaw);
                    float offy=flgoff*sinf(-ryaw);
                    drawradarent(cpos.x+offx, cpos.y-offy, camera1->yaw, 3, m_ktf ? 2 : f.team, iconsize, false); // draw flag on entity pos or whereever dropped
                }
            }
            if(f.state == CTFF_STOLEN)
            {
                if(m_teammode && !m_ktf && player1->team == TEAM_SPECT && p->spectatemode > SM_FOLLOW3RD_TRANSPARENT) continue;
                vec apos(d2c, -d2c, 0);
                if(f.actor)
                {
                    apos.add(f.actor->o);
                    bool tm = i != p_baseteam;
                    if(m_htf) tm = !tm;
                    else if(m_ktf) tm = true;
                    if(tm)
                    {
                        apos.sub(d->o);
                        if(apos.magnitude() < d2s)
                        {
                            apos.mul(scaled);
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

void drawradar(playerent *p, int w, int h)
{
    if(showmap) drawradar_showmap(p,w,h);
    else drawradar_vicinity(p,w,h);
}

void drawteamicons(int w, int h, bool spect)
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3f(1, 1, 1);
    static Texture *icons = NULL;
    if(!icons) icons = textureload("packages/misc/teamicons.png", 3);
    if(player1->team < TEAM_SPECT || spect) quad(icons->id, VIRTW-VIRTH/12-10, 10, VIRTH/12, team_base(spect ? players[player1->followplayercn]->team : player1->team) ? 0.5f : 0, 0, 0.49f, 1.0f);
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
            playerent *mpl = getclient(a_medals[i].cn);
            draw_textf("%s %s: %d", left+txtdx, top+txtdy, medal_str[i], mpl->name, a_medals[i].item); top+=medalsdy;
        }
    }

    glPopAttrib();
}

string enginestateinfo = "";
void CSgetEngineState() { result(enginestateinfo); }
COMMANDN(getEngineState, CSgetEngineState, "");

VARP(gametimedisplay,0,1,2);
VARP(dbgpos,0,0,1);
VARP(showtargetname,0,1,1);
VARP(showspeed, 0, 0, 1);
VAR(blankouthud, 0, 0, 10000); //for "clean" screenshot
string gtime;

void gl_drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater)
{
    if(blankouthud > 0) { blankouthud--; return; }
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
    bool reloading = lastmillis < p->weaponsel->reloading + p->weaponsel->info.reloadtime;
    if((p->state==CS_ALIVE || p->state==CS_EDITING) && !reloading)
    {
        bool drawteamwarning = crosshairteamsign && targetplayer && isteam(targetplayer->team, p->team) && targetplayer->state==CS_ALIVE;
        p->weaponsel->renderaimhelp(drawteamwarning);
    }

    drawdmgindicator();

    if(p->state==CS_ALIVE && !hidehudequipment) drawequipicons(p);

    bool is_spect = (( player1->spectatemode==SM_FOLLOW1ST || player1->spectatemode==SM_FOLLOW3RD || player1->spectatemode==SM_FOLLOW3RD_TRANSPARENT ) &&
            players.inrange(player1->followplayercn) && players[player1->followplayercn]);

    if(/*!menu &&*/ (!hideradar || showmap)) drawradar(p, w, h);
    //if(showsgpat) drawsgpat(w,h); // shotty
    if(!editmode)
    {
        glMatrixMode(GL_MODELVIEW);
        if(!hideteam && m_teammode) drawteamicons(w, h, is_spect);
        glMatrixMode(GL_PROJECTION);
    }

    char *infostr = editinfo();
    int commandh = HUDPOS_Y_BOTTOMLEFT + FONTH;
    if(command) commandh -= rendercommand(-1, HUDPOS_Y_BOTTOMLEFT, VIRTW - FONTH); // dryrun to get height
    else if(infostr) draw_text(infostr, HUDPOS_X_BOTTOMLEFT, HUDPOS_Y_BOTTOMLEFT);
    else if(targetplayer && showtargetname) draw_text(colorname(targetplayer), HUDPOS_X_BOTTOMLEFT, HUDPOS_Y_BOTTOMLEFT);
    glLoadIdentity();
    glOrtho(0, VIRTW*2, VIRTH*2, 0, -1, 1);
    extern int tsens(int x);
    tsens(-2000);
    extern void r_accuracy(int h);
    extern void *scoremenu;
    extern gmenu *curmenu;
    if(!is_spect && !editmode && !watchingdemo && !command && curmenu == scoremenu) r_accuracy(commandh);
    if(!hideconsole) renderconsole();
    formatstring(enginestateinfo)("%d %d %d %d %d", curfps, lod_factor(), nquads, curvert, xtraverts);

    string ltime;
    const char *ltimeformat = getalias("wallclockformat");
    bool wallclock = ltimeformat && *ltimeformat;
    //wallclockformat beginning with "U" shows UTC/GMT time
    if(wallclock) filtertext(ltime, timestring(*ltimeformat != 'U', ltimeformat + int(*ltimeformat == 'U')), FTXT_TOLOWER);

    if(showstats)
    {
        if(showstats==2 && !dbgpos)
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

            if(wallclock) draw_text(ltime, ttll - text_width(ltime), top - 90);
            if(unsavededits) draw_text("U", ttll - text_width("U"), top - 90 - (wallclock ? 2*FONTH/2 : 0));
        }
        else
        {
            if(dbgpos)
            {
                pushfont("mono");
                defformatstring(o_yw)("%05.2f YAW", player1->yaw);
                draw_text(o_yw, VIRTW*2 - ( text_width(o_yw) + FONTH ), VIRTH*2 - 17*FONTH/2);
                defformatstring(o_p)("%05.2f PIT", player1->pitch);
                draw_text(o_p, VIRTW*2 - ( text_width(o_p) + FONTH ), VIRTH*2 - 15*FONTH/2);
                defformatstring(o_x)("%05.2f X  ", player1->o.x);
                draw_text(o_x, VIRTW*2 - ( text_width(o_x) + FONTH ), VIRTH*2 - 13*FONTH/2);
                defformatstring(o_y)("%05.2f Y  ", player1->o.y);
                draw_text(o_y, VIRTW*2 - ( text_width(o_y) + FONTH ), VIRTH*2 - 11*FONTH/2);
                defformatstring(o_z)("%05.2f Z  ", player1->o.z);
                draw_text(o_z, VIRTW*2 - ( text_width(o_z) + FONTH ), VIRTH*2 - 9*FONTH/2);
                popfont();
            }
            defformatstring(c_val)("fps %d", curfps);
            draw_text(c_val, VIRTW*2 - ( text_width(c_val) + FONTH ), VIRTH*2 - 3*FONTH/2);

            if(wallclock) draw_text(ltime, VIRTW*2 - text_width(ltime) - FONTH, VIRTH*2 - 5*FONTH/2);
            if(unsavededits) draw_text("U", VIRTW*2 - text_width("U") - FONTH, VIRTH*2 - (wallclock ? 7 : 5)*FONTH/2);
        }
    }
    else if(wallclock) draw_text(ltime, VIRTW*2 - text_width(ltime) - FONTH, VIRTH*2 - 3*FONTH/2);

    if(!intermission && lastgametimeupdate!=0)
    {
        int cssec = (gametimecurrent+(lastmillis-lastgametimeupdate))/1000;
        int gtsec = cssec%60;
        int gtmin = cssec/60;
        if(gametimedisplay == 1)
        {
            int gtmax = gametimemaximum/60000;
            gtmin = gtmax - gtmin;
            if(gtsec!=0)
            {
                gtmin -= 1;
                gtsec = 60 - gtsec;
            }
        }
        formatstring(gtime)("%02d:%02d", gtmin, gtsec);
        if(gametimedisplay) draw_text(gtime, (VIRTW-225-10)*2 - (text_width(gtime)/2 + FONTH/2), 20);
    }

    if(hidevote < 2 && multiplayer(false))
    {
        extern votedisplayinfo *curvote;

        if(curvote && curvote->millis >= totalmillis && !(hidevote == 1 && curvote->localplayervoted && curvote->result == VOTE_NEUTRAL))
        {
            const int left = 2 * HUDPOS_X_BOTTOMLEFT, top = VIRTH;
            defformatstring(str)("%s called a vote:", curvote->owner ? colorname(curvote->owner) : "");
            draw_text(str, left, top + 240, 255, 255, 255, votealpha);
            draw_text(curvote->desc, left, top + 320, 255, 255, 255, votealpha);
            draw_text("----", left, top + 400, 255, 255, 255, votealpha);
            formatstring(str)("%d yes vs. %d no", curvote->stats[VOTE_YES], curvote->stats[VOTE_NO]);
            draw_text(str, left, top + 480, 255, 255, 255, votealpha);

            switch(curvote->result)
            {
                case VOTE_NEUTRAL:
                    drawvoteicon(left, top, 0, 0, false);
                    if(!curvote->localplayervoted)
                        draw_text("\f3press F1/F2 to vote yes or no", left, top+560, 255, 255, 255, votealpha);
                    break;
                default:
                    drawvoteicon(left, top, (curvote->result-1)&1, 1, false);
                    formatstring(str)("\f3vote %s", curvote->result == VOTE_YES ? "PASSED" : "FAILED");
                    draw_text(str, left, top + 560, 255, 255, 255, votealpha);
                    break;
            }
        }
    }
    //else draw_textf("%c%d here F1/F2 will be praised during a vote", 20*2, VIRTH+560, '\f', 0); // see position (left/top) setting in block above

    if(menu) rendermenu();
    else if(command) renderdoc(40, VIRTH, max(commandh*2 - VIRTH, 0));

    if(!hidespecthud && !menu && p->state==CS_DEAD && p->spectatemode<=SM_DEATHCAM)
    {
        glLoadIdentity();
        glOrtho(0, VIRTW*3/2, VIRTH*3/2, 0, -1, 1);
        const int left = (VIRTW)*3/2, top = (VIRTH*3/2)*3/4;
        draw_textf("SPACE to change view", left - (text_width("SCROLL to change player") + FONTH/2), top);
        if(!m_botmode) draw_textf("SCROLL to change player", left - (text_width("SCROLL to change player") + FONTH/2), top+80);
    }

    extern void renderhudtexturepreviews();
    if(editmode) renderhudtexturepreviews();

    /* * /
    glLoadIdentity();
    glOrtho(0, VIRTW*3/2, VIRTH*3/2, 0, -1, 1);
    const int tbMSGleft = (VIRTW*3/2)*5/6;
    const int tbMSGtop = (VIRTH*3/2)*7/8;
    draw_textf("!TEST BUILD!", tbMSGleft, tbMSGtop);
    / * */

    if(showspeed && !menu)
    {
        glLoadIdentity();
        glPushMatrix();
        glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
        glScalef(0.8, 0.8, 1);
        draw_textf("Speed: %.2f", VIRTW/2, VIRTH, p->vel.magnitudexy());
        glPopMatrix();
    }

    drawscores();
    if(!hidespecthud && spectating && player1->spectatemode!=SM_DEATHCAM)
    {
        glLoadIdentity();
        glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
        const char *specttext = "GHOST";
        if(player1->team == TEAM_SPECT) specttext = "GHOST";
        else if(player1->team == TEAM_CLA_SPECT) specttext = "[CLA]";
        else if(player1->team == TEAM_RVSF_SPECT) specttext = "[RVSF]";
        draw_text(specttext, VIRTW/40, VIRTH/10*7);
        if(is_spect)
        {
            defformatstring(name)("Player %s", colorname(players[player1->followplayercn]));
            draw_text(name, VIRTW/40, VIRTH/10*8);
        }
    }

    if(p->state == CS_ALIVE || (p->state == CS_DEAD && p->spectatemode == SM_DEATHCAM))
    {
        glLoadIdentity();
        glOrtho(0, VIRTW/2, VIRTH/2, 0, -1, 1);

        if(p->state == CS_ALIVE && !hidehudequipment)
        {
            pushfont("huddigits");
            draw_textf("%d", HUDPOS_HEALTH + HUDPOS_NUMBERSPACING, 823, p->health);
            if(p->armour) draw_textf("%d", HUDPOS_ARMOUR + HUDPOS_NUMBERSPACING, 823, p->armour);
            if(p->weaponsel && p->weaponsel->type>=GUN_KNIFE && p->weaponsel->type<NUMGUNS)
            {
                glMatrixMode(GL_MODELVIEW);
                if (p->weaponsel->type!=GUN_GRENADE) p->weaponsel->renderstats();
                else if (p->prevweaponsel->type==GUN_AKIMBO || p->prevweaponsel->type==GUN_PISTOL) p->weapons[p->akimbo ? GUN_AKIMBO : GUN_PISTOL]->renderstats();
                else p->prevweaponsel->renderstats();
                if(p->mag[GUN_GRENADE]) p->weapons[GUN_GRENADE]->renderstats();
                glMatrixMode(GL_PROJECTION);
            }
            popfont();
        }

        if(m_flags && !hidectfhud)
        {
            glLoadIdentity();
            glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
            glColor4f(1.0f, 1.0f, 1.0f, 0.2f);
            turn_on_transparency(255);
            int flagscores[2];
            teamflagscores(flagscores[0], flagscores[1]);

            loopi(2) // flag state
            {
                drawctficon(i*120+VIRTW/4.0f*3.0f, 1650, 120, i, 0, 1/4.0f, flaginfos[i].state == CTFF_INBASE ? 255 : 100);
                if(m_teammode)
                {
                    defformatstring(count)("%d", flagscores[i]);
                    int cw, ch;
                    text_bounds(count, cw, ch);
                    draw_textf("%s", i * 120 + VIRTW / 4.0f * 3.0f + 60 - cw / 2, 1590, count);
                }
            }

            // big flag-stolen icon
            int ft = 0;
            if((flaginfos[0].state==CTFF_STOLEN && flaginfos[0].actor == p && flaginfos[0].ack) ||
               (flaginfos[1].state==CTFF_STOLEN && flaginfos[1].actor == p && flaginfos[1].ack && ++ft))
            {
                drawctficon(VIRTW-225-10, VIRTH*5/8, 225, ft, 1, 1/2.0f, (sinf(lastmillis/100.0f)+1.0f) *128);
            }
        }
    }

    if(!hidehudmsgs) hudmsgs.render();

    glLoadIdentity();
    glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
    if(command) commandh -= rendercommand(HUDPOS_X_BOTTOMLEFT, HUDPOS_Y_BOTTOMLEFT, VIRTW - FONTH);

    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
}


Texture *startscreen = NULL;

void loadingscreen(const char *fmt, ...)
{
    if(!startscreen) startscreen = textureload("packages/misc/startscreen.png", 3);

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
        quad(startscreen->id, (VIRTW-VIRTH)/2, 0, VIRTH, 0, 0, 1);
        if(fmt)
        {
            glEnable(GL_BLEND);
            defvformatstring(str, fmt, fmt);
            int w = text_width(str);
            draw_text(str, w>=VIRTW ? 0 : (VIRTW-w)/2, VIRTH*3/4);
            glDisable(GL_BLEND);
        }
        SDL_GL_SwapWindow(screen);
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
    SDL_GL_SwapWindow(screen);
}

