struct playerent;

struct weapon
{
    const static int weaponchangetime;
    const static float weaponbeloweye;
    static void equipplayer(playerent *pl);

    weapon(struct playerent *owner, int type);
    virtual ~weapon() {}

    int type;
    playerent *owner;
    const struct guninfo &info;
    int &ammo, &mag, &gunwait, shots;
    virtual int dynspread();
    virtual float dynrecoil();
    int reloading, lastaction;

    virtual bool attack(vec &targ) = 0;
    virtual void attackfx(vec &from, vec &to, int millis) = 0;
    virtual void attackphysics(vec &from, vec &to);
    virtual void attacksound();
    virtual bool reload();
    virtual void reset() {}
    
    virtual int modelanim() = 0;
    virtual void updatetimers();
    virtual bool selectable();
    virtual void renderstats();
    virtual void renderhudmodel();
    
    virtual void onselecting();
    virtual void ondeselecting() {}
    virtual void onammopicked() {}

    void sendshoot(vec &from, vec &to);
    bool modelattacking();
    void renderhudmodel(int lastaction, int index = 0);
};

