//
// C++ Implementation: bot
//
// Description: 
//
// Header specific for AC_CUBE
//
// Author:  Rick <rickhelmus@gmail.com>
//
//
//

#ifndef AC_BOT_H
#define AC_BOT_H

#ifdef AC_CUBE

#define MAX_WEAPONS      7

class CACBot: public CBot
{
public:
     friend class CBotManager;
     friend class CWaypointClass;

     virtual void CheckItemPickup(void);
     virtual void PickUp(int n);
     virtual void AddItem(int i, int &v, int spawnsec);
     
     // AI Functions
     virtual bool ChoosePreferredWeapon(void);
     void Reload(int Gun);
     virtual entity *SearchForEnts(bool bUseWPs, float flRange=9999.0f,
                                   float flMaxHeight=JUMP_HEIGHT);
     virtual bool HeadToTargetEnt(void);
     virtual bool DoSPStuff(void);
        
     virtual void Spawn(void);
     virtual void BotPain(int damage, dynent *d);
};

inline void AddScreenText(char *t, ...) {}; // UNDONE
inline void AddDebugText(char *t, ...) {}; // UNDONE

#endif

#endif
