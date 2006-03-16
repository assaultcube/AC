// entities.cpp: map entity related functions (pickup etc.)

#include "cube.h"

vector<entity> ents;

char *entmdlnames[] = 
{
//FIXME : fix the "pickups" infront
	"pickups/pistolclips", "pickups/ammobox", "ammobox" /*grenade*/, "pickups/health", "pickups/kevlar", "pickups/duals" /*dual pistols*/,
};

int triggertime = 0;

void renderent(entity &e, char *mdlname, float z, float yaw, int frame = 0, int numf = 1, int basetime = 0, float speed = 10.0f)
{
	rendermodel(mdlname, frame, numf, 0, 1.1f, e.x, z+S(e.x, e.y)->floor, e.y, yaw, 0, false, 1.0f, speed, 0, basetime);
};

void renderentities()
{
    if(lastmillis>triggertime+1000) triggertime = 0;
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==MAPMODEL)
        {
            mapmodelinfo &mmi = getmminfo(e.attr2);
            if(!&mmi) continue;
			rendermodel(mmi.name, 0, 1, e.attr4, (float)mmi.rad, e.x, (float)S(e.x, e.y)->floor+mmi.zoff+e.attr3, e.y, (float)((e.attr1+7)-(e.attr1+7)%15), 0, false, 1.0f, 10.0f, mmi.snap);
        }
        else
        {
            if(OUTBORD(e.x, e.y)) continue;
            if(e.type!=CARROT)
            {
				if(!e.spawned) continue;
				if(e.type<I_CLIPS || e.type>I_QUAD) continue;
                                renderent(e, entmdlnames[e.type-I_CLIPS], (float)(1+sin(lastmillis/100.0+e.x+e.y)/20), lastmillis/10.0f);
            }
			else switch(e.attr2)
            {			
				case 1:
				case 3:
					continue;
					
                case 2: 
                case 0:
					if(!e.spawned) continue;
					renderent(e, "carrot", (float)(1+sin(lastmillis/100.0+e.x+e.y)/20), lastmillis/(e.attr2 ? 1.0f : 10.0f));
					break;
					
                case 4: renderent(e, "switch2", 3,      (float)e.attr3*90, (!e.spawned && !triggertime) ? 1  : 0, (e.spawned || !triggertime) ? 1 : 2,  triggertime, 1050.0f);  break;
                case 5: renderent(e, "switch1", -0.15f, (float)e.attr3*90, (!e.spawned && !triggertime) ? 30 : 0, (e.spawned || !triggertime) ? 1 : 30, triggertime, 35.0f); break;
            }; 
        };
    };
};

struct itemstat { int add, start, max, sound; } itemstats[] =
{
     16,   24,    36, S_ITEMAMMO,   //pistol
     14,    7,     21, S_ITEMAMMO,   //shotgun
     60,   60,    90, S_ITEMAMMO,   //subgun
     10,    10,    15, S_ITEMAMMO,   //sniper
     40,   20,    60, S_ITEMAMMO,   //assult
     1,    0,     2,  S_ITEMAMMO,   //grenade
    33,   100,    100, S_ITEMHEALTH, //health
    50,   100,    100, S_ITEMARMOUR, //armour
  20000,    0,    30000, S_ITEMPUP,    //powerup
};

void baseammo(int gun) { player1->ammo[gun] = itemstats[gun-1].add*2; };

// these two functions are called when the server acknowledges that you really
// picked up the item (in multiplayer someone may grab it before you).

/*
void radditem(int i, int &v)
{
    itemstat &is = itemstats[ents[i].type-I_CLIPS];
    ents[i].spawned = false;
    v += is.add;
    if(v>is.max) v = is.max;
    playsoundc(is.sound);
};
*/

void radditem(int i, int &v, int t)
{
    itemstat &is = itemstats[t-1];
    ents[i].spawned = false;
    v += is.add;
    if(v>is.max) v = is.max;
    playsoundc(is.sound);
};

void realpickup(int n, dynent *d)
{
    switch(ents[n].type)
    {
	//case I_PISTOL: radditem(n, d->ammo[1]); break;
        //case I_SHOTGUN:  radditem(n, d->ammo[2]); break;
        //case I_SUBGUN: radditem(n, d->ammo[3]); break;
        //case I_SNIPER: radditem(n, d->ammo[4]); break;
        //case I_ASSULT:  radditem(n, d->ammo[5]); break;
        case I_CLIPS: radditem(n, d->ammo[1], 1); break;
        case I_AMMO: radditem(n, d->ammo[d->primary], d->primary); break;
	case I_GRENADE: radditem(n, d->ammo[6], 6); break;
        case I_HEALTH:  radditem(n, d->health, 7);  break;

        case I_ARMOUR:
            radditem(n, d->armour, 8);
            //d->hasarmour = true;
            break;

        case I_QUAD:
            //radditem(n, d->quadmillis, 9);
            conoutf("a lesser man would use a single pistol");
            break;
    };
};

// these functions are called when the client touches the item

/*
void additem(int i, int &v, int spawnsec)
{
    if(v<itemstats[ents[i].type-I_CLIPS].max)                              // don't pick up if not needed
    {
        //addmsg(1, 3, SV_ITEMPICKUP, i, m_classicsp ? 100000 : spawnsec);    // first ask the server for an ack
        addmsg(1, 3, SV_ITEMPICKUP, i, spawnsec);    // first ask the server for an ack
        ents[i].spawned = false;                                            // even if someone else gets it first
    };
};
*/

void additem(int i, int &v, int spawnsec, int t)
{
      if(v<itemstats[t-1].max) 
      {
            addmsg(1, 3, SV_ITEMPICKUP, i, spawnsec);
            ents[i].spawned = false;
      };
};

void pickup(int n, dynent *d)
{
    int np = 1;
    loopv(players) if(players[i]) np++;
    np = np<3 ? 4 : (np>4 ? 2 : 3);         // spawn times are dependent on number of players
    int ammo = np*2;
    switch(ents[n].type)
    {
        //case I_PISTOL: additem(n, d->ammo[1], ammo); break;
	//case I_SHOTGUN:  additem(n, d->ammo[2], ammo); break;
        //case I_SUBGUN: additem(n, d->ammo[3], ammo); break;
        //case I_SNIPER: additem(n, d->ammo[4], ammo); break;
        //case I_ASSULT:  additem(n, d->ammo[5], ammo); break;
        case I_CLIPS: additem(n, d->ammo[1], ammo, 1); break;
        case I_AMMO: additem(n, d->ammo[d->primary], ammo, d->primary); break;
	case I_GRENADE: additem(n, d->ammo[6], ammo, 6); break;
        case I_HEALTH:  additem(n, d->health,  np*5, 7); break;

        case I_ARMOUR:
            additem(n, d->armour, 20, 8);
            break;

        case I_QUAD:
            //additem(n, d->quadmillis, 60, 9);
            break;
            
        case CARROT:
            ents[n].spawned = false;
            triggertime = lastmillis;
            trigger(ents[n].attr1, ents[n].attr2, false);  // needs to go over server for multiplayer
            break;
            
        case LADDER:
        {
            d->onladder = true;
            break;
        };
    };
};

void checkitems()
{
    if(editmode) return;
    player1->onladder = false;
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==NOTUSED) continue;
        if(e.type==LADDER)
        {
            if(OUTBORD(e.x, e.y)) continue;
            vec v = { e.x, e.y, player1->o.z };
            vdist(dist1, t, player1->o, v);
            float dist2 = player1->o.z - (S(e.x, e.y)->floor+player1->eyeheight);
            if(dist1<1.5 && dist2<e.attr1) pickup(i, player1);
            continue;
        };
        
        if(!e.spawned) continue;
        if(OUTBORD(e.x, e.y)) continue;
        vec v = { e.x, e.y, S(e.x, e.y)->floor+player1->eyeheight };
        vdist(dist, t, player1->o, v);
        if(dist<2.5) pickup(i, player1);
    };
};

void putitems(uchar *&p)            // puts items in network stream and also spawns them locally
{
    loopv(ents) if((ents[i].type>=I_CLIPS && ents[i].type<=I_QUAD) || ents[i].type==CARROT)
    {
        putint(p, i);
        ents[i].spawned = true;
    };
};

void resetspawns() { loopv(ents) ents[i].spawned = false; };
void setspawn(int i, bool on) { if(i<ents.length()) ents[i].spawned = on; };

void radd(dynent *d)
{
      loopi(NUMGUNS) if(d->nextprimary!=i) d->ammo[i] = 0;
      d->mag[GUN_KNIFE] = 1;

      d->mag[GUN_PISTOL] = 8;
      d->ammo[GUN_PISTOL] = 24;

      if (m_pistol || d->primary==GUN_PISTOL)  // || pistol only mode!
      {
           d->primary = GUN_PISTOL;
           d->ammo[d->primary] = 72;
      }
      else if (d->primary>GUN_PISTOL && d->primary<GUN_GRENADE)
      {
            if (m_arena)
                  d->ammo[d->primary] = itemstats[d->primary-1].max;
            else 
                  d->ammo[d->primary] = itemstats[d->primary-1].start;

            d->mag[d->primary] = itemstats[d->primary-1].add;
      }
      
/*
      if (d->primary==GUN_GRENADE)
      {
            //conoutf("you don't have to worry about blowing your hand off just yet...");
      }
*/
      if (d->hasarmour)
      {
            if(gamemode==m_arena)
                  d->armour = 100;
      }

      d->gunselect = d->primary;  //draw main weapon
};

void item(int num)
{
      if (num>0 && num<7) player1->nextprimary = num;

      //if (num==6) conoutf("sorry, no nades yet");

      if (num==7) { player1->hasarmour=!player1->hasarmour;  /*toggle armour*/ };

};

void weapon(int num)
{
    if(num>0 && num<7) 
    {    
        player1->nextprimary = num;
        player1->gunselect = num;
        player1->primary = num;
        
        radd(player1);
    }
    gun_changed = true;
}

COMMAND(weapon, ARG_1INT);
COMMAND(item,ARG_1INT);
