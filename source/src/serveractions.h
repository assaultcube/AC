// available server actions

enum { EE_LOCAL_SERV = 1, EE_DED_SERV = 1<<1 }; // execution environment

int roleconf(int key)
{ // current defaults: "fGkBMasRCDxP"
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

struct mapaction : serveraction
{
    char *map;
    int mode;
    void perform()
    {
        if(isdedicated && numclients() > 2 && smode >= 0 && smode != 1 && gamemillis > gamelimit/4)
        {
            forceintermission = true;
            nextgamemode = mode;
            s_strcpy(nextmapname, map);
        }
        else
        {
            resetmap(map, mode);
        }
    }
    bool isvalid() { return serveraction::isvalid() && mode != GMODE_DEMO && map[0]; }
    bool isdisabled() { return configsets.inrange(curcfgset) && !configsets[curcfgset].vote; }
    mapaction(char *map, int mode, int caller) : map(map), mode(mode)
    {
        if(isdedicated)
        {
            bool notify = valid_client(caller) && clients[caller]->role == CR_DEFAULT;
            mapstats *ms = getservermapstats(map);
            if(strchr(scl.voteperm, 'X') && !ms) // admin needed for unknown maps
            {
                role = CR_ADMIN;
                if(notify) sendservmsg("the server does not have this map", caller);
            }
            if(ms && !strchr(scl.voteperm, 'p')) // admin needed for mismatched modes
            {
                int smode = mode;  // 'borrow' the mode macros by replacing a global by a local var
                bool spawns = (m_teammode && !m_ktf) ? ms->hasteamspawns : ms->hasffaspawns;
                bool flags = m_flags && !m_htf ? ms->hasflags : true;
                if(!spawns || !flags)
                {
                    role = CR_ADMIN;
                    s_sprintfd(msg)("\f3map \"%s\" does not support \"%s\": ", behindpath(map), modestr(mode, false));
                    if(!spawns) s_strcat(msg, "player spawns");
                    if(!spawns && !flags) s_strcat(msg, " and ");
                    if(!flags) s_strcat(msg, "flag bases");
                    s_strcat(msg, " missing");
                    if(notify) sendservmsg(msg, caller);
                    logger->writeline(log::info, "%s", msg);
                }
            }
        }
        area |= EE_LOCAL_SERV; // local too
        s_sprintf(desc)("load map '%s' in mode '%s'", map, modestr(mode));
    }
    ~mapaction() { DELETEA(map); }
};

struct demoplayaction : serveraction
{
    char *map;
    void perform() { resetmap(map, GMODE_DEMO); }
    demoplayaction(char *map) : map(map)
    {
        area = EE_LOCAL_SERV; // only local
    }

    ~demoplayaction() { DELETEA(map); }
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
    void perform() { forceteam(cn, team_opposite(team_int(clients[cn]->team)), true); }
    virtual bool isvalid() { return m_teammode && valid_client(cn); }
    forceteamaction(int cn, int caller) : playeraction(cn)
    {
        if(cn != caller) role = roleconf('f');
        if(isvalid()) s_sprintf(desc)("force player %s to the enemy team", clients[cn]->name);
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
    kickaction(int cn) : playeraction(cn)
    {
        wasvalid = false;
        role = roleconf('k');
        if(isvalid())
        {
            wasvalid = true;
            s_sprintf(desc)("kick player %s", clients[cn]->name);
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
    banaction(int cn) : playeraction(cn)
    {
        role = roleconf('B');
        if(isvalid())
        {
            wasvalid = true;
            s_sprintf(desc)("ban player %s", clients[cn]->name);
        }
    }
};

struct removebansaction : serveraction
{
    void perform() { bans.setsize(0); }
    removebansaction()
    {
        role = roleconf('B');
        s_strcpy(desc, "remove all bans");
    }
};

struct mastermodeaction : serveraction
{
    int mode;
    void perform() { mastermode = mode; }
    bool isvalid() { return mode >= 0 && mode < MM_NUM; }
    mastermodeaction(int mode) : mode(mode)
    {
        role = roleconf('M');
        if(isvalid()) s_sprintf(desc)("change mastermode to '%s'", mmfullname(mode));
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
        sendf(-1, 1, "ri2", SV_AUTOTEAM, (autoteam = enable) == 1 ? AT_ENABLED : AT_DISABLED);
        if(m_teammode && enable) refillteams(true);
    }
    autoteamaction(bool enable) : enableaction(enable)
    {
        role = roleconf('a');
        if(isvalid()) s_sprintf(desc)("%s autoteam", enable ? "enable" : "disable");
    }
};

struct shuffleteamaction : serveraction
{
    void perform()
    {
        sendf(-1, 1, "ri2", SV_AUTOTEAM, AT_SHUFFLE);
        shuffleteams();
    }
    bool isvalid() { return serveraction::isvalid() && m_teammode; }
    shuffleteamaction()
    {
        role = roleconf('s');
        if(isvalid()) s_strcpy(desc, "shuffle teams");
    }
};

struct recorddemoaction : enableaction
{
    void perform() { demonextmatch = enable; }
    recorddemoaction(bool enable) : enableaction(enable)
    {
        role = roleconf('R');
        if(isvalid()) s_sprintf(desc)("%s demorecord", enable ? "enable" : "disable");
    }
};

struct stopdemoaction : serveraction
{
    void perform()
    {
        if(m_demo) enddemoplayback();
        else enddemorecord();
    }
    stopdemoaction()
    {
        role = CR_ADMIN;
        area |= EE_LOCAL_SERV;
        s_strcpy(desc, "stop demo");
    }
};

struct cleardemosaction : serveraction
{
    int demo;
    void perform() { cleardemos(demo); }
    cleardemosaction(int demo) : demo(demo)
    {
        role = roleconf('C');
        if(isvalid()) s_sprintf(desc)("clear demo %d", demo);
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
        s_sprintf(desc)("set server description to '%s'", sdesc);
        if(isvalid()) address = clients[cn]->peer->address;
    }
    ~serverdescaction() { DELETEA(sdesc); }
};
