// client processing of the incoming network stream

#include "cube.h"
#include "bot/bot.h"

VARP(networkdebug, 0, 0, 1);
#define DEBUGCOND (networkdebug==1)

extern bool watchingdemo;
extern string clientpassword;

packetqueue pktlogger;

void neterr(const char *s)
{
    conoutf("\f3illegal network message (%s)", s);

    // might indicate a client/server communication bug, create error report
    pktlogger.flushtolog("packetlog.txt");
    conoutf("\f3wrote a network error report to packetlog.txt, please post this file to the bugtracker now!");

    disconnect();
}

VARP(autogetmap, 0, 1, 1); // only if the client doesn't have that map
VARP(autogetnewmaprevisions, 0, 1, 1);

bool localwrongmap = false;
int MA = 0, Hhits = 0; // flowtron: moved here
bool changemapserv(char *name, int mode, int download, int revision)        // forced map change from the server
{
    MA = Hhits = 0; // reset for checkarea()
    gamemode = mode;
    if(m_demo) return true;
    if(m_coop)
    {
        if(!name[0] || !load_world(name)) empty_world(0, true);
        return true;
    }
    else if(player1->state==CS_EDITING) { /*conoutf("SANITY drop from EDITING");*/ toggleedit(true); } // fix stuck-in-editmode bug
    bool loaded = load_world(name);
    if(download > 0)
    {
        bool revmatch = hdr.maprevision == revision || revision == 0;
        if(watchingdemo)
        {
            if(!revmatch) conoutf(_("%c3demo was recorded on map revision %d, you have map revision %d"), CC, revision, hdr.maprevision);
        }
        else
        {
            if(securemapcheck(name, false)) return true;
            bool sizematch = maploaded == download || download < 10;
            if(loaded && sizematch && revmatch) return true;
            bool getnewrev = autogetnewmaprevisions && revision > hdr.maprevision;
            if(autogetmap || getnewrev)
            {
                if(!loaded || getnewrev) getmap(); // no need to ask
                else
                {
                    defformatstring(msg)("map '%s' revision: local %d, provided by server %d", name, hdr.maprevision, revision);
                    alias("__getmaprevisions", msg);
                    showmenu("getmap");
                }
            }
            else
            {
                if(!loaded || download < 10) conoutf(_("\"getmap\" to download the current map from the server"));
                else conoutf(_("\"getmap\" to download a %s version of the current map from the server"),
                         revision == 0 ? _("different") : (revision > hdr.maprevision ? _("newer") : _("older")));
            }
        }
    }
    else return true;
    return false;
}

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
    }
}

void updatelagtime(playerent *d)
{
    int lagtime = totalmillis-d->lastupdate;
    if(lagtime)
    {
        if(d->state!=CS_SPAWNING && d->lastupdate) d->plag = (d->plag*5+lagtime)/6;
        d->lastupdate = totalmillis;
    }
}

extern void trydisconnect();

VARP(maxrollremote, 0, 0, 20); // bound remote "roll" values by our maxroll?!

void parsepositions(ucharbuf &p)
{
    int type;
    while(p.remaining()) switch(type = getint(p))
    {
        case SV_POS:                        // position of another client
        case SV_POSC:
        {
            int cn, f, g;
            vec o, vel;
            float yaw, pitch, roll = 0;
            bool scoping;//, shoot;
            if(type == SV_POSC)
            {
                bitbuf<ucharbuf> q(p);
                cn = q.getbits(5);
                int usefactor = q.getbits(2) + 7;
                o.x = q.getbits(usefactor + 4) / DMF;
                o.y = q.getbits(usefactor + 4) / DMF;
                yaw = q.getbits(9) * 360.0f / 512;
                pitch = (q.getbits(8) - 128) * 90.0f / 127;
                roll = !q.getbits(1) ? (q.getbits(6) - 32) * 20.0f / 31 : 0.0f;
                if(!q.getbits(1))
                {
                    vel.x = (q.getbits(4) - 8) / DVELF;
                    vel.y = (q.getbits(4) - 8) / DVELF;
                    vel.z = (q.getbits(4) - 8) / DVELF;
                }
                else vel.x = vel.y = vel.z = 0.0f;
                f = q.getbits(8);
                int negz = q.getbits(1);
                int full = q.getbits(1);
                int s = q.rembits();
                if(s < 3) s += 8;
                if(full) s = 11;
                int z = q.getbits(s);
                if(negz) z = -z;
                o.z = z / DMF;
                scoping = ( q.getbits(1) ? true : false );
                q.getbits(1);//shoot = ( q.getbits(1) ? true : false );
            }
            else
            {
                cn = getint(p);
                o.x   = getuint(p)/DMF;
                o.y   = getuint(p)/DMF;
                o.z   = getuint(p)/DMF;
                yaw   = (float)getuint(p);
                pitch = (float)getint(p);
                g = getuint(p);
                if ((g>>3) & 1) roll  = (float)(getint(p)*20.0f/125.0f);
                if (g & 1) vel.x = getint(p)/DVELF; else vel.x = 0;
                if ((g>>1) & 1) vel.y = getint(p)/DVELF; else vel.y = 0;
                if ((g>>2) & 1) vel.z = getint(p)/DVELF; else vel.z = 0;
                scoping = ( (g>>4) & 1 ? true : false );
                //shoot = ( (g>>5) & 1 ? true : false ); // we are not using this yet
                f = getuint(p);
            }
            int seqcolor = (f>>6)&1;
            playerent *d = getclient(cn);
            if(!d || seqcolor!=(d->lifesequence&1)) continue;
            vec oldpos(d->o);
            float oldyaw = d->yaw, oldpitch = d->pitch;
            loopi(3)
            {
                float dr = o.v[i] - d->o.v[i] + ( i == 2 ? d->eyeheight : 0);
                if ( !dr ) d->vel.v[i] = 0.0f;
                else if ( d->vel.v[i] ) d->vel.v[i] = dr * 0.05f + d->vel.v[i] * 0.95f;
                d->vel.v[i] += vel.v[i];
                if ( i==2 && d->onfloor && d->vel.v[i] < 0.0f ) d->vel.v[i] = 0.0f;
            }
            d->o = o;
            d->o.z += d->eyeheight;
            d->yaw = yaw;
            d->pitch = pitch;
            if(d->weaponsel->type == GUN_SNIPER)
            {
                sniperrifle *sr = (sniperrifle *)d->weaponsel;
                sr->scoped = d->scoping = scoping;
            }
            d->roll = roll;
            d->strafe = (f&3)==3 ? -1 : f&3;
            f >>= 2;
            d->move = (f&3)==3 ? -1 : f&3;
            f >>= 2;
            d->onfloor = f&1;
            f >>= 1;
            d->onladder = f&1;
            f >>= 2;
            d->last_pos = totalmillis;
            updatecrouch(d, f&1);
            updatepos(d);
            updatelagtime(d);
            extern int smoothmove, smoothdist;
            if(d->state==CS_DEAD)
            {
                d->resetinterp();
                d->smoothmillis = 0;
            }
            else if(smoothmove && d->smoothmillis>=0 && oldpos.dist(d->o) < smoothdist)
            {
                d->newpos = d->o;
                d->newpos.z -= d->eyeheight;
                d->newyaw = d->yaw;
                d->newpitch = d->pitch;
                d->o = oldpos;
                d->yaw = oldyaw;
                d->pitch = oldpitch;
                oldpos.z -= d->eyeheight;
                (d->deltapos = oldpos).sub(d->newpos);
                d->deltayaw = oldyaw - d->newyaw;
                if(d->deltayaw > 180) d->deltayaw -= 360;
                else if(d->deltayaw < -180) d->deltayaw += 360;
                d->deltapitch = oldpitch - d->newpitch;
                d->smoothmillis = lastmillis;
            }
            else d->smoothmillis = 0;
            if(d->state==CS_LAGGED || d->state==CS_SPAWNING) d->state = CS_ALIVE;
            // when playing a demo spectate first player we know about
            if(player1->isspectating() && player1->spectatemode==SM_NONE) togglespect();
            extern void clamproll(physent *pl);
            if(maxrollremote) clamproll((physent *) d);
            break;
        }

        default:
            neterr("type");
            return;
    }
}

extern int checkarea(int maplayout_factor, char *maplayout);
char *mlayout = NULL;
int Mv = 0, Ma = 0, F2F = 1000 * MINFF; // moved up:, MA = 0;
float Mh = 0;
extern int connected;
extern int lastpm;
extern bool noflags;
bool item_fail = false;
int map_quality = MAP_IS_EDITABLE;

/// TODO: many functions and variables are redundant between client and server... someone should redo the entire server code and unify client and server.
bool good_map() // call this function only at startmap
{
    if (mlayout) MA = checkarea(sfactor, mlayout);

    F2F = 1000 * MINFF;
    if(m_flags)
    {
        flaginfo &f0 = flaginfos[0];
        flaginfo &f1 = flaginfos[1];
#define DIST(x) (f0.pos.x - f1.pos.x)
        F2F = (!numflagspawn[0] || !numflagspawn[1]) ? 1000 * MINFF : DIST(x)*DIST(x)+DIST(y)*DIST(y);
#undef DIST
    }

    item_fail = false;
    loopv(ents)
    {
        entity &e1 = ents[i];
        if (e1.type < I_CLIPS || e1.type > I_AKIMBO) continue;
        float density = 0, hdensity = 0;
        loopvj(ents)
        {
            entity &e2 = ents[j];
            if (e2.type < I_CLIPS || e2.type > I_AKIMBO || i == j) continue;
#define DIST(x) (e1.x - e2.x)
#define DIST_ATT ((e1.z + e1.attr1) - (e2.z + e2.attr1))
            float r2 = DIST(x)*DIST(x) + DIST(y)*DIST(y) + DIST_ATT*DIST_ATT;
#undef DIST_ATT
#undef DIST
            if ( r2 == 0.0f ) { conoutf("\f3MAP CHECK FAIL: Items too close %s %s (%hd,%hd)", entnames[e1.type], entnames[e2.type],e1.x,e1.y); item_fail = true; break; }
            r2 = 1/r2;
            if (r2 < 0.0025f) continue;
            if (e1.type != e2.type)
            {
                hdensity += r2;
                continue;
            }
            density += r2;
        }
        if ( hdensity > 0.5f ) { conoutf("\f3MAP CHECK FAIL: Items too close %s %.2f (%hd,%hd)", entnames[e1.type],hdensity,e1.x,e1.y); item_fail = true; break; }
        switch(e1.type)
        {
#define LOGTHISSWITCH(X) if( density > X ) { conoutf("\f3MAP CHECK FAIL: Items too close %s %.2f (%hd,%hd)", entnames[e1.type],density,e1.x,e1.y); item_fail = true; break; }
            case I_CLIPS:
            case I_HEALTH: LOGTHISSWITCH(0.24f); break;
            case I_AMMO: LOGTHISSWITCH(0.04f); break;
            case I_HELMET: LOGTHISSWITCH(0.02f); break;
            case I_ARMOUR:
            case I_GRENADE:
            case I_AKIMBO: LOGTHISSWITCH(0.005f); break;
            default: break;
#undef LOGTHISSWITCH
        }
    }

    map_quality = (!item_fail && F2F > MINFF && MA < MAXMAREA && Mh < MAXMHEIGHT && Hhits < MAXHHITS) ? MAP_IS_GOOD : MAP_IS_BAD;
    if ( (!connected || gamemode == GMODE_COOPEDIT) && map_quality == MAP_IS_BAD ) map_quality = MAP_IS_EDITABLE;
    return map_quality > 0;
}

VARP(hudextras, 0, 0, 3);

int teamworkid = -1;

void showhudextras(char hudextras, char value){
    void (*outf)(const char *s, ...) = (hudextras > 1 ? hudoutf : conoutf);
    bool caps = hudextras < 3 ? false : true;
    switch(value)
    {
        case HE_COMBO:
        case HE_COMBO2:
        case HE_COMBO3:
        case HE_COMBO4:
        case HE_COMBO5:
        {
            int n = value - HE_COMBO;
            if (n > 3) outf("\f3%s",strcaps("monster combo!!!",caps)); // I expect to never see this one
            else if (!n) outf("\f5%s",strcaps("combo", caps));
            else outf("\f5%s x%d",strcaps("multi combo", caps),n+1);
            break;
        }
        case HE_TEAMWORK:
            outf("\f5%s",strcaps("teamwork done", caps)); break;
        case HE_FLAGDEFENDED:
            outf("\f5%s",strcaps("you defended the flag", caps)); break;
        case HE_FLAGCOVERED:
            outf("\f5%s",strcaps("you covered the flag", caps)); break;
        case HE_COVER:
            if (teamworkid >= 0)
            {
                playerent *p = getclient(teamworkid);
                if (!p || p == player1) teamworkid = -1;
                else outf("\f5you covered %s",p->name); break;
            }
        default:
        {
            if (value >= HE_NUM)
            {
                teamworkid = value - HE_NUM;
                playerent *p = getclient(teamworkid);
                if (!p || p == player1) teamworkid = -1;
                else outf("\f4you replied to %s",p->name);
            }
            else outf("\f3Update your client!");
            break;
        }
    }
#undef SSPAM
}

int lastspawn = 0;

void onCallVote(int type, int vcn, char *text, char *a)
{
    if(identexists("onCallVote"))
    {
        defformatstring(runas)("%s %d %d [%s] [%s]", "onCallVote", type, vcn, text, a);
        execute(runas);
    }
}

void onChangeVote(int mod, int id)
{
    if(identexists("onChangeVote"))
    {
        defformatstring(runas)("%s %d %d", "onChangeVote", mod, id);
        execute(runas);
    }
}

VARP(voicecomsounds, 0, 1, 2);
bool medals_arrived=0;
medalsst a_medals[END_MDS];
void parsemessages(int cn, playerent *d, ucharbuf &p, bool demo = false)
{
    static char text[MAXTRANS];
    int type, joining = 0;
    bool demoplayback = false;

    while(p.remaining())
    {
        type = getint(p);

        if(demo && watchingdemo && demoprotocol == 1132)
        {
            if(type > SV_IPLIST) --type;            // SV_WHOIS removed
            if(type >= SV_TEXTPRIVATE) ++type;      // SV_TEXTPRIVATE added
            if(type == SV_SWITCHNAME)               // SV_SPECTCN removed
            {
                getint(p);
                continue;
            }
            else if(type > SV_SWITCHNAME) --type;
        }

        #ifdef _DEBUG
        if(type!=SV_POS && type!=SV_CLIENTPING && type!=SV_PING && type!=SV_PONG && type!=SV_CLIENT)
        {
            DEBUGVAR(d);
            ASSERT(type>=0 && type<SV_NUM);
            DEBUGVAR(messagenames[type]);
            protocoldebug(true);
        }
        else protocoldebug(false);
        #endif

        switch(type)
        {
            case SV_SERVINFO:  // welcome message from the server
            {
                int mycn = getint(p), prot = getint(p);
                if(prot!=CUR_PROTOCOL_VERSION && !(watchingdemo && prot == -PROTOCOL_VERSION))
                {
                    conoutf(_("%c3incompatible game protocol (local protocol: %d :: server protocol: %d)"), CC, CUR_PROTOCOL_VERSION, prot);
                    conoutf("\f3if this occurs a lot, obtain an upgrade from \f1http://assault.cubers.net");
                    if(watchingdemo) conoutf("breaking loop : \f3this demo is using a different protocol\f5 : end it now!"); // SVN-WiP-bug: causes endless retry loop else!
                    else disconnect();
                    return;
                }
                sessionid = getint(p);
                player1->clientnum = mycn;
                if(getint(p) > 0) conoutf(_("INFO: this server is password protected"));
                sendintro();
                break;
            }

            case SV_WELCOME:
                joining = getint(p);
                player1->resetspec();
                resetcamera();
                break;

            case SV_CLIENT:
            {
                int cn = getint(p), len = getuint(p);
                ucharbuf q = p.subbuf(len);
                parsemessages(cn, getclient(cn), q, demo);
                break;
            }

            case SV_SOUND:
                audiomgr.playsound(getint(p), d);
                break;

            case SV_VOICECOMTEAM:
            {
                playerent *d = getclient(getint(p));
                if(d) d->lastvoicecom = lastmillis;
                int t = getint(p);
                if(!d || !(d->muted || d->ignored))
                {
                    if ( voicecomsounds == 1 || (voicecomsounds == 2 && m_teammode) ) audiomgr.playsound(t, SP_HIGH);
                }
                break;
            }
            case SV_VOICECOM:
            {
                int t = getint(p);
                if(!d || !(d->muted || d->ignored))
                {
                    if ( voicecomsounds == 1 ) audiomgr.playsound(t, SP_HIGH);
                }
                if(d) d->lastvoicecom = lastmillis;
                break;
            }

            case SV_TEAMTEXTME:
            case SV_TEAMTEXT:
            {
                int cn = getint(p);
                getstring(text, p);
                filtertext(text, text);
                playerent *d = getclient(cn);
                if(!d) break;
                if(d->ignored) clientlogf("ignored: %s%s %s", colorname(d), type == SV_TEAMTEXT ? ":" : "", text);
                else
                {
                    if(m_teammode) conoutf(type == SV_TEAMTEXTME ? "\f1%s %s" : "%s:\f1 %s", colorname(d), highlight(text));
                    else conoutf(type == SV_TEAMTEXTME ? "\f0%s %s" : "%s:\f0 %s", colorname(d), highlight(text));
                }
                break;
            }

            case SV_TEXTME:
            case SV_TEXT:
                if(cn == -1)
                {
                    getstring(text, p);
                    conoutf("MOTD:");
                    conoutf("\f4%s", text);
                }
                else if(d)
                {
                    getstring(text, p);
                    filtertext(text, text);
                    if(d->ignored && d->clientrole != CR_ADMIN) clientlogf("ignored: %s%s %s", colorname(d), type == SV_TEXT ? ":" : "", text);
                    else conoutf(type == SV_TEXTME ? "\f0%s %s" : "%s:\f0 %s", colorname(d), highlight(text));
                }
                else return;
                break;

            case SV_TEXTPRIVATE:
            {
                int cn = getint(p);
                getstring(text, p);
                filtertext(text, text);
                playerent *d = getclient(cn);
                if(!d) break;
                if(d->ignored) clientlogf("ignored: pm %s %s", colorname(d), text);
                else
                {
                    conoutf("%s (PM):\f9 %s", colorname(d), highlight(text));
                    lastpm = d->clientnum;
                    if(identexists("onPM"))
                    {
                        defformatstring(onpm)("onPM %d [%s]", d->clientnum, text);
                        execute(onpm);
                    }
                }
                break;
            }

            case SV_MAPCHANGE:
            {
                extern int spawnpermission;
                spawnpermission = SP_SPECT;
                getstring(text, p);
                int mode = getint(p);
                int downloadable = getint(p);
                int revision = getint(p);
                localwrongmap = !changemapserv(text, mode, downloadable, revision);
                if(m_arena && joining>2) deathstate(player1);
                break;
            }

            case SV_ITEMLIST:
            {
                int n;
                resetspawns();
                while((n = getint(p))!=-1) setspawn(n, true);
                break;
            }

            case SV_MAPIDENT:
            {
                loopi(2) getint(p);
                break;
            }

            case SV_SWITCHNAME:
                getstring(text, p);
                filtertext(text, text, 0, MAXNAMELEN);
                if(!text[0]) copystring(text, "unarmed");
                if(d)
                {
                    if(strcmp(d->name, text))
                        conoutf(_("%s is now known as %s"), colorname(d), colorname(d, text));
                    if(identexists("onNameChange"))
                    {
                        defformatstring(onnamechange)("onNameChange %d \"%s\"", d->clientnum, text);
                        execute(onnamechange);
                    }
                    copystring(d->name, text, MAXNAMELEN+1);
                    updateclientname(d);
                }
                break;

            case SV_SWITCHTEAM:
                getint(p);
                break;

            case SV_SWITCHSKIN:
                loopi(2)
                {
                    int skin = getint(p);
                    if(d) d->setskin(i, skin);
                }
                break;

            case SV_INITCLIENT:            // another client either connected or changed name/team
            {
                int cn = getint(p);
                playerent *d = newclient(cn);
                if(!d)
                {
                    getstring(text, p);
                    loopi(2) getint(p);
                    getint(p);
                    if(!demo || !watchingdemo || demoprotocol > 1132) getint(p);
                    break;
                }
                getstring(text, p);
                filtertext(text, text, 0, MAXNAMELEN);
                if(!text[0]) copystring(text, "unarmed");
                if(d->name[0])          // already connected
                {
                    if(strcmp(d->name, text))
                        conoutf(_("%s is now known as %s"), colorname(d), colorname(d, text));
                }
                else                    // new client
                {
                    conoutf(_("connected: %s"), colorname(d, text));
                }
                copystring(d->name, text, MAXNAMELEN+1);
                if(identexists("onConnect"))
                {
                    defformatstring(onconnect)("onConnect %d", d->clientnum);
                    execute(onconnect);
                }
                loopi(2) d->setskin(i, getint(p));
                d->team = getint(p);

                if(!demo || !watchingdemo || demoprotocol > 1132) d->address = getint(p); // partial IP address

                if(m_flags) loopi(2)
                {
                    flaginfo &f = flaginfos[i];
                    if(!f.actor) f.actor = getclient(f.actor_cn);
                }
                updateclientname(d);
                break;
            }

            case SV_CDIS:
            {
                int cn = getint(p);
                playerent *d = getclient(cn);
                if(!d) break;
                if(d->name[0]) conoutf(_("player %s disconnected"), colorname(d));
                zapplayer(players[cn]);
                if(identexists("onDisconnect"))
                {
                    defformatstring(ondisconnect)("onDisconnect %d", d->clientnum);
                    execute(ondisconnect);
                }
                break;
            }

            case SV_EDITMODE:
            {
                int val = getint(p);
                if(!d) break;
                if(val) d->state = CS_EDITING;
                else
                {
                    //2011oct16:flowtron:keep spectator state
                    //specators shouldn't be allowed to toggle editmode for themselves. they're ghosts!
                    d->state = d->state==CS_SPECTATE?CS_SPECTATE:CS_ALIVE;
                }
                break;
            }

            case SV_SPAWN:
            {
                playerent *s = d;
                if(!s) { static playerent dummy; s = &dummy; }
                s->respawn();
                s->lifesequence = getint(p);
                s->health = getint(p);
                s->armour = getint(p);
                int gunselect = getint(p);
                s->setprimary(gunselect);
                s->selectweapon(gunselect);
                loopi(NUMGUNS) s->ammo[i] = getint(p);
                loopi(NUMGUNS) s->mag[i] = getint(p);
                s->state = CS_SPAWNING;
                if(s->lifesequence==0) s->resetstats(); //NEW
                break;
            }

            case SV_SPAWNSTATE:
            {
                if ( map_quality == MAP_IS_BAD )
                {
                    loopi(6+2*NUMGUNS) getint(p);
                    conoutf(_("map deemed unplayable - fix it before you can spawn"));
                    break;
                }

                if(editmode) toggleedit(true);
                showscores(false);
                setscope(false);
                setburst(false);
                player1->respawn();
                player1->lifesequence = getint(p);
                player1->health = getint(p);
                player1->armour = getint(p);
                player1->setprimary(getint(p));
                player1->selectweapon(getint(p));
                int arenaspawn = getint(p);
                loopi(NUMGUNS) player1->ammo[i] = getint(p);
                loopi(NUMGUNS) player1->mag[i] = getint(p);
                player1->state = CS_ALIVE;
                lastspawn = lastmillis;
                findplayerstart(player1, false, arenaspawn);
                arenaintermission = 0;
                if(m_arena && !localwrongmap)
                {
                    closemenu(NULL);
                    conoutf(_("new round starting... fight!"));
                    hudeditf(HUDMSG_TIMER, "FIGHT!");
                    if(m_botmode) BotManager.RespawnBots();
                }
                addmsg(SV_SPAWN, "rii", player1->lifesequence, player1->weaponsel->type);
                player1->weaponswitch(player1->primweap);
                player1->weaponchanging -= weapon::weaponchangetime/2;
                if(player1->lifesequence==0) player1->resetstats(); //NEW
                break;
            }

            case SV_SHOTFX:
            {
                int scn = getint(p), gun = getint(p);
                vec from, to;
                loopk(3) to[k] = getint(p)/DMF;
                playerent *s = getclient(scn);
                if(!s || !weapon::valid(gun)) break;
                loopk(3) from[k] = s->o.v[k];
                if(gun==GUN_SHOTGUN) createrays(from, to);
                s->lastaction = lastmillis;
                s->weaponchanging = 0;
                s->mag[gun]--;
                if(s->weapons[gun])
                {
                    s->lastattackweapon = s->weapons[gun];
                    s->weapons[gun]->gunwait = s->weapons[gun]->info.attackdelay;
                    s->weapons[gun]->attackfx(from, to, -1);
                    s->weapons[gun]->reloading = 0;
                }
                s->pstatshots[gun]++; //NEW
                break;
            }

            case SV_THROWNADE:
            {
                vec from, to;
                loopk(3) from[k] = getint(p)/DMF;
                loopk(3) to[k] = getint(p)/DMF;
                int nademillis = getint(p);
                if(!d) break;
                d->lastaction = lastmillis;
                d->weaponchanging = 0;
                d->lastattackweapon = d->weapons[GUN_GRENADE];
                if(d->weapons[GUN_GRENADE])
                {
                    d->weapons[GUN_GRENADE]->attackfx(from, to, nademillis);
                    d->weapons[GUN_GRENADE]->reloading = 0;
                }
                if(d!=player1) d->pstatshots[GUN_GRENADE]++; //NEW
                break;
            }

            case SV_RELOAD:
            {
                int cn = getint(p), gun = getint(p);
                playerent *p = getclient(cn);
                if(p && p!=player1) p->weapons[gun]->reload(false);
                break;
            }

            // for AUTH: WIP

            case SV_AUTHREQ:
            {
                extern int autoauth;
                getstring(text, p);
                if(autoauth && text[0] && tryauth(text)) conoutf("server requested authkey \"%s\"", text);
                break;
            }

            case SV_AUTHCHAL:
            {
                getstring(text, p);
                authkey *a = findauthkey(text);
                uint id = (uint)getint(p);
                getstring(text, p);
                if(a && a->lastauth && lastmillis - a->lastauth < 60*1000)
                {
                    vector<char> buf;
                    answerchallenge(a->key, text, buf);
                    //conoutf("answering %u, challenge %s with %s", id, text, buf.getbuf());
                    addmsg(SV_AUTHANS, "rsis", a->desc, id, buf.getbuf());
                }
                break;
            }

            // :for AUTH

            case SV_GIBDAMAGE:
            case SV_DAMAGE:
            {
                int tcn = getint(p),
                    acn = getint(p),
                    gun = getint(p),
                    damage = getint(p),
                    armour = getint(p),
                    health = getint(p);
                playerent *target = getclient(tcn), *actor = getclient(acn);
                if(!target || !actor) break;
                target->armour = armour;
                target->health = health;
                dodamage(damage, target, actor, -1, type==SV_GIBDAMAGE, false);
                actor->pstatdamage[gun]+=damage; //NEW
                break;
            }

            case SV_POINTS:
            {
                int count = getint(p);
                if ( count > 0 ) {
                    loopi(count){
                        int pcn = getint(p); int score = getint(p);
                        playerent *ppl = getclient(pcn);
                        if (!ppl) break;
                        ppl->points += score;
                    }
                } else {
                    int medals = getint(p);
                    if(medals > 0) {
//                         medals_arrived=1;
                        loopi(medals) {
                            int mcn=getint(p); int mtype=getint(p); int mitem=getint(p);
                            a_medals[mtype].assigned=1;
                            a_medals[mtype].cn=mcn;
                            a_medals[mtype].item=mitem;
                        }
                    }
                }
                break;
            }

            case SV_HUDEXTRAS:
            {
                char value = getint(p);
                if (hudextras) showhudextras(hudextras, value);
                break;
            }

            case SV_HITPUSH:
            {
                int gun = getint(p), damage = getint(p);
                vec dir;
                loopk(3) dir[k] = getint(p)/DNF;
                player1->hitpush(damage, dir, NULL, gun);
                break;
            }

            case SV_GIBDIED:
            case SV_DIED:
            {
                int vcn = getint(p), acn = getint(p), frags = getint(p), gun = getint(p);
                playerent *victim = getclient(vcn), *actor = getclient(acn);
                if(!actor) break;
                if ( m_mp(gamemode) ) actor->frags = frags;
                if(!victim) break;
                dokill(victim, actor, type==SV_GIBDIED, gun);
                break;
            }

            case SV_RESUME:
            {
                loopi(MAXCLIENTS)
                {
                    int cn = getint(p);
                    if(p.overread() || cn<0) break;
                    int state = getint(p), lifesequence = getint(p), primary = getint(p), gunselect = getint(p), flagscore = getint(p), frags = getint(p), deaths = getint(p), health = getint(p), armour = getint(p), points = getint(p);
                    int teamkills = 0;
                    if(!demo || !watchingdemo || demoprotocol > 1132) teamkills = getint(p);
                    int ammo[NUMGUNS], mag[NUMGUNS];
                    loopi(NUMGUNS) ammo[i] = getint(p);
                    loopi(NUMGUNS) mag[i] = getint(p);
                    playerent *d = (cn == getclientnum() ? player1 : newclient(cn));
                    if(!d) continue;
                    if(d!=player1) d->state = state;
                    d->lifesequence = lifesequence;
                    d->flagscore = flagscore;
                    d->frags = frags;
                    d->deaths = deaths;
                    d->points = points;
                    d->tks = teamkills;
                    if(d!=player1)
                    {
                        d->setprimary(primary);
                        d->selectweapon(gunselect);
                        d->health = health;
                        d->armour = armour;
                        memcpy(d->ammo, ammo, sizeof(ammo));
                        memcpy(d->mag, mag, sizeof(mag));
                        if(d->lifesequence==0) d->resetstats(); //NEW
                    }
                }
                break;
            }

            case SV_DISCSCORES:
            {
                discscores.shrink(0);
                int team;
                while((team = getint(p)) >= 0)
                {
                    discscore &ds = discscores.add();
                    ds.team = team;
                    getstring(text, p);
                    filtertext(ds.name, text, 0, MAXNAMELEN);
                    ds.flags = getint(p);
                    ds.frags = getint(p);
                    ds.deaths = getint(p);
                    ds.points = getint(p);
                }
                break;
            }
            case SV_ITEMSPAWN:
            {
                int i = getint(p);
                setspawn(i, true);
                break;
            }

            case SV_ITEMACC:
            {
                int i = getint(p), cn = getint(p);
                playerent *d = getclient(cn);
                pickupeffects(i, d);
                break;
            }

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
                }
                break;
            }

            case SV_NEWMAP:
            {
                int size = getint(p);
                if(size>=0) empty_world(size, true);
                else empty_world(-1, true);
                if(d && d!=player1)
                    conoutf(size>=0 ? _("%s started a new map of size %d") : _("%s enlarged the map to size %d"), colorname(d), sfactor);
                break;
            }

            case SV_EDITENT:            // coop edit of ent
            {
                uint i = getint(p);
                while((uint)ents.length()<=i) ents.add().type = NOTUSED;
                int to = ents[i].type;
                if(ents[i].type==SOUND)
                {
                    entity &e = ents[i];

                    entityreference entref(&e);
                    location *loc = audiomgr.locations.find(e.attr1, &entref, mapsounds);

                    if(loc)
                        loc->drop();
                }

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
                if(ents[i].type==SOUND) audiomgr.preloadmapsound(ents[i]);
                break;
            }

            case SV_PONG:
            {
                int millis = getint(p);
                addmsg(SV_CLIENTPING, "i", player1->ping = max(0, (player1->ping*5+totalmillis-millis)/6));
                break;
            }

            case SV_CLIENTPING:
                if(!d) return;
                d->ping = getint(p);
                break;

            case SV_GAMEMODE:
                nextmode = getint(p);
                if (nextmode >= GMODE_NUM) nextmode -= GMODE_NUM;
                break;

            case SV_TIMEUP:
            {
                int curgamemillis = getint(p);
                int curgamelimit = getint(p);
                timeupdate(curgamemillis, curgamelimit);
                break;
            }

            case SV_WEAPCHANGE:
            {
                int gun = getint(p);
                if(d) d->selectweapon(gun);
                break;
            }

            case SV_SERVMSG:
                getstring(text, p);
                conoutf("%s", text);
                break;

            case SV_FLAGINFO:
            {
                int flag = getint(p);
                if(flag<0 || flag>1) return;
                flaginfo &f = flaginfos[flag];
                f.state = getint(p);
                switch(f.state)
                {
                    case CTFF_STOLEN:
                        flagstolen(flag, getint(p));
                        break;
                    case CTFF_DROPPED:
                    {
                        float x = getuint(p)/DMF;
                        float y = getuint(p)/DMF;
                        float z = getuint(p)/DMF;
                        flagdropped(flag, x, y, z);
                        break;
                    }
                    case CTFF_INBASE:
                        flaginbase(flag);
                        break;
                    case CTFF_IDLE:
                        flagidle(flag);
                        break;
                }
                break;
            }

            case SV_FLAGMSG:
            {
                int flag = getint(p);
                int message = getint(p);
                int actor = getint(p);
                int flagtime = message == FM_KTFSCORE ? getint(p) : -1;
                flagmsg(flag, message, actor, flagtime);
                break;
            }

            case SV_FLAGCNT:
            {
                int fcn = getint(p);
                int flags = getint(p);
                playerent *p = getclient(fcn);
                if(p) p->flagscore = flags;
                break;
            }

            case SV_ARENAWIN:
            {
                int acn = getint(p);
                playerent *alive = getclient(acn);
                conoutf(_("the round is over! next round in 5 seconds..."));
                if(m_botmode && acn==-2) hudoutf(_("the bots have won the round!"));
                else if(!alive) hudoutf(_("everyone died!"));
                else if(m_teammode) hudoutf(_("team %s has won the round!"), team_string(alive->team));
                else if(alive==player1) hudoutf(_("you are the survivor!"));
                else hudoutf(_("%s is the survivor!"), colorname(alive));
                arenaintermission = lastmillis;
                break;
            }

            case SV_SPAWNDENY:
            {
                extern int spawnpermission;
                spawnpermission = getint(p);
                if(spawnpermission == SP_REFILLMATCH) hudoutf("\f3You can now spawn to refill your team.");
                break;
            }
            case SV_FORCEDEATH:
            {
                int cn = getint(p);
                playerent *d = cn==getclientnum() ? player1 : newclient(cn);
                if(!d) break;
                deathstate(d);
                break;
            }

            case SV_SERVOPINFO:
            {
                loopv(players) { if(players[i]) players[i]->clientrole = CR_DEFAULT; }
                player1->clientrole = CR_DEFAULT;

                int cl = getint(p), r = getint(p);
                if(cl >= 0 && r >= 0)
                {
                    playerent *pl = (cl == getclientnum() ? player1 : newclient(cl));
                    if(pl)
                    {
                        pl->clientrole = r;
                        if(pl->name[0])
                        {
                            // two messages required to allow for proper german translation - is there a better way to do it?
                            if(pl==player1) conoutf(_("you claimed %s status"), r == CR_ADMIN ? "admin" : "master");
                            else conoutf(_("%s claimed %s status"), colorname(pl), r == CR_ADMIN ? "admin" : "master");
                        }
                    }
                }
                break;
            }

            case SV_TEAMDENY:
            {
                int t = getint(p);
                if(m_teammode)
                {
                    if(team_isvalid(t)) conoutf(_("you can't change to team %s"), team_string(t));
                }
                else
                {
                    conoutf(_("you can't change to %s mode"), team_isspect(t) ? _("spectate") : _("active"));
                }
                break;
            }

            case SV_SETTEAM:
            {
                int fpl = getint(p), fnt = getint(p), ftr = fnt >> 4;
                fnt &= 0x0f;
                playerent *d = (fpl == getclientnum() ? player1 : newclient(fpl));
                if(d)
                {
                    const char *nts = team_string(fnt);
                    bool you = fpl == player1->clientnum;
                    if(m_teammode || team_isspect(fnt))
                    {
                        if(d->team == fnt)
                        {
                            if(you && ftr == FTR_AUTOTEAM) hudoutf("you stay in team %s", nts);
                        }
                        else
                        {
                            if(you && !watchingdemo)
                            {
                                switch(ftr)
                                {
                                    case FTR_PLAYERWISH:
                                        conoutf(_("you're now in team %s"), nts);
                                        break;
                                    case FTR_AUTOTEAM:
                                        hudoutf(_("the server forced you to team %s"), nts);
                                        break;
                                }
                            }
                            else
                            {
                                const char *pls = colorname(d);
                                bool et = team_base(player1->team) != team_base(fnt);
                                switch(ftr)
                                {
                                    case FTR_PLAYERWISH:
                                        conoutf(_("player %s switched to team %s"), pls, nts); // new message
                                        break;
                                    case FTR_AUTOTEAM:
                                        if(watchingdemo) conoutf(_("the server forced %s to team %s"), colorname(d), nts);
                                        else hudoutf(_("the server forced %s to %s team"), colorname(d), et ? _("the enemy") : _("your"));
                                        break;
                                }
                            }
                            if(you && !team_isspect(d->team) && team_isspect(fnt) && d->state == CS_DEAD) spectatemode(SM_FLY);
                        }
                    }
                    else if(d->team != fnt && ftr == FTR_PLAYERWISH) conoutf(_("%s changed to active play"), you ? _("you") : colorname(d));
                    d->team = fnt;
                    if(team_isspect(d->team)) d->state = CS_SPECTATE;
                }
                break;
            }

            case SV_SERVERMODE:
            {
                int sm = getint(p);
                servstate.autoteam = sm & 1;
                servstate.mastermode = (sm >> 2) & MM_MASK;
                servstate.matchteamsize = sm >> 4;
                //if(sm & AT_SHUFFLE) playsound(TEAMSHUFFLE);    // TODO
                break;
            }

            case SV_CALLVOTE:
            {
                int type = getint(p);
                int vcn = -1, n_yes = 0, n_no = 0;
                if ( type == -1 )
                {
                    d = getclient(vcn = getint(p));
                    n_yes = getint(p);
                    n_no = getint(p);
                    type = getint(p);
                }
                if (type == SA_MAP && d == NULL) d = player1;      // gonext uses this
                if( type < 0 || type >= SA_NUM || !d ) return;
                votedisplayinfo *v = NULL;
                string a1, a2;
                switch(type)
                {
                    case SA_MAP:
                        getstring(text, p);
                        filtertext(text, text);
                        itoa(a1, getint(p));
                        defformatstring(t)("%d", getint(p));
                        v = newvotedisplayinfo(d, type, text, a1, t);
                        break;
                    case SA_KICK:
                    case SA_BAN:
                    {
                        itoa(a1, getint(p));
                        getstring(text, p);
                        filtertext(text, text);
                        v = newvotedisplayinfo(d, type, a1, text);
                        break;
                    }
                    case SA_SERVERDESC:
                        getstring(text, p);
                        filtertext(text, text);
                        v = newvotedisplayinfo(d, type, text, NULL);
                        break;
                    case SA_STOPDEMO:
                        // compatibility
                        break;
                    case SA_REMBANS:
                    case SA_SHUFFLETEAMS:
                        v = newvotedisplayinfo(d, type, NULL, NULL);
                        break;
                    case SA_FORCETEAM:
                        itoa(a1, getint(p));
                        itoa(a2, getint(p));
                        v = newvotedisplayinfo(d, type, a1, a2);
                        break;
                    default:
                        itoa(a1, getint(p));
                        v = newvotedisplayinfo(d, type, a1, NULL);
                        break;
                }
                displayvote(v);
                onCallVote(type, v->owner->clientnum, text, a1);
                if (vcn >= 0)
                {
                    loopi(n_yes) votecount(VOTE_YES);
                    loopi(n_no) votecount(VOTE_NO);
                }
                extern int vote(int);
                if (d == player1) vote(VOTE_YES);
                break;
            }

            case SV_CALLVOTESUC:
            {
                callvotesuc();
                onChangeVote( 0, -1);
                break;
            }

            case SV_CALLVOTEERR:
            {
                int errn = getint(p);
                callvoteerr(errn);
                onChangeVote( 1, errn);
                break;
            }

            case SV_VOTE:
            {
                int vote = getint(p);
                votecount(vote);
                onChangeVote( 2, vote);
                break;
            }

            case SV_VOTERESULT:
            {
                int vres = getint(p);
                voteresult(vres);
                onChangeVote( 3, vres);
                break;
            }

            case SV_IPLIST:
            {
                int cn;
                while((cn = getint(p)) >= 0 && !p.overread())
                {
                    playerent *pl = getclient(cn);
                    int ip = getint(p);
                    if(!pl) continue;
                    else pl->address = ip;
                }
                break;
            }

            case SV_SENDDEMOLIST:
            {
                int demos = getint(p);
                if(!demos) conoutf(_("no demos available"));
                else loopi(demos)
                {
                    getstring(text, p);
                    conoutf("%d. %s", i+1, text);
                }
                break;
            }

            case SV_DEMOPLAYBACK:
            {
                string demofile;
                extern char *curdemofile;
                if(demo && watchingdemo && demoprotocol == 1132)
                {
                    watchingdemo = demoplayback = getint(p)!=0;
                    copystring(demofile, "n/a");
                }
                else
                {
                    getstring(demofile, p, MAXSTRLEN);
                    watchingdemo = demoplayback = demofile[0] != '\0';
                }
                DELETEA(curdemofile);
                if(demoplayback)
                {
                    curdemofile = newstring(demofile);
                    player1->resetspec();
                    player1->state = CS_SPECTATE;
                    player1->team = TEAM_SPECT;
                }
                else
                {
                    // cleanups
                    curdemofile = newstring("n/a");
                    loopv(players) zapplayer(players[i]);
                    clearvote();
                    player1->state = CS_ALIVE;
                    player1->resetspec();
                }
                player1->clientnum = getint(p);
                break;
            }

            default:
                neterr("type");
                return;
        }
    }

    #ifdef _DEBUG
    protocoldebug(false);
    #endif
}

void setDemoFilenameFormat(char *fmt)
{
    extern string demofilenameformat;
    if(fmt && fmt[0]!='\0')
    {
        copystring(demofilenameformat, fmt);
    } else copystring(demofilenameformat, DEFDEMOFILEFMT); // reset to default if passed empty string - or should we output the current value in this case?
}
COMMANDN(demonameformat, setDemoFilenameFormat, "s");
void setDemoTimestampFormat(char *fmt)
{
    extern string demotimestampformat;
    if(fmt && fmt[0]!='\0')
    {
        copystring(demotimestampformat, fmt);
    } else copystring(demotimestampformat, DEFDEMOTIMEFMT); // reset to default if passed empty string - or should we output the current value in this case?
}
COMMANDN(demotimeformat, setDemoTimestampFormat, "s");
void setDemoTimeLocal(int *truth)
{
    extern int demotimelocal;
    demotimelocal = *truth == 0 ? 0 : 1;
}
COMMANDN(demotimelocal, setDemoTimeLocal, "i");
void getdemonameformat() { extern string demofilenameformat; result(demofilenameformat); } COMMAND(getdemonameformat, "");
void getdemotimeformat() { extern string demotimestampformat; result(demotimestampformat); } COMMAND(getdemotimeformat, "");
void getdemotimelocal() { extern int demotimelocal; intret(demotimelocal); } COMMAND(getdemotimelocal, "");


const char *parseDemoFilename(char *srvfinfo)
{
    int gmode = 0; //-314;
    int mplay = 0;
    int mdrop = 0;
    int stamp = 0;
    string srvmap;
    if(srvfinfo && srvfinfo[0])
    {
        int fip = 0;
        char sep[] = ":";
        char *pch;
        pch = strtok (srvfinfo,sep);
        while (pch != NULL && fip < 4)
        {
            fip++;
            switch(fip)
            {
                case 1: gmode = atoi(pch); break;
                case 2: mplay = atoi(pch); break;
                case 3: mdrop = atoi(pch); break;
                case 4: stamp = atoi(pch); break;
                default: break;
            }
            pch = strtok (NULL, sep);
        }
        copystring(srvmap, pch);
    }
    extern const char *getDemoFilename(int gmode, int mplay, int mdrop, int tstamp, char *srvmap);
    return getDemoFilename(gmode, mplay, mdrop, stamp, srvmap);
}

void receivefile(uchar *data, int len)
{
    static char text[MAXTRANS];
    ucharbuf p(data, len);
    int type = getint(p);
    data += p.length();
    len -= p.length();
    switch(type)
    {
        case SV_SENDDEMO:
        {
            getstring(text, p);
            extern string demosubpath;
            defformatstring(demofn)("%s", parseDemoFilename(text));
            defformatstring(fname)("demos/%s%s.dmo", demosubpath, demofn);
            copystring(demosubpath, "");
            data += strlen(text);
            int demosize = getint(p);
            if(p.remaining() < demosize)
            {
                p.forceoverread();
                break;
            }
            path(fname);
            stream *demo = openrawfile(fname, "wb");
            if(!demo)
            {
                conoutf(_("failed writing to \"%s\""), fname);
                return;
            }
            conoutf(_("received demo \"%s\""), fname);
            demo->write(&p.buf[p.len], demosize);
            delete demo;
            break;
        }

        case SV_RECVMAP:
        {
            getstring(text, p);
            conoutf(_("received map \"%s\" from server, reloading.."), text);
            int mapsize = getint(p);
            int cfgsize = getint(p);
            int cfgsizegz = getint(p);
            /* int revision = */ getint(p);
            int size = mapsize + cfgsizegz;
            if(MAXMAPSENDSIZE < mapsize + cfgsizegz || cfgsize > MAXCFGFILESIZE) { // sam's suggestion
                conoutf(_("map %s is too large to receive"), text);
            } else {
                if(p.remaining() < size)
                {
                    p.forceoverread();
                    break;
                }
                if(securemapcheck(text))
                {
                    p.len += size;
                    break;
                }
                writemap(path(text), mapsize, &p.buf[p.len]);
                p.len += mapsize;
                writecfggz(path(text), cfgsize, cfgsizegz, &p.buf[p.len]);
                p.len += cfgsizegz;
            }
            break;
        }

        default:
            p.len = 0;
            parsemessages(-1, NULL, p);
            break;
    }
}

void servertoclient(int chan, uchar *buf, int len, bool demo)   // processes any updates from the server
{
    ucharbuf p(buf, len);
    switch(chan)
    {
        case 0: parsepositions(p); break;
        case 1: parsemessages(-1, NULL, p, demo); break;
        case 2: receivefile(p.buf, p.maxlen); break;
    }
}

void localservertoclient(int chan, uchar *buf, int len, bool demo)   // processes any updates from the server
{
//    pktlogger.queue(enet_packet_create (buf, len, 0));  // log local & demo packets
    servertoclient(chan, buf, len, demo);
}
