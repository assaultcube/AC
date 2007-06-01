// renderhud.cpp: HUD rendering

#include "cube.h"

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

VARP(crosshairsize, 0, 15, 50);

int dblend = 0;
void damageblend(int n) { dblend += n; }

VARP(hidestats, 0, 1, 1);
VARP(crosshairfx, 0, 1, 1);
VARP(hideradar, 0, 0, 1);
VARP(radarres, 1, 64, 1024);
VARP(radarentsize, 1, 4, 64);
VARP(hidectfhud, 0, 0, 1);
VARP(hideteamhud, 0, 1, 1);
VARP(hidedemohud, 0, 0, 1);

VAR(showmap, 0, 0, 1);

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

void drawradarent(float x, float y, float yaw, int col, int row, float iconsize, bool blend, char *label = NULL, ...)
{
    if(label && showmap) glColor3f(1, 1, 1);
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(yaw, 0, 0, 1);
    drawradaricon(-iconsize/2.0f, -iconsize/2.0f, iconsize, col, row, blend); 
    glPopMatrix();
    if(label && showmap)
    {
        glPushMatrix();
        glEnable(GL_BLEND);
        glTranslatef(iconsize/2, iconsize/2, 0);
        glScalef(1/2.0f, 1/2.0f, 1/2.0f);
        s_sprintfdv(lbl, label);
        draw_text(lbl, (int)(x*2), (int)(y*2));
        glDisable(GL_BLEND);
        glPopMatrix();
    }
}

bool insideradar(const vec &centerpos, float radius, const vec &o)
{
    if(showmap) return !o.reject(centerpos, radius);
    return o.distxy(centerpos)<=radius;
}

void drawradar(int w, int h)
{
    vec center = showmap ? vec(ssize/2, ssize/2, 0) : player1->o;
    int res = showmap ? ssize : radarres;

    float worldsize = (float)ssize;
    float radarviewsize = showmap ? VIRTH : VIRTH/6;
    float radarsize = worldsize/res*radarviewsize;
    float iconsize = radarentsize/(float)res*radarviewsize;
    float coordtrans = radarsize/worldsize;
    float overlaysize = radarviewsize*4.0f/3.25f;

    glPushMatrix();

    if(showmap) glTranslatef(VIRTW/2-radarviewsize/2, 0, 0);
    else 
    {
        glTranslatef(VIRTW-radarviewsize-(overlaysize-radarviewsize)/2-10+radarviewsize/2, 10+(overlaysize-radarviewsize)/2+radarviewsize/2, 0);
        glRotatef(-camera1->yaw, 0, 0, 1);
        glTranslatef(-radarviewsize/2, -radarviewsize/2, 0);
    }

    extern GLuint minimaptex;

    vec centerpos(min(max(center.x, res/2), worldsize-res/2), min(max(center.y, res/2), worldsize-res/2), 0);
    if(showmap) 
    {
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
        quad(minimaptex, 0, 0, radarviewsize, (centerpos.x-res/2)/worldsize, (centerpos.y-res/2)/worldsize, res/worldsize);
        glDisable(GL_BLEND);
    }
    else 
    {
        glDisable(GL_BLEND);
        circle(minimaptex, radarviewsize/2, radarviewsize/2, radarviewsize/2, centerpos.x/worldsize, centerpos.y/worldsize, res/2/worldsize);
    }
    glTranslatef(-(centerpos.x-res/2)/worldsize*radarsize, -(centerpos.y-res/2)/worldsize*radarsize, 0);

    drawradarent(player1->o.x*coordtrans, player1->o.y*coordtrans, player1->yaw, player1->state==CS_ALIVE ? (player1->attacking ? 2 : 0) : 1, 2, iconsize, false, player1->name); // local player

    loopv(players) // other players
    {
        playerent *pl = players[i];
        if(!pl || !isteam(player1->team, pl->team) || !insideradar(centerpos, res/2, pl->o)) continue;
        drawradarent(pl->o.x*coordtrans, pl->o.y*coordtrans, pl->yaw, pl->state==CS_ALIVE ? (pl->attacking ? 2 : 0) : 1, team_int(pl->team), iconsize, false, pl->name);
    }
    if(m_ctf)
    {
        glColor4f(1.0f, 1.0f, 1.0f, (sinf(lastmillis / 100.0f) + 1.0f) / 2.0f);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        loopi(2) // flag items
        {
            flaginfo &f = flaginfos[i];
            entity *e = f.flag;
            if(!e) continue;
            if(f.state==CTFF_STOLEN && f.actor && insideradar(centerpos, res/2, f.actor->o))
                drawradarent(f.actor->o.x*coordtrans+iconsize/2, f.actor->o.y*coordtrans+iconsize/2, 0, 3, f.team, iconsize, true); // draw near flag thief
            else if(insideradar(centerpos, res/2, vec(e->x, e->y, centerpos.z))) drawradarent(e->x*coordtrans, e->y*coordtrans, 0, 3, f.team, iconsize, false); // draw on entitiy pos
        }
    }

    glEnable(GL_BLEND);
    glPopMatrix();

    if(!showmap)
    {
        glDisable(GL_CULL_FACE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor3f(1, 1, 1);
        static Texture *overlaytex = NULL;
        if(!overlaytex) overlaytex = textureload("packages/misc/radaroverlay.png", 3);
        quad(overlaytex->id, VIRTW-overlaysize-10, 10, overlaysize, 0.5f*team_int(player1->team), 0, 0.5f, 1.0f); 
        glEnable(GL_CULL_FACE);
    }
}

void gl_drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater)
{
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
    glEnable(GL_BLEND);

    if(dblend || underwater)
    {
        glDepthMask(GL_FALSE);
        if(dblend) 
        {
            glBlendFunc(GL_ZERO, GL_SRC_COLOR);
            glColor3f(1.0f, 0.1f, 0.1f);
        }
        else
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4ub(hdr.watercolor[0], hdr.watercolor[1], hdr.watercolor[2], 102);
        }
        glBegin(GL_QUADS);
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

    bool demo3rd = demoplayback && !localdemoplayer1st();
    playerent *targetplayer = playerincrosshair();
    bool didteamkill = player1->lastteamkill && player1->lastteamkill + 5000 > lastmillis;
    bool menuvisible = rendermenu();
    bool command = getcurcommand() ? true : false;

    if(!demo3rd)
    {
        if(player1->state==CS_ALIVE && !player1->reloading && !didteamkill && !menuvisible)
        {
            bool drawteamwarning = targetplayer ? (isteam(targetplayer->team, player1->team) && targetplayer->state!=CS_DEAD) : false;
            if(player1->gunselect==GUN_SNIPER && scoped) drawscope();
            else if((player1->gunselect!=GUN_SNIPER || drawteamwarning)) drawcrosshair(drawteamwarning);
        }

        drawequipicons();

        if(!hideradar) 
        {
            glMatrixMode(GL_MODELVIEW);
            drawradar(w, h);
            glMatrixMode(GL_PROJECTION);
        }

        char *infostr = editinfo();
        if(command) rendercommand(20, 1570);
        else if(infostr) draw_text(infostr, 20, 1570);
        else if(targetplayer) draw_text(targetplayer->name, 20, 1570);
    }

    glLoadIdentity();
    glOrtho(0, VIRTW*2, VIRTH*2, 0, -1, 1);

    renderconsole();
    if(command) renderdoc(40, VIRTH);
    if(!hidestats)
    {
        const int left = (VIRTW-225-10)*2, top = (VIRTH*7/8)*2;
        draw_textf("fps %d", left, top, curfps);
        draw_textf("lod %d", left, top+80, lod_factor());
        draw_textf("wqd %d", left, top+160, nquads); 
        draw_textf("wvt %d", left, top+240, curvert);
        draw_textf("evt %d", left, top+320, xtraverts);
    }
    
    if(!hidedemohud && demoplayback)
    {   
        const int left = VIRTW*2/80, top = VIRTH*2*3/4;
        int dmillis = demomillis();
        s_sprintfd(time)("%d:%02d", dmillis/1000/60, dmillis/1000%60);
        s_sprintfd(following)("\f0Following %s", demoplayer->name);

        draw_text(time, left, top-FONTH);
        draw_text(following, left, top);
        draw_text("jump to pause", left, top+2*FONTH);
        draw_text("attack to change view", left, top+3*FONTH);
        draw_text("reload for slow-motion", left, top+4*FONTH);
    }

    if(player1->state==CS_ALIVE && !demo3rd)
    {
        glLoadIdentity();
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

        if(m_teammode && !hideteamhud)
        {
            glLoadIdentity();
            glOrtho(0, VIRTW, VIRTH, 0, -1, 1);
            glDisable(GL_BLEND);
            drawctficon(VIRTW-120-10, VIRTH/6+10+10, 120, team_int(player1->team), 1, 1/4.0f); // local players team
            glEnable(GL_BLEND);
            draw_textf(player1->team, VIRTW-VIRTH/6-10, VIRTH/6+10+10+120/2);
        }

		if(didteamkill)
		{
			glLoadIdentity();
			glOrtho(0, VIRTW/3, VIRTH/3, 0, -1, 1);
			draw_text("\f3you killed a teammate", VIRTW/3/40, VIRTH/3/2);
		}

        if(m_ctf && !hidectfhud)
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
            flaginfo &f = flaginfos[team_opposite(team_int(player1->team))];
            if(f.state==CTFF_STOLEN && f.actor == player1 && f.ack)
            {
                glColor4f(1.0f, 1.0f, 1.0f, (sinf(lastmillis/100.0f)+1.0f) / 2.0f);
                glEnable(GL_BLEND);
                drawctficon(VIRTW-225-10, VIRTH*5/8, 225, team_opposite(team_int(player1->team)), 1, 1/2.0f);
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

    loopi(fmt ? 1 : 2)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        quad(logo->id, (VIRTW-VIRTH)/2, 0, VIRTH, 0, 0, 1);
        if(fmt)
        {
            glEnable(GL_BLEND);
            s_sprintfdlv(str, fmt, fmt);
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
    int side = 50;
    glColor3f(r, g, b);
    glVertex2f(side,                    o*FONTH);
    glVertex2f(bar*(VIRTW-2*side)+side, o*FONTH);
    glVertex2f(bar*(VIRTW-2*side)+side, (o+2)*FONTH);
    glVertex2f(side,                    (o+2)*FONTH);
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
    glOrtho(0, VIRTW, VIRTH, 0, -1, 1);

    glBegin(GL_QUADS);

    if(text1)
    {
        bar(1,    1, 0.1f, 0.1f, 0.1f);
        bar(bar1, 1, 0.2f, 0.2f, 0.2f);
    }

    if(bar2>0)
    {
        bar(1,    3, 0.1f, 0.1f, 0.1f);
        bar(bar2, 3, 0.2f, 0.2f, 0.2f);
    }

    glEnd();

    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    if(text1) draw_text(text1, 70, 1*FONTH + FONTH/2);
    if(bar2>0) draw_text(text2, 70, 3*FONTH + FONTH/2);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_DEPTH_TEST);
    SDL_GL_SwapBuffers();
}

