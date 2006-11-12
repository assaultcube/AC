#include "cube.h"

VAR(swayspeeddiv, 1, 125, 1000);
VAR(swaymovediv, 1, 200, 1000); 

VAR(swayupspeeddiv, 1, 125, 1000);
VAR(swayupmovediv, 1, 200, 1000); 

VAR(x, 0, 0, 1000);
VAR(y, 0, 0, 1000);
VAR(z, 0, 0, 1000);

struct weaponmove
{
    float k_rot, kick;
    vec pos;
    int anim;
    
	weaponmove() : k_rot(0), kick(0), anim(0) { pos.x = pos.y = pos.z = 0.0f; };

    void calcmove(vec base, int basetime)
    {
        int timediff = NADE_THROWING ? (lastmillis-player1->thrownademillis) : lastmillis-basetime;
        int animtime = attackdelay(player1->gunselect);
        int rtime = reloadtime(player1->gunselect);
        
        kick = k_rot = 0.0f;
        pos = player1->o;
        anim = ANIM_GUN_IDLE;
        
        if(player1->weaponchanging)
        {
            anim = ANIM_GUN_RELOAD;
            float percent_done = (float)(timediff)*100.0f/(float) WEAPONCHANGE_TIME;
            if(percent_done>=100 || percent_done<0) percent_done = 100;
            k_rot = -(sin((float)(percent_done*2/100.0f*90.0f)*PI/180.0f)*90);
        }
        else if(player1->reloading)
        {
            anim = ANIM_GUN_RELOAD;
            float percent_done = (float)(timediff)*100.0f/(float)rtime;
            if(percent_done>=100 || percent_done<0) percent_done = 100;
            k_rot = -(sin((float)(percent_done*2/100.0f*90.0f)*PI/180.0f)*90);
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
                kick = -sin(pow((1.5f/100.0f*percent_done)-1.5f,3));
            };
            
			if(player1->lastaction && player1->lastattackgun==player1->gunselect)
            {
				if(NADE_THROWING && timediff<animtime) anim = ANIM_GUN_THROW;
				else if(lastmillis-player1->lastaction<animtime || NADE_IN_HAND) 
					anim = ANIM_GUN_SHOOT|(player1->gunselect!=GUN_KNIFE && player1->gunselect!=GUN_GRENADE ? ANIM_LOOP : 0);
			};
            
            if(player1->gunselect!=GUN_GRENADE && player1->gunselect!=GUN_KNIFE)
            {
                k_rot = kick_rot(player1->gunselect)*kick;
                k_back = kick_back(player1->gunselect)*kick/10;
            };
    
            float swayspeed = (float) (sin((float)lastmillis/swayspeeddiv))/(swaymovediv/10.0f);
            float swayupspeed = (float) (sin((float)lastmillis/swayupspeeddiv-90))/(swayupmovediv/10.0f);

            #define g0(x) ((x) < 0.0f ? -(x) : (x))
            float plspeed = min(1.0f, sqrt(g0(player1->vel.x*player1->vel.x) + g0(player1->vel.y*player1->vel.y)));
            
            swayspeed *= plspeed/2;
            swayupspeed *= plspeed/2;
            
            float tmp = sway.x;
            sway.x = sway.y;
            sway.y = -tmp;
            
            if(swayupspeed<0.0f)swayupspeed = -swayupspeed; // sway a semicirle only
            sway.z = 1.0f;
            
            sway.x *= swayspeed;
            sway.y *= swayspeed;
            sway.z *= swayupspeed;
            
            pos.x = player1->o.x-base.x*k_back+sway.x;
            pos.y = player1->o.y-base.y*k_back+sway.y;
            pos.z = player1->o.z-base.z*k_back+sway.z;
        };
    };
};

void renderhudgun(int gun, int lastaction, int index = 0)
{
    vec unitv;
    float dist = worldpos.dist(player1->o, unitv);
    unitv.div(dist);
    
	weaponmove wm;
	if(!intermission) wm.calcmove(unitv, lastaction);
    s_sprintfd(path)("weapons/%s", hudgunnames[gun]);
    static int lastanim[2], lastswitch[2];
    if(lastanim[index]!=(wm.anim|(gun<<16)))
    {
        lastanim[index] = wm.anim|(gun<<16);
        lastswitch[index] = lastmillis;
    };
    rendermodel(path, wm.anim|(index ? ANIM_MIRROR : 0), 0, 0, wm.pos.x, wm.pos.z, wm.pos.y, player1->yaw + 90, player1->pitch+wm.k_rot, 40.0f, lastswitch[index], NULL, NULL, 1.28f);  
};

void renderhudgun()
{
    if(player1->akimbo && player1->gunselect==GUN_PISTOL) // akimbo
    {
        renderhudgun(GUN_PISTOL, player1->akimbolastaction[0], 0);
        renderhudgun(GUN_PISTOL, player1->akimbolastaction[1], 1);
    }
    else
    {
        renderhudgun(player1->gunselect, player1->lastaction);
    };
};

void preload_hudguns()
{
    loopi(NUMGUNS)
    {
        s_sprintfd(path)("weapons/%s", hudgunnames[i]);
        loadmodel(path);
    };
};

