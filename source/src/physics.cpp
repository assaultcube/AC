// physics.cpp: no physics books were hurt nor consulted in the construction of this code.
// All physics computations and constants were invented on the fly and simply tweaked until
// they "felt right", and have no basis in reality. Collision detection is simplistic but
// very robust (uses discrete steps at fixed fps).

#include "cube.h"
/*
bool plcollide(dynent *d, dynent *o, float &headspace)          // collide with player or monster
{
    if(o->state!=CS_ALIVE) return true;
    const float r = o->radius+d->radius;
    if(fabs(o->o.x-d->o.x)<r && fabs(o->o.y-d->o.y)<r) 
    {
        if(fabs(o->o.z-d->o.z)<o->aboveeye+d->eyeheight) return false;
        if(d->monsterstate) return false; // hack
        headspace = d->o.z-o->o.z-o->aboveeye-d->eyeheight;
        if(headspace<0) headspace = 10;
    };
    return true;
};
*/

bool plcollide(dynent *d, dynent *o, float &headspace, float &hi, float &lo) // collide with player or monster
{
    if(o->state!=CS_ALIVE) return true;
    const float r = o->radius+d->radius;
    if(fabs(o->o.x-d->o.x)<r && fabs(o->o.y-d->o.y)<r) 
    {
        if(d->o.z-d->eyeheight<o->o.z-o->eyeheight) { if(o->o.z-o->eyeheight<hi) hi = o->o.z-o->eyeheight-1; }
        else if(o->o.z+o->aboveeye>lo) lo = o->o.z+o->aboveeye+1;
    
        if(fabs(o->o.z-d->o.z)<o->aboveeye+d->eyeheight) return false;
        if(d->monsterstate) return false; // hack
        headspace = d->o.z-o->o.z-o->aboveeye-d->eyeheight;
        if(headspace<0) headspace = 10;        
    };
    return true;
};

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
    };
    return stest;
};

void mmcollide(dynent *d, float &hi, float &lo)           // collide with a mapmodel
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
        };
    };
};

// all collision happens here
// spawn is a dirty side effect used in spawning
// drop & rise are supplied by the physics below to indicate gravity/push for current mini-timestep

bool collide(dynent *d, bool spawn, float drop, float rise)
{
    const float fx1 = d->o.x-d->radius;     // figure out integer cube rectangle this entity covers in map
    const float fy1 = d->o.y-d->radius;
    const float fx2 = d->o.x+d->radius;
    const float fy2 = d->o.y+d->radius;
    const int x1 = fast_f2nat(fx1);
    const int y1 = fast_f2nat(fy1);
    const int x2 = fast_f2nat(fx2);
    const int y2 = fast_f2nat(fy2);
    float hi = 127, lo = -128;
    float minfloor = (d->monsterstate && !spawn && d->health>100) ? d->o.z-d->eyeheight-4.5f : -1000.0f;  // big monsters are afraid of heights, unless angry :)

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
            };

            case FHF:       // FIXME: too simplistic collision with slopes, makes it feels like tiny stairs
                floor -= (s->vdelta+S(x+1,y)->vdelta+S(x,y+1)->vdelta+S(x+1,y+1)->vdelta)/16.0f;
                break;

            case CHF:
                ceil += (s->vdelta+S(x+1,y)->vdelta+S(x,y+1)->vdelta+S(x+1,y+1)->vdelta)/16.0f;

        };
        if(ceil<hi) hi = ceil;
        if(floor>lo) lo = floor;
        if(floor<minfloor) return false;   
    };

    if(hi-lo < d->eyeheight+d->aboveeye) return false;

    float headspace = 10;
    loopv(players)       // collide with other players
    {
        dynent *o = players[i]; 
        if(!o || o==d) continue;
        if(!plcollide(d, o, headspace, hi, lo)) return false;
    };
    if(d!=player1) if(!plcollide(d, player1, headspace, hi, lo)) return false; 
    headspace -= 0.01f;
    
    mmcollide(d, hi, lo);    // collide with map models

    if(spawn)
    {
        d->o.z = lo+d->eyeheight;       // just drop to floor (sideeffect)
	d->startheight = d->o.z; //no falling damage on spawn
        d->onfloor = true;
    }
    else
    {
        const float space = d->o.z-d->eyeheight-lo;
        if(space<0)
        {
            if(space>-0.01) d->o.z = lo+d->eyeheight;   // stick on step
            else if(space>-1.26f) d->o.z += rise;       // rise thru stair
            else return false;
        }
        else
        {
            d->o.z -= min(min(drop, space), headspace);       // gravity
        };

        const float space2 = hi-(d->o.z+d->aboveeye);
        if(space2<0)
        {
            if(space2<-0.1) return false;     // hack alert!
            d->o.z = hi-d->aboveeye;          // glue to ceiling
            d->vel.z = 0;                     // cancel out jumping velocity
        };

        d->onfloor = d->o.z-d->eyeheight-lo<0.01f;
    };
    return true;
}

float rad(float x) { return x*3.14159f/180; };

VAR(maxroll, 0, 3, 20);

int physicsfraction = 0, physicsrepeat = 0;
const int MINFRAMETIME = 20; // physics always simulated at 50fps or better

void physicsframe()          // optimally schedule physics frames inside the graphics frames
{
    if(curtime>=MINFRAMETIME)
    {
        int faketime = curtime+physicsfraction;
        physicsrepeat = faketime/MINFRAMETIME;
        physicsfraction = faketime-physicsrepeat*MINFRAMETIME;
    }
    else
    {
        physicsrepeat = 1;
    };
};

void selfdamage(dynent * d, int dm)
{
    if(d==player1) selfdamage(dm, -1, d);
    else { addmsg(1, 4, SV_DAMAGE, d, dm, d->lifesequence); playsound(S_FALL1+rnd(5), &d->o); };
    particle_splash(3, dm, 1000, d->o);  //edit out?
    demodamage(dm, d->o);
};


// main physics routine, moves a player/monster for a curtime step
// moveres indicated the physics precision (which is lower for monsters and multiplayer prediction)
// local is false for multiplayer prediction

void moveplayer(dynent *pl, int moveres, bool local, int curtime)
{
    const bool water = hdr.waterlevel>pl->o.z-0.5f;
    const bool floating = (editmode && local) || pl->state==CS_EDITING;

    vec d;      // vector of direction we ideally want to move in

    d.x = (float)(pl->move*cos(rad(pl->yaw-90)));
    d.y = (float)(pl->move*sin(rad(pl->yaw-90)));
    d.z = 0;

    if(floating || water)
    {
        d.x *= (float)cos(rad(pl->pitch));
        d.y *= (float)cos(rad(pl->pitch));
        d.z = (float)(pl->move*sin(rad(pl->pitch)));
    };

    d.x += (float)(pl->strafe*cos(rad(pl->yaw-180)));
    d.y += (float)(pl->strafe*sin(rad(pl->yaw-180)));

    const float speed = curtime/(water ? 2000.0f : 1000.0f)*pl->maxspeed;
    const float friction = water ? 20.0f : (pl->onfloor || floating ? 6.0f : 30.0f);

    const float fpsfric = friction/curtime*20.0f;   
    
    vmul(pl->vel, fpsfric-1);   // slowly apply friction and direction to velocity, gives a smooth movement
    vadd(pl->vel, d);
    vdiv(pl->vel, fpsfric);
    d = pl->vel;
    vmul(d, speed);             // d is now frametime based velocity vector

    pl->blocked = false;
    pl->moving = true;

    if(floating)                // just apply velocity
    {
        vadd(pl->o, d);
        if(pl->jumpnext) { pl->jumpnext = false; pl->vel.z = 2;    }
    }
    else                        // apply velocity with collision
    {
        if(pl->onfloor || water)
        {
	    if (pl->onfloor || !pl->vel.z)
	    {
                int damage = ((int)pl->startheight - (int)pl->o.z)*2;
		if (damage > 20 && local) //try adding "&& local" if falling not fixed
		{
                        selfdamage(damage,-1,pl); 
                        demodamage(damage, player1->o);
                        playsound(S_FALL1+rnd(2), &pl->o);
                };
                pl->startheight=pl->o.z;
            };

            if(pl->jumpnext)
            {
                pl->jumpnext = false;
                if (pl->hasarmour)
                pl->vel.z = 1.9f;
                else
                pl->vel.z = 1.7f;       // physics impulse upwards
                if(water) { pl->vel.x /= 8; pl->vel.y /= 8; };      // dampen velocity change even harder, gives correct water feel
                if(local) playsoundc(S_JUMP);
                else if(pl->monsterstate) playsound(S_JUMP, &pl->o);
            }
            else if(pl->timeinair>800)  // if we land after long time must have been a high jump, make thud sound
            {
                if(local) playsoundc(S_LAND);
                else if(pl->monsterstate) playsound(S_LAND, &pl->o);
            };
            pl->timeinair = 0;
        }
        else
        {
            pl->timeinair += curtime;
        };

        const float gravity = 20;
        const float f = 1.0f/moveres;
        float dropf = ((gravity-1)+pl->timeinair/15.0f);        // incorrect, but works fine
        if(water) { dropf = 5; pl->timeinair = 0; };            // float slowly down in water
        const float drop = dropf*curtime/gravity/100/moveres;   // at high fps, gravity kicks in too fast
        const float rise = speed/moveres/1.2f;                  // extra smoothness when lifting up stairs

        loopi(moveres)                                          // discrete steps collision detection & sliding
        {
            // try move forward
            pl->o.x += f*d.x;
            pl->o.y += f*d.y;
            pl->o.z += f*d.z;
            if(collide(pl, false, drop, rise)) continue;                     
            // player stuck, try slide along y axis
            pl->blocked = true;
            pl->o.x -= f*d.x;
            if(collide(pl, false, drop, rise)) { d.x = 0; continue; };   
            pl->o.x += f*d.x;
            // still stuck, try x axis
            pl->o.y -= f*d.y;
            if(collide(pl, false, drop, rise)) { d.y = 0; continue; };       
            pl->o.y += f*d.y;
            // try just dropping down
            pl->moving = false;
            pl->o.x -= f*d.x;
            pl->o.y -= f*d.y;
            if(collide(pl, false, drop, rise)) { d.y = d.x = 0; continue; }; 
            pl->o.z -= f*d.z;
            break;
        };
    };

    // detect wether player is outside map, used for skipping zbuffer clear mostly

    if(pl->o.x < 0 || pl->o.x >= ssize || pl->o.y <0 || pl->o.y > ssize)
    {
        pl->outsidemap = true;
    }
    else
    {
        sqr *s = S((int)pl->o.x, (int)pl->o.y);
        pl->outsidemap = SOLID(s)
           || pl->o.z < s->floor - (s->type==FHF ? s->vdelta/4 : 0)
           || pl->o.z > s->ceil  + (s->type==CHF ? s->vdelta/4 : 0);
    };
    
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
    };
    
    // play sounds on water transitions
    
    if(!pl->inwater && water) { playsound(S_SPLASH2, &pl->o); pl->vel.z = 0; }
    else if(pl->inwater && !water) playsound(S_SPLASH1, &pl->o);
    pl->inwater = water;
};

void moveplayer(dynent *pl, int moveres, bool local)
{
    loopi(physicsrepeat) moveplayer(pl, moveres, local, i ? curtime/physicsrepeat : curtime-curtime/physicsrepeat*(physicsrepeat-1));
};

