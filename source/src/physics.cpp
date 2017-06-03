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

int cornertest(int x, int y, int &bx, int &by, int &bs, sqr *&s, sqr *&h)    // iteratively collide with a mipmapped corner cube
{
    int mip = 1, res = -1;
    while(SWS(wmip[mip], x>>mip, y>>mip, sfactor-mip)->type==CORNER) mip++;
    mip--;
    x >>= mip;
    y >>= mip;
    int mfactor = sfactor - mip;
    bx = x<<mip;                     // bx, by and bs are the real-world coordinates and size of the corner mip
    by = y<<mip;                     // s is the corner mip and h is the companion mip of a corner between non-solids (to get floor and ceil from)
    bs = 1<<mip;
    sqr *z = SWS(wmip[mip],x - 1, y,mfactor);
    sqr *t = SWS(z,2,0,mfactor);     //   w
    sqr *w = SWS(z,1,-1,mfactor);    //  zst
    sqr *v = SWS(z,1,1,mfactor);     //   v
    s = SWS(z,1,0,mfactor);

    // now, this is _exactly_ how the renderer interprets map geometry...
    if(SOLID(z))
    {
        if(SOLID(w)) res = 2; // corners between solids are solid behind the wall or SPACE in front of it
        else if(SOLID(v)) res = 3;
    }
    else if(SOLID(t))
    {
        if(SOLID(w)) res = 1;
        else if(SOLID(v)) res = 0;
    }
    else
    { // not a corner between solids
        bool wv = w->ceil-w->floor < v->ceil-v->floor;
        h = wv ? v : w;  // in front of the corner, use floor and ceil from h
        if(z->ceil-z->floor < t->ceil-t->floor) res = wv ? 2 : 3;
        else res = wv ? 1 : 0;
    }            //  03
    return res;  //  12
}

static int cornersurface = 0;

bool mmcollide(physent *d, float &hi, float &lo)           // collide with a mapmodel
{
    const float eyeheight = d->eyeheight, SQRT2HALF = SQRT2 / 2.0f;
    const float playerheight = eyeheight + d->aboveeye;
    if(editmode) clentstats.firstclip = 0;
    for(int i = clentstats.firstclip; i < ents.length(); i++)
    {
        entity &e = ents[i];
        // if(e.type==CLIP || (e.type == PLCLIP && d->type == ENT_PLAYER))
        if (e.type==CLIP || (e.type == PLCLIP && (d->type == ENT_BOT || d->type == ENT_PLAYER || (d->type == ENT_BOUNCE && ((bounceent *)d)->plclipped)))) // don't allow bots to hack themselves into plclips - Bukz 2011/04/14
        {
            bool hitarea = false;
            switch(e.attr7 & 3)
            {
                default: // classic unrotated clip, possibly tilted
                    hitarea = fabs(e.x - d->o.x) < float(e.attr2) / ENTSCALE5 + d->radius && fabs(e.y - d->o.y) < float(e.attr3) / ENTSCALE5 + d->radius;
                    break;
                case 3: // clip rotated 45°
                {
                    float rx = (e.x - d->o.x) * SQRT2HALF, ry = (e.y - d->o.y) * SQRT2HALF, rr = d->radius * SQRT2; // rotate player instead of clip (adjust player radius to compensate)
                    float a1 = fabs(rx - ry) - float(e.attr3) / ENTSCALE5 - rr, a2 = fabs(rx + ry) - float(e.attr2) / ENTSCALE5 - rr;
                    if(a1 < 0 && a2 < 0)
                    {
                        float a3 =  a1 + a2 + rr;
                        if(a3 < 0) hitarea = true;
                        if(a3 < -1e-2 && (a3 < -rr || a1 * a2 < 0.42f)) cornersurface = a1 > a2 ? 1 : 2;
                    }
                    break;
                }
            }
            if(hitarea)
            {
                float cz = float(S(e.x, e.y)->floor + float(e.attr1) / ENTSCALE10), ch = float(e.attr4) / ENTSCALE5;
                if(e.attr6) switch(e.attr7 & 3)
                { // incredibly ugly solution - but it only applies on the one tilted clip we stand on
                    case 1: cz += (floor(0.5f + clamp(d->o.x - e.x + d->radius * (e.attr6 > 0 ? 1 : -1), -float(e.attr2) / ENTSCALE5, float(e.attr2) / ENTSCALE5))) * float(e.attr6) / (4 * ENTSCALE10); break; // tilt x
                    case 2: cz += (floor(0.5f + clamp(d->o.y - e.y + d->radius * (e.attr6 > 0 ? 1 : -1), -float(e.attr3) / ENTSCALE5, float(e.attr3) / ENTSCALE5))) * float(e.attr6) / (4 * ENTSCALE10); break; // tilt y
                }
                const float dz = d->o.z - d->eyeheight;
                if(dz < cz - 0.42) { if(cz<hi) hi = cz; }
                else if(cz+ch>lo) lo = cz+ch;
                if(hi-lo < playerheight) return true;
                if(dz + (d->type != ENT_BOUNCE ? 1.26 : 0) > cz + ch || dz + playerheight < cz) cornersurface = 0;
            }
        }
        else if(e.type==MAPMODEL)
        {
            mapmodelinfo *mmi = getmminfo(e.attr2);
            if(!mmi || !mmi->h) continue;
            const float r = mmi->rad + d->radius;
            if(fabs(e.x-d->o.x)<r && fabs(e.y-d->o.y)<r)
            {
                const float mmz = float(S(e.x, e.y)->floor + mmi->zoff + float(e.attr3) / ENTSCALE5);
                const float dz = d->o.z-eyeheight;
                if(dz < mmz - 0.42) { if(mmz < hi) hi = mmz; }
                else if(mmz + mmi->h > lo) lo = mmz + mmi->h;
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

bool collide(physent *d, bool spawn, float drop, float rise)
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
    float z1 = d->o.z-eyeheight, z2 = z1 + playerheight;
    if(d->type != ENT_BOUNCE) z1 += 1.26;
    const int applyclip = d->type == ENT_BOT || d->type == ENT_PLAYER || (d->type == ENT_BOUNCE && ((bounceent *)d)->plclipped) ? TAGANYCLIP : TAGCLIP;

    for(int y = y1; y<=y2; y++) for(int x = x1; x<=x2; x++)     // collide with map
    {
        if(OUTBORD(x,y)) return true;
        sqr *s = S(x,y);
        bool tagclipped = (s->tag & applyclip) != 0;
        float ceil = s->ceil;
        float floor = s->floor;
        switch(s->type)
        {
            case SOLID:
                return true;

            case CORNER:
            {
                sqr *ns, *h = NULL;
                int bx, by, bs;
                int q = cornertest(x, y, bx, by, bs, ns, h);
                bool matter = false, match = false;
                switch(q)                          //  0XX3
                {                                  //  XXXX
                    case 0:                        //  1XX2
                        match = x==x2 && y==y2;
                        matter = fx2-bx+fy2-by>=bs;
                        break;
                    case 1:
                        match = x==x2 && y==y1;
                        matter = fx2-bx>=fy1-by;
                        break;
                    case 2:
                        match = x==x1 && y==y1;
                        matter = fx1-bx+fy1-by<=bs;
                        break;
                    case 3:
                        match = x==x1 && y==y2;
                        matter = fx1-bx<=fy2-by;
                        break;
                    default:
                        return true; // mapper's fault: corner with unsufficient solids: renderer can't handle those anyway: treat as solid
                }
                cornersurface = (q & 1) ? 5 : 6;
                sqr *n = h && !matter ? h : ns;
                ceil = n->ceil;  // use floor & ceil from higher mips (like the renderer)
                floor = n->floor;
                if(!match && d->type == ENT_BOUNCE) match = q == (d->vel.x < 0) * 2 + (d->vel.y * d->vel.x < 0); // when coming towards corner surface: prefer corner bounce
                if(match && matter)
                {
                    if(!h) return true; // we hit a corner between solids...
                    else if(z1 < ns->floor || z2 > ns->ceil || tagclipped) return true; // corner is not between solids, but we hit it...
                }
                if(h) tagclipped = false; // for corners between non-solids, tagclips extend the corner (visualisation does not reflect that, yet)
                                          // tagclipped corners between solids are fully clipped
                cornersurface = 0;
                break;
            }

            case FHF:       // FIXME: too simplistic collision with slopes, makes it feels like tiny stairs
                floor -= (s->vdelta+S(x+1,y)->vdelta+S(x,y+1)->vdelta+S(x+1,y+1)->vdelta)/16.0f;
                break;

            case CHF:
                ceil += (s->vdelta+S(x+1,y)->vdelta+S(x,y+1)->vdelta+S(x+1,y+1)->vdelta)/16.0f;

        }
        if(tagclipped) return true; // tagged clips feel like solids
        if(ceil<hi) hi = ceil;
        if(floor>lo) lo = floor;
    }

    if(hi - lo < playerheight) return true;

    float headspace = 10.0f;

    if(d->type!=ENT_CAMERA)
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
    if(mmcollide(d, hi, lo)) return true;    // collide with map models

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

        const float floorclamp = d->crouching && (!d->lastjump || lastmillis - d->lastjump > 250) ? 0.1f : 0.01f;
        d->onfloor = d->o.z-eyeheight-lo < floorclamp;
    }
    return false;
}

VARFP(maxroll, 0, ROLLMOVDEF, ROLLMOVMAX, player1->maxroll = maxroll);
VARFP(maxrolleffect, 0, ROLLEFFDEF, ROLLEFFMAX, player1->maxrolleffect = maxrolleffect);
VARP(maxrollremote, 0, ROLLMOVDEF + ROLLEFFDEF, ROLLMOVMAX + ROLLEFFMAX);

void resizephysent(physent *pl, int moveres, int curtime, float min, float max)
{
    if(pl->eyeheightvel==0.0f) return;

    const bool water = waterlevel > pl->o.z;
    const float speed = curtime*pl->maxspeed/(water ? 2000.0f : 1000.0f);
    float h = pl->eyeheightvel * speed / moveres;

    loopi(moveres)
    {
        pl->eyeheight += h;
        pl->o.z += h;
        if(collide(pl) && !(pl != player1 && player1->isspectating() && player1->spectatemode==SM_FOLLOW1ST)) // don't check collision during spectating in 1st person view
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
        water = waterlevel > pl->o.z;

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
        if(!editfly) water = waterlevel > pl->o.z - 0.5f;

        float chspeed = (pl->onfloor || pl->onladder || !pl->crouchedinair) ? 0.4f : 1.0f;

        const bool crouching = pl->crouching || (pl->eyeheight < pl->maxeyeheight && pl->eyeheight > 1.1f);
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

        float curfullspeed = d.magnitudexy();

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
                pl->vel.z = 0.5f; // fly directly upwards while holding jump keybinds
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
                    else if(pl->type==ENT_PLAYER && pl->state == CS_ALIVE)
                    {
                        if(((playerent *)pl)->k_up && pl->move > 0) pl->vel.z = climbspeed;
                        else if(((playerent *)pl)->k_down && pl->move < 0) pl->vel.z = -climbspeed;
                    }
                    pl->timeinair = 0;
                }
                else
                {
                    if(pl->onfloor || water)
                    {
                        if(pl->jumpnext)
                        {
                            pl->jumpd = true;
                            pl->jumpnext = false;
                            bool doublejump = pl->lastjump && lastmillis - pl->lastjump < 250 && pl->strafe != 0 && pl->o.z - pl->eyeheight - pl->lastjumpheight > 0.2f;
                            pl->lastjumpheight = pl->o.z - pl->eyeheight;
                            pl->vel.z = 2.0f; // physics impulse upwards
                            if(doublejump && curfullspeed > 0.1f) // more velocity on double jump
                            {
                                pl->vel.mul(1.25f / max(pl->vel.magnitudexy() / curfullspeed, 1.0f));
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

                if(timeinair > 200 && !pl->timeinair && pl->inwater == water)
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
        vec o_null = pl->o;
        pl->o.x += f*d.x;
        pl->o.y += f*d.y;
        pl->o.z += f*d.z;
        volatile vec gcco3(pl->o.x, pl->o.y, pl->o.z);  // force capping the o-values to float-representables (avoid player stuck conditions on 32-bit g++ builds)
        pl->o = vec(gcco3.x, gcco3.y, gcco3.z);
        hitplayer = NULL;
        if(!collide(pl, false, drop, rise)) continue;
        int cornersurface1 = cornersurface;
        vec o_trying = pl->o;  // o_trying = o_null + f * d  (= one microstep in desired direction)
        if(!cornersurface1)
        { // brute-force check, if it is a corner hit after all
            if(pl->type != ENT_BOUNCE)
            {
                int collx = 0, colly = 0;
                pl->o.x = o_null.x;
                if(collide(pl, false, drop, rise)) collx = cornersurface;
                pl->o = o_trying;
                pl->o.y = o_null.y;
                if(collide(pl, false, drop, rise)) colly = cornersurface;
                cornersurface1 = collx | colly;
                if((cornersurface1 & 3) == 3) cornersurface1 = 0;
            }
            else
            { // try really, really hard to detect corners for bounces (to compensate for the not-so-micro bounceent-microsteps)
                vec vd(d.x, d.y, 0);
                static const float a = 3.0f*PI2/10.0f, ca = cosf(a), sa = sinf(a); // 10 probe points, stretched over three turns
                loopj(10)
                {
                    if(j) vd = vec(ca * vd.x - sa * vd.y, ca * vd.y + sa *vd.x, 0);
                    pl->o = o_trying;
                    pl->o.add(vd);
                    if(collide(pl, false, drop, rise) && cornersurface) break;
                }
                cornersurface1 = cornersurface;
            }
            pl->o = o_trying;
        }
        collided = true;
        if(pl->type==ENT_BOUNCE && cornersurface1)
        { // try corner bounce
            float ct2f = cornersurface1 & 2 ? -1.0 : 1.0;
            vec xd = d;
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
            pl->o = o_trying;
        }
        if(pl->type==ENT_CAMERA || (pl->type==ENT_PLAYER && pl->state==CS_DEAD && ((playerent *)pl)->spectatemode != SM_FLY))
        {
            pl->o = o_null;
            break;
        }
        if(pl->type!=ENT_BOUNCE && hitplayer)
        {
            vec dr(hitplayer->o.x-pl->o.x,hitplayer->o.y-pl->o.y,0);
            float invdist = 1.0f / dr.magnitudexy(),
                  push = (invdist < 10.0f ? dr.dotxy(d)*1.1f*invdist : dr.dotxy(d) * 11.0f);

            pl->o.x -= f*d.x*push;
            pl->o.y -= f*d.y*push;
            if(i==0 && pl->type==ENT_BOT) pl->yaw += (dr.cxy(d)>0 ? 2:-2); // force the bots to change direction
            if( !collide(pl, false, drop, rise) ) continue;
            pl->o.x += f*d.x*push;
            pl->o.y += f*d.y*push;
        }

        // the desired direction didn't work
        pl->o.x = o_null.x;
        pl->o.y = o_null.y;
        vec o_nullxy = pl->o; // x and y from o_null but z from o_trying (possibly altered by collide() as well)

        // try sliding
        if(cornersurface1)
        {
            // along a corner wall
            float ct2f = cornersurface1 & 2 ? -1.0 : 1.0;
            float diag = fabs(d.x + ct2f * d.y) * 0.5f;
            vec vd = vec((d.y*ct2f+d.x >= 0.0f ? diag : -diag), (d.x*ct2f+d.y >= 0.0f ? diag : -diag), 0);
            float ff = f / (cornersurface1 & 4 ? 42.0f : 333.0f);
            pl->o.x += f*vd.x - ff*d.x;
            pl->o.y += f*vd.y - ff*d.y;
            if(!collide(pl, false, drop, rise))
            {
                d.x = vd.x; d.y = vd.y;
                continue;
            }
            pl->o = o_nullxy;
        }
        // try slide along y axis
        pl->o.y = o_trying.y;
        if(!collide(pl, false, drop, rise))
        {
            d.x = 0;
            if(pl->type==ENT_BOUNCE) { pl->vel.x = -pl->vel.x; pl->vel.mul(0.7f); }
            continue;
        }
        pl->o = o_nullxy;
        // try x axis
        pl->o.x = o_trying.x;
        if(!collide(pl, false, drop, rise))
        {
            d.y = 0;
            if(pl->type==ENT_BOUNCE) { pl->vel.y = -pl->vel.y; pl->vel.mul(0.7f); }
            continue;
        }
        pl->o = o_nullxy;
        // try just dropping down
        if(!collide(pl, false, drop, rise))
        {
            d.y = d.x = 0;
            continue;
        }
        pl->o = o_null;
        if(pl->type==ENT_BOUNCE) { pl->vel.z = -pl->vel.z; pl->vel.mul(0.5f); }
        break;
    }

    pl->stuck = (oldorigin==pl->o);
    if(collided) pl->oncollision();
    else pl->onmoved(oldorigin.sub(pl->o));

    if(pl->type==ENT_CAMERA) return;

    if(pl->type!=ENT_BOUNCE)
    {
        if(pl->type == ENT_PLAYER)
        {
            // automatically apply smooth roll when strafing
            playerent *p = (playerent *)pl;
            float iir = 1.0f + sqrtf((float)curtime) / 25.0f;
            if(pl->strafe==0)
            {
                p->movroll /= iir;
            }
            else
            {
                p->movroll = clamp(p->movroll + pl->strafe * curtime / -30.0f, -p->maxroll, p->maxroll);
            }
            p->effroll /= iir; // fade damage roll
            pl->roll = p->movroll + p->effroll;
            if(pl != player1) pl->roll = clamp(pl->roll, (float)-maxrollremote, (float)maxrollremote);
        }
        // smooth pitch
        const float fric = 6.0f/curtime*20.0f;
        pl->pitch += pl->pitchvel*(curtime/1000.0f)*pl->maxspeed*(pl->crouching ? 0.75f : 1.0f);
        pl->pitchvel *= fric-3;
        pl->pitchvel /= fric;
        if(pl->pitchvel < 0.05f && pl->pitchvel > 0.001f) pl->pitchvel -= ((playerent *)pl)->weaponsel->info.recoilbackfade/100.0f; // slide back
        if(pl->pitchvel) fixcamerarange(pl); // fix pitch if necessary
    }

    // play sounds on water transitions
    if(pl->type!=ENT_CAMERA)
    {
        if(!pl->inwater && water)
        {
            if(!pl->lastsplash || lastmillis-pl->lastsplash>500)
            {
                audiomgr.playsound(S_SPLASH2, &pl->o);
                pl->lastsplash = lastmillis;
            }
            if(pl==player1) pl->vel.z = 0;
        }
        else if(pl->inwater && !water)
        {
            audiomgr.playsound(S_SPLASH1, &pl->o);
            if(pl->type == ENT_BOUNCE) pl->maxspeed /= 8; // prevent nades from jumping out of water
        }
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
        if(!multiplayer(NULL) && physsteps > 1000) physsteps = 1000;
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
    static bool wason = false;
    player1->jumpnext = on && !wason && !player1->crouching && !intermission && !player1->isspectating();
    wason = on;
    if(player1->isspectating())
    {
        if(lastmillis - player1->lastdeath > 1000 && on) togglespect();
    }
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
    if(player1->isspectating() && on) return;
    player1->trycrouch = on;
}

COMMAND(backward, "d");
COMMAND(forward, "d");
COMMAND(left, "d");
COMMAND(right, "d");
COMMANDN(jump, jumpn, "d");
COMMAND(attack, "d");
COMMAND(crouch, "d");

void fixcamerarange(physent *cam)
{
    if(cam->pitch>MAXPITCH) cam->pitch = MAXPITCH;
    if(cam->pitch<-MAXPITCH) cam->pitch = -MAXPITCH;
    while(cam->yaw<0.0f) cam->yaw += 360.0f;
    while(cam->yaw>=360.0f) cam->yaw -= 360.0f;
}

FVARP(sensitivity, 1e-3f, 3.0f, 1000.0f);       // general mouse sensitivity ("unscoped")
FVARP(scopesens, 0, 0, 1000);                   // scoped mouse sensitivity (if zero, autoscopesens determines, how sensitivity is changed during scoping)
FVARP(sensitivityscale, 1e-3f, 1, 1000);        // scale all sensitivity values (if unsure, keep at default value "1"- this parameter achieves cosmetic value changes only)

VARP(autoscopesens, 0, 0, 1);                   // switches between scopesensscale and autoscopesensscale to calculate a scoped sensitivity (if scopesens is 0)
FVARP(scopesensscale, 1e-3f, 0.5f, 1000.0f);    // if scoped, sens = sensitivity * scopesensscale (roughly)
float autoscopesensscale = 0.4663077f;          // roughly scopefov/fov, recalculated if fov or scopefov changes (init value fits defaults fov 90 and scopefov 50, update, if defaults change)

VARP(invmouse, 0, 0, 1);                        // invert y-axis movement (if "1")
FVARP(mouseaccel, 0, 0, 1000);                  // make fast movement even faster (zero deactivates the feature)
FVARP(mfilter, 0.0f, 0.0f, 6.0f);               // simple lowpass filtering (zero deactivates the feature)

void mousemove(int idx, int idy)
{
    if(intermission || (player1->isspectating() && player1->spectatemode==SM_FOLLOW1ST)) return;
    bool zooming = player1->weaponsel->type == GUN_SNIPER && ((sniperrifle *)player1->weaponsel)->scoped;               // check if player uses scope
    float dx = idx, dy = idy;
    if(mfilter > 0.0001f)
    { // simple IIR-like filter (1st order lowpass)
        static float fdx = 0, fdy = 0;
        float k = mfilter * 0.1f;
        dx = fdx = dx * ( 1.0f - k ) + fdx * k;
        dy = fdy = dy * ( 1.0f - k ) + fdy * k;
    }
    double cursens = sensitivity;                                                                                       // basic unscoped sensitivity
    if(mouseaccel > 0.0001f && curtime && (idx || idy)) cursens += 0.02f * mouseaccel * sqrtf(dx*dx + dy*dy)/curtime;   // optionally accelerated
    if(zooming)
    {                                                                                                                   //      when scoped:
        if(scopesens > 0.0001f) cursens = scopesens;                                                                    //          if specified, use dedicated (fixed) scope sensitivity
        else cursens *= autoscopesens ? autoscopesensscale : scopesensscale;                                            //          or adjust sensitivity by given (fixed) factor or based on scopefov/fov
    }
    cursens /= 33.0f * sensitivityscale;                                                                                // final scaling

    camera1->yaw += (float) (dx * cursens);
    camera1->pitch -= (float) (dy * cursens * (invmouse ? -1 : 1));

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
    conoutf("can't find entity spawn spot! (%d, %d)", int(d->o.x), int(d->o.y));
}

