// available server actions

enum { EE_LOCAL_SERV = 1, EE_DED_SERV = 1<<1 }; // execution environment

int roleconf(int key)
{ // current defaults: "fGkBMasRCDEW"
    if(strchr(scl.voteperm, tolower(key))) return CR_DEFAULT;
    if(strchr(scl.voteperm, toupper(key))) return CR_ADMIN;
    return (key) == tolower(key) ? CR_DEFAULT : CR_ADMIN;
}

struct serveraction
{
    int role; // required client role
    int area; // only on ded servers
    string desc;

    virtual void perform() = 0;
    virtual bool isvalid() { return true; }
    virtual bool isdisabled() { return false; }
    serveraction() : role(CR_DEFAULT), area(EE_DED_SERV) { desc[0] = '\0'; }
    virtual ~serveraction() { }
};

void kick_abuser(int cn, int &cmillis, int &count, int limit)
{
    if ( cmillis + 30000 > servmillis ) count++;
    else {
        count -= count > 0 ? (servmillis - cmillis)/30000 : 0;
        if ( count <= 0 ) count = 1;
    }
    cmillis = servmillis;
    if( count >= limit ) disconnect_client(cn, DISC_ABUSE);
}

bool mapisok(mapstats *ms)
{
    if ( ms->hasflags ) // Check if flags are ok
    {
        struct { short x, y; } fl[2];
        loopi(2)
        {
            if(ms->flags[i] == 1)
            {
                short *fe = ms->entposs + ms->flagents[i] * 3;
                fl[i].x = *fe; fe++; fl[i].y = *fe;
            }
            else fl[i].x = fl[i].y = 0; // the map has no valid flags
        }
        FlagFlag = pow2(fl[0].x - fl[1].x) + pow2(fl[0].y - fl[1].y);
    }
    else FlagFlag = MINFF * 1000; // the map has no flags
    return Mheight < MAXMHEIGHT && (Mopen = checkarea(testlayout_factor, testlayout)) < MAXMAREA && FlagFlag > MINFF;
}

struct mapaction : serveraction
{
    char *map;
    int mode;
    bool mapok, queue;
    void perform()
    {
        if (queue) return;
        if(isdedicated && numclients() > 2 && smode >= 0 && smode != 1 && gamemillis > gamelimit/4)
        {
            forceintermission = true;
            nextgamemode = mode;
            copystring(nextmapname, map);
        }
        else
        {
            startgame(map, mode);
        }
    }
    bool isvalid() { return serveraction::isvalid() && mode != GMODE_DEMO && map[0] && mapok && !(isdedicated && !m_mp(mode)); }
    bool isdisabled() { return maprot.current() && !maprot.current()->vote; }
    mapaction(char *map, int mode, int caller, bool q) : map(map), mode(mode), queue(q)
    {
        if(isdedicated)
        {
            bool notify = valid_client(caller);
            int maploc = MAP_NOTFOUND;
            mapstats *ms = map[0] ? getservermapstats(map, false, &maploc) : NULL;
            mapok = ms != NULL && ( (mode != GMODE_COOPEDIT && mapisok(ms)) || (mode == GMODE_COOPEDIT && !readonlymap(maploc)) );
            if(!mapok)
            {
                defformatstring(msg)("%s", ms ? ( readonlymap(maploc) ? "this map cannot be coopedited in this server" : "sorry, but this map does not satisfy some quality requisites to be played in MultiPlayer Mode" ) : "the server does not have this map" );
                if(notify) sendservmsg(msg, caller);
            }
            else
            { // check, if map supports mode
                if(mode == GMODE_COOPEDIT && !strchr(scl.voteperm, 'e')) role = CR_ADMIN;
                bool romap = mode == GMODE_COOPEDIT && readonlymap(maploc);
                int smode = mode;  // 'borrow' the mode macros by replacing a global by a local var
                bool spawns = mode == GMODE_COOPEDIT || (m_teammode && !m_ktf ? ms->hasteamspawns : ms->hasffaspawns);
                bool flags = mode != GMODE_COOPEDIT && m_flags && !m_htf ? ms->hasflags : true;
                if(!spawns || !flags || romap)
                { // unsupported mode
                    if(strchr(scl.voteperm, 'P')) role = CR_ADMIN;
                    else if(!strchr(scl.voteperm, 'p')) mapok = false; // default: no one can vote for unsupported mode/map combinations
                    defformatstring(msg)("\f3map \"%s\" does not support \"%s\": ", behindpath(map), modestr(mode, false));
                    if(romap) concatstring(msg, "map is readonly");
                    else
                    {
                        if(!spawns) concatstring(msg, "player spawns");
                        if(!spawns && !flags) concatstring(msg, " and ");
                        if(!flags) concatstring(msg, "flag bases");
                        concatstring(msg, " missing");
                    }
                    if(notify) sendservmsg(msg, caller);
                    logline(ACLOG_INFO, "%s", msg);
                }
            }
            loopv(scl.adminonlymaps)
            {
                const char *s = scl.adminonlymaps[i], *h = strchr(s, '#'), *m = behindpath(map);
                size_t sl = strlen(s);
                if(h && h != s)
                {
                    sl = h - s;
                    if(mode != atoi(h + 1)) continue;
                }
                if(sl == strlen(m) && !strncmp(m, scl.adminonlymaps[i], sl)) role = CR_ADMIN;
            }
        }
        else mapok = true;
        area |= EE_LOCAL_SERV; // local too
        formatstring(desc)("load map '%s' in mode '%s'", map, modestr(mode));
    }
    ~mapaction() { DELETEA(map); }
};

struct demoplayaction : serveraction
{
    char *demofilename;
    void perform() { startdemoplayback(demofilename); }
    demoplayaction(char *demofilename) : demofilename(demofilename)
    {
        area = EE_LOCAL_SERV; // only local
    }

    ~demoplayaction() { DELETEA(demofilename); }
};

struct playeraction : serveraction
{
    int cn;
    ENetAddress address;
    void disconnect(int reason)
    {
        int i = findcnbyaddress(&address);
        if(i >= 0) disconnect_client(i, reason);
    }
    virtual bool isvalid() { return valid_client(cn) && clients[cn]->role != CR_ADMIN; } // actions can't be done on admins
    playeraction(int cn) : cn(cn)
    {
        if(isvalid()) address = clients[cn]->peer->address;
    };
};

struct forceteamaction : playeraction
{
    void perform() { updateclientteam(cn, team_opposite(clients[cn]->team), FTR_SILENTFORCE); }
    virtual bool isvalid() { return m_teammode && valid_client(cn); }
    forceteamaction(int cn, int caller) : playeraction(cn)
    {
        if(cn != caller) role = roleconf('f');
        if(isvalid() && !(clients[cn]->state.forced && clients[caller]->role != CR_ADMIN)) formatstring(desc)("force player %s to the enemy team", clients[cn]->name);
    }
};

struct giveadminaction : playeraction
{
    void perform() { changeclientrole(cn, CR_ADMIN, NULL, true); }
    giveadminaction(int cn) : playeraction(cn)
    {
        role = CR_ADMIN;
//        role = roleconf('G');
    }
};

struct kickaction : playeraction
{
    bool wasvalid;
    void perform()  { disconnect(DISC_MKICK); }
    virtual bool isvalid() { return wasvalid || playeraction::isvalid(); }
    kickaction(int cn, char *reason) : playeraction(cn)
    {
        wasvalid = false;
        role = roleconf('k');
        if(isvalid() && strlen(reason) > 3)
        {
            wasvalid = true;
            formatstring(desc)("kick player %s, reason: %s", clients[cn]->name, reason);
        }
    }
};

struct banaction : playeraction
{
    bool wasvalid;
    void perform()
    {
        ban b = { address, servmillis+20*60*1000 };
		bans.add(b);
        disconnect(DISC_MBAN);
    }
    virtual bool isvalid() { return wasvalid || playeraction::isvalid(); }
    banaction(int cn, char *reason) : playeraction(cn)
    {
        wasvalid = false;
        role = roleconf('B');
        if(isvalid() && strlen(reason) > 3)
        {
            wasvalid = true;
            formatstring(desc)("ban player %s, reason: %s", clients[cn]->name, reason);
        }
    }
};

struct removebansaction : serveraction
{
    void perform() { bans.shrink(0); }
    removebansaction()
    {
        role = roleconf('B');
        copystring(desc, "remove all bans");
    }
};

struct mastermodeaction : serveraction
{
    int mode;
    void perform() { changemastermode(mode); }
    bool isvalid() { return mode >= 0 && mode < MM_NUM; }
    mastermodeaction(int mode) : mode(mode)
    {
        role = roleconf('M');
        if(isvalid()) formatstring(desc)("change mastermode to '%s'", mmfullname(mode));
    }
};

struct enableaction : serveraction
{
    bool enable;
    enableaction(bool enable) : enable(enable) {}
};

struct autoteamaction : enableaction
{
    void perform()
    {
        autoteam = enable;
        sendservermode();
        if(m_teammode && enable) refillteams(true);
    }
    autoteamaction(bool enable) : enableaction(enable)
    {
        role = roleconf('A');
        if(isvalid()) formatstring(desc)("%s autoteam", enable ? "enable" : "disable");
    }
};

struct shuffleteamaction : serveraction
{
    void perform()
    {
        sendf(-1, 1, "ri2", SV_SERVERMODE, sendservermode(false) | AT_SHUFFLE);
        shuffleteams();
    }
    bool isvalid() { return serveraction::isvalid() && m_teammode; }
    shuffleteamaction()
    {
        role = roleconf('S');
        if(isvalid()) copystring(desc, "shuffle teams");
    }
};

struct recorddemoaction : enableaction            // TODO: remove completely
{
    void perform() { }
    bool isvalid() { return serveraction::isvalid(); }
    recorddemoaction(bool enable) : enableaction(enable)
    {
        role = roleconf('R');
        if(isvalid()) formatstring(desc)("%s demorecord", enable ? "enable" : "disable");
    }
};

struct stopdemoaction : serveraction
{
    bool isvalid() { return serveraction::isvalid() && m_demo; }
    void perform()
    {
        if(m_demo) enddemoplayback();
    }
    stopdemoaction()
    {
        role = CR_ADMIN;
        area |= EE_LOCAL_SERV;
        copystring(desc, "stop demo playback");
    }
};

struct cleardemosaction : serveraction
{
    int demo;
    void perform() { cleardemos(demo); }
    cleardemosaction(int demo) : demo(demo)
    {
        role = roleconf('C');
        if(isvalid()) formatstring(desc)("clear demo %d", demo);
    }
};

struct serverdescaction : serveraction
{
    char *sdesc;
    int cn;
    ENetAddress address;
    void perform() { updatesdesc(sdesc, &address); }
    bool isvalid() { return serveraction::isvalid() && updatedescallowed() && valid_client(cn); }
    serverdescaction(char *sdesc, int cn) : sdesc(sdesc), cn(cn)
    {
        role = roleconf('D');
        formatstring(desc)("set server description to '%s'", sdesc);
        if(isvalid()) address = clients[cn]->peer->address;
    }
    ~serverdescaction() { DELETEA(sdesc); }
};
