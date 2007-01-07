// client processing of the incoming network stream

#include "cube.h"

extern int clientnum;
extern bool c2sinit, senditemstoserver;
extern string clientpassword;

void neterr(char *s)
{
    conoutf("\f3illegal network message (%s)", s);
    disconnect();
};

void changemapserv(char *name, int mode)        // forced map change from the server
{
    gamemode = mode;
    load_world(name);
};

void changemap(char *name)                      // request map change, server may ignore
{
    addmsg(SV_MAPCHANGE, "rsi", name, nextmode);
};

// update the position of other clients in the game in our world
// don't care if he's in the scenery or other players,
// just don't overlap with our client

void updatepos(playerent *d)
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

void parsepositions(ucharbuf &p)
{
    int type;
    while(p.remaining()) switch(type = getint(p))
    {
        case SV_POS:                        // position of another client
        {
            int cn = getint(p);
            vec o, vel;
            float yaw, pitch, roll;
            o.x   = getuint(p)/DMF;
            o.y   = getuint(p)/DMF;
            o.z   = getuint(p)/DMF;
            yaw   = getuint(p)/DAF;
            pitch = getint(p)/DAF;
            roll  = getint(p)/DAF;
            vel.x = getint(p)/DVF;
            vel.y = getint(p)/DVF;
            vel.z = getint(p)/DVF;
            int f = getint(p);
            playerent *d = getclient(cn);
            if(!d) continue;
            d->o = o;
            d->yaw = yaw;
            d->pitch = pitch;
            d->roll = roll;
            d->vel = vel;
            d->strafe = (f&3)==3 ? -1 : f&3;
            f >>= 2;  
            d->move = (f&3)==3 ? -1 : f&3;
            f >>= 2;
            d->onfloor = f&1;
            f >>= 1;
            int state = f&7; 
            if(state==CS_DEAD && d->state!=CS_DEAD) d->lastaction = lastmillis;
            d->state = state; 
            f >>= 3;
            d->onladder = f&1;
            if(!demoplayback) updatepos(d);
            break;
        };

        default:
            neterr("type");
            return;
    };
};
    
void parsemessages(int cn, playerent *d, ucharbuf &p)
{
    static char text[MAXTRANS];
    int type;
    bool mapchanged = false, c2si = false, gib = false, joining = false;

    while(p.remaining()) switch(type = getint(p))
    {
        case SV_INITS2C:                    // welcome messsage from the server
        {
            int mycn = getint(p), prot = getint(p);
            if(prot!=PROTOCOL_VERSION)
            {
                conoutf("you are using a different game protocol (you: %d, server: %d)", PROTOCOL_VERSION, prot);
                disconnect();
                return;
            };
            clientnum = mycn;                 // we are now fully connected
			bool firstplayer = !getint(p);
            if(getint(p) > 0) conoutf("INFO: this server is password protected");
            if(!firstplayer) joining = true;
			else if(getclientmap()[0]) changemap(getclientmap()); // we are the first client on this server, set map
            break;
        };

        case SV_CLIENT:
        {
            int cn = getint(p), len = getuint(p);
            ucharbuf q = p.subbuf(len);
            parsemessages(cn, getclient(cn), q);
            break;
        };

        case SV_SOUND:
            playsound(getint(p), d ? &d->o : NULL);
            break;

        case SV_TEXT:
            if(!d) return;
            getstring(text, p);
            conoutf("%s:\f0 %s", d->name, &text); 
            break;

        case SV_MAPCHANGE:     
            getstring(text, p);
            changemapserv(text, getint(p));
            if(joining && m_arena) 
            {
                /* TESTME
				player1->state = CS_DEAD;
                showscores(true);*/
				playerdeath(player1);
            };
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
            s_sprintfd(nextmapalias)("nextmap_%s", getclientmap());
            char *map = getalias(nextmapalias);     // look up map in the cycle
            changemap(map ? map : getclientmap());
            break;
        };

        case SV_INITC2S:            // another client either connected or changed name/team
        {
            d = newclient(cn);
            getstring(text, p);
            if(!text[0]) s_strcpy(text, "unarmed");
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
            }; 
            s_strncpy(d->name, text, MAXNAMELEN+1);
            getstring(text, p);
            s_strncpy(d->team, text, MAXTEAMLEN+1);
			setskin(d, getint(p));
            d->lifesequence = getint(p);
            c2si = true;
            break;
        };

        case SV_CDIS:
        {
            int cn = getint(p);
            playerent *d = getclient(cn);
            if(!d) break;
			if(d->name[0]) conoutf("player %s disconnected", d->name); 
            zapplayer(players[cn]);
            break;
        };

        case SV_SHOT:
        {
            if(!d) return;
            int gun = getint(p);
            vec s, e;
            s.x = getint(p)/DMF;
            s.y = getint(p)/DMF;
            s.z = getint(p)/DMF;
            e.x = getint(p)/DMF;
            e.y = getint(p)/DMF;
            e.z = getint(p)/DMF;
            if(gun==GUN_SHOTGUN) createrays(s, e);
            d->lastaction = lastmillis;
            d->lastattackgun = gun;
            shootv(gun, s, e, d, false, getint(p));
            break;
        };
		case SV_GIBDAMAGE:
			gib = true;
        case SV_DAMAGE:             
		{   
            if(!d) return;
			int target = getint(p);
            int damage = getint(p);
            int ls = getint(p);
			if(target==clientnum) { if(ls==player1->lifesequence) selfdamage(damage, cn, d, gib); }
            else
            {
                playerent *victim = getclient(target);
                if(victim)
                {
                    playsound(S_PAIN1+rnd(5), &victim->o);
                    victim->lastpain = lastmillis;
                };
            };
			gib = false;
			break;
		};
        
		case SV_GIBDIED:
			gib = true;
        case SV_DIED:
        {
            if(!d) return;
            int actor = getint(p);

            if(actor==cn)
            {
                conoutf("%s suicided", d->name);
            }
            else if(actor==clientnum)
            {
                int frags;
                if(isteam(player1->team, d->team))
                {
                    frags = -1;
                    conoutf("you fragged a teammate (%s)", d->name);
					if(m_ctf)
					{
						flaginfo &flag = flaginfos[team_opposite(team_int(d->team))];
						if(flag.state == CTFF_STOLEN && flag.actor == d) playerdeath(player1); // punish for ctf TK
					};
                }
                else
                {
                    frags = 1;
                    conoutf("you fragged %s", d->name);
                };
                addmsg(SV_FRAGS, "ri", player1->frags += frags);
				if(gib) addgib(d);
            }
            else
            {
				playerent *a = getclient(actor);
                if(a)
                {
                    if(isteam(a->team, d->team))
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
            gib = false;
            break;
        };
        
        case SV_FRAGS:
            if(!d) return;
            d->frags = getint(p);
            break;

        case SV_RESUME:
        {
            int cn = getint(p), frags = getint(p), flags = getint(p);
            playerent *d = cn==clientnum ? player1 : newclient(cn);
            if(d)
            {
                d->frags = frags;
                d->flagscore = flags;
            };
            break;
        };

        case SV_ITEMPICKUP:
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

        case SV_PONG: 
            addmsg(SV_CLIENTPING, "i", player1->ping = (player1->ping*5+lastmillis-getint(p))/6);
            break;

        case SV_CLIENTPING:
            if(!d) return;
            d->ping = getint(p);
            break;

        case SV_GAMEMODE:
            nextmode = getint(p);
            break;

        case SV_TIMEUP:
            timeupdate(getint(p));
            break;

        case SV_RECVMAP:
        {
            getstring(text, p);
            conoutf("received map \"%s\" from server, reloading..", &text);
            int mapsize = getint(p);
            if(p.remaining() < mapsize)
            {
                p.forceoverread();
                break;
            };
            writemap(text, mapsize, &p.buf[p.len]);
            p.len += mapsize;
            changemapserv(text, gamemode);
            break;
        };
        
        case SV_WEAPCHANGE:
        {
            if(!d) return;
            d->gunselect = getint(p);
            break;
        };
        
        case SV_SERVMSG:
            getstring(text, p);
            conoutf("%s", text);
            break;

        case SV_FLAGINFO:
        {
            int flag = getint(p);
            if(flag<0||flag>1) return;
            flaginfo &f = flaginfos[flag];
            f.state = getint(p);
            int action = getint(p);

			switch(f.state)
			{
				case CTFF_STOLEN:
				{ 
					int actor = getint(p);
					flagstolen(flag, action, actor == getclientnum() ? player1 : getclient(actor));
					break;
				};
				case CTFF_DROPPED:
				{
					short x = (ushort) (getint(p)/DMF);
					short y = (ushort) (getint(p)/DMF);
					short z = (ushort) (getint(p)/DMF);
					flagdropped(flag, action, x, y, z);
					break;
				};
				case CTFF_INBASE:
				{
					playerent *actor = NULL;
					if(action == SV_FLAGRETURN)
					{
						int a = getint(p);
						actor = a == getclientnum() ? player1 : getclient(a);
					};
					flaginbase(flag, action, actor);
					break;
				};
			};
            break;
        };
        
        case SV_FLAGS:
        {
			if(!d) return;
            d->flagscore = getint(p);
            break;
        };

		case SV_MASTERINFO:
		{
			loopv(players) { players[i]->ismaster = false; }
			int m = getint(p);
			if(m != -1)
			{	
				playerent *pl = (m == getclientnum() ? player1 : getclient(m));
				if(pl)
				{
					pl->ismaster = true;
					conoutf("%s claimed master status", pl == player1 ? "you" : pl->name);
				};
			};
			break;
		};

		case SV_MASTERCMD:
		{
			int cmd = getint(p), arg = getint(p);
			switch(cmd)
			{
				case MCMD_KICK:
				case MCMD_BAN:
				{
					playerent *pl = (arg == getclientnum() ? NULL : getclient(arg));
					if(pl) conoutf("%s has been %s", pl->name, cmd == MCMD_KICK ? "kicked" : "banned");
					break;
				}
				case MCMD_REMBANS:
					conoutf("bans removed");
					break;
					
				case MCMD_MASTERMODE:
					conoutf("mastermode set to %i", arg);
					break;
			};
			break;
		};

		case SV_FORCETEAM:
		{
			changeteam(getint(p));
			break;
		};

		case SV_AUTOTEAM:
		{
			conoutf("autoteam is %s", (autoteambalance = getint(p) == 1) ? "enabled" : "disabled");
			break;
		};
               
        /* demo recording compat */
        case SV_PING:
            getint(p);
            break;

        default:
            neterr("type");
            return;
    };
};

void localservertoclient(int chan, uchar *buf, int len)   // processes any updates from the server
{
    incomingdemodata(chan, buf, len);

    ucharbuf p(buf, len);

    switch(chan)
    {
        case 0: parsepositions(p); break;
        case 1: 
        case 2: parsemessages(-1, NULL, p); break;
        case 42: // player1 demo data only
            extern int democlientnum;
            parsemessages(democlientnum, getclient(democlientnum), p);
            break;
    };
};
