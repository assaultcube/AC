// entities.cpp: map entity related functions (pickup etc.)

#include "cube.h"

vector<entity> ents;

//fill in 2nd rrounds wth grenade, 3rd with plasma
char *entmdlnames[] = 
{
	"shells", "bullets", "rockets", "rrounds", "rrounds", "rrounds", "health", "armour", "quad",     
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
				if(e.type<I_AMMO || e.type>I_QUAD) continue;
				renderent(e, entmdlnames[e.type-I_AMMO], (float)(1+sin(lastmillis/100.0+e.x+e.y)/20), lastmillis/10.0f);
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

//edit these items
// struct itemstat { int add, max, sound; } itemstats[] = Modified by Rick
itemstat itemstats[] =
{
     10,    50, S_ITEMAMMO, //semipistol
     20,   100, S_ITEMAMMO, //autopistol
      5,    25, S_ITEMAMMO, //shotgun
      5,    25, S_ITEMAMMO,  //sniper
     25,   100, S_ITEMAMMO,  //subgun
     50,   200, S_ITEMAMMO,  //carbine
    100,   100, S_ITEMAMMO,  //semigun
    150,   150, S_ITEMAMMO,  //autorifle
      1,     1, S_ITEMAMMO,  //grenade
    150,   150, S_ITEMARMOUR,  //helmet
    100,   100, S_ITEMARMOUR,  //armour
  20000, 30000, S_ITEMPUP,  //bomb
  
  
};

void baseammo(int gun) { player1->ammo[gun] = itemstats[gun-1].add*2; };

// Added by Rick: baseammo for bots
void botbaseammo(int gun, dynent *d) { d->ammo[gun] = itemstats[gun-1].add*2; };
// End add

// these two functions are called when the server acknowledges that you really
// picked up the item (in multiplayer someone may grab it before you).

void radditem(int i, int &v)
{
    itemstat &is = itemstats[ents[i].type-I_AMMO];
    ents[i].spawned = false;
    v += is.add;
    if(v>is.max) v = is.max;
    playsoundc(is.sound);
};

void realpickup(int n, dynent *d)
{
    switch(ents[n].type)
    {
/*	case I_PISTOL: radditem(n, d->ammo[1]); break;
        
        case I_AMMO:  radditem(n, d->ammo[2]); break;
        case I_SUBGUN: radditem(n, d->ammo[3]); break;
        case I_SNIPER: radditem(n, d->ammo[4]); break;
        case I_ASSAULT:  radditem(n, d->ammo[5]); break;
	case I_GRENADE: radditem(n, d->ammo[6]); break;
*/
        case I_HEALTH:  radditem(n, d->health);  break;

        case I_ARMOUR:
            radditem(n, d->armour);
            d->hasarmour = true;
            break;

        case I_QUAD:
            radditem(n, d->quadmillis);
            conoutf("you got the quad!");
            break;
    };
};

// these functions are called when the client touches the item

void additem(int i, int &v, int spawnsec)
{
    if(v<itemstats[ents[i].type-I_AMMO].max)                              // don't pick up if not needed
    {
        //addmsg(1, 3, SV_ITEMPICKUP, i, m_classicsp ? 100000 : spawnsec);    // first ask the server for an ack
        addmsg(1, 3, SV_ITEMPICKUP, i, 10000);    // first ask the server for an ack
        ents[i].spawned = false;                                            // even if someone else gets it first
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
/*
        case I_PISTOL: additem(n, d->ammo[1], ammo); break;
	
        case I_AMMO:  additem(n, d->ammo[2], ammo); break;
        case I_SUBGUN: additem(n, d->ammo[3], ammo); break;
        case I_SNIPER: additem(n, d->ammo[4], ammo); break;
        case I_ASSAULT:  additem(n, d->ammo[5], ammo); break;
	case I_GRENADE: additem(n, d->ammo[6], ammo); break;
*/
        case I_HEALTH:  additem(n, d->health,  np*5); break;

        case I_ARMOUR:
            additem(n, d->armour, 20);
            break;

        case I_QUAD:
            additem(n, d->quadmillis, 60);
            break;
            
        case CARROT:
            ents[n].spawned = false;
            triggertime = lastmillis;
            trigger(ents[n].attr1, ents[n].attr2, false);  // needs to go over server for multiplayer
            break;

        case OBJ_ITEM:
            additem(n, d->quadmillis, 60);
            break;
            
        case TRIGGER:
            ents[n].spawned = false;
            triggertime = lastmillis;
            trigger(ents[n].attr1, ents[n].attr2, false);  // needs to go over server for multiplayer
            BotManager.PickNextTrigger(); // Added by Rick
            break;

    };
};

void checkitems()
{
    if(editmode) return;
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==NOTUSED) continue;
        if(!ents[i].spawned) continue;
        if(OUTBORD(e.x, e.y)) continue;
        vec v = { e.x, e.y, S(e.x, e.y)->floor+player1->eyeheight };
        vdist(dist, t, player1->o, v);
        if(dist<2.5) pickup(i, player1);
    };
};

void putitems(uchar *&p)            // puts items in network stream and also spawns them locally
{
    loopv(ents) if((ents[i].type>=I_AMMO && ents[i].type<=I_QUAD) || ents[i].type==TRIGGER)
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

      if(m_pistols) 
      {
            d->primary=GUN_PISTOL;
            d->ammo[GUN_PISTOL] = 48; //change?
            return;
      };

      if (d->primary==GUN_PISTOL)  // || pistol only mode!
      {
           d->ammo[d->primary] = 48;
      }
      else if (d->primary>GUN_PISTOL && d->primary<GUN_GRENADE)
      {
            d->ammo[d->primary] = itemstats[d->primary-2].max;
            d->mag[d->primary] = itemstats[d->primary-2].add;
      }
      else if (d->primary==GUN_GRENADE)
      {
            conoutf("you don't have to worry about blowing your hand off just yet...");
      }
      
      /*
      if (d->armour)
      {
            d->armour = 100;
      }
      */
      //if(m_noitems) { conoutf("i am (not) sorry, but an armour choice is only for ts and lms"); return; };

      if(d->nextarmour==true)
      {     
            if(!m_noitems) return;{ conoutf("armour is only for ts and lms modes, maybe you can hide"); return; };
            d->hasarmour = true;
            d->armour=itemstats[7].add;
      }
      else
      {
            d->hasarmour = false;
      };
};

void add(int num)
{
      if (num>0 && num<6) player1->nextprimary = num;
      else 
      {
            if (num==7  && m_noitems) player1->nextarmour=!player1->nextarmour;
            else conoutf("armour is only for ts and lms modes, maybe you can hide");
      };
};

COMMAND(add,ARG_1INT);

// Added by Rick
bool intersect(entity *e, vec &from, vec &to, vec *end) // if lineseg hits entity bounding box(entity version)
{
    mapmodelinfo &mmi = getmminfo(e->attr2);
    if(!&mmi || !mmi.h) return false;
    
    float lo = (float)(S(e->x, e->y)->floor+mmi.zoff+e->attr3);
    float hi = lo+mmi.h;
    vec v = to, w = { e->x, e->y, lo + (fabs(hi-lo)/2.0f) }, *p; 
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
};
// End add by Ricks
