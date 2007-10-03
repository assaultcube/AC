// entities.cpp: map entity related functions (pickup etc.)

#include "cube.h"

vector<entity> ents;

char *entnames[] =
{   
    "none?", "light", "playerstart",
    "clips", "ammobox","grenades",
    "health", "armour", "akimbo",
    "mapmodel", "trigger", 
    "ladder", "ctf-flag", "sound", "?", "?",
};
char *entmdlnames[] = 
{
//FIXME : fix the "pickups" infront
	"pickups/pistolclips", "pickups/ammobox", "pickups/nades", "pickups/health", "pickups/kevlar", "pickups/akimbo",
};

void renderent(entity &e, char *mdlname, float z, float yaw, int anim = ANIM_MAPMODEL|ANIM_LOOP, int basetime = 0, float speed = 0)
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
    if(d!=player1 && d->type!=ENT_BOT) return;
    d->pickup(e.type);
    itemstat &is = d->itemstats(e.type);
    if(&is)
    {
        if(d==player1) playsoundc(is.sound);
        else playsound(is.sound, &d->o);
    }
    switch(e.type)
    {
        case I_GRENADE:
            d->thrownademillis = 0;
            break;

        case I_AKIMBO:
            d->akimbomillis = lastmillis+30000;
            if(d==player1)
            {
                if(d->gunselect!=GUN_SNIPER && !d->inhandnade) weaponswitch(GUN_PISTOL);
                addmsg(SV_AKIMBO, "ri", lastmillis);
            }
            break;
    }
}

// these functions are called when the client touches the item

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
            d->onladder = true;
            break;

        case CTF_FLAG:
        {
            if(d==player1)
            {
                int flag = e.attr2;
                flaginfo &f = flaginfos[flag];
                flaginfo &of = flaginfos[team_opposite(flag)];
                if(f.state == CTFF_STOLEN) break;

                if(flag == team_int(d->team)) // its the own flag
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

#if 0
void additem(playerent *d, int i, int &v, int spawnsec, int t)
{
	if(v<itemstats[t].max) 
	{
		if(d->type==ENT_PLAYER) addmsg(SV_ITEMPICKUP, "rii", i, spawnsec);
		else if(d->type==ENT_BOT && serverpickup(i, spawnsec, -1)) realpickup(i, d);
		ents[i].spawned = false;
	}
}

void pickup(int n, playerent *d)
{
    int np = 1;
    loopv(players) if(players[i]) np++;
    np = np<3 ? 4 : (np>4 ? 2 : 3);         // spawn times are dependent on number of players
    int ammo = np*2;
    switch(ents[n].type)
    {
        case I_CLIPS: 
            additem(d, n, d->ammo[1], ammo, 1);
            break;
    	case I_AMMO: 
            additem(d, n, d->ammo[d->primary], ammo, d->primary); 
            break;
    	case I_GRENADE: additem(d, n, d->mag[6], ammo, 6); break;
        case I_HEALTH:  additem(d, n, d->health,  np*5, 7); break;

        case I_ARMOUR:
            additem(d, n, d->armour, 20, 8);
            break;

        case I_AKIMBO:
            additem(d, n, d->akimbo, 60, 9);
            break;
            
        case LADDER:
            d->onladder = true;
            break;

        case CTF_FLAG:
        {
			if(d==player1)
			{
				int flag = ents[n].attr2;
				flaginfo &f = flaginfos[flag];
				flaginfo &of = flaginfos[team_opposite(flag)];
				if(f.state == CTFF_STOLEN) break;
	            
				if(flag == team_int(d->team)) // its the own flag
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
#endif

void checkitems(playerent *d)
{
    if(editmode) return;
    d->onladder = false;
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==NOTUSED) continue;
        if(e.type==LADDER)
        {
            if(OUTBORD(e.x, e.y)) continue;
            vec v(e.x, e.y, d->o.z);
            float dist1 = d->o.dist(v);
            float dist2 = d->o.z - (S(e.x, e.y)->floor+d->eyeheight);
            if(dist1<1.5f && dist2<e.attr1) trypickup(i, d);
            continue;
        }
        
        if(!e.spawned) continue;
        if(OUTBORD(e.x, e.y)) continue;
        vec v(e.x, e.y, S(e.x, e.y)->floor+d->eyeheight);
        if(d->o.dist(v)<2.5f) trypickup(i, d);
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

void item(int num)
{
    switch(num)
    {
        case GUN_SHOTGUN:
        case GUN_SUBGUN:
        case GUN_SNIPER:
        case GUN_ASSAULT:
            player1->nextprimary = num;
            addmsg(SV_PRIMARYWEAP, "ri", player1->nextprimary);
            break;

        default:
            conoutf("sorry, you can't use that item yet");
            break;
    }
}

COMMAND(item,ARG_1INT);

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
        flagdropmillis = lastmillis+1000;
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
    if(OUTBORD(x, y)) return;
    f.flag->x = x;
    f.flag->y = y;
    f.flag->z = (short)floor(x, y);
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

