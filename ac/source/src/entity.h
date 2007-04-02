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
    bool spawned;               // the only dynamic state of a map entity
};

enum { GUN_KNIFE = 0, GUN_PISTOL, GUN_SHOTGUN, GUN_SUBGUN, GUN_SNIPER, GUN_ASSAULT, GUN_GRENADE, NUMGUNS };
#define reloadable_gun(g) (g != GUN_KNIFE && g != GUN_GRENADE)

#define isteam(a,b)   (m_teammode && strcmp(a, b)==0)

#define TEAM_CLA 0
#define TEAM_RVSF 1
#define team_valid(t) (!strcmp(t, "RVSF") || !strcmp(t, "CLA"))
#define team_string(t) ((t) ? "RVSF" : "CLA")
#define team_int(t) (strcmp((t), "CLA") == 0 ? TEAM_CLA : TEAM_RVSF)
#define team_opposite(o) ((o) == TEAM_CLA ? TEAM_RVSF : TEAM_CLA)

struct itemstat { int add, start, max, sound; };
// End add

enum { ENT_PLAYER = 0, ENT_BOT, ENT_CAMERA, ENT_BOUNCE };

enum { CS_ALIVE = 0, CS_DEAD, CS_LAGGED, CS_EDITING };

struct physent
{
    vec o, vel;                         // origin, velocity
    float yaw, pitch, roll;             // used as vec in one place
    float maxspeed;                     // cubes per second, 24 for player
    int timeinair;                      // used for fake gravity
    float radius, eyeheight, aboveeye;  // bounding box size
    bool inwater;
    bool onfloor, onladder, jumpnext;
    char move, strafe;
    uchar state, type;

    physent() : o(0, 0, 0), yaw(270), pitch(0), roll(0), maxspeed(16),
                radius(1.1f), eyeheight(4.5f), aboveeye(0.7f),
                state(CS_ALIVE), type(ENT_PLAYER)
    {
        reset();
    }

    void reset()
    {
        vel.x = vel.y = vel.z = 0;
        move = strafe = 0;
        timeinair = 0;
        onfloor = onladder = inwater = jumpnext = false;
    }
};

struct dynent : physent                 // animated ent
{
    bool k_left, k_right, k_up, k_down; // see input code  

    animstate prev[2], current[2];              // md2's need only [0], md3's need both for the lower&upper model
    int lastanimswitchtime[2];
    void *lastmodel[2];

    void stopmoving()
    {
        k_left = k_right = k_up = k_down = jumpnext = false;
        move = strafe = 0;
    }

    void reset()
    {
        physent::reset();
        stopmoving();
    }

    dynent() { reset(); loopi(2) { lastanimswitchtime[i] = -1; lastmodel[i] = NULL; } }
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

struct playerent : dynent
{
    int clientnum, lastupdate, plag, ping;
    int lifesequence;                   // sequence id for each respawn, used in damage test
    int frags, flagscore;
    int health, armour;
    int gunselect, gunwait;
    int lastaction, lastattackgun, lastmove, lastpain, lastteamkill;
	bool ismaster;
    bool attacking;
    int ammo[NUMGUNS];
    int mag[NUMGUNS];
    string name, team;
    int shots;                          //keeps track of shots from auto weapons
    bool reloading, hasarmour, weaponchanging;
    int nextweapon; // weapon we switch to
    int primary;                        //primary gun
    int nextprimary; // primary after respawning
    int skin, nextskin; // skin after respawning

    int thrownademillis;
    struct bounceent *inhandnade;
    int akimbo;
    int akimbolastaction[2];
	int akimbomillis;

    poshist history; // Previous stored locations of this player

    playerent() : clientnum(-1), plag(0), ping(0), lifesequence(0), frags(0), flagscore(0), lastpain(0), lastteamkill(0), ismaster(false),
                  shots(0), reloading(false), primary(GUN_ASSAULT), nextprimary(GUN_ASSAULT),
                  skin(0), nextskin(0), inhandnade(NULL)
    {
        name[0] = team[0] = 0;
        respawn();
    }

    void respawn()
    {
        reset();
        history.reset();
        health = 100;
        armour = 0;
        hasarmour = false;
        lastaction = akimbolastaction[0] = akimbolastaction[1] = 0;
        akimbomillis = 0;
        gunselect = GUN_PISTOL;
        gunwait = 0;
        attacking = false;
        weaponchanging = false;
        akimbo = 0;
        loopi(NUMGUNS) ammo[i] = mag[i] = 0;
    }
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

// Moved from server.cpp by Rick
struct server_entity            // server side version of "entity" type
{
    bool spawned;
    int spawnsecs;
};
// End move

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
};

#define NADE_IN_HAND (player1->gunselect==GUN_GRENADE && player1->inhandnade)
#define NADE_THROWING (player1->gunselect==GUN_GRENADE && player1->thrownademillis && !player1->inhandnade)

#define has_akimbo(d) ((d)->gunselect==GUN_PISTOL && (d)->akimbo)

#define WEAPONCHANGE_TIME 400

