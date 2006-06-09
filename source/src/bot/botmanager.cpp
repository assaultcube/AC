//
// C++ Implementation: bot
//
// Description: Code for botmanager
//
// Main bot file
//
// Author:  Rick <rickhelmus@gmail.com>
//
//
//

#include "../cube.h"

#ifndef VANILLA_CUBE // UNDONE
bool dedserv = false;
#define CS_DEDHOST -1
#endif

extern ENetHost *clienthost;

extern void respawnself();

CBotManager BotManager;

// Bot manager class begin

CBotManager::~CBotManager(void)
{
     EndMap();
     
     while(m_StoredBots.Empty() == false)
          delete m_StoredBots.Pop();
}

void CBotManager::Init()
{
     m_pBotToView = NULL;
     
     if (!ishost())
          return;

     m_bBotsShoot = true;
     m_bIdleBots = false;
     m_iFrameTime = 0;
     m_iPrevTime = lastmillis;
     m_sBotSkill = 1; // Default all bots have the skill 'Good'
     
     CreateSkillData();
     LoadBotNamesFile();
     LoadBotTeamsFile();
     WaypointClass.Init();
     lsrand(time(NULL));
}
     
void CBotManager::Think()
{    
     if (m_bInit)
     {
          Init();
          m_bInit = false;
     }

     if (m_pBotToView)
          ViewBot();

     if (!ishost())
          return;
     
     AddDebugText("m_sMaxAStarBots: %d", m_sMaxAStarBots);
     AddDebugText("m_sCurrentTriggerNr: %d", m_sCurrentTriggerNr);
     short x, y;
     WaypointClass.GetNodeIndexes(player1->o, &x, &y);
     AddDebugText("x: %d y: %d", x, y);
     
     m_iFrameTime = lastmillis - m_iPrevTime;
     if (m_iFrameTime > 250) m_iFrameTime = 250;
     m_iPrevTime = lastmillis;

     // Is it time to re-add bots?
     if ((m_fReAddBotDelay < lastmillis) && (m_fReAddBotDelay != -1.0f))
     {
          while(m_StoredBots.Empty() == false)
          {
               CStoredBot *pStoredBot = m_StoredBots.Pop();
               pStoredBot->ReAddBot();
               delete pStoredBot;
          }
          
          m_fReAddBotDelay = -1.0f;
     }
     
     // If this is a ded server check if there are any players, if not bots should be idle
     if (dedserv)
     {
          bool botsbeidle = true;
          loopv(players)
          {
               if (players[i] && (players[i]->state != CS_DEDHOST)) { botsbeidle = false; break; }
          }
     
          if (botsbeidle) return;
     }
     
     // Let all bots 'think'
     loopv(bots)
     {
          if (!bots[i])
               continue;

          if (bots[i]->pBot)
               bots[i]->pBot->Think();
          else
               condebug("Error: pBot == NULL in bot ent\n");
     }
}

void CBotManager::LoadBotNamesFile()
{
     if (!ishost())
          return;
          
     // Init bot names array first
     for (int i=0;i<100;i++)
          strcpy(m_szBotNames[i], "Bot");
     
     m_sBotNameCount = 0;
     
     // Load bot file
     char szNameFileName[256];
     MakeBotFileName("bot_names.txt", NULL, NULL, szNameFileName);
     FILE *fp = fopen(szNameFileName, "r");
     char szNameBuffer[256];
     int iIndex, iStrIndex;
     
     if (!fp)
     {
          conoutf("Warning: Couldn't load bot names file");
          return;
     }
     
     while (fgets(szNameBuffer, 80, fp) != NULL)
     {
          if (m_sBotNameCount >= 150)
          {
               conoutf("Warning: Max bot names reached(150), ignoring the rest of the"
                       "names");
               break;
          }
          
          short length = strlen(szNameBuffer);

          if (szNameBuffer[length-1] == '\n')
          {
               szNameBuffer[length-1] = 0;  // remove '\n'
               length--;
          }

          iStrIndex = 0;
          while (iStrIndex < length)
          {
               if ((szNameBuffer[iStrIndex] < ' ') || (szNameBuffer[iStrIndex] > '~') ||
                   (szNameBuffer[iStrIndex] == '"'))
               {
                    for (iIndex=iStrIndex; iIndex < length; iIndex++)
                         szNameBuffer[iIndex] = szNameBuffer[iIndex+1];
               }
 
               iStrIndex++;
          }

          if (szNameBuffer[0] != 0)
          {
               if (strlen(szNameBuffer) >= 16)
               {     conoutf("Warning: bot name \"%s\" has to many characters(16 is max)",
                            (int)szNameBuffer);
               }
               strn0cpy(m_szBotNames[m_sBotNameCount], szNameBuffer, 16);
               m_sBotNameCount++;
          }
     }
     fclose(fp);
}
     
char *CBotManager::GetBotName()
{
     char *szOutput = NULL;
     TMultiChoice<char *> BotNameChoices;
     short ChoiceVal;
     
     for(int j=0;j<m_sBotNameCount;j++)
     {
          ChoiceVal = 50;
          
          loopv(players)
          {
               if (players[i] && (players[i]->state != CS_DEDHOST) && 
                   !strcasecmp(players[i]->name, m_szBotNames[j]))
                    ChoiceVal -= 10;
          }
          
          loopv(bots)
          {
               if (bots[i] && (!strcasecmp(bots[i]->name, m_szBotNames[j])))
                    ChoiceVal -= 10;
          }
          
          if ((player1->state != CS_DEDHOST) && !strcasecmp(player1->name, m_szBotNames[j]))
               ChoiceVal -= 10;
               
          if (ChoiceVal <= 0)
               ChoiceVal = 1;
          
          BotNameChoices.Insert(m_szBotNames[j], ChoiceVal);
     }
     
     // Couldn't find a selection?
     if (!BotNameChoices.GetSelection(szOutput))
          szOutput = "Bot";
     
     return szOutput;
}
     
void CBotManager::LoadBotTeamsFile()
{
     if (!ishost())
          return;
          
     // Init bot teams array first
     for (int i=0;i<20;i++)
          strcpy(m_szBotTeams[i], "b0ts");
     
     m_sBotTeamCount = 0;
     
     // Load bot file
     char szNameFileName[256];
     MakeBotFileName("bot_teams.txt", NULL, NULL, szNameFileName);
     FILE *fp = fopen(szNameFileName, "r");
     char szNameBuffer[256];
     int iIndex, iStrIndex;
     
     if (!fp)
     {
          conoutf("Warning: Couldn't load bot teams file");
          return;
     }
     
     while ((m_sBotTeamCount < 20) && (fgets(szNameBuffer, 80, fp) != NULL))
     {
          short length = strlen(szNameBuffer);

          if (szNameBuffer[length-1] == '\n')
          {
               szNameBuffer[length-1] = 0;  // remove '\n'
               length--;
          }

          iStrIndex = 0;
          while (iStrIndex < length)
          {
               if ((szNameBuffer[iStrIndex] < ' ') || (szNameBuffer[iStrIndex] > '~') ||
                   (szNameBuffer[iStrIndex] == '"'))
               {
                    for (iIndex=iStrIndex; iIndex < length; iIndex++)
                         szNameBuffer[iIndex] = szNameBuffer[iIndex+1];
               }
 
               iStrIndex++;
          }

          if (szNameBuffer[0] != 0)
          {
               strn0cpy(m_szBotTeams[m_sBotTeamCount], szNameBuffer, 5);
               m_sBotTeamCount++;
          }
     }
     fclose(fp);
}

char *CBotManager::GetBotTeam()
{
     char *szOutput = NULL;
     TMultiChoice<char *> BotTeamChoices;
     short ChoiceVal;
     
     for(int j=0;j<m_sBotTeamCount;j++)
     {
          ChoiceVal = 50;
          /* UNDONE?
          loopv(players)
          {
               if (players[i] && (!strcasecmp(players[i]->name, m_szBotNames[j])))
                    ChoiceVal -= 10;
          }
          
          loopv(bots)
          {
               if (bots[i] && (!strcasecmp(bots[i]->name, m_szBotNames[j])))
                    ChoiceVal -= 10;
          }
          
          if (!strcasecmp(player1->name, m_szBotNames[j]))
               ChoiceVal -= 10;
               
          if (ChoiceVal <= 0)
               ChoiceVmonsterclearal = 1;*/
          
          BotTeamChoices.Insert(m_szBotTeams[j], ChoiceVal);
     }
     
     // Couldn't find a selection?
     if (!BotTeamChoices.GetSelection(szOutput))
          szOutput = "b0t";
     
     return szOutput;
}

void CBotManager::RenderBots()
{
     static bool drawblue;
     
     loopv(bots)
     {
          if (bots[i] && (bots[i] != m_pBotToView))
          {
               drawblue = (m_sp || isteam(player1->team, bots[i]->team));
               renderclient(bots[i], drawblue, "playermodels/counterterrorist", false, 1.0f);
          }
     }
}

void CBotManager::EndMap()
{
     // Remove all bots
     loopv(bots)
     {                
          if (!bots[i])
               continue;

          if (ishost()) // Store bots so they can be re-added after map change
          {
               if (bots[i]->pBot && bots[i]->name[0] && bots[i]->team[0])
               {
                    CStoredBot *pStoredBot = new CStoredBot(bots[i]->name, bots[i]->team,
                                                            bots[i]->pBot->m_sSkillNr);
                    m_StoredBots.AddNode(pStoredBot);
               }
               delete bots[i]->pBot;
          }
          
          bots[i]->pBot = NULL;
          zapdynent(bots[i]);
     }
     bots.setsize(0);     
     condebug("Cleared all bots");
     m_fReAddBotDelay = lastmillis + 7500;
     //if(ishost()) WaypointClass.SaveWPExpFile(); //UNDONE
}

void CBotManager::BeginMap(char *szMapName)
{
     EndMap(); // End previous map
     
     if (!ishost()) return;
     
     WaypointClass.Init();
     WaypointClass.SetMapName(szMapName);
     if (!WaypointClass.LoadWaypoints())
          WaypointClass.StartFlood();
     //WaypointClass.LoadWPExpFile(); // UNDONE

     CalculateMaxAStarCount();
     m_sUsingAStarBotsCount = 0;
     PickNextTrigger();
}
     
int CBotManager::GetBotIndex(dynent *m)
{
     loopv(bots)
     {
          if (!bots[i])
               continue;
     
          if (bots[i] == m)
               return i;
     }
     
     return -1;
}

void CBotManager::LetBotsUpdateStats()
{
     loopv(bots) if (bots[i] && bots[i]->pBot) bots[i]->pBot->m_bSendC2SInit = false;
}

void CBotManager::LetBotsHear(int n, vec *loc)
{
     if (bots.length() == 0) return;
     
     if (!loc) return;
          
     loopv(bots)
     {
          if (!bots[i] || !bots[i]->pBot || (bots[i]->state == CS_DEAD)) continue;
          bots[i]->pBot->HearSound(n, loc);
     }
}

// Notify all bots of a new waypoint
void CBotManager::AddWaypoint(node_s *pNode)
{
     if (bots.length())
     {
          short x, y;
          waypoint_s *pWP;
     
          loopv(bots)
          {
               if (!bots[i] || !bots[i]->pBot) continue;
          
               pWP = new waypoint_s;
               pWP->pNode = pNode;
               WaypointClass.GetNodeIndexes(pNode->v_origin, &x, &y);
               bots[i]->pBot->m_WaypointList[x][y].AddNode(pWP);
               
#ifndef RELEASE_BUILD
               if (!bots[i]->pBot->GetWPFromNode(pNode)) condebug("Error adding bot wp!");
#endif               
          }
     }
     
     CalculateMaxAStarCount();
}

// Notify all bots of a deleted waypoint
void CBotManager::DelWaypoint(node_s *pNode)
{
     if (bots.length())
     {
          short x, y;
          TLinkedList<waypoint_s *>::node_s *p;
     
          loopv(bots)
          {
               if (!bots[i] || !bots[i]->pBot) continue;
          
               WaypointClass.GetNodeIndexes(pNode->v_origin, &x, &y);
               p = bots[i]->pBot->m_WaypointList[x][y].GetFirst();
          
               while(p)
               {
                    if (p->Entry->pNode == pNode)
                    {
                         delete p->Entry;
                         bots[i]->pBot->m_WaypointList[x][y].DeleteNode(p);
                         break;
                    }
                    p = p->next;
               }          
          }
     }
     
     CalculateMaxAStarCount();
}

void CBotManager::MakeBotFileName(const char *szFileName, const char *szDir1, const char *szDir2, char *szOutput)
{
     char *DirSeperator;

#ifndef __linux__
     DirSeperator = "\\";
     strcpy(szOutput, "bot\\");
#else
     DirSeperator = "/";
     strcpy(szOutput, "bot/");
#endif
     
     if (szDir1)
     {
          strcat(szOutput, szDir1);
          strcat(szOutput, DirSeperator);
     }
     
     if (szDir2)
     {
          strcat(szOutput, szDir2);
          strcat(szOutput, DirSeperator);
     }
     
     strcat(szOutput, szFileName);
}

void CBotManager::CreateSkillData()
{
     // First give the bot skill structure some default data
     InitSkillData();
     
     // Now see if we can load the skill.cfg file
     char SkillFileName[256] = "";
     FILE *pSkillFile = NULL;
     int SkillNr = -1;
     float value = 0;

     MakeBotFileName("bot_skill.cfg", NULL, NULL, SkillFileName);

     pSkillFile = fopen(SkillFileName, "r");

     conoutf("Reading bot_skill.cfg file... ");

     if (pSkillFile == NULL) // file doesn't exist
     {
          conoutf("skill file not found, default settings will be used\n");
          return;
     }

     int ch;
     char cmd_line[256];
     int cmd_index;
     char *cmd, *arg1;

     while (pSkillFile)
     {    
          cmd_index = 0;
          cmd_line[cmd_index] = 0;

          ch = fgetc(pSkillFile);

          // skip any leading blanks
          while (ch == ' ')
               ch = fgetc(pSkillFile);

          while ((ch != EOF) && (ch != '\r') && (ch != '\n'))
          {
               if (ch == '\t')  // convert tabs to spaces
                    ch = ' ';

               cmd_line[cmd_index] = ch;

               ch = fgetc(pSkillFile);

               // skip multiple spaces in input file
               while ((cmd_line[cmd_index] == ' ') && (ch == ' '))      
                    ch = fgetc(pSkillFile);

               cmd_index++;
          }

          if (ch == '\r')  // is it a carriage return?
          {
               ch = fgetc(pSkillFile);  // skip the linefeed
          }

          // if reached end of file, then close it
          if (ch == EOF)
          {
               fclose(pSkillFile);
               pSkillFile = NULL;
          }

          cmd_line[cmd_index] = 0;  // terminate the command line

          cmd_index = 0;
          cmd = cmd_line;
          arg1 = NULL;

          // skip to blank or end of string...
          while ((cmd_line[cmd_index] != ' ') && (cmd_line[cmd_index] != 0))
                cmd_index++;

          if (cmd_line[cmd_index] == ' ')
          {
                cmd_line[cmd_index++] = 0;
                arg1 = &cmd_line[cmd_index];
          }

          if ((cmd_line[0] == '#') || (cmd_line[0] == 0))
               continue;  // skip if comment or blank line


          if (strcasecmp(cmd, "[SKILL1]") == 0)
               SkillNr = 0;
          else if (strcasecmp(cmd, "[SKILL2]") == 0)
               SkillNr = 1;
          else if (strcasecmp(cmd, "[SKILL3]") == 0)
               SkillNr = 2;
          else if (strcasecmp(cmd, "[SKILL4]") == 0)
               SkillNr = 3;
          else if (strcasecmp(cmd, "[SKILL5]") == 0)
               SkillNr = 4;

          if ((arg1 == NULL) || (*arg1 == 0))
              continue;

          if (SkillNr == -1) // Not in a skill block yet?
               continue;

          value = atof(arg1);

          if (strcasecmp(cmd, "min_x_aim_speed") == 0)
          {
               m_BotSkills[SkillNr].flMinAimXSpeed = value;
          }
          else if (strcasecmp(cmd, "max_x_aim_speed") == 0)
          {
               m_BotSkills[SkillNr].flMaxAimXSpeed = value;
          }
          else if (strcasecmp(cmd, "min_y_aim_speed") == 0)
          {
               m_BotSkills[SkillNr].flMinAimYSpeed = value;
          }
          else if (strcasecmp(cmd, "max_y_aim_speed") == 0)
          {
               m_BotSkills[SkillNr].flMaxAimYSpeed = value;
          }
          else if (strcasecmp(cmd, "min_x_aim_offset") == 0)
          {
               m_BotSkills[SkillNr].flMinAimXOffset = value;
          }
          else if (strcasecmp(cmd, "max_x_aim_offset") == 0)
          {
               m_BotSkills[SkillNr].flMaxAimXOffset = value;
          }
          else if (strcasecmp(cmd, "min_y_aim_offset") == 0)
          {
               m_BotSkills[SkillNr].flMinAimYOffset = value;
          }
          else if (strcasecmp(cmd, "max_y_aim_offset") == 0)
          {
               m_BotSkills[SkillNr].flMaxAimYOffset = value;
          }
          else if (strcasecmp(cmd, "min_attack_delay") == 0)
          {
               m_BotSkills[SkillNr].flMinAttackDelay = value;
          }
          else if (strcasecmp(cmd, "max_attack_delay") == 0)
          {
               m_BotSkills[SkillNr].flMaxAttackDelay = value;
          }
          else if (strcasecmp(cmd, "min_enemy_search_delay") == 0)
          {
               m_BotSkills[SkillNr].flMinEnemySearchDelay = value;
          }
          else if (strcasecmp(cmd, "max_enemy_search_delay") == 0)
          {
               m_BotSkills[SkillNr].flMaxEnemySearchDelay = value;
          }
          else if (strcasecmp(cmd, "shoot_at_feet_percent") == 0)
          {
               if (value < 0) value = 0;
               else if (value > 100) value = 100;
               m_BotSkills[SkillNr].sShootAtFeetWithRLPercent = (short)value;
          }
          else if (strcasecmp(cmd, "can_predict_position") == 0)
          {
               m_BotSkills[SkillNr].bCanPredict = value;
          }
          else if (strcasecmp(cmd, "max_hear_volume") == 0)
          {
               if (value < 0) value = 0;
               else if (value > 255) value = 100;
               m_BotSkills[SkillNr].iMaxHearVolume = (int)value;
          }
          else if (strcasecmp(cmd, "can_circle_strafe") == 0)
          {
               m_BotSkills[SkillNr].bCircleStrafe = value;
          }     
          else if (strcasecmp(cmd, "can_search_items_in_combat") == 0)
          {
               m_BotSkills[SkillNr].bCanSearchItemsInCombat = value;
          }          
     }

     conoutf("done\n");
}

void CBotManager::InitSkillData()
{
     // Best skill
     m_BotSkills[0].flMinReactionDelay = 0.015f;
     m_BotSkills[0].flMaxReactionDelay = 0.035f;
     m_BotSkills[0].flMinAimXOffset = 15.0f;
     m_BotSkills[0].flMaxAimXOffset = 20.0f;
     m_BotSkills[0].flMinAimYOffset = 10.0f;
     m_BotSkills[0].flMaxAimYOffset = 15.0f;
     m_BotSkills[0].flMinAimXSpeed = 330.0f;
     m_BotSkills[0].flMaxAimXSpeed = 355.0f;
     m_BotSkills[0].flMinAimYSpeed = 400.0f;
     m_BotSkills[0].flMaxAimYSpeed = 450.0f;
     m_BotSkills[0].flMinAttackDelay = 0.1f;
     m_BotSkills[0].flMaxAttackDelay = 0.4f;
     m_BotSkills[0].flMinEnemySearchDelay = 0.09f;
     m_BotSkills[0].flMaxEnemySearchDelay = 0.12f;
     m_BotSkills[0].sShootAtFeetWithRLPercent = 85;
     m_BotSkills[0].bCanPredict = true;
     m_BotSkills[0].iMaxHearVolume = 75;
     m_BotSkills[0].bCircleStrafe = true;
     m_BotSkills[0].bCanSearchItemsInCombat = true;

     // Good skill
     m_BotSkills[1].flMinReactionDelay = 0.035f;
     m_BotSkills[1].flMaxReactionDelay = 0.045f;
     m_BotSkills[1].flMinAimXOffset = 20.0f;
     m_BotSkills[1].flMaxAimXOffset = 25.0f;
     m_BotSkills[1].flMinAimYOffset = 15.0f;
     m_BotSkills[1].flMaxAimYOffset = 20.0f;
     m_BotSkills[1].flMinAimXSpeed = 250.0f;
     m_BotSkills[1].flMaxAimXSpeed = 265.0f;
     m_BotSkills[1].flMinAimYSpeed = 260.0f;
     m_BotSkills[1].flMaxAimYSpeed = 285.0f;
     m_BotSkills[1].flMinAttackDelay = 0.3f;
     m_BotSkills[1].flMaxAttackDelay = 0.6f;
     m_BotSkills[1].flMinEnemySearchDelay = 0.12f;
     m_BotSkills[1].flMaxEnemySearchDelay = 0.17f;
     m_BotSkills[1].sShootAtFeetWithRLPercent = 60;
     m_BotSkills[1].bCanPredict = true;
     m_BotSkills[1].iMaxHearVolume = 60;
     m_BotSkills[1].bCircleStrafe = true;
     m_BotSkills[1].bCanSearchItemsInCombat = true;

     // Medium skill
     m_BotSkills[2].flMinReactionDelay = 0.075f;
     m_BotSkills[2].flMaxReactionDelay = 0.010f;
     m_BotSkills[2].flMinAimXOffset = 25.0f;
     m_BotSkills[2].flMaxAimXOffset = 30.0f;
     m_BotSkills[2].flMinAimYOffset = 20.0f;
     m_BotSkills[2].flMaxAimYOffset = 25.0f;
     m_BotSkills[2].flMinAimXSpeed = 190.0f;
     m_BotSkills[2].flMaxAimXSpeed = 125.0f;
     m_BotSkills[2].flMinAimYSpeed = 210.0f;
     m_BotSkills[2].flMaxAimYSpeed = 240.0f;
     m_BotSkills[2].flMinAttackDelay = 0.75f;
     m_BotSkills[2].flMaxAttackDelay = 1.0f;
     m_BotSkills[2].flMinEnemySearchDelay = 0.18f;
     m_BotSkills[2].flMaxEnemySearchDelay = 0.22f;
     m_BotSkills[2].sShootAtFeetWithRLPercent = 25;
     m_BotSkills[2].bCanPredict = false;
     m_BotSkills[2].iMaxHearVolume = 45;
     m_BotSkills[2].bCircleStrafe = true;
     m_BotSkills[2].bCanSearchItemsInCombat = false;

     // Worse skill
     m_BotSkills[3].flMinReactionDelay = 0.15f;
     m_BotSkills[3].flMaxReactionDelay = 0.20f;
     m_BotSkills[3].flMinAimXOffset = 30.0f;
     m_BotSkills[3].flMaxAimXOffset = 35.0f;
     m_BotSkills[3].flMinAimYOffset = 25.0f;
     m_BotSkills[3].flMaxAimYOffset = 30.0f;
     m_BotSkills[3].flMinAimXSpeed = 155.0f;
     m_BotSkills[3].flMaxAimXSpeed = 170.0f;
     m_BotSkills[3].flMinAimYSpeed = 160.0f;
     m_BotSkills[3].flMaxAimYSpeed = 210.0f;
     m_BotSkills[3].flMinAttackDelay = 1.2f;
     m_BotSkills[3].flMaxAttackDelay = 1.6f;
     m_BotSkills[3].flMinEnemySearchDelay = 0.25f;
     m_BotSkills[3].flMaxEnemySearchDelay = 0.30f;
     m_BotSkills[3].sShootAtFeetWithRLPercent = 10;
     m_BotSkills[3].bCanPredict = false;
     m_BotSkills[3].iMaxHearVolume = 30;
     m_BotSkills[3].bCircleStrafe = false;
     m_BotSkills[3].bCanSearchItemsInCombat = false;

     // Bad skill
     m_BotSkills[4].flMinReactionDelay = 0.30f;
     m_BotSkills[4].flMaxReactionDelay = 0.50f;
     m_BotSkills[4].flMinAimXOffset = 35.0f;
     m_BotSkills[4].flMaxAimXOffset = 40.0f;
     m_BotSkills[4].flMinAimYOffset = 30.0f;
     m_BotSkills[4].flMaxAimYOffset = 35.0f;
     m_BotSkills[4].flMinAimXSpeed = 45.0f;
     m_BotSkills[4].flMaxAimXSpeed = 60.0f;
     m_BotSkills[4].flMinAimYSpeed = 125.0f;
     m_BotSkills[4].flMaxAimYSpeed = 180.0f;
     m_BotSkills[4].flMinAttackDelay = 1.5f;
     m_BotSkills[4].flMaxAttackDelay = 2.0f;
     m_BotSkills[4].flMinEnemySearchDelay = 0.30f;
     m_BotSkills[4].flMaxEnemySearchDelay = 0.36f;
     m_BotSkills[4].sShootAtFeetWithRLPercent = 0;
     m_BotSkills[4].bCanPredict = false;
     m_BotSkills[4].iMaxHearVolume = 15;
     m_BotSkills[4].bCircleStrafe = false;
     m_BotSkills[4].bCanSearchItemsInCombat = false;
}
    
void CBotManager::ChangeBotSkill(short Skill, dynent *bot)
{
     static char *SkillNames[5] = { "best", "good", "medium", "worse", "bad" };
     
     if (bot && bot->pBot)
     {
          // Only change skill of a single bot
          bot->pBot->m_pBotSkill = &m_BotSkills[Skill];
          bot->pBot->m_sSkillNr = Skill;
          conoutf("Skill of %s is now %s", (int)bot->name, (int)SkillNames[Skill]);
          return;
     }
     
     // Change skill of all bots
     loopv(bots)
     {
          if (!bots[i] || !bots[i]->pBot) continue;
          
          bots[i]->pBot->m_pBotSkill = &m_BotSkills[Skill];
          bots[i]->pBot->m_sSkillNr = Skill;
     }
     
     // Change default bot skill
     m_sBotSkill = Skill;
     
     conoutf("Skill of all bots is now %s", (int)SkillNames[Skill]);
}

void CBotManager::ViewBot()
{   
     // Check if this bot is still in game
     bool bFound = false;
     loopv(bots)
     {
          if (bots[i] == m_pBotToView)
          {
               bFound = true;
               break;
          }
     }
     
     if (!bFound)
     {
          DisableBotView();
          return;
     }
     
     player1->state = CS_DEAD; // Fake dead
     
     player1->o = m_pBotToView->o;
     player1->o.z += 1.0f;
     player1->yaw = m_pBotToView->yaw;
     player1->pitch = m_pBotToView->pitch;
     player1->roll = m_pBotToView->roll;
     player1->radius = 0; // Don't collide
}

void CBotManager::DisableBotView()
{
     m_pBotToView = NULL;
     respawnself();
     player1->radius = 1.1f;
}

void CBotManager::CalculateMaxAStarCount()
{  
     if (WaypointClass.m_iWaypointCount > 0) // Are there any waypoints?
     {
          m_sMaxAStarBots = 8 - short(ceil((float)WaypointClass.m_iWaypointCount /
                                      1000.0f));
          if (m_sMaxAStarBots < 1)
               m_sMaxAStarBots = 1;
     }
     else
          m_sMaxAStarBots = 1;
}

void CBotManager::PickNextTrigger()
{
     if (!ishost()) return;

     short lowest = -1;
     bool found0 = false; // True if found a trigger with nr 0
     
     loopv(ents)
     {
          entity &e = ents[i];
          
#if defined AC_CUBE          
/*          if ((e.type != TRIGGER) || !e.spawned)
               continue;*/
#elif defined VANILLA_CUBE
          if ((e.type != CARROT) || !e.spawned)
               continue;
#endif          
          if (OUTBORD(e.x, e.y)) continue;
          
          vec o = { e.x, e.y, S(e.x, e.y)->floor+player1->eyeheight };

          node_s *pWptNearEnt = NULL;
          
          pWptNearEnt = WaypointClass.GetNearestTriggerWaypoint(o, 2.0f);
          
          if (pWptNearEnt)
          {
               if ((pWptNearEnt->sTriggerNr > 0) &&
                   ((pWptNearEnt->sTriggerNr < lowest) || (lowest == -1)))
                    lowest = pWptNearEnt->sTriggerNr;
               if (pWptNearEnt->sTriggerNr == 0) found0 = true;
          }
          
#ifdef WP_FLOOD
          pWptNearEnt = WaypointClass.GetNearestTriggerFloodWP(o, 2.0f);
          
          if (pWptNearEnt)
          {
               if ((pWptNearEnt->sTriggerNr > 0) && 
                   ((pWptNearEnt->sTriggerNr < lowest) || (lowest == -1)))
                    lowest = pWptNearEnt->sTriggerNr;
               if (pWptNearEnt->sTriggerNr == 0) found0 = true;
          }
                    
#endif               
     }
     
     if ((lowest == -1) && found0) lowest = 0;
     
     if (lowest != -1)
          m_sCurrentTriggerNr = lowest;
}

dynent *CBotManager::CreateBot(const char *team, const char *skill, const char *name)
{
     dynent *m = newdynent();
     if (!m) return NULL;
     
     // Create new bot class, dependand on the current mod
#if defined VANILLA_CUBE     
     m->pBot = new CCubeBot;
#elif defined AC_CUBE
    m->pBot = new CACBot;
#else
     #error "Unsupported mod!"
#endif
     m->bIsBot = true;
     m->pBot->m_pMyEnt = m;
     m->pBot->m_iLastBotUpdate = 0;
     m->pBot->m_bSendC2SInit = false;

     if (name && *name)
          strn0cpy(m->name, name, 16);
     else
          strn0cpy(m->name, BotManager.GetBotName(), 16);
     
     if (team && *team && strcmp(team, "random"))
          strn0cpy(m->team, team, 5);
     else
          strn0cpy(m->team, BotManager.GetBotTeam(), 5);
     
     if (skill && *skill && strcmp(skill, "random"))
     {
          if (!strcasecmp(skill, "best"))
               m->pBot->m_sSkillNr = 0;
          else if (!strcasecmp(skill, "good"))
               m->pBot->m_sSkillNr = 1;
          else if (!strcasecmp(skill, "medium"))
               m->pBot->m_sSkillNr = 2;
          else if (!strcasecmp(skill, "worse"))
               m->pBot->m_sSkillNr = 3;
          else if (!strcasecmp(skill, "bad"))
               m->pBot->m_sSkillNr = 4;
          else
          {
               conoutf("Wrong skill specified. Should be best, good, medium, "
                       "worse or bad");
               conoutf("Using default skill instead...");
               m->pBot->m_sSkillNr = BotManager.m_sBotSkill;
          }
     }
     else // No skill specified, use default
          m->pBot->m_sSkillNr = BotManager.m_sBotSkill;
          
     m->pBot->m_pBotSkill = &BotManager.m_BotSkills[m->pBot->m_sSkillNr];

     // Sync waypoints
     m->pBot->SyncWaypoints();
              
     m->pBot->Spawn();
     
     bots.add(m);
     
     return m;     
}

// Bot manager class end

void addbot(char *arg1, char *arg2, char *arg3)
{
     if (ishost())
     {
          conoutf("Creating bot...\n");
          dynent *b = BotManager.CreateBot(arg1, arg2, arg3);
           if (b)
               conoutf("connected: %s", (int)b->name);
           else
           {
               conoutf("Error: Couldn't create bot!");
               return;
           }
     }
     else if (clienthost)
     {
          char team[32], name[32];
          int skill = -1;
          
          team[0] = name[0] = 0;
          
          if (arg1 && arg1[0]) strcpy(team, arg1);
          if (arg3 && arg3[0]) strcpy(name, arg3);
          if (arg2 && arg2[0])
          {
               if (!strcasecmp(arg2, "best")) skill = 0;
               else if (!strcasecmp(arg2, "good")) skill = 1;
               else if (!strcasecmp(arg2, "medium")) skill = 2;
               else if (!strcasecmp(arg2, "worse")) skill = 3;
               else if (!strcasecmp(arg2, "bad")) skill = 4;
          }
          
          ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
          uchar *start = packet->data;
          uchar *p = start+2;
          putint(p, SV_BOTCOMMAND);
          putint(p, int(COMMAND_ADDBOT)); // Bot command type
          putint(p, 1); // Bot count
          putint(p, skill); // Bot skill
          sendstring(team, p); // Bot team
          sendstring(name, p); // Bot name
          *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
          enet_packet_resize(packet, p-start);
          sendpackettoserv(packet);
     }
}

COMMAND(addbot, ARG_3STR);

void addnbot(char *arg1, char *arg2, char *arg3)
{
     if (!arg1 || !arg1[0]) return;
     
     int i = atoi(arg1);
     
     if (ishost())
     {
          while(i > 0)
          {
               addbot(arg2, arg3, NULL);
               i--;
          }
     }
     else if (clienthost)
     {
          char team[32], name[32];
          int skill = -1;
          
          team[0] = name[0] = 0;
          
          if (arg2 && arg2[0]) strcpy(team, arg2);
//          if (arg4 && arg4[0]) strcpy(name, arg4);
          if (arg3 && arg3[0])
          {
               if (!strcasecmp(arg2, "best")) skill = 0;
               else if (!strcasecmp(arg2, "good")) skill = 1;
               else if (!strcasecmp(arg2, "medium")) skill = 2;
               else if (!strcasecmp(arg2, "worse")) skill = 3;
               else if (!strcasecmp(arg2, "bad")) skill = 4;
          }
          
          ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
          uchar *start = packet->data;
          uchar *p = start+2;
          putint(p, SV_BOTCOMMAND);
          putint(p, int(COMMAND_ADDBOT)); // Bot command type
          putint(p, i); // Bot count
          putint(p, skill); // Bot skill
          sendstring(team, p); // Bot team
          sendstring(name, p); // Bot name
          *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
          enet_packet_resize(packet, p-start);
          sendpackettoserv(packet);
     }     
}

COMMAND(addnbot, ARG_3STR);

void botsshoot(int Shoot)
{
     if (!ishost()) return;
     if (Shoot)
     {
          BotManager.SetBotsShoot(true);
          conoutf("Bots will shoot");
     }
     else
     {
          BotManager.SetBotsShoot(false);
          conoutf("Bots won't shoot");
     }
}

COMMAND(botsshoot, ARG_1INT);
          
void idlebots(int Idle)
{
     if (!ishost()) return;
     if (Idle)
     {
          BotManager.SetIdleBots(true);
          conoutf("Bots are idle");
     }
     else
     {
          BotManager.SetIdleBots(false);
          conoutf("Bots aren't idle");
     }
}

COMMAND(idlebots, ARG_1INT);

void drawbeamtobots()
{
     loopv(bots)
     {
          if (bots[i])
               particle_trail(1, 500, player1->o, bots[i]->o);
     }
}

COMMAND(drawbeamtobots, ARG_NONE);

void kickbot(const char *szName)
{
     if (!szName || !(*szName))
          return;
          
     int iBotInd = -1;
     loopv(bots)
     {
          if (!bots[i]) continue;
          
          if (!strcmp(bots[i]->name, szName))
          {
               iBotInd = i;
               break;
          }
     }
     
     if (iBotInd != -1)
     {
          if (ishost())
          {
               addmsg(1, 2, SV_BOTDIS, iBotInd);
          
               conoutf("bot %s disconnected", (int)(bots[iBotInd]->name[0] ? bots[iBotInd]->name :
                       "[incompatible client]"));
               delete bots[iBotInd]->pBot;
               zapdynent(bots[iBotInd]);
               bots.remove(iBotInd);
          }
          else if (clienthost) // Ask server to destroy bot(with voting)
          {
               ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
               uchar *start = packet->data;
               uchar *p = start+2;
               putint(p, SV_BOTCOMMAND);
               putint(p, int(COMMAND_KICKBOT)); // Bot command type
               putint(p, 1); // 0==kick all bots, 1==kick one specific bot
               char tmp[20];
               strcpy(tmp, szName);
               sendstring(tmp, p); // Bot name
               *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
               enet_packet_resize(packet, p-start);
               sendpackettoserv(packet);
          }               
     }
}

COMMAND(kickbot, ARG_1STR);

void kickallbots(void)
{
     loopv(bots)
     {
          if (bots[i])
          {
               if (ishost())
               {
                    addmsg(1, 2, SV_BOTDIS, i);
          
                    conoutf("bot %s disconnected",(int)(bots[i]->name[0] ?
                            bots[i]->name : "[incompatible client]"));
                    delete bots[i]->pBot;
                    zapdynent(bots[i]);
               }
               else if (clienthost) // Ask the server to destroy all bots
               {
                    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS,
                                                            ENET_PACKET_FLAG_RELIABLE);
                    uchar *start = packet->data;
                    uchar *p = start+2;
                    putint(p, SV_BOTCOMMAND);
                    putint(p, int(COMMAND_KICKBOT)); // Bot command type
                    putint(p, 0); // 0==kick all bots, 1==kick one specific bot
                    *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
                    enet_packet_resize(packet, p-start);
                    sendpackettoserv(packet);
               }               
          }
     }
     
     bots.setsize(0);
}

COMMAND(kickallbots, ARG_NONE);

void togglegrap()
{
     if (SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GrabMode(0))
          SDL_WM_GrabInput(SDL_GRAB_ON);
     else
          SDL_WM_GrabInput(SDL_GrabMode(0));
}

COMMAND(togglegrap, ARG_NONE);

void togglebotview(char *bot)
{         
     if (BotManager.m_pBotToView)
          BotManager.DisableBotView();
     else if (bot && *bot)
     {
          loopv(bots)
          {
               if (!bots[i]) continue;
               
               if (!strcmp(bots[i]->name, bot))
               {
                    BotManager.EnableBotView(bots[i]);
                    break;
               }
          }
     }
}

COMMAND(togglebotview, ARG_1STR);

void botskill(char *bot, char *skill)
{
     if (!skill || !(*skill))
          return;
     
     short SkillNr;
     
     if (!strcasecmp(skill, "best"))
          SkillNr = 0;
     else if (!strcasecmp(skill, "good"))
          SkillNr = 1;
     else if (!strcasecmp(skill, "medium"))
          SkillNr = 2;
     else if (!strcasecmp(skill, "worse"))
          SkillNr = 3;
     else if (!strcasecmp(skill, "bad"))
          SkillNr = 4;
     else
     {
          conoutf("Wrong skill specified. Should be best, good, medium, worse or bad");
          return;
     }
     
     if (ishost())
     {
          if (bot)
          {
               loopv(bots)
               {
                    if (bots[i] && !strcmp(bots[i]->name, bot))
                    {
                         BotManager.ChangeBotSkill(SkillNr, bots[i]);
                         break;
                    }
               }
          }
          else
               BotManager.ChangeBotSkill(SkillNr);
     }
     else if (clienthost) // ask server to change botskill
     {
          ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
          uchar *start = packet->data;
          uchar *p = start+2;
          putint(p, SV_BOTCOMMAND);
          putint(p, int(COMMAND_BOTSKILL)); // Bot command type
          putint(p, SkillNr);
          *(ushort *)start = ENET_HOST_TO_NET_16(p-start);
          enet_packet_resize(packet, p-start);
          sendpackettoserv(packet);
     }                         
}

COMMAND(botskill, ARG_2STR);

void botskillall(char *skill)
{
     botskill(NULL, skill);
}

COMMAND(botskillall, ARG_1STR);

#ifndef RELEASE_BUILD

#ifdef VANILLA_CUBE
void drawbeamtocarrots()
{
     loopv(ents)
     {
          entity &e = ents[i];
          vec o = { e.x, e.y, S(e.x, e.y)->floor+player1->eyeheight };
          if ((e.type != CARROT) || !e.spawned) continue;
          particle_trail(1, 500, player1->o, o);
     }
}

COMMAND(drawbeamtocarrots, ARG_NONE);

void drawbeamtoteleporters()
{
     loopv(ents)
     {
          entity &e = ents[i];
          vec o = { e.x, e.y, S(e.x, e.y)->floor+player1->eyeheight };
          if (e.type != TELEPORT) continue;
          particle_trail(1, 500, player1->o, o);
     }
}

COMMAND(drawbeamtoteleporters, ARG_NONE);
#endif

void telebot(void)
{
     vec dest = player1->o, forward, right, up;
     vec angles = { player1->pitch, player1->yaw, player1->roll };
     traceresult_s tr;
     
     AnglesToVectors(angles, forward, right, up);
     vmul(forward, 4.0f);
     vadd(dest, forward);
     
     TraceLine(player1->o, dest, player1, true, &tr);
     
     if (!tr.collided)
     {
          // Get the first bot
          loopv(bots)
          {
               if (!bots[i] || !bots[i]->pBot) continue;
               bots[i]->o = tr.end;
               break;
          }
     }
}

COMMAND(telebot, ARG_NONE);

void testvisible(int iDir)
{
     vec angles, end, forward, right, up;
     traceresult_s tr;
     int Dir;
     
     switch(iDir)
     {
          case 0: default: Dir = FORWARD; break;
          case 1: Dir = BACKWARD; break;
          case 2: Dir = LEFT; break;
          case 3: Dir = RIGHT; break;
          case 4: Dir = UP; break;
          case 5: Dir = DOWN; break;
     }
     
     vec from = player1->o;
     from.z -= (player1->eyeheight - 1.25f);
     end = from;
     makevec(&angles, player1->pitch, player1->yaw, player1->roll);
     angles.x=0;
     
     if (Dir & UP)
          angles.x = WrapXAngle(angles.x + 45.0f);
     else if (Dir & DOWN)
          angles.x = WrapXAngle(angles.x - 45.0f);
          
     if ((Dir & FORWARD) || (Dir & BACKWARD))
     {
          if (Dir & BACKWARD)
               angles.y = WrapYZAngle(angles.y + 180.0f);
          
          if (Dir & LEFT)
          {
               if (Dir & FORWARD)
                    angles.y = WrapYZAngle(angles.y - 45.0f);
               else
                    angles.y = WrapYZAngle(angles.y + 45.0f);
          }
          else if (Dir & RIGHT)
          {
               if (Dir & FORWARD)
                    angles.y = WrapYZAngle(angles.y + 45.0f);
               else
                    angles.y = WrapYZAngle(angles.y - 45.0f);
          }
     }
     else if (Dir & LEFT)
          angles.y = WrapYZAngle(angles.y - 90.0f);
     else if (Dir & RIGHT)
          angles.y = WrapYZAngle(angles.y + 90.0f);
     else if (Dir & UP)
          angles.x = WrapXAngle(angles.x + 90.0f);
     else if (Dir & DOWN)
          angles.x = WrapXAngle(angles.x - 90.0f);
          
     AnglesToVectors(angles, forward, right, up);
     
     vmul(forward, 20.0f);
     vadd(end, forward);
         
     TraceLine(from, end, player1, false, &tr);
     
     //debugbeam(from, tr.end);
     char sz[250];
     sprintf(sz, "dist: %f; hit: %d", GetDistance(from, tr.end), tr.collided);
     condebug(sz);
}

COMMAND(testvisible, ARG_1INT);

void mapsize(void)
{
     conoutf("ssize: %d", ssize);
}

COMMAND(mapsize, ARG_NONE);
               
#endif
