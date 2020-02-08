// physics.cpp: no physics books were hurt nor consulted in the construction of this code.
// All physics computations and constants were invented on the fly and simply tweaked until
// they "felt right", and have no basis in reality. Collision detection is simplistic but
// very robust (uses discrete steps at fixed fps).

#include "cube.h"

float raycube(const vec &o, const vec &ray, vec &surface)
{
    surface = vec(0, 0, 0);

    if(ray.iszero()) return -1;

    vec v = o;
    float dist = 0, dx = 0, dy = 0, dz = 0;

    int nr;
    for(nr=0;nr<512;nr++) // sam's suggestion :: I found no map which got nr > 350
    {
        int x = int(v.x), y = int(v.y);
        if(x < 0 || y < 0 || x >= ssize || y >= ssize) return -1;
        sqr *s = S(x, y);
        float floor = s->floor, ceil = s->ceil;
        if(s->type==FHF) floor -= s->vdelta/4.0f;
        if(s->type==CHF) ceil += s->vdelta/4.0f;
        if(SOLID(s) || v.z < floor || v.z > ceil)
        {
            if((!dx && !dy) || s->wtex==DEFAULT_SKY || (!SOLID(s) && v.z > ceil && s->ctex==DEFAULT_SKY)) return -1;
            if(s->type!=CORNER)// && s->type!=FHF && s->type!=CHF)
            {
                if(dx<dy) surface.x = ray.x>0 ? -1 : 1;
                else surface.y = ray.y>0 ? -1 : 1;
                sqr *n = S(x+(int)surface.x, y+(int)surface.y);
                if(SOLID(n) || (v.z < floor && v.z < n->floor) || (v.z > ceil && v.z > n->ceil))
                {
                    surface = dx<dy ? vec(0, ray.y>0 ? -1 : 1, 0) : vec(ray.x>0 ? -1 : 1, 0, 0);
                    n = S(x+(int)surface.x, y+(int)surface.y);
                    if(SOLID(n) || (v.z < floor && v.z < n->floor) || (v.z > ceil && v.z > n->ceil))
                        surface = vec(0, 0, ray.z>0 ? -1 : 1);
                }
            }
            dist = max(dist-0.1f, 0.0f);
            break;
        }
        dx = ray.x ? (x + (ray.x > 0 ? 1 : 0) - v.x)/ray.x : 1e16f;
        dy = ray.y ? (y + (ray.y > 0 ? 1 : 0) - v.y)/ray.y : 1e16f;
        dz = ray.z ? ((ray.z > 0 ? ceil : floor) - v.z)/ray.z : 1e16f;
        if(dz < dx && dz < dy)
        {
            if(ray.z>0 && s->ctex==DEFAULT_SKY) return -1;
            if(s->type!=FHF && s->type!=CHF) surface.z = ray.z>0 ? -1 : 1;
            dist += dz;
            break;
        }
        float disttonext = 0.1f + min(dx, dy);
        v.add(vec(ray).mul(disttonext));
        dist += disttonext;
    }
    if (nr == 512) return -1;
    return dist;
}

bool raycubelos(const vec &from, const vec &to, float margin)
{
    vec dir(to);
    dir.sub(from);
    float limit = dir.magnitude();
    dir.mul(1.0f/limit);
    vec surface;
    float dist = raycube(from, dir, surface);
    return dist > max(limit - margin, 0.0f);
}

physent *hitplayer = NULL;

bool plcollide(physent *d, physent *o, float &headspace, float &hi, float &lo)          // collide with physent
{
    if(o->state!=CS_ALIVE || !o->cancollide) return false;
    const float r = o->radius+d->radius;
    const vec dr = vec(o->o.x-d->o.x,o->o.y-d->o.y,0);
    const float deyeheight = d->eyeheight, oeyeheight = o->eyeheight;
    if((d->type==ENT_PLAYER && o->type==ENT_PLAYER ? dr.sqrxy() < r*r : fabs(dr.x)<r && fabs(dr.y)<r) && dr.dotxy(d->vel) >= 0.0f)
    {
        if(d->o.z-deyeheight<o->o.z-oeyeheight) { if(o->o.z-oeyeheight<hi) hi = o->o.z-oeyeheight-1; }
        else if(o->o.z+o->aboveeye>lo) lo = o->o.z+o->aboveeye+1;

        if(fabs(o->o.z-d->o.z)<o->aboveeye+deyeheight) { hitplayer = o; return true; }
        headspace = d->o.z-o->o.z-o->aboveeye-deyeheight;
        if(headspace<0) headspace = 10;
    }
    return false;
}

bool cornertest(int mip, int x, int y, int dx, int dy, int &bx, int &by, int &bs)    // recursively collide with a mipmapped corner cube
{
    sqr *w = wmip[mip];
    int mfactor = sfactor - mip;
    bool stest = SOLID(SWS(w, x+dx, y, mfactor)) && SOLID(SWS(w, x, y+dy, mfactor));
    mip++;
    x /= 2;
    y /= 2;
    if(SWS(wmip[mip], x, y, mfactor-1)->type==CORNER)
    {
        bx = x<<mip;
        by = y<<mip;
        bs = 1<<mip;
        return cornertest(mip, x, y, dx, dy, bx, by, bs);
    }
    return stest;
}

bool mmcollide(physent *d, float &hi, float &lo)           // collide with a mapmodel
{
    const float eyeheight = d->eyeheight;
    const float playerheight = eyeheight + d->aboveeye;
    loopv(ents)
    {
        entity &e = ents[i];
        // if(e.type==CLIP || (e.type == PLCLIP && d->type == ENT_PLAYER))
        if (e.type==CLIP || (e.type == PLCLIP && (d->type == ENT_BOT || d->type == ENT_PLAYER || (d->type == ENT_BOUNCE && ((bounceent *)d)->plclipped)))) // don't allow bots to hack themselves into plclips - Bukz 2011/04/14
        {
            if(fabs(e.x-d->o.x) < e.attr2 + d->radius && fabs(e.y-d->o.y) < e.attr3 + d->radius)
            {
                const float cz = float(S(e.x, e.y)->floor+e.attr1), ch = float(e.attr4);
                const float dz = d->o.z-d->eyeheight;
                if(dz < cz - 0.001) { if(cz<hi) hi = cz; }
                else if(cz+ch>lo) lo = cz+ch;
                if(hi-lo < playerheight) return true;
            }
        }
        else if(e.type==MAPMODEL)
        {
            mapmodelinfo *mmi = getmminfo(e.attr2);
            if(!mmi || !mmi->h) continue;
            const float r = mmi->rad+d->radius;
            if(fabs(e.x-d->o.x)<r && fabs(e.y-d->o.y)<r)
            {
                const float mmz = float(S(e.x, e.y)->floor+mmi->zoff+e.attr3);
                const float dz = d->o.z-eyeheight;
                if(dz<mmz) { if(mmz<hi) hi = mmz; }
                else if(mmz+mmi->h>lo) lo = mmz+mmi->h;
                if(hi-lo < playerheight) return true;
            }
        }
    }
    return false;
}

bool objcollide(physent *d, const vec &objpos, float objrad, float objheight) // collide with custom/typeless objects
{
    const float r = d->radius+objrad;
    if(fabs(objpos.x-d->o.x)<r && fabs(objpos.y-d->o.y)<r)
    {
        const float maxdist = (d->eyeheight+d->aboveeye+objheight)/2.0f;
        const float dz = d->o.z+(-d->eyeheight+d->aboveeye)/2.0f;
        const float objz = objpos.z+objheight/2.0f;
        return dz-objz <= maxdist && dz-objz >= -maxdist;
    }
    return false;
}

// all collision happens here
// spawn is a dirty side effect used in spawning
// drop & rise are supplied by the physics below to indicate gravity/push for current mini-timestep
static int cornersurface = 0;

bool collide(physent *d, bool spawn, float drop, float rise, int level) // levels 1 = map, 2 = players, 4 = models, default: 1+2+4
{
    cornersurface = 0;
    const float fx1 = d->o.x-d->radius;     // figure out integer cube rectangle this entity covers in map
    const float fy1 = d->o.y-d->radius;
    const float fx2 = d->o.x+d->radius;
    const float fy2 = d->o.y+d->radius;
    const int x1 = int(fx1);
    const int y1 = int(fy1);
    const int x2 = int(fx2);
    const int y2 = int(fy2);
    float hi = 127, lo = -128;
    const float eyeheight = d->eyeheight;
    const float playerheight = eyeheight + d->aboveeye;

    if(level&1) for(int y = y1; y<=y2; y++) for(int x = x1; x<=x2; x++)     // collide with map
    {
        if(OUTBORD(x,y)) return true;
        sqr *s = S(x,y);
        float ceil = s->ceil;
        float floor = s->floor;
        switch(s->type)
        {
            case SOLID:
                return true;

            case CORNER:
            {
                int bx = x, by = y, bs = 1;
                cornersurface = 1;
                if((x==x1 && y==y2 && cornertest(0, x, y, -1,  1, bx, by, bs) && fx1-bx<=fy2-by)
                || (x==x2 && y==y1 && cornertest(0, x, y,  1, -1, bx, by, bs) && fx2-bx>=fy1-by) || !(++cornersurface)
                || (x==x1 && y==y1 && cornertest(0, x, y, -1, -1, bx, by, bs) && fx1-bx+fy1-by<=bs)
                || (x==x2 && y==y2 && cornertest(0, x, y,  1,  1, bx, by, bs) && fx2-bx+fy2-by>=bs))
                    return true;
                cornersurface = 0;
                break;
            }

            case FHF:       // FIXME: too simplistic collision with slopes, makes it feels like tiny stairs
                floor -= (s->vdelta+S(x+1,y)->vdelta+S(x,y+1)->vdelta+S(x+1,y+1)->vdelta)/16.0f;
                break;

            case CHF:
                ceil += (s->vdelta+S(x+1,y)->vdelta+S(x,y+1)->vdelta+S(x+1,y+1)->vdelta)/16.0f;

        }
        if(ceil<hi) hi = ceil;
        if(floor>lo) lo = floor;
    }

    if(level&1 && hi-lo < playerheight) return true;

    float headspace = 10.0f;

    if( level&2 && d->type!=ENT_CAMERA)
    {
        loopv(players)       // collide with other players
        {
            playerent *o = players[i];
            if(!o || o==d || (o==player1 && d->type==ENT_CAMERA)) continue;
            if(plcollide(d, o, headspace, hi, lo)) return true;
        }
        if(d!=player1) if(plcollide(d, player1, headspace, hi, lo)) return true;
    }

    headspace -= 0.01f;
    if( level&4 && mmcollide(d, hi, lo)) return true;    // collide with map models

    if(spawn)
    {
        d->o.z = lo+eyeheight;       // just drop to floor (sideeffect)
        d->onfloor = true;
    }
    else
    {
        const float spacelo = d->o.z-eyeheight-lo;
        if(spacelo<0)
        {
            if(spacelo>-0.01)
            {
                d->o.z = lo+eyeheight;   // stick on step
            }
            else if(spacelo>-1.26f && d->type!=ENT_BOUNCE) d->o.z += rise;       // rise thru stair
            else return true;
        }
        else
        {
            d->o.z -= min(min(drop, spacelo), headspace);       // gravity
        }

        const float spacehi = hi-(d->o.z+d->aboveeye);
        if(spacehi<0)
        {
            if(spacehi<-0.1) return true;     // hack alert!
            if(spacelo>0.1f) d->o.z = hi-d->aboveeye; // glue to ceiling if in midair
            d->vel.z = 0;                     // cancel out jumping velocity
        }

        const float floorclamp = d->crouching ? 0.1f : 0.01f;
        d->onfloor = d->o.z-eyeheight-lo < floorclamp;
    }
    return false;
}

VARP(maxroll, 0, 0, 20); // note: when changing max value, fix network transmission
//VAR(recoilbackfade, 0, 100, 1000);

void resizephysent(physent *pl, int moveres, int curtime, float min, float max)
{
    if(pl->eyeheightvel==0.0f) return;

    const bool water = hdr.waterlevel>pl->o.z;
    const float speed = curtime*pl->maxspeed/(water ? 2000.0f : 1000.0f);
    float h = pl->eyeheightvel * speed / moveres;

    loopi(moveres)
    {
        pl->eyeheight += h;
        pl->o.z += h;
        if(collide(pl))
        {
            pl->eyeheight -= h; // collided, revert mini-step
            pl->o.z -= h;
            break;
        }
        if(pl->eyeheight<min) // clamp to min
        {
            pl->o.z += min - pl->eyeheight;
            pl->eyeheight = min;
            pl->eyeheightvel = 0.0f;
            break;
        }
        if(pl->eyeheight>max)
        {
            pl->o.z -= pl->eyeheight - max;
            pl->eyeheight = max;
            pl->eyeheightvel = 0.0f;
            break;
        }
    }
}

// main physics routine, moves a player/monster for a curtime step
// moveres indicated the physics precision (which is lower for monsters and multiplayer prediction)
// local is false for multiplayer prediction

void clamproll(physent *pl)
{
    extern int maxrollremote;
    int mroll = pl == player1 ? maxroll : maxrollremote;
    if(pl->roll > mroll) pl->roll = mroll;
    else if(pl->roll < -mroll) pl->roll = -mroll;
}

float var_f = 0;
int var_i = 0;
bool var_b = true;

FVARP(flyspeed, 1.0, 2.0, 5.0);

void moveplayer(physent *pl, int moveres, bool local, int curtime)
{
    bool water = false;
    const bool editfly = pl->state==CS_EDITING;
    const bool specfly = pl->type==ENT_PLAYER && ((playerent *)pl)->spectatemode==SM_FLY;
    const bool isfly = editfly || specfly;

    vec d;      // vector of direction we ideally want to move in

    float drop = 0, rise = 0;

    if(pl->type==ENT_BOUNCE)
    {
        bounceent* bounce = (bounceent *) pl;
        water = hdr.waterlevel>pl->o.z;

        const float speed = curtime*pl->maxspeed/(water ? 2000.0f : 1000.0f);
        const float friction = water ? 20.0f : (pl->onfloor || isfly ? 6.0f : 30.0f);
        const float fpsfric = max(friction*20.0f/curtime, 1.0f);

        if(pl->onfloor) // apply friction
        {
            pl->vel.mul(fpsfric-1);
        pl->vel.div(fpsfric);
        }
        else // apply gravity
        {
            const float CUBES_PER_METER = 4; // assumes 4 cubes make up 1 meter
            const float BOUNCE_MASS = 0.5f; // sane default mass of 0.5 kg
            const float GRAVITY = BOUNCE_MASS*9.81f/CUBES_PER_METER/1000.0f;
            bounce->vel.z -= GRAVITY*curtime;
        }

        d = bounce->vel;
        d.mul(speed);
        if(water) d.div(6.0f); // incorrect

        // rotate
        float rotspeed = bounce->rotspeed*d.magnitude();
        pl->pitch = fmod(pl->pitch+rotspeed, 360.0f);
        pl->yaw = fmod(pl->yaw+rotspeed, 360.0f);
    }
    else // fake physics for player ents to create _the_ cube movement (tm)
    {
        const int timeinair = pl->timeinair;
        int move = pl->onladder && !pl->onfloor && pl->move == -1 ? 0 : pl->move; // movement on ladder
        water = hdr.waterlevel>pl->o.z-0.5f;

        float chspeed = 0.4f;
        if(!(pl->onfloor || pl->onladder)) chspeed = 1.0f;

        const bool crouching = pl->crouching || pl->eyeheight < pl->maxeyeheight;
        const float speed = curtime/(water ? 2000.0f : 1000.0f)*pl->maxspeed*(crouching && pl->state != CS_EDITING ? chspeed : 1.0f)*(pl==player1 && isfly ? flyspeed : 1.0f);
        const float friction = water ? 20.0f : (pl->onfloor || isfly ? 6.0f : (pl->onladder ? 1.5f : 30.0f));
        const float fpsfric = max(friction/curtime*20.0f, 1.0f);

        d.x = (float)(move*cosf(RAD*(pl->yaw-90)));
        d.y = (float)(move*sinf(RAD*(pl->yaw-90)));
        d.z = 0.0f;

        if(isfly || water)
        {
            d.x *= (float)cosf(RAD*(pl->pitch));
            d.y *= (float)cosf(RAD*(pl->pitch));
            d.z = (float)(move*sinf(RAD*(pl->pitch)));
        }

        d.x += (float)(pl->strafe*cosf(RAD*(pl->yaw-180)));
        d.y += (float)(pl->strafe*sinf(RAD*(pl->yaw-180)));

        pl->vel.mul(fpsfric-1.0f);   // slowly apply friction and direction to velocity, gives a smooth movement
        pl->vel.add(d);
        pl->vel.div(fpsfric);
        d = pl->vel;
        d.mul(speed);

        if(editfly)                // just apply velocity
        {
            pl->o.add(d);
            if(pl->jumpnext && !pl->trycrouch)
            {
                pl->jumpnext = true; // fly directly upwards while holding jump keybinds
                pl->vel.z = 0.5f;
            }
            else if (pl->trycrouch && !pl->jumpnext)
            {
                pl->vel.z = -0.5f; // fly directly down while holding crouch keybinds
            }
        }
        else if(specfly)
        {
            rise = speed/moveres/1.2f;
            if(pl->jumpnext)
            {
                pl->jumpnext = false;
                pl->vel.z = 2;
            }
        }
        else                        // apply velocity with collisions
        {
            if(pl->type!=ENT_CAMERA)
            {
                if(pl->onladder)
                {
                    const float climbspeed = 1.0f;

                    if(pl->type==ENT_BOT && pl->state == CS_ALIVE) pl->vel.z = climbspeed; // bots climb upwards only
                    else if(pl->type==ENT_PLAYER)
                    {
                        if(((playerent *)pl)->k_up) pl->vel.z = climbspeed;
                        else if(((playerent *)pl)->k_down) pl->vel.z = -climbspeed;
                    }
                    pl->timeinair = 0;
                }
                else
                {
                    if(pl->onfloor || water)
                    {
                        if(pl->jumpnext)
                        {
                            pl->jumpnext = false;
                            bool doublejump = pl->lastjump && lastmillis-pl->lastjump < 250 && pl->strafe != 0 && pl->lastjumpheight != 0 && pl->lastjumpheight != pl->o.z;
                            pl->lastjumpheight = pl->o.z;
                            pl->vel.z = 2.0f; // physics impulse upwards
                            if(doublejump) // more velocity on double jump
                            {
                                pl->vel.mul(1.25f);
                            }
                            if(water) // dampen velocity change even harder, gives correct water feel
                            {
                                pl->vel.x /= 8.0f;
                                pl->vel.y /= 8.0f;
                            }
                            else if(pl==player1 || pl->type!=ENT_PLAYER) audiomgr.playsoundc(S_JUMP, pl);
                            pl->lastjump = lastmillis;
                        }
                        pl->timeinair = 0;
                        pl->crouchedinair = false;
                    }
                    else
                    {
                        pl->timeinair += curtime;
                        if (pl->trycrouch && !pl->crouching && !pl->crouchedinair && pl->state!=CS_EDITING) {
                            pl->vel.z += 0.3f;
                            pl->crouchedinair = true;
                        }
                    }
                }

                if(timeinair > 200 && !pl->timeinair)
                {
                    int sound = timeinair > 800 ? S_HARDLAND : S_SOFTLAND;
                    if(pl->state!=CS_DEAD)
                    {
                        if(pl==player1 || pl->type!=ENT_PLAYER) audiomgr.playsoundc(sound, pl);
                    }
                }
            }

            const float gravity = 20.0f;
            float dropf = (gravity-1)+pl->timeinair/15.0f;         // incorrect, but works fine
            if(water) { dropf = 5; pl->timeinair = 0; }            // float slowly down in water
            if(pl->onladder) { dropf = 0; pl->timeinair = 0; }

            drop = dropf*curtime/gravity/100/moveres;              // at high fps, gravity kicks in too fast
            rise = speed/moveres/1.2f;                             // extra smoothness when lifting up stairs
            if(pl->maxspeed-16.0f>0.5f) pl += 0xF0F0;
        }
    }

    bool collided = false;
    vec oldorigin = pl->o;

    if(!editfly) loopi(moveres)                                // discrete steps collision detection & sliding
    {
        const float f = 1.0f/moveres;

        // try move forward
        pl->o.x += f*d.x;
        pl->o.y += f*d.y;
        pl->o.z += f*d.z;
        hitplayer = NULL;
        if(!collide(pl, false, drop, rise)) continue;
        else collided = true;
        if(pl->type==ENT_BOUNCE && cornersurface)
        { // try corner bounce
            float ct2f = cornersurface == 2 ? -1.0 : 1.0;
            vec oo = pl->o, xd = d;
            xd.x = d.y * ct2f;
            xd.y = d.x * ct2f;
            pl->o.x += f * (-d.x + xd.x);
            pl->o.y += f * (-d.y + xd.y);
            if(!collide(pl, false, drop, rise))
            {
                d = xd;
                float sw = pl->vel.x * ct2f;
                pl->vel.x = pl->vel.y * ct2f;
                pl->vel.y = sw;
                pl->vel.mul(0.7f);
                continue;
            }
            pl->o = oo;
        }
        if(pl->type==ENT_CAMERA || (pl->type==ENT_PLAYER && pl->state==CS_DEAD && ((playerent *)pl)->spectatemode != SM_FLY))
        {
            pl->o.x -= f*d.x;
            pl->o.y -= f*d.y;
            pl->o.z -= f*d.z;
            break;
        }
        if(pl->type!=ENT_BOUNCE && hitplayer)
        {
            vec dr(hitplayer->o.x-pl->o.x,hitplayer->o.y-pl->o.y,0);
            float invdist = ufInvSqrt(dr.sqrxy()),
                  push = (invdist < 10.0f ? dr.dotxy(d)*1.1f*invdist : dr.dotxy(d) * 11.0f);

            pl->o.x -= f*d.x*push;
            pl->o.y -= f*d.y*push;
            if(i==0 && pl->type==ENT_BOT) pl->yaw += (dr.cxy(d)>0 ? 2:-2); // force the bots to change direction
            if( !collide(pl, false, drop, rise) ) continue;
            pl->o.x += f*d.x*push;
            pl->o.y += f*d.y*push;
        }
        if (cornersurface)
        {
            float ct2f = (cornersurface == 2 ? -1.0 : 1.0);
            float diag = f*d.magnitudexy()*2;
            vec vd = vec((d.y*ct2f+d.x >= 0.0f ? diag : -diag), (d.x*ct2f+d.y >= 0.0f ? diag : -diag), 0);
            pl->o.x -= f*d.x;
            pl->o.y -= f*d.y;

            pl->o.x += vd.x;
            pl->o.y += vd.y;
            if(!collide(pl, false, drop, rise))
            {
                d.x = vd.x; d.y = vd.y;
                continue;
            }
            pl->o.x -= vd.x;
            pl->o.y -= vd.y;
        }
        else
        {
#define WALKALONGAXIS(x,y) \
            pl->o.x -= f*d.x; \
            if(!collide(pl, false, drop, rise)) \
            { \
                d.x = 0; \
                if(pl->type==ENT_BOUNCE) { pl->vel.x = -pl->vel.x; pl->vel.mul(0.7f); } \
                continue; \
            } \
            pl->o.x += f*d.x;
            // player stuck, try slide along y axis
            WALKALONGAXIS(x,y);
            // still stuck, try x axis
            WALKALONGAXIS(y,x);
        }
//         try just dropping down
        pl->o.x -= f*d.x;
        pl->o.y -= f*d.y;
        if(!collide(pl, false, drop, rise))
        {
            d.y = d.x = 0;
            continue;
        }
        pl->o.z -= f*d.z;
        if(pl->type==ENT_BOUNCE) { pl->vel.z = -pl->vel.z; pl->vel.mul(0.5f); }
        break;
    }

    pl->stuck = (oldorigin==pl->o);
    if(collided) pl->oncollision();
    else pl->onmoved(oldorigin.sub(pl->o));

    if(pl->type==ENT_CAMERA) return;

    if(pl->type!=ENT_BOUNCE && pl==player1)
    {
        // automatically apply smooth roll when strafing
        if(pl->strafe==0)
        {
            pl->roll = pl->roll/(1+(float)sqrt((float)curtime)/25);
        }
        else
        {
            pl->roll += pl->strafe*curtime/-30.0f;
            clamproll(pl);
        }

        // smooth pitch
        const float fric = 6.0f/curtime*20.0f;
        pl->pitch += pl->pitchvel*(curtime/1000.0f)*pl->maxspeed*(pl->crouching ? 0.75f : 1.0f);
        pl->pitchvel *= fric-3;
        pl->pitchvel /= fric;
        /*extern int recoiltest;
        if(recoiltest)
        {
            if(pl->pitchvel < 0.05f && pl->pitchvel > 0.001f) pl->pitchvel -= recoilbackfade/100.0f; // slide back
        }
        else*/ if(pl->pitchvel < 0.05f && pl->pitchvel > 0.001f) pl->pitchvel -= ((playerent *)pl)->weaponsel->info.recoilbackfade/100.0f; // slide back
        if(pl->pitchvel) fixcamerarange(pl); // fix pitch if necessary
    }

    // play sounds on water transitions
    if(pl->type!=ENT_CAMERA)
    {
        if(!pl->inwater && water)
        {
            if(!pl->lastsplash || lastmillis-pl->lastsplash>500)
            {
                audiomgr.playsound(S_SPLASH2, pl);
                pl->lastsplash = lastmillis;
            }
            if(pl==player1) pl->vel.z = 0;
        }
        else if(pl->inwater && !water) audiomgr.playsound(S_SPLASH1, &pl->o);
        pl->inwater = water;
    }

    // store previous locations of all players/bots
    if(pl->type==ENT_PLAYER || pl->type==ENT_BOT)
    {
        ((playerent *)pl)->history.update(pl->o, lastmillis);
    }

    // apply volume-resize when crouching
    if(pl->type==ENT_PLAYER || pl->type==ENT_BOT)
    {
//         if(pl==player1 && !(intermission || player1->onladder || (pl->trycrouch && !player1->onfloor && player1->timeinair > 50))) updatecrouch(player1, player1->trycrouch);
        if(!intermission && (pl == player1 || pl->type == ENT_BOT)) updatecrouch((playerent *)pl, pl->trycrouch);
        const float croucheyeheight = pl->maxeyeheight*3.0f/4.0f;
        resizephysent(pl, moveres, curtime, croucheyeheight, pl->maxeyeheight);
    }
}

const int PHYSFPS = 200;
const int PHYSFRAMETIME = 1000 / PHYSFPS;
int physsteps = 0, physframetime = PHYSFRAMETIME, lastphysframe = 0;

void physicsframe()          // optimally schedule physics frames inside the graphics frames
{
    int diff = lastmillis - lastphysframe;
    if(diff <= 0) physsteps = 0;
    else
    {
        extern int gamespeed;
        physframetime = clamp((PHYSFRAMETIME*gamespeed)/100, 1, PHYSFRAMETIME);
        physsteps = (diff + physframetime - 1)/physframetime;
        lastphysframe += physsteps * physframetime;
    }
}

VAR(physinterp, 0, 1, 1);

void interppos(physent *pl)
{
    pl->o = pl->newpos;
    pl->o.z += pl->eyeheight;

    int diff = lastphysframe - lastmillis;
    if(diff <= 0 || !physinterp) return;

    vec deltapos(pl->deltapos);
    deltapos.mul(min(diff, physframetime)/float(physframetime));
    pl->o.add(deltapos);
}

void moveplayer(physent *pl, int moveres, bool local)
{
    if(physsteps <= 0)
    {
        if(local) interppos(pl);
        return;
    }

    if(local)
    {
        pl->o = pl->newpos;
        pl->o.z += pl->eyeheight;
    }
    loopi(physsteps-1) moveplayer(pl, moveres, local, physframetime);
    if(local) pl->deltapos = pl->o;
    moveplayer(pl, moveres, local, physframetime);
    if(local)
    {
        pl->newpos = pl->o;
        pl->deltapos.sub(pl->newpos);
        pl->newpos.z -= pl->eyeheight;
        interppos(pl);
    }
}

void movebounceent(bounceent *p, int moveres, bool local)
{
    moveplayer(p, moveres, local);
}

// movement input code

#define dir(name,v,d,s,os) void name(bool isdown) { player1->s = isdown; player1->v = isdown ? d : (player1->os ? -(d) : 0); player1->lastmove = lastmillis; }

dir(backward, move,   -1, k_down,  k_up)
dir(forward,  move,    1, k_up,    k_down)
dir(left,     strafe,  1, k_left,  k_right)
dir(right,    strafe, -1, k_right, k_left)

void attack(bool on)
{
    if(intermission) return;
    if(editmode) editdrag(on);
    else if(player1->state==CS_DEAD || player1->state==CS_SPECTATE)
    {
        if(!on) tryrespawn();
    }
    else player1->attacking = on;
}

void jumpn(bool on)
{
    if(intermission) return;
    if(player1->isspectating())
    {
        if(lastmillis - player1->respawnoffset > 1000 && on) togglespect();
    }
    else if(player1->crouching) return;
    else player1->jumpnext = on;
}

void updatecrouch(playerent *p, bool on)
{
    if(p->crouching == on) return;
    if(p->state == CS_EDITING) return; // don't apply regular crouch physics in editfly
    const float crouchspeed = 0.6f;
    p->crouching = on;
    p->eyeheightvel = on ? -crouchspeed : crouchspeed;
    if(p==player1) audiomgr.playsoundc(on ? S_CROUCH : S_UNCROUCH);
}

void crouch(bool on)
{
    if(player1->isspectating()) return;
    player1->trycrouch = on;
}

int inWater(int *type)
{
    if(hdr.waterlevel > (*type ? player1->o.z : (player1->o.z - player1->eyeheight))) return 1;
    else return 0;
}

COMMAND(backward, "d");
COMMAND(forward, "d");
COMMAND(left, "d");
COMMAND(right, "d");
COMMANDN(jump, jumpn, "d");
COMMAND(attack, "d");
COMMAND(crouch, "d");
COMMAND(inWater, "i");

void fixcamerarange(physent *cam)
{
    const float MAXPITCH = 90.0f;
    if(cam->pitch>MAXPITCH) cam->pitch = MAXPITCH;
    if(cam->pitch<-MAXPITCH) cam->pitch = -MAXPITCH;
    while(cam->yaw<0.0f) cam->yaw += 360.0f;
    while(cam->yaw>=360.0f) cam->yaw -= 360.0f;
}

FVARP(sensitivity, 1e-3f, 3.0f, 1000.0f);
FVARP(scopesensscale, 1e-3f, 0.5f, 1000.0f);
FVARP(sensitivityscale, 1e-3f, 1, 1000);
FVARP(scopesens, 0, 0, 1000);
VARP(scopesensfeel, 0, 0, 1);
VARP(invmouse, 0, 0, 1);
FVARP(mouseaccel, 0, 0, 1000);
FVARP(mfilter, 0.0f, 0.0f, 6.0f);
VARP(autoscopesens, 0, 0, 1);

float testsens=0;
bool senst=0;
int tsens(int x)
{
    static bool highlock=0,lowlock=0;
    static bool hightry=0,lowtry=0;
    static float sensn=0,sensl=0,sensh=0;
    static int nstep=1;
    if (x==-2000) {  // RENDERING PART!!!
        if(senst) {
        draw_textf(
        "\fJSensitivity Training (hotkeys):\n\fE1. try High Sens. %s\n2. try Low Sens. %s\n\fJ%s :"
        "\fE\n3. choose: High Sens.\n4. choose: Low Sens.\n\fIrepeat the steps above until the training stops.\n\f35. Stop Training.",
        VIRTW/4  , VIRTH/3,
        hightry?"(TRIED)":"" , lowtry?"(TRIED)":"",
        hightry&&lowtry?"after trying both choose the one you liked most":"now you can choose the sensitivity you preferred");
        glPushMatrix(); glScalef(2,2,2);
        draw_textf("step: \f0%d",VIRTW/2  , VIRTH/3*2,nstep);
        glPopMatrix();
        }
        return 0;
    }
    if (x == SDLK_3 || x == SDLK_4)
    {
        if(!hightry || !lowtry) {
            if(!hightry && !lowtry) {
                conoutf("--- \f3ERROR:\f0before choosing a sensitivity, first try both higher and lower sens.");
            } else {
                conoutf("--- \f3ERROR:\f0try the %s%ser sensitivity too.",hightry?"":"high",lowtry?"":"low");
            }
        } else {
            if (x == SDLK_3) //high sens
            {
                lowlock=1;
                if (highlock)
                {
                    sensl=sensn;
                    sensn=(sensh+sensl)/2.0f;
                }
                else
                {
                    sensl=sensn;
                    sensn=sensh;
                    sensh=sensn*2.0f;
                }
            }
            if (x == SDLK_4) //low sens
            {
                highlock=1;
                if (lowlock)
                {
                    sensh=sensn;
                    sensn=(sensh+sensl)/2.0f;
                }
                else
                {
                    sensh=sensn;
                    sensn=sensl;
                    sensl=sensn/2.0f;
                }
            }
            if(sensh/sensn > 1.04f) {
            conoutf("--- \f0you chose the %ser sensitivity.",x==SDLK_3?"higher":"lower");
            conoutf("--- \f0repeat previous steps by trying both higher and lower sens and then by choosing the one you like most.");
            hudoutf("next step!");
            nstep++;
            }
            testsens=sensn;
            hightry=lowtry=0;
        }

    }
    if (x == SDLK_2)
    {
        testsens=sensl;
        conoutf("--- \f0You are now %strying the lower sensitivity.",lowtry?"re-":""); lowtry=1;
    }
    if (x == SDLK_1)
    {
        testsens=sensh;
        conoutf("--- \f0You are now %strying the higher sensitivity.",hightry?"re-":""); hightry=1;
    }
    if (x==-1000)
    {
        float factor=rnd(800)+600;
        factor/=1000;
        sensh=sensitivity*factor*2.0f;
        sensl=sensitivity*factor/2.0f;
        sensn=sensitivity*factor;
    }
    if (sensh/sensn <= 1.04f || x == SDLK_5)
    {
        senst=0;
        if(sensh/sensn <= 1.04f) {
            conoutf("--- \f0Sensitivity Training Ended. happy fragging.");
            sensitivity=sensn;
        } else {
            conoutf("--- \f0Sensitivity Training Stopped.");
        }
        hightry=lowtry=highlock=lowlock=0;
        sensn=sensl=sensh=0;
        testsens=0;
        nstep=1;
    }
    return 0;
}

void findsens()
{
    if(!watchingdemo) {
        senst=1;
        tsens(-1000);
        testsens=sensitivity;
        conoutf("--- \f0Sensitivity Training Started.");
        return;
    }
}
COMMAND(findsens, "");

inline bool zooming(playerent *plx) { return (plx->weaponsel->type == GUN_SNIPER && ((sniperrifle *)plx->weaponsel)->scoped); }

void mousemove(int odx, int ody)
{
    static float fdx = 0, fdy = 0;
    if(intermission || (player1->isspectating() && player1->spectatemode==SM_FOLLOW1ST)) return;
    float dx = odx, dy = ody;
    if(mfilter > 0.0f)
    {
        float k = mfilter * 0.1f;
        dx = fdx = dx * ( 1.0f - k ) + fdx * k;
        dy = fdy = dy * ( 1.0f - k ) + fdy * k;
    }
    extern float scopesensfunc;
    float cursens = sensitivity;
    if(senst) {cursens=testsens;}
    if(mouseaccel && curtime && (dx || dy)) cursens += 0.02f * mouseaccel * sqrtf(dx*dx + dy*dy)/curtime;
    if(scopesens==0 || !zooming(player1))
    {
        if(scopesensfeel)
        {
            // AC 1.1
            cursens /= 33.0f*sensitivityscale;
            if( zooming(player1) ) { cursens *= autoscopesens ? scopesensfunc : scopesensscale; }
            camera1->yaw += dx*cursens;
            camera1->pitch -= dy*cursens*(invmouse ? -1 : 1);
        }
        else
        {
            // AC 1.0
            if( zooming(player1) ) { cursens *= autoscopesens ? scopesensfunc : scopesensscale; }
            float sensfactor = 33.0f*sensitivityscale;
            camera1->yaw += dx*cursens/sensfactor;
            camera1->pitch -= dy*cursens*(invmouse ? -1 : 1)/sensfactor;
        }
    }
    else
    {
        // user provided value
        float sensfactor = 33.0f*sensitivityscale;
        camera1->yaw += dx*scopesens/sensfactor;
        camera1->pitch -= dy*scopesens*(invmouse ? -1 : 1)/sensfactor;
    }

    fixcamerarange();
    if(camera1!=player1 && player1->spectatemode!=SM_DEATHCAM)
    {
        player1->yaw = camera1->yaw;
        player1->pitch = camera1->pitch;
    }
}

void entinmap(physent *d)    // brute force but effective way to find a free spawn spot in the map
{
    vec orig(d->o);
    loopi(100)              // try max 100 times
    {
        float dx = (rnd(21)-10)/10.0f*i;  // increasing distance
        float dy = (rnd(21)-10)/10.0f*i;
        d->o.x += dx;
        d->o.y += dy;
        if(!collide(d, true))
        {
            d->resetinterp();
            return;
        }
        d->o = orig;
    }
    // leave ent at original pos, possibly stuck
    d->resetinterp();
    conoutf(_("can't find entity spawn spot! (%d, %d)"), d->o.x, d->o.y);
}

void vecfromyawpitch(float yaw, float pitch, int move, int strafe, vec &m)
{
    if(move)
    {
        m.x = move*-sinf(RAD*yaw);
        m.y = move*cosf(RAD*yaw);
    }
    else m.x = m.y = 0;

    if(pitch)
    {
        m.x *= cosf(RAD*pitch);
        m.y *= cosf(RAD*pitch);
        m.z = move*sinf(RAD*pitch);
    }
    else m.z = 0;

    if(strafe)
    {
        m.x += strafe*cosf(RAD*yaw);
        m.y += strafe*sinf(RAD*yaw);
    }
}

void vectoyawpitch(const vec &v, float &yaw, float &pitch)
{
    yaw = -atan2(v.x, v.y)/RAD;
    pitch = asin(v.z/v.magnitude())/RAD;
}
