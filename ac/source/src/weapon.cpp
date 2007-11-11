// weapon.cpp: all shooting and effects code

#include "cube.h"
#include "bot/bot.h"

vec sg[SGRAYS];

char *gunnames[NUMGUNS] = { "knife", "pistol", "shotgun", "subgun", "sniper", "assault", "grenade" };

int nadetimer = 2000; // detonate after $ms

void updatelastaction(playerent *d, int millis = 0, bool akimbo = false)
{
    millis += lastmillis;
    loopi(NUMGUNS) if(d->gunwait[i]) d->gunwait[i] = max(d->gunwait[i] - (millis-d->lastaction), 0);
    d->lastaction = millis;
    if(akimbo) loopi(2) d->akimbolastaction[i] = millis;
}

void checkweaponswitch()
{
	if(!player1->weaponchanging) return;
    int timeprogress = lastmillis-player1->weaponchanging;
	if(timeprogress>WEAPONCHANGE_TIME) 
	{
        addmsg(SV_WEAPCHANGE, "ri", player1->gunselect);
		player1->weaponchanging = 0;
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
    player1->weaponchanging = lastmillis;
	player1->thrownademillis = 0;
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
	if(!d || d->state!=CS_ALIVE || d->reloading || d->weaponchanging) return;
	if(!reloadable_gun(d->gunselect) || d->ammo[d->gunselect]<=0) return;
	if(d->mag[d->gunselect] >= (has_akimbo(d) ? 2 : 1)*guns[d->gunselect].magsize) return;
	if(d == player1) setscope(false);

    updatelastaction(d, 0, true);
    d->reloading = lastmillis;
    d->gunwait[d->gunselect] += guns[d->gunselect].reloadtime;
    
    int numbullets = (has_akimbo(d) ? 2 : 1)*guns[d->gunselect].magsize - d->mag[d->gunselect];
	if(numbullets > d->ammo[d->gunselect]) numbullets = d->ammo[d->gunselect];
	d->mag[d->gunselect] += numbullets;
	d->ammo[d->gunselect] -= numbullets;

    if(has_akimbo(d)) playsoundc(S_RAKIMBO);
	else if(d->type==ENT_BOT) playsound(guns[d->gunselect].reload, d);
	else playsoundc(guns[d->gunselect].reload);

    if(d==player1) addmsg(SV_RELOAD, "ri2", lastmillis, d->gunselect);
}

void selfreload() { reload(player1); }
COMMANDN(reload, selfreload, ARG_NONE);

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

static inline bool intersect(const vec &o, const vec &rad, const vec &from, const vec &to, vec *end) // if lineseg hits entity bounding box
{
    const vec *p;
    vec v = to, w = o; 
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

    if(p->x <= o.x+rad.x
       && p->x >= o.x-rad.x
       && p->y <= o.y+rad.y
       && p->y >= o.y-rad.y
       && p->z <= o.z+rad.z
       && p->z >= o.z-rad.z)
    {
        if(end) *end = *p;
        return true;
    }
    return false;
}

bool intersect(dynent *d, const vec &from, const vec &to, vec *end)
{
    vec o(d->o);
    o.z += (d->aboveeye - d->dyneyeheight())/2;
    return intersect(o, vec(d->radius, d->radius, (d->aboveeye + d->dyneyeheight())/2), from, to, end);
}

bool intersect(entity *e, const vec &from, const vec &to, vec *end)
{
    mapmodelinfo &mmi = getmminfo(e->attr2);
    if(!&mmi || !mmi.h) return false;

    float lo = float(S(e->x, e->y)->floor+mmi.zoff+e->attr3);
    return intersect(vec(e->x, e->y, lo+mmi.h/2.0f), vec(mmi.rad, mmi.rad, mmi.h/2.0f), from, to, end);
}

playerent *intersectclosest(vec &from, vec &to, playerent *at)
{
    playerent *best = NULL;
    float bestdist = 1e16f;
    if(at!=player1 && player1->state==CS_ALIVE && intersect(player1, from, to))
    {
        best = player1;
        bestdist = at->o.dist(player1->o);
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
        }
    }
    return best;
}

playerent *playerincrosshair()
{
    return intersectclosest(player1->o, worldpos, player1);
}

struct projectile { vec o, to; float speed; playerent *owner; int gun; bool local; int id; };
vector<projectile > projs;

void projreset() { projs.setsize(0); }

void newprojectile(vec &from, vec &to, float speed, bool local, playerent *owner, int gun, int id = lastmillis)
{
    projectile &p = projs.add();
    p.o = from;
    p.to = to;
    p.speed = speed;
    p.local = local;
    p.owner = owner;
    p.gun = gun;
    p.id = id;
}

void removeprojectiles(playerent *owner)
{
    loopv(projs) if(projs[i].owner==owner) projs.remove(i--);
}
    
void damageeffect(int damage, playerent *d)
{
    particle_splash(3, damage/10, 1000, d->o);
}

struct hitmsg
{
    int target, lifesequence, info;
    ivec dir;
};
vector<hitmsg> hits;

void hit(int damage, playerent *d, playerent *at, const vec &vel, int gun, bool gib, int info)
{
    if(d==player1 || d->type==ENT_BOT || !m_mp(gamemode)) d->hitpush(damage, vel, at, gun);

    if(!m_mp(gamemode)) dodamage(damage, d, at, gib);
    else
    {
        hitmsg &h = hits.add();
        h.target = d->clientnum;
        h.lifesequence = d->lifesequence;
        h.info = info;
        if(d==player1)
        {
            h.dir = ivec(0, 0, 0);
            d->damageroll(damage);
            damageblend(damage);
            playsound(S_PAIN6);
        }
        else 
        {
            h.dir = ivec(int(vel.x*DNF), int(vel.y*DNF), int(vel.z*DNF));
            damageeffect(damage, d);
            playsound(S_PAIN1+rnd(5), d);
        }
    }
}

void hitpush(int damage, playerent *d, playerent *at, vec &from, vec &to, int gun, bool gib, int info)
{
    vec v(to);
    v.sub(from);
    v.normalize();
    hit(damage, d, at, v, gun, gib, info);
}

float expdist(playerent *o, vec &dir, const vec &v)
{
    vec middle = o->o;
    middle.z += (o->aboveeye-o->dyneyeheight())/2;
    float dist = middle.dist(v, dir);
    dir.div(dist);
    if(dist<0) dist = 0;
    return dist;
}
 
void radialeffect(playerent *o, vec &v, int qdam, playerent *at, int gun)
{
    if(o->state!=CS_ALIVE) return;
    vec dir;
    float dist = expdist(o, dir, v);
    if(dist<EXPDAMRAD) 
    {
        int damage = (int)(qdam*(1-dist/EXPDAMRAD));
        hit(damage, o, at, dir, gun, true, int(dist*DMF)); 
    }
}

void splash(projectile &p, vec &v, vec &vold, playerent *notthis, int qdam)
{
    particle_splash(0, 50, 300, v);
    if(p.gun!=GUN_GRENADE)
    {
        playsound(S_FEXPLODE, NULL, NULL, &v);
        // no push?
    }
    else
    {
        //playsound(S_RLHIT, &v);
        particle_fireball(5, v); 
        addscorchmark(v);
        dodynlight(vold, v, 0, 0, p.owner);
        if(!p.local) return;
        radialeffect(player1, v, qdam, p.owner, p.gun);
        loopv(players)
        {
            playerent *o = players[i];
            if(!o || o==notthis) continue; 
            radialeffect(o, v, qdam, p.owner, p.gun);
        }
    }
}

bool projdamage(playerent *o, projectile &p, vec &v, int qdam)
{
    if(o->state!=CS_ALIVE || !intersect(o, p.o, v)) return false;
    splash(p, v, p.o, o, qdam);
    vec dir;
    expdist(o, dir, v);
    hit(qdam, o, p.owner, dir, p.gun, true, 0);
    return true;
}

void moveprojectiles(float time)
{
    loopv(projs)
    {
        projectile &p = projs[i];
        int qdam = guns[p.gun].damage;
        vec v;
        float dist = p.to.dist(p.o, v);
        float dtime = dist*1000/p.speed;
        if(time>dtime) dtime = time;
        v.mul(time/dtime).add(p.o);
        bool exploded = false;
        hits.setsizenodelete(0);
        if(p.local)
        {
            loopvj(players)
            {
                playerent *o = players[j];
                if(!o || p.owner==o || o->o.reject(v, 10.0f)) continue; 
                if(projdamage(o, p, v, qdam)) exploded = true;
            }
            if(p.owner!=player1 && projdamage(player1, p, v, qdam)) exploded = true;
        }
        if(!exploded)
        {
            if(time==dtime) 
            {
                splash(p, v, p.o, NULL, qdam);
                exploded = true;
            }
            else
            {
                if(p.gun==GUN_GRENADE) { dodynlight(p.o, v, 0, 255, p.owner); particle_splash(5, 2, 200, v); }
                else { particle_splash(1, 1, 200, v); particle_splash(guns[p.gun].part, 1, 1, v); }
                // Added by Rick
                traceresult_s tr;
                TraceLine(p.o, v, p.owner, true, &tr);
                if(tr.collided) splash(p, v, p.o, NULL, qdam);
                // End add                
            }       
        }
        if(exploded)
        {
            if(p.local)
                addmsg(SV_EXPLODE, "ri3iv", lastmillis, p.gun, p.id,
                    hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
            projs.remove(i--);
        }
        else p.o = v;
    }
}

vector<bounceent *> bounceents;

bounceent *newbounceent()
{
    bounceent *p = new bounceent;
    bounceents.add(p);
    return p;
}

void removebounceents(playerent *owner)
{
    loopv(bounceents) if(bounceents[i]->owner==owner) { delete bounceents[i]; bounceents.remove(i--); }
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
        vec o(p->o);

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
                    o.z -= t*t/4000000000.0f*t;
                }
                break;
            }
        }
        path(model);
        rendermodel(model, ANIM_MAPMODEL|ANIM_LOOP, 0, 1.1f, o, p->yaw+90, p->pitch);
    }
}

VARP(gibnum, 0, 6, 1000); 
VARP(gibttl, 0, 5000, 15000);
VARP(gibspeed, 1, 30, 100);

void addgib(playerent *d)
{   
    if(!d) return;
    playsound(S_GIB, d);

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
        updatelastaction(player1);
        addmsg(SV_THROWNADE, "ri7", int(d->o.x*DMF), int(d->o.y*DMF), int(d->o.z*DMF), int(vel.x*DMF), int(vel.y*DMF), int(vel.z*DMF), lastmillis-p->millis);
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

bounceent *newnade(playerent *d, int millis = 0)
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
	if(d==player1) playsoundc(S_GRENADEPULL);
    return p;
}

void explode_nade(bounceent *i)
{ 
    if(!i) return;
    if(i->bouncestate != NADE_THROWED) thrownade(i->owner, vec(0,0,0), i);
    playsound(S_FEXPLODE, NULL, NULL, &i->o);
    newprojectile(i->o, i->o, 1, i->owner==player1, i->owner, GUN_GRENADE, i->millis);
}

void shootv(int gun, vec &from, vec &to, playerent *d, bool local, int nademillis)     // create visual effect from a shot
{
    if(guns[gun].sound) playsound(guns[gun].sound, d==player1 ? NULL : d);
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
            break;
		}

        case GUN_GRENADE:
		{
			if(d!=player1)
			{
				bounceent *p = newnade(d, nademillis);
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

void shorten(vec &from, vec &to, vec &target)
{
    target.sub(from).normalize().mul(from.dist(to)).add(from);
}

const float HEADSIZE = 0.8f;
vec hitpos;

void raydamage(vec &from, vec &to, playerent *d)
{
    int gdam = guns[d->gunselect].damage;
    playerent *o = NULL;
    if(d->gunselect==GUN_SHOTGUN)
    {
        uint done = 0;
        playerent *cl = NULL;
        for(;;)
        {
            bool raysleft = false;
            int hitrays = 0;
            o = NULL;
            loop(r, SGRAYS) if((done&(1<<r))==0 && (cl = intersectclosest(from, sg[r], d)))
            {
                if(!o || o==cl)
                {
                    hitrays++;
                    o = cl;
                    done |= 1<<r;
                    shorten(from, o->o, sg[r]);
                }
                else raysleft = true;
            }
            if(hitrays) hitpush(hitrays*gdam, o, d, from, to, d->gunselect, false, hitrays);
            if(!raysleft) break;
        }
    }
    else if((o = intersectclosest(from, to, d)))
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

        hitpush(gdam, o, d, from, to, d->gunselect, gib, gib ? 1 : 0);
        shorten(from, o->o, to);
    }
}

VAR(testrecoil, 0, 100, 200);

void spreadandrecoil(vec &from, vec &to, playerent *d)
{
    if(d->gunselect==GUN_KNIFE || d->gunselect==GUN_GRENADE) return; //nothing special for a knife

    vec unitv;
    float dist = to.dist(from, unitv);
    float f = dist/1000;
    int spd = guns[d->gunselect].spread;
    float rcl = guns[d->gunselect].recoil*-0.01f;

    if(d->gunselect==GUN_ASSAULT)
    {
        if(d->shots > 3)
            spd = 70;
        rcl += (rnd(8)*-0.01f);
    }
   
    if((d->gunselect==GUN_SNIPER) && scoped)
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
    d->vel.add(vec(unitv).mul(rcl/dist).mul(d->crouching ? 0.5 : 1.0f));
    if(d->pitch < 80.0f) d->pitchvel += guns[d->gunselect].recoil*0.15f*(float)testrecoil/100.0f;
    //if(d->pitch<80.0f) d->pitch += guns[d->gunselect].recoil*0.05f;
}

bool hasammo(playerent *d) 	// bot mod
{
	if(!d->mag[d->gunselect])
	{
		if(d->type==ENT_BOT) playsound(S_NOAMMO, d);
		else playsoundc(S_NOAMMO);
        updatelastaction(d);
		d->gunwait[d->gunselect] += 250;
		d->lastattackgun = -1;
		return false;
	} else return true;
}

VARP(autoreload, 0, 1, 1);

bool akimboside = false;

void shoot(playerent *d, vec &targ)
{
	int attacktime = lastmillis-d->lastaction;

	vec from = d->o;
	vec to = targ;

	if(d->gunselect==GUN_GRENADE)
	{
		d->shots = 0;

		if(d->thrownademillis && attacktime >= NADE_THROW_TIME)
		{
			d->weaponchanging = lastmillis-1-WEAPONCHANGE_TIME/2;
			d->nextweapon = d->mag[GUN_GRENADE] ? GUN_GRENADE : d->primary;
			d->thrownademillis = 0;
		}

		if(d->weaponchanging || attacktime<d->gunwait[d->gunselect]) return;
		d->gunwait[d->gunselect] = 0;
		d->reloading = 0;

		if(d->attacking && !d->inhandnade) // activate
		{
			if(!hasammo(d)) return;
			d->mag[d->gunselect]--;
			newnade(d);
            updatelastaction(d);
			d->gunwait[d->gunselect] = attackdelay(d->gunselect);
			d->lastattackgun = d->gunselect;

            addmsg(SV_SHOOT, "ri2i6i", lastmillis, d->gunselect,
                   (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF), 
                   (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF),
                   0);
		}
		else if(!d->attacking && d->inhandnade && attacktime>attackdelay(d->gunselect)) thrownade(d, d->inhandnade);
		return;
	}
	else
	{
        if(autoreload && d == player1 && !d->mag[d->gunselect] && lastmillis-d->lastaction>guns[d->gunselect].attackdelay) { reload(d); return; }
		if(d->weaponchanging || attacktime<d->gunwait[d->gunselect]) return;
		d->gunwait[d->gunselect] = 0;
		d->reloading = 0;

		if(!d->attacking) { d->shots = 0; return; }
        updatelastaction(d);
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

    hits.setsizenodelete(0);

    if(!guns[d->gunselect].projspeed) raydamage(from, to, d);
	
    d->gunwait[d->gunselect] = attackdelay(d->gunselect);
	if(has_akimbo(d)) d->gunwait[d->gunselect] /= 2; // make akimbo pistols shoot twice as fast as normal pistol
	
	shootv(d->gunselect, from, to, d, true, 0);
    if(d==player1)
    {
        addmsg(SV_SHOOT, "ri2i6iv", lastmillis, d->gunselect,
	           (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF), 
               (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF), 
               hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
    }
}

