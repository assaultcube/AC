// client processing of the incoming network stream

#include "cube.h"

extern int clientnum;
extern bool c2sinit, senditemstoserver;
extern string toservermap;
extern string clientpassword;
extern char *toserverpwd;

void neterr(char *s)
{
    conoutf("illegal network message (%s)", s);
    disconnect();
};

void changemapserv(char *name, int mode)        // forced map change from the server
{
    gamemode = mode;
    load_world(name);
};

void changemap(char *name)                      // request map change, server may ignore
{
    strcpy_s(toservermap, name);
};

// Added by Rick
void botcommand(uchar *&p, char *text)
{
     int type = getint(p);
     switch(EBotCommands(type))
     {
          case COMMAND_ADDBOT:
               getint(p);
               getint(p);
               sgetstr();
               sgetstr();
               break;
          case COMMAND_KICKBOT:
               if (getint(p)==1) // Kick a specific bot
                    sgetstr();
               break;
          case COMMAND_BOTSKILL:
               getint(p);
               break;
     }
}
// End add


// update the position of other clients in the game in our world
// don't care if he's in the scenery or other players,
// just don't overlap with our client

void updatepos(dynent *d)
{
    const float r = player1->radius+d->radius;
    const float dx = player1->o.x-d->o.x;
    const float dy = player1->o.y-d->o.y;
    const float dz = player1->o.z-d->o.z;
    const float rz = player1->aboveeye+d->eyeheight;
    const float fx = (float)fabs(dx), fy = (float)fabs(dy), fz = (float)fabs(dz);
    if(fx<r && fy<r && fz<rz && d->state!=CS_DEAD)
    {
        if(fx<fy) d->o.y += dy<0 ? r-fy : -(r-fy);  // push aside
        else      d->o.x += dx<0 ? r-fx : -(r-fx);
    };
    int lagtime = lastmillis-d->lastupdate;
    if(lagtime)
    {
        d->plag = (d->plag*5+lagtime)/6;
        d->lastupdate = lastmillis;
    };
};

extern void trydisconnect();

#define CN_CHECK if(cn<0 || cn>=players.length()) { conoutf("invalid client (msg %i)", type); return; };
//#define SENDER_CHECK if(sender<0) { conoutf("invalid sender (msg %i)", type); return; };

void localservertoclient(uchar *buf, int len)   // processes any updates from the server
{
    if(ENET_NET_TO_HOST_16(*(ushort *)buf)!=len) neterr("packet length");
    incomingdemodata(buf, len);
    
    uchar *end = buf+len;
    uchar *p = buf+2;
    char text[MAXTRANS];
    int cn = -1, type;
    dynent *d = NULL;
    bool mapchanged = false;
    bool c2si = false, killedbybot=false;
	bool gib=false;

    while(p<end) switch(type = getint(p))
    {
        case SV_INITS2C:                    // welcome messsage from the server
        {
            cn = getint(p);
            int prot = getint(p);
            if(prot!=PROTOCOL_VERSION)
            {
                conoutf("you are using a different game protocol (you: %d, server: %d)", PROTOCOL_VERSION, prot);
                disconnect();
                return;
            };
            toservermap[0] = 0;
            clientnum = cn;                 // we are now fully connectedss
			bool firstplayer = !getint(p);
            if(getint(p) > 0)
			{
				conoutf("INFO: this server is password protected");
				toserverpwd = clientpassword;
			};
			int m = getint(p);
            if(m==1)
            {
                conoutf("server is FULL, disconnecting..");
            }
			else if(m==2)
			{
				conoutf("you are BANNED from this server");
			}
			else if(firstplayer) strcpy_s(toservermap, getclientmap()); // we are the first client on this server, set map
            break;
        };

        case SV_POS:                        // position of another client
        {
            cn = getint(p);
            d = getclient(cn);
            if(!d) return;
            c2si = false;
            d->o.x   = getint(p)/DMF;
            d->o.y   = getint(p)/DMF;
            d->o.z   = getint(p)/DMF;
            d->yaw   = getint(p)/DAF;
            d->pitch = getint(p)/DAF;
            d->roll  = getint(p)/DAF;
            d->vel.x = getint(p)/DVF;
            d->vel.y = getint(p)/DVF;
            d->vel.z = getint(p)/DVF;
            int f = getint(p);
            d->strafe = (f&3)==3 ? -1 : f&3;
            f >>= 2; 
            d->move = (f&3)==3 ? -1 : f&3;
            d->onfloor = (f>>2)&1;
            int state = f>>3;
            if(state==CS_DEAD && d->state!=CS_DEAD) d->lastaction = lastmillis;
            d->state = state;
            if(!demoplayback) updatepos(d);
            break;
        };

        case SV_SOUND:
            playsound(getint(p), &d->o);
            break;

        case SV_TEXT:
            sgetstr();
            conoutf("%s:\f %s", d->name, &text); 
            break;

        case SV_MAPCHANGE:     
            sgetstr();
            changemapserv(text, getint(p));
            mapchanged = true;
            break;
        
        case SV_ITEMLIST:
        {
            int n;
            if(mapchanged) { senditemstoserver = false; resetspawns(); };
            while((n = getint(p))!=-1) if(mapchanged) setspawn(n, true);
            break;
        };

        case SV_MAPRELOAD:          // server requests next map
        {
            getint(p);
            sprintf_sd(nextmapalias)("nextmap_%s", getclientmap());
            char *map = getalias(nextmapalias);     // look up map in the cycle
            changemap(map ? map : getclientmap());
            break;
        };

        case SV_INITC2S:            // another client either connected or changed name/team
        {
            sgetstr();
            if(d->name[0])          // already connected
            {
                if(strcmp(d->name, text))
                    conoutf("%s is now known as %s", d->name, &text);
            }
            else                    // new client
            {
                c2sinit = false;    // send new players my info again 
                conoutf("connected: %s", &text);
                gun_changed = true;
                // Added by Rick: If we are the host("the bot owner"), tell the bots
                // to update their stats
                if (ishost())
                    BotManager.LetBotsUpdateStats();
                // End add by Rick                
            }; 
            strcpy_s(d->name, text);
            sgetstr();
            strcpy_s(d->team, text);
            d->skin = getint(p);
            d->lifesequence = getint(p);
            c2si = true;
            break;
        };

        case SV_CDIS:
            cn = getint(p);
            if(!(d = getclient(cn))) break;
			conoutf("player %s disconnected", (d->name[0] ? d->name : "[incompatible client]")); 
            zapdynent(players[cn]);
            break;

        case SV_SHOT:
        {
            int gun = getint(p);
            vec s, e;
            s.x = getint(p)/DMF;
            s.y = getint(p)/DMF;
            s.z = getint(p)/DMF;
            e.x = getint(p)/DMF;
            e.y = getint(p)/DMF;
            e.z = getint(p)/DMF;
            if(gun==GUN_SHOTGUN) createrays(s, e);
            shootv(gun, s, e, d, false, getint(p));
            break;
        };
		case SV_GIBDAMAGE:
			gib = true;
        case SV_DAMAGE:             
		{   
			int target = getint(p);
            int damage = getint(p);
            int ls = getint(p);
			if(target==clientnum) { if(ls==player1->lifesequence) selfdamage(damage, cn, d, gib); }
            else playsound(S_PAIN1+rnd(5), &getclient(target)->o);
			gib = false;
			break;
		}
        
        case SV_BOT2CLIENTDMG:
        {
        
            int target = getint(p);
            int damage = getint(p);
            int ls = getint(p);
            int damager = getint(p);
            
            if(target==clientnum)
            {
                 dynent *a = getbot(damager);
                 if(ls==player1->lifesequence)
                      selfdamage(damage, cn, a);
            }            
            else playsound(S_PAIN1+rnd(5), &getclient(target)->o);
            break;
        }


        case SV_DIEDBYBOT:
            killedbybot = true;
		case SV_GIBDIED:
			if(type!=SV_DIEDBYBOT) gib = true; // uargl.. fixme
        case SV_DIED:
        {
            int actor = getint(p);

            if(actor==cn && !killedbybot)
            {
                conoutf("%s suicided", d->name);
            }
            else if(actor==clientnum && !killedbybot)
            {
                int frags;
                if(isteam(player1->team, d->team))
                {
                    frags = -1;
                    conoutf("you fragged a teammate (%s)", d->name);
                }
                else
                {
                    frags = 1;
                    conoutf("you fragged %s", d->name);
                };
                addmsg(1, 2, SV_FRAGS, player1->frags += frags);
				if(gib) addgib(d);
            }
            else
            {
				dynent *a = killedbybot ? getbot(actor) : getclient(actor);
                if(a)
                {
                    if(isteam(a->team, d->name))
                    {
                        conoutf("%s fragged his teammate (%s)", a->name, d->name);
                    }
                    else
                    {
                        conoutf("%s fragged %s", a->name, d->name);
                    };
					if(gib) addgib(d);
                };
            };
            playsound(S_DIE1+rnd(2), &d->o);
            if(!c2si) d->lifesequence++; 
            killedbybot = gib = false;
            break;
        };
        
        

        case SV_FRAGS:
			CN_CHECK;
            players[cn]->frags = getint(p);
            break;

        case SV_ITEMPICKUP:
        case SV_BOTITEMPICKUP:
            setspawn(getint(p), false);
            getint(p);
            break;

        case SV_ITEMSPAWN:
        {
            uint i = getint(p);
            setspawn(i, true);
            break;
        };

        case SV_ITEMACC:            // server acknowledges that I picked up this item
            realpickup(getint(p), player1);
            break;

        case SV_EDITH:              // coop editing messages, should be extended to include all possible editing ops
        case SV_EDITT:
        case SV_EDITS:
        case SV_EDITD:
        case SV_EDITE:
        {
            int x  = getint(p);
            int y  = getint(p);
            int xs = getint(p);
            int ys = getint(p);
            int v  = getint(p);
            block b = { x, y, xs, ys };
            switch(type)
            {
                case SV_EDITH: editheightxy(v!=0, getint(p), b); break;
                case SV_EDITT: edittexxy(v, getint(p), b); break;
                case SV_EDITS: edittypexy(v, b); break;
                case SV_EDITD: setvdeltaxy(v, b); break;
                case SV_EDITE: editequalisexy(v!=0, b); break;
            };
            break;
        };

        case SV_EDITENT:            // coop edit of ent
        {
            uint i = getint(p);
            while((uint)ents.length()<=i) ents.add().type = NOTUSED;
            int to = ents[i].type;
            ents[i].type = getint(p);
            ents[i].x = getint(p);
            ents[i].y = getint(p);
            ents[i].z = getint(p);
            ents[i].attr1 = getint(p);
            ents[i].attr2 = getint(p);
            ents[i].attr3 = getint(p);
            ents[i].attr4 = getint(p);
            ents[i].spawned = false;
            if(ents[i].type==LIGHT || to==LIGHT) calclight();
            break;
        };

        case SV_PING:
            getint(p);
            break;

        case SV_PONG: 
            addmsg(0, 2, SV_CLIENTPING, player1->ping = (player1->ping*5+lastmillis-getint(p))/6);
            break;

        case SV_CLIENTPING:
			CN_CHECK;
            players[cn]->ping = getint(p);
            break;

        case SV_GAMEMODE:
            nextmode = getint(p);
            break;

        case SV_TIMEUP:
            timeupdate(getint(p));
            break;

        case SV_RECVMAP:
        {
            sgetstr();
            conoutf("received map \"%s\" from server, reloading..", &text);
            int mapsize = getint(p);
            writemap(text, mapsize, p);
            p += mapsize;
            changemapserv(text, gamemode);
            break;
        };
        
        case SV_WEAPCHANGE:
        {
			CN_CHECK;
            players[cn]->gunselect = getint(p);
            break;
        };
        
        case SV_SERVMSG:
            sgetstr();
            conoutf("%s", text);
            break;

        // EDIT: AH
        case SV_FLAGINFO:
        {
            int flag = getint(p);
            if(flag<0||flag>1) return;
            flaginfo &f = flaginfos[flag];
            f.state = getint(p);
            int action = getint(p);
            if(f.state==CTFF_STOLEN) 
            { 
                int thief_cn = getint(p);
                f.thief = thief_cn == getclientnum() ? player1 : getclient(thief_cn);
                f.flag->spawned = false;
            }
            else if(f.state==CTFF_DROPPED)
            {
                f.flag->x = (ushort) (getint(p)/DMF);
                f.flag->y = (ushort) (getint(p)/DMF);
                f.flag->z = (ushort) (getint(p)/DMF);
                f.flag->z -= 4;
                float floor = (float)S(f.flag->x, f.flag->y)->floor;
                if(f.flag->z > hdr.waterlevel) // above water
                {
                    if(floor < hdr.waterlevel)
                        f.flag->z = hdr.waterlevel; // avoid dropping into water
                    else
                        f.flag->z = (short)floor;
                };  
                f.flag->spawned = true;
            }
            else if(f.state==CTFF_INBASE)
            {
                if(action==SV_FLAGRETURN)
                {
                    int returnerer_cn = getint(p);
                    f.thief = returnerer_cn == getclientnum() ? player1 : getclient(returnerer_cn);
                }
                f.flag->x = (ushort) f.originalpos.x;
                f.flag->y = (ushort) f.originalpos.y;
                f.flag->z = (ushort) f.originalpos.z;
                f.flag->spawned = true;
            };
            flagaction(flag, action);
            break;
        };
        
        case SV_FLAGS:
        {
			CN_CHECK;
            players[cn]->flagscore = getint(p);
            break;
        };
        
        case SV_FLAGPICKUP:
        {
            int flag = getint(p);
            if(flag<0||flag>1) return;
            flaginfos[flag].state=CTFF_STOLEN;
            break;
        };
        
        case SV_FLAGDROP:
        case SV_FLAGRETURN:
        case SV_FLAGSCORE:
        {
            getint(p);
            break;
        };

        // Added by Rick: Bot specific messages
        case SV_BOTSOUND:
        {
             int Sound = getint(p);
             dynent *pBot = getbot(getint(p));
            
             if (pBot)
                  playsound(Sound, &pBot->o);
                 
             break;
        }
        
        case SV_ADDBOT: // Bot joined the game
        {
            dynent *b = getbot(getint(p));
            if (!b) break;

            sgetstr();
            if(b->name[0])          // already connected
            {
                if(strcmp(b->name, text))
                    conoutf("%s is now known as %s", b->name, &text);
            }
            else                    // new client
            {
                //c2sinit = false;    // send new players my info again 
                conoutf("connected: %s", &text);
            }; 
            strcpy_s(b->name, text);
            sgetstr();
            strcpy_s(b->team, text);
            b->lifesequence = getint(p);
            b->bIsBot = true;
            b->pBot = NULL;
            break;
        };
        
        case SV_BOTDIS:
        {
            int n = getint(p);
            dynent *b = getbot(n);

            if (!b)
               break;

            conoutf("bot %s disconnected", b->name[0] ? b->name : "[incompatible client]");
            delete b->pBot;
            zapdynent(bots[n]);
            bots.remove(n);
            break;
        }
        
        case SV_CLIENT2BOTDMG:
        case SV_BOT2BOTDMG:
        {
            int target = getint(p);
            int damage = getint(p);
            int damager = getint(p);
            dynent *b = getbot(target);

            dynent *a;
            if (damager == -1)
            {
               // HACK! if the local client who sended the message is the damager, its -1
               a = d;
            }   
            else if (type == SV_CLIENT2BOTDMG)
            {
                if (damager == clientnum) a = player1;
                else a = getclient(damager);
            }
            else
               a = getbot(damager);

            if (b && a)
            {
                // Do we know the bot info? if so we are the host...
                if (b->pBot) b->pBot->BotPain(damage, a);
                playsound(S_PAIN1+rnd(5), &b->o);
            }
            break;
        }
        
     case SV_BOTDIED:
     {
            int b = getint(p);
            int killer = getint(p);
            bool KilledByABot = getint(p) > 0; //fixmebot
            dynent *bot = getbot(b);

            if((b==killer) && KilledByABot)
            {
                conoutf("%s suicided", bot->name);
            }
            else if((killer==clientnum) && !KilledByABot)
            {
                int frags;
                if(isteam(player1->team, bot->team))
                {
                    frags = -1;
                    conoutf("you fragged a teammate (%s)", bot->name);
                }
                else
                {
                    frags = 1;
                    conoutf("you fragged %s", bot->name);
                };
                addmsg(1, 2, SV_FRAGS, player1->frags += frags);
				if(player1->gunselect==GUN_KNIFE) 
					addgib(bot);
            } 
            else
            {
                dynent *k;
                if (KilledByABot)
                     k = getbot(killer);
                else if (killer == -1)
                    // if killer = -1, 'a player1' sended the message(hack)
                     k = d;
                else
                     k = getclient(killer);
                     
                if(bot && k)
                {
                    if(isteam(bot->team, k->name))
                    {
                        conoutf("%s fragged his teammate (%s)", bot->name, k->name);
                    }
                    else
                    {
                        conoutf("%s fragged %s", bot->name, k->name);
                    }
					if(k->gunselect==GUN_KNIFE) addgib(bot);
                }
            }
            playsound(S_DIE1+rnd(2), &bot->o);
            bot->lifesequence++;
            break;
        };

        case SV_BOTFRAGS:
        {
            dynent *b = getbot(getint(p));
            if (b) b->frags = getint(p);
            break;
        }
         
        case SV_BOTUPDATE:
        {
            int n = getint(p);
            dynent *b = getbot(n);
            if(!b) return;
            b->o.x   = getint(p)/DMF;
            b->o.y   = getint(p)/DMF;
            b->o.z   = getint(p)/DMF;
            b->yaw   = getint(p)/DAF;
            b->pitch = getint(p)/DAF;
            b->roll  = getint(p)/DAF;
            b->vel.x = getint(p)/DVF;
            b->vel.y = getint(p)/DVF;
            b->vel.z = getint(p)/DVF;
            int f = getint(p);
            b->strafe = (f&3)==3 ? -1 : f&3;
            f >>= 2; 
            b->move = (f&3)==3 ? -1 : f&3;
            b->onfloor = (f>>2)&1;
            int state = f>>3;
            if(state==CS_DEAD && b->state!=CS_DEAD) b->lastaction = lastmillis;
            b->state = state;
    
            if (!b->bIsBot)
               b->bIsBot = true;

            if(!demoplayback) updatepos(b);
            break;
        }
        case SV_BOTCOMMAND:
            botcommand(p, text);
            break;

		case SV_MASTERCMD:
		{
			int cmd = getint(p), arg1 = getint(p);
			dynent *a = getclient(arg1);
			if(a && d) conoutf("player %s has been %s by %s", a->name, cmd == MCMD_KICK ? "kicked" : "banned", d->name);
			break;
		};
               
        // End add

        default:
            neterr("type");
            return;
    };
};
