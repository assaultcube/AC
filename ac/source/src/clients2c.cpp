// client processing of the incoming network stream

#include "cube.h"

extern int clientnum;
extern bool c2sinit, senditemstoserver;
extern string clientpassword;

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

#define CN_CHECK if(!players.inrange(cn)) { conoutf("invalid client (msg %i)", type); return; };
//#define SENDER_CHECK if(sender<0) { conoutf("invalid sender (msg %i)", type); return; };

void parsepositions(ucharbuf &p)
{
    int cn = -1, type;
    playerent *d = NULL;
    while(p.remaining()) switch(type = getint(p))
    {
        case SV_POS:                        // position of another client
        {
            cn = getint(p);
            d = getclient(cn);
            if(!d) return;
            d->o.x   = getuint(p)/DMF;
            d->o.y   = getuint(p)/DMF;
            d->o.z   = getuint(p)/DMF;
            d->yaw   = getuint(p)/DAF;
            d->pitch = getint(p)/DAF;
            d->roll  = getint(p)/DAF;
            d->vel.x = getint(p)/DVF;
            d->vel.y = getint(p)/DVF;
            d->vel.z = getint(p)/DVF;
            int f = getint(p);
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
    bool mapchanged = false;
    bool c2si = false;
    bool gib=false;

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
			if(firstplayer && getclientmap()[0]) changemap(getclientmap()); // we are the first client on this server, set map
            break;
        };

        case SV_SOUND:
            playsound(getint(p), &d->o);
            break;

        case SV_TEXT:
            getstring(text, p);
            conoutf("%s:\f %s", d->name, &text); 
            break;

        case SV_MAPCHANGE:     
            getstring(text, p);
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
            s_sprintfd(nextmapalias)("nextmap_%s", getclientmap());
            char *map = getalias(nextmapalias);     // look up map in the cycle
            changemap(map ? map : getclientmap());
            break;
        };

        case SV_INITC2S:            // another client either connected or changed name/team
        {
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
                // Added by Rick: If we are the host("the bot owner"), tell the bots
                // to update their stats
                if (ishost())
                    BotManager.LetBotsUpdateStats();
                // End add by Rick                
            }; 
            s_strncpy(d->name, text, MAXNAMELEN+1);
            getstring(text, p);
            s_strncpy(d->team, text, MAXTEAMLEN+1);
            d->skin = getint(p);
            d->lifesequence = getint(p);
            c2si = true;
            break;
        };

        case SV_CDIS:
        {
            int cn = getint(p);
            if(!(d = getclient(cn))) break;
			if(d->name[0]) conoutf("player %s disconnected", d->name); 
            zapplayer(players[cn]);
            break;
        };

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
            d->lastaction = lastmillis;
            d->lastattackgun = gun;
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
            else
            {
                playerent *victim = getclient(target);
                playsound(S_PAIN1+rnd(5), &victim->o);
                victim->lastpain = lastmillis;
            };
			gib = false;
			break;
		}
        
		case SV_GIBDIED:
			gib = true;
        case SV_DIED:
        {
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
            gib = false;
            break;
        };
        
        

        case SV_FRAGS:
			CN_CHECK;
            players[cn]->frags = getint(p);
            break;

        case SV_RESUME:
        {
            int cn = getint(p), frags = getint(p), flags = getint(p);
            playerent *d = cn==clientnum ? player1 : getclient(cn);
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
			CN_CHECK;
            players[cn]->gunselect = getint(p);
            break;
        };
        
        case SV_SERVMSG:
            getstring(text, p);
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
                int actor_cn = getint(p);
                f.actor = actor_cn == getclientnum() ? player1 : getclient(actor_cn);
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
                    f.actor = returnerer_cn == getclientnum() ? player1 : getclient(returnerer_cn);
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
               
        /* demo recording compat */
        case SV_PING:
            getint(p);
            break;

        // End add

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
        case 1: parsemessages(-1, NULL, p); break;
        case 2:
            while(p.remaining())
            {
                int cn = p.get();
                playerent *d = getclient(cn);
                int len = p.get();
                len += p.get()<<8;
                ucharbuf q(&p.buf[p.len], min(len, p.maxlen-p.len));
                if(d) parsemessages(cn, d, q);
                p.len += min(len, p.maxlen-p.len);
            };
            break;
        case 3: parsemessages(-1, NULL, p); break;
        case 42: // player1 demo data only
            extern int democlientnum;
            parsemessages(democlientnum, getclient(democlientnum), p);
            break;
    };
};

