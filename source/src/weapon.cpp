// weapon.cpp: all shooting and effects code

#include "cube.h"
#include "bot/bot.h"

struct guninfo { short sound, reload, reloadtime, attackdelay, damage, projspeed, part, spread, recoil, magsize, mdl_kick_rot, mdl_kick_back; bool isauto; };

const int SGRAYS = 21;  //down from 21, must be 32 or less (default)
const float SGSPREAD = 2;
vec sg[SGRAYS];

char *gunnames[NUMGUNS] = { "knife", "pistol", "shotgun", "subgun", "sniper", "assault", "grenade" };

guninfo guns[NUMGUNS] =
{    
    { S_KNIFE,		S_NULL,     0,      500,    50,     0,   0,  1,    1,   1,    0,  0,  false },
    { S_PISTOL,		S_RPISTOL,  1400,   170,    19,     0,   0, 80,   10,   8,    6,  5,  false },  // *SGRAYS
    { S_SHOTGUN,	S_RSHOTGUN, 2400,   1000,   5,      0,   0,  1,   35,   7,    9,  9,  false },  //reload time is for 1 shell from 7 too powerful to 6
    { S_SUBGUN,		S_RSUBGUN,  1650,   80,		16,		0,   0, 70,   15,   30,   1,  2,  true  },
    { S_SNIPER,		S_RSNIPER,  1950,   1500,   85,     0,   0, 60,   50,   5,    4,  4,  false },
    { S_ASSAULT,	S_RASSAULT, 2000,   130,    24,     0,   0, 20,   40,   15,   0,  2,  true  },  //recoil was 44
    { S_NULL,		S_NULL,     1000,   1200,   150,    20,  6,  1,    1,   1,    3,  1,  false },
};

int nadetimer = 2000; // detonate after $ms
bool gun_changed = false;

void checkweaponswitch()
{
	if(!player1->weaponchanging) return;
    int timeprogress = lastmillis-player1->lastaction;
	if(timeprogress>WEAPONCHANGE_TIME) 
	{
		gun_changed = true;
		player1->weaponchanging = false; 
		player1->lastaction = 0;
	}
    else if(timeprogress>WEAPONCHANGE_TIME/2)
    {
        if((!player1->akimbo && player1->akimbomillis && player1->nextweapon==GUN_PISTOL) || 
            (player1->akimbo && !player1->akimbomillis && player1->gunselect==GUN_PISTOL)) 
                player1->akimbo = !player1->akimbo;
        player1->gunselect = player1->nextweapon;
    }
}

void weaponswitch(int gun)
{
    player1->weaponchanging = true;
	player1->thrownademillis = 0;
    player1->lastaction = player1->akimbolastaction[0] = player1->akimbolastaction[1] = lastmillis;
    player1->nextweapon = gun;
	playsound(S_GUNCHANGE);
}

void weapon(int gun)
{
	if(gun == player1->gunselect) return;
	if(player1->state!=CS_ALIVE || player1->weaponchanging || NADE_IN_HAND || player1->reloading) return;
    if(gun != GUN_KNIFE && gun != GUN_GRENADE && gun != GUN_PISTOL && gun != player1->primary) return;

    if(m_noguns && gun != GUN_KNIFE && gun != GUN_GRENADE) return;
    if(m_noprimary && gun != GUN_KNIFE && gun != GUN_GRENADE && gun != GUN_PISTOL) return;
    if(m_nopistol && gun == GUN_PISTOL) return;
    if(gun == GUN_GRENADE && !player1->mag[GUN_GRENADE]) return;

    setscope(false);
    weaponswitch(gun);
}

void shiftweapon(int s)
{
    for(int i = 0; i < NUMGUNS && !player1->weaponchanging; i++) 
    {
        int trygun = player1->gunselect + s + (s < 0 ? -i : i);
        if((trygun %= NUMGUNS) < 0) trygun += NUMGUNS;
        weapon(trygun);
    }
}

int currentprimary() { return player1->primary; }
int curweapon() { return player1->gunselect; }
int magcontent(int gun) { if(gun > 0 && gun < NUMGUNS) return player1->mag[gun]; else return -1;}

COMMAND(weapon, ARG_1INT);
COMMAND(shiftweapon, ARG_1INT);
COMMAND(currentprimary, ARG_1EST);
COMMAND(curweapon, ARG_1EXP);
COMMAND(magcontent, ARG_1EXP);

VAR(scopefov, 5, 50, 50);
bool scoped = false;
int oldfov = 100;

void setscope(bool activate)
{
	if(player1->gunselect != GUN_SNIPER || player1->state == CS_DEAD) return;
	if(activate == scoped) return;
	if(activate)
	{
		oldfov = getvar("fov");
		setvar("fov", scopefov);
	}
	else
	{
		setvar("fov", oldfov);
	}
	scoped = activate;
}

COMMAND(setscope, ARG_1INT);

void reload(playerent *d)
{
    if(demoplayback) { setvar("gamespeed", getvar("gamespeed") != 100 ? 100 : 35); return; }
	if(!d || d->state!=CS_ALIVE || d->reloading || d->weaponchanging) return;
	if(!reloadable_gun(d->gunselect) || d->ammo[d->gunselect]<=0) return;
	if(d->mag[d->gunselect] >= (has_akimbo(d) ? 2 : 1)*guns[d->gunselect].magsize) return;
	if(d == player1) setscope(false);
	
    d->reloading = true;
    d->lastaction = lastmillis;
    d->akimbolastaction[0] = d->akimbolastaction[1] = lastmillis;
    d->gunwait = guns[d->gunselect].reloadtime;
    
    int numbullets = (has_akimbo(d) ? 2 : 1)*guns[d->gunselect].magsize - d->mag[d->gunselect];
    
	if(has_akimbo(d)) numbullets = guns[d->gunselect].magsize * 2 - d->mag[d->gunselect];
    
	if(numbullets > d->ammo[d->gunselect]) numbullets = d->ammo[d->gunselect];
	d->mag[d->gunselect] += numbullets;
	d->ammo[d->gunselect] -= numbullets;

    if(has_akimbo(d)) playsoundc(S_RAKIMBO);
	else if(d->type==ENT_BOT) playsound(guns[d->gunselect].reload, &d->o);
	else playsoundc(guns[d->gunselect].reload);
}

void selfreload() { reload(player1); }
COMMANDN(reload, selfreload, ARG_NONE);

int reloadtime(int gun) { return guns[gun].reloadtime; }
int attackdelay(int gun) { return guns[gun].attackdelay; }
int magsize(int gun) { return guns[gun].magsize; }
int kick_rot(int gun) { return guns[gun].mdl_kick_rot; }
int kick_back(int gun) { return guns[gun].mdl_kick_back; }

void createrays(vec &from, vec &to)             // create random spread of rays for the shotgun
{
    float f = to.dist(from)*SGSPREAD/1000;
    loopi(SGRAYS)
    {
        #define RNDD (rnd(101)-50)*f
        vec r(RNDD, RNDD, RNDD);
        sg[i] = to;
        sg[i].add(r);
        #undef RNDD
    }
}

bool intersect(dynent *d, vec &from, vec &to, vec *end)   // if lineseg hits entity bounding box
{
    vec v = to, w = d->o, *p; 
    v.sub(from);
    w.sub(from);
    float c1 = w.dot(v);

    if(c1<=0) p = &from;
    else
    {
        float c2 = v.squaredlen();
        if(c2<=c1) p = &to;
        else
        {
            float f = c1/c2;
            v.mul(f).add(from);
            p = &v;
        }
    }

    return p->x <= d->o.x+d->radius
        && p->x >= d->o.x-d->radius
        && p->y <= d->o.y+d->radius
        && p->y >= d->o.y-d->radius
        && p->z <= d->o.z+d->aboveeye
        && p->z >= d->o.z-d->eyeheight;
}

playerent *intersectclosest(vec &from, vec &to, int &n, playerent *at)
{
    playerent *best = NULL;
    float bestdist = 1e16f;
    if(at!=player1 && player1->state==CS_ALIVE && intersect(player1, from, to))
    {
        best = player1;
        bestdist = at->o.dist(player1->o);
        n = getclientnum();
    }
    loopv(players)
    {
        playerent *o = players[i];
        if(!o || o==at || o->state!=CS_ALIVE) continue;
        if(!intersect(o, from, to)) continue;
        float dist = at->o.dist(o->o);
        if(dist<bestdist)
        {
            best = o;
            bestdist = dist;
            n = i; 
        }
    }
    return best;
}

playerent *playerincrosshair()
{
    if(demoplayback) return NULL;
    int n;
    return intersectclosest(player1->o, worldpos, n, player1);
}

const int MAXPROJ = 100;
struct projectile { vec o, to; float speed; playerent *owner; int gun; bool inuse, local; };
projectile projs[MAXPROJ];

void projreset() { loopi(MAXPROJ) projs[i].inuse = false; }

void newprojectile(vec &from, vec &to, float speed, bool local, playerent *owner, int gun)
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
    }
}

void hit(int target, int damage, playerent *d, playerent *at, bool gib=false)
{
    if(d==player1 || d->type==ENT_BOT) dodamage(damage, -1, at, gib, d);
    else
    {
         addmsg(gib ? SV_GIBDAMAGE : SV_DAMAGE, "ri3", target, damage, d->lifesequence);
         playsound(S_PAIN1+rnd(5), &d->o);
    }
    particle_splash(3, damage, 1000, d->o);
    demodamage(damage, d->o);
}

const float RL_DAMRAD = 10;

void radialeffect(playerent *o, vec &v, int cn, int qdam, playerent *at)
{
    if(o->state!=CS_ALIVE) return;
    vec temp;
    float dist = o->o.dist(v, temp);
    dist -= 2; // account for eye distance imprecision
    if(dist<RL_DAMRAD) 
    {
        if(dist<0) dist = 0;
        int damage = (int)(qdam*(1-(dist/RL_DAMRAD)));
        hit(cn, damage, o, at, true);
        o->vel.add(temp.mul((RL_DAMRAD-dist)*damage/800));
    }
}

void splash(projectile *p, vec &v, vec &vold, int notthisplayer, int qdam)
{
    particle_splash(0, 50, 300, v);
    p->inuse = false;
    if(p->gun!=GUN_GRENADE)
    {
        playsound(S_FEXPLODE, &v);
        // no push?
    }
    else
    {
        //playsound(S_RLHIT, &v);
        particle_fireball(5, v); 
        dodynlight(vold, v, 0, 0, p->owner);
        if(!p->local) return;
        radialeffect(player1, v, -1, qdam, p->owner);
        loopv(players)
        {
            if(i==notthisplayer) continue;
            playerent *o = players[i];
            if(!o) continue; 
            radialeffect(o, v, i, qdam, p->owner);
        }
    }
}

inline void projdamage(playerent *o, projectile *p, vec &v, int i, int qdam)
{
    if(o->state!=CS_ALIVE) return;
    if(intersect(o, p->o, v))
    {
        splash(p, v, p->o, i, qdam);
        hit(i, qdam, o, p->owner, true); //fixme
    } 
}

void moveprojectiles(float time)
{
    loopi(MAXPROJ)
    {
        projectile *p = &projs[i];
        if(!p->inuse) continue;
        //int qdam = guns[p->gun].damage*(p->owner->quadmillis ? 4 : 1);
        int qdam = guns[p->gun].damage;
        vec v;
        float dist = p->to.dist(p->o, v);
        float dtime = dist*1000/p->speed;
        if(time>dtime) dtime = time;
        v.mul(time/dtime).add(p->o);
        if(p->local)
        {
            loopvj(players)
            {
                playerent *o = players[j];
                if(!o) continue; 
                projdamage(o, p, v, j, qdam);
            }
            if(p->owner!=player1) projdamage(player1, p, v, -1, qdam);
        }
        if(p->inuse)
        {
            if(time==dtime) splash(p, v, p->o, -1, qdam);
            else
            {
                if(p->gun==GUN_GRENADE) { dodynlight(p->o, v, 0, 255, p->owner); particle_splash(5, 2, 200, v); }
                else { particle_splash(1, 1, 200, v); particle_splash(guns[p->gun].part, 1, 1, v); }
                // Added by Rick
                traceresult_s tr;
                TraceLine(p->o, v, p->owner, true, &tr);
                if (tr.collided) splash(p, v, p->o, -1, qdam);
                // End add                
            }       
        }
        p->o = v;
    }
}

vector<bounceent *> bounceents;

bounceent *newbounceent()
{
    bounceent *p = new bounceent;
    bounceents.add(p);
    return p;
}

void explode_nade(bounceent *i);

void movebounceents()
{
    loopv(bounceents) if(bounceents[i])
    {
        bounceent *p = bounceents[i];
        if(p->bouncestate == NADE_THROWED || p->bouncestate == GIB) moveplayer(p, 2, false);
        
        if(lastmillis - p->millis >= p->timetolife)
        {
            if(p->bouncestate==NADE_ACTIVATED || p->bouncestate==NADE_THROWED) explode_nade(bounceents[i]);
            delete p;
            bounceents.remove(i);
            i--;
        }
    }
}

void clearbounceents()
{   
    if(gamespeed==100);
    else if(multiplayer(false)) bounceents.add((bounceent *)player1);
    loopv(bounceents) if(bounceents[i]) { delete bounceents[i]; bounceents.remove(i--); }
}

void renderbounceents()
{
    loopv(bounceents)
    {
        bounceent *p = bounceents[i];
        if(!p) continue;
        string model;
        float z = p->o.z;

        switch(p->bouncestate)
        {
            case NADE_THROWED:
                s_strcpy(model, "weapons/grenade/static");
                break;
            case GIB:
            default:
            {    
                uint n = (((4*(uint)(size_t)p)+(uint)p->timetolife)%3)+1;
                s_sprintf(model)("misc/gib0%u", n);
                int t = lastmillis-p->millis;
                if(t>p->timetolife-2000)
                {
                    t -= p->timetolife-2000;
                    z -= t*t/4000000000.0f*t;
                }
                break;
            }
        }
        path(model);
        rendermodel(model, ANIM_MAPMODEL|ANIM_LOOP, 0, 1.1f, p->o.x, p->o.y, z, p->yaw+90, p->pitch);
    }
}

VARP(gibnum, 0, 6, 1000); 
VARP(gibttl, 0, 5000, 15000);
VARP(gibspeed, 1, 30, 100);

void addgib(playerent *d)
{   
    if(!d) return;
    playsound(S_GIB, &d->o);

    loopi(gibnum)
    {
        bounceent *p = newbounceent();
        p->owner = d;
        p->millis = lastmillis;
        p->timetolife = gibttl+rnd(10)*100;
        p->bouncestate = GIB;

        p->o = d->o;
        p->o.z -= d->aboveeye;
    
        p->yaw = (float)rnd(360);
        p->pitch = (float)rnd(360);

        p->maxspeed = 30.0f;
        p->rotspeed = 3.0f;

        const float angle = (float)rnd(360);
        const float speed = (float)gibspeed;

        p->vel.x = sinf(RAD*angle)*rnd(1000)/1000.0f;
        p->vel.y = cosf(RAD*angle)*rnd(1000)/1000.0f;
        p->vel.z = rnd(1000)/1000.0f;
        p->vel.mul(speed/100.0f);
    }
}

void thrownade(playerent *d, const vec &vel, bounceent *p)
{
    if(!d || !p) return;
    vec dir(vel);
    dir.mul(1.1f*d->radius);
    p->vel = vel;
    p->o.add(dir);
    p->o.add(d->o);
    loopi(10) moveplayer(p, 10, true, 10);

	p->bouncestate = NADE_THROWED;
    d->thrownademillis = lastmillis;
    d->inhandnade = NULL;
    
    if(d==player1)
    {
		player1->lastaction = lastmillis;
        addmsg(SV_SHOT, "ri8", d->gunselect, (int)(d->o.x*DMF), (int)(d->o.y*DMF), (int)(d->o.z*DMF), (int)(vel.x*DMF), (int)(vel.y*DMF), (int)(vel.z*DMF), lastmillis-p->millis);
    }
}

void thrownade(playerent *d, bounceent *p)
{
	if(!d || !p) return;
	const float speed = cosf(RAD*d->pitch);
	vec vel(sinf(RAD*d->yaw)*speed, -cosf(RAD*d->yaw)*speed, sinf(RAD*d->pitch));
    vel.mul(1.5f);
	thrownade(d, vel, p);
}

bounceent *new_nade(playerent *d, int millis = 0)
{
    bounceent *p = newbounceent();
    p->owner = d;
    p->millis = lastmillis;
    p->timetolife = 2000-millis;
    p->bouncestate = NADE_ACTIVATED;
	p->maxspeed = 27.0f;
    p->rotspeed = 6.0f;
    
    d->inhandnade = p;
    d->thrownademillis = 0;  
	if(d==player1 && !demoplayback) playsoundc(S_GRENADEPULL);
    return p;
}

void explode_nade(bounceent *i)
{ 
    if(!i) return;
    if(i->bouncestate != NADE_THROWED) thrownade(i->owner, vec(0,0,0), i);
    playsound(S_FEXPLODE, &i->o);
    newprojectile(i->o, i->o, 1, i->owner==player1, i->owner, GUN_GRENADE);
}

void shootv(int gun, vec &from, vec &to, playerent *d, bool local, int nademillis)     // create visual effect from a shot
{
    if(guns[gun].sound) playsound(guns[gun].sound, d==player1 ? NULL : &d->o);
    switch(gun)
    {
        case GUN_KNIFE:
            break;

        case GUN_SHOTGUN:
        {
            loopi(SGRAYS) particle_splash(0, 5, 200, sg[i]);
            if(addbullethole(from, to))
            {
                int holes = 3+rnd(5);
                loopi(holes) addbullethole(from, sg[i], 0);
            }
            break;
        }

        case GUN_PISTOL:
        case GUN_SUBGUN:
        case GUN_ASSAULT:
		{
            addbullethole(from, to);
            addshotline(d, from, to);
            particle_splash(0, 5, 250, to);
            if(!local && !CMPB(guns, 0xc7d70531)) player1->clientnum += 250*(int)to.dist(from);
            break;
		}

        case GUN_GRENADE:
		{
			if(d!=player1 || demoplayback)
			{
				bounceent *p = new_nade(d, nademillis);
				thrownade(d, to, p);
			}
			break;
		}
            
        case GUN_SNIPER: 
		{
            addbullethole(from, to);
            addshotline(d, from, to);
            particle_splash(0, 50, 200, to);
            particle_trail(1, 500, from, to);
            break;
		}
    }
}

void hitpush(int target, int damage, playerent *d, playerent *at, vec &from, vec &to, bool gib = false)
{
	hit(target, damage, d, at, gib);
    vec v;
    float dist = to.dist(from, v);
    v.mul(damage/dist/50);
    d->vel.add(v);
}

void shorten(vec &from, vec &to, vec &target)
{
    target.sub(from).normalize().mul(from.dist(to)).add(from);
}

const float HEADSIZE = 0.8f;
vec hitpos;

void raydamage(vec &from, vec &to, playerent *d)
{
    int i = -1, gdam = guns[d->gunselect].damage;
    playerent *o = NULL;
    if(d->gunselect==GUN_SHOTGUN)
    {
        uint done = 0;
        playerent *cl = NULL;
        int n = -1;
        for(;;)
        {
            bool raysleft = false;
            int damage = 0;
            o = NULL;
            loop(r, SGRAYS) if((done&(1<<r))==0 && (cl = intersectclosest(from, sg[r], n, d)))
            {
                if(!o || o==cl)
                {
                    damage += gdam;
                    o = cl;
                    done |= 1<<r;
                    i = n;
                    shorten(from, o->o, sg[r]);
                }
                else raysleft = true;
            }
            if(damage) hitpush(i, damage, o, d, from, to);
            if(!raysleft) break;
        }
    }
    else if((o = intersectclosest(from, to, i, d)))
    {
        bool gib = false;
        if(d->gunselect==GUN_KNIFE) gib = true;
    	else if(d==player1 && d->gunselect==GUN_SNIPER
        && worldpos!=hitpos && hitpos.z>=o->o.z+o->aboveeye-HEADSIZE
        && intersect(o, from, hitpos)) 
        {
            gdam *= 3;
            gib = true;
        }

        hitpush(i, gdam, o, d, from, to, gib);
        shorten(from, o->o, to);
    }
}

void spreadandrecoil(vec &from, vec &to, playerent *d)
{
    //nothing special for a knife
    if (d->gunselect==GUN_KNIFE || d->gunselect==GUN_GRENADE) return;

    //spread
    vec unitv;
    float dist = to.dist(from, unitv);
    float f = dist/1000;
    int spd = guns[d->gunselect].spread;

    //recoil
    float rcl = guns[d->gunselect].recoil*-0.01f;

    if(d->gunselect==GUN_ASSAULT)
    {
        if(d->shots > 3)
            spd = 70;
        rcl += (rnd(8)*-0.01f);
    }
   
    if((d->gunselect==GUN_SNIPER) /*&& (d->vel.x<.25f && d->vel.y<.25f)*/ && scoped)
    {
        spd = 1;
        rcl = rcl / 3;
    }

    if(d->gunselect!=GUN_SHOTGUN)  //no spread on shotgun
    {   
        #define RNDD (rnd(spd)-spd/2)*f
        vec r(RNDD, RNDD, RNDD);
        to.add(r);
        #undef RNDD
    }

   //increase pitch for recoil
    d->vel.add(vec(unitv).mul(rcl/dist));

    if(d->pitch<80.0f) d->pitch += guns[d->gunselect].recoil*0.05f;
}

bool hasammo(playerent *d) 	// bot mod
{
	if(!d->mag[d->gunselect])
	{
		if (d->type==ENT_BOT) playsound(S_NOAMMO, &d->o);
		else playsoundc(S_NOAMMO);
		d->gunwait = 250;
		d->lastaction = lastmillis;
		d->lastattackgun = -1;
		return false;
	} else return true;
}

VARP(autoreload, 0, 1, 1);

const int grenadepulltime = 650;
bool akimboside = false;

void shoot(playerent *d, vec &targ)
{
	int attacktime = lastmillis-d->lastaction;

	vec from = d->o;
	vec to = targ;

	if(d->gunselect==GUN_GRENADE)
	{
		d->shots = 0;

		if(d->thrownademillis && attacktime >= 13*(1000/25))
		{
			d->weaponchanging = true;
			d->nextweapon = d->mag[GUN_GRENADE] ? GUN_GRENADE : d->primary;
			d->thrownademillis = 0;
			d->lastaction = lastmillis-1-WEAPONCHANGE_TIME/2;
		}

		if(d->weaponchanging || attacktime<d->gunwait) return;
		d->gunwait = 0;
		d->reloading = false;

		if(d->attacking && !d->inhandnade) // activate
		{
			if(!hasammo(d)) return;
			d->mag[d->gunselect]--;
			new_nade(d);
			d->gunwait = grenadepulltime;
			d->lastaction = lastmillis;
			d->lastattackgun = d->gunselect;
		}
		else if(!d->attacking && d->inhandnade && attacktime>grenadepulltime) thrownade(d, d->inhandnade);
		return;
	}
	else
	{
        if(autoreload && d == player1 && !d->mag[d->gunselect] && lastmillis-d->lastaction>guns[d->gunselect].attackdelay) { reload(d); return; }
		if(d->weaponchanging || attacktime<d->gunwait) return;
		d->gunwait = 0;
		d->reloading = false;

		if(!d->attacking) { d->shots = 0; return; }
		d->lastaction = lastmillis;
		d->lastattackgun = d->gunselect;

		if(d->gunselect!=GUN_KNIFE && !hasammo(d)) { d->shots = 0; return; }
			
		if(guns[d->gunselect].isauto) d->shots++;
		else d->attacking = false;

		if(d->gunselect==GUN_PISTOL && d->akimbo) 
		{
			d->attacking = true;  // make akimbo auto
			d->akimbolastaction[akimboside?1:0] = lastmillis;
			akimboside = !akimboside;
		}
	    
		if(d->gunselect!=GUN_KNIFE) d->mag[d->gunselect]--;
		from.z -= 0.2f;    // below eye
        if(attacktime<d->gunwait) from = to;
	}
	
	spreadandrecoil(from,to,d);
    vec unitv;
    float dist = to.dist(from, unitv);
    unitv.div(dist);

	if(d->gunselect==GUN_KNIFE) 
	{
        unitv.mul(3); // punch range
		to = from;
        to.add(unitv);
	}   
	if(d->gunselect==GUN_SHOTGUN) createrays(from, to);
	
	if(has_akimbo(d)) d->gunwait = guns[d->gunselect].attackdelay / 2;  //make akimbo pistols shoot twice as fast as normal pistol
	else d->gunwait = guns[d->gunselect].attackdelay;
	
	shootv(d->gunselect, from, to, d, true, 0);
	if(d->type==ENT_PLAYER) addmsg(SV_SHOT, "ri8", d->gunselect, (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF), (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF), 0);

	if(guns[d->gunselect].projspeed || d->gunselect==GUN_GRENADE) return;

    raydamage(from, to, d);	
}
