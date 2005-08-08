// weapon.cpp: all shooting and effects code

#include "cube.h"

struct guninfo { short sound, reload, reloadtime, attackdelay,  damage, projspeed, part, spread, recoil, magsize; char *name; };

const int SGRAYS = 20;  //down from 20 (defualt)
const float SGSPREAD = 2;
vec sg[SGRAYS];

//change the particales
guninfo guns[NUMGUNS] =
{
    { S_KNIFE,    S_NULL,     0,      250,    50,     0,   0,  1,    1,   1,    "knife"   },
    { S_PISTOL,   S_RPISTOL,  1400,   170,    20,     0,   0, 20,   10,   8,    "pistol"  },  // *SGRAYS
    { S_SHOTGUN,  S_RSHOTGUN, 2400,   1100,   6,      0,   0,  1,   35,   7,    "shotgun" },  //reload time is for 1 shell from 7 too powerful to 6
    { S_SUBGUN,   S_RSUBGUN,  1650,   90,     14,     0,   0, 45,   15,   30,   "subgun"  },
    { S_SNIPER,   S_RSNIPER,  1950,   1500,   72,     0,   0, 60,   50,   5,    "sniper"  },
    { S_ASSULT,   S_RASSULT,  2000,   130,    33,     0,   0, 90,   40,   20,   "assult"  },  //recoil was 44
    { S_GRENADE,  S_NULL,     0,      2000,   40,    30,   6,  1,    1,   1,    "grenade" },

};


//weapon selection
void next()
{
      if(player1->gunselect==GUN_KNIFE)
            player1->gunselect = GUN_PISTOL;
      else if(player1->gunselect==GUN_PISTOL && player1->primary!=GUN_PISTOL)
            player1->gunselect = player1->primary;
      else if(player1->gunselect==player1->primary)
            player1->gunselect=GUN_KNIFE;

      conoutf("%s selected", (int)guns[player1->gunselect].name);
};

void previous()
{
      if(player1->gunselect==GUN_KNIFE)
            player1->gunselect = player1->primary;
      else if(player1->gunselect==player1->primary && player1->primary!=GUN_PISTOL)
            player1->gunselect = GUN_PISTOL;
      else if(player1->gunselect==GUN_PISTOL)
            player1->gunselect=GUN_KNIFE;

      conoutf("%s selected", (int)guns[player1->gunselect].name);
};


COMMAND(next,ARG_NONE);
COMMAND(previous,ARG_NONE);

void primary()
{
      player1->gunselect=player1->primary;
};

void secondary()
{
      player1->gunselect=GUN_PISTOL;
};

void melee()
{
      player1->gunselect=GUN_KNIFE;
};

COMMAND(primary,ARG_NONE);
COMMAND(secondary,ARG_NONE);
COMMAND(melee,ARG_NONE);

void reload()
{
      if(player1->gunselect==GUN_KNIFE) return;
      if(player1->mag[player1->gunselect]>=guns[player1->gunselect].magsize) return;
      if(player1->ammo[player1->gunselect]<=0) return;
      if(player1->reloading) return;

      player1->reloading = true;
      player1->lastaction = lastmillis;

      int a = guns[player1->gunselect].magsize - player1->mag[player1->gunselect];

      player1->gunwait = guns[player1->gunselect].reloadtime;
      
      //temp hack in drawhudgun() so no guns drawn while reloading, needs fixed

      

      if (a >= player1->ammo[player1->gunselect])
      {
            player1->mag[player1->gunselect] += player1->ammo[player1->gunselect];
            player1->ammo[player1->gunselect] = 0;
      }
      else
      {
            player1->mag[player1->gunselect] += a;
            player1->ammo[player1->gunselect] -= a;
      }

      playsoundc(guns[player1->gunselect].reload);
};

COMMAND(reload,ARG_NONE);

int reloadtime(int gun) { return guns[gun].attackdelay; };
int delaytime(int gun) { return guns[gun].attackdelay; };

void createrays(vec &from, vec &to)             // create random spread of rays for the shotgun
{
    vdist(dist, dvec, from, to);
    float f = dist*SGSPREAD/1000;
    loopi(SGRAYS)
    {
        #define RNDD (rnd(101)-50)*f
        vec r = { RNDD, RNDD, RNDD };
        sg[i] = to;
        vadd(sg[i], r); 
    };
};

//bool intersect(dynent *d, vec &from, vec &to)   // if lineseg hits entity bounding box
bool intersect(dynent *d, vec &from, vec &to, vec *end)   // if lineseg hits entity bounding box
{
    vec v = to, w = d->o, *p; 
    vsub(v, from);
    vsub(w, from);
    float c1 = dotprod(w, v);

    if(c1<=0) p = &from;
    else
    {
        float c2 = dotprod(v, v);
        if(c2<=c1) p = &to;
        else
        {
            float f = c1/c2;
            vmul(v, f);
            vadd(v, from);
            p = &v;
        };
    };

    /* Modified by Rick
    return p->x <= d->o.x+d->radius
        && p->x >= d->o.x-d->radius
        && p->y <= d->o.y+d->radius
        && p->y >= d->o.y-d->radius
        && p->z <= d->o.z+d->aboveeye
        && p->z >= d->o.z-d->eyeheight;
    */
    if( p->x <= d->o.x+d->radius
        && p->x >= d->o.x-d->radius
        && p->y <= d->o.y+d->radius
        && p->y >= d->o.y-d->radius
        && p->z <= d->o.z+d->aboveeye
        && p->z >= d->o.z-d->eyeheight)
    {
          if (end) *end = *p;
          return true;
    }
    return false;    
};

char *playerincrosshair()
{
        if(demoplayback) return NULL;
    loopv(players)
    {
        dynent *o = players[i];
        if(!o) continue; 
        if(intersect(o, player1->o, worldpos)) return o->name;
    };
    return NULL;
};

// Added by Rick
char *botincrosshair()
{
    if(demoplayback) return NULL;
    loopv(bots)
    {
        dynent *o = bots[i];
        if(!o) continue; 
        if(intersect(o, player1->o, worldpos)) return o->name;
    };
    return NULL;
};
// End add by Rick

const int MAXPROJ = 100;
struct projectile { vec o, to; float speed; dynent *owner; int gun; bool inuse, local; };
projectile projs[MAXPROJ];

void projreset() { loopi(MAXPROJ) projs[i].inuse = false; };

void newprojectile(vec &from, vec &to, float speed, bool local, dynent *owner, int gun)
{
    loopi(MAXPROJ)
    {
        projectile *p = &projs[i];
        if(p->inuse) continue;
        p->inuse = true;
        p->o = from;
        p->to = to;
        p->speed = speed;
        p->local = local;
        p->owner = owner;
        p->gun = gun;
        return;
    };
};

void hit(int target, int damage, dynent *d, dynent *at)
{
    if(d==player1) selfdamage(damage, at==player1 ? -1 : -2, at);
    //else if(d->monsterstate) monsterpain(d, damage, at);
    // Added by Rick: Let bots take damage
    else if (d->pBot) d->pBot->BotPain(damage, at);
    else if (d->bIsBot)
    {
         int PlayerIndex = -1;
         if (at->bIsBot) PlayerIndex = BotManager.GetBotIndex(at);
         else
         {
             loopv(players)
             {
                  if (!players[i]) continue;

                  if (players[i] == at)
                  { 
                       PlayerIndex = i;
                       break;
                  }
             }
         }

         int msgtype = (at->bIsBot) ? SV_BOT2BOTDMG : SV_CLIENT2BOTDMG;
         addmsg(1, 4, msgtype, BotManager.GetBotIndex(d), damage, PlayerIndex);
         playsound(S_PAIN1+rnd(5), &d->o);
    }
    // End add by Rick    
    //else { addmsg(1, 4, SV_DAMAGE, target, damage, d->lifesequence); playsound(S_PAIN1+rnd(5), &d->o); }; Modified by Rick: Added IsBot and PlayerIndex to message
    else
    {
         // Modified by Rick: Added IsBot and PlayerIndex to message
         int PlayerIndex = -1;
         if (at->bIsBot)
             PlayerIndex = BotManager.GetBotIndex(at);
         else
         {
             loopv(players)
             {
                 if (!players[i])
                     continue;

                  if (players[i] == at)
                  { 
                       PlayerIndex = i;
                       break;
                  }
             }
         }
         addmsg(1, 6, SV_DAMAGE, target, damage, d->lifesequence, at->bIsBot, PlayerIndex);
         playsound(S_PAIN1+rnd(5), &d->o);
    };    
    
    particle_splash(3, damage, 1000, d->o);
	 demodamage(damage, d->o);
};

const float RL_RADIUS = 5;
const float RL_DAMRAD = 7;   // hack

void radialeffect(dynent *o, vec &v, int cn, int qdam, dynent *at)
{
    if(o->state!=CS_ALIVE) return;
    vdist(dist, temp, v, o->o);
    dist -= 2; // account for eye distance imprecision
    if(dist<RL_DAMRAD) 
    {
        if(dist<0) dist = 0;
        int damage = (int)(qdam*(1-(dist/RL_DAMRAD)));
        hit(cn, damage, o, at);
        vmul(temp, (RL_DAMRAD-dist)*damage/800);
        vadd(o->vel, temp);
    };
};

// Modified by Rick: Added notthisbot
void splash(projectile *p, vec &v, vec &vold, int notthisplayer, int notthismonster, int notthisbot, int qdam)
{
    particle_splash(0, 50, 300, v);
    p->inuse = false;
    if(p->gun!=GUN_GRENADE)
    if(p->gun!=GUN_GRENADE) //changed
    {
        playsound(S_FEXPLODE, &v);
        // no push?
    }
    else
    {
        //playsound(S_RLHIT, &v);
        newsphere(v, RL_RADIUS, 0);
        dodynlight(vold, v, 0, 0, p->owner);
        if(!p->local) return;
        radialeffect(player1, v, -1, qdam, p->owner);
        loopv(players)
        {
            if(i==notthisplayer) continue;
            dynent *o = players[i];
            if(!o) continue; 
            radialeffect(o, v, i, qdam, p->owner);
        };
        // Added by Rick
        loopv(bots)
        {
            if(i==notthisbot) continue;
            dynent *o = bots[i];
            if(!o) continue; 
            radialeffect(o, v, i, qdam, p->owner);
        }        
        // End add by Rick                
        //dvector &mv = getmonsters();
        //loopv(mv) if(i!=notthismonster) radialeffect(mv[i], v, i, qdam, p->owner);
    };
};

// Modified by Rick
// inline void projdamage(dynent *o, projectile *p, vec &v, int i, int im, int qdam)
inline void projdamage(dynent *o, projectile *p, vec &v, int i, int im, int ib, int qdam)
{
    if(o->state!=CS_ALIVE) return;
    if(intersect(o, p->o, v))
    {
        // splash(p, v, p->o, i, im, qdam); Modified by Rick
        splash(p, v, p->o, i, im, ib, qdam);
        hit(i, qdam, o, p->owner);
    }; 
};

void moveprojectiles(float time)
{
    loopi(MAXPROJ)
    {
        projectile *p = &projs[i];
        if(!p->inuse) continue;
        int qdam = guns[p->gun].damage*(p->owner->quadmillis ? 4 : 1);
        vdist(dist, v, p->o, p->to);
        float dtime = dist*1000/p->speed;
        if(time>dtime) dtime = time;
        vmul(v, time/dtime);
        vadd(v, p->o)
        if(p->local)
        {
            loopv(players)
            {
                dynent *o = players[i];
                if(!o) continue; 
                // projdamage(o, p, v, i, -1, qdam); Modified by Rick
                projdamage(o, p, v, i, -1, -1, qdam);
            };
            // Added by Rick: Damage bots aswell
            loopv(bots)
            {
                dynent *o = bots[i];
                if(!o || (o == p->owner)) continue;
                projdamage(o, p, v, -1, -1, i, qdam);
            };
            // End add by Rick            
            // if(p->owner!=player1) projdamage(player1, p, v, -1, -1, qdam); Modified by Rick
            if(p->owner!=player1) projdamage(player1, p, v, -1, -1, -1, qdam);
            //dvector &mv = getmonsters();
            //loopv(mv) if(!vreject(mv[i]->o, v, 10.0f) && mv[i]!=p->owner) projdamage(mv[i], p, v, -1, i, qdam); Modified by Rick
            //loopv(mv) if(!vreject(mv[i]->o, v, 10.0f) && mv[i]!=p->owner) projdamage(mv[i], p, v, -1, i, -1, qdam);            
        };
        if(p->inuse)
        {
            // if(time==dtime) splash(p, v, p->o, -1, -1, qdam); Modified by Rick
            if(time==dtime) splash(p, v, p->o, -1, -1, -1, qdam);
            else
            {
                if(p->gun==GUN_GRENADE) { dodynlight(p->o, v, 0, 255, p->owner); particle_splash(5, 2, 200, v); }
                else { particle_splash(1, 1, 200, v); particle_splash(guns[p->gun].part, 1, 1, v); };
                // Added by Rick
                traceresult_s tr;
                TraceLine(p->o, v, p->owner, true, &tr);
                if (tr.collided) splash(p, v, p->o, -1, -1, -1, qdam);
                // End add                
            };       
        };
        p->o = v;
    };
};

void shootv(int gun, vec &from, vec &to, dynent *d, bool local)     // create visual effect from a shot
{
    playsound(guns[gun].sound, &d->o);
    int pspeed = 25;
    switch(gun)
    {
        case GUN_KNIFE:
            break;

        case GUN_SHOTGUN:
        {
            loopi(SGRAYS) particle_splash(0, 5, 200, sg[i]);
            break;
        };

        case GUN_PISTOL:
        case GUN_SUBGUN:
        case GUN_ASSULT:
            particle_splash(0, 100, 250, to);
            //particle_trail(1, 10, from, to);
            break;

        case GUN_GRENADE:
            pspeed = guns[gun].projspeed;
            newprojectile(from, to, (float)pspeed, local, d, gun);
            break;

        case GUN_SNIPER: 
            particle_splash(0, 50, 200, to);
            //particle_trail(1, 500, from, to);
            break;
    };
};

void hitpush(int target, int damage, dynent *d, dynent *at, vec &from, vec &to)
{
    hit(target, damage, d, at);
    vdist(dist, v, from, to);
    vmul(v, damage/dist/50);
    vadd(d->vel, v);
};

void raydamage(dynent *o, vec &from, vec &to, dynent *d, int i)
{
    if(o->state!=CS_ALIVE) return;
    int qdam = guns[d->gunselect].damage;
    if(d->quadmillis) qdam *= 4;
    if(d->gunselect==GUN_SHOTGUN)
    {
        int damage = 0;
        loop(r, SGRAYS) if(intersect(o, from, sg[r])) damage += qdam;
        if(damage) hitpush(i, damage, o, d, from, to);
    }
    else if(intersect(o, from, to)) hitpush(i, qdam, o, d, from, to);
};
void spreadandrecoil(vec & from, vec & to, dynent * d)
{
    //nothing specail for a knife
    if (d->gunselect==GUN_KNIFE) return;

    //spread
    vdist(dist, unitv, from, to);
    float f = dist/1000;
    int spd = guns[d->gunselect].spread;

    //recoil
    int rcl = guns[d->gunselect].recoil*-0.01f;

    if (d->gunselect==GUN_ASSULT)
    {
            if (d->shots<=3)
                  spd = spd / 5;

            rcl += (rnd(8)*-0.01f);
    };

    if ((d->gunselect==GUN_SNIPER) && (d->vel.x<.25f && d->vel.y<.25f))
    {
            spd = 1;
            rcl = rcl / 3;
    };

    if (d->gunselect!=GUN_SHOTGUN)  //no spread on shotgun
    {   
        vec r = { RNDD, RNDD, RNDD };
        vadd(to, r);
    };

    
 
   //increase pitch for recoil
    vdiv(unitv, dist);
    vec recoil = unitv;
    vmul(recoil, rcl);
    vadd(d->vel, recoil);
    if(d->pitch<80.0f) d->pitch += guns[d->gunselect].recoil*0.05f;

};

void shoot(dynent *d, vec &targ)
{
    int attacktime = lastmillis-d->lastaction;
    if(d->reloading && (attacktime<d->gunwait)) return;
    else if(attacktime<d->gunwait) return;
    d->gunwait = 0;
    d->reloading = false;
    if(!d->attacking) { d->shots = 0; return; };
    if(!(d->gunselect==GUN_SUBGUN || d->gunselect==GUN_ASSULT)) d->attacking = false;  //makes sub/assult autos
    else
    {
         d->shots++;
    };
    d->lastaction = lastmillis;
    d->lastattackgun = d->gunselect;
    // Modified by Rick
    // if(!d->mag[d->gunselect]) { playsoundc(S_NOAMMO); d->gunwait = 250; d->lastattackgun = -1; return; };
    
    if(!d->mag[d->gunselect])
    {
        if (d->bIsBot) botplaysound(S_NOAMMO, d);
        else playsoundc(S_NOAMMO);
        d->gunwait = 250;
        d->lastattackgun = -1;
        return;
    };
    // End mod    
    
    if (d->gunselect!=0) //if knife skip
    {
   	if(d->gunselect) d->mag[d->gunselect]--;
    }
    
    if(!d->mag[d->gunselect]) { playsoundc(S_NOAMMO); d->gunwait = 250; d->lastattackgun = -1; return; };
    if(d->gunselect) d->mag[d->gunselect]--;
    vec from = d->o;
    vec to = targ;
    from.z -= 0.2f;    // below eye
    
    spreadandrecoil(from,to,d);

    vdist(dist, unitv, from, to);
    vdiv(unitv, dist);

    if(d->gunselect==GUN_KNIFE) 
    {
        vmul(unitv, 3); // punch range
        to = from;
        vadd(to, unitv);
    };   
    if(d->gunselect==GUN_SHOTGUN) createrays(from, to);

    //if(d->quadmillis && attacktime>200) playsoundc(S_ITEMPUP); Modified by Rick
    if(d->quadmillis && attacktime>200)
    {
        if (d->bIsBot) botplaysound(S_ITEMPUP, d);
        else playsoundc(S_ITEMPUP);
    }
    // End mod        
    shootv(d->gunselect, from, to, d, true);
    // Modified by Rick
    // if(!d->monsterstate) addmsg(1, 8, SV_SHOT, d->gunselect, (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF), (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF));
    if(!d->monsterstate)
    {
        int index = -1;
        if (d->bIsBot)
            index = BotManager.GetBotIndex(d);
        addmsg(1, 9, SV_SHOT, d->gunselect, (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF), (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF), index);
    }
    // End mod    
    d->gunwait = delaytime(d->gunselect);// hack guns[d->gunselect].attackdelay;

    if(guns[d->gunselect].projspeed) return;
    
    loopv(players)
    {
        dynent *o = players[i];
        if(!o) continue; 
        raydamage(o, from, to, d, i);
    };

    // Added by Rick: raydamage on bots too
    loopv(bots)
    {
        dynent *o = bots[i];
        if(!o || (o == d)) continue; 
        raydamage(o, from, to, d, i);
    };
    // End add by Rick
    
    //dvector &v = getmonsters();
    //loopv(v) if(v[i]!=d) raydamage(v[i], from, to, d, -2);

    //if(d->monsterstate) raydamage(player1, from, to, d, -1); Modified by Rick
    if(d->monsterstate || d->bIsBot) raydamage(player1, from, to, d, -1);
};

//currently client side only, need to make serv
void additem(int gun)
{
	if (gun>0 && gun<10)
	{
//		player1->mag[gun] = guns[gun].magazine;
//		player1->ammo[gun] = guns[gun].magazine * 3;
	
	};	

	if (gun==10)  //helmet
	{  
		conoutf("NO HELMET FOR YOU!");
//	   		player1->armour = 20
//			break;
	};
    
	if (gun==11)   //armour
	{
		conoutf("you can't add armor yet!");
//        	additem(I_ARMOUR, player1->armour, 20);
//			break;
	};
};

COMMAND(additem,ARG_1INT);


//gotta find a neater way of doing this -argh
void altattack(bool on)
{
/*
	if (on)
	{
		switch(player1->gunselect)
		{
			case(GUN_KNIFE):
				break;
			case(GUN_SEMIPISTOL):
				break;
			case(GUN_SHOTGUN):
				break;
			case(GUN_SNIPER):
				setvar("fov",getvar("fov")-55);
				break;
			case(GUN_SUBGUN):
				break;
			case(GUN_AUTORIFLE):
				break;
			case(GUN_GRENADE):
				break;
		};
	}
	else 
	{
		switch(player1->gunselect)
		{
			case(GUN_KNIFE):
				break;
			case(GUN_SEMIPISTOL):
				break;
			case(GUN_SHOTGUN):
				break;
			case(GUN_SNIPER):
				setvar("fov",getvar("fov")+55);
				break;
			case(GUN_SUBGUN):
				break;
			case(GUN_AUTORIFLE):
				break;
			case(GUN_GRENADE):
				break;
		};
	};
    */
};


void altattack(void)
{
      player1->altattack = !player1->altattack;
      
      switch(player1->gunselect)
      {
            case(GUN_SHOTGUN):
                  if(player1->mag[player1->gunselect]>=guns[player1->gunselect].magsize) return;
                  if(player1->ammo[player1->gunselect]<=0) return;
                  if(player1->reloading) return;

                  player1->reloading = true;
                  player1->lastaction = lastmillis;

                  player1->gunwait = guns[player1->gunselect].reloadtime / 2;
      
                  if(player1->ammo[GUN_SHOTGUN]>1)
                        player1->mag[GUN_SHOTGUN]++;
                        player1->ammo[GUN_SHOTGUN]--;
                  
                  playsoundc(guns[player1->gunselect].reload);
                  break;

            case(GUN_SNIPER):
                  if(player1->altattack)
                        setvar("fov",getvar("fov")-55);
                  else
                        setvar("fov",getvar("fov")+55);
                  break;

                  
      }


};

COMMAND(altattack, ARG_NONE);
