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

#include "../cube.h"

#ifdef AC_CUBE

extern vector<server_entity> sents;
extern int triggertime;
extern itemstat itemstats[];
extern void spawnstate(dynent *d);

//AC Bot class begin

void CACBot::Spawn()
{
     // Init all bot variabeles
     m_pMyEnt->eyeheight = 4.25f;
     m_pMyEnt->aboveeye = 0.7f;
     m_pMyEnt->radius = 1.1f;
         
     spawnplayer(m_pMyEnt);
     
     m_pMyEnt->targetyaw = m_pMyEnt->yaw = m_pMyEnt->targetpitch = m_pMyEnt->pitch = 0.0f;
     m_pMyEnt->move = 0;
     m_pMyEnt->enemy = NULL;
     m_pMyEnt->maxspeed = 16.0f;
     m_pMyEnt->health = 100;
     m_pMyEnt->armour = 50;
     m_pMyEnt->pitch = 0;
     m_pMyEnt->roll = 0;
     m_pMyEnt->state = CS_ALIVE;
     m_pMyEnt->anger = 0;
     m_pMyEnt->pBot = this;     
     loopi(NUMGUNS) m_pMyEnt->ammo[i] = 0;
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
                         botbaseammo(m_pMyEnt->gunselect = gun1, m_pMyEnt);
                         for(;;)
                         {
                              int gun2 = rnd(4)+1;
                              if(gun1!=gun2) { botbaseammo(gun2, m_pMyEnt); break; };
                         }
                    }
                    else if(m_arena)    // insta arena
                    {
                         m_pMyEnt->ammo[GUN_RIFLE] = 100;
                    }
                    else // efficiency
                    {
                         loopi(4) botbaseammo(i+1, m_pMyEnt);
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
         
     m_pMyEnt->ammo[GUN_KNIFE] = 1;    
     m_pMyEnt->ammo[GUN_SEMIPISTOL] = 5;
     m_pMyEnt->mag[GUN_SEMIPISTOL] = 5;
     SelectGun(GUN_SEMIPISTOL);
     
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

void CACBot::BotPain(int damage, dynent *d)
{
     if(m_pMyEnt->state!=CS_ALIVE || editmode || intermission) return;
     int ad = damage*(m_pMyEnt->armourtype+1)*20/100; // let armour absorb when possible
     if(ad>m_pMyEnt->armour) ad = m_pMyEnt->armour;
     m_pMyEnt->armour -= ad;
     damage -= ad;
     float droll = damage/0.5f;
     m_pMyEnt->roll += m_pMyEnt->roll>0 ? droll : (m_pMyEnt->roll<0 ? -droll : (rnd(2) ? droll : -droll));  // give player a kick depending on amount of damage
     if((m_pMyEnt->health -= damage)<=0)
     {
          if (player1 == d)
          {
               int frags;
               if(isteam(player1->team, m_pMyEnt->team))
               {
                    frags = -1;
                    conoutf("you fragged a teammate (%s)", (int)m_pMyEnt->name);
               }
               else
               {
                    frags = 1;
                    conoutf("you fragged %s", (int)m_pMyEnt->name);
               }
               addmsg(1, 2, SV_FRAGS, player1->frags += frags);             
               addmsg(1, 4, SV_BOTDIED, BotManager.GetBotIndex(m_pMyEnt), -1, false);
          }
          else if (d->monsterstate)
               conoutf("%s got killed by %s", (int)m_pMyEnt->name, (int)d->name);
          else
          {                 
               int KillerIndex = -1;
         
               if (d == m_pMyEnt)
               {
                    conoutf("%s suicided", (int)d->name);
                    KillerIndex = BotManager.GetBotIndex(d);
                    addmsg(1, 3, SV_BOTFRAGS, KillerIndex, --d->frags);
               }     
               else
               {     
                    if (d->bIsBot)
                    {
                         KillerIndex = BotManager.GetBotIndex(d);
                         addmsg(1, 3, SV_BOTFRAGS, KillerIndex, ++d->frags);             
                    }
                    else
                         loopv(players){ if (players[i] == d) { KillerIndex = i; break;} }
                  
                    if(isteam(m_pMyEnt->team, d->team))
                    {
                         conoutf("%s fragged his teammate (%s)", (int)d->name,
                                 (int)m_pMyEnt->name);
                    }
                    else
                    {
                         conoutf("%s fragged %s", (int)d->name, (int)m_pMyEnt->name);
                    }
               }
               addmsg(1, 4, SV_BOTDIED, BotManager.GetBotIndex(m_pMyEnt), KillerIndex,
                      d->bIsBot);
          }

          m_pMyEnt->lifesequence++;
          m_pMyEnt->attacking = false;
          m_pMyEnt->state = CS_DEAD;
          m_pMyEnt->pitch = 0;
          m_pMyEnt->roll = 60;
          playsound(S_DIE1+rnd(2), &m_pMyEnt->o);
          spawnstate(m_pMyEnt);
          m_pMyEnt->lastaction = lastmillis;
     }
     else
     {
          playsound(S_PAIN6, &m_pMyEnt->o);
     }
}

void CACBot::CheckItemPickup()
{
     if(editmode) return;
     loopv(ents)
     {
          entity &e = ents[i];
          if(e.type==NOTUSED) continue;
          
          if (!e.spawned) continue;
          if ((i < sents.length()) && (!sents[i].spawned)) continue;
                    
          if(OUTBORD(e.x, e.y)) continue;
          vec v = { e.x, e.y, S(e.x, e.y)->floor+m_pMyEnt->eyeheight };
          vdist(dist, t, m_pMyEnt->o, v);
          if(dist<2.5)
          {
               PickUp(i);
          }
     }
}

void CACBot::PickUp(int n)
{
    int np = 1;
    loopv(players) if(players[i]) np++;
    loopv(bots) if(bots[i]) np++;
    np = np<3 ? 4 : (np>4 ? 2 : 3); // spawn times are dependent on number of players
    int ammo = np*2;
    switch(ents[n].type)
    {
        case I_SEMIPISTOL:  AddItem(n, m_pMyEnt->ammo[1], ammo); break;
        case I_AUTOPISTOL: AddItem(n, m_pMyEnt->ammo[2], ammo); break;
        case I_SHOTGUN: AddItem(n, m_pMyEnt->ammo[3], ammo); break;
        case I_SNIPER:  AddItem(n, m_pMyEnt->ammo[4], ammo); break;
        case I_SUBGUN:  AddItem(n, m_pMyEnt->ammo[5], ammo); break;
        case I_CARBINE: AddItem(n, m_pMyEnt->ammo[6], ammo); break;
        case I_SEMIRIFLE: AddItem(n, m_pMyEnt->ammo[7], ammo); break;
        case I_AUTORIFLE:  AddItem(n, m_pMyEnt->ammo[8], ammo); break;
        case I_GRENADE:  AddItem(n, m_pMyEnt->ammo[9], ammo); break;

        case I_HELMET:
            // (100h/100g only absorbs 166 damage)
            if(m_pMyEnt->armourtype==A_YELLOW && m_pMyEnt->armour>66) break;
            AddItem(n, m_pMyEnt->armour, 20);
            break;

        case I_ARMOUR:
            AddItem(n, m_pMyEnt->armour, 20);
            break;

        case OBJ_ITEM:
            AddItem(n, m_pMyEnt->quadmillis, 60);
            break;
            
        case TRIGGER:
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
            break;
    };
};

void CACBot::AddItem(int i, int &v, int spawnsec)
{
     if((i>=sents.length()) || (!sents[i].spawned))
          return;

     if(v>=itemstats[ents[i].type-I_SEMIPISTOL].max)  // don't pick up if not needed
          return;
             
     itemstat &is = itemstats[ents[i].type-I_SEMIPISTOL];
     ents[i].spawned = false;
     v += is.add;
     if(v>is.max) v = is.max;
     botplaysound(is.sound, m_pMyEnt);
     sents[i].spawned = false;
     sents[i].spawnsecs = spawnsec;
        
     addmsg(1, 4, SV_ITEMPICKUP, i, m_classicsp ? 100000 : spawnsec, false);
        
     // HACK: Reset ent goal if bot was looking for for this ent
     if (m_pTargetEnt == &ents[i])
     {
          m_pTargetEnt = NULL;
          m_vGoal = g_vecZero;
          ResetWaypointVars();
          m_iCheckEntsDelay = lastmillis + RandomLong(5000, 10000);
     }        
}
          
// AC Bot class end

#endif
