//
// C++ Implementation: bot_waypoint.h
//
// Description: Containts all defintions for bot waypoint code
//
//
// Author: Rick Helmus <rickhelmus@gmail.com>
//

// Enable this for flood support
#define WP_FLOOD

#ifndef BOT_WAYPOINT_H
#define BOT_WAYPOINT_H

#define WAYPOINT_VERSION 2
#define EXP_WP_VERSION 1
#define REACHABLE_RANGE 15.0f
#define MAX_MAP_GRIDS    64
#define SECTOR_SIZE      18

#define W_FL_TELEPORT         (1<<1) // used if waypoint is at a teleporter
#define W_FL_TELEPORTDEST     (1<<2) // used if waypoint is at a teleporter destination
#define W_FL_FLOOD            (1<<3) // used if this waypoint is made by flooding the map
#define W_FL_JUMP             (1<<4) // if set, bot jumps when reached this waypoint
#define W_FL_TRIGGER          (1<<5) // used if this waypoint is at a trigger
#define W_FL_INTAG            (1<<6) // used if this waypoint is in a tagged cube

#define MAX_FLOODWPCONNECTDIST     2
#define MAX_FLOODWPDIST            4.0f

struct waypoint_header_s
{
    char szFileType[10];
    int iFileVersion;
    int iFileFlags;  // not currently used
    int iWPCount;
    char szMapName[32];  // name of map for these waypoints
};

// Old waypoint structure, used for waypoint version 1
struct waypoint_version_1_s
{
     int iFlags;
     vec v_origin;
     TLinkedList<waypoint_version_1_s *> ConnectedWPs;
     
     // A* stuff
     float f, g;
     waypoint_version_1_s *pParent;
     
     // Construction
     waypoint_version_1_s(void) : iFlags(0), f(0.0f), g(0.0f), pParent(NULL) { };     
};

struct node_s
{
     vec v_origin;     
     int iFlags;
     short sTriggerNr;
     short sYaw;
     short sCost; // Base and static cost
     TLinkedList<node_s *> ConnectedWPs;
     TLinkedList<node_s *> ConnectedWPsWithMe;
     
     TLinkedList<node_s *> FailedGoalList;
          
     // Construction
     node_s(void) : iFlags(0), sTriggerNr(0), sYaw(-1), sCost(10) { };
     node_s(const vec &o, const int &f, const short t=0, const short y=-1) : v_origin(o),
                                                                             iFlags(f),
                                                                             sTriggerNr(t),
                                                                             sYaw(y),
                                                                             sCost(0) { };
};

struct waypoint_s
{
     node_s *pNode;
     short g[2];
     waypoint_s *pParent[2];
     bool bIsOpen[2], bIsClosed[2];
     
     // Construction
     waypoint_s(void) : pNode(NULL) { bIsOpen[0] = bIsOpen[1] = bIsClosed[0] =
                                      bIsClosed[1] = false; pParent[0] =  pParent[1] = NULL;
                                      g[0] = g[1] = 0; };     
};

class CWaypointClass
{
protected:
     bool m_bDrawWaypoints;
     bool m_bDrawWPPaths;
     bool m_bAutoWaypoint;
     bool m_bAutoPlacePaths;
     bool m_bDrawWPText;
     vec m_vLastCreatedWP;
     float m_fPathDrawTime;
     char m_szMapName[32];
     
#ifdef WP_FLOOD
     bool m_bFlooding; // True if flooding map with waypoints
     bool m_bFilteringNodes;
     int m_iFloodStartTime;
     int m_iCurFloodX, m_iCurFloodY;
     int m_iFloodSize;
     int m_iFilteredNodes;
#endif        
     friend class CBot;
     
public:
     TLinkedList<node_s *> m_Waypoints[MAX_MAP_GRIDS][MAX_MAP_GRIDS];
     int m_iWaypointCount; // number of waypoints currently in use

     CWaypointClass(void);
     virtual ~CWaypointClass(void) { Clear(); };

     void Think(void);
     void Init(void);
     void Clear(void);
     bool LoadWaypoints(void);
     void SaveWaypoints(void);
     bool LoadWPExpFile(void);
     void SaveWPExpFile(void);
     void SetMapName(const char *map) { strcpy(m_szMapName, map); };

     void SetWaypointsVisible(bool Visible) { m_bDrawWaypoints = Visible; };
     void SetPathsVisible(bool Visible) { m_bDrawWPPaths = Visible; };
     void SetAutoWaypoint(bool On) { m_bAutoWaypoint = On; };
     void SetWPInfo(bool on) { m_bDrawWPText = on; };
     void SetWPFlags(node_s *wp, int iFlags) { wp->iFlags |= iFlags; };
     void UnsetWPFlags(node_s *wp, int iFlags) { wp->iFlags &= ~iFlags; };
     void SetWPTriggerNr(node_s *wp, short sTriggerNr) { wp->sTriggerNr = sTriggerNr; };
     void SetWPYaw(node_s *wp, short sYaw) { wp->sYaw = sYaw; };
     bool WaypointsAreVisible(void) { return m_bDrawWaypoints; };
     node_s *AddWaypoint(vec o, bool connectwp);
     void DeleteWaypoint(vec v_src);
     void AddPath(node_s *pWP1, node_s *pWP2);
     void DeletePath(node_s *pWP);
     void DeletePath(node_s *pWP1, node_s *pWP2);
     void ManuallyCreatePath(vec v_src, int iCmd, bool TwoWay);
     void ManuallyDeletePath(vec v_src, int iCmd, bool TwoWay);

     bool WPIsReachable(vec from, vec to);
     //node_s *GetNearestWaypoint(vec v_src, float flRange, bool CheckVisible = true, bool SkipFloodWPs = true);
     node_s *GetNearestWaypoint(vec v_src, float flRange);
     node_s *GetNearestWaypoint(dynent *d, float flRange) { return GetNearestWaypoint(d->o, flRange); };
     node_s *GetNearestTriggerWaypoint(vec v_src, float flRange);
     node_s *GetWaypointFromVec(const vec &v_src);
     void DrawNearWaypoints();
     void GetNodeIndexes(const vec &v_origin, short *i, short *j);
     void CalcCost(node_s *pNode);
     void ReCalcCosts(void);

     
#ifdef WP_FLOOD
     // Flood functions
     virtual void StartFlood(void);
     void StopFlood(void);
     void FloodThink(void);
     bool CanPlaceNodeHere(const vec &from);
     void ConnectFloodWP(node_s *pWP);
     node_s *GetNearestFloodWP(vec v_origin, float flRange, node_s *pIgnore, bool SkipTags=false);
     node_s *GetNearestFloodWP(dynent *d, float flRange) { return GetNearestFloodWP(d->o, flRange, NULL); };
     node_s *GetNearestTriggerFloodWP(vec v_origin, float flRange);
#endif
};

class CACWaypointClass: public CWaypointClass
{
public:
#ifdef WP_FLOOD
     // Flood functions
     virtual void StartFlood(void);
#endif
};

class CCubeWaypointClass: public CWaypointClass
{
public:
     void CreateWPsAtTeleporters(void);
     void CreateWPsAtTriggers(void);

#ifdef WP_FLOOD
     // Flood functions
     virtual void StartFlood(void);
#endif
};

#endif // BOT_WAYPOINT_H
