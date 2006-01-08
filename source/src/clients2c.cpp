// client processing of the incoming network stream

#include "cube.h"

extern int clientnum;
extern bool c2sinit, senditemstoserver;
extern string toservermap;
extern string clientpassword;

void neterr(char *s)
{
    conoutf("illegal network message (%s)", (int)s);
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
            clientnum = cn;                 // we are now fully connected
            if(!getint(p)) strcpy_s(toservermap, getclientmap());   // we are the first client on this server, set map
            sgetstr();
            if(text[0] && strcmp(text, clientpassword))
            {
                conoutf("you need to set the correct password to join this server!");
                disconnect();
                return;
            };
            break;
        };

        case SV_POS:                        // position of another client
        {
            cn = getint(p);
            d = getclient(cn);
            if(!d) return;
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
            conoutf("%s:\f %s", (int)d->name, (int)&text); 
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
                    conoutf("%s is now known as %s", (int)d->name, (int)&text);
            }
            else                    // new client
            {
                c2sinit = false;    // send new players my info again 
                conoutf("connected: %s", (int)&text);
            }; 
            strcpy_s(d->name, text);
            sgetstr();
            strcpy_s(d->team, text);
            d->lifesequence = getint(p);
            break;
        };

        case SV_CDIS:
            cn = getint(p);
            if(!(d = getclient(cn))) break;
			conoutf("player %s disconnected", (int)(d->name[0] ? d->name : "[incompatible client]")); 
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
            int nadesec = getint(p);
            printf("nadesec: %i\n", nadesec); 
            shootv(gun, s, e, d, nadesec);
            break;
        };

        case SV_DAMAGE:             
        {
            int target = getint(p);
            int damage = getint(p);
            int ls = getint(p);
            if(target==clientnum) { if(ls==player1->lifesequence) selfdamage(damage, cn, d); }
            else playsound(S_PAIN1+rnd(5), &getclient(target)->o);
            break;
        };

        case SV_DIED:
        {
            int actor = getint(p);
            if(actor==cn)
            {
                conoutf("%s suicided", (int)d->name);
            }
            else if(actor==clientnum)
            {
                int frags;
                if(isteam(player1->team, d->team))
                {
                    frags = -1;
                    conoutf("you fragged a teammate (%s)", (int)d->name);
                }
                else
                {
                    frags = 1;
                    conoutf("you fragged %s", (int)d->name);
                };
                addmsg(1, 2, SV_FRAGS, player1->frags += frags);
            }
            else
            {
                dynent *a = getclient(actor);
                if(a)
                {
                    if(isteam(a->team, d->name))
                    {
                        conoutf("%s fragged his teammate (%s)", (int)a->name, (int)d->name);
                    }
                    else
                    {
                        conoutf("%s fragged %s", (int)a->name, (int)d->name);
                    };
                };
            };
            playsound(S_DIE1+rnd(2), &d->o);
            d->lifesequence++;
            break;
        };

        case SV_FRAGS:
            players[cn]->frags = getint(p);
            break;

        case SV_ITEMPICKUP:
            setspawn(getint(p), false);
            getint(p);
            break;

        case SV_ITEMSPAWN:
        {
            int i = getint(p);
            setspawn(i, true);
            if(i>=ents.length()) break;
            vec v = { ents[i].x, ents[i].y, ents[i].z };
            playsound(S_ITEMSPAWN, &v); 
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
            int i = getint(p);
            while(ents.length()<=i) ents.add().type = NOTUSED;
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
            conoutf("received map \"%s\" from server, reloading..", (int)&text);
            int mapsize = getint(p);
            writemap(text, mapsize, p);
            p += mapsize;
            changemapserv(text, gamemode);
            break;
        };
        
        case SV_SERVMSG:
            sgetstr();
            conoutf("%s", (int)text);
            break;

        case SV_EXT:        // so we can messages without breaking previous clients/servers, if necessary
        {
            for(int n = getint(p); n; n--) getint(p);
            break;
        };

        default:
            neterr("type");
            return;
    };
};
