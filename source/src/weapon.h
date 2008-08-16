
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
    virtual void attackfx(const vec &from, const vec &to, int millis) = 0;
    virtual void attackphysics(vec &from, vec &to);
    virtual void attacksound();
    virtual bool reload();
    virtual void reset() {}
    virtual bool busy() { return false; }

    virtual int modelanim() = 0;
    virtual void updatetimers();
    virtual bool selectable();
    virtual bool deselectable();
    virtual void renderstats();
    virtual void renderhudmodel();
    virtual void renderaimhelp(bool teamwarning);
    
    virtual void onselecting();
    virtual void ondeselecting() {}
    virtual void onammopicked() {}
    virtual void onownerdies() {}

    void sendshoot(vec &from, vec &to);
    bool modelattacking();
    void renderhudmodel(int lastaction, int index = 0);

    static bool valid(int id);
};

struct grenadeent;
struct bounceent;

enum { GST_NONE, GST_INHAND, GST_THROWING };

struct grenades : weapon
{
    grenadeent *inhandnade;
    const int throwwait;
    int throwmillis;
    int state;

    grenades(playerent *owner);
    bool attack(vec &targ);
    void attackfx(const vec &from, const vec &to, int millis);
    int modelanim();
    void activatenade(const vec &to);
    void thrownade();
    void thrownade(const vec &vel);
    void dropnade();
    void renderstats();
    bool selectable();
    void reset();
    bool busy();
    void onselecting();
    void onownerdies();
};


struct gun : weapon
{
    gun(playerent *owner, int type);
    virtual bool attack(vec &targ);
    virtual void attackfx(const vec &from, const vec &to, int millis);
    int modelanim();
    void checkautoreload();
};


struct subgun : gun
{
    subgun(playerent *owner);
    bool selectable();
};


struct sniperrifle : gun
{
    bool scoped;

    sniperrifle(playerent *owner);
    void attackfx(const vec &from, const vec &to, int millis);
    bool reload();

    int dynspread();
    float dynrecoil();
    bool selectable();
    void onselecting();
    void ondeselecting();
    void onownerdies();
    void renderhudmodel();
    void renderaimhelp(bool teamwarning);
    void setscope(bool enable);
};


struct shotgun : gun
{
    shotgun(playerent *owner);
    bool attack(vec &targ);
    void attackfx(const vec &from, const vec &to, int millis);
    bool selectable();
};


struct assaultrifle : gun
{
    assaultrifle(playerent *owner);
    int dynspread();
    float dynrecoil();
    bool selectable();
};


struct pistol : gun
{
    pistol(playerent *owner);
    bool selectable();
};


struct akimbo : gun
{
    akimbo(playerent *owner);
        
    bool akimboside;
    int akimbomillis;
    int akimbolastaction[2];

    bool attack(vec &targ);
    void onammopicked();
    void onselecting();
    bool selectable();
    void updatetimers();
    void reset();
    void renderhudmodel();
    bool timerout();
};


struct knife : weapon
{
    knife(playerent *owner);

    bool attack(vec &targ);
    int modelanim();

    void drawstats();
    void attackfx(const vec &from, const vec &to, int millis);
    void renderstats();
};

