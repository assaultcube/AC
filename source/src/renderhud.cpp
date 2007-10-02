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

void drawradaricon(float x, float y, float s, int col, int row)
{
    static Texture *tex = NULL;
    if(!tex) tex = textureload("packages/misc/radaricons.png");
    if(tex) 
    {
        glEnable(GL_BLEND);     
        drawicon(tex, x, y, s, col, row, 1/4.0f);
        glDisable(GL_BLEND);
    }
}

void drawctficon(float x, float y, float s, int col, int row, float ts)
{
    static Texture *tex = NULL;
    if(!tex) tex = textureload("packages/misc/ctficons.png");
    if(tex) drawicon(tex, x, y, s, col, row, ts);
}

void drawvoteicon(float x, float y, int col, int row, bool noblend)
{
    static Texture *tex = NULL;
    if(!tex) tex = textureload("packages/misc/voteicons.png");
    if(tex)
    {
        if(noblend) glDisable(GL_BLEND);
        drawicon(tex, x, y, 240, col, row, 1/2.0f);
        if(noblend) glEnable(GL_BLEND);
    }
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
VARP(hidevote, 0, 0, 1);

VAR(showmap, 0, 0, 1);

void drawscope()
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
}

Texture *crosshair = NULL;

void loadcrosshair(char *c)
{
    s_sprintfd(p)("packages/misc/crosshairs/%s", c);
    crosshair = textureload(p);
    if(crosshair==notexture) crosshair = textureload("packages/misc/crosshairs/default.png");
}

COMMAND(loadcrosshair, ARG_1STR);

void drawcrosshair(bool showteamwarning)
{
    glBlendFunc(GL_ONE, GL_ONE);
    static Texture *teammatetex = NULL;
    if(!teammatetex) teammatetex = textureload("packages/misc/teammate.png");
    if(!crosshair) crosshair = textureload("packages/misc/crosshairs/default.png");
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
    drawequipicon(20, 1650, 1, 0, (player1->state!=CS_DEAD && player1->health<=20 && !m_osok));
    
    // weapons
    int c = player1->gunselect, r = 1;
    if(c==GUN_GRENADE) c = r = 0;
    else if(c>2) { c -= 3; r = 2; }

    drawequipicon(1220, 1650, c, r, (!player1->mag[player1->gunselect] && player1->gunselect != GUN_KNIFE && player1->gunselect != GUN_GRENADE));
    glEnable(GL_BLEND);
}

void drawradarent(float x, float y, float yaw, int col, int row, float iconsize, bool pulse, char *label = NULL, ...)
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
        s_sprintfdv(lbl, label);
        draw_text(lbl, (int)(x*2), (int)(y*2));
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
        glPopMatrix();
    }
}

bool insideradar(const vec &centerpos, float radius, const vec &o)
{
    if(showmap) return !o.reject(centerpos, radius);
    return o.distxy(centerpos)<=radius;
}

bool isattacking(playerent *p) { return lastmillis-p->lastaction < 500; }

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
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_BLEND);
    }
    else 
    {
        glDisable(GL_BLEND);
        circle(minimaptex, radarviewsize/2, radarviewsize/2, radarviewsize/2, centerpos.x/worldsize, centerpos.y/worldsize, res/2/worldsize);
    }
    glTranslatef(-(centerpos.x-res/2)/worldsize*radarsize, -(centerpos.y-res/2)/worldsize*radarsize, 0);

    drawradarent(player1->o.x*coordtrans, player1->o.y*coordtrans, player1->yaw, player1->state==CS_ALIVE ? (isattacking(player1) ? 2 : 0) : 1, 2, iconsize, isattacking(player1), colorname(player1)); // local player

    loopv(players) // other players
    {
        playerent *pl = players[i];
        if(!pl || !isteam(player1->team, pl->team) || !insideradar(centerpos, res/2, pl->o)) continue;
        drawradarent(pl->o.x*coordtrans, pl->o.y*coordtrans, pl->yaw, pl->state==CS_ALIVE ? (isattacking(pl) ? 2 : 0) : 1, team_int(pl->team), iconsize, isattacking(pl), colorname(pl));
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
            float yaw = showmap ? 0 : player1->yaw;
            if(f.state==CTFF_STOLEN)
            {
                if(f.actor && i != team_int(player1->team) && insideradar(centerpos, res/2, f.actor->o))
                    drawradarent(f.actor->o.x*coordtrans+iconsize/2, f.actor->o.y*coordtrans+iconsize/2, yaw, 3, f.team, iconsize, true); // draw near flag thief
            }
            else if(insideradar(centerpos, res/2, vec(e->x, e->y, centerpos.z))) drawradarent(e->x*coordtrans, e->y*coordtrans, yaw, 3, f.team, iconsize, false); // draw on entitiy pos
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
        if(!overlaytex) overlaytex = textureload("packages/misc/radaroverlays.png", 3);
        quad(overlaytex->id, VIRTW-overlaysize-10, 10, overlaysize, m_teammode ? 0.5f*team_int(player1->team) : 0, m_teammode ? 0 : 0.5f, 0.5f, 0.5f); 
        glEnable(GL_CULL_FACE);
    }
}

void drawteamicons(int w, int h)
{
    glDisable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3f(1, 1, 1);
    static Texture *icons = NULL;
    if(!icons) icons = textureload("packages/misc/teamicons.png");
    quad(icons->id, VIRTW-VIRTH/12-10, 10, VIRTH/12, team_int(player1->team) ? 0.5f : 0, 0, 0.5f, 0.5f);
    glEnable(GL_CULL_FACE);
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

    playerent *targetplayer = playerincrosshair();
    bool didteamkill = player1->lastteamkill && player1->lastteamkill + 5000 > lastmillis;
    bool menuvisible = rendermenu();
    bool command = getcurcommand() ? true : false;

    if(player1->state==CS_ALIVE && !player1->reloading && !didteamkill && !menuvisible)
    {
        bool drawteamwarning = targetplayer ? (isteam(targetplayer->team, player1->team) && targetplayer->state!=CS_DEAD) : false;
        if(player1->gunselect==GUN_SNIPER && scoped) drawscope();
        else if((player1->gunselect!=GUN_SNIPER || drawteamwarning)) drawcrosshair(drawteamwarning);
    }

    drawequipicons();

    glMatrixMode(GL_MODELVIEW);
    if(!menuvisible && !hideradar) drawradar(w, h);
    else drawteamicons(w, h);
    glMatrixMode(GL_PROJECTION);

    char *infostr = editinfo();
    if(command) rendercommand(20, 1570);
    else if(infostr) draw_text(infostr, 20, 1570);
    else if(targetplayer) draw_text(colorname(targetplayer), 20, 1570);

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
    if(!hidevote)
    {
        extern votedisplayinfo *curvote;

        if(curvote && curvote->millis >= lastmillis)
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
                    draw_textf("\f3press F1/F2 to vote yes or no", left, top+560);
                    break;
                default:
                    drawvoteicon(left, top, (curvote->result-1)&1, 1, false);
                    draw_textf("\f3vote %s", left, top+560, curvote->result == VOTE_YES ? "PASSED" : "FAILED");
                    break;
            }
        }
    }
    if(player1->state==CS_ALIVE)
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
    int side = 2*FONTH;
    float x1 = side, x2 = bar*(VIRTW-2*side)+side;
    float y1 = o*FONTH;
    glColor3f(r, g, b);
    glBegin(GL_TRIANGLE_STRIP);
    loopk(10)
    {
       float c = cosf(M_PI/2 + k/9.0f*M_PI), s = 1 + sinf(M_PI/2 + k/9.0f*M_PI);
       glVertex2f(x2 - c*FONTH, y1 + s*FONTH);
       glVertex2f(x1 + c*FONTH, y1 + s*FONTH);
    }
    glEnd();

    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_LINE_LOOP);
    loopk(10)
    {
        float c = cosf(M_PI/2 + k/9.0f*M_PI), s = 1 + sinf(M_PI/2 + k/9.0f*M_PI);
        glVertex2f(x1 + c*FONTH, y1 + s*FONTH);
    }
    loopk(10)
    {
        float c = cosf(M_PI/2 + k/9.0f*M_PI), s = 1 - sinf(M_PI/2 + k/9.0f*M_PI);
        glVertex2f(x2 - c*FONTH, y1 + s*FONTH);
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
    glOrtho(0, VIRTW, VIRTH, 0, -1, 1);

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

