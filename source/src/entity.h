enum                            // static entity types
{
    NOTUSED = 0,                // entity slot not in use in map
    LIGHT,                      // lightsource, attr1 = radius, attr2 = intensity
    PLAYERSTART,                // attr1 = angle
    I_CLIPS, I_AMMO,I_GRENADE,
    I_HEALTH, I_ARMOUR, I_AKIMBO,
    MAPMODEL,                   // attr1 = angle, attr2 = idx
    CARROT,                     // attr1 = tag, attr2 = type
    LADDER,
    CTF_FLAG,                   // attr1 = angle, attr2 = red/blue
    SOUND,
    MAXENTTYPES
};

#define isitem(i) (i >= I_CLIPS && i <= I_AKIMBO)

struct persistent_entity        // map entity
{
    short x, y, z;              // cube aligned position
    short attr1;
    uchar type;                 // type is one of the above
    uchar attr2, attr3, attr4;
};

struct entity : public persistent_entity
{
    bool spawned;               //the only dynamic state of a map entity
    bool soundinuse;
};

struct itemstat { int add, start, max, sound; };
static itemstat ammostats[] =
{
    {1,  1,   1,   S_ITEMAMMO},   //knife dummy
    {16, 32,  72,  S_ITEMAMMO},   //pistol
    {14, 28,  21,  S_ITEMAMMO},   //shotgun
    {60, 90,  90,  S_ITEMAMMO},   //subgun
    {10, 20,  15,  S_ITEMAMMO},   //sniper
    {30, 60,  60,  S_ITEMAMMO},   //assault
    {2,  0,   2,   S_ITEMAMMO},   //grenade
    {72, 0,   72,  S_ITEMAKIMBO}  //akimbo
};

static itemstat powerupstats[] =
{
    {33, 100, 100, S_ITEMHEALTH}, //health
    {50, 100, 100, S_ITEMARMOUR}, //armour
    //{16, 0,   72,  S_ITEMPUP}     //powerup
};

enum { GUN_KNIFE = 0, GUN_PISTOL, GUN_SHOTGUN, GUN_SUBGUN, GUN_SNIPER, GUN_ASSAULT, GUN_GRENADE, GUN_AKIMBO, NUMGUNS };
#define reloadable_gun(g) (g != GUN_KNIFE && g != GUN_GRENADE)

#define SGRAYS 21
#define SGSPREAD 2
#define EXPDAMRAD 10

struct guninfo { string modelname; short sound, reload, reloadtime, attackdelay, damage, projspeed, part, spread, recoil, magsize, mdl_kick_rot, mdl_kick_back, recoilincrease, recoilbase, maxrecoil, recoilbackfade; bool isauto; };
static guninfo guns[NUMGUNS] =
{
    { "knife",      S_KNIFE,      S_NULL,     0,      500,    50,     0,   0,  1,    1,   1,    0,  0,    0,  0,      0,      0,      false },
    { "pistol",     S_PISTOL,     S_RPISTOL,  1400,   170,    19,     0,   0, 80,   10,   8,    6,  5,    1,  40,     75,     150,    false },
    { "shotgun",    S_SHOTGUN,    S_RSHOTGUN, 2400,   1000,   5,      0,   0,  1,   35,   7,    9,  9,    1,  130,    500,    150,    false },
    { "subgun",     S_SUBGUN,     S_RSUBGUN,  1650,   80,     16,     0,   0, 70,   15,   30,   1,  2,    3,  20,     60,     200,    true },
    { "sniper",     S_SNIPER,     S_RSNIPER,  1950,   1500,   85,     0,   0, 60,   50,   5,    4,  4,    1,  100,    500,    100,    false },
    { "assault",    S_ASSAULT,    S_RASSAULT, 2000,   130,    24,     0,   0, 20,   40,   15,   0,  2,    2,  25,     60,     150,    true }, 
    { "grenade",    S_NULL,       S_NULL,     1000,   650,    150,    20,  6,  1,    1,   1,    3,  1,    0,  0,      0,      0,      false },
    { "pistol",     S_PISTOL,     S_RAKIMBO,  1400,   85,    19,     0,   0, 80,   10,   16,   6,  5,    1,  40,     75,     150,    true },
};

static inline int reloadtime(int gun) { return guns[gun].reloadtime; }
static inline int attackdelay(int gun) { return guns[gun].attackdelay; }
static inline int magsize(int gun) { return guns[gun].magsize; }

#define isteam(a,b)   (m_teammode && strcmp(a, b)==0)

#define TEAM_CLA 0
#define TEAM_RVSF 1
#define team_valid(t) (!strcmp(t, "RVSF") || !strcmp(t, "CLA"))
#define team_string(t) ((t) ? "RVSF" : "CLA")
#define team_int(t) (strcmp((t), "CLA") == 0 ? TEAM_CLA : TEAM_RVSF)
#define team_opposite(o) ((o) == TEAM_CLA ? TEAM_RVSF : TEAM_CLA)

enum { ENT_PLAYER = 0, ENT_BOT, ENT_CAMERA, ENT_BOUNCE };
enum { CS_ALIVE = 0, CS_DEAD, CS_SPAWNING, CS_LAGGED, CS_EDITING, CS_SPECTATE };
enum { CR_DEFAULT = 0, CR_MASTER, CR_ADMIN };
enum { SM_NONE = 0, SM_FLY, SM_FOLLOWPLAYER, SM_NUM };

struct physent
{
    vec o, vel;                         // origin, velocity
    float yaw, pitch, roll;             // used as vec in one place
    float pitchvel;                     
    float maxspeed;                     // cubes per second, 24 for player
    int timeinair;                      // used for fake gravity
    float radius, eyeheight, aboveeye;  // bounding box size
    bool inwater;
    bool onfloor, onladder, jumpnext, crouching;
    int lastcrouch;
    char move, strafe;
    uchar state, type;

    physent() : o(0, 0, 0), yaw(270), pitch(0), roll(0), pitchvel(0),
                state(CS_ALIVE), crouching(false), lastcrouch(0)
    {
        reset();
    }
    virtual ~physent() {}

    void reset()
    {
        vel.x = vel.y = vel.z = 0;
        move = strafe = 0;
        timeinair = 0;
        onfloor = onladder = inwater = jumpnext = crouching = false;
    }

    virtual float dyneyeheight()
    { 
        extern int lastmillis, crouchtime;
        const float croucheyeheight = eyeheight*3.0f/4.0f;
        const float t = (lastmillis-lastcrouch)/(float)crouchtime;

        if(lastcrouch && t < 1.0f && t >= 0.0f) // crouch move in progress
        {            
            if(crouching) return eyeheight - t*(eyeheight-croucheyeheight); // crouch
            else return croucheyeheight + t*(eyeheight-croucheyeheight); // uncrouch
        }
        else if(crouching) return croucheyeheight; // fully crouching
        return eyeheight; // not crouching
    }
};

struct dynent : physent                 // animated ent
{
    bool k_left, k_right, k_up, k_down; // see input code  

    animstate prev[2], current[2];              // md2's need only [0], md3's need both for the lower&upper model
    int lastanimswitchtime[2];
    void *lastmodel[2];
    int lastrendered;

    void stopmoving()
    {
        k_left = k_right = k_up = k_down = jumpnext = false;
        move = strafe = 0;
    }

    void resetanim() 
    { 
        loopi(2) 
        { 
            prev[i].reset(); 
            current[i].reset();
            lastanimswitchtime[i] = -1;
            lastmodel[i] = NULL;
        }
        lastrendered = 0;
    }

    void reset()
    {
        physent::reset();
        stopmoving();
    }

    dynent() { reset(); resetanim(); }
};

#define MAXNAMELEN 15
#define MAXTEAMLEN 4

struct bounceent;

#define POSHIST_SIZE 7

struct poshist
{
    int nextupdate, curpos, numpos;
    vec pos[POSHIST_SIZE];

    poshist() : nextupdate(0) { reset(); }

    const int size() const { return numpos; }
    
    void reset()
    {
        curpos = 0;
        numpos = 0;
    }

    void addpos(const vec &o)
    {
        pos[curpos] = o;
        curpos++;
        if(curpos>=POSHIST_SIZE) curpos = 0;
        if(numpos<POSHIST_SIZE) numpos++;
    }

    const vec &getpos(int i) const
    {
        i = curpos-1-i;
        if(i < 0) i += POSHIST_SIZE;
        return pos[i];
    }

    void update(const vec &o, int lastmillis)
    {
        if(lastmillis<nextupdate) return;
        if(o.dist(pos[0]) >= 4.0f) addpos(o);
        nextupdate = lastmillis + 100;
    }
};

struct playerstate
{
    int health, armour;
    int primary, nextprimary;
    int gunselect;
    bool akimbo;
    int ammo[NUMGUNS], mag[NUMGUNS], gunwait[NUMGUNS];

    playerstate() : primary(GUN_ASSAULT), nextprimary(GUN_ASSAULT) {}
    virtual ~playerstate() {}

    itemstat &itemstats(int type)
    {
        switch(type)
        {
            case I_CLIPS: return ammostats[GUN_PISTOL];
            case I_AMMO: return ammostats[primary];
            case I_GRENADE: return ammostats[GUN_GRENADE];
            case I_AKIMBO: return ammostats[GUN_AKIMBO];
            case I_HEALTH:
            case I_ARMOUR:
            default:
                return *(itemstat *)0;
        }
    }

    bool canpickup(int type)
    {
        switch(type)
        {
            case I_CLIPS: return ammo[GUN_PISTOL]<ammostats[GUN_PISTOL].max;
            case I_AMMO: return ammo[primary]<ammostats[primary].max;
            case I_GRENADE: return mag[GUN_GRENADE]<ammostats[GUN_GRENADE].max;
            case I_HEALTH: return health<powerupstats[type-I_HEALTH].max;
            case I_ARMOUR: return armour<powerupstats[type-I_HEALTH].max;
            case I_AKIMBO: return !akimbo;
            default: return false;
        }
    }

    void additem(itemstat &is, int &v)
    {
        v += is.add;
        if(v > is.max) v = is.max;
    }

    void pickup(int type)
    {
        switch(type)
        {
            case I_CLIPS: additem(ammostats[GUN_PISTOL], ammo[GUN_PISTOL]); break;
            case I_AMMO: additem(ammostats[primary], ammo[primary]); break;
            case I_GRENADE: additem(ammostats[GUN_GRENADE], mag[GUN_GRENADE]); break;
            case I_HEALTH: additem(powerupstats[type-I_HEALTH], health); break;
            case I_ARMOUR: additem(powerupstats[type-I_HEALTH], armour); break;
            case I_AKIMBO:
                akimbo = true;
                mag[GUN_AKIMBO] = guns[GUN_AKIMBO].magsize;
                additem(ammostats[GUN_AKIMBO], ammo[GUN_AKIMBO]); break;
                break;
        }
    }

    void respawn()
    {
        health = 100;
        armour = 0;
        gunselect = GUN_PISTOL;
        akimbo = false;
        loopi(NUMGUNS) ammo[i] = mag[i] = gunwait[i] = 0;
        ammo[GUN_KNIFE] = mag[GUN_KNIFE] = 1;
    }

    virtual void spawnstate(int gamemode)
    {
        if(m_pistol) primary = GUN_PISTOL;
        else if(m_osok) primary = GUN_SNIPER;
        else if(m_lss) primary = GUN_KNIFE;
        else primary = nextprimary;

        if(!m_nopistol)
        {
            ammo[GUN_PISTOL] = ammostats[GUN_PISTOL].max-magsize(GUN_PISTOL);
            mag[GUN_PISTOL] = magsize(GUN_PISTOL);
        }

        if(!m_noprimary)
        {
            ammo[primary] = ammostats[primary].start-magsize(primary);
            mag[primary] = magsize(primary);
        }

        gunselect = primary;

        if(m_osok) health = 1;
    }

    // just subtract damage here, can set death, etc. later in code calling this 
    int dodamage(int damage)
    {
        int ad = damage*30/100; // let armour absorb when possible
        if(ad>armour) ad = armour;
        armour -= ad;
        damage -= ad;
        health -= damage;
        return damage;
    }
};

struct playerent : dynent, playerstate
{
    int clientnum, lastupdate, plag, ping;
    int lifesequence;                   // sequence id for each respawn, used in damage test
    int frags, flagscore;
    int deaths() { return lifesequence + (state==CS_DEAD ? 1 : 0); }
    int lastaction, lastmove, lastpain, lastteamkill;
    int clientrole;
    bool attacking;
    string name, team;
    int weaponchanging;
    int nextweapon; // weapon we switch to
    int skin, nextskin; // skin after respawning
    int spectating, followplayercn;
    virtual float dyneyeheight() { return (state==CS_DEAD || state==CS_SPECTATE) && spectating==SM_FLY ? 1.0f : physent::dyneyeheight(); }
    bool allowmove() { return state!=CS_DEAD || spectating==SM_FLY; }
    
    weapon *weapons[NUMGUNS];
    weapon *weaponsel, *nextweaponsel, *primweap, *nextprimweap, *lastattackweapon;

    struct bounceent *inhandnade;

    poshist history; // Previous stored locations of this player

    playerent() : clientnum(-1), plag(0), ping(0), lifesequence(0), frags(0), flagscore(0), lastpain(0), lastteamkill(0), clientrole(CR_DEFAULT),
                  skin(0), nextskin(0), spectating(SM_NONE), followplayercn(0),
                  weaponsel(NULL), nextweaponsel(NULL), primweap(NULL), nextprimweap(NULL), lastattackweapon(NULL), inhandnade(NULL)
    {
        type = ENT_PLAYER;
        name[0] = team[0] = 0;
        eyeheight = 4.5f;
        aboveeye = 0.7f;
        radius = 1.1f;
        maxspeed = 16.0f;
        respawn();
    }

    void damageroll(float damage)
    {
        float damroll = 2.0f*damage;
        roll += roll>0 ? damroll : (roll<0 ? -damroll : (rnd(2) ? damroll : -damroll)); // give player a kick
    }

    void hitpush(int damage, const vec &dir, playerent *actor, int gun)
    {
        vec push(dir);
        push.mul(damage/100.0f);
        vel.add(push);
    }

    void respawn()
    {
        dynent::reset();
        playerstate::respawn();
        history.reset();
        if(weaponsel) weaponsel->reset();
        lastaction = 0;
        lastattackweapon = NULL;
        attacking = false;
        weaponchanging = 0;
        spectating = SM_NONE;
        followplayercn = 0;
    }

    void spawnstate(int gamemode)
    {
        playerstate::spawnstate(gamemode);
        weaponsel = weapons[gunselect];
        primweap = weapons[primary];
        nextprimweap = weapons[nextprimary];
    }

    void selectweapon(int w) { weaponsel = weapons[(gunselect = w)]; }
    void setprimary(int w) { primweap = weapons[(primary = w)]; }
    void setnextprimary(int w) { nextprimweap = weapons[(nextprimary = w)]; }
};

class CBot;

struct botent : playerent
{
    // Added by Rick
    CBot *pBot; // Only used if this is a bot, points to the bot class if we are the host,
                // for other clients its NULL
    // End add by Rick      

    playerent *enemy;                      // monster wants to kill this entity
    // Added by Rick: targetpitch
    float targetpitch;                    // monster wants to look in this direction
    // End add   
    float targetyaw;                    // monster wants to look in this direction

    botent() : pBot(NULL), enemy(NULL) { type = ENT_BOT; }
};

enum { CTFF_INBASE = 0, CTFF_STOLEN, CTFF_DROPPED };

struct flaginfo
{
	int team;
    entity *flag;
	int actor_cn;
	playerent *actor;
    vec originalpos;
    int state; // one of CTFF_*
    bool ack;
    flaginfo() : flag(0), actor(0), state(CTFF_INBASE), ack(false) {}
};

enum { NADE_ACTIVATED = 1, NADE_THROWED, GIB};

struct bounceent : physent // nades, gibs
{
    int millis, timetolife, bouncestate; // see enum above
    float rotspeed;
    playerent *owner;

    bounceent() : bouncestate(0), rotspeed(1.0f), owner(NULL)
    {
        type = ENT_BOUNCE;
        maxspeed = 40;
        radius = 0.2f;
        eyeheight = 0.3f;
        aboveeye = 0.0f;
    }

    virtual ~bounceent() {}

    bool isalive(int lastmillis) { return lastmillis - millis < timetolife; }
    virtual void destroy() {}
};


