#include "cube.h"

VAR(swayspeeddiv, 1, 125, 1000);
VAR(swaymovediv, 1, 200, 1000); 

VAR(swayupspeeddiv, 1, 125, 1000);
VAR(swayupmovediv, 1, 200, 1000); 

struct weaponmove
{
    static vec swaydir;
    static int swaymillis, lastsway;

    float k_rot, kick;
    vec pos;
    int anim;
    
	weaponmove() : k_rot(0), kick(0), anim(0) { pos.x = pos.y = pos.z = 0.0f; }

    void calcmove(vec base, int basetime)
    {
        int timediff = NADE_THROWING ? lastmillis-player1->thrownademillis : lastmillis-basetime;
        int animtime = NADE_THROWING ? NADE_THROW_TIME : min(player1->gunwait[player1->gunselect], attackdelay(player1->gunselect));
        int rtime = reloadtime(player1->gunselect);
       
        kick = k_rot = 0.0f;
        pos = player1->o;
        anim = ANIM_GUN_IDLE;
       
        float k = pow(0.7f, (lastmillis-lastsway)/10.0f);
        swaydir.mul(k);
        vec dv(player1->vel);
        dv.mul((1-k)/max(player1->vel.magnitude(), player1->maxspeed));
        dv.x *= 1.5f;
        dv.y *= 1.5f;
        dv.z *= 0.4f;
        swaydir.add(dv);
        pos.add(swaydir);
        if(player1->onfloor || player1->onladder || player1->inwater) swaymillis += lastmillis-lastsway;
        lastsway = lastmillis;

        if(player1->weaponchanging)
        {
            anim = ANIM_GUN_RELOAD;
            float percent_done = (float)(timediff)*100.0f/(float) WEAPONCHANGE_TIME;
            if(percent_done>=100 || percent_done<0) percent_done = 100;
            k_rot = -(sinf((float)(percent_done*2/100.0f*90.0f)*PI/180.0f)*90);
        }
        else if(player1->reloading)
        {
            anim = ANIM_GUN_RELOAD;
            float percent_done = (float)(timediff)*100.0f/(float)rtime;
            if(percent_done>=100 || percent_done<0) percent_done = 100;
            k_rot = -(sinf((float)(percent_done*2/100.0f*90.0f)*PI/180.0f)*90);
        }
        else
        {
            vec sway = base;
            float percent_done = 0.0f;
            float k_back = 0.0f;
            
            if(player1->gunselect==player1->lastattackgun)
            {
                percent_done = timediff*100.0f/(float)animtime;
                if(percent_done > 100.0f) percent_done = 100.0f;
                // f(x) = -sin(x-1.5)^3
                kick = -sinf(pow((1.5f/100.0f*percent_done)-1.5f,3));
            }
            
			if(player1->lastaction && player1->lastattackgun==player1->gunselect)
            {
				if(NADE_THROWING && timediff<animtime) anim = ANIM_GUN_THROW;
				else if(lastmillis-player1->lastaction<animtime || NADE_IN_HAND) 
					anim = ANIM_GUN_SHOOT|(player1->gunselect!=GUN_KNIFE && player1->gunselect!=GUN_GRENADE ? ANIM_LOOP : 0);
			}
            
            if(player1->gunselect!=GUN_GRENADE && player1->gunselect!=GUN_KNIFE)
            {
                k_rot = kick_rot(player1->gunselect)*kick;
                k_back = kick_back(player1->gunselect)*kick/10;
            }
    
            float swayspeed = sinf((float)swaymillis/swayspeeddiv)/(swaymovediv/10.0f);
            float swayupspeed = cosf((float)swaymillis/swayupspeeddiv)/(swayupmovediv/10.0f);

            float plspeed = min(1.0f, sqrt(player1->vel.x*player1->vel.x + player1->vel.y*player1->vel.y));
            
            swayspeed *= plspeed/2;
            swayupspeed *= plspeed/2;

            swap(float, sway.x, sway.y);
            sway.y = -sway.y;
            
            swayupspeed = fabs(swayupspeed); // sway a semicirle only
            sway.z = 1.0f;
            
            sway.x *= swayspeed;
            sway.y *= swayspeed;
            sway.z *= swayupspeed;
            
            pos.x -= base.x*k_back+sway.x;
            pos.y -= base.y*k_back+sway.y;
            pos.z -= base.z*k_back+sway.z;
        }
    }
};

vec weaponmove::swaydir(0, 0, 0);
int weaponmove::lastsway = 0, weaponmove::swaymillis = 0;

VARP(hudgun,0,1,1);

void renderhudgun(int gun, int lastaction, int index = 0)
{
    vec unitv;
    float dist = worldpos.dist(player1->o, unitv);
    unitv.div(dist);
    
	weaponmove wm;
	if(!intermission) wm.calcmove(unitv, lastaction);
    s_sprintfd(path)("weapons/%s", gunnames[gun]);
    static int lastanim[2], lastswitch[2];
    if(lastanim[index]!=(wm.anim|(gun<<16)))
    {
        lastanim[index] = wm.anim|(gun<<16);
        lastswitch[index] = lastmillis;
    }
    rendermodel(path, wm.anim|(index ? ANIM_MIRROR : 0), 0, 0, wm.pos, player1->yaw+90, player1->pitch+wm.k_rot, 40.0f, lastswitch[index], NULL, NULL, 1.28f);  
}

void renderhudgun()
{
    if(!hudgun) return;
    if(player1->akimbo && player1->gunselect==GUN_PISTOL) // akimbo
    {
        renderhudgun(GUN_PISTOL, player1->akimbolastaction[0], 0);
        renderhudgun(GUN_PISTOL, player1->akimbolastaction[1], 1);
    }
    else
    {
        renderhudgun(player1->gunselect, player1->lastaction);
    }
}

void preload_hudguns()
{
    loopi(NUMGUNS)
    {
        s_sprintfd(path)("weapons/%s", gunnames[i]);
        loadmodel(path);
    }
}

