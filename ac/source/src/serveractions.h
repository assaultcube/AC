// available server actions

enum { EE_LOCAL_SERV = 1, EE_DED_SERV = 1<<1 }; // execution environment

struct serveraction
{
    int role; // required client role
    int area; // only on ded servers
    string desc;

    virtual void perform() = 0;
    virtual bool isvalid() { return true; }
    serveraction() : role(CR_DEFAULT), area(EE_DED_SERV) { desc[0] = '\0'; }
    virtual ~serveraction() { }
};

struct mapaction : serveraction
{
    char *map;
    int mode;
    void perform() { resetmap(map, mode); }
    bool isvalid() { return serveraction::isvalid() && mode != GMODE_DEMO; }
    mapaction(char *map, int mode) : map(map), mode(mode)
    {
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
    void disconnect(int reason) { disconnect_client(cn, reason); }
    virtual bool isvalid() { return valid_client(cn) && clients[cn]->role != CR_ADMIN; } // actions can't be done on admins
    playeraction(int cn) : cn(cn) {};
};

struct forceteamaction : playeraction
{
    void perform() { forceteam(cn, team_opposite(team_int(clients[cn]->team)), true); }
    virtual bool isvalid() { return m_teammode && valid_client(cn); }
    forceteamaction(int cn) : playeraction(cn)
    {
        if(isvalid()) s_sprintf(desc)("force player %s to the enemy team", clients[cn]->name);
    }
};

struct giveadminaction : playeraction
{
    void perform() { changeclientrole(cn, CR_ADMIN, NULL, true); }
    giveadminaction(int cn) : playeraction(cn)
    {
        role = CR_ADMIN;
    }
};

struct kickaction : playeraction
{
    void perform()  { disconnect(DISC_MKICK); }
    kickaction(int cn) : playeraction(cn)
    {
        if(isvalid()) s_sprintf(desc)("kick player %s", clients[cn]->name);
    }
};

struct banaction : playeraction
{
    void perform()
    {
        ban b = { clients[cn]->peer->address, servmillis+20*60*1000 };
		bans.add(b);
        disconnect(DISC_MBAN);
    }
    banaction(int cn) : playeraction(cn)
    {
        role = CR_ADMIN;
        if(isvalid()) s_sprintf(desc)("ban player %s", clients[cn]->name);
    }
};

struct removebansaction : serveraction
{
    void perform() { bans.setsize(0); }
    removebansaction()
    {
        role = CR_ADMIN;
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
        role = CR_ADMIN;
        if(isvalid()) s_sprintf(desc)("change mastermode to '%s'", mode == MM_OPEN ? "open" : "private");
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
        if(isvalid()) s_strcpy(desc, "shuffle teams");
    }
};

struct recorddemoaction : enableaction
{
    void perform() { demonextmatch = enable; }
    recorddemoaction(bool enable) : enableaction(enable)
    {
        role = CR_ADMIN;
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
        role = CR_ADMIN;
        if(isvalid()) s_sprintf(desc)("clear demo %d", demo);
    }
};

struct serverdescaction : serveraction
{
    char *sdesc;
    void perform() { updatesdesc(sdesc); }
    bool isvalid() { return serveraction::isvalid() && updatedescallowed(); }
    serverdescaction(char *sdesc) : sdesc(sdesc)
    {
        role = CR_ADMIN;
        s_sprintf(desc)("set server description to '%s'", sdesc);
    }
    ~serverdescaction() { DELETEA(sdesc); }
};
