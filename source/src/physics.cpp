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

    for(;;)
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
            dist = max(dist-0.1f, 0);
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
    return dist;    
} 

bool plcollide(physent *d, physent *o, float &headspace, float &hi, float &lo)          // collide with player or monster
{
    if(o->state!=CS_ALIVE) return true;
    const float r = o->radius+d->radius;
    if(fabs(o->o.x-d->o.x)<r && fabs(o->o.y-d->o.y)<r) 
    {
        if(d->o.z-d->eyeheight<o->o.z-o->eyeheight) { if(o->o.z-o->eyeheight<hi) hi = o->o.z-o->eyeheight-1; }
        else if(o->o.z+o->aboveeye>lo) lo = o->o.z+o->aboveeye+1;
    
        if(fabs(o->o.z-d->o.z)<o->aboveeye+d->eyeheight) return false;
        headspace = d->o.z-o->o.z-o->aboveeye-d->eyeheight;
        if(headspace<0) headspace = 10;
    }
    return true;
}

bool cornertest(int mip, int x, int y, int dx, int dy, int &bx, int &by, int &bs)    // recursively collide with a mipmapped corner cube
{
    sqr *w = wmip[mip];
    int sz = ssize>>mip;
    bool stest = SOLID(SWS(w, x+dx, y, sz)) && SOLID(SWS(w, x, y+dy, sz));
    mip++;
    x /= 2;
    y /= 2;
    if(SWS(wmip[mip], x, y, ssize>>mip)->type==CORNER)
    {
        bx = x<<mip;
        by = y<<mip;
        bs = 1<<mip;
        return cornertest(mip, x, y, dx, dy, bx, by, bs);
    }
    return stest;
}

void mmcollide(physent *d, float &hi, float &lo)           // collide with a mapmodel
{
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type!=MAPMODEL) continue;
        mapmodelinfo &mmi = getmminfo(e.attr2);
        if(!&mmi || !mmi.h) continue;
        const float r = mmi.rad+d->radius;
        if(fabs(e.x-d->o.x)<r && fabs(e.y-d->o.y)<r)
        { 
            float mmz = (float)(S(e.x, e.y)->floor+mmi.zoff+e.attr3);
            if(d->o.z-d->eyeheight<mmz) { if(mmz<hi) hi = mmz; }
            else if(mmz+mmi.h>lo) lo = mmz+mmi.h;
        }
    }
}

// all collision happens here
// spawn is a dirty side effect used in spawning
// drop & rise are supplied by the physics below to indicate gravity/push for current mini-timestep

bool collide(physent *d, bool spawn, float drop, float rise)
{
    const float fx1 = d->o.x-d->radius;     // figure out integer cube rectangle this entity covers in map
    const float fy1 = d->o.y-d->radius;
    const float fx2 = d->o.x+d->radius;
    const float fy2 = d->o.y+d->radius;
    const int x1 = int(fx1);
    const int y1 = int(fy1);
    const int x2 = int(fx2);
    const int y2 = int(fy2);
    float hi = 127, lo = -128;

    for(int x = x1; x<=x2; x++) for(int y = y1; y<=y2; y++)     // collide with map
    {
        if(OUTBORD(x,y)) return false;
        sqr *s = S(x,y);
        float ceil = s->ceil;
        float floor = s->floor;
        switch(s->type)
        {
            case SOLID:
                return false;

            case CORNER:
            {
                int bx = x, by = y, bs = 1;
                if(x==x1 && y==y1 && cornertest(0, x, y, -1, -1, bx, by, bs) && fx1-bx+fy1-by<=bs
                || x==x2 && y==y1 && cornertest(0, x, y,  1, -1, bx, by, bs) && fx2-bx>=fy1-by
                || x==x1 && y==y2 && cornertest(0, x, y, -1,  1, bx, by, bs) && fx1-bx<=fy2-by
                || x==x2 && y==y2 && cornertest(0, x, y,  1,  1, bx, by, bs) && fx2-bx+fy2-by>=bs)
                   return false;
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

    if(hi-lo < d->eyeheight+d->aboveeye) return false;

    // Modified by Rick: plcollide now takes hi and lo in account aswell, that way we can jump/walk on players
    
    float headspace = 10;
    loopv(players)       // collide with other players
    {
        playerent *o = players[i]; 
        if(!o || o==d || (o==player1 && d->type==ENT_CAMERA)) continue;
        if(!plcollide(d, o, headspace, hi, lo)) return false;
    }
    
    if(d!=player1) if(!plcollide(d, player1, headspace, hi, lo)) return false;
    headspace -= 0.01f;
    
    mmcollide(d, hi, lo);    // collide with map models

    if(spawn)
    {
        d->o.z = lo+d->eyeheight;       // just drop to floor (sideeffect)
        d->onfloor = true;
    }
    else
    {
        const float spacetop = d->o.z-d->eyeheight-lo;
        if(spacetop<0)
        {
            if(spacetop>-0.01) 
            {
                d->o.z = lo+d->eyeheight;   // stick on step
            }
            else if(spacetop>-1.26f && d->type!=ENT_BOUNCE) d->o.z += rise;       // rise thru stair
            else return false;
        }
        else
        {
            d->o.z -= min(min(drop, spacetop), headspace);       // gravity
        }

        const float spacebottom = hi-(d->o.z+d->aboveeye);
        if(spacebottom<0)
        {
            if(spacebottom<-0.1) return false;     // hack alert!
            d->o.z = hi-d->aboveeye;          // glue to ceiling
            d->vel.z = 0;                     // cancel out jumping velocity
        }

        d->onfloor = d->o.z-d->eyeheight-lo<0.01f;
    }
    return true;
}

float floor(short x, short y)
{
    sqr *s = S(x, y);
    return s->type == FHF ? s->floor-(s->vdelta+S(x+1,y)->vdelta+S(x,y+1)->vdelta+S(x+1,y+1)->vdelta)/16.0f : s->floor;
}

VARP(maxroll, 0, 0, 20);

// main physics routine, moves a player/monster for a curtime step
// moveres indicated the physics precision (which is lower for monsters and multiplayer prediction)
// local is false for multiplayer prediction

void moveplayer(physent *pl, int moveres, bool local, int curtime)
{
    const bool water = hdr.waterlevel>pl->o.z-0.5f;
    const bool floating = (editmode && local) || pl->state==CS_EDITING;
    
    const float speed = curtime/(water ? 2000.0f : 1000.0f)*pl->maxspeed;
    const float friction = water ? 20.0f : (pl->onfloor || floating ? 6.0f : (pl->onladder ? 1.5f : 30.0f));
    const float fpsfric = friction/curtime*20.0f;

    vec d;      // vector of direction we ideally want to move in

    float drop, rise;

    if(pl->type==ENT_BOUNCE)
    {
        bounceent* bounce = (bounceent *) pl;
        drop = rise = 0;

        if(pl->onfloor) // apply friction
        {
	        pl->vel.mul(fpsfric-1);
	        pl->vel.div(fpsfric);
        }
        else // apply gravity
        {
            const float gravity = 9.81f/1000.0f*bounce->maxspeed;
            const float heightvel = (gravity)*pow(speed, 2.0f);
            bounce->vel.z -= heightvel;
        }

	    d = bounce->vel;
	    d.mul(speed);

        if(water) { d.x /= 6; d.y /= 6; }

        // rotate
        float rotspeed = bounce->rotspeed*d.magnitude();
        pl->pitch = fmod(pl->pitch+rotspeed, 360.0f);
        pl->yaw = fmod(pl->yaw+rotspeed, 360.0f);
    }
    else // fake physics for player ents to create _the_ cube movement (tm)
    {
        int move = pl->onladder && !pl->onfloor && pl->move == -1 ? 0 : pl->move; // movement on ladder
        
        d.x = (float)(move*cosf(RAD*(pl->yaw-90)));
        d.y = (float)(move*sinf(RAD*(pl->yaw-90)));
        d.z = 0.0f;
        
        if(floating || water)
        {
            d.x *= (float)cosf(RAD*(pl->pitch));
            d.y *= (float)cosf(RAD*(pl->pitch));
            d.z = (float)(move*sinf(RAD*(pl->pitch)));
        }

        d.x += (float)(pl->strafe*cosf(RAD*(pl->yaw-180)));
        d.y += (float)(pl->strafe*sinf(RAD*(pl->yaw-180)));
    	
	    pl->vel.mul(fpsfric-1);   // slowly apply friction and direction to velocity, gives a smooth movement
	    pl->vel.add(d);
	    pl->vel.div(fpsfric);
        d = pl->vel;
	    d.mul(speed);

        if(floating)                // just apply velocity
        {
            pl->o.add(d);
            if(pl->jumpnext) { pl->jumpnext = false; pl->vel.z = 2; }
        }
        else                        // apply velocity with collisions
        {   
            if(pl->onladder)
            {
			    const float climbspeed = 1.0f;

			    if(pl->type==ENT_BOT) pl->vel.z = climbspeed; // bots climb upwards only
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
                        pl->vel.z = 2.0f; //1.7f;                           // physics impulse upwards
                        if(water) { pl->vel.x /= 8; pl->vel.y /= 8; }      // dampen velocity change even harder, gives correct water feel
                        if(local) playsoundc(S_JUMP);
                        else if(pl->type==ENT_BOT) playsound(S_JUMP, &pl->o); // Added by Rick
                    }
                    pl->timeinair = 0;
                }
                else
                {
                    pl->timeinair += curtime;
                }
            }
        }

        const float gravity = 20.0f;
        float dropf = (gravity-1)+pl->timeinair/15.0f;			// incorrect, but works fine
        if(water) { dropf = 5; pl->timeinair = 0; }            // float slowly down in water
        if(pl->onladder) { dropf = 0; pl->timeinair = 0; }

        drop = dropf*curtime/gravity/100/moveres;			    // at high fps, gravity kicks in too fast
        rise = speed/moveres/1.2f;					            // extra smoothness when lifting up stairs
        if(pl->maxspeed-16>0.5f) pl += 0xF0F0;
    }

	if(!floating) loopi(moveres)                                // discrete steps collision detection & sliding
    {
        const float f = 1.0f/moveres;

        // try move forward
        pl->o.x += f*d.x;
        pl->o.y += f*d.y;
        pl->o.z += f*d.z;
        if(collide(pl, false, drop, rise)) continue;                     
        if(pl->type==ENT_CAMERA) return;
        // player stuck, try slide along y axis
        pl->o.x -= f*d.x;
        if(collide(pl, false, drop, rise))
        { 
            d.x = 0; 
			if(pl->type==ENT_BOUNCE) { pl->vel.x = -pl->vel.x; pl->vel.mul(0.7f); }
            continue; 
        }
        pl->o.x += f*d.x;
        // still stuck, try x axis
        pl->o.y -= f*d.y;
        if(collide(pl, false, drop, rise)) 
        { 
            d.y = 0; 
			if(pl->type==ENT_BOUNCE) { pl->vel.y = -pl->vel.y; pl->vel.mul(0.7f); }
            continue; 
        }
        pl->o.y += f*d.y;
        // try just dropping down
        pl->o.x -= f*d.x;
        pl->o.y -= f*d.y;
        if(collide(pl, false, drop, rise))
        { 
            d.y = d.x = 0;
            continue;
        }
        pl->o.z -= f*d.z;
        if(pl->type==ENT_BOUNCE) { pl->vel.z = -pl->vel.z; pl->vel.mul(0.5f); }
        break;
	}

    if(pl->type==ENT_CAMERA) return;
    else if(pl->type!=ENT_BOUNCE)
    {
        // automatically apply smooth roll when strafing
        if(pl->strafe==0) 
        {
            pl->roll = pl->roll/(1+(float)sqrt((float)curtime)/25);
        }
        else
        {
            pl->roll += pl->strafe*curtime/-30.0f;
            if(pl->roll>maxroll) pl->roll = (float)maxroll;
            if(pl->roll<-maxroll) pl->roll = (float)-maxroll;
        }
    }

    // play sounds on water transitions
    if(!pl->inwater && water) { playsound(S_SPLASH2, &pl->o); pl->vel.z = 0; }
    else if(pl->inwater && !water) playsound(S_SPLASH1, &pl->o);
    pl->inwater = water;
    // Added by Rick: Easy hack to store previous locations of all players/monsters/bots
    if(pl->type==ENT_PLAYER || pl->type==ENT_BOT) ((playerent *)pl)->history.update(pl->o, lastmillis);
    // End add
}

VARP(minframetime, 5, 10, 20);

int physicsfraction = 0, physicsrepeat = 0;

void physicsframe()          // optimally schedule physics frames inside the graphics frames
{
    if(curtime>=minframetime)
    {
        int faketime = curtime+physicsfraction;
        physicsrepeat = faketime/minframetime;
        physicsfraction = faketime%minframetime;
    }
    else
    {
        physicsrepeat = 1;
    }
}

void moveplayer(physent *p, int moveres, bool local)
{
    loopi(physicsrepeat) moveplayer(p, moveres, local, min(curtime, minframetime));
}

// movement input code

#define dir(name,v,d,s,os) void name(bool isdown) { player1->s = isdown; player1->v = isdown ? d : (player1->os ? -(d) : 0); player1->lastmove = lastmillis; }

dir(backward, move,   -1, k_down,  k_up);
dir(forward,  move,    1, k_up,    k_down);
dir(left,     strafe,  1, k_left,  k_right);
dir(right,    strafe, -1, k_right, k_left); 

void attack(bool on)
{
    if(intermission) return;
    if(editmode) editdrag(on);
    else if(demoplayback && on) shiftdemoplayer(1);
    else if(player1->state==CS_DEAD) respawn();
    else player1->attacking = on;
}   

void jumpn(bool on)
{ 
    if(intermission) return;
    if(demoplayback && on) demopaused = !demopaused;
    else if(player1->state==CS_DEAD)
    {
        if(on) respawn();
    }
    else player1->jumpnext = on;
}

COMMAND(backward, ARG_DOWN);
COMMAND(forward, ARG_DOWN);
COMMAND(left, ARG_DOWN);
COMMAND(right, ARG_DOWN);
COMMANDN(jump, jumpn, ARG_DOWN);
COMMAND(attack, ARG_DOWN);

void fixcamerarange(physent *cam)
{
    const float MAXPITCH = 90.0f;
    if(cam->pitch>MAXPITCH) cam->pitch = MAXPITCH;
    if(cam->pitch<-MAXPITCH) cam->pitch = -MAXPITCH;
    while(cam->yaw<0.0f) cam->yaw += 360.0f;
    while(cam->yaw>=360.0f) cam->yaw -= 360.0f;
}

VARP(sensitivity, 0, 30, 10000);
VARP(sensitivityscale, 1, 10, 10000);
VARP(invmouse, 0, 0, 1);

void mousemove(int dx, int dy)
{
    if(intermission) return;
    const float SENSF = 33.0f;     // try match quake sens
    camera1->yaw += (dx/SENSF)*(sensitivity/(float)sensitivityscale);
    camera1->pitch -= (dy/SENSF)*(sensitivity/(float)sensitivityscale)*(invmouse ? -1 : 1);
    fixcamerarange();
    if(camera1!=player1 && player1->state!=CS_DEAD && !demoplayback)
    {
        player1->yaw = camera1->yaw;
        player1->pitch = camera1->pitch;
    }
}

void entinmap(physent *d)    // brute force but effective way to find a free spawn spot in the map
{
    loopi(100)              // try max 100 times
    {
        float dx = (rnd(21)-10)/10.0f*i;  // increasing distance
        float dy = (rnd(21)-10)/10.0f*i;
        d->o.x += dx;
        d->o.y += dy;
        if(collide(d, true, 0, 0)) return;
        d->o.x -= dx;
        d->o.y -= dy;
    }
    conoutf("can't find entity spawn spot! (%d, %d)", d->o.x, d->o.y);
    // leave ent at original pos, possibly stuck
}

