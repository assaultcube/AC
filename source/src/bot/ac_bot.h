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

#define MAX_WEAPONS      10

class CACBot: public CBot
{
public:
     friend class CBotManager;
     friend class CWaypointClass;

     virtual void CheckItemPickup(void);

     // AI Functions
     virtual bool ChoosePreferredWeapon(void);
     void Reload(int Gun);
     virtual entity *SearchForEnts(bool bUseWPs, float flRange=9999.0f,
                                   float flMaxHeight=JUMP_HEIGHT);
     virtual bool HeadToTargetEnt(void);
     virtual bool DoSPStuff(void);

     virtual void Spawn(void);
};

inline void AddScreenText(const char *t, ...) {} // UNDONE
inline void AddDebugText(const char *t, ...)
{
#ifdef _DEBUG
    va_list v;
    va_start(v, t);
    conoutf(t,v);
    va_end(v);
#endif
}




#endif

#endif
