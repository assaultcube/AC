
class playerent;
class bounceent;

struct weapon
{
    const static int weaponchangetime;
    const static float weaponbeloweye;
    static void equipplayer(playerent *pl);

    weapon(class playerent *owner, int type);
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
    virtual bool reload(bool autoreloaded);
    virtual void reset() {}
    virtual bool busy() { return false; }

    virtual int modelanim() = 0;
    virtual void updatetimers(int millis);
    virtual bool selectable();
    virtual bool deselectable();
    virtual void renderstats();
    virtual void renderhudmodel();
    virtual void renderaimhelp(bool teamwarning);

    virtual void onselecting();
    virtual void ondeselecting() {}
    virtual void onammopicked() {}
    virtual void onownerdies() {}
    virtual void removebounceent(bounceent *b) {}

    void sendshoot(vec &from, vec &to, int millis);
    bool modelattacking();
    void renderhudmodel(int lastaction, int index = 0);

    static bool valid(int id);

    virtual int flashtime() const;
};

class grenadeent;

enum { GST_NONE = 0, GST_INHAND, GST_THROWING };

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
    void removebounceent(bounceent *b);
    int flashtime() const;
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
    int dynspread();
    bool selectable();
};


struct sniperrifle : gun
{
    bool scoped;
    int scoped_since;

    sniperrifle(playerent *owner);
    void attackfx(const vec &from, const vec &to, int millis);
    bool reload(bool autoreloaded);

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


struct carbine : gun
{
    carbine(playerent *owner);
    bool selectable();
};

struct shotgun : gun
{
    shotgun(playerent *owner);
    void attackphysics(vec &from, vec &to);
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

struct cpistol : gun
{
    bool bursting;
    cpistol(playerent *owner);
    bool reload(bool autoreloaded);
    bool selectable();
    void onselecting();
    void ondeselecting();
    bool attack(vec &targ);
    void setburst(bool enable);
};

struct pistol : gun
{
    pistol(playerent *owner);
    bool selectable();
};


struct akimbo : gun
{
    akimbo(playerent *owner);

    int akimboside;
    int akimbomillis;
    int akimbolastaction[2];

    void attackfx(const vec &from, const vec &to, int millis);
    void onammopicked();
    void onselecting();
    bool selectable();
    void updatetimers(int millis);
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

    int flashtime() const;
};

