// available server actions

struct serveraction
{
    int role;
    bool dedicated;
    virtual ~serveraction() {}
    virtual void perform() = 0;
    virtual bool isvalid() { return true; }
};

struct mapaction : serveraction
{
    char *map;
    int mode;    
    void perform() { resetmap(map, mode); }
    mapaction(char *map, int mode) : map(map), mode(mode)
    {  
        role = CR_MASTER; 
        dedicated = false; 
    }
    ~mapaction() { DELETEA(map); }
};

struct playeraction : serveraction 
{ 
    int cn;
    void disconnect(int reason) { disconnect_client(cn, reason); }
    bool isvalid() { return valid_client(cn); }
    playeraction(int cn) : cn(cn) {};
};

struct forceteamaction : playeraction
{
    void perform() { forceteam(cn, team_opposite(team_int(clients[cn]->team)), true); }
    forceteamaction(int cn) : playeraction(cn) 
    { 
        role = CR_MASTER; 
        dedicated = true; 
    }
};

struct givemasteraction : playeraction
{
    void perform() { changeclientrole(cn, CR_MASTER, NULL, true); }
    givemasteraction(int cn) : playeraction(cn) 
    { 
        role = CR_ADMIN; 
        dedicated = true; 
    }
};

struct kickaction : playeraction
{
    void perform() { disconnect(DISC_MKICK); }
    kickaction(int cn) : playeraction(cn) 
    { 
        role = CR_MASTER; 
        dedicated = true; 
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
        role = CR_MASTER; 
        dedicated = true; 
    }
};

struct removebansaction : serveraction
{
    void perform() { bans.setsize(0); }
    removebansaction() 
    { 
        role = CR_MASTER; 
        dedicated = true; 
    }
};

struct mastermodeaction : serveraction 
{ 
    int mode;
    void perform() { mastermode = mode; }
    bool isvalid() { return mode >= 0 && mode < MM_NUM; }
    mastermodeaction(int mode) : mode(mode)
    { 
        role = CR_MASTER; 
        dedicated = true; 
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
    autoteamaction(bool enable) : enableaction(enable)
    { 
        role = CR_MASTER; 
        dedicated = true; 
    }
};

struct recorddemoaction : enableaction
{
    void perform() { demonextmatch = enable; }
    recorddemoaction(bool enable) : enableaction(enable)
    {
        role = CR_ADMIN;
        dedicated = true;
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
        dedicated = true;
    }
};

struct cleardemosaction : serveraction
{
    int demo;
    void perform() { cleardemos(demo); }
    cleardemosaction(int demo) : demo(demo)
    {
        role = CR_ADMIN;
        dedicated = true;
    }
};

