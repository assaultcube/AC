// available server actions

struct serveraction
{
    int type, role;
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
        type = SA_MAP; 
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
    void perform() { forceteam(cn, team_opposite(team_int(clients[cn]->team))); }
    forceteamaction(int cn) : playeraction(cn) 
    { 
        type = SA_FORCETEAM; 
        role = CR_MASTER; 
        dedicated = true; 
    }
};

struct givemasteraction : playeraction
{
    void perform() { changeclientrole(cn, CR_MASTER, NULL, true); }
    givemasteraction(int cn) : playeraction(cn) 
    { 
        type = SA_GIVEMASTER; 
        role = CR_ADMIN; 
        dedicated = true; 
    }
};

struct kickaction : playeraction
{
    void perform() { disconnect(DISC_MKICK); }
    kickaction(int cn) : playeraction(cn) 
    { 
        type = SA_KICK; 
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
        type = SA_BAN; 
        role = CR_MASTER; 
        dedicated = true; 
    }
};

struct removebansaction : serveraction
{
    void perform() { bans.setsize(0); }
    removebansaction() 
    { 
        type = SA_REMBANS; 
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
        type = SA_MASTERMODE; 
        role = CR_MASTER; 
        dedicated = true; 
    }
};

struct autoteamaction : serveraction
{
    bool enabled;
    void perform() 
    { 
        sendf(-1, 1, "ri2", SV_AUTOTEAM, (autoteam = enabled) == 1 ? 1 : 0);
        if(m_teammode) shuffleteams();
    }
    autoteamaction(bool enabled) : enabled(enabled)
    { 
        type = SA_AUTOTEAM; 
        role = CR_MASTER; 
        dedicated = true; 
    }
};