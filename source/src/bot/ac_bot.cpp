//
// C++ Implementation: bot
//
// Description: Main bot code for Action Cube
//
// Main bot file
//
// Author:  Rick <rickhelmus@gmail.com>
//
//
//

#include "bot.h"

#ifdef AC_CUBE

extern vector<server_entity> sents;
extern int triggertime;
extern itemstat itemstats[];
extern void spawnstate(playerent *d);

//AC Bot class begin

void CACBot::Spawn()
{
     // Init all bot variabeles
     m_pMyEnt->primary = m_pMyEnt->nextprimary = 2 + rnd(4);
         
     spawnplayer(m_pMyEnt);
    
     m_pMyEnt->primary = m_pMyEnt->nextprimary = 2 + rnd(4);
     m_pMyEnt->targetyaw = m_pMyEnt->targetpitch = 0.0f;
	 equip(this->m_pMyEnt);
     m_pMyEnt->armour = 50;
     m_pMyEnt->pBot = this;
     
     /* UNDONE
     m_pMyEnt->ammo[GUN_FIST] = 1;
     if(m_noitems)
     {
          m_pMyEnt->gunselect = GUN_RIFLE;
          m_pMyEnt->armour = 0;
          if(m_noitemsrail)
          {
               m_pMyEnt->health = 1;
               m_pMyEnt->ammo[GUN_RIFLE] = 100;
          }
          else
          {
               if(gamemode==12) // eihrul's secret "instafist" mode
               {
                    m_pMyEnt->gunselect = GUN_FIST;
               }
               else
               {
                    m_pMyEnt->health = 256;
                    if(m_tarena)
                    {
                         int gun1 = rnd(4)+1;
                         baseammo(m_pMyEnt->gunselect = gun1, m_pMyEnt);
                         for(;;)
                         {
                              int gun2 = rnd(4)+1;
                              if(gun1!=gun2) { baseammo(gun2, m_pMyEnt); break; };
                         }
                    }
                    else if(m_arena)    // insta arena
                    {
                         m_pMyEnt->ammo[GUN_RIFLE] = 100;
                    }
                    else // efficiency
                    {
                         loopi(4) baseammo(i+1, m_pMyEnt);
                         m_pMyEnt->gunselect = GUN_CG;
                    }
                    m_pMyEnt->ammo[GUN_CG] /= 2;
               }
          }
     }
     else
     {
          m_pMyEnt->ammo[GUN_SEMIPISTOL] = 5;
          SelectGun(GUN_SG);
     }*/
         
     SelectGun(m_pMyEnt->primary);
     
     m_eCurrentBotState = STATE_NORMAL;
     m_iShootDelay = m_iChangeWeaponDelay = 0;
     m_iCheckEnvDelay = 0;
     m_vPrevOrigin = g_vecZero;
     m_iStuckCheckDelay = lastmillis + 250;
     m_bStuck = false;
     m_iStuckTime = 0;
     m_iStrafeTime = m_iStrafeCheckDelay = 0;
     m_iMoveDir = DIR_NONE;
     
     m_pPrevEnemy = NULL;
     m_iCombatNavTime = 0;
     m_iSPMoveTime = 0;
     m_iEnemySearchDelay = 0;
     m_bCombatJump = false;
     m_iCombatJumpDelay = 0;
     m_bShootAtFeet = (RandomLong(1, 100) <= m_pBotSkill->sShootAtFeetWithRLPercent);
     m_iHuntDelay = 0;
     m_vHuntLocation = m_vPrevHuntLocation = g_vecZero;
     m_pHuntTarget = NULL;
     m_fPrevHuntDist = 0.0f;
     m_iHuntLastTurnLessTime = m_iHuntPlayerUpdateTime = m_iHuntPauseTime = 0;
        
     m_iLastJumpPad = 0;
     m_pTargetEnt = NULL;
     m_iCheckTeleporterDelay = m_iCheckJumppadsDelay = 0;
     m_iCheckEntsDelay = 0;
     m_iCheckTriggersDelay = 0;
     m_iLookForWaypointTime = 0;
        
     m_iAimDelay = 0;
     m_fYawToTurn = m_fPitchToTurn = 0.0f;
     
     m_vGoal = m_vWaterGoal = g_vecZero;
          
     ResetWaypointVars();
}

void CACBot::CheckItemPickup()
{
     if(editmode) return;
     loopv(ents)
     {
          entity &e = ents[i];
          if(e.type==NOTUSED) continue;
          
          if (!e.spawned) continue;
          if (sents.inrange(i) && (!sents[i].spawned)) continue;
                    
          if(OUTBORD(e.x, e.y)) continue;
          vec v(e.x, e.y, S(e.x, e.y)->floor+m_pMyEnt->eyeheight);

          if(v.dist(m_pMyEnt->o)<2.5)
          {
               PickUp(i);
          }
     }
}

void CACBot::PickUp(int n)
{
    int np = 1;
    loopv(players) if(players[i]) np++;
    np = np<3 ? 4 : (np>4 ? 2 : 3); // spawn times are dependent on number of players
    int ammo = np*2;
    switch(ents[n].type)
    {
		case I_AMMO:    AddItem(n, m_pMyEnt->ammo[m_pMyEnt->primary], ammo, m_pMyEnt->primary); break;
        case I_CLIPS:   AddItem(n, m_pMyEnt->ammo[GUN_PISTOL], ammo, GUN_PISTOL); break;
        case I_GRENADE: AddItem(n, m_pMyEnt->ammo[GUN_GRENADE], ammo, 6); break;
		case I_HEALTH:	AddItem(n, m_pMyEnt->health, ammo, 7); break;
		case I_ARMOUR:  AddItem(n, m_pMyEnt->armour, ammo, 8); break;

/*        case I_HELMET:
            // (100h/100g only absorbs 166 damage)
            if(m_pMyEnt->armourtype==A_YELLOW && m_pMyEnt->armour>66) break;
            AddItem(n, m_pMyEnt->armour, 20);
            break;

        case I_ARMOUR:
            AddItem(n, m_pMyEnt->armour, 20);
            break;

        case OBJ_ITEM:
            AddItem(n, m_pMyEnt->quadmillis, 60);
            break;*/
            
/*        case TRIGGER:
            ents[n].spawned = false;
            triggertime = lastmillis;
            trigger(ents[n].attr1, ents[n].attr2, false);  // needs to go over server for multiplayer
            
            // HACK: Reset ent goal if bot was looking for this ent
            if (m_pTargetEnt == &ents[n])
            {
               m_pTargetEnt = NULL;
               m_vGoal = g_vecZero;
               ResetWaypointVars();
               m_iCheckEntsDelay = lastmillis + RandomLong(5000, 10000);               
            }
            
            BotManager.PickNextTrigger();
            break;*/
    };
};

void CACBot::AddItem(int i, int &v, int spawnsec, int t)
{
     if((i>=sents.length()) || (!sents[i].spawned))
          return;

//fixmebot 
     if(v>=itemstats[t].max)  // don't pick up if not needed
          return;
             
     itemstat &is = itemstats[t];
     ents[i].spawned = false;
     v += is.add;
     if(v>is.max) v = is.max;
     playsound(is.sound, &m_pMyEnt->o);
     sents[i].spawned = false;
     sents[i].spawnsecs = spawnsec;
        
     // HACK: Reset ent goal if bot was looking for for this ent
     if (m_pTargetEnt == &ents[i])
     {
          m_pTargetEnt = NULL;
          m_vGoal = g_vecZero;
          ResetWaypointVars();
          m_iCheckEntsDelay = lastmillis + RandomLong(5000, 10000);
     };
}
          
// AC Bot class end

#endif
