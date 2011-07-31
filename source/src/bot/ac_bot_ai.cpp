//
// C++ Implementation: ac_bot_ai
//
// Description: The AI part of the bot for ac is here(navigation, shooting etc)
//
//
// Author:  <rickhelmus@gmail.com>
//


#include "cube.h"
#include "bot.h"

#ifdef AC_CUBE

weaponinfo_s WeaponInfoTable[MAX_WEAPONS] =
{
    // DD: desired distance
    // FD: fire distance
    // mA: min desired Ammo
    // ---- Type : -- minDD -- maxDD -- minFD -- maxFD -- mA
    { TYPE_MELEE,      0.0f,    5.0f,   0.0f,    3.5f,   0 }, // KNIFE
    { TYPE_NORMAL,     3.5f,   15.0f,   3.0f,   60.0f,   6 }, // PISTOL
    { TYPE_SNIPER,    15.0f,   80.0f,   3.0f,  180.0f,   2 }, // CARBINE    // min and max FD just guesstimates for now
    { TYPE_SHOTGUN,    0.0f,   15.0f,   0.0f,   50.0f,   3 }, // SHOTGUN
    { TYPE_AUTO,       0.0f,   25.0f,   3.0f,   60.0f,  10 }, // SUBGUN
    { TYPE_SNIPER,    10.0f,  125.0f,   3.0f,  200.0f,   3 }, // SNIPER
    { TYPE_AUTO,      20.0f,   80.0f,   3.0f,  150.0f,   6 }, // ASSAULT
    { TYPE_AUTO,       0.0f,    0.0f,   3.0f,    0.0f,   6 }, // CPISTOL
    { TYPE_GRENADE,    5.0f,   30.0f,   3.0f,   50.0f,   1 }, // GRENADE
    { TYPE_AUTO,       0.0f,   50.0f,   3.0f,   50.0f,   0 }  // AKIMBO
};

// Code of CACBot - Start

bool CACBot::ChoosePreferredWeapon()
{
    short bestWeapon = m_pMyEnt->gunselect;
    short bestWeaponScore = 0;

    short sWeaponScore;
    float flDist = GetDistance(m_pMyEnt->enemy->o);

    // Choose a weapon
    for(int i=0;i<MAX_WEAPONS;i++)
    {
        sWeaponScore = 0; // Minimal score for a weapon
        sWeaponScore = i > 1 ? 5 : 0; // Primary are usually better
        
        if (!m_pMyEnt->mag[i] && WeaponInfoTable[i].eWeaponType != TYPE_MELEE)
        {
             continue;
        }
        else
        {
             sWeaponScore += 5;
        }

        if((flDist >= WeaponInfoTable[i].flMinDesiredDistance) &&
            (flDist <= WeaponInfoTable[i].flMaxDesiredDistance))
        {
                // In desired range for this weapon
            sWeaponScore += 5; // Increase score much

            if(i == GUN_PISTOL || WeaponInfoTable[i].eWeaponType == TYPE_MELEE)
            {
                if(WeaponInfoTable[m_pMyEnt->primary].eWeaponType == TYPE_SNIPER)
                {
                    sWeaponScore += 10; // At a close range, knife & pistol are strong with a sniper like primary weapon
                }
                if(WeaponInfoTable[i].eWeaponType == TYPE_MELEE && WeaponInfoTable[m_pMyEnt->primary].eWeaponType == TYPE_SHOTGUN)
                {
                    sWeaponScore -= 2; // Penalize a bit knife on close range with shotgun
                }
            }
        }
        else if ((flDist < WeaponInfoTable[i].flMinFireDistance) ||
                (flDist > WeaponInfoTable[i].flMaxFireDistance))
        {
            continue; // Wrong distance for this weapon
        }

        if(i == GUN_GRENADE)
        {
            sWeaponScore += 30; // Nades have high priority
        }

        // The ideal distance would be between the Min and Max desired distance.
        // Score on the difference of the avarage of the Min and Max desired distance.
        float flAvarage = (WeaponInfoTable[i].flMinDesiredDistance +
                        WeaponInfoTable[i].flMaxDesiredDistance) / 2.0f;
        float flIdealDiff = fabs(flDist - flAvarage);

        if (flIdealDiff < 0.5f) // Close to ideal distance
            sWeaponScore += 4;
        else if (flIdealDiff <= 1.0f)
            sWeaponScore += 2;

        // Now rate the weapon on available ammo...
        if (WeaponInfoTable[i].sMinDesiredAmmo > 0)
        {
            // Calculate how much percent of the min desired ammo the bot has
            float flDesiredPercent = (float(m_pMyEnt->ammo[i]) /
                                 float(WeaponInfoTable[i].sMinDesiredAmmo)) *
                                 100.0f;

            if (flDesiredPercent >= 400.0f)
                sWeaponScore += 4;
            else if (flDesiredPercent >= 200.0f)
                sWeaponScore += 3;
            else if (flDesiredPercent >= 100.0f)
                sWeaponScore += 1;
        }
        else
        {
            // Not needing ammo is an advantage...
            sWeaponScore += 10;
        }

        if(sWeaponScore > bestWeaponScore)
        {
            bestWeaponScore = sWeaponScore;
            bestWeapon = i;
        }
    }

    if (WeaponInfoTable[m_pMyEnt->gunselect].eWeaponType == TYPE_ROCKET)
    {
        m_bShootAtFeet = true;
    
    }
    if(lastmillis > m_iChangeWeaponDelay)
    {
        SelectGun(bestWeapon);
    }
    return true;
};

void CACBot::Reload(int Gun)
{

};

entity *CACBot::SearchForEnts(bool bUseWPs, float flRange, float flMaxHeight)
{
    /* Entities are scored on the following things:
        - Visibility
        - For ammo: Need(ie has this bot much of this type or not)
        - distance
    */

    float flDist;
    entity *pNewTargetEnt = NULL;
    waypoint_s *pWptNearBot = NULL, *pBestWpt = NULL;
    short sScore, sHighestScore = 0;

    if ((WaypointClass.m_iWaypointCount >= 1) && bUseWPs)
        pWptNearBot = GetNearestWaypoint(15.0f);

#ifdef WP_FLOOD
    if (!pWptNearBot && bUseWPs)
        pWptNearBot = GetNearestFloodWP(5.0f);
#endif

    loopv(ents)
    {
        sScore = 0;
        entity &e = ents[i];
        vec o(e.x, e.y, S(e.x, e.y)->floor+player1->eyeheight);

        if (!ents[i].spawned) continue;
        if (OUTBORD(e.x, e.y)) continue;

        bool bInteresting = false;
        short sAmmo = 0, sMaxAmmo = 0;

        switch(e.type)
        {
            case I_CLIPS:
                sMaxAmmo = ammostats[GUN_PISTOL].max;
                bInteresting = (m_pMyEnt->ammo[GUN_PISTOL]<sMaxAmmo);
                sAmmo = m_pMyEnt->ammo[GUN_PISTOL];
                break;
            case I_AMMO:
                sMaxAmmo = ammostats[m_pMyEnt->primary].max;
                bInteresting = (m_pMyEnt->ammo[m_pMyEnt->primary]<sMaxAmmo);
                sAmmo = m_pMyEnt->ammo[m_pMyEnt->primary];
                break;
            case I_GRENADE:
                sMaxAmmo = ammostats[GUN_GRENADE].max;
                bInteresting = (m_pMyEnt->mag[GUN_GRENADE]<sMaxAmmo);
                sAmmo = -1;
                break;
            case I_HEALTH:
                sMaxAmmo = powerupstats[0].max;
                bInteresting = (m_pMyEnt->health < sMaxAmmo);
                sAmmo = m_pMyEnt->health;
                break;
            case I_HELMET:
            case I_ARMOUR:
               sMaxAmmo = powerupstats[I_ARMOUR-I_HEALTH].max;
               bInteresting = (m_pMyEnt->armour < sMaxAmmo);
               sAmmo = m_pMyEnt->armour;
               break;
            case I_AKIMBO:
               bInteresting = !m_pMyEnt->akimbo;
               sAmmo = -1;
               break;
        };

        if (!bInteresting)
            continue; // Not an interesting item, skip

        // Score on ammo and need
        // Akimbo & nade
        if (sAmmo == -1)
        {
            sScore += 75; // Bonus
        }
        else
        {
            // Calculate current percentage of max ammo
            float percent = ((float)sAmmo / (float)sMaxAmmo) * 100.0f;
            if (percent > 100.0f) percent = 100.0f;
            sScore += ((100 - short(percent))/2);
        }

        flDist = GetDistance(o);

        if (flDist > flRange) continue;

        // Score on distance
        float f = flDist;
        if (f > 100.0f) f = 100.0f;
        sScore += ((100 - short(f)) / 2);

        waypoint_s *pWptNearEnt = NULL;
        // If this entity isn't visible check if there is a nearby waypoint
        if (!IsReachable(o, flMaxHeight))//(!IsVisible(o))
        {
            if (!pWptNearBot) continue;

#ifdef WP_FLOOD
            if (pWptNearBot->pNode->iFlags & W_FL_FLOOD)
                pWptNearEnt = GetNearestFloodWP(o, 8.0f);
            else
#endif
                pWptNearEnt = GetNearestWaypoint(o, 15.0f);

            if (!pWptNearEnt) continue;
        }

        // Score on visibility
        if (pWptNearEnt == NULL) // Ent is visible
            sScore += 30;
        else
            sScore += 15;

        if (sScore > sHighestScore)
        {
            // Found a valid wp near the bot and the ent,so...lets store it :)
            if (pWptNearEnt)
                pBestWpt = pWptNearEnt;
            else
                pBestWpt = NULL; // Best ent so far doesn't need any waypoints

            sHighestScore = sScore;
            pNewTargetEnt = &ents[i];
        }
    }

    if (pNewTargetEnt)
    {
        // Need waypoints to reach it?
        if (pBestWpt)
        {
            ResetWaypointVars();
            SetCurrentWaypoint(pWptNearBot);
            SetCurrentGoalWaypoint(pBestWpt);
        }

        m_vGoal.x = pNewTargetEnt->x;
        m_vGoal.y = pNewTargetEnt->y;
        m_vGoal.z = S(pNewTargetEnt->x, pNewTargetEnt->y)->floor+player1->eyeheight;
    }

    return pNewTargetEnt;
}

bool CACBot::HeadToTargetEnt()
{
    if (m_pTargetEnt)
    {
        vec o(m_pTargetEnt->x, m_pTargetEnt->y,
                S(m_pTargetEnt->x, m_pTargetEnt->y)->floor+m_pMyEnt->eyeheight);

        if (m_pTargetEnt->spawned && (!UnderWater(m_pMyEnt->o) ||
            !UnderWater(o)))
        {
            bool bIsVisible = false;
            if (m_pCurrentGoalWaypoint)
            {
                if ((GetDistance(o) <= 20.0f) && IsReachable(o, 1.0f))
                    bIsVisible = true;
                else if (HeadToGoal())
                {
                    //debugbeam(m_pMyEnt->o, m_pCurrentWaypoint->pNode->v_origin);
                    //debugbeam(m_pMyEnt->o,
                    //          m_pCurrentGoalWaypoint->pNode->v_origin);
                    AddDebugText("Using WPs for ents");
                    return true;
                }
            }
            else
                bIsVisible = IsVisible(o);

            if (bIsVisible)
            {
                if (m_pCurrentWaypoint || m_pCurrentGoalWaypoint)
                {
                    condebug("ent is now visible");
                    ResetWaypointVars();
                }

                float flHeightDiff = o.z - m_pMyEnt->o.z;
                bool bToHigh = false;
                if (Get2DDistance(o) <= 2.0f)
                {
                    if (flHeightDiff >= 1.5f)
                    {
                        if (flHeightDiff <= JUMP_HEIGHT)
                        {
#ifndef RELEASE_BUILD
                            char sz[64];
                            sprintf(sz, "Ent z diff: %f", o.z-m_pMyEnt->o.z);
                            condebug(sz);
#endif
                            // Jump if close to ent and the ent is high
                            m_pMyEnt->jumpnext = true;
                        }
                        else
                            bToHigh = true;
                    }
                }

                if (!bToHigh)
                {
                    AimToVec(o);
                    return true;
                }
            }
        }
    }

    return false;
}

bool CACBot::DoSPStuff()
{
    return false;
}

// Code of CACBot - End

#endif
