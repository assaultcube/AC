// available server actions

struct serveraction
{
    int role; // required client role
    bool dedicated; // only on ded servers
    virtual ~serveraction() {}
    virtual void perform() = 0;
    virtual bool isvalid() { return true; }
    serveraction() : role(CR_DEFAULT), dedicated(true) {}
};

struct mapaction : serveraction
{
    char *map;
    int mode;    
    void perform() { resetmap(map, mode); }
    mapaction(char *map, int mode) : map(map), mode(mode)
    {  
        dedicated = false; 
    }
    ~mapaction() { DELETEA(map); }
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
    forceteamaction(int cn) : playeraction(cn) {}
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
    void perform() { disconnect(DISC_MKICK); }
    kickaction(int cn) : playeraction(cn) {}
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
    }
};

struct removebansaction : serveraction
{
    void perform() { bans.setsize(0); }
    removebansaction() 
    { 
        role = CR_ADMIN; 
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
        sendf(-1, 1, "ri2", SV_AUTOTEAM, (autoteam = enable) == 1 ? 1 : 0);
        if(m_teammode && enable) shuffleteams();
    }
    autoteamaction(bool enable) : enableaction(enable) {}
};

struct recorddemoaction : enableaction
{
    void perform() { demonextmatch = enable; }
    recorddemoaction(bool enable) : enableaction(enable)
    {
        role = CR_ADMIN;
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
    }
};

struct cleardemosaction : serveraction
{
    int demo;
    void perform() { cleardemos(demo); }
    cleardemosaction(int demo) : demo(demo)
    {
        role = CR_ADMIN;
    }
};

