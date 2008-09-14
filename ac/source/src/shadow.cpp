// shadow.cpp: stencil shadow rendering

#include "pch.h"
#include "cube.h"

VARP(stencilshadow, 0, 40, 100);

int stenciling = 0;

VAR(shadowclip, 0, 1, 1);
VAR(shadowtile, 0, 1, 1);
VAR(dbgtiles, 0, 0, 1);
VAR(shadowcasters, 1, 0, 0);

#define SHADOWTILES 32
#define SHADOWTILEMASK (0xFFFFFFFFU>>(32-SHADOWTILES))
uint shadowtiles[SHADOWTILES+1];
float shadowx1 = 1, shadowy1 = 1, shadowx2 = -1, shadowy2 = -1;

static void extrudeshadowtiles(int x1, int y1, int x2, int y2, int x3, int y3)
{
    if(y1 > y2) { swap(x1, x2); swap(y1, y2); }
    if(y2 > y3) { swap(x2, x3); swap(y2, y3); }
    if(y1 > y2) { swap(x1, x2); swap(y1, y2); }

    if(y3 < 0 || y1 >= SHADOWTILES) return;

    int lx = x1, rx = x1,
        fracl = 0, fracr = 0,
        dlx, dly, drx, dry;
    if(x2 <= x3) { dlx = x2; dly = y2; drx = x3; dry = y3; }
    else { dlx = x3; dly = y3; drx = x2; dry = y2; }
    dlx -= x1; dly -= y1;
    drx -= x1; dry -= y1;
    int ldir = 1, rdir = 1;
    if(dlx < 0)
    {
        ldir = -1;
        dlx = -dlx;
    }
    if(drx < 0)
    {
        rdir = -1;
        drx = -drx;
    }

    int cy = y1;
    y2 = min(y2, SHADOWTILES);
    if(cy < 0 && y2 > cy)
    {
        int dy = min(y2, 0) - cy;
        cy += dy;
        fracl += dy*dlx;
        lx += ldir*(fracl/dly);
        fracl %= dly;
        fracr += dy*drx;
        rx += rdir*(fracr/dry);
        fracr %= dry;
    }

    for(; cy < y2; cy++)
    {
        int cx1 = lx, cx2 = rx;
        fracl += dlx;
        while(fracl >= dly) { lx += ldir; if(ldir < 0) cx1 = lx; fracl -= dly; }
        fracr += drx;
        while(fracr >= dry) { rx += rdir; if(rdir > 0) cx2 = rx; fracr -= dry; }
        if(cx1 < SHADOWTILES && cx2 >= 0) shadowtiles[cy] |= (SHADOWTILEMASK>>(SHADOWTILES - (min(cx2, SHADOWTILES-1)+1))) & (SHADOWTILEMASK<<max(cx1, 0));
    }

    if(cy >= SHADOWTILES) return;

    if(x2 < x3 || y1 == cy)
    {
        if(x2 < lx) lx = x2;
        dlx = x3 - lx; dly = y3 - cy;
        ldir = 1;
        if(dlx < 0)
        {
            ldir = -1;
            dlx = -dlx;
        }
        fracl = 0;
    }
    if(x2 > x3 || y1 == cy) 
    {
        if(x2 > rx) rx = x2;
        drx = x3 - rx; dry = y3 - cy;
        rdir = 1;
        if(drx < 0)
        {
            rdir = -1;
            drx = -drx;
        }
        fracr = 0;
    }

    y3 = min(y3, SHADOWTILES);
    if(cy < 0 && y3 > cy)
    {
        int dy = min(y3, 0) - cy;
        cy += dy;
        fracl += dy*dlx;
        lx += ldir*(fracl/dly);
        fracl %= dly;
        fracr += dy*drx;
        rx += rdir*(fracr/dry);
        fracr %= dry;
    }

    for(; cy < y3; cy++)
    {
        int cx1 = lx, cx2 = rx;
        fracl += dlx;
        while(fracl >= dly) { lx += ldir; if(ldir < 0) cx1 = lx; fracl -= dly; }
        fracr += drx;
        while(fracr >= dry) { rx += rdir; if(rdir > 0) cx2 = rx; fracr -= dry; }
        if(cx1 < SHADOWTILES && cx2 >= 0) shadowtiles[cy] |= (SHADOWTILEMASK>>(SHADOWTILES - (min(cx2, SHADOWTILES-1)+1))) & (SHADOWTILEMASK<<max(cx1, 0));
    }

    if(cy < SHADOWTILES)
    {
        int cx1 = lx, cx2 = rx;
        if(dly)
        {
            fracl += dlx;
            while(fracl >= dly) { lx += ldir; if(ldir < 0) cx1 = lx; fracl -= dly; }
        }
        if(dry)
        {
            fracr += drx;
            while(fracr >= dry) { rx += rdir; if(rdir > 0) cx2 = rx; fracr -= dry; }
        }
        if(cx1 < SHADOWTILES && cx2 >= 0) shadowtiles[cy] |= (SHADOWTILEMASK>>(SHADOWTILES - (min(cx2, SHADOWTILES-1)+1))) & (SHADOWTILEMASK<<max(cx1, 0));
    }
}

static void addshadowtiles(float x1, float y1, float x2, float y2)
{
    shadowx1 = min(shadowx1, x1);
    shadowy1 = min(shadowy1, y1);
    shadowx2 = max(shadowx2, x2);
    shadowy2 = max(shadowy2, y2);

    int tx1 = clamp(int(floor((x1 + 1)/2 * SHADOWTILES)), 0, SHADOWTILES - 1),
        ty1 = clamp(int(floor((y1 + 1)/2 * SHADOWTILES)), 0, SHADOWTILES - 1),
        tx2 = clamp(int(floor((x2 + 1)/2 * SHADOWTILES)), 0, SHADOWTILES - 1),
        ty2 = clamp(int(floor((y2 + 1)/2 * SHADOWTILES)), 0, SHADOWTILES - 1);

    uint mask = (SHADOWTILEMASK>>(SHADOWTILES - (tx2+1))) & (SHADOWTILEMASK<<tx1);
    for(int y = ty1; y <= ty2; y++) shadowtiles[y] |= mask;
}

bool addshadowbox(const vec &bbmin, const vec &bbmax, const vec &extrude, const glmatrixf &mat)
{
    vec4 v[8];
    float sx1 = 1e16f, sy1 = 1e16f, sx2 = -1e16f, sy2 = -1e16f;
    int front = 0;
    loopi(8)
    {
        vec4 &p = v[i];
        mat.transform(vec(i&1 ? bbmax.x : bbmin.x, i&2 ? bbmax.y : bbmin.y, i&4 ? bbmax.z : bbmin.z), p); 
        if(p.z >= 0)
        {
            float x = p.x / p.w, y = p.y / p.w;
            sx1 = min(sx1, x);
            sy1 = min(sy1, y);
            sx2 = max(sx2, x);
            sy2 = max(sy2, y);
            front++;
        }
    }
    vec4 ev;
    mat.transform(extrude, ev);
    if(ev.z < 0 && !front) return false;
    if(front < 8 || ev.z < 0) loopi(8)
    {
        const vec4 &p = v[i];
        if(p.z >= 0) 
        {
            if(ev.z >= 0) continue;
            float t = ev.z/(ev.z - p.z),
                  w = ev.w + t*(p.w - ev.w),
                  x = (ev.x + t*(p.x - ev.x))/w,
                  y = (ev.y + t*(p.y - ev.y))/w;
            sx1 = min(sx1, x);
            sy1 = min(sy1, y);
            sx2 = max(sx2, x);
            sy2 = max(sy2, y);
            continue;
        }
        loopj(3)
        {
            const vec4 &o = v[i^(1<<j)];
            if(o.z < 0) continue;
            float t = p.z/(p.z - o.z),
                  w = p.w + t*(o.w - p.w),
                  x = (p.x + t*(o.x - p.x))/w,
                  y = (p.y + t*(o.y - p.y))/w;
            sx1 = min(sx1, x);
            sy1 = min(sy1, y);
            sx2 = max(sx2, x);
            sy2 = max(sy2, y);
        }
        if(ev.z < 0) continue;
        float t = p.z/(p.z - ev.z),
              w = p.w + t*(ev.w - p.w),
              x = (p.x + t*(ev.x - p.x))/w,
              y = (p.y + t*(ev.y - p.y))/w;
        sx1 = min(sx1, x);
        sy1 = min(sy1, y);
        sx2 = max(sx2, x);
        sy2 = max(sy2, y);
    }
    if(ev.z >= 0)
    {
        float x = ev.x/ev.w, y = ev.y/ev.w;
        if((sx1 >= 1 && x >= 1) || (sy1 >= 1 && y >= 1) || (sx2 <= -1 && x <= -1) || (sy2 <= -1 && y <= -1)) return false;
        int tx = int(floor(SHADOWTILES * (x + 1) / 2)), ty = int(floor(SHADOWTILES * (y + 1) / 2)),
            tx1 = int(floor(SHADOWTILES * (sx1 + 1) / 2)), ty1 = int(floor(SHADOWTILES * (sy1 + 1) / 2)),
            tx2 = int(floor(SHADOWTILES * (sx2 + 1) / 2)), ty2 = int(floor(SHADOWTILES * (sy2 + 1) / 2));
        if(tx < tx1)
        {
            if(ty < ty1) { swap(ty1, ty2); tx1--; ty1--; }
            else if(ty > ty2) { tx1--; ty1++; }
            else { tx2 = tx1; tx1--; tx2--; }
        }
        else if(tx > tx2)
        {
            if(ty < ty1) { ty1--; tx2++; }
            else if(ty > ty2) { swap(ty1, ty2); ty1++; tx2++; }
            else { tx1 = tx2; tx1++; tx2++; }
        }
        else
        {
            if(ty < ty1) { ty2 = ty1; ty1--; ty2--; }
            else if(ty > ty2) { ty1 = ty2; ty1++; ty2++; }
            else goto noextrusion;
        }
        extrudeshadowtiles(tx, ty, tx1, ty1, tx2, ty2);
        shadowx1 = min(x, shadowx1);
        shadowy1 = min(y, shadowy1);
        shadowx2 = max(x, shadowx2);
        shadowy2 = max(y, shadowy2);
    noextrusion:;
    }
    else if(sx1 >= 1 || sy1 >= 1 || sx2 <= -1 || sy2 <= -1) return false;
    shadowcasters++;
    addshadowtiles(sx1, sy1, sx2, sy2);
    return true;
}

static void rendershadowtiles()
{
    shadowx1 = clamp(shadowx1, -1.0f, 1.0f);
    shadowy1 = clamp(shadowy1, -1.0f, 1.0f);
    shadowx2 = clamp(shadowx2, -1.0f, 1.0f);
    shadowy2 = clamp(shadowy2, -1.0f, 1.0f);

    if(shadowx1 >= shadowx2 || shadowy1 >= shadowy2) return;

    float clipx1 = (shadowx1 + 1) / 2,
          clipy1 = (shadowy1 + 1) / 2,
          clipx2 = (shadowx2 + 1) / 2,
          clipy2 = (shadowy2 + 1) / 2;
    if(!shadowclip)
    {
        clipx1 = clipy1 = 0;
        clipx2 = clipy2 = 1;
    }

    if(!shadowtile)
    {
        glBegin(GL_QUADS);
        glVertex2f(clipx1, clipy2);
        glVertex2f(clipx2, clipy2);
        glVertex2f(clipx2, clipy1);
        glVertex2f(clipx1, clipy1);
        xtraverts += 4;
        glEnd();
        return;
    }

    glBegin(GL_QUADS);
    float tsz = 1.0f/SHADOWTILES;
    loop(y, SHADOWTILES+1)
    {
        uint mask = shadowtiles[y];
        int x = 0;
        while(mask)
        {
            while(!(mask&0xFF)) { mask >>= 8; x += 8; }
            while(!(mask&1)) { mask >>= 1; x++; }
            int xstart = x;
            do { mask >>= 1; x++; } while(mask&1);
            uint strip = (SHADOWTILEMASK>>(SHADOWTILES - x)) & (SHADOWTILEMASK<<xstart);
            int yend = y;
            do { shadowtiles[yend] &= ~strip; yend++; } while((shadowtiles[yend] & strip) == strip);
            float vx = xstart*tsz,
                  vy = y*tsz,
                  vw = (x-xstart)*tsz,
                  vh = (yend-y)*tsz,
                  vx1 = max(vx, clipx1),
                  vy1 = max(vy, clipy1),
                  vx2 = min(vx+vw, clipx2),
                  vy2 = min(vy+vh, clipy2);
            glVertex2f(vx1, vy2);
            glVertex2f(vx2, vy2);
            glVertex2f(vx2, vy1);
            glVertex2f(vx1, vy1);
            xtraverts += 4;
        }
    }
    glEnd();
}

void drawstencilshadows()
{
    glDisable(GL_FOG);
    glEnable(GL_STENCIL_TEST);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    stenciling = 1;

    shadowcasters = 0;
    shadowx2 = shadowy2 = -1;
    shadowx1 = shadowy1 = 1;
    memset(shadowtiles, 0, sizeof(shadowtiles));

    if((hasST2 || hasSTS) && hasSTW)
    {
        glDisable(GL_CULL_FACE);

        if(hasST2)
        {
            glEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);

            glActiveStencilFace_(GL_BACK);
            glStencilFunc(GL_ALWAYS, 0, ~0U);
            glStencilOp(GL_KEEP, GL_KEEP, GL_DECR_WRAP_EXT);

            glActiveStencilFace_(GL_FRONT);
            glStencilFunc(GL_ALWAYS, 0, ~0U);
            glStencilOp(GL_KEEP, GL_KEEP, GL_INCR_WRAP_EXT);
        }
        else
        {
            glStencilFuncSeparate_(GL_ALWAYS, GL_ALWAYS, 0, ~0U);
            glStencilOpSeparate_(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP_EXT);
            glStencilOpSeparate_(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP_EXT);
        }

        startmodelbatches();
        renderentities();
        renderclients();
        renderbounceents();
        endmodelbatches();

        if(hasST2) glDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);
        glEnable(GL_CULL_FACE);
    }
    else
    {
        glStencilFunc(GL_ALWAYS, 0, ~0U);
        glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);

        startmodelbatches();
        renderentities();
        renderclients();
        renderbounceents();
        endmodelbatches(false);

        stenciling = 2;

        glStencilFunc(GL_ALWAYS, 0, ~0U);
        glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
        glCullFace(GL_BACK);

        endmodelbatches(true);

        glCullFace(GL_FRONT);
    }

    stenciling = 0;

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    if(shadowcasters)
    {
        glDisable(GL_DEPTH_TEST);

        glStencilFunc(GL_NOTEQUAL, 0, ~0U);
        glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ZERO, GL_SRC_COLOR);

        float intensity = 1.0f - stencilshadow/100.0f;
        glColor3f(intensity, intensity, intensity);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, 1, 0, 1, -1, 1);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        static uint debugtiles[SHADOWTILES+1];
        if(dbgtiles) memcpy(debugtiles, shadowtiles, sizeof(debugtiles));

        rendershadowtiles();

        if(dbgtiles)
        {
            glDisable(GL_STENCIL_TEST);
            glColor3f(0.5f, 1, 0.5f);
            memcpy(shadowtiles, debugtiles, sizeof(debugtiles));
            rendershadowtiles();
            glColor3f(0, 0, 1);
            glDisable(GL_BLEND);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            memcpy(shadowtiles, debugtiles, sizeof(debugtiles));
            rendershadowtiles();
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(fovy, aspect, 0.15f, farplane);

        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();

        glEnable(GL_DEPTH_TEST);
    }

    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_TEXTURE_2D);
}

