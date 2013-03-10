//
// C++ Implementation: bot_ai
//
// Description: The AI part comes here(navigation, shooting etc)
//
//
// Author:  <rickhelmus@gmail.com>
//


// Code of CBot - Start

#include "cube.h"
#include "bot.h"

extern weaponinfo_s WeaponInfoTable[MAX_WEAPONS];

vec CBot::GetEnemyPos(playerent *d)
{
    // Aim offset idea by botman
    vec o = m_pMyEnt->weaponsel->type == GUN_SNIPER && d->head.x >= 0 ? d->head : d->o, offset;
    float flDist = GetDistance(o), flScale;

    if (WeaponInfoTable[m_pMyEnt->gunselect].eWeaponType == TYPE_ROCKET)
    {
        // Bot is using a rocket launcher, aim at enemy feet?
        if (m_bShootAtFeet && !OUTBORD(d->o.x, d->o.y))
        {
            // Only do this when enemy is fairly close to the ground
            vec end = o;
            end.z -= 900.0f;
            traceresult_s tr;
            TraceLine(o, end, NULL, false, &tr);
            if ((o.z - tr.end.z) < 8.0f)
            {
                end = o;
                end.z = tr.end.z;
                if (IsVisible(end))
                {
                    // Target at ground
                    o.z = tr.end.z;
                }
            }
        }

        if (m_pBotSkill->bCanPredict)
        {
            // How higher the skill, how further the bot predicts
            float flPredictTime = RandomFloat(1.25f, 1.7f) / (m_sSkillNr+1);
            o = PredictPos(o, d->vel, flPredictTime);
        }
    }
    else
    {
        if (m_pBotSkill->bCanPredict)
        {
            // How higher the skill, how 'more' the bot predicts
            float flPredictTime = RandomFloat(0.8f, 1.2f) / (m_sSkillNr+1);
            o = PredictPos(o, d->vel, flPredictTime);
        }
    }

    if (flDist > 60.0f)
        flScale = 1.0f;
    else if (flDist > 6.0f)
        flScale = flDist / 60.0f;
    else
        flScale = 0.1f;

    switch (m_sSkillNr)
    {
    case 0:
        // no offset
        offset.x = 0;
        offset.y = 0;
        offset.z = 0;
        break;
    case 1:
        // GOOD, offset a little for x, y, and z
        offset.x = RandomFloat(-3, 3) * flScale;
        offset.y = RandomFloat(-3, 3) * flScale;
        offset.z = RandomFloat(-6, 6) * flScale;
        break;
    case 2:
        // FAIR, offset somewhat for x, y, and z
        offset.x = RandomFloat(-8, 8) * flScale;
        offset.y = RandomFloat(-8, 8) * flScale;
        offset.z = RandomFloat(-12, 12) * flScale;
        break;
    case 3:
        // POOR, offset for x, y, and z
        offset.x = RandomFloat(-15, 15) * flScale;
        offset.y = RandomFloat(-15, 15) * flScale;
        offset.z = RandomFloat(-25, 25) * flScale;
        break;
    case 4:
        // BAD, offset lots for x, y, and z
        offset.x = RandomFloat(-20, 20) * flScale;
        offset.y = RandomFloat(-20, 20) * flScale;
        offset.z = RandomFloat(-35, 35) * flScale;
        break;
    }

    o.add(offset);
    return o;
}

// WIP
bool CBot::BotsAgainstHumans()
{
    return false;
}

bool CBot::DetectEnemy(playerent *p)
{
    return (IsInFOV(p) || (m_pBotSkill->flAlwaysDetectDistance > m_pMyEnt->o.dist(p->o)))
       && IsVisible(p);
}

bool CBot::FindEnemy(void)
{
    // UNDONE: Enemies are now only scored on their distance
    if(BotsAgainstHumans())
    {
        m_pMyEnt->enemy = NULL;
        if(player1->state == CS_ALIVE)
        {
            m_pMyEnt->enemy = player1;
        }
       
        return m_pMyEnt->enemy != NULL;
    }

    if (m_pMyEnt->enemy) // Bot already has an enemy
    {
        // Check if the enemy is still in game
        bool found = IsInGame(m_pMyEnt->enemy);

        // Check if the enemy is still ingame, still alive, not joined my team and is visible
        if (found && !isteam(m_pMyEnt->team, m_pMyEnt->enemy->team))
        {
            if ((m_pMyEnt->enemy->state == CS_ALIVE) && (IsVisible(m_pMyEnt->enemy)))
                return true;
            else
                m_pPrevEnemy = m_pMyEnt->enemy;
        }
        else
            m_pMyEnt->enemy = NULL;
    }

    if (m_iEnemySearchDelay > lastmillis) return (m_pMyEnt->enemy!=NULL);

    m_pMyEnt->enemy = NULL;

    // Add enemy searchy delay
    float MinDelay = m_pBotSkill->flMinEnemySearchDelay;
    float MaxDelay = m_pBotSkill->flMaxEnemySearchDelay;
    m_iEnemySearchDelay = lastmillis + int(RandomFloat(MinDelay, MaxDelay) * 1000.0f);

    playerent *pNewEnemy = NULL, *d = NULL;
    float flDist, flNearestDist = 99999.9f;
    short EnemyVal, BestEnemyVal = -100;

        // First loop through all players
        loopv(players)
        {
            d = players[i]; // Handy shortcut

            if (d == m_pMyEnt || !d || isteam(d->team, m_pMyEnt->team) || (d->state != CS_ALIVE))
                continue;

            // Check if the enemy is visible
            if(!DetectEnemy(d))
                continue;

            flDist = GetDistance(d->o);
            EnemyVal = 1;

            if (flDist < flNearestDist)
            {
                EnemyVal+=2;
                flNearestDist = flDist;
            }

            if (EnemyVal > BestEnemyVal)
            {
                pNewEnemy = d;
                BestEnemyVal = EnemyVal;
            }
        }

        // Then examine the local player
        if (player1 && !isteam(player1->team, m_pMyEnt->team) &&
            (player1->state == CS_ALIVE))
        {
            // Check if the enemy is visible
            if(DetectEnemy(player1))
            {
                flDist = GetDistance(player1->o);
                EnemyVal = 1;

                if (flDist < flNearestDist)
                {
                    EnemyVal+=2;
                    flNearestDist = flDist;
                }

                if (EnemyVal > BestEnemyVal)
                {
                    pNewEnemy = player1;
                    BestEnemyVal = EnemyVal;
                }
            }
        }
    //}

    if (pNewEnemy)
    {
        if (!m_pMyEnt->enemy) // Add shoot delay if new enemy is found
        {
            float flMinShootDelay = m_pBotSkill->flMinAttackDelay;
            float flMaxShootDelay = m_pBotSkill->flMaxAttackDelay;

            m_iShootDelay = lastmillis + int(RandomFloat(flMinShootDelay,
                                       flMaxShootDelay) * 1000.0f);
        }

        if ((m_pMyEnt->enemy != pNewEnemy) && m_pMyEnt->enemy)
            m_pPrevEnemy = m_pMyEnt->enemy;

        m_pMyEnt->enemy = pNewEnemy;
        return true;
    }

    return false;
}

bool CBot::CheckHunt(void)
{
    if (!BotManager.BotsShoot()) return false;

    if (m_pHuntTarget) // Bot already has an enemy to hunt
    {
        // Check if the enemy is still in game
        bool found = IsInGame(m_pHuntTarget);

        // Check if the enemy is still ingame, still alive, not joined my team and is visible
        if (found && !isteam(m_pMyEnt->team, m_pHuntTarget->team))
        {
            if ((m_pHuntTarget->state == CS_ALIVE) && IsReachable(m_vHuntLocation))
                return true;
        }
        else
            m_pHuntTarget = NULL;
    }

    if (m_iHuntDelay > lastmillis) return (m_pHuntTarget!=NULL);

    if (m_vHuntLocation!=g_vecZero)
        m_vPrevHuntLocation = m_vHuntLocation;

    m_pHuntTarget = NULL;
    m_vHuntLocation = g_vecZero;

    // Add enemy hunt search delay
    m_iHuntDelay = lastmillis + 1500;

    playerent *pNewEnemy = NULL, *d = NULL;
    float flDist, flNearestDist = 99999.9f, flNearestOldPosDistToEnemy = 99999.9f;
    float flNearestOldPosDistToBot = 99999.9f;
    short EnemyVal, BestEnemyVal = -100;
    vec BestOldPos;

        // First loop through all players
        loopv(players)
        {
            d = players[i]; // Handy shortcut

            if (d == m_pMyEnt || !d || isteam(d->team, m_pMyEnt->team) || (d->state != CS_ALIVE))
                continue;

            flDist = GetDistance(d->o);

            if (flDist > 250.0f) continue;

            EnemyVal = 1;

            if (flDist < flNearestDist)
            {
                EnemyVal+=2;
                flNearestDist = flDist;
            }

            if (d == m_pPrevEnemy)
                EnemyVal+=2;

            if (EnemyVal < BestEnemyVal) continue;

            vec bestfromenemy = g_vecZero, bestfrombot = g_vecZero;
            flNearestOldPosDistToEnemy = flNearestOldPosDistToBot = 9999.9f;

            // Check previous locations of enemy
            for (int j=0;j<d->history.size();j++)
            {
                const vec &v = d->history.getpos(j);
                if (v==m_vPrevHuntLocation) continue;

                flDist = GetDistance(d->o, v);

                if ((flDist < flNearestOldPosDistToEnemy) && IsReachable(v))
                {
                    flNearestOldPosDistToEnemy = flDist;
                    bestfromenemy = v;
                }
            }

            // Check previous locations of bot hisself
            for (int j=0;j<m_pMyEnt->history.size();j++)
            {
                const vec &v = m_pMyEnt->history.getpos(j);
                if (v==m_vPrevHuntLocation) continue;

                flDist = GetDistance(v);

                if ((flDist < flNearestOldPosDistToBot) && ::IsVisible(d->o, v) &&
                    IsReachable(v))
                {
                    flNearestOldPosDistToBot = flDist;
                    bestfrombot = v;
                }
            }

            if (bestfromenemy!=g_vecZero)
            {
                pNewEnemy = d;
                BestEnemyVal = EnemyVal;
                BestOldPos = bestfromenemy;
            }
            else if (bestfrombot!=g_vecZero)
            {
                pNewEnemy = d;
                BestEnemyVal = EnemyVal;
                BestOldPos = bestfrombot;
            }

        // Then examine the local player
        if (player1 && !isteam(player1->team, m_pMyEnt->team) &&
            (player1->state == CS_ALIVE) && ((flDist = GetDistance(player1->o)) <= 250.0f))
        {
            d = player1;
            EnemyVal = 1;

            if (flDist < flNearestDist)
            {
                EnemyVal+=2;
                flNearestDist = flDist;
            }

            if (d == m_pPrevEnemy)
                EnemyVal+=2;

            if (EnemyVal >= BestEnemyVal)
            {
                BestEnemyVal = EnemyVal;
                vec bestfromenemy = g_vecZero, bestfrombot = g_vecZero;
                flNearestOldPosDistToEnemy = flNearestOldPosDistToBot = 9999.9f;

                // Check previous locations of enemy
                for (int j=0;j<d->history.size();j++)
                {
                    const vec &v = d->history.getpos(j);
                    if (v==m_vPrevHuntLocation) continue;

                    flDist = GetDistance(d->o, v);

                    if ((flDist < flNearestOldPosDistToEnemy) && IsReachable(v))
                    {
                        flNearestOldPosDistToEnemy = flDist;
                        bestfromenemy = v;
                    }
                }

                // Check previous locations of bot hisself
                for (int j=0;j<m_pMyEnt->history.size();j++)
                {
                    const vec &v = m_pMyEnt->history.getpos(j);
                    if (v==m_vPrevHuntLocation) continue;

                    flDist = GetDistance(v);

                    if ((flDist < flNearestOldPosDistToBot) && ::IsVisible(d->o, v) &&
                        IsReachable(v))
                    {
                        flNearestOldPosDistToBot = flDist;
                        bestfrombot = v;
                    }
                }

                if (bestfromenemy!=g_vecZero)
                {
                    pNewEnemy = d;
                    BestEnemyVal = EnemyVal;
                    BestOldPos = bestfromenemy;
                }
                else if (bestfrombot!=g_vecZero)
                {
                    pNewEnemy = d;
                    BestEnemyVal = EnemyVal;
                    BestOldPos = bestfrombot;
                }
            }
        }
    }

    if (pNewEnemy)
    {
        if (!m_pHuntTarget) // Add shoot delay if new enemy is found
        {
            float flMinShootDelay = m_pBotSkill->flMinAttackDelay;
            float flMaxShootDelay = m_pBotSkill->flMaxAttackDelay;

            m_iShootDelay = lastmillis + int(RandomFloat(flMinShootDelay,
                                       flMaxShootDelay) * 1000.0f);
        }

        if (m_vHuntLocation!=g_vecZero)
            m_vPrevHuntLocation = m_vHuntLocation;

        m_pHuntTarget = pNewEnemy;
        m_vHuntLocation = BestOldPos;
        m_fPrevHuntDist = 0.0f;
        return true;
    }

    return false;
}

bool CBot::HuntEnemy(void)
{
    if (m_iCombatNavTime > lastmillis)
    {
        SetMoveDir(m_iMoveDir, false);
        return true;
    }

    m_iCombatNavTime = m_iMoveDir = 0;

    bool bDone = false, bNew = false;
    float flDist = GetDistance(m_vHuntLocation);

    if (flDist <= 3.0f)
        bDone = true;

    if ((m_fPrevHuntDist > 0.0) && (flDist > m_fPrevHuntDist))
        bDone = true;

    m_fPrevHuntDist = flDist;

    if ((m_iHuntPlayerUpdateTime < lastmillis) || bDone)
    {
        m_iHuntPlayerUpdateTime = lastmillis + 1250;

        short BestPosIndexFromEnemy = -1, BestPosIndexFromBot = -1,  j;
        float NearestDistToEnemy = 9999.9f, NearestDistToBot = 9999.9f;

        // Check previous locations of enemy
        for (j=0;j<m_pHuntTarget->history.size();j++)
        {
            const vec &OldPos = m_pHuntTarget->history.getpos(j);

            if (bDone && m_vHuntLocation==OldPos)
                continue;

            if (GetDistance(OldPos) > 250.0f)
                continue;

            flDist = GetDistance(m_pHuntTarget->o, OldPos);

            if ((flDist < NearestDistToEnemy) && (IsReachable(OldPos)))
            {
                NearestDistToEnemy = flDist;
                BestPosIndexFromEnemy = j;
                break;
            }
        }

        // Check previous locations of bot
        for (j=0;j<m_pMyEnt->history.size();j++)
        {
            const vec &OldPos = m_pMyEnt->history.getpos(j);

            if (bDone && m_vHuntLocation==OldPos)
                continue;

            if (GetDistance(OldPos) > 25.0f)
                continue;

            flDist = GetDistance(OldPos);

            if ((flDist < NearestDistToBot) && ::IsVisible(m_pHuntTarget->o, OldPos) &&
                (IsReachable(OldPos)))
            {
                NearestDistToBot = flDist;
                BestPosIndexFromBot = j;
                break;
            }
        }

        if (BestPosIndexFromEnemy > -1)
        {
            m_vPrevHuntLocation = m_vHuntLocation;
            m_vHuntLocation = m_pHuntTarget->history.getpos(BestPosIndexFromEnemy);
            bNew = true;
            m_fPrevHuntDist = 0.0f;
        }
        else if (BestPosIndexFromBot > -1)
        {
            m_vPrevHuntLocation = m_vHuntLocation;
            m_vHuntLocation = m_pMyEnt->history.getpos(BestPosIndexFromEnemy);
            bNew = true;
            m_fPrevHuntDist = 0.0f;
        }
    }

    if (!bNew) // Check if current location is still reachable
    {
        if (bDone || !IsReachable(m_vHuntLocation))
        {
            m_pHuntTarget = NULL;
            m_vPrevHuntLocation = m_vHuntLocation;
            m_vHuntLocation = g_vecZero;
            m_fPrevHuntDist = 0.0f;
            m_iHuntDelay = lastmillis + 3500;
            return false;
        }
    }
    else
        condebug("New hunt pos");

    // Aim to position
    //AimToVec(m_vHuntLocation);

    int iMoveDir = GetDirection(GetViewAngles(), m_pMyEnt->o, m_vHuntLocation);

    if (iMoveDir != DIR_NONE)
    {
        m_iMoveDir = iMoveDir;
        m_iCombatNavTime = lastmillis + 125;
    }

    bool aimtopos = true;

    if ((lastmillis - m_iSawEnemyTime) > 1500)
    {
        if (m_iLookAroundDelay < lastmillis)
        {
            if (m_iLookAroundTime > lastmillis)
            {
                if (m_iLookAroundUpdateTime < lastmillis)
                {
                    float flAddAngle;
                    if (m_bLookLeft) flAddAngle = RandomFloat(-110, -80);
                    else flAddAngle = RandomFloat(80, 110);
                    m_pMyEnt->targetyaw = WrapYZAngle(m_pMyEnt->targetyaw + flAddAngle);
                    m_iLookAroundUpdateTime = lastmillis + RandomLong(400, 800);
                }
                aimtopos = false;
            }
            else if (m_iLookAroundTime > 0)
            {
                m_iLookAroundTime = 0;
                m_iLookAroundDelay = lastmillis + RandomLong(750, 1000);
            }
            else
                m_iLookAroundTime = lastmillis + RandomLong(2200, 3200);
        }
    }

    if (aimtopos)
        AimToVec(m_vHuntLocation);

    debugbeam(m_pMyEnt->o, m_vHuntLocation);

    if (m_fYawToTurn <= 25.0f)
        m_iHuntLastTurnLessTime = lastmillis;

    // Bot had to turn much for a while?
    if ((m_iHuntLastTurnLessTime > 0) &&  (m_iHuntLastTurnLessTime < (lastmillis - 1000)))
    {
        m_iHuntPauseTime = lastmillis + 200;
    }

    if (m_iHuntPauseTime >= lastmillis)
    {
        m_pMyEnt->move = 0;
        m_fPrevHuntDist = 0.0f;
    }
    else
    {
        // Check if bot has to jump over a wall...
        if (CheckJump())
            m_pMyEnt->jumpnext = true;
        else // Check if bot has to jump to reach this location
        {
            float flHeightDiff = m_vHuntLocation.z - m_pMyEnt->o.z;
            bool bToHigh = false;
            if (Get2DDistance(m_vHuntLocation) <= 2.0f)
            {
                if (flHeightDiff >= 1.5f)
                {
                    if (flHeightDiff <= JUMP_HEIGHT)
                    {
#ifndef RELEASE_BUILD
                        char sz[64];
                        sprintf(sz, "OldPos z diff: %f", m_vHuntLocation.z-m_pMyEnt->o.z);
                        condebug(sz);
#endif
                        // Jump if close to pos and the pos is high
                        m_pMyEnt->jumpnext = true;
                    }
                    else
                        bToHigh = true;
                }
            }

            if (bToHigh)
            {
                m_pHuntTarget = NULL;
                m_vPrevHuntLocation = m_vHuntLocation;
                m_vHuntLocation = g_vecZero;
                m_fPrevHuntDist = 0.0f;
                m_iHuntDelay = lastmillis + 3500;
                return false;
            }
        }
    }

    return true;
}

void CBot::CheckWeaponSwitch()
{
    if(m_pMyEnt->nextweaponsel == NULL)  m_pMyEnt->weaponchanging = 0;
    if(!m_pMyEnt->weaponchanging) return;

    int timeprogress = lastmillis-m_pMyEnt->weaponchanging;
    if(timeprogress>weapon::weaponchangetime)
    {
       m_pMyEnt->prevweaponsel = m_pMyEnt->weaponsel;
       m_pMyEnt->weaponsel = m_pMyEnt->nextweaponsel;
       if(m_pMyEnt->weaponsel!=NULL) addmsg(SV_WEAPCHANGE, "ri", m_pMyEnt->weaponsel->type); // 2011jan17:ft: message possibly not needed in a local game!?!
       m_pMyEnt->weaponchanging = 0;
       m_iChangeWeaponDelay = 0;
       if(!m_pMyEnt->weaponsel->mag)
       {
          tryreload(m_pMyEnt);
       }
    }
}

void CBot::ShootEnemy()
{
    if(!m_pMyEnt->enemy) return;
    if(!IsVisible(m_pMyEnt->enemy)) return;

    m_iSawEnemyTime = lastmillis;

    // Aim to enemy
    vec enemypos = GetEnemyPos(m_pMyEnt->enemy);
    AimToVec(enemypos);

    // Time to shoot?
    if (m_iShootDelay < lastmillis)
    //if ((lastmillis-m_pMyEnt->lastaction) >= m_pMyEnt->gunwait)
    {
        if (m_pMyEnt->mag[m_pMyEnt->gunselect])
        {
            // If the bot is using a sniper only shoot if crosshair is near the enemy
            if (WeaponInfoTable[m_pMyEnt->gunselect].eWeaponType == TYPE_SNIPER)
            {
                float yawtoturn = fabs(WrapYZAngle(m_pMyEnt->yaw - m_pMyEnt->targetyaw));
                float pitchtoturn = fabs(WrapYZAngle(m_pMyEnt->pitch - m_pMyEnt->targetpitch));

                if ((yawtoturn > 5) || (pitchtoturn > 15)) // UNDONE: Should be skill based
                    return;
            }

            float flDist = GetDistance(enemypos);

            // Check if bot is in fire range
            if ((flDist < WeaponInfoTable[m_pMyEnt->gunselect].flMinFireDistance) ||
                (flDist > WeaponInfoTable[m_pMyEnt->gunselect].flMaxFireDistance))
                return;

            // Now shoot!
            m_pMyEnt->attacking = true;

            // Get the position the bot is aiming at
            vec forward, right, up, dest;
            traceresult_s tr;

            AnglesToVectors(GetViewAngles(), forward, right, up);

            dest = m_pMyEnt->o;
            forward.mul(1000);
            dest.add(forward);

            TraceLine(m_pMyEnt->o, dest, m_pMyEnt, false, &tr);
            debugbeam(m_pMyEnt->o, tr.end);

            shoot(m_pMyEnt, tr.end);

            // Add shoot delay
            m_iShootDelay = lastmillis + GetShootDelay();
        }
    }
#ifndef RELEASE_BUILD
    else
    {
        char sz[64];
        sprintf(sz, "shootdelay: %d\n", (m_iShootDelay-lastmillis));
        AddDebugText(sz);
    }
#endif
}

bool CBot::ChoosePreferredWeapon()
{
    return true;
}

int CBot::GetShootDelay()
{
    // UNDONE
    return m_pMyEnt->gunwait[m_pMyEnt->gunselect];
    if ((WeaponInfoTable[m_pMyEnt->gunselect].eWeaponType == TYPE_MELEE) ||
        (WeaponInfoTable[m_pMyEnt->gunselect].eWeaponType == TYPE_AUTO))
        return m_pMyEnt->gunwait[m_pMyEnt->gunselect];

    float flMinShootDelay = m_pBotSkill->flMinAttackDelay;
    float flMaxShootDelay = m_pBotSkill->flMaxAttackDelay;
    return max(m_pMyEnt->gunwait[m_pMyEnt->gunselect], int(RandomFloat(flMinShootDelay, flMaxShootDelay) * 1000.0f));
}

void CBot::CheckReload() // reload gun if no enemies are around
{
    if(m_pMyEnt->mag[m_pMyEnt->weaponsel->type] >= WeaponInfoTable[m_pMyEnt->weaponsel->type].sMinDesiredAmmo) return; // do not reload if mindesiredammo is satisfied
    if(m_pMyEnt->enemy && m_pMyEnt->mag[m_pMyEnt->weaponsel->type])
    {
          return; // ignore the enemy, if no ammo in mag.
    }
    tryreload(m_pMyEnt);
    return;
}

void CBot::CheckScope()
{
#define MINSCOPEDIST 15
#define MINSCOPETIME 1000
    if(m_pMyEnt->weaponsel->type != GUN_SNIPER) return;
    sniperrifle *sniper = (sniperrifle *)m_pMyEnt->weaponsel;
    if(m_pMyEnt->enemy && m_pMyEnt->o.dist(m_pMyEnt->enemy->o) > MINSCOPEDIST)
    {
        sniper->setscope(true);
    }
    else if(m_pMyEnt->scoping && lastmillis - sniper->scoped_since < MINSCOPETIME)
    {
        sniper->setscope(false);
    }
}


void CBot::MainAI()
{
    // Default bots will run forward
    m_pMyEnt->move = 1;

    // Default bots won't strafe
    m_pMyEnt->strafe = 0;

    // Whatever the bot is doing, check for needed crouch
    if(CheckCrouch()) m_pMyEnt->trycrouch = true;
    else m_pMyEnt->trycrouch = false;

    if (!BotManager.BotsShoot() && m_pMyEnt->enemy)
        m_pMyEnt->enemy = NULL; // Clear enemy when bots may not shoot

    if (m_bGoToDebugGoal) // For debugging the waypoint navigation
    {
        if (!HeadToGoal())
        {
            ResetWaypointVars();
            m_vGoal = g_vecZero;
        }
        else
            AddDebugText("Heading to debug goal...");
    }
    if (BotManager.BotsShoot() && FindEnemy()) // Combat
    {
        CheckReload();
        CheckScope();
        AddDebugText("has enemy");
        // Use best weapon
        ChoosePreferredWeapon();
        // Shoot at enemy
        ShootEnemy();

        if (m_eCurrentBotState != STATE_ENEMY)
        {
            m_vGoal = g_vecZero;
            ResetWaypointVars();
        }

        m_eCurrentBotState = STATE_ENEMY;
        if (!CheckJump())
            DoCombatNav();
    }
    else if (CheckHunt() && HuntEnemy())
    {
        CheckReload();
        CheckScope();
        AddDebugText("Hunting to %s", m_pHuntTarget->name);
        m_eCurrentBotState = STATE_HUNT;
    }
    // Heading to an interesting entity(ammo, armour etc)
    else if (CheckItems())
    {
        CheckReload();
        AddDebugText("has ent");
        m_eCurrentBotState = STATE_ENT;
    }
    else if (m_classicsp && DoSPStuff()) // Home to goal, find/follow friends etc.
    {

        AddDebugText("SP stuff");
        m_eCurrentBotState = STATE_SP;
    }
    else // Normal navigation
    {
        CheckReload();
        if (m_eCurrentBotState != STATE_NORMAL)
        {
            m_vGoal = g_vecZero;
            ResetWaypointVars();
        }

        m_eCurrentBotState = STATE_NORMAL;
        bool bDoNormalNav = true;

        AddDebugText("normal nav");

        // Make sure the bot looks straight forward and not up or down
        m_pMyEnt->pitch = 0;

        // if it is time to look for a waypoint AND if there are waypoints in this
        // level...
        if (WaypointClass.m_iWaypointCount >= 1)
        {
            // check if we need to find a waypoint...
            if (CurrentWPIsValid() == false)
            {
                if (m_iLookForWaypointTime <= lastmillis)
                {
                    // find the nearest reachable waypoint
                    waypoint_s *pWP = GetNearestWaypoint(10.0f);

                    if (pWP && (pWP != m_pCurrentWaypoint))
                    {
                        SetCurrentWaypoint(pWP);
                        condebug("New nav wp");
                        bDoNormalNav = !HeadToWaypoint();
                        if (bDoNormalNav)
                            ResetWaypointVars();
                    }
                    else
                        ResetWaypointVars();

                    m_iLookForWaypointTime = lastmillis + 250;
                }
            }
            else
            {
                bDoNormalNav = !HeadToWaypoint();
                if (bDoNormalNav)
                    ResetWaypointVars();
                AddDebugText("Using wps for nav");
            }
        }

        // If nothing special, do regular (waypointless) navigation
        if(bDoNormalNav)
        {
            // Is the bot underwater?
            if (UnderWater(m_pMyEnt->o) && WaterNav())
            {
                // Bot is under water, navigation happens in WaterNav
            }
            // Time to check the environment?
            else if (m_iCheckEnvDelay < lastmillis)
            {
                if (m_vWaterGoal!=g_vecZero) m_vWaterGoal = g_vecZero;

                // Check for stuck and strafe
                if (UnderWater(m_pMyEnt->o) || !CheckStuck())
                {
                    // Only do this when the bot is underwater or when the bot isn't stuck

                    // Check field of view (FOV)
                    CheckFOV();
                }
            }

            // Check if the bot has to strafe
            CheckStrafe();

            m_pMyEnt->move = 1;
        }
    }
}

void CBot::DoCombatNav()
{
    if (m_iCombatNavTime > lastmillis)
    {
        // If bot has a lower skill and has to turn much, wait
        if ((m_sSkillNr > 2) && (m_fYawToTurn > 90.0f))
        {
            ResetMoveSpeed();
        }
        else
        {
            SetMoveDir(m_iMoveDir, false);
        }
        return;
    }

    if (m_bCombatJump)
    {
        m_pMyEnt->jumpnext = true;
        m_bCombatJump = false;
        m_iCombatJumpDelay = lastmillis + RandomLong(1500, 2800);
        return;
    }

    m_iMoveDir = DIR_NONE;

    // Check if bot is on top of his enemy
    float r = m_pMyEnt->radius+m_pMyEnt->enemy->radius;
    if ((fabs(m_pMyEnt->enemy->o.x-m_pMyEnt->o.x)<r &&
        fabs(m_pMyEnt->enemy->o.y-m_pMyEnt->o.y)<r) &&
        ((m_pMyEnt->enemy->o.z+m_pMyEnt->enemy->aboveeye) < (m_pMyEnt->o.z + m_pMyEnt->aboveeye)))
    {
        // Try to get off him!
        condebug("On enemy!");
        TMultiChoice<int> AwayDirChoices;

        if (IsVisible(LEFT, 4.0f, false))
            AwayDirChoices.Insert(LEFT);
        if (IsVisible(RIGHT, 4.0f, false))
            AwayDirChoices.Insert(RIGHT);
        if (IsVisible(FORWARD, 4.0f, false))
            AwayDirChoices.Insert(FORWARD);
        if (IsVisible(BACKWARD, 4.0f, false))
            AwayDirChoices.Insert(BACKWARD);

        int iDir;
        if (AwayDirChoices.GetSelection(iDir))
        {
            m_iMoveDir = iDir;
            m_iCombatNavTime = lastmillis + 500;
        }
    }

    float flDist = GetDistance(m_pMyEnt->enemy->o);

    // Check for nearby items?
    if (((m_iCheckEntsDelay < lastmillis) || m_pTargetEnt) &&
        m_pBotSkill->bCanSearchItemsInCombat)
    {
        m_iCheckEntsDelay = lastmillis + 125;
        bool bSearchItems = false;

        if (m_pTargetEnt)
        {
            // Bot has already found an entity, still valid?
            vec v(m_pTargetEnt->x, m_pTargetEnt->y,
                    S(m_pTargetEnt->x, m_pTargetEnt->y)->floor+m_pMyEnt->eyeheight);
            if ((GetDistance(v) > 25.0f) || !IsVisible(m_pTargetEnt))
                m_pTargetEnt = NULL;
        }

        if (!m_pTargetEnt && (m_iCheckEntsDelay <= lastmillis))
        {
            if (WeaponInfoTable[m_pMyEnt->gunselect].eWeaponType == TYPE_MELEE)
                bSearchItems = (flDist >= 8.0f);
            else
                bSearchItems = (m_pMyEnt->ammo[m_pMyEnt->gunselect] <=
                             WeaponInfoTable[m_pMyEnt->gunselect].sMinDesiredAmmo);

            if (bSearchItems)
                m_pTargetEnt = SearchForEnts(false, 25.0f, 1.0f);
        }

        if (m_pTargetEnt)
        {
            condebug("Combat ent");
            vec v(m_pTargetEnt->x, m_pTargetEnt->y,
                    S(m_pTargetEnt->x, m_pTargetEnt->y)->floor+m_pMyEnt->eyeheight);

            debugbeam(m_pMyEnt->o, v);

            float flHeightDiff = v.z - m_pMyEnt->o.z;
            bool bToHigh = false;

            // Check he height for this ent
            if (Get2DDistance(v) <= 2.0f)
            {
                if (flHeightDiff >= 1.5f)
                {
                    if (flHeightDiff <= JUMP_HEIGHT)
                    {
#ifndef RELEASE_BUILD
                        char sz[64];
                        sprintf(sz, "Ent z diff: %f", v.z-m_pMyEnt->o.z);
                        condebug(sz);
#endif
                        m_pMyEnt->jumpnext = true; // Jump if close to ent and the ent is high
                    }
                    else
                        bToHigh = true;
                }
            }

            if (!bToHigh)
            {
                int iMoveDir = GetDirection(GetViewAngles(), m_pMyEnt->o, v);

                if (iMoveDir != DIR_NONE)
                {
                    m_iMoveDir = iMoveDir;
                    m_iCombatNavTime = lastmillis + RandomLong(125, 250);
                }

                // Check if bot needs to jump over something
                vec from = m_pMyEnt->o;
                from.z -= 1.0f;
                if (!IsVisible(from, iMoveDir, 3.0f, false))
                    m_pMyEnt->jumpnext = true;

                return;
            }
        }
    }

    // High skill and enemy is close?
    if ((m_sSkillNr <= 1) && (m_fYawToTurn < 80.0f) && (flDist <= 20.0f) &&
        (m_iCombatJumpDelay < lastmillis))
    {
        // Randomly jump a bit, to avoid some basic firepower ;)

        // How lower the distance to the enemy, how higher the chance for a jump
        short sJumpPercent = (100 - ((short)flDist * 8));
        if (RandomLong(1, 100) <= sJumpPercent)
        {
            // Choose a nice direction to jump to

            // Is the enemy close?
            if ((GetDistance(m_pMyEnt->enemy->o) <= 4.0f) ||
                (WeaponInfoTable[m_pMyEnt->gunselect].eWeaponType == TYPE_MELEE))
            {
                m_iMoveDir = FORWARD; // Jump forward
                SetMoveDir(FORWARD, false);
                m_bCombatJump = true;
            }
            else if (WeaponInfoTable[m_pMyEnt->gunselect].eWeaponType != TYPE_MELEE)// else jump to a random direction
            {
                /*
                    Directions to choose:
                    - Forward-right
                    - Right
                    - Backward-right
                    - Backward
                    - Backward-left
                    - Left
                    - Forward-left

                */

                TMultiChoice<int> JumpDirChoices;
                short sForwardScore = ((flDist > 8.0f) || (flDist < 4.0f)) ? 20 : 10;
                short sBackwardScore = (flDist <= 6.0f) ? 20 : 10;
                short sStrafeScore = (flDist < 6.0f) ? 20 : 10;

                if (IsVisible((FORWARD | LEFT), 4.0f, false))
                    JumpDirChoices.Insert((FORWARD | LEFT), sForwardScore);
                if (IsVisible((FORWARD | RIGHT), 4.0f, false))
                    JumpDirChoices.Insert((FORWARD | RIGHT), sForwardScore);
                if (IsVisible(BACKWARD, 4.0f, false))
                    JumpDirChoices.Insert(BACKWARD, sBackwardScore);
                if (IsVisible((BACKWARD | LEFT), 4.0f, false))
                    JumpDirChoices.Insert((BACKWARD | LEFT), sBackwardScore);
                if (IsVisible((BACKWARD | RIGHT), 4.0f, false))
                    JumpDirChoices.Insert((BACKWARD | RIGHT), sBackwardScore);
                if (IsVisible(LEFT, 4.0f, false))
                    JumpDirChoices.Insert(LEFT, sStrafeScore);
                if (IsVisible(RIGHT, 4.0f, false))
                    JumpDirChoices.Insert(RIGHT, sStrafeScore);

                int JumpDir;
                if (JumpDirChoices.GetSelection(JumpDir))
                {
                    m_iMoveDir = JumpDir;
                    SetMoveDir(JumpDir, false);
                    m_bCombatJump = true;
                }
            }

            if (m_bCombatJump)
            {
                m_iCombatNavTime = lastmillis + RandomLong(125, 250);
                return;
            }
        }
    }

    if (WeaponInfoTable[m_pMyEnt->gunselect].eWeaponType == TYPE_MELEE)
        return; // Simply walk towards enemy if using a melee type

    flDist = Get2DDistance(m_pMyEnt->enemy->o);

    // Out of desired range for current weapon?
    if ((flDist <= WeaponInfoTable[m_pMyEnt->gunselect].flMinDesiredDistance) ||
        (flDist >= WeaponInfoTable[m_pMyEnt->gunselect].flMaxDesiredDistance))
    {
        if (flDist >= WeaponInfoTable[m_pMyEnt->gunselect].flMaxDesiredDistance)
        {
            m_iMoveDir = FORWARD;
        }
        else
        {
            m_iMoveDir = BACKWARD;
        }

        vec src, forward, right, up, dest, MyAngles = GetViewAngles(), o = m_pMyEnt->o;
        traceresult_s tr;

        // Is it furthest or farthest? bleh
        float flFurthestDist = 0;
        int bestdir = -1, dir = 0;
        bool moveback = (m_pMyEnt->move == -1);

        for(int j=-45;j<=45;j+=45)
        {
            src = MyAngles;
            src.y = WrapYZAngle(src.y + j);
            src.x = 0.0f;

            // If we're moving backwards, trace backwards
            if (moveback)
                src.y = WrapYZAngle(src.y + 180);

            AnglesToVectors(src, forward, right, up);

            dest = o;
            forward.mul(40);
            dest.add(forward);

            TraceLine(o, dest, m_pMyEnt, false, &tr);

            //debugbeam(origin, end);
            flDist = GetDistance(tr.end);

            if (flFurthestDist < flDist)
            {
                flFurthestDist = flDist;
                bestdir = dir;
            }
            dir++;
        }

        switch(bestdir)
        {
        case 0:
            if (moveback)
                m_iMoveDir |= RIGHT; // Strafe right
            else
                m_iMoveDir |= LEFT; // Strafe left
            break;
        case 2:
            if (moveback)
                m_iMoveDir |= LEFT; // Strafe left
            else
                m_iMoveDir |= RIGHT; // Strafe right
            break;
        }

        if (m_iMoveDir != DIR_NONE)
        {
            SetMoveDir(m_iMoveDir, false);
            m_iCombatNavTime = lastmillis + 500;
        }
    }
    else if (m_pBotSkill->bCircleStrafe) // Circle strafe when in desired range...
    {
        traceresult_s tr;
        vec angles, end, forward, right, up;
        TMultiChoice<int> StrafeDirChoices;

        // Check the left side...
        angles = GetViewAngles();
        angles.y = WrapYZAngle(angles.y - 75.0f); // Not 90 degrees because the bot
                                          // doesn't strafe in a straight line
                                          // (aims still to enemy).

        AnglesToVectors(angles, forward, right, up);
        end = m_pMyEnt->o;
        forward.mul(15.0f);
        end.add(forward);

        TraceLine(m_pMyEnt->o, end, m_pMyEnt, true, &tr);
        StrafeDirChoices.Insert(LEFT, (int)GetDistance(m_pMyEnt->o, tr.end));

        // Check the right side...
        angles = GetViewAngles();
        angles.y = WrapYZAngle(angles.y + 75.0f); // Not 90 degrees because the bot
                                          // doesn't strafe in a straight line
                                          // (aims still to enemy).

        AnglesToVectors(angles, forward, right, up);
        end = m_pMyEnt->o;
        forward.mul(15.0f);
        end.add(forward);

        TraceLine(m_pMyEnt->o, end, m_pMyEnt, true, &tr);
        StrafeDirChoices.Insert(RIGHT, (int)GetDistance(m_pMyEnt->o, tr.end));

        int StrafeDir;
        if (StrafeDirChoices.GetSelection(StrafeDir))
        {
            m_iMoveDir = StrafeDir;
            SetMoveDir(StrafeDir, false);
            m_iCombatNavTime = lastmillis + RandomLong(1500, 3000);
        }
    }
    else // Bot can't circle strafe(low skill), just stand still
        ResetMoveSpeed();
}

bool CBot::CheckStuck()
{
    if (m_iStuckCheckDelay + (CheckCrouch() ? 2000 : 0) >= lastmillis)
        return false;

    if ((m_vGoal!=g_vecZero) && (GetDistance(m_vGoal) < 2.0f))
        return false;

    bool IsStuck = false;

    vec CurPos = m_pMyEnt->o, PrevPos = m_vPrevOrigin;
    CurPos.z = PrevPos.z = 0;
    // Did the bot hardly move the last frame?
    if (GetDistance(CurPos, PrevPos) <= 0.1f)
    {
        if (m_bStuck)
        {
            if (m_iStuckTime < lastmillis)
                IsStuck = true;
        }
        else
        {
            m_bStuck = true;
            m_iStuckTime = lastmillis + 1000;
        }
    }
    else
    {
        m_bStuck = false;
        m_iStuckTime = 0;
    }

    if (IsStuck)
    {
#ifndef RELEASE_BUILD
        char msg[64];
        sprintf(msg, "stuck (%f)", GetDistance(m_vPrevOrigin));
        condebug(msg);
#endif

        m_bStuck = false;
        m_iStuckTime = 0;

        // Crap bot is stuck, lets just try some random things

        // Check if the bot can turn around
        vec src = GetViewAngles();
        src.x = 0;
        vec forward, right, up, dir, dest;
        traceresult_s tr;

        AnglesToVectors(src, forward, right, up);

        // Check the left side...
        dir = right;
        dest = m_pMyEnt->o;
        dir.mul(3);
        dest.sub(dir);

        TraceLine(m_pMyEnt->o, dest, m_pMyEnt, false, &tr);
        //debugbeam(m_pMyEnt->o, end);

        if (!tr.collided)
        {
            // Bot can turn left, do so
            m_pMyEnt->targetyaw = WrapYZAngle(m_pMyEnt->yaw - 90);
            m_iStuckCheckDelay = m_iCheckEnvDelay = lastmillis + 500;
            return true;
        }

        // Check the right side...
        dir = right;
        dest = m_pMyEnt->o;
        dir.mul(3);
        dest.add(dir);

        TraceLine(m_pMyEnt->o, dest, m_pMyEnt, true, &tr);
        //debugbeam(m_pMyEnt->o, end);

        if (!tr.collided)
        {
            // Bot can turn right, do so
            m_pMyEnt->targetyaw = WrapYZAngle(m_pMyEnt->yaw + 90);
            m_iStuckCheckDelay = m_iCheckEnvDelay = lastmillis + 500;
            return true;
        }

        // Check if bot can turn 180 degrees
        dir = forward;
        dest = m_pMyEnt->o;
        dir.mul(3);
        dest.add(dir);

        TraceLine(m_pMyEnt->o, dest, m_pMyEnt, true, &tr);
        //debugbeam(m_pMyEnt->o, end);

        if (!tr.collided)
        {
            // Bot can turn around, do so
            m_pMyEnt->targetyaw = WrapYZAngle(m_pMyEnt->yaw + 180);
            m_iStuckCheckDelay = m_iCheckEnvDelay = lastmillis + 500;
            return true;
        }

        // Bleh bot couldn't turn, lets just randomly jump :|

        condebug("Randomly avoiding stuck...");
        if (RandomLong(0, 2) == 0)
            m_pMyEnt->jumpnext = true;
        else
            m_pMyEnt->targetyaw = WrapYZAngle(m_pMyEnt->yaw + RandomLong(60, 160));
        return true;
    }

    return false;
}

// Check if a near wall is blocking and we can jump over it
bool CBot::CheckJump()
{
    bool bHasGoal = m_vGoal!=g_vecZero;
    float flGoalDist = (bHasGoal) ? GetDistance(m_vGoal) : 0.0f;

//    if ((bHasGoal) && (flGoalDist < 2.0f))
  //       return false; UNDONE?

    vec start = m_pMyEnt->o;
    float flTraceDist = 3.0f;

    if (bHasGoal && (flGoalDist < flTraceDist))
        flTraceDist = flGoalDist;

    // Something blocks at eye hight?
    if (!IsVisible(start, FORWARD, flTraceDist, false))
    {
        // Check if the bot can jump over it
        start.z += (JUMP_HEIGHT - 1.0f);
        if (IsVisible(start) && !IsVisible(start, FORWARD, flTraceDist, false))
        {
            // Jump
            debugnav("High wall");
            m_pMyEnt->jumpnext = true;
            return true;
        }
    }
    else
    {
        // Check if something is blocking at feet height, so the bot can jump over it
        start.z -= 1.7f;

        // Trace was blocked?
        if (!IsVisible(start, FORWARD, flTraceDist, false))
        {
            //debugbeam(start, end);

            // Jump
            debugnav("Low wall");
            m_pMyEnt->jumpnext = true;
            return true;
        }
    }
    return false; // Bot didn't had to jump(or couldn't)
}

bool CBot::CheckCrouch()
{
    bool bHasGoal = m_vGoal!=g_vecZero;
    float flGoalDist = (bHasGoal) ? GetDistance(m_vGoal) : 0.0f;

    vec start = m_pMyEnt->o;
    vec crouch = vec(0, 0, 2.0f);
    float flTraceDist = 3.0f;

    if (bHasGoal && (flGoalDist < flTraceDist))
       flTraceDist = flGoalDist;

    if (!IsVisible(vec(start).add(crouch), FORWARD, flTraceDist, false) && IsVisible(vec(start).sub(crouch), FORWARD, flTraceDist, false)) return true;
    return false;
}

bool CBot::CheckStrafe()
{
    if (m_iStrafeTime >= lastmillis)
    {
        SetMoveDir(m_iMoveDir, true);
        return true;
    }

    if (m_iStrafeCheckDelay >= lastmillis)
        return false;

    // Check for near walls
    traceresult_s tr;
    vec from = m_pMyEnt->o, to, forward, right, up, dir;
    float flLeftDist = -1.0f, flRightDist = -1.0f;
    bool bStrafe = false;
    int iStrafeDir = DIR_NONE;

    AnglesToVectors(GetViewAngles(), forward, right, up);

    // Check for a near left wall
    to = from;
    dir = right;
    dir.mul(3.0f);
    to.sub(dir);
    TraceLine(from, to, m_pMyEnt, false, &tr);
    if (tr.collided)
        flLeftDist = GetDistance(from, tr.end);
    //debugbeam(m_pMyEnt->o, to);

    // Check for a near right wall
    to = from;
    dir = right;
    dir.mul(3.0f);
    to.add(dir);
    TraceLine(from, to, m_pMyEnt, false, &tr);
    if (tr.collided)
        flRightDist = GetDistance(from, tr.end);
    //debugbeam(m_pMyEnt->o, to);

    if ((flLeftDist == -1.0f) && (flRightDist == -1.0f))
    {
        dir = right;
        dir.mul(m_pMyEnt->radius);

        // Check left
        from = m_pMyEnt->o;
        from.sub(dir);
        if (IsVisible(from, FORWARD, 3.0f, false, &flLeftDist))
            flLeftDist = -1.0f;

        // Check right
        from = m_pMyEnt->o;
        from.add(dir);
        if (IsVisible(from, FORWARD, 3.0f, false, &flRightDist))
            flRightDist = -1.0f;
    }

    if ((flLeftDist != -1.0f) && (flRightDist != -1.0f))
    {
        if (flLeftDist < flRightDist)
        {
            // Strafe right
            bStrafe = true;
            iStrafeDir = RIGHT;
        }
        else if (flRightDist < flLeftDist)
        {
            // Strafe left
            bStrafe = true;
            iStrafeDir = LEFT;
        }
        else
        {
            // Randomly choose a strafe direction
            bStrafe = true;
            if (RandomLong(0, 1))
                iStrafeDir = LEFT;
            else
                iStrafeDir = RIGHT;
        }
    }
    else if (flLeftDist != -1.0f)
    {
        // Strafe right
        bStrafe = true;
        iStrafeDir = RIGHT;
    }
    else if (flRightDist != -1.0f)
    {
        // Strafe left
        bStrafe = true;
        iStrafeDir = LEFT;
    }

    if (bStrafe)
    {
        SetMoveDir(iStrafeDir, true);
        m_iMoveDir = iStrafeDir;
        m_iStrafeTime = lastmillis + RandomLong(75, 150);
    }

    return bStrafe;
}

void CBot::CheckFOV()
{
    m_iCheckEnvDelay = lastmillis + RandomLong(125, 250);
    vec MyAngles = GetViewAngles();
    vec src, forward, right, up, dest, best(0, 0, 0);
    vec origin = m_pMyEnt->o;
    float flDist, flFurthestDist = 0;
    bool WallLeft = false;
    traceresult_s tr;

    //origin.z -= 1.5; // Slightly under eye level

    // Scan 90 degrees FOV
    for(int angle=-45;angle<=45;angle+=5)
    {
        src = MyAngles;
        src.y = WrapYZAngle(src.y + angle);

        AnglesToVectors(src, forward, right, up);

        dest = origin;
        forward.mul(40);
        dest.add(forward);

        TraceLine(origin, dest, m_pMyEnt, false, &tr);

        //debugbeam(origin, end);
        flDist = GetDistance(tr.end);

        if (flFurthestDist < flDist)
        {
            flFurthestDist = flDist;
            best = tr.end;
        }
    }

    if (best.x && best.y && best.z)
    {
        AimToVec(best);
        // Update MyAngles, since their (going to be) change(d)
        MyAngles.x = m_pMyEnt->targetpitch;
        MyAngles.y = m_pMyEnt->targetyaw;
    }

    float flNearestHitDist = GetDistance(best);

    if (!UnderWater(m_pMyEnt->o) && m_pMyEnt->onfloor)
    {
        // Check if a near wall is blocking and we can jump over it
        if (flNearestHitDist < 4)
        {
            // Check if the bot can jump over it
            src = MyAngles;
            src.x = 0;

            AnglesToVectors(src, forward, right, up);

            vec start = origin;
            start.z += 2.0f;
            dest = start;
            forward.mul(6);
            dest.add(forward);

            TraceLine(start, dest, m_pMyEnt, false, &tr);
            //debugbeam(start, end);

            if (!tr.collided)
            {
                // Jump
                debugnav("High wall");
                m_pMyEnt->jumpnext = true;
                m_iStrafeCheckDelay = lastmillis + RandomLong(250, 500);
                return;
            }
        }
        else
        {
            // Check if something is blocking below us, so the bot can jump over it
            src = MyAngles;
            src.x = 0;

            AnglesToVectors(src, forward, right, up);

            vec start = origin;
            start.z -= 1.7f;
            dest = start;
            forward.mul(4);
            dest.add(forward);

            TraceLine(start, dest, m_pMyEnt, false, &tr);

            // Trace was blocked?
            if (tr.collided)
            {
                //debugbeam(start, end);

                // Jump
                debugnav("Low wall");
                m_pMyEnt->jumpnext = true;
                m_iStrafeCheckDelay = lastmillis + RandomLong(250, 500);
                return;
            }
        }

        // Check if the bot is going to fall...
        src = MyAngles;
        src.x = 0.0f;
        AnglesToVectors(src, forward, right, up);

        dest = origin;
        forward.mul(3.0f);
        dest.add(forward);

        TraceLine(origin, dest, m_pMyEnt, false, &tr);

        int cx = int(tr.end.x), cy = int(tr.end.y);
        short CubesInWater=0;
        for(int x=cx-1;x<=(cx+1);x++)
        {
            for(int y=cy-1;y<=(cy+1);y++)
            {
                if (OUTBORD(x, y)) continue;
                //sqr *s = S(fast_f2nat(x), fast_f2nat(y));
                //if (!SOLID(s))
                {
                    vec from(x, y, m_pMyEnt->o.z);
                    dest = from;
                    dest.z -= 6.0f;
                    TraceLine(from, dest, m_pMyEnt, false, &tr);
                    bool turn = false;
                    if (UnderWater(tr.end)) CubesInWater++;
                    if (CubesInWater > 2) turn = true; // Always avoid water
                    if (!tr.collided && RandomLong(0, 1))
                        turn = true; // Randomly avoid a fall

                    if (turn)
                    {
                        m_pMyEnt->targetyaw = WrapYZAngle(m_pMyEnt->yaw + 180);
                        m_iCheckEnvDelay = m_iStrafeCheckDelay = lastmillis + RandomLong(750, 1500);
                        debugnav("Water or a fall in front");
                        return;
                    }
                }
            }
        }
    }

    // Is the bot about to head a corner?
    if (flNearestHitDist <= 4.0f)
    {
        src = MyAngles;
        src.y = WrapYZAngle(src.y - 45.0f);
        AnglesToVectors(src, forward, right, up);

        dest = origin;
        forward.mul(4.0f);
        dest.add(forward);

        TraceLine(origin, dest, m_pMyEnt, false, &tr);

        WallLeft = (tr.collided);

        src = MyAngles;
        src.y += WrapYZAngle(src.y + 45.0f);
        AnglesToVectors(src, forward, right, up);

        dest = origin;
        forward.mul(4.0f);
        dest.add(forward);

        TraceLine(origin, dest, m_pMyEnt, false, &tr);

        if (WallLeft && tr.collided)
        {
            // We're about to hit a corner, turn away
            debugnav("Corner");
            m_pMyEnt->targetyaw = WrapYZAngle(m_pMyEnt->yaw + RandomFloat(160.0f, 200.0f));
            m_iCheckEnvDelay = m_iStrafeCheckDelay = lastmillis + RandomLong(750, 1500);
            return;
        }
    }
}

// Called when bot is underwater
bool CBot::WaterNav()
{
    const int iSearchRange = 4;

    if (m_vWaterGoal==g_vecZero)
    {
        AddDebugText("WaterNav");
        // Find the nearest and reachable cube which isn't underwater

        int cx = int(m_pMyEnt->o.x);
        int cy = int(m_pMyEnt->o.y);
        float flNearestDist = 9999.0f, flDist;

        if (OUTBORD(cx, cy)) return false;

        // Check all cubes in range...
        for (int x=(cx-iSearchRange);x<=(cx+iSearchRange);x++)
        {
            for (int y=(cy-iSearchRange);y<=(cy+iSearchRange);y++)
            {
                sqr *s = S(x, y);

                if (SOLID(s)) continue;
                if ((x==cx) && (y==cy)) continue;

                vec v(x, y, GetCubeFloor(x, y));

                if (UnderWater(v)) continue; // Skip, cube is underwater

                if (hdr.waterlevel < (v.z - 2.0f)) continue; // Cube is too high

                // Check if the bot 'can fit' on the cube(no near obstacles)
                bool small_ = false;

                for (int a=(x-2);a<=(x+2);a++)
                {
                    if (small_) break;
                    for (int b=(y-2);b<=(y+2);b++)
                    {
                        if ((x==a) && (y==b)) continue;
                        vec v2(a, b, GetCubeFloor(a, b));
                        if (v.z < (v2.z-1-JUMP_HEIGHT))
                        {
                            small_=true;
                            break;
                        }

                        if ((a >= (x-1)) && (a <= (x+1)) && (b >= (y-1)) && (b <= (y+1)))
                        {
                            if ((v2.z) < (v.z-2.0f))
                            {
                                small_ = true;
                                break;
                            }
                        }

                        traceresult_s tr;
                        TraceLine(v, v2, NULL, false, &tr);
                        if (tr.collided)
                        {
                            small_=true;
                            break;
                        }
                    }
                    if (small_) break;
                }
                if (small_)
                {
                    debugbeam(m_pMyEnt->o, v);
                    continue;
                }

                // Okay, cube is valid.
                flDist = GetDistance(v);
                if (flDist < flNearestDist)
                {
                    flNearestDist = flDist;
                    m_vWaterGoal = v;
                }
            }
        }
    }

    if (m_vWaterGoal!=g_vecZero)
    {
        AddDebugText("WaterNav");
        //debugbeam(m_pMyEnt->o, m_vWaterGoal);
        vec aim = m_vWaterGoal;
        aim.z += 1.5f; // Aim a bit further up
        AimToVec(aim);
        if ((RandomLong(1, 100) <= 15) && (Get2DDistance(m_vWaterGoal) <= 7.0f))
            m_pMyEnt->jumpnext = true;
        return true;
    }

    return false;
}

bool CBot::CheckItems()
{
    if (!m_pCurrentGoalWaypoint && !CheckJump() && CheckStuck())
    {
        // Don't check for ents a while when stuck
        m_iCheckEntsDelay = lastmillis + RandomLong(1000, 2000);
        return false;
    }

    if (m_vGoal==g_vecZero)
        m_pTargetEnt = NULL;

    if (!m_pTargetEnt)
    {
        if (m_iCheckEntsDelay > lastmillis)
            return false;
        else
        {
            m_pTargetEnt = SearchForEnts(!m_classicsp);
            m_iCheckEntsDelay = lastmillis + RandomLong(2500, 5000);
        }
    }

    if (m_pTargetEnt)
    {
        if (HeadToTargetEnt())
            return true;
    }

    if (m_eCurrentBotState == STATE_ENT)
    {
        ResetWaypointVars();
        m_vGoal = g_vecZero;
        m_pTargetEnt = NULL;
    }

    return false;
}

bool CBot::InUnreachableList(entity *e)
{
    TLinkedList<unreachable_ent_s *>::node_s *p = m_UnreachableEnts.GetFirst();
    while(p)
    {
        if (p->Entry->ent == e) return true;
        p = p->next;
    }
    return false;
}

bool CBot::IsReachable(vec to, float flMaxHeight)
{
    vec from = m_pMyEnt->o;
    traceresult_s tr;
    float curr_height, last_height;

    float distance = GetDistance(from, to);

    // is the destination close enough?
    //if (distance < REACHABLE_RANGE)
    {
        if (IsVisible(to))
        {
            // Look if bot can 'fit trough'
            vec src = from, forward, right, up;
            AnglesToVectors(GetViewAngles(), forward, right, up);

            // Trace from 1 cube to the left
            vec temp = right;
            temp.mul(1.0f);
            src.sub(temp);
            if (!::IsVisible(src, to)) return false;

            // Trace from 1 cube to the right
            src.add(temp);
            if (!::IsVisible(src, to)) return false;

            if (UnderWater(from) && UnderWater(to))
            {
                // No need to worry about heights in water
                return true;
            }
/*
            if (to.z > (from.z + JUMP_HEIGHT))
            {
                vec v_new_src = to;
                vec v_new_dest = to;

                v_new_dest.z = v_new_dest.z - (JUMP_HEIGHT + 1.0f);

                // check if we didn't hit anything, if so then it's in mid-air
                if (::IsVisible(v_new_src, v_new_dest, NULL))
                {
                    condebug("to is in midair");
                    debugbeam(from, to);
                    return false;  // can't reach this one
                }
            }
*/

            // check if distance to ground increases more than jump height
            // at points between from and to...

            vec v_temp = to;
            v_temp.sub(from);
            vec v_direction = Normalize(v_temp);  // 1 unit long
            vec v_check = from;
            vec v_down = from;

            v_down.z = v_down.z - 100.0f;  // straight down

            TraceLine(v_check, v_down, NULL, false, &tr);

              // height from ground
            last_height = GetDistance(v_check, tr.end);

            distance = GetDistance(to, v_check);  // distance from goal

            while (distance > 2.0f)
            {
                // move 2 units closer to the goal
                v_temp = v_direction;
                v_temp.mul(2.0f);
                v_check.add(v_temp);

                v_down = v_check;
                v_down.z = v_down.z - 100.0f;

                TraceLine(v_check, v_down, NULL, false, &tr);

                curr_height = GetDistance(v_check, tr.end);

                // is the difference in the last height and the current height
                // higher that the jump height?
                if ((last_height - curr_height) >= flMaxHeight)
                {
                    // can't get there from here...
                    //condebug("traces failed to to");
                    debugbeam(from, to);
                    return false;
                }

                last_height = curr_height;

                distance = GetDistance(to, v_check);  // distance from goal
            }

            return true;
        }
    }

    return false;
}

void CBot::HearSound(int n, vec *o)
{
    // Has the bot already an enemy?
    if (m_pMyEnt->enemy) return;


    //fixmebot
    // Is the sound not interesting?
    if(n == S_DIE1 || n == S_DIE2) return;

    int soundvol = m_pBotSkill->iMaxHearVolume -
                       (int)(GetDistance(*o)*3*m_pBotSkill->iMaxHearVolume/255);

    if (soundvol == 0) return;

    // Look who made the sound(check for the nearest enemy)
    float flDist, flNearestDist = 3.0f; // Range of 3 units
    playerent *pNearest = NULL;

        // Check all players first
        loopv(players)
        {
            playerent *d = players[i];

            if (d == m_pMyEnt || !d || (d->state != CS_ALIVE) ||
                isteam(m_pMyEnt->team, d->team))
                continue;

            flDist = GetDistance(*o, d->o);
            if ((flDist < flNearestDist) && IsVisible(d))
            {
                pNearest = d;
                flNearestDist = flDist;
            }
        }

        // Check local player
        if (player1 && (player1->state == CS_ALIVE) &&
            !isteam(m_pMyEnt->team, player1->team))
        {
            flDist = GetDistance(*o, player1->o);
            if ((flDist < flNearestDist) && IsVisible(player1))
            {
                pNearest = player1;
                flNearestDist = flDist;
            }
        }

    if (pNearest)
    {
        if (m_pMyEnt->enemy != pNearest)
            m_iShootDelay = lastmillis + GetShootDelay(); // Add shoot delay when new enemy found

        m_pMyEnt->enemy = pNearest;
    }
}

bool CBot::IsInFOV(const vec &o)
{
    vec target, dir, forward, right, up;
    float flDot, flAngle;

    AnglesToVectors(GetViewAngles(), forward, right, up);

    // direction the bot is aiming at
    dir = forward;
    dir.z = 0; // Make 2D

    // ideal direction 
    target = o;
    target.sub(m_pMyEnt->o);
    target.z = 0.0f; // Make 2D

    // angle between these two directions
    flDot = target.dot(dir);
    flAngle = acos(flDot/(target.magnitude() * dir.magnitude()));

    return m_pBotSkill->iFov/2.0f >= flAngle/RAD;
} 
// Code of CBot - End
