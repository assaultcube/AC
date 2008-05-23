// entities.cpp: map entity related functions (pickup etc.)

#include "pch.h"
#include "cube.h"

const int physent::crouchtime = 200;

vector<entity> ents;

const char *entnames[] =
{   
    "none?", "light", "playerstart",
    "clips", "ammobox","grenades",
    "health", "armour", "akimbo",
    "mapmodel", "trigger", 
    "ladder", "ctf-flag", "sound", "?", "?",
};

const char *entmdlnames[] = 
{
	"pickups/pistolclips", "pickups/ammobox", "pickups/nades", "pickups/health", "pickups/kevlar", "pickups/akimbo",
};

void renderent(entity &e, const char *mdlname, float z, float yaw, int anim = ANIM_MAPMODEL|ANIM_LOOP, int basetime = 0, float speed = 0)
{
	rendermodel(mdlname, anim, 0, 1.1f, vec(e.x, e.y, z+S(e.x, e.y)->floor), yaw, 0, speed, basetime);
}

void renderentities()
{
    if(editmode) loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==NOTUSED) continue;
        vec v(e.x, e.y, e.z); 
        particle_splash(2, 2, 40, v);
    }
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==MAPMODEL)
        {
            mapmodelinfo &mmi = getmminfo(e.attr2);
            if(!&mmi) continue;
			rendermodel(mmi.name, ANIM_MAPMODEL|ANIM_LOOP, e.attr4, mmi.rad ? (float)mmi.rad : 1.1f, vec(e.x, e.y, (float)S(e.x, e.y)->floor+mmi.zoff+e.attr3), (float)((e.attr1+7)-(e.attr1+7)%15), 0, 10.0f);
        }
        else if(m_ctf && e.type==CTF_FLAG)
        {
            flaginfo &f = flaginfos[e.attr2];
            if(f.state==CTFF_STOLEN)
            {
				if(!f.actor || f.actor == player1) continue;
                s_sprintfd(path)("pickups/flags/small_%s", team_string(e.attr2));
                rendermodel(path, ANIM_FLAG|ANIM_START, 0, 1.1f, vec(f.actor->o).add(vec(0, 0, 0.3f+(sinf(lastmillis/100.0f)+1)/10)), lastmillis/2.5f, 0, 120.0f);
            }
            else
            {
                s_sprintfd(path)("pickups/flags/%s", team_string(e.attr2));
                rendermodel(path, ANIM_FLAG|ANIM_LOOP, 0, 4, vec(e.x, e.y, f.state==CTFF_INBASE ? (float)S(e.x, e.y)->floor : e.z), (float)((e.attr1+7)-(e.attr1+7)%15), 0, 120.0f);
            }
        }
        else if(isitem(e.type))
        {
            if(OUTBORD(e.x, e.y) || !e.spawned) continue;
            renderent(e, entmdlnames[e.type-I_CLIPS], (float)(1+sinf(lastmillis/100.0f+e.x+e.y)/20), lastmillis/10.0f);
        }
    }
}

// these two functions are called when the server acknowledges that you really
// picked up the item (in multiplayer someone may grab it before you).

void pickupeffects(int n, playerent *d)
{
    if(!ents.inrange(n)) return;
    entity &e = ents[n];
    e.spawned = false;
    if(!d) return;
    d->pickup(e.type);
    itemstat &is = d->itemstats(e.type);
    if(d!=player1 && d->type!=ENT_BOT) return;
    if(&is)
    {
        if(d==player1) playsoundc(is.sound);
        else playsound(is.sound, d);
    }

    weapon *w = NULL;
    switch(e.type)
    {
        case I_AKIMBO: w = d->weapons[GUN_AKIMBO]; break;
        case I_CLIPS: w = d->weapons[GUN_PISTOL]; break;
        case I_AMMO: w = d->primweap; break;
        case I_GRENADE: w = d->weapons[GUN_GRENADE]; break;
    }
    if(w) w->onammopicked();
}

// these functions are called when the client touches the item

void trypickup(int n, playerent *d)
{
    entity &e = ents[n];
    switch(e.type)
    {
        default:
            if(d->canpickup(e.type))
            {
                if(d->type==ENT_PLAYER) addmsg(SV_ITEMPICKUP, "ri", n);
                else if(d->type==ENT_BOT && serverpickup(n, -1)) pickupeffects(n, d);
                e.spawned = false;
            }
            break;

        case LADDER:
            if(!d->crouching) d->onladder = true;
            break;

        case CTF_FLAG:
        {
            if(d==player1)
            {
                int flag = e.attr2;
                flaginfo &f = flaginfos[flag];
                flaginfo &of = flaginfos[team_opposite(flag)];
                if(f.state == CTFF_STOLEN) break;

                if(flag == team_int(d->team)) // it's the own flag
                {
                    if(f.state == CTFF_DROPPED) flagreturn();
                    else if(f.state == CTFF_INBASE && of.state == CTFF_STOLEN && of.actor == d && of.ack) flagscore();
                }
                else flagpickup();
            }
            break;
        }
    }
}

void checkitems(playerent *d)
{
    if(editmode || d->state!=CS_ALIVE) return;
    d->onladder = false;
    float eyeheight = d->dyneyeheight();
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==NOTUSED) continue;
        if(e.type==LADDER)
        {
            if(OUTBORD(e.x, e.y)) continue;
            vec v(e.x, e.y, d->o.z);
            float dist1 = d->o.dist(v);
            float dist2 = d->o.z - (S(e.x, e.y)->floor+eyeheight);
            if(dist1<1.5f && dist2<e.attr1) trypickup(i, d);
            continue;
        }
        
        if(!e.spawned) continue;
        if(OUTBORD(e.x, e.y)) continue;

        if(e.type==CTF_FLAG && flaginfos[e.attr2].state==CTFF_DROPPED) // 3d collision for dropped ctf flags
        {
            vec v(e.x, e.y, e.z);
            if(objcollide(d, v, 2.5f, 4.0f)) trypickup(i, d);
        }
        else // simple 2d collision
        {
            vec v(e.x, e.y, S(e.x, e.y)->floor+eyeheight);
            if(d->o.dist(v)<2.5f) trypickup(i, d);
        }
    }
}

void putitems(ucharbuf &p)            // puts items in network stream and also spawns them locally
{
    loopv(ents) if(isitem(ents[i].type) || (multiplayer(false) && gamespeed!=100 && (i=-1)))
    {
		if(m_noitemsnade && ents[i].type!=I_GRENADE) continue;
		else if(m_pistol && ents[i].type==I_AMMO) continue;
        putint(p, i);
        putint(p, ents[i].type);
        ents[i].spawned = true;
    }
}

void resetspawns() 
{
	loopv(ents) ents[i].spawned = false;
	if(m_noitemsnade || m_pistol)
		loopv(ents)
		{
			entity &e = ents[i];
			if(m_noitemsnade && e.type == I_CLIPS) e.type = I_GRENADE;
			else if(m_pistol && e.type==I_AMMO) e.type = I_CLIPS;
		}
}
void setspawn(int i, bool on) { if(ents.inrange(i)) ents[i].spawned = on; }

void selectnextprimary(int num)
{
    switch(num)
    {
        case GUN_SHOTGUN:
        case GUN_SUBGUN:
        case GUN_SNIPER:
        case GUN_ASSAULT:
            player1->setnextprimary(num);
            addmsg(SV_PRIMARYWEAP, "ri", player1->nextprimweap->type);
            break;

        default:
            conoutf("this is not a valid primary weapon");
            break;
    }
}

COMMANDN(nextprimary, selectnextprimary, ARG_1INT);

// Added by Rick
bool intersect(entity *e, vec &from, vec &to, vec *end) // if lineseg hits entity bounding box(entity version)
{
    mapmodelinfo &mmi = getmminfo(e->attr2);
    if(!&mmi || !mmi.h) return false;
    
    float lo = (float)(S(e->x, e->y)->floor+mmi.zoff+e->attr3);
    float hi = lo+mmi.h;
    vec v = to, w(e->x, e->y, lo + (fabs(hi-lo)/2.0f)), *p; 
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
            v.mul(f);
            v.add(from);
            p = &v;
        }
    }
                        
    if (p->x <= e->x+mmi.rad
        && p->x >= e->x-mmi.rad
        && p->y <= e->y+mmi.rad
        && p->y >= e->y-mmi.rad
        && p->z <= hi
        && p->z >= lo)
     {
          if (end) *end = *p;
          return true;
     }
     return false;
}
// End add by Ricks

// flag ent actions done by the local player

int flagdropmillis = 0;

void flagpickup()
{
    if(flagdropmillis && flagdropmillis>lastmillis) return;
	flaginfo &f = flaginfos[team_opposite(team_int(player1->team))];
	f.flag->spawned = false;
	f.state = CTFF_STOLEN;
	f.actor = player1; // do this although we don't know if we picked the flag to avoid getting it after a possible respawn
	f.actor_cn = getclientnum();
	f.ack = false;
	addmsg(SV_FLAGPICKUP, "ri", f.team);
}

void tryflagdrop(bool reset)
{
	flaginfo &f = flaginfos[team_opposite(team_int(player1->team))];
	if(f.state==CTFF_STOLEN && f.actor==player1)
    {
        f.flag->spawned = false;
        f.state = CTFF_DROPPED;
		f.ack = false;
        flagdropmillis = lastmillis+3000;
		addmsg(reset ? SV_FLAGRESET : SV_FLAGDROP, "ri", f.team);
    }
}

void flagreturn()
{
	flaginfo &f = flaginfos[team_int(player1->team)];
	f.flag->spawned = false;
	f.ack = false;
	addmsg(SV_FLAGRETURN, "ri", f.team);
}

void flagscore()
{
	flaginfo &f = flaginfos[team_opposite(team_int(player1->team))];
	f.ack = false;
	addmsg(SV_FLAGSCORE, "ri", f.team);
}

// flag ent actions from the net

void flagstolen(int flag, int action, int act)
{
	playerent *actor = act == getclientnum() ? player1 : getclient(act);
	if(!actor) return;
	flaginfo &f = flaginfos[flag];
	f.actor = actor;
	f.actor_cn = act;
	f.flag->spawned = false;
	f.ack = true;
	flagmsg(flag, action);
}

void flagdropped(int flag, int action, short x, short y, short z)
{
	flaginfo &f = flaginfos[flag];
    if(OUTBORD(x, y)) return; // valid pos
    bounceent p;
    p.rotspeed = 0.0f;
    p.o.x = x;
    p.o.y = y;
    p.o.z = z;
    p.vel.z = -0.8f;
    loopi(50) moveplayer(&p, 50, true); // calc drop position
    f.flag->x = (short)p.o.x;
    f.flag->y = (short)p.o.y;
    f.flag->z = (short)p.o.z;
    if(f.flag->z < hdr.waterlevel) f.flag->z = (short) hdr.waterlevel;
	f.flag->spawned = true;
	f.ack = true;
	flagmsg(flag, action);
}

void flaginbase(int flag, int action, int act)
{
	flaginfo &f = flaginfos[flag];
	playerent *actor = act == getclientnum() ? player1 : getclient(act);
	if(actor) { f.actor = actor; f.actor_cn = act; }
	f.flag->x = (ushort) f.originalpos.x;
	f.flag->y = (ushort) f.originalpos.y;
	f.flag->z = (ushort) f.originalpos.z;
	f.flag->spawned = true;
	f.ack = true;
	flagmsg(flag, action);
}

