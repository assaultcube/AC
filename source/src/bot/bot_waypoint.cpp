//
// C++ Implementation: bot_waypoint
//
// Description: Waypoint class for bots
//
//
// Author: Rick <rickhelmus@gmail.com>
//

#include "cube.h"
#include "bot.h"

vec v_debuggoal = g_vecZero;

#if defined AC_CUBE
CACWaypointClass WaypointClass;
#elif defined VANILLA_CUBE
CCubeWaypointClass WaypointClass;
#endif

VAR(xhairwpsel, 0, 1, 1);
// FIXME: multiple selections support ?
// well, TBH I haven't groked this myself yet, but the following curselection segfaults the client, so I'm just putting in a sane-variant until someone groks it ;-) - ft 2011sep28
extern vector<block> sels;
//#define curselection (xhairwpsel ? vec(sels.last().x, sels.last().y, S(sels.last().x, sels.last().y)->floor+2.0f) : vec(player1->o.x, player1->o.y, player1->o.z))
#define curselection (vec(player1->o.x, player1->o.y, player1->o.z))

// Waypoint class begin

CWaypointClass::CWaypointClass(void) : m_bDrawWaypoints(false), m_bDrawWPPaths(true),
                                       m_bAutoWaypoint(false),  m_bAutoPlacePaths(true),
                                       m_bDrawWPText(false), m_vLastCreatedWP(g_vecZero),
                                       m_bFlooding(false), m_bFilteringNodes(false),
                                       m_iFloodStartTime(0), m_iCurFloodX(0),
                                       m_iCurFloodY(0), m_iFloodSize(0),
                                       m_iFilteredNodes(0), m_iWaypointCount(0)
{
     loopi(MAX_MAP_GRIDS)
     {
          loopj(MAX_MAP_GRIDS)
          {
               while(!m_Waypoints[i][j].Empty())
                    delete m_Waypoints[i][j].Pop();
          }
     }

     m_szMapName[0] = 0;
}

void CWaypointClass::Init()
{
     Clear();  // Free previously allocated path memory

     loopi(MAX_MAP_GRIDS)
     {
          loopj(MAX_MAP_GRIDS)
          {
               while(!m_Waypoints[i][j].Empty())
                    delete m_Waypoints[i][j].Pop();
          }
     }

     m_fPathDrawTime = 0.0;
     m_iWaypointCount = 0;
     m_vLastCreatedWP = g_vecZero;
}

void CWaypointClass::Clear()
{
     for(int i=0;i<MAX_MAP_GRIDS;i++)
     {
          for(int j=0;j<MAX_MAP_GRIDS;j++)
          {
               while(!m_Waypoints[i][j].Empty())
               {
                    node_s *p = m_Waypoints[i][j].Pop();
                    p->ConnectedWPs.DeleteAllNodes();
                    p->ConnectedWPsWithMe.DeleteAllNodes();
                    BotManager.DelWaypoint(p);
                    delete p;
               }
          }
     }
}

//fixme
TLinkedList<node_s *>::node_s *testx;

// returns true if waypoints succesfull loaded
bool CWaypointClass::LoadWaypoints()
{
     stream *bfp;
     char szWPFileName[64];
     char filename[256];
     waypoint_header_s header;
     short num, triggernr, yaw;
     int i, j, flags;
     vec from, to;

     strcpy(szWPFileName, m_szMapName);
     strcat(szWPFileName, ".wpt");

     BotManager.MakeBotFileName(szWPFileName, "waypoints", NULL, filename);

     bfp = openfile(filename, "rb");

     BotManager.m_sCurrentTriggerNr = -1;

     // if file exists, read the waypoint structure from it
     if (bfp != NULL)
     {
          bfp->read(&header, sizeof(header));

          header.szFileType[sizeof(header.szFileType)-1] = 0;
          if (strcmp(header.szFileType, "cube_bot") == 0)
          {
               header.szMapName[sizeof(header.szMapName)-1] = 0;

               if (strcmp(header.szMapName, m_szMapName) == 0)
               {
                    Init();  // remove any existing waypoints

                    // Check which waypoint file version this is, handle each of them different
                    if (header.iFileVersion == 1)
                    {
                         // First version, works with an big array and has a limit

                         waypoint_version_1_s WPs[1024];
                         int path_index;

                         m_iWaypointCount = header.iWPCount;
                         for (i=0; i < header.iWPCount; i++)
                         {
                              bfp->read(&WPs[i], sizeof(WPs[0]));
                              WPs[i].ConnectedWPs.Reset(); // We get garbage when we read this from
                                                           // a file, so just clear it

                              // Convert to new waypoint structure
                              node_s *pNode = new node_s(WPs[i].v_origin, WPs[i].iFlags, 0);

                              short x, y;
                              GetNodeIndexes(pNode->v_origin, &x, &y);

                              m_Waypoints[x][y].AddNode(pNode);
                              BotManager.AddWaypoint(pNode);
                         }

                         // read and add waypoint paths...
                         for (i=0; i < m_iWaypointCount; i++)
                         {
                              // read the number of paths from this node...
                              bfp->read(&num, sizeof(num));

                              // See which waypoint the current is
                              node_s *pCurrent = GetWaypointFromVec(WPs[i].v_origin);

                              if (!pCurrent)
                              {
                                   conoutf("Error: NULL path in waypoint file");
                                   continue;
                              }

                              for (j=0; j < num; j++)
                              {
                                   bfp->read(&path_index, sizeof(path_index));

                                   // See which waypoint this is
                                   node_s *pTo = GetWaypointFromVec(WPs[path_index].v_origin);

                                   if (!pTo)
                                   {
                                        conoutf("Error: NULL path in waypoint file");
                                        continue;
                                   }

                                   AddPath(pCurrent, pTo);
                              }
                         }
                         conoutf("Old waypoint version(%d) converted to new version(%d)",
                                  header.iFileVersion, WAYPOINT_VERSION);
                         conoutf("Use the wpsave command to convert permently");

                    }
                    else if (header.iFileVersion == 2)
                    {
                         m_iWaypointCount = header.iWPCount;

                         for (i=0; i < header.iWPCount; i++)
                         {
                              bfp->read(&from, sizeof(from)); // Read origin
                              bfp->read(&flags, sizeof(flags)); // Read waypoint flags
                              bfp->read(&triggernr, sizeof(triggernr)); // Read trigger nr
                              bfp->read(&yaw, sizeof(yaw)); // Read target yaw

                              node_s *pNode = new node_s(from, flags, triggernr, yaw);

                              short x, y;
                              GetNodeIndexes(pNode->v_origin, &x, &y);

                              m_Waypoints[x][y].AddNode(pNode);
                              BotManager.AddWaypoint(pNode);

                              if ((BotManager.m_sCurrentTriggerNr == -1) ||
                                  (triggernr < BotManager.m_sCurrentTriggerNr))
                                  BotManager.m_sCurrentTriggerNr = triggernr;
                         }

                         for (i=0; i < header.iWPCount; i++)
                         {
                              // read the number of paths from this node...
                              bfp->read(&num, sizeof(num));
                              bfp->read(&from, sizeof(from));

                              if (!num)
                                   continue;

                              node_s *pCurrent = GetWaypointFromVec(from);

                              if (!pCurrent)
                              {
                                   conoutf("Error: NULL path in waypoint file");

                                   for(j=0;j<num;j++) bfp->read(&to, sizeof(to)); // Read rest of block
                                   continue;
                              }

                              for (j=0; j < num; j++)
                              {
                                   bfp->read(&to, sizeof(to));
                                   node_s *p = GetWaypointFromVec(to);
                                   if (p)
                                        AddPath(pCurrent, p);
                              }
                         }
                    }
                    else if (header.iFileVersion > WAYPOINT_VERSION)
                    {
                         conoutf("Error: Waypoint file is newer then current, upgrade cube bot.");
                         delete bfp;
                         return false;
                    }
                    else
                    {
                         conoutf("Error: Unknown waypoint version for cube bot");
                         delete bfp;
                         return false;
                    }
               }
               else
               {
                    conoutf("Waypoints aren't for map %s but for map %s", m_szMapName,
                                  header.szMapName);
                    delete bfp;
                    return false;
               }
          }
          else
          {
               conoutf("Waypoint file isn't for cube bot");
               //conoutf("Header FileType: %s", int(header.szFileType));

               delete bfp;
               return false;
          }

          delete bfp;

          //RouteInit();
     }
     else
     {
          conoutf("Waypoint file %s does not exist", (filename));
          return false;
     }

     if (BotManager.m_sCurrentTriggerNr == -1)
          BotManager.m_sCurrentTriggerNr = 0;

     ReCalcCosts();

     conoutf("Waypoints for map %s loaded", (m_szMapName));

     testx = m_Waypoints[1][1].pNodeList;

     return true;
}

void CWaypointClass::SaveWaypoints()
{
     char filename[256];
     char mapname[64];
     waypoint_header_s header;
     short int num;
     TLinkedList<node_s *>::node_s *pPath;

     strcpy(header.szFileType, "cube_bot");

     header.iFileVersion = WAYPOINT_VERSION;

     header.iFileFlags = 0;  // not currently used

     header.iWPCount = m_iWaypointCount;

     memset(header.szMapName, 0, sizeof(header.szMapName));
     strncpy(header.szMapName, m_szMapName, sizeof(header.szMapName)-1);
     header.szMapName[sizeof(header.szMapName)-1] = 0;

     strcpy(mapname, m_szMapName);
     strcat(mapname, ".wpt");

     BotManager.MakeBotFileName(mapname, "waypoints", NULL, filename);

     stream *bfp = openfile(filename, "wb");

     if (!bfp)
     {
          conoutf("Error writing waypoint file, check if the directory \"bot/waypoint\" exists and "
                       "the right permissions are set");
          return;
     }

     // write the waypoint header to the file...
     bfp->write(&header, sizeof(header));

     // write the waypoint data to the file...
     loopi(MAX_MAP_GRIDS)
     {
          loopj(MAX_MAP_GRIDS)
          {
               TLinkedList<node_s *>::node_s *p = m_Waypoints[i][j].GetFirst();
               while(p)
               {
                    bfp->write(&p->Entry->v_origin, sizeof(p->Entry->v_origin)); // Write origin
                    bfp->write(&p->Entry->iFlags, sizeof(p->Entry->iFlags)); // Write waypoint flags
                    bfp->write(&p->Entry->sTriggerNr, sizeof(p->Entry->sTriggerNr)); // Write trigger nr
                    bfp->write(&p->Entry->sYaw, sizeof(p->Entry->sYaw)); // Write target yaw

                    p = p->next;
               }
          }
     }

     loopi(MAX_MAP_GRIDS)
     {
          loopj(MAX_MAP_GRIDS)
          {
               TLinkedList<node_s *>::node_s *p = m_Waypoints[i][j].GetFirst();
               while(p)
               {
                    // save the waypoint paths...

                    // count the number of paths from this node...
                    pPath = p->Entry->ConnectedWPs.GetFirst();
                    num = p->Entry->ConnectedWPs.NodeCount();

                    bfp->write(&num, sizeof(num));  // write the count
                    bfp->write(&p->Entry->v_origin, sizeof(p->Entry->v_origin)); // write the origin of this path

                    // now write out each path...
                    while (pPath != NULL)
                    {
                         bfp->write(&pPath->Entry->v_origin, sizeof(pPath->Entry->v_origin));
                         pPath = pPath->next;  // go to next node in linked list
                    }
                    p = p->next;
               }
          }
     }

     delete bfp;
}

bool CWaypointClass::LoadWPExpFile()
{
     FILE *bfp;
     char szWPFileName[64];
     char filename[256];
     waypoint_header_s header;
     short int num;
     int i, j;
     vec from, to;

     if (m_iWaypointCount == 0)
          return false;

     strcpy(szWPFileName, m_szMapName);
     strcat(szWPFileName, ".exp");

     BotManager.MakeBotFileName(szWPFileName, "waypoints", NULL, filename);

     bfp = fopen(filename, "rb");

     // if file exists, read the waypoint structure from it
     if (bfp != NULL)
     {
          fread(&header, sizeof(header), 1, bfp);

          header.szFileType[sizeof(header.szFileType)-1] = 0;
          if (strcmp(header.szFileType, "cube_bot") == 0)
          {
               header.szMapName[sizeof(header.szMapName)-1] = 0;

               if (strcmp(header.szMapName, m_szMapName) == 0)
               {
                    // Check which waypoint file version this is, handle each of them different
                    if (header.iFileVersion == 1)
                    {
                         for (i=0; i < header.iWPCount; i++)
                         {
                              // read the number of paths from this node...
                              fread(&num, sizeof(num), 1, bfp);
                              fread(&from, sizeof(vec), 1, bfp);

                              if (!num) continue;

                              node_s *pCurrent = GetWaypointFromVec(from);

                              if (!pCurrent)
                              {
                                   conoutf("Error: NULL node in waypoint experience file");
                                   continue;
                              }

                              for (j=0; j < num; j++)
                              {
                                   fread(&to, sizeof(vec), 1, bfp);
                                   node_s *p = GetWaypointFromVec(to);
                                   if (p)
                                   {
                                        pCurrent->FailedGoalList.AddNode(p);
                                   }
                              }
                         }
                    }
                    else if (header.iFileVersion > EXP_WP_VERSION)
                    {
                         conoutf("Error: Waypoint experience file is newer then current, upgrade cube bot.");
                         fclose(bfp);
                         return false;
                    }
                    else
                    {
                         conoutf("Error: Unknown waypoint experience file version for cube bot");
                         fclose(bfp);
                         return false;
                    }
               }
               else
               {
                    conoutf("Waypoint experience file isn't for map %s but for map %s", (m_szMapName),
                                  (header.szMapName));
                    fclose(bfp);
                    return false;
               }
          }
          else
          {
               conoutf("Waypoint experience file isn't for cube bot");
               //conoutf("Header FileType: %s", int(header.szFileType));

               fclose(bfp);
               return false;
          }

          fclose(bfp);

          //RouteInit();
     }
     else
     {
          conoutf("Waypoint experience file %s does not exist", (filename));
          return false;
     }

     conoutf("Waypoint experience file for map %s loaded", (m_szMapName));
     return true;
}

void CWaypointClass::SaveWPExpFile()
{
     if (!m_iWaypointCount)
          return;

     char filename[256];
     char mapname[64];
     waypoint_header_s header;
     short int num;

     strcpy(header.szFileType, "cube_bot");

     header.iFileVersion = EXP_WP_VERSION;

     header.iFileFlags = 0;  // not currently used

     header.iWPCount = m_iWaypointCount;

     memset(header.szMapName, 0, sizeof(header.szMapName));
     strncpy(header.szMapName, m_szMapName, 31);
     header.szMapName[31] = 0;

     strcpy(mapname, m_szMapName);
     strcat(mapname, ".exp");

     BotManager.MakeBotFileName(mapname, "waypoints", NULL, filename);

     FILE *bfp = fopen(filename, "wb");

     if (!bfp)
     {
          conoutf("Error writing waypoint experience file, check if the directory \"bot/waypoint\" exists and "
                       "the right permissions are set");
          return;
     }

     // write the waypoint header to the file...
     fwrite(&header, sizeof(header), 1, bfp);

     // save the waypoint experience data...
     loopi(MAX_MAP_GRIDS)
     {
          loopj(MAX_MAP_GRIDS)
          {
               TLinkedList<node_s *>::node_s *p = m_Waypoints[i][j].GetFirst();
               while(p)
               {
                    // count the number of paths from this node...
                    TLinkedList<node_s *>::node_s *p2 = p->Entry->FailedGoalList.GetFirst();
                    num = p->Entry->FailedGoalList.NodeCount();

                    if (!num || !p2) continue;

                    fwrite(&num, sizeof(num), 1, bfp);  // write the count
                    fwrite(&p->Entry->v_origin, sizeof(vec), 1, bfp); // write the origin of this node

                    while (p2 != NULL)
                    {
                         // Write out the node which a bot can't reach with the current node
                         fwrite(&p2->Entry->v_origin, sizeof(vec), 1, bfp);
                         p2 = p2->next;  // go to next node in linked list
                    }
                    p = p->next;
               }
          }
     }

     fclose(bfp);
}

void CWaypointClass::Think()
{
     if (dedserv) return;

#ifdef WP_FLOOD
     FloodThink();
#endif

     if (m_bAutoWaypoint) // is auto waypoint on?
     {
          // find the distance from the last used waypoint
          float flDistance = GetDistance(m_vLastCreatedWP, player1->o);

          bool NoLastCreatedWP = ((m_vLastCreatedWP.x == 0) && (m_vLastCreatedWP.y == 0) &&
                                                    (m_vLastCreatedWP.z == 0));

          if ((flDistance > 3.0f) || NoLastCreatedWP)
          {
               // check that no other reachable waypoints are nearby...
               if (!GetNearestWaypoint(player1, 10.0f))
               {
                    AddWaypoint(player1->o, true);  // place a waypoint here
               }
          }
     }

     // Draw info for nearest waypoint?
     if (m_bDrawWPText)
     {
          node_s *nearestwp = GetNearestWaypoint(player1, 20.0f);

#ifdef WP_FLOOD
          if (!nearestwp)
               nearestwp = GetNearestFloodWP(player1, 10.0f);
#endif

          if (nearestwp)
          {
               char szWPInfo[256];
               sprintf(szWPInfo, "Distance nearest waypoint: %f", GetDistance(player1->o, nearestwp->v_origin));
               AddScreenText(szWPInfo);

               strcpy(szWPInfo, "Flags: ");
               if (nearestwp->iFlags & W_FL_TELEPORT)
                    strcat(szWPInfo, "Teleport ");
               if (nearestwp->iFlags & W_FL_TELEPORTDEST)
                    strcat(szWPInfo, "Teleport destination ");
               if (nearestwp->iFlags & W_FL_JUMP)
                    strcat(szWPInfo, "Jump ");
               if (nearestwp->iFlags & W_FL_TRIGGER)
               {
                    char sz[32];
                    sprintf(sz, "Trigger(nr %d) ", nearestwp->sTriggerNr);
                    strcat(szWPInfo, sz);
               }
               if (nearestwp->iFlags & W_FL_INTAG)
                    strcat(szWPInfo, "In tagged cube(s) ");
               if (strlen(szWPInfo) == 7)
                    strcat(szWPInfo, "None");

               AddScreenText(szWPInfo);

               if (nearestwp->sYaw != -1)
                    AddScreenText("yaw %d", nearestwp->sYaw);

               sprintf(szWPInfo, "Waypoint has %d connections",
                       nearestwp->ConnectedWPs.NodeCount());
               AddScreenText(szWPInfo);
               AddScreenText("Base Cost: %d", nearestwp->sCost);
          }
     }

     if (m_bDrawWaypoints)  // display the waypoints if turned on...
     {
          DrawNearWaypoints();
     }
}

void CWaypointClass::DrawNearWaypoints()
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_CULL_FACE);

     TLinkedList<node_s *>::node_s *pNode;
     node_s *pNearest = NULL;
     short i, j, MinI, MaxI, MinJ, MaxJ, Offset = (short)ceilf(15.0f / MAX_MAP_GRIDS);
     float flNearestDist = 9999.99f, flDist;

     GetNodeIndexes(player1->o, &i, &j);
     MinI = i - Offset;
     MaxI = i + Offset;
     MinJ = j - Offset;
     MaxJ = j + Offset;

     if (MinI < 0)
          MinI = 0;
     if (MaxI > MAX_MAP_GRIDS - 1)
          MaxI = MAX_MAP_GRIDS - 1;
     if (MinJ < 0)
          MinJ = 0;
     if (MaxJ > MAX_MAP_GRIDS - 1)
          MaxJ = MAX_MAP_GRIDS - 1;

     node_s *nearestwp = WaypointClass.GetNearestWaypoint(curselection, 20.0f);

#ifdef WP_FLOOD
     if (!nearestwp)
          nearestwp = GetNearestFloodWP(player1, 20.0f);
#endif

     for (int x=MinI;x<=MaxI;x++)
     {
          for(int y=MinJ;y<=MaxJ;y++)
          {
               pNode = m_Waypoints[x][y].GetFirst();

               while(pNode)
               {
                    flDist = GetDistance(worldpos, pNode->Entry->v_origin);
                    if (flDist <= 15)
                    {
                         vec o = pNode->Entry->v_origin;
                         vec e = o;
                         o.z -= 2;
                         e.z += 2;

                         if (pNode->Entry->iFlags & W_FL_JUMP)
                         {
                              // draw a red waypoint
                              linestyle(2.5f, 0xFF, 0x40, 0x40);
                         }
                         else if (nearestwp == pNode->Entry)
                         {
                              // draw a green waypoint
                              linestyle(2.5f, 0x40, 0xFF, 0x40);
                         }
                         else
                         {
                              // draw a blue waypoint
                              linestyle(2.5f, 0x40, 0x40, 0xFF);
                         }

                         line(int(o.x), int(o.y), int(o.z), int(e.x), int(e.y), int(e.z));

                         if (flNearestDist > flDist)
                         {
                              flNearestDist = flDist;
                              pNearest = pNode->Entry;
                         }
                    }

                    pNode = pNode->next;
               }
          }
     }

     if (pNearest)
     {
          pNode = pNearest->ConnectedWPs.GetFirst();

          linestyle(2.0f, 0xFF, 0xFF, 0xFF);

          while(pNode)
          {
               vec o = pNode->Entry->v_origin;
               vec e = pNearest->v_origin;

               line(int(o.x), int(o.y), int(o.z), int(e.x), int(e.y), int(e.z));

               pNode = pNode->next;
          }

          // Has this waypoint an target yaw?
          if (pNearest->sYaw != -1)
          {
               vec angles(0, pNearest->sYaw, 0);
               vec from = pNearest->v_origin, end = pNearest->v_origin;
               vec forward, right, up;

               from.z = end.z = (pNearest->v_origin.z-1.0f);

               AnglesToVectors(angles, forward, right, up);
               forward.mul(5.0f);
               end.add(forward);

               linestyle(2.0f, 0xFF, 0x40, 0x40);
               line(int(from.x), int(from.y), int(from.z), int(end.x), int(end.y), int(end.z));
          }
     }

#ifndef RELEASE_BUILD
     // check if path waypointing is on...
     if (m_bDrawWPPaths)
     {
          // Draw path from first bot
          if (bots.length() && bots[0] && bots[0]->pBot && bots[0]->pBot->m_pCurrentWaypoint &&
               bots[0]->pBot->m_pCurrentGoalWaypoint)
          {
               CBot *pB = bots[0]->pBot;
               if (!pB->m_bCalculatingAStarPath && !pB->m_AStarNodeList.Empty())
               {
                    TLinkedList<waypoint_s *>::node_s *pNode = pB->m_AStarNodeList.GetFirst(), *pNext;

                    linestyle(2.5f, 0xFF, 0x40, 0x40);

                    line((int)pB->m_pCurrentWaypoint->pNode->v_origin.x,
                           (int)pB->m_pCurrentWaypoint->pNode->v_origin.y,
                           (int)pB->m_pCurrentWaypoint->pNode->v_origin.z,
                           (int)pNode->Entry->pNode->v_origin.x,
                           (int)pNode->Entry->pNode->v_origin.y,
                           (int)pNode->Entry->pNode->v_origin.z);

                    while(pNode && pNode->next)
                    {
                         pNext = pNode->next;
                         vec &v1 = pNode->Entry->pNode->v_origin;
                         vec &v2 = pNext->Entry->pNode->v_origin;

                         line(int(v1.x), int(v1.y), int(v1.z), int(v2.x), int(v2.y), int(v2.z));

                         pNode = pNode->next;
                    }
               }
          }
     }
#endif

    glEnable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

     if (intermission) return;

     /*for(int i=0;i<MAX_STORED_LOCATIONS;i++)
     {
          if (player1->PrevLocations.prevloc[i]==g_vecZero) continue;
          vec v1 = player1->PrevLocations.prevloc[i];
          v1.z -= 1.0f;
          vec v2 = v1;
          v2.z += 2.0f;
          linestyle(2.5f, 0xFF, 0x40, 0x40);
          line(int(v1.x), int(v1.y), int(v1.z), int(v2.x), int(v2.y), int(v2.z));
     }*/
}

// Add waypoint at location o, returns pointer of created wp
node_s *CWaypointClass::AddWaypoint(vec o, bool connectwp)
{
     short x, y;
     int flags = 0;
     if (S((int)o.x, (int)o.y)->tag) flags |= W_FL_INTAG;

     node_s *pNode = new node_s(o, flags, 0);
     m_vLastCreatedWP = o;

     GetNodeIndexes(o, &x, &y);

     m_Waypoints[x][y].AddNode(pNode);
     BotManager.AddWaypoint(pNode);

     m_iWaypointCount++;

     if (connectwp && m_bAutoPlacePaths)
     {
          // Connect new waypoint with other near waypoints.

          loopi(MAX_MAP_GRIDS)
          {
               loopj(MAX_MAP_GRIDS)
               {
                    TLinkedList<node_s *>::node_s *p = m_Waypoints[i][j].GetFirst();

                    while(p)
                    {
                         if (p->Entry == pNode)
                         {
                              p = p->next;
                              continue;  // skip the waypoint that was just added
                         }

                         // check if the waypoint is reachable from the new one (one-way)
                         if (WPIsReachable(o, p->Entry->v_origin))
                              AddPath(pNode, p->Entry); // Add a path from a to b
                         if (WPIsReachable(p->Entry->v_origin, pNode->v_origin))
                              AddPath(p->Entry, pNode); // Add a path from b to a

                         p = p->next;
                    }
               }
          }
     }

     return pNode;
}

void CWaypointClass::DeleteWaypoint(vec v_src)
{
     node_s *pWP;

     pWP = GetNearestWaypoint(v_src, 7.0f);

     if (!pWP)
     {
          conoutf("Error: Couldn't find near waypoint");
          return;
     }

     BotManager.DelWaypoint(pWP);

     // delete any paths that lead to this index...
     DeletePath(pWP);

     short x, y;
     GetNodeIndexes(pWP->v_origin, &x, &y);

     m_Waypoints[x][y].DeleteEntry(pWP);

     pWP->ConnectedWPs.DeleteAllNodes();
     pWP->ConnectedWPsWithMe.DeleteAllNodes();

     m_iWaypointCount--;
     delete pWP;
}

void CWaypointClass::AddPath(node_s *pWP1, node_s *pWP2)
{
     pWP1->ConnectedWPs.AddNode(pWP2);
     pWP2->ConnectedWPsWithMe.AddNode(pWP1);
}

// Deletes all paths connected to the given waypoint
void CWaypointClass::DeletePath(node_s *pWP)
{
     TLinkedList<node_s *>::node_s *p;

     loopi(MAX_MAP_GRIDS)
     {
          loopj(MAX_MAP_GRIDS)
          {
               p = m_Waypoints[i][j].GetFirst();
               while (p)
               {
                    p->Entry->ConnectedWPs.DeleteEntry(pWP);
                    pWP->ConnectedWPsWithMe.DeleteEntry(p->Entry);
                    p = p->next;
               }
          }
     }
}

// Deletes path between 2 waypoints(1 way)
void CWaypointClass::DeletePath(node_s *pWP1, node_s *pWP2)
{
     pWP1->ConnectedWPs.DeleteEntry(pWP2);
     pWP2->ConnectedWPsWithMe.DeleteEntry(pWP1);
}

void CWaypointClass::ManuallyCreatePath(vec v_src, int iCmd, bool TwoWay)
{
     static node_s *waypoint1 = NULL;  // initialized to unassigned
     static node_s *waypoint2 = NULL;  // initialized to unassigned

     if (iCmd == 1)  // assign source of path
     {
          waypoint1 = GetNearestWaypoint(v_src, 7.0f);

          if (!waypoint1)
          {
               conoutf("Error: Couldn't find near waypoint");
               return;
          }

          return;
     }

     if (iCmd == 2)  // assign dest of path and make path
     {
          if (!waypoint1)
          {
               conoutf("Error: First waypoint unset");
               return;
          }

          waypoint2 = GetNearestWaypoint(v_src, 7.0f);

          if (!waypoint2)
          {
               conoutf("Error: Couldn't find near waypoint");
               return;
          }

          AddPath(waypoint1, waypoint2);

          if (TwoWay)
               AddPath(waypoint2, waypoint1);
     }
}

void CWaypointClass::ManuallyDeletePath(vec v_src, int iCmd, bool TwoWay)
{
     static node_s *waypoint1 = NULL;  // initialized to unassigned
     static node_s *waypoint2 = NULL;  // initialized to unassigned

     if (iCmd == 1)  // assign source of path
     {
          waypoint1 = GetNearestWaypoint(v_src, 7.0f);

          if (!waypoint1)
          {
               conoutf("Error: Couldn't find near waypoint");
               return;
          }

          return;
     }

     if (iCmd == 2)  // assign dest of path and delete path
     {
          if (!waypoint1)
          {
               conoutf("Error: First waypoint unset");
               return;
          }

          waypoint2 = GetNearestWaypoint(v_src, 7.0f);

          if (!waypoint2)
          {
               conoutf("Error: Couldn't find near waypoint");
               return;
          }

          DeletePath(waypoint1, waypoint2);

          if (TwoWay)
               DeletePath(waypoint2, waypoint1);
     }
}

bool CWaypointClass::WPIsReachable(vec from, vec to)
{
     traceresult_s tr;
     float curr_height, last_height;

     float distance = GetDistance(from, to);

     // is the destination close enough?
     if (distance < REACHABLE_RANGE)
     {
          if (IsVisible(from, to))
          {
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
                         conoutf("to is in midair");
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
                    if ((last_height - curr_height) >= JUMP_HEIGHT)
                    {
                         // can't get there from here...
                         //conoutf("traces failed to to");
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

node_s *CWaypointClass::GetNearestWaypoint(vec v_src, float flRange)
{
     TLinkedList<node_s *>::node_s *pNode;
     node_s *pNearest = NULL;
     short i, j, MinI, MaxI, MinJ, MaxJ, Offset = (short)ceil(flRange / MAX_MAP_GRIDS);
     float flNearestDist = 9999.99f, flDist;

     GetNodeIndexes(v_src, &i, &j);
     MinI = i - Offset;
     MaxI = i + Offset;
     MinJ = j - Offset;
     MaxJ = j + Offset;

     if (MinI < 0)
          MinI = 0;
     if (MaxI > MAX_MAP_GRIDS - 1)
          MaxI = MAX_MAP_GRIDS - 1;
     if (MinJ < 0)
          MinJ = 0;
     if (MaxJ > MAX_MAP_GRIDS - 1)
          MaxJ = MAX_MAP_GRIDS - 1;

     for (int x=MinI;x<=MaxI;x++)
     {
          for(int y=MinJ;y<=MaxJ;y++)
          {
               pNode = m_Waypoints[x][y].GetFirst();

               while(pNode)
               {
                    flDist = GetDistance(v_src, pNode->Entry->v_origin);
                    if ((flDist < flNearestDist) && (flDist <= flRange))
                    {
                         if (IsVisible(v_src, pNode->Entry->v_origin, NULL))
                         {
                              pNearest = pNode->Entry;
                              flNearestDist = flDist;
                         }
                    }

                    pNode = pNode->next;
               }
          }
     }
     return pNearest;
}

node_s *CWaypointClass::GetNearestTriggerWaypoint(vec v_src, float flRange)
{
     TLinkedList<node_s *>::node_s *pNode;
     node_s *pNearest = NULL;
     short i, j, MinI, MaxI, MinJ, MaxJ, Offset = (short)ceil(flRange / MAX_MAP_GRIDS);
     float flNearestDist = 9999.99f, flDist;

     GetNodeIndexes(v_src, &i, &j);
     MinI = i - Offset;
     MaxI = i + Offset;
     MinJ = j - Offset;
     MaxJ = j + Offset;

     if (MinI < 0)
          MinI = 0;
     if (MaxI > MAX_MAP_GRIDS - 1)
          MaxI = MAX_MAP_GRIDS - 1;
     if (MinJ < 0)
          MinJ = 0;
     if (MaxJ > MAX_MAP_GRIDS - 1)
          MaxJ = MAX_MAP_GRIDS - 1;

     for (int x=MinI;x<=MaxI;x++)
     {
          for(int y=MinJ;y<=MaxJ;y++)
          {
               pNode = m_Waypoints[x][y].GetFirst();

               while(pNode)
               {
                    if ((pNode->Entry->iFlags & W_FL_FLOOD) || !(pNode->Entry->iFlags & W_FL_TRIGGER))
                    {
                         pNode = pNode->next;
                         continue;
                    }

                    flDist = GetDistance(v_src, pNode->Entry->v_origin);
                    if ((flDist < flNearestDist) && (flDist <= flRange))
                    {
                         if (IsVisible(v_src, pNode->Entry->v_origin, NULL))
                         {
                              pNearest = pNode->Entry;
                              flNearestDist = flDist;
                         }
                    }

                    pNode = pNode->next;
               }
          }
     }
     return pNearest;
}

node_s *CWaypointClass::GetWaypointFromVec(const vec &v_src)
{
     static TLinkedList<node_s *>::node_s *pNode;
     static short x, y;

     GetNodeIndexes(v_src, &x, &y);

     pNode = m_Waypoints[x][y].GetFirst();

     while(pNode)
     {
          if (pNode->Entry->v_origin==v_src)
               return pNode->Entry;

          pNode = pNode->next;
     }
     return NULL;
}

void CWaypointClass::CalcCost(node_s *pNode)
{
     float flCost = 10.0f;

     // Check nearby cubes...
     int x = int(pNode->v_origin.x);
     int y = int(pNode->v_origin.y);
     int a, b, row;

     for (row=0;row<=1;row++)
     {
          if (row==0) b = y - 6;
          else b = y + 6;

          for (a=(x-6);a<=(x+6);a++)
          {
               if (OUTBORD(a, b)) continue;

               vec to(a, b, GetCubeFloor(a, b) + 1.0f);

               // See if there is a obstacle(cube or mapmodel) nearby
               traceresult_s tr;
               TraceLine(pNode->v_origin, to, NULL, false, &tr);
               if (tr.collided)
               {
                    float flFraction = (GetDistance(pNode->v_origin, tr.end) /
                                        GetDistance(pNode->v_origin, to));
                    flCost += (1.0f-flFraction)*0.5f;
               }

               if ((iabs(a) > 4) || (iabs(b) > 4)) continue;

               vec from = to;
               to.z -= (JUMP_HEIGHT - 1.0f);
               TraceLine(from, to, NULL, false, &tr);
               if (!tr.collided)
                    flCost += 0.5f;
          }

          if (row==0) a = x - 6;
          else a = x + 6;

          for (b=(y-6);b<=(y+6);b++)
          {
               if (OUTBORD(a, b)) continue;

               vec to(a, b, GetCubeFloor(a, b) + 1.0f);

               // See if there is a obstacle(cube or mapmodel) nearby
               traceresult_s tr;
               TraceLine(pNode->v_origin, to, NULL, false, &tr);
               if (tr.collided)
               {
                    float flFraction = (GetDistance(pNode->v_origin, tr.end) /
                                        GetDistance(pNode->v_origin, to));
                    flCost += (1.0f-flFraction)*0.5f;
               }

               if ((iabs(a) > 4) || (iabs(b) > 4)) continue;

               vec from = to;
               to.z -= (JUMP_HEIGHT - 1.0f);
               TraceLine(from, to, NULL, false, &tr);
               if (!tr.collided)
                    flCost += 0.5f;
          }
     }

     if (UnderWater(pNode->v_origin)) // Water is annoying
          flCost += 5.0f;

     pNode->sCost = (short)flCost;
}

void CWaypointClass::ReCalcCosts(void)
{
     loopi(MAX_MAP_GRIDS)
     {
          loopj(MAX_MAP_GRIDS)
          {
               TLinkedList<node_s *>::node_s *p = m_Waypoints[i][j].GetFirst();
               while(p)
               {
                    CalcCost(p->Entry);
                    p = p->next;
               }
          }
     }
}

#ifdef WP_FLOOD

void CWaypointClass::StartFlood()
{
     if (m_bFlooding) return;

     conoutf("Starting flood, this may take a while on large maps....");
     m_bFlooding = true;
     m_iFloodStartTime = SDL_GetTicks();
     m_iCurFloodX = m_iCurFloodY = MINBORD;
     m_iFloodSize = 0;
}

void CWaypointClass::FloodThink()
{
     if (!m_bFlooding) return;

     static int x, y, count;
     count = 0;

     if (!m_bFilteringNodes)
     {
          // loop through ALL cubes and check if we should add a waypoint here
          for (x=m_iCurFloodX; x<(ssize-MINBORD); x+=4)
          {
               if (count >= 3)
               {
                    AddScreenText("Flooding map with waypoints... %.2f %%",
                                  ((float)x / float(ssize-MINBORD)) * 100.0f);
                    m_iCurFloodX = x;
                    return;
               }

               count++;

               for (y=MINBORD; y<(ssize-MINBORD); y+=4)
               {
                    vec from(x, y, GetCubeFloor(x, y)+2.0f);
                    bool bFound = CanPlaceNodeHere(from);

                    if (!bFound)
                    {
                         for (int a=x-2;a<=(x+2);a++)
                         {
                              for (int b=y-2;b<=(y+2);b++)
                              {
                                   if (OUTBORD(a, b)) continue;
                                   if ((a==x) && (b==y)) continue;
                                   makevec(&from, a, b, GetCubeFloor(a, b) + 2.0f);
                                   if (CanPlaceNodeHere(from))
                                   {
                                        bFound = true;
                                        break;
                                   }
                              }

                              if (bFound) break;
                         }
                    }

                    if (!bFound) continue;

                    // Add WP
                    int flags = W_FL_FLOOD;
                    if (S((int)from.x, (int)from.y)->tag) flags |= W_FL_INTAG;

                    node_s *pWP = new node_s(from, flags, 0);

                    short i, j;
                    GetNodeIndexes(from, &i, &j);
                    m_Waypoints[i][j].PushNode(pWP);
                    BotManager.AddWaypoint(pWP);
                    m_iFloodSize += sizeof(node_s);
                    m_iWaypointCount++;

                    // Connect with other nearby nodes
                    ConnectFloodWP(pWP);
               }
          }
     }

     if (!m_bFilteringNodes)
     {
          m_bFilteringNodes = true;
          m_iCurFloodX = m_iCurFloodY = 0;
          m_iFilteredNodes = 0;
     }

     count = 0;

     // Filter all nodes which aren't connected to any other nodes
     for (x=m_iCurFloodX;x<MAX_MAP_GRIDS;x++)
     {
          if (count > 3)
          {
               AddScreenText("Filtering useless waypoints and");
               AddScreenText("adding costs... %.2f %%", ((float)x / float(MAX_MAP_GRIDS)) * 100.0f);
               m_iCurFloodX = x;
               return;
          }

          count++;

          for (y=0;y<MAX_MAP_GRIDS;y++)
          {
               TLinkedList<node_s *>::node_s *pNode = m_Waypoints[x][y].GetFirst(), *pTemp;
               while(pNode)
               {
                    if (pNode->Entry->ConnectedWPs.Empty() ||
                         pNode->Entry->ConnectedWPsWithMe.Empty())
                    {
                         BotManager.DelWaypoint(pNode->Entry);
                         pTemp = pNode;
                         pNode = pNode->next;
                         delete pTemp->Entry;
                         m_Waypoints[x][y].DeleteNode(pTemp);
                         m_iFilteredNodes++;
                         m_iFloodSize -= sizeof(node_s);
                         m_iWaypointCount--;
                         continue;
                    }
                    else
                         CalcCost(pNode->Entry);
                    pNode = pNode->next;
               }
          }
     }

     // Done with flooding
     m_bFlooding = false;
     m_bFilteringNodes = false;

     //ReCalcCosts();
     BotManager.PickNextTrigger();

     m_iFloodSize += sizeof(m_Waypoints);
     conoutf("Added %d wps in %d milliseconds", m_iWaypointCount, SDL_GetTicks()-m_iFloodStartTime);
     conoutf("Filtered %d wps", m_iFilteredNodes);

     BotManager.CalculateMaxAStarCount();

     char szSize[64];
     sprintf(szSize, "Total size: %.2f Kb", float(m_iFloodSize)/1024.0f);
     conoutf(szSize);
}

bool CWaypointClass::CanPlaceNodeHere(const vec &from)
{
     static short x, y, a, b;
     static traceresult_s tr;
     static vec to, v1, v2;

     x = short(from.x);
     y = short(from.y);

     sqr *s = S(x, y);
     if (SOLID(s))
     {
          return false;
     }

     if (fabs((float)(s->ceil - s->floor)) < player1->radius)
     {
          return false;
     }

     if (GetNearestFloodWP(from, 2.0f, NULL)) return false;

     for (a=(x-1);a<=(x+1);a++)
     {
          for (b=(y-1);b<=(y+1);b++)
          {
               if ((x==a) && (y==b)) continue;
               if (OUTBORD(a, b)) return false;
               makevec(&v1, a, b, from.z);
               v2 = v1;
               v2.z -= 1000.0f;

               TraceLine(v1, v2, NULL, false, &tr, true);
               to = tr.end;

               if ((a >= (x-1)) && (a <= (x+1)) && (b >= (y-1)) && (b <= (y+1)))
               {
                    if (!tr.collided || (to.z < (from.z-4.0f)))
                    {
                         return false;
                    }
               }

               to.z += 2.0f;

               if (from.z < (to.z-JUMP_HEIGHT))
               {
                    return false;
               }

               TraceLine(from, to, NULL, false, &tr, true);
               if (tr.collided)
                    return false;
          }
     }

     return true;
}

void CWaypointClass::ConnectFloodWP(node_s *pWP)
{
     if (!pWP) return;

     static float flRange;
     static TLinkedList<node_s *>::node_s *pNode;
     static short i, j, MinI, MaxI, MinJ, MaxJ, x, y, Offset;
     static float flDist;
     static node_s *p;

     // Calculate range, based on distance to nearest node
     p = GetNearestFloodWP(pWP->v_origin, 15.0f, pWP, true);
     if (p)
     {
          flDist = GetDistance(pWP->v_origin, p->v_origin);
          flRange = min(flDist+2.0f, 15.0f);
          if (flRange < 5.0f) flRange = 5.0f;
     }
     else
          return;

     Offset = (short)ceil(flRange / MAX_MAP_GRIDS);

     GetNodeIndexes(pWP->v_origin, &i, &j);
     MinI = i - Offset;
     MaxI = i + Offset;
     MinJ = j - Offset;
     MaxJ = j + Offset;

     if (MinI < 0)
          MinI = 0;
     if (MaxI > MAX_MAP_GRIDS - 1)
          MaxI = MAX_MAP_GRIDS - 1;
     if (MinJ < 0)
          MinJ = 0;
     if (MaxJ > MAX_MAP_GRIDS - 1)
          MaxJ = MAX_MAP_GRIDS - 1;

     for (x=MinI;x<=MaxI;x++)
     {
          for(y=MinJ;y<=MaxJ;y++)
          {
               pNode = m_Waypoints[x][y].GetFirst();

               while(pNode)
               {
                    if (pNode->Entry == pWP)
                    {
                         pNode = pNode->next;
                         continue;
                    }

                    flDist = GetDistance(pWP->v_origin, pNode->Entry->v_origin);
                    if (flDist <= flRange)
                    {
                         if (IsVisible(pWP->v_origin, pNode->Entry->v_origin, NULL, true))
                         {
                              // Connect a with b
                              pWP->ConnectedWPs.AddNode(pNode->Entry);
                              pNode->Entry->ConnectedWPsWithMe.AddNode(pWP);

                              // Connect b with a
                              pNode->Entry->ConnectedWPs.AddNode(pWP);
                              pWP->ConnectedWPsWithMe.AddNode(pNode->Entry);

                              m_iFloodSize += (2 * sizeof(node_s *));
                         }
                    }

                    pNode = pNode->next;
               }
          }
     }
}

node_s *CWaypointClass::GetNearestFloodWP(vec v_origin, float flRange, node_s *pIgnore,
                                          bool SkipTags)
{
     TLinkedList<node_s *>::node_s *p;
     node_s *pNearest = NULL;
     short i, j, MinI, MaxI, MinJ, MaxJ, Offset = (short)ceil(flRange / MAX_MAP_GRIDS);
     float flNearestDist = 9999.99f, flDist;

     GetNodeIndexes(v_origin, &i, &j);
     MinI = i - Offset;
     MaxI = i + Offset;
     MinJ = j - Offset;
     MaxJ = j + Offset;

     if (MinI < 0)
          MinI = 0;
     if (MaxI > MAX_MAP_GRIDS - 1)
          MaxI = MAX_MAP_GRIDS - 1;
     if (MinJ < 0)
          MinJ = 0;
     if (MaxJ > MAX_MAP_GRIDS - 1)
          MaxJ = MAX_MAP_GRIDS - 1;

     for (int x=MinI;x<=MaxI;x++)
     {
          for(int y=MinJ;y<=MaxJ;y++)
          {
               p = m_Waypoints[x][y].GetFirst();

               while(p)
               {
                    if ((p->Entry == pIgnore) || (!(p->Entry->iFlags & W_FL_FLOOD)))
                    {
                         p = p->next;
                         continue;
                    }

                    flDist = GetDistance(v_origin, p->Entry->v_origin);
                    if ((flDist < flNearestDist) && (flDist <= flRange))
                    {
                         if (IsVisible(v_origin, p->Entry->v_origin, NULL, SkipTags))
                         {
                              pNearest = p->Entry;
                              flNearestDist = flDist;
                         }
                    }

                    p = p->next;
               }
          }
     }
     return pNearest;
}

node_s *CWaypointClass::GetNearestTriggerFloodWP(vec v_origin, float flRange)
{
     TLinkedList<node_s *>::node_s *p;
     node_s *pNearest = NULL;
     short i, j, MinI, MaxI, MinJ, MaxJ, Offset = (short)ceil(flRange / MAX_MAP_GRIDS);
     float flNearestDist = 9999.99f, flDist;

     GetNodeIndexes(v_origin, &i, &j);
     MinI = i - Offset;
     MaxI = i + Offset;
     MinJ = j - Offset;
     MaxJ = j + Offset;

     if (MinI < 0)
          MinI = 0;
     if (MaxI > MAX_MAP_GRIDS - 1)
          MaxI = MAX_MAP_GRIDS - 1;
     if (MinJ < 0)
          MinJ = 0;
     if (MaxJ > MAX_MAP_GRIDS - 1)
          MaxJ = MAX_MAP_GRIDS - 1;

     for (int x=MinI;x<=MaxI;x++)
     {
          for(int y=MinJ;y<=MaxJ;y++)
          {
               p = m_Waypoints[x][y].GetFirst();

               while(p)
               {
                    if (!(p->Entry->iFlags & (W_FL_FLOOD | W_FL_TRIGGER)))
                    {
                         p = p->next;
                         continue;
                    }

                    flDist = GetDistance(v_origin, p->Entry->v_origin);
                    if ((flDist < flNearestDist) && (flDist <= flRange))
                    {
                         if (IsVisible(v_origin, p->Entry->v_origin, NULL))
                         {
                              pNearest = p->Entry;
                              flNearestDist = flDist;
                         }
                    }

                    p = p->next;
               }
          }
     }
     return pNearest;
}

void CWaypointClass::GetNodeIndexes(const vec &v_origin, short *i, short *j)
{
     // Function code by cheesy and PMB
     //*i = iabs((int)((int)(v_origin.x + (2*ssize)) / SECTOR_SIZE));
     //*j = iabs((int)((int)(v_origin.y + (2*ssize)) / SECTOR_SIZE));
     //*i = (int)((v_origin.x) / ssize * MAX_MAP_GRIDS);
     //*j = (int)((v_origin.y) / ssize * MAX_MAP_GRIDS);
     *i = iabs((int)((v_origin.x) / MAX_MAP_GRIDS));
     *j = iabs((int)((v_origin.y) / MAX_MAP_GRIDS));

     if (*i > MAX_MAP_GRIDS - 1)
          *i = MAX_MAP_GRIDS - 1;
     if (*j > MAX_MAP_GRIDS - 1)
          *j = MAX_MAP_GRIDS - 1;
}

#endif // WP_FLOOD
// Waypoint class end

#if defined AC_CUBE

// AC waypoint class begin
void CACWaypointClass::StartFlood()
{
     // UNDONE?
     CWaypointClass::StartFlood();
}

// AC waypoint class end

#elif defined VANILLA_CUBE

// Cube waypoint class begin

void CCubeWaypointClass::StartFlood()
{
     CWaypointClass::StartFlood();

     // Add wps at triggers and teleporters and their destination
     loopv(ents)
     {
          entity &e = ents[i];

          if (OUTBORD(e.x, e.y)) continue;

          if (e.type == TELEPORT)
          {
               vec telepos = { e.x, e.y, S(e.x, e.y)->floor+player1->eyeheight }, teledestpos = g_vecZero;

               // Find the teleport destination
               int n = -1, tag = e.attr1, beenhere = -1;
               for(;;)
               {
                    n = findentity(TELEDEST, n+1);
                    if(n==beenhere || n<0) { conoutf("no teleport destination for tag %d", tag); break; };
                    if(beenhere<0) beenhere = n;
                    if(ents[n].attr2==tag)
                    {
                         teledestpos.x = ents[n].x;
                         teledestpos.y = ents[n].y;
                         teledestpos.z = S(ents[n].x, ents[n].y)->floor+player1->eyeheight;
                         break;
                    }
               }

               if (vis(teledestpos, g_vecZero)) continue;

               int flags = (W_FL_FLOOD | W_FL_TELEPORT);
               if (S((int)telepos.x, (int)telepos.y)->tag) flags |= W_FL_INTAG;

               // Add waypoint at teleporter and teleport destination
               node_s *pWP = new node_s(telepos, flags, 0);

               short i, j;
               GetNodeIndexes(telepos, &i, &j);
               m_Waypoints[i][j].PushNode(pWP);
               BotManager.AddWaypoint(pWP);
               m_iFloodSize += sizeof(node_s);
               m_iWaypointCount++;

               flags = (W_FL_FLOOD | W_FL_TELEPORTDEST);
               if (S((int)teledestpos.x, (int)teledestpos.y)->tag) flags |= W_FL_INTAG;

               node_s *pWP2 = new node_s(teledestpos, flags, 0);

               GetNodeIndexes(teledestpos, &i, &j);
               m_Waypoints[i][j].PushNode(pWP2);
               BotManager.AddWaypoint(pWP2);
               m_iFloodSize += sizeof(node_s);
               m_iWaypointCount++;

               // Connect the teleporter waypoint with the teleport-destination waypoint(1 way)
               AddPath(pWP, pWP2);

               // Connect with other nearby nodes
               ConnectFloodWP(pWP);
          }
          else if (e.type == CARROT)
          {
               vec pos = { e.x, e.y, S(e.x, e.y)->floor+player1->eyeheight };

               int flags = (W_FL_FLOOD | W_FL_TRIGGER);
               if (S(e.x, e.y)->tag) flags |= W_FL_INTAG;

               node_s *pWP = new node_s(pos, flags, 0);

               short i, j;
               GetNodeIndexes(pos, &i, &j);
               m_Waypoints[i][j].PushNode(pWP);
               BotManager.AddWaypoint(pWP);
               m_iFloodSize += sizeof(node_s);
               m_iWaypointCount++;

               // Connect with other nearby nodes
               ConnectFloodWP(pWP);
          }
          else if (e.type == MAPMODEL)
          {
               mapmodelinfo &mmi = getmminfo(e.attr2);
               if(!&mmi || !mmi.h || !mmi.rad) continue;

               float floor = (float)(S(e.x, e.y)->floor+mmi.zoff+e.attr3)+mmi.h;

               float x1 = e.x - mmi.rad;
               float x2 = e.x + mmi.rad;
               float y1 = e.y - mmi.rad;
               float y2 = e.y + mmi.rad;

               // UNDONE?
               for (float x=(x1+1.0f);x<=(x2-1.0f);x++)
               {
                    for (float y=(y1+1.0f);y<=(y2-1.0f);y++)
                    {
                         vec from = { x, y, floor+2.0f };
                         if (GetNearestFloodWP(from, 2.0f, NULL)) continue;

                         // Add WP
                         int flags = W_FL_FLOOD;
                         if (S((int)x, (int)y)->tag) flags |= W_FL_INTAG;

                         node_s *pWP = new node_s(from, flags, 0);

                         short i, j;
                         GetNodeIndexes(from, &i, &j);
                         m_Waypoints[i][j].PushNode(pWP);
                         BotManager.AddWaypoint(pWP);
                         m_iFloodSize += sizeof(node_s);
                         m_iWaypointCount++;

                         // Connect with other nearby nodes
                         ConnectFloodWP(pWP);
                    }
               }
          }
     }
     CWaypointClass::StartFlood();
}

void CCubeWaypointClass::CreateWPsAtTeleporters()
{
     loopv(ents)
     {
          entity &e = ents[i];

          if (e.type != TELEPORT) continue;
          if (OUTBORD(e.x, e.y)) continue;

          vec telepos = { e.x, e.y, S(e.x, e.y)->floor+player1->eyeheight }, teledestpos = g_vecZero;

          // Find the teleport destination
          int n = -1, tag = e.attr1, beenhere = -1;
          for(;;)
          {
               n = findentity(TELEDEST, n+1);
               if(n==beenhere || n<0) { conoutf("no teleport destination for tag %d", tag); continue; };
               if(beenhere<0) beenhere = n;
               if(ents[n].attr2==tag)
               {
                    teledestpos.x = ents[n].x;
                    teledestpos.y = ents[n].y;
                    teledestpos.z = S(ents[n].x, ents[n].y)->floor+player1->eyeheight;
                    break;
               }
          }

          if (vis(teledestpos, g_vecZero)) continue;

          // Add waypoint at teleporter and teleport destination
          node_s *telewp = AddWaypoint(telepos, false);
          node_s *teledestwp = AddWaypoint(teledestpos, false);

          if (telewp && teledestwp)
          {
               // Connect the teleporter waypoint with the teleport-destination waypoint(1 way)
               AddPath(telewp, teledestwp);

               // Flag waypoints
               telewp->iFlags = W_FL_TELEPORT;
               teledestwp->iFlags = W_FL_TELEPORTDEST;
          }
     }
}

void CCubeWaypointClass::CreateWPsAtTriggers()
{
     loopv(ents)
     {
          entity &e = ents[i];

          if (e.type != CARROT) continue;
          if (OUTBORD(e.x, e.y)) continue;

          vec pos = { e.x, e.y, S(e.x, e.y)->floor+player1->eyeheight };

          node_s *wp = AddWaypoint(pos, false);

          if (wp)
          {
               // Flag waypoints
               wp->iFlags = W_FL_TRIGGER;
          }
     }
}

#endif

// Cube waypoint class end

// Waypoint commands begin

void addwp(int *autoconnect)
{
     WaypointClass.SetWaypointsVisible(true);
     WaypointClass.AddWaypoint(curselection, *autoconnect!=0);
}

COMMAND(addwp, "i");

void delwp(void)
{
     WaypointClass.SetWaypointsVisible(true);
     WaypointClass.DeleteWaypoint(curselection);
}

COMMAND(delwp, "");

void wpvisible(int *on)
{
     WaypointClass.SetWaypointsVisible(*on!=0);
}

COMMAND(wpvisible, "i");

void wpsave(void)
{
     WaypointClass.SaveWaypoints();
}

COMMAND(wpsave, "");

void wpload(void)
{
     WaypointClass.LoadWaypoints();
}

COMMAND(wpload, "");

void wpclear(void)
{
    WaypointClass.Clear();
}

COMMAND(wpclear, "");

void autowp(int *on)
{
     WaypointClass.SetWaypointsVisible(true);
     WaypointClass.SetAutoWaypoint(*on!=0);
}

COMMAND(autowp, "i");

void wpinfo(int *on)
{
     WaypointClass.SetWaypointsVisible(true);
     WaypointClass.SetWPInfo(*on!=0);
}

COMMAND(wpinfo, "i");

void addpath1way1(void)
{
     WaypointClass.SetWaypointsVisible(true);
     WaypointClass.ManuallyCreatePath(curselection, 1, false);
}

COMMAND(addpath1way1, "");

void addpath1way2(void)
{
     WaypointClass.SetWaypointsVisible(true);
     WaypointClass.ManuallyCreatePath(curselection, 2, false);
}

COMMAND(addpath1way2, "");

void addpath2way1(void)
{
     WaypointClass.SetWaypointsVisible(true);
     WaypointClass.ManuallyCreatePath(curselection, 1, true);
}

COMMAND(addpath2way1, "");

void addpath2way2(void)
{
     WaypointClass.SetWaypointsVisible(true);
     WaypointClass.ManuallyCreatePath(curselection, 2, true);
}

COMMAND(addpath2way2, "");

void delpath1way1(void)
{
     WaypointClass.SetWaypointsVisible(true);
     WaypointClass.ManuallyDeletePath(curselection, 1, false);
}

COMMAND(delpath1way1, "");

void delpath1way2(void)
{
     WaypointClass.SetWaypointsVisible(true);
     WaypointClass.ManuallyDeletePath(curselection, 2, false);
}

COMMAND(delpath1way2, "");

void delpath2way1(void)
{
     WaypointClass.SetWaypointsVisible(true);
     WaypointClass.ManuallyDeletePath(curselection, 1, true);
}

COMMAND(delpath2way1, "");

void delpath2way2(void)
{
     WaypointClass.SetWaypointsVisible(true);
     WaypointClass.ManuallyDeletePath(curselection, 2, true);
}

COMMAND(delpath2way2, "");

void setjumpwp(void)
{
     node_s *wp = WaypointClass.GetNearestWaypoint(curselection, 20.0f);
     if (wp)
     {
          WaypointClass.SetWPFlags(wp, W_FL_JUMP);
     }
}

COMMAND(setjumpwp, "");

void unsetjumpwp(void)
{
     node_s *wp = WaypointClass.GetNearestWaypoint(curselection, 20.0f);
     if (wp)
     {
          WaypointClass.UnsetWPFlags(wp, W_FL_JUMP);
     }
}

COMMAND(unsetjumpwp, "");

void setwptriggernr(int *nr)
{
     node_s *wp = WaypointClass.GetNearestWaypoint(curselection, 20.0f);
     if (wp)
     {
          WaypointClass.SetWPTriggerNr(wp, *nr);
     }
}

COMMAND(setwptriggernr, "i");

void setwpyaw(void)
{
     node_s *wp = WaypointClass.GetNearestWaypoint(curselection, 20.0f);
     if (wp)
     {
          WaypointClass.SetWPYaw(wp, short(player1->yaw));
     }
}

COMMAND(setwpyaw, "");

#ifdef WP_FLOOD
void wpflood(void)
{
     WaypointClass.StartFlood();
}

COMMAND(wpflood, "");
#endif

#ifdef VANILLA_CUBE
// Commands specific for cube
void addtelewps(void)
{
     WaypointClass.SetWaypointsVisible(true);
     WaypointClass.CreateWPsAtTeleporters();
}

COMMAND(addtelewps, "");

void addtriggerwps(void)
{
     WaypointClass.SetWaypointsVisible(true);
     WaypointClass.CreateWPsAtTriggers();
}

COMMAND(addtriggerwps, "");
#endif

// Debug functions
#ifdef WP_FLOOD

#ifndef RELEASE_BUILD
void botsheadtome(void)
{
     loopv(bots)
     {
          if (!bots[i] || !bots[i]->pBot) continue;
          bots[i]->pBot->HeadToGoal();
          bots[i]->pBot->GoToDebugGoal(player1->o);
     }
}

COMMAND(botsheadtome, "");

void setdebuggoal(void) { v_debuggoal = player1->o; };
COMMAND(setdebuggoal, "");

void botsheadtodebuggoal(void)
{
     loopv(bots)
     {
          if (!bots[i] || !bots[i]->pBot) continue;

          bots[i]->pBot->GoToDebugGoal(v_debuggoal);
     }
}

COMMAND(botsheadtodebuggoal, "");

#endif // RELEASE_BUILD

#endif // WP_FLOOD

// End debug functions
// Waypoint commands end


// Bot class begin

bool CBot::FindWaypoint()
{
     waypoint_s *wp, *wpselect;
     int index;
     float distance, min_distance[3];
     waypoint_s *min_wp[3];

     for (index=0; index < 3; index++)
     {
          min_distance[index] = 9999.0;
          min_wp[index] = NULL;
     }

     TLinkedList<node_s *>::node_s *pNode = m_pCurrentWaypoint->pNode->ConnectedWPs.GetFirst();

     while (pNode)
     {
          if ((pNode->Entry->iFlags & W_FL_INTAG) &&
              SOLID(S((int)pNode->Entry->v_origin.x, (int)pNode->Entry->v_origin.y)))
          {
               pNode = pNode->next;
               continue;
          }

          wp = GetWPFromNode(pNode->Entry);
          if (!wp)
          {
               pNode = pNode->next;
               continue;
          }

          // if index is not a current or recent previous waypoint...
          if ((wp != m_pCurrentWaypoint) &&
              (wp != m_pPrevWaypoints[0]) &&
              (wp != m_pPrevWaypoints[1]) &&
              (wp != m_pPrevWaypoints[2]) &&
              (wp != m_pPrevWaypoints[3]) &&
              (wp != m_pPrevWaypoints[4]))
          {
               // find the distance from the bot to this waypoint
               distance = GetDistance(wp->pNode->v_origin);

               if (distance < min_distance[0])
               {
                    min_distance[2] = min_distance[1];
                    min_wp[2] = min_wp[1];

                    min_distance[1] = min_distance[0];
                    min_wp[1] = min_wp[0];

                    min_distance[0] = distance;
                    min_wp[0] = wp;
               }
               else if (distance < min_distance [1])
               {
                    min_distance[2] = min_distance[1];
                    min_wp[2] = min_wp[1];

                    min_distance[1] = distance;
                    min_wp[1] = wp;
               }
               else if (distance < min_distance[2])
               {
                    min_distance[2] = distance;
                    min_wp[2] = wp;
               }
          }
          pNode = pNode->next;
     }

     wpselect = NULL;

     // about 20% of the time choose a waypoint at random
     // (don't do this any more often than every 10 seconds)

     if ((RandomLong(1, 100) <= 20) && (m_iRandomWaypointTime <= lastmillis))
     {
          m_iRandomWaypointTime = lastmillis + 10000;

          if (min_wp[2])
               index = RandomLong(0, 2);
          else if (min_wp[1])
               index = RandomLong(0, 1);
          else if (min_wp[0])
               index = 0;
          else
               return false;  // no waypoints found!

          wpselect = min_wp[index];
     }
     else
     {
          // use the closest waypoint that has been recently used
          wpselect = min_wp[0];
     }

     if (wpselect)  // was a waypoint found?
     {
          m_pPrevWaypoints[4] = m_pPrevWaypoints[3];
          m_pPrevWaypoints[3] = m_pPrevWaypoints[2];
          m_pPrevWaypoints[2] = m_pPrevWaypoints[1];
          m_pPrevWaypoints[1] = m_pPrevWaypoints[0];
          m_pPrevWaypoints[0] = m_pCurrentWaypoint;

          SetCurrentWaypoint(wpselect);
          return true;
     }

     return false;  // couldn't find a waypoint
}

bool CBot::HeadToWaypoint()
{
     if (!m_pCurrentWaypoint)
          return false; // Can't head to waypoint

     bool Touching = false;
     float WPDist = GetDistance(m_pCurrentWaypoint->pNode->v_origin);

#ifndef RELEASE_BUILD
     if (m_pCurrentGoalWaypoint && m_vGoal==g_vecZero)
          condebug("Warning: m_vGoal unset");
#endif

     // did the bot run past the waypoint? (prevent the loop-the-loop problem)
     if ((m_fPrevWaypointDistance > 1.0) && (WPDist > m_fPrevWaypointDistance) &&
         (WPDist <= 5.0f))
          Touching = true;
     // bot needs to be close for jump and trigger waypoints
     else if ((m_pCurrentWaypoint->pNode->iFlags & W_FL_JUMP) ||
                (m_pCurrentWaypoint->pNode->iFlags & W_FL_TRIGGER))
          Touching = (WPDist <= 1.5f);
     else if (m_pCurrentWaypoint->pNode->iFlags & W_FL_TELEPORT)
          Touching = (WPDist <= 4.0f);
     // are we close enough to a target waypoint...
     else if (WPDist <= 3.0f)
     {
          if (!m_pCurrentGoalWaypoint || (m_pCurrentWaypoint != m_pCurrentGoalWaypoint) ||
               IsVisible(m_vGoal) || (WPDist <= 1.5f))
               Touching = true;
          // If the bot has a goal check if he can see his next wp
          if (m_pCurrentGoalWaypoint && (m_pCurrentGoalWaypoint != m_pCurrentWaypoint) &&
             !m_AStarNodeList.Empty() && (WPDist >= 1.0f) &&
             !IsVisible(m_AStarNodeList.GetFirst()->Entry->pNode->v_origin))
               Touching = false;
     }

     // save current distance as previous
     m_fPrevWaypointDistance = WPDist;

     // Reached the waypoint?
     if (Touching)
     {
          // Does this waypoint has a targetyaw?
          if (m_pCurrentWaypoint->pNode->sYaw != -1)
          {
               // UNDONE: Inhuman
               m_pMyEnt->yaw = m_pMyEnt->targetyaw = m_pCurrentWaypoint->pNode->sYaw;
          }

          // Reached a jump waypoint?
          if (m_pCurrentWaypoint->pNode->iFlags & W_FL_JUMP)
               m_pMyEnt->jumpnext = true;

          m_fPrevWaypointDistance = 0.0f;

          // Does the bot has a goal?
          if (m_pCurrentGoalWaypoint)
          {
               if (m_pCurrentWaypoint != m_pCurrentGoalWaypoint)
               {
                    if (m_AStarNodeList.Empty())
                    {
                         if (!AStar())
                         {
                              // Bot is calculating a new path, just stand still for now
                              ResetMoveSpeed();
                              m_iWaypointTime += 200;
                              return true;
                         }
                         else
                         {
                              if (m_AStarNodeList.Empty())
                              {
                                   //m_UnreachableNodes.PushNode(unreachable_node_s(m_pCurrentGoalWaypoint, gpGlobals->time));
                                   return false; // Couldn't get a new wp to go to
                              }
                         }
                    }

                    m_pCurrentWaypoint = m_AStarNodeList.Pop();

                    if (!IsVisible(m_pCurrentWaypoint->pNode->v_origin))
                        //(!(m_pCurrentWaypoint->iFlags & W_FL_TELEPORT)))
                    {
                         //m_UnreachableNodes.PushNode(unreachable_node_s(m_pCurrentGoalWaypoint, gpGlobals->time));
                         condebug("Next WP not visible");
                         return false;
                    }

                    SetCurrentWaypoint(m_pCurrentWaypoint);
               }
               else
               {
                    // Bot reached the goal waypoint but couldn't reach the goal itself
                    // (otherwise we wouldn't be in this function)
                    //m_UnreachableNodes.PushNode(unreachable_node_s(m_pCurrentGoalWaypoint, gpGlobals->time));
                    return false;
               }
          }
          else
          {
               short index = 4;
               bool status;

               // try to find the next waypoint
               while (((status = FindWaypoint()) == false) && (index > 0))
               {
                    // if waypoint not found, clear oldest previous index and try again

                    m_pPrevWaypoints[index] = NULL;
                    index--;
               }

               if (status == false)
               {
                    ResetWaypointVars();
                    condebug("Couldn't find new random waypoint");
                    return false;
               }
          }
          m_iWaypointHeadPauseTime = lastmillis + 75;
          m_iWaypointTime += 75;
     }

     // keep turning towards the waypoint...
     /*if (m_pCurrentWaypoint->pNode->iFlags & W_FL_FLOOD)
     {UNDONE?
          vec aim = m_pCurrentWaypoint->pNode->v_origin;
          aim.x+=0.5f;
          aim.y+=0.5f;
          AimToVec(aim);
     }
     else*/
          AimToVec(m_pCurrentWaypoint->pNode->v_origin);

     if (m_fYawToTurn <= 25.0f)
          m_iWaypointHeadLastTurnLessTime = lastmillis;

     // Bot had to turn much for a while?
     if ((m_iWaypointHeadLastTurnLessTime > 0) &&
         (m_iWaypointHeadLastTurnLessTime < (lastmillis - 1000)))
     {
          m_iWaypointHeadPauseTime = lastmillis + 200;
          m_iWaypointTime += 200;
     }

     if (m_iWaypointHeadPauseTime >= lastmillis)
     {
          m_pMyEnt->move = 0;
          //conoutf("Pause in HeadToWaypoint()");
     }
     else
     {
          // Check if bot has to jump
          vec from = m_pMyEnt->o;
          from.z -= (m_pMyEnt->eyeheight - 1.25f);
          float flEndDist;
          if (!IsVisible(from, FORWARD, 3.0f, false, &flEndDist) &&
              (GetDistance(from, m_pCurrentWaypoint->pNode->v_origin) > flEndDist))
          {
               m_pMyEnt->jumpnext = true;
               condebug("Low wall in HeadToWaypoint()");
          }

          // Check if bot has to strafe
          if (m_iStrafeTime > lastmillis)
               SetMoveDir(m_iMoveDir, true);
          else
          {
               m_iStrafeTime = 0;
               m_iMoveDir = DIR_NONE;

               vec forward, up, right;
               AnglesToVectors(GetViewAngles(), forward, right, up);

               float flLeftDist = -1.0f, flRightDist = -1.0f;
               vec dir = right;
               bool bStrafe = false;
               int iStrafeDir = 0;

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
                    m_iStrafeTime = lastmillis + RandomLong(30, 50);
               }
          }
     }

     return true;
}

// returns true when done or failure
bool CBot::HeadToGoal()
{
     // Does the bot has a goal(waypoint)?
     if (m_pCurrentGoalWaypoint)
     {
          if (ReachedGoalWP())
          {
               return false;
          }
          else
          {
               if (CurrentWPIsValid())
               {
                    if (m_bCalculatingAStarPath)
                    {
                         // Bot is calculating a new path, just stand still for now
                         ResetMoveSpeed();
                         m_iWaypointTime += 200;

                         // Done calculating the path?
                         if (AStar())
                         {
                              if (m_AStarNodeList.Empty())
                              {
                                   return false; // Couldn't find a path
                              }
                              //else
                               //    SetCurrentWaypoint(m_AStarNodeList.Pop());
                         }
                         else
                              return true; // else just wait a little bit longer
                    }
                    //ResetMoveSpeed();
                    //return true;
               }
               else
               {
                    // Current waypoint isn't reachable, search new one
                    waypoint_s *pWP = NULL;

#ifdef WP_FLOOD
                    if (m_pCurrentGoalWaypoint->pNode->iFlags & W_FL_FLOOD)
                         pWP = GetNearestFloodWP(8.0f);
                    else
#endif
                         pWP = GetNearestWaypoint(15.0f);

                    if (!pWP || (pWP == m_pCurrentWaypoint))
                         return false;

                    SetCurrentWaypoint(pWP);
                    if (AStar())
                    {
                         if (m_AStarNodeList.Empty()) return false;
                    }
                    else
                    {
                         m_iWaypointTime += 200;
                         ResetMoveSpeed();
                         return true;
                    }
               }

               //debugbeam(m_pMyEnt->o, m_pCurrentWaypoint->pNode->v_origin);
          }
     }
     else
          return false;

     return HeadToWaypoint();
}

// return true when done calculating
bool CBot::AStar()
{
     if (!m_pCurrentGoalWaypoint || !m_pCurrentWaypoint)
     {
          if (m_bCalculatingAStarPath)
          {
               m_bCalculatingAStarPath = false;
               BotManager.m_sUsingAStarBotsCount--;
          }
          return true;
     }

     // Ideas by PMB :
     // * Make locals static to speed up a bit
     // * MaxCycles per frame and make it fps dependent

     static int iMaxCycles;
     static int iCurrentCycles;
     static short newg;
     static waypoint_s *n, *n2;
     static TLinkedList<node_s *>::node_s *pPath = NULL;
     static bool bPathFailed;

     iMaxCycles = BotManager.m_iFrameTime / 10;
     if (iMaxCycles < 10) iMaxCycles = 10;
     //condebug("MaxCycles: %d", iMaxCycles);
     iCurrentCycles = 0;
     bPathFailed = false;

     if (!m_bCalculatingAStarPath)
     {
          if ((BotManager.m_sUsingAStarBotsCount+1) > BotManager.m_sMaxAStarBots)
          {
               return true;
          }

          BotManager.m_sUsingAStarBotsCount++;

          CleanAStarLists(false);

          m_pCurrentWaypoint->g[0] = m_pCurrentWaypoint->g[1] = 0;
          m_pCurrentWaypoint->pParent[0] = m_pCurrentWaypoint->pParent[1] = NULL;
          m_pCurrentGoalWaypoint->g[0] = m_pCurrentGoalWaypoint->g[1] = 0;
          m_pCurrentGoalWaypoint->pParent[0] = m_pCurrentGoalWaypoint->pParent[1] = NULL;

          m_AStarNodeList.DeleteAllNodes();

          m_AStarOpenList[0].Clear();
          m_AStarOpenList[1].Clear();
          m_AStarClosedList[0].DeleteAllNodes();
          m_AStarClosedList[1].DeleteAllNodes();

          m_AStarOpenList[0].AddEntry(m_pCurrentWaypoint,
                                      GetDistance(m_pCurrentGoalWaypoint->pNode->v_origin));
          m_AStarOpenList[1].AddEntry(m_pCurrentGoalWaypoint,
                                      GetDistance(m_pCurrentGoalWaypoint->pNode->v_origin));

          m_pCurrentWaypoint->bIsOpen[0] = m_pCurrentGoalWaypoint->bIsOpen[1] = true;
          m_pCurrentWaypoint->bIsOpen[1] = m_pCurrentGoalWaypoint->bIsOpen[0] = false;
          m_pCurrentWaypoint->bIsClosed[0] = m_pCurrentGoalWaypoint->bIsClosed[1] = false;
          m_pCurrentWaypoint->bIsClosed[1] = m_pCurrentGoalWaypoint->bIsClosed[0] = false;
     }

     while(!m_AStarOpenList[0].Empty())
     {
          if (iCurrentCycles >= (iMaxCycles/2))
          {
               m_bCalculatingAStarPath = true;
               break;
          }

          n = m_AStarOpenList[0].Pop();
          n->bIsOpen[0] = false;

          if (n->pNode->FailedGoalList.IsInList(m_pCurrentGoalWaypoint->pNode))
          {
               bPathFailed = true;
               break; // Can't make path to goal
          }

          // Done with calculating
          if (n == m_pCurrentGoalWaypoint)
          {
               m_bCalculatingAStarPath = false;
               BotManager.m_sUsingAStarBotsCount--;

               while(n)
               {
                    m_AStarNodeList.PushNode(n);
                    n = n->pParent[0];
               }

               CleanAStarLists(false);

               return true;
          }

          pPath = n->pNode->ConnectedWPs.GetFirst();
          while(pPath)
          {
               if ((pPath->Entry->iFlags & W_FL_INTAG) &&
                    SOLID(S((int)pPath->Entry->v_origin.x, (int)pPath->Entry->v_origin.y)))
               {
                    pPath = pPath->next;
                    continue;
               }

               n2 = GetWPFromNode(pPath->Entry);

               if (!n2)
               {
                    pPath = pPath->next;
                    continue;
               }

               if (n2->bIsClosed[1])
               {
                    m_bCalculatingAStarPath = false;
                    BotManager.m_sUsingAStarBotsCount--;

                    while(n2)
                    {
                         m_AStarNodeList.AddNode(n2);
                         n2 = n2->pParent[1];
                    }

                    while(n)
                    {
                         m_AStarNodeList.PushNode(n);
                         n = n->pParent[0];
                    }

                    CleanAStarLists(false);

                    return true;
               }

               newg = n->g[0] + (short)AStarCost(n, n2);

               if ((n2->g[0] <= newg) && (n2->bIsOpen[0] || n2->bIsClosed[0]))
               {
                    pPath = pPath->next;
                    continue;
               }

               n2->pParent[0] = n;
               n2->g[0] = newg;
               n2->bIsClosed[0] = false;

               if (!n2->bIsOpen[0])
               {
                    m_AStarOpenList[0].AddEntry(n2, n2->g[1] + GetDistance(n2->pNode->v_origin,
                                                 m_pCurrentGoalWaypoint->pNode->v_origin));
                    n2->bIsOpen[0] = true;
               }
               pPath = pPath->next;
          }

          m_AStarClosedList[0].PushNode(n);
          n->bIsClosed[0] = true;
          iCurrentCycles++;
     }

     if (!bPathFailed)
     {
          while(!m_AStarOpenList[1].Empty())
          {
               if (iCurrentCycles >= iMaxCycles)
               {
                    m_bCalculatingAStarPath = true;
                    return false;
               }

               n = m_AStarOpenList[1].Pop();
               n->bIsOpen[1] = false;

               if (n->pNode->FailedGoalList.IsInList(m_pCurrentWaypoint->pNode))
               {
                    bPathFailed = true;
                    break; // Can't make path to goal
               }

               // Done with calculating
               if (n == m_pCurrentWaypoint)
               {
                    m_bCalculatingAStarPath = false;
                    BotManager.m_sUsingAStarBotsCount--;

                    while(n)
                    {
                         m_AStarNodeList.AddNode(n);
                         n = n->pParent[1];
                    }

                    CleanAStarLists(false);

                    return true;
               }

               pPath = n->pNode->ConnectedWPsWithMe.GetFirst();
               while(pPath)
               {
                    if ((pPath->Entry->iFlags & W_FL_INTAG) &&
                        SOLID(S((int)pPath->Entry->v_origin.x, (int)pPath->Entry->v_origin.y)))
                    {
                         pPath = pPath->next;
                         continue;
                    }

                    n2 = GetWPFromNode(pPath->Entry);

                    if (!n2)
                    {
                         pPath = pPath->next;
                         continue;
                    }

                    if (n2->bIsClosed[0])
                    {
                         m_bCalculatingAStarPath = false;
                         BotManager.m_sUsingAStarBotsCount--;

                         while(n2)
                         {
                              m_AStarNodeList.PushNode(n2);
                              n2 = n2->pParent[0];
                         }

                         while(n)
                         {
                              m_AStarNodeList.AddNode(n);
                              n = n->pParent[1];
                         }

                         CleanAStarLists(false);

                         return true;
                    }

                    newg = n->g[1] + (short)AStarCost(n, n2);

                    if ((n2->g[1] <= newg) && (n2->bIsOpen[1] || n2->bIsClosed[1]))
                    {
                         pPath = pPath->next;
                         continue;
                    }

                    n2->pParent[1] = n;
                    n2->g[1] = newg;
                    n2->bIsClosed[1] = false;

                    if (!n2->bIsOpen[1])
                    {
                         m_AStarOpenList[1].AddEntry(n2, n2->g[1] + GetDistance(n2->pNode->v_origin,
                                                      m_pCurrentWaypoint->pNode->v_origin));
                         n2->bIsOpen[1] = true;
                    }
                    pPath = pPath->next;
               }

               m_AStarClosedList[1].PushNode(n);
               n->bIsClosed[1] = true;
               iCurrentCycles++;
          }
     }

     // Failed making path
     condebug("Path failed");

     CleanAStarLists(true);
     m_bCalculatingAStarPath = false;
     BotManager.m_sUsingAStarBotsCount--;
     return true;
}

float CBot::AStarCost(waypoint_s *pWP1, waypoint_s *pWP2)
{
     // UNDONE?
     return (GetDistance(pWP1->pNode->v_origin, pWP2->pNode->v_origin) * pWP2->pNode->sCost);
}

void CBot::CleanAStarLists(bool bPathFailed)
{
     while(m_AStarOpenList[0].Empty() == false)
     {
          waypoint_s *p = m_AStarOpenList[0].Pop();
          p->bIsOpen[0] = p->bIsOpen[1] = false;
          p->bIsClosed[0] = p->bIsClosed[1] = false;
          p->pParent[0] = p->pParent[1] = NULL;
          p->g[0] = p->g[1] = 0;
     }

     while(m_AStarOpenList[1].Empty() == false)
     {
          waypoint_s *p = m_AStarOpenList[1].Pop();
          p->bIsOpen[0] = p->bIsOpen[1] = false;
          p->bIsClosed[0] = p->bIsClosed[1] = false;
          p->pParent[0] = p->pParent[1] = NULL;
          p->g[0] = p->g[1] = 0;
     }

     while(m_AStarClosedList[0].Empty() == false)
     {
          waypoint_s *p = m_AStarClosedList[0].Pop();
          p->bIsOpen[0] = p->bIsOpen[1] = false;
          p->bIsClosed[0] = p->bIsClosed[1] = false;
          p->pParent[0] = p->pParent[1] = NULL;
          p->g[0] = p->g[1] = 0;
          if (bPathFailed)
               p->pNode->FailedGoalList.AddNode(m_pCurrentGoalWaypoint->pNode);
     }

     while(m_AStarClosedList[1].Empty() == false)
     {
          waypoint_s *p = m_AStarClosedList[1].Pop();
          p->bIsOpen[0] = p->bIsOpen[1] = false;
          p->bIsClosed[0] = p->bIsClosed[1] = false;
          p->pParent[0] = p->pParent[1] = NULL;
          p->g[0] = p->g[1] = 0;
          if (bPathFailed)
               p->pNode->FailedGoalList.AddNode(m_pCurrentWaypoint->pNode);
     }
}

void CBot::ResetWaypointVars()
{
     m_iWaypointTime = 0;
     m_pCurrentWaypoint = NULL;
     m_pCurrentGoalWaypoint = NULL;
     m_pPrevWaypoints[0] = NULL;
     m_pPrevWaypoints[1] = NULL;
     m_pPrevWaypoints[2] = NULL;
     m_pPrevWaypoints[3] = NULL;
     m_pPrevWaypoints[4] = NULL;
     m_fPrevWaypointDistance = 0;
     m_iWaypointHeadLastTurnLessTime = 0;
     m_iWaypointHeadPauseTime = 0;
     if (m_bCalculatingAStarPath)
     {
          m_bCalculatingAStarPath = false;
          BotManager.m_sUsingAStarBotsCount--;
     }
     m_AStarNodeList.DeleteAllNodes();
     CleanAStarLists(false);
     m_bGoToDebugGoal = false;
}

void CBot::SetCurrentWaypoint(node_s *pNode)
{
     waypoint_s *pWP = GetWPFromNode(pNode);
#ifndef RELEASE_BUILD
     if (!pWP || !pNode) condebug("NULL WP In SetCurrentWP");
#endif

     m_pCurrentWaypoint = pWP;
     m_iWaypointTime = lastmillis;
}

void CBot::SetCurrentWaypoint(waypoint_s *pWP)
{
#ifndef RELEASE_BUILD
     if (!pWP) condebug("NULL WP In SetCurrentWP(2)");
#endif

     m_pCurrentWaypoint = pWP;
     m_iWaypointTime = lastmillis;
}

void CBot::SetCurrentGoalWaypoint(node_s *pNode)
{
     waypoint_s *pWP = GetWPFromNode(pNode);
#ifndef RELEASE_BUILD
     if (!pWP || !pNode) condebug("NULL WP In SetCurrentGoalWP");
#endif

     m_pCurrentGoalWaypoint = pWP;
}

void CBot::SetCurrentGoalWaypoint(waypoint_s *pWP)
{
#ifndef RELEASE_BUILD
     if (!pWP) condebug("NULL WP In SetCurrentGoalWP(2)");
#endif

     m_pCurrentGoalWaypoint = pWP;
}

bool CBot::CurrentWPIsValid()
{
     if (!m_pCurrentWaypoint)
     {
          //condebug("Invalid WP: Is NULL");
          return false;
     }

     // check if the bot has been trying to get to this waypoint for a while...
     if ((m_iWaypointTime + 5000) < lastmillis)
     {
          condebug("Invalid WP: time over");
          return false;
     }

#ifndef RELEASE_BUILD
     if (!IsVisible(m_pCurrentWaypoint->pNode->v_origin))
          condebug("Invalid WP: Not visible");
#endif

     return (IsVisible(m_pCurrentWaypoint->pNode->v_origin));
}

bool CBot::ReachedGoalWP()
{
     if ((!m_pCurrentWaypoint) || (!m_pCurrentGoalWaypoint))
          return false;

     if (m_pCurrentWaypoint != m_pCurrentGoalWaypoint)
          return false;

     return ((GetDistance(m_pCurrentGoalWaypoint->pNode->v_origin) <= 3.0f) &&
             (IsVisible(m_pCurrentGoalWaypoint->pNode->v_origin)));
}

waypoint_s *CBot::GetWPFromNode(node_s *pNode)
{
     if (!pNode) return NULL;

     short x, y;
     WaypointClass.GetNodeIndexes(pNode->v_origin, &x, &y);

     TLinkedList<waypoint_s *>::node_s *p = m_WaypointList[x][y].GetFirst();
     while(p)
     {
          if (p->Entry->pNode == pNode)
               return p->Entry;

          p = p->next;
     }

     return NULL;
}

waypoint_s *CBot::GetNearestWaypoint(vec v_src, float flRange)
{
     TLinkedList<waypoint_s *>::node_s *p;
     waypoint_s *pNearest = NULL;
     short i, j, MinI, MaxI, MinJ, MaxJ, Offset = (short)ceil(flRange / MAX_MAP_GRIDS);
     float flNearestDist = 9999.99f, flDist;

     WaypointClass.GetNodeIndexes(v_src, &i, &j);
     MinI = i - Offset;
     MaxI = i + Offset;
     MinJ = j - Offset;
     MaxJ = j + Offset;

     if (MinI < 0)
          MinI = 0;
     if (MaxI > MAX_MAP_GRIDS - 1)
          MaxI = MAX_MAP_GRIDS - 1;
     if (MinJ < 0)
          MinJ = 0;
     if (MaxJ > MAX_MAP_GRIDS - 1)
          MaxJ = MAX_MAP_GRIDS - 1;

     for (int x=MinI;x<=MaxI;x++)
     {
          for(int y=MinJ;y<=MaxJ;y++)
          {
               p = m_WaypointList[x][y].GetFirst();

               while(p)
               {
                    if (p->Entry->pNode->iFlags & W_FL_FLOOD)
                    {
                         p = p->next;
                         continue;
                    }

                    flDist = GetDistance(v_src, p->Entry->pNode->v_origin);
                    if ((flDist < flNearestDist) && (flDist <= flRange))
                    {
                         if (::IsVisible(v_src, p->Entry->pNode->v_origin, NULL))
                         {
                              pNearest = p->Entry;
                              flNearestDist = flDist;
                         }
                    }

                    p = p->next;
               }
          }
     }
     return pNearest;
}

waypoint_s *CBot::GetNearestTriggerWaypoint(vec v_src, float flRange)
{
     TLinkedList<waypoint_s *>::node_s *p;
     waypoint_s *pNearest = NULL;
     short i, j, MinI, MaxI, MinJ, MaxJ, Offset = (short)ceil(flRange / MAX_MAP_GRIDS);
     float flNearestDist = 9999.99f, flDist;

     WaypointClass.GetNodeIndexes(v_src, &i, &j);
     MinI = i - Offset;
     MaxI = i + Offset;
     MinJ = j - Offset;
     MaxJ = j + Offset;

     if (MinI < 0)
          MinI = 0;
     if (MaxI > MAX_MAP_GRIDS - 1)
          MaxI = MAX_MAP_GRIDS - 1;
     if (MinJ < 0)
          MinJ = 0;
     if (MaxJ > MAX_MAP_GRIDS - 1)
          MaxJ = MAX_MAP_GRIDS - 1;

     for (int x=MinI;x<=MaxI;x++)
     {
          for(int y=MinJ;y<=MaxJ;y++)
          {
               p = m_WaypointList[x][y].GetFirst();

               while(p)
               {
                    if (!(p->Entry->pNode->iFlags & W_FL_TRIGGER))
                    {
                         p = p->next;
                         continue;
                    }

                    flDist = GetDistance(v_src, p->Entry->pNode->v_origin);
                    if ((flDist < flNearestDist) && (flDist <= flRange))
                    {
                         if (::IsVisible(v_src, p->Entry->pNode->v_origin, NULL))
                         {
                              pNearest = p->Entry;
                              flNearestDist = flDist;
                         }
                    }

                    p = p->next;
               }
          }
     }
     return pNearest;
}

// Makes a waypoint list for this bot based on the list from WaypointClass
void CBot::SyncWaypoints()
{
     short x, y;
     TLinkedList<node_s *>::node_s *p;

     // Clean everything first
     for (x=0;x<MAX_MAP_GRIDS;x++)
     {
          for (y=0;y<MAX_MAP_GRIDS;y++)
          {
               while(!m_WaypointList[x][y].Empty())
                    delete m_WaypointList[x][y].Pop();
          }
     }

     // Sync
     for (x=0;x<MAX_MAP_GRIDS;x++)
     {
          for (y=0;y<MAX_MAP_GRIDS;y++)
          {
               p = WaypointClass.m_Waypoints[x][y].GetFirst();
               while(p)
               {
                    waypoint_s *pWP = new waypoint_s;
                    pWP->pNode = p->Entry;
                    m_WaypointList[x][y].AddNode(pWP);

#ifndef RELEASE_BUILD
                    if (!GetWPFromNode(p->Entry)) condebug("Error adding bot wp!");
#endif

                    p = p->next;
               }
          }
     }
}

#ifdef WP_FLOOD
waypoint_s *CBot::GetNearestFloodWP(vec v_origin, float flRange)
{
     TLinkedList<waypoint_s *>::node_s *p;
     waypoint_s *pNearest = NULL;
     short i, j, MinI, MaxI, MinJ, MaxJ, Offset = (short)ceil(flRange / MAX_MAP_GRIDS);
     float flNearestDist = 9999.99f, flDist;

     WaypointClass.GetNodeIndexes(v_origin, &i, &j);
     MinI = i - Offset;
     MaxI = i + Offset;
     MinJ = j - Offset;
     MaxJ = j + Offset;

     if (MinI < 0)
          MinI = 0;
     if (MaxI > MAX_MAP_GRIDS - 1)
          MaxI = MAX_MAP_GRIDS - 1;
     if (MinJ < 0)
          MinJ = 0;
     if (MaxJ > MAX_MAP_GRIDS - 1)
          MaxJ = MAX_MAP_GRIDS - 1;

     for (int x=MinI;x<=MaxI;x++)
     {
          for(int y=MinJ;y<=MaxJ;y++)
          {
               p = m_WaypointList[x][y].GetFirst();

               while(p)
               {
                    if (!(p->Entry->pNode->iFlags & W_FL_FLOOD))
                    {
                         p = p->next;
                         continue;
                    }

                    flDist = GetDistance(v_origin, p->Entry->pNode->v_origin);
                    if ((flDist < flNearestDist) && (flDist <= flRange))
                    {
                         if (::IsVisible(v_origin, p->Entry->pNode->v_origin, NULL))
                         {
                              pNearest = p->Entry;
                              flNearestDist = flDist;
                         }
                    }

                    p = p->next;
               }
          }
     }
     return pNearest;
}

waypoint_s *CBot::GetNearestTriggerFloodWP(vec v_origin, float flRange)
{
     TLinkedList<waypoint_s *>::node_s *p;
     waypoint_s *pNearest = NULL;
     short i, j, MinI, MaxI, MinJ, MaxJ, Offset = (short)ceil(flRange / MAX_MAP_GRIDS);
     float flNearestDist = 9999.99f, flDist;

     WaypointClass.GetNodeIndexes(v_origin, &i, &j);
     MinI = i - Offset;
     MaxI = i + Offset;
     MinJ = j - Offset;
     MaxJ = j + Offset;

     if (MinI < 0)
          MinI = 0;
     if (MaxI > MAX_MAP_GRIDS - 1)
          MaxI = MAX_MAP_GRIDS - 1;
     if (MinJ < 0)
          MinJ = 0;
     if (MaxJ > MAX_MAP_GRIDS - 1)
          MaxJ = MAX_MAP_GRIDS - 1;

     for (int x=MinI;x<=MaxI;x++)
     {
          for(int y=MinJ;y<=MaxJ;y++)
          {
               p = m_WaypointList[x][y].GetFirst();

               while(p)
               {
                    if (!(p->Entry->pNode->iFlags & (W_FL_FLOOD | W_FL_TRIGGER)))
                    {
                         p = p->next;
                         continue;
                    }

                    flDist = GetDistance(v_origin, p->Entry->pNode->v_origin);
                    if ((flDist < flNearestDist) && (flDist <= flRange))
                    {
                         if (::IsVisible(v_origin, p->Entry->pNode->v_origin, NULL))
                         {
                              pNearest = p->Entry;
                              flNearestDist = flDist;
                         }
                    }

                    p = p->next;
               }
          }
     }
     return pNearest;
}

void CBot::GoToDebugGoal(vec o)
{
     ResetWaypointVars();

     node_s *wp = WaypointClass.GetNearestWaypoint(m_pMyEnt, 20.0f);
     node_s *goalwp = WaypointClass.GetNearestWaypoint(player1, 20.0f);

     if (!wp || !goalwp)
     {
          wp = WaypointClass.GetNearestFloodWP(m_pMyEnt, 8.0f);
          goalwp = WaypointClass.GetNearestFloodWP(player1, 5.0f);
     }
     if (!wp || !goalwp) { condebug("No near WP"); return; }

     SetCurrentWaypoint(wp);
     SetCurrentGoalWaypoint(goalwp);
     m_vGoal = o;
     m_bGoToDebugGoal = true;
}
#endif

// Bot class end
