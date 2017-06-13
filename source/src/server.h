// server.h

#define gamemode smode   // allows the gamemode macros to work with the server mode

#define SERVER_PROTOCOL_VERSION    (PROTOCOL_VERSION)    // server without any gameplay modification
//#define SERVER_PROTOCOL_VERSION   (-PROTOCOL_VERSION)  // server with gameplay modification but compatible to vanilla client (using /modconnect)
//#define SERVER_PROTOCOL_VERSION  (PROTOCOL_VERSION)    // server with incompatible protocol (change PROTOCOL_VERSION in file protocol.h to a negative number!)

#define valid_flag(f) (f >= 0 && f < 2)

enum { GE_NONE = 0, GE_SHOT, GE_EXPLODE, GE_HIT, GE_AKIMBO, GE_RELOAD, GE_SUICIDE, GE_PICKUP };
enum { ST_EMPTY, ST_LOCAL, ST_TCPIP };

extern int smode, servmillis;

struct shotevent
{
    int type;
    int millis, id;
    int gun;
    float from[3], to[3];
};

struct explodeevent
{
    int type;
    int millis, id;
    int gun;
};

struct hitevent
{
    int type;
    int target;
    int lifesequence;
    union
    {
        int info;
        float dist;
    };
    float dir[3];
};

struct suicideevent
{
    int type;
};

struct pickupevent
{
    int type;
    int ent;
};

struct akimboevent
{
    int type;
    int millis, id;
};

struct reloadevent
{
    int type;
    int millis, id;
    int gun;
};

union gameevent
{
    int type;
    shotevent shot;
    explodeevent explode;
    hitevent hit;
    suicideevent suicide;
    pickupevent pickup;
    akimboevent akimbo;
    reloadevent reload;
};

template <int N>
struct projectilestate
{
    int projs[N];
    int numprojs;

    projectilestate() : numprojs(0) {}

    void reset() { numprojs = 0; }

    void add(int val)
    {
        if(numprojs>=N) numprojs = 0;
        projs[numprojs++] = val;
    }

    bool remove(int val)
    {
        loopi(numprojs) if(projs[i]==val)
        {
            projs[i] = projs[--numprojs];
            return true;
        }
        return false;
    }
};

static const int DEATHMILLIS = 300;

struct clientstate : playerstate
{
    vec o;
    int state;
    int lastdeath, lastspawn, spawn, lifesequence;
    bool forced;
    int lastshot;
    projectilestate<8> grenades;
    int akimbomillis;
    bool scoped;
    int flagscore, frags, teamkills, deaths, shotdamage, damage, points, events, lastdisc, reconnections;

    clientstate() : state(CS_DEAD) {}

    bool isalive(int gamemillis)
    {
        return state==CS_ALIVE || (state==CS_DEAD && gamemillis - lastdeath <= DEATHMILLIS);
    }

    bool waitexpired(int gamemillis)
    {
        int wait = gamemillis - lastshot;
        loopi(NUMGUNS) if(wait < gunwait[i]) return false;
        return true;
    }

    void reset()
    {
        state = CS_DEAD;
        lifesequence = -1;
        grenades.reset();
        akimbomillis = 0;
        scoped = forced = false;
        flagscore = frags = teamkills = deaths = shotdamage = damage = points = events = lastdisc = reconnections = 0;
        respawn();
    }

    void respawn()
    {
        playerstate::respawn();
        o = vec(-1e10f, -1e10f, -1e10f);
        lastdeath = 0;
        lastspawn = -1;
        spawn = 0;
        lastshot = 0;
        akimbomillis = 0;
        scoped = false;
    }
};

struct savedscore
{
    string name;
    uint ip;
    int frags, flagscore, deaths, teamkills, shotdamage, damage, team, points, events, lastdisc, reconnections;
    bool valid, forced;

    void reset()
    {
        // to avoid 2 connections with the same score... this can disrupt some laggers that eventually produces 2 connections (but it is rare)
        frags = flagscore = deaths = teamkills = shotdamage = damage = points = events = lastdisc = reconnections = 0;
    }

    void save(clientstate &cs, int t)
    {
        frags = cs.frags;
        flagscore = cs.flagscore;
        deaths = cs.deaths;
        teamkills = cs.teamkills;
        shotdamage = cs.shotdamage;
        damage = cs.damage;
        points = cs.points;
        forced = cs.forced;
        events = cs.events;
        lastdisc = cs.lastdisc;
        reconnections = cs.reconnections;
        team = t;
        valid = true;
    }

    void restore(clientstate &cs)
    {
        cs.frags = frags;
        cs.flagscore = flagscore;
        cs.deaths = deaths;
        cs.teamkills = teamkills;
        cs.shotdamage = shotdamage;
        cs.damage = damage;
        cs.points = points;
        cs.forced = forced;
        cs.events = events;
        cs.lastdisc = lastdisc;
        cs.reconnections = reconnections;
        reset();
    }
};

struct medals
{
    int dpt, lasthit, lastgun, ncovers, nhs;
    int combohits, combo, combofrags, combotime, combodamage, ncombos;
    int ask, askmillis, linked, linkmillis, linkreason, upmillis, flagmillis;
    int totalhits, totalshots;
    bool updated, combosend;
    vec pos, flagpos;
    void reset()
    {
        dpt = lasthit = lastgun = ncovers = nhs = 0;
        combohits = combo = combofrags = combotime = combodamage = ncombos = 0;
        askmillis = linkmillis = upmillis = flagmillis = 0;
        linkreason = linked = ask = -1;
        totalhits = totalshots = 0;
        updated = combosend = false;
        pos = flagpos = vec(-1e10f, -1e10f, -1e10f);
    }
};

struct client                   // server side version of "dynent" type
{
    int type;
    int clientnum;
    ENetPeer *peer;
    string hostname;
    string name;
    int team;
    char lang[3];
    int ping;
    int skin[2];
    int vote;
    int role;
    int connectmillis, lmillis, ldt, spj;
    int mute, spam, lastvc; // server side voice comm spam control
    int acversion, acbuildtype;
    bool isauthed; // for passworded servers
    bool haswelcome;
    bool isonrightmap, loggedwrongmap, freshgame;
    bool timesync;
    int overflow;
    int gameoffset, lastevent, lastvotecall;
    int lastprofileupdate, fastprofileupdates;
    int demoflags;
    clientstate state;
    vector<gameevent> events;
    vector<uchar> position, messages;
    string lastsaytext;
    int saychars, lastsay, spamcount, badspeech, badmillis;
    int at3_score, at3_lastforce, eff_score;
    bool at3_dontmove;
    int spawnindex;
    int spawnperm, spawnpermsent;
    bool autospawn;
    int salt;
    string pwd;
    uint authreq; // for AUTH
    string authname; // for AUTH
    int mapcollisions, farpickups;
    enet_uint32 bottomRTT;
    medals md;
    bool upspawnp;
    int lag;
    vec spawnp;
    int nvotes;
    int input, inputmillis;
    int ffire, wn, f, g, t, y, p;
    int yb, pb, oy, op, lda, ldda, fam;
    int nt[10], np, lp, ls, lsm, ld, nd, nlt, lem, led;
    vec cp[10], dp[10], d0, lv, lt, le;
    float dda, tr, sda;
    int ps, ph, tcn, bdt, pws;
    float pr;
    int yls, pls, tls;
    int bs, bt, blg, bp;

    gameevent &addevent()
    {
        static gameevent dummy;
        if(events.length()>100) return dummy;
        return events.add();
    }

    void mapchange(bool getmap = false)
    {
        state.reset();
        events.setsize(0);
        overflow = 0;
        timesync = false;
        isonrightmap = m_coop;
        spawnperm = SP_WRONGMAP;
        spawnpermsent = servmillis;
        autospawn = false;
        if(!getmap)
        {
            loggedwrongmap = false;
            freshgame = true;         // permission to spawn at once
        }
        lastevent = 0;
        at3_lastforce = eff_score = 0;
        mapcollisions = farpickups = 0;
        md.reset();
        upspawnp = false;
        lag = 0;
        spawnp = vec(-1e10f, -1e10f, -1e10f);
        lmillis = ldt = spj = 0;
        ffire = 0;
        f = g = y = p = t = 0;
        yb = pb = oy = op = lda = ldda = fam = 0;
        np = lp = ls = lsm = ld = nd = nlt = lem = led = 0;
        d0 = lv = lt = le = vec(0,0,0);
        loopi(10) { cp[i] = dp[i] = vec(0,0,0); nt[i] = 0; }
        dda = tr = sda = 0;
        ps = ph = bdt = pws = 0;
        tcn = -1;
        pr = 0.0f;
        yls = pls = tls = 0;
    }

    void reset()
    {
        name[0] = pwd[0] = demoflags = 0;
        bottomRTT = ping = 9999;
        team = TEAM_SPECT;
        state.state = CS_SPECTATE;
        loopi(2) skin[i] = 0;
        position.setsize(0);
        messages.setsize(0);
        isauthed = haswelcome = false;
        role = CR_DEFAULT;
        lastvotecall = 0;
        lastprofileupdate = fastprofileupdates = 0;
        vote = VOTE_NEUTRAL;
        lastsaytext[0] = '\0';
        saychars = 0;
        spawnindex = -1;
        authreq = 0; // for AUTH
        mapchange();
        freshgame = false;         // don't spawn into running games
        mute = spam = lastvc = badspeech = badmillis = nvotes = 0;
        input = inputmillis = 0;
        wn = -1;
        bs = bt = blg = bp = 0;
    }

    void zap()
    {
        type = ST_EMPTY;
        role = CR_DEFAULT;
        isauthed = haswelcome = false;
    }
};

struct ban
{
    ENetAddress address;
    int millis, type;
};

struct worldstate
{
    enet_uint32 uses;
    vector<uchar> positions, messages;
};

struct server_entity            // server side version of "entity" type
{
    int type;
    bool spawned, legalpickup, twice;
    int spawntime;
    short x, y;
};

struct clientidentity
{
    uint ip;
    int clientnum;
};

struct demofile
{
    string info;
    string file;
    uchar *data;
    int len;
    vector<clientidentity> clientssent;
};

void startgame(const char *newname, int newmode, int newtime = -1, bool notify = true);
void disconnect_client(int n, int reason = -1);
void sendiplist(int receiver, int cn = -1);
int clienthasflag(int cn);
bool refillteams(bool now = false, int ftr = FTR_AUTOTEAM);
void changeclientrole(int client, int role, char *pwd = NULL, bool force=false);
mapstats *getservermapstats(const char *mapname, bool getlayout = false, int *maploc = NULL);
int findmappath(const char *mapname, char *filename = NULL);
int calcscores();
void recordpacket(int chan, void *data, int len);
void senddisconnectedscores(int cn);
void process(ENetPacket *packet, int sender, int chan);
void welcomepacket(packetbuf &p, int n);
void sendwelcome(client *cl, int chan = 1);
void sendpacket(int n, int chan, ENetPacket *packet, int exclude = -1, bool demopacket = false);
int numclients();
bool updateclientteam(int cln, int newteam, int ftr);
void forcedeath(client *cl);
void sendf(int cn, int chan, const char *format, ...);

extern bool isdedicated;
extern string smapname;
extern mapstats smapstats;
extern char *maplayout;

const char *messagenames[SV_NUM] =
{
    "SV_SERVINFO", "SV_WELCOME", "SV_INITCLIENT", "SV_POS", "SV_POSC", "SV_POSN", "SV_TEXT", "SV_TEAMTEXT", "SV_TEXTME", "SV_TEAMTEXTME", "SV_TEXTPRIVATE",
    "SV_SOUND", "SV_VOICECOM", "SV_VOICECOMTEAM", "SV_CDIS",
    "SV_SHOOT", "SV_EXPLODE", "SV_SUICIDE", "SV_AKIMBO", "SV_RELOAD", "SV_AUTHT", "SV_AUTHREQ", "SV_AUTHTRY", "SV_AUTHANS", "SV_AUTHCHAL",
    "SV_GIBDIED", "SV_DIED", "SV_GIBDAMAGE", "SV_DAMAGE", "SV_HITPUSH", "SV_SHOTFX", "SV_THROWNADE",
    "SV_TRYSPAWN", "SV_SPAWNSTATE", "SV_SPAWN", "SV_SPAWNDENY", "SV_FORCEDEATH", "SV_RESUME",
    "SV_DISCSCORES", "SV_TIMEUP", "SV_EDITENT", "SV_ITEMACC",
    "SV_MAPCHANGE", "SV_ITEMSPAWN", "SV_ITEMPICKUP",
    "SV_PING", "SV_PONG", "SV_CLIENTPING", "SV_GAMEMODE",
    "SV_EDITMODE", "SV_EDITXY", "SV_EDITARCH", "SV_EDITBLOCK", "SV_EDITD", "SV_EDITE", "SV_NEWMAP",
    "SV_SENDMAP", "SV_RECVMAP", "SV_REMOVEMAP",
    "SV_SERVMSG", "SV_ITEMLIST", "SV_WEAPCHANGE", "SV_PRIMARYWEAP",
    "SV_FLAGACTION", "SV_FLAGINFO", "SV_FLAGMSG", "SV_FLAGCNT",
    "SV_ARENAWIN",
    "SV_SETADMIN", "SV_SERVOPINFO",
    "SV_CALLVOTE", "SV_CALLVOTESUC", "SV_CALLVOTEERR", "SV_VOTE", "SV_VOTERESULT",
    "SV_SETTEAM", "SV_TEAMDENY", "SV_SERVERMODE",
    "SV_IPLIST",
    "SV_LISTDEMOS", "SV_SENDDEMOLIST", "SV_GETDEMO", "SV_SENDDEMO", "SV_DEMOPLAYBACK",
    "SV_CONNECT",
    "SV_SWITCHNAME", "SV_SWITCHSKIN", "SV_SWITCHTEAM",
    "SV_CLIENT",
    "SV_EXTENSION",
    "SV_MAPIDENT", "SV_HUDEXTRAS", "SV_POINTS"
};

const char *entnames[] =
{
    "none?",
    "light", "playerstart", "pistol", "ammobox","grenades",
    "health", "helmet", "armour", "akimbo",
    "mapmodel", "trigger", "ladder", "ctf-flag", "sound", "clip", "plclip", "dummy", ""
};

// entity attribute scaling and wraparound definitions for mapformat 10
short entwraparound[MAXENTTYPES][7] =
{ // limit angles to 0..359 degree and mapsound slot numbers to 0..255
    {    0,    0,    0,    0,    0,    0,    0 },  // deleted
    {    0,    0,    0,    0,    0,    0,    0 },  // light
    { 3600,    0,    0,    0,    0,    0,    0 },  // playerstart
    {    0,    0,    0,    0,    0,    0,    0 },  // pistol
    {    0,    0,    0,    0,    0,    0,    0 },  // ammobox
    {    0,    0,    0,    0,    0,    0,    0 },  // grenades
    {    0,    0,    0,    0,    0,    0,    0 },  // health
    {    0,    0,    0,    0,    0,    0,    0 },  // helmet
    {    0,    0,    0,    0,    0,    0,    0 },  // armour
    {    0,    0,    0,    0,    0,    0,    0 },  // akimbo
    { -3600,   0,    0,    0, -3600,   0,    0 },  // mapmodel
    {    0,    0,    0,    0,    0,    0,    0 },  // trigger
    {    0,    0,    0,    0,    0,    0,    0 },  // ladder
    { -3600,   0,    0,    0,    0,    0,    0 },  // ctf-flag
    {  256,    0,    0,    0,    0,    0,    0 },  // sound
    {    0,    0,    0,    0,    0,    0,    4 },  // clip
    {    0,    0,    0,    0,    0,    0,    4 },  // plclip
    {    0,    0,    0,    0,    0,    0,    0 }   // dummy
};

uchar entscale[MAXENTTYPES][7] =
{ // (no zeros allowed here!)
    {  1,  1,  1,  1,  1,  1,  1 },  // deleted
    {  1,  1,  1,  1,  1,  1,  1 },  // light
    { 10,  1,  1,  1,  1,  1,  1 },  // playerstart
    { 10,  1,  1,  1,  1,  1,  1 },  // pistol
    { 10,  1,  1,  1,  1,  1,  1 },  // ammobox
    { 10,  1,  1,  1,  1,  1,  1 },  // grenades
    { 10,  1,  1,  1,  1,  1,  1 },  // health
    { 10,  1,  1,  1,  1,  1,  1 },  // helmet
    { 10,  1,  1,  1,  1,  1,  1 },  // armour
    { 10,  1,  1,  1,  1,  1,  1 },  // akimbo
    { 10,  1,  5,  1, 10,  1,  1 },  // mapmodel
    {  1,  1,  1,  1,  1,  1,  1 },  // trigger
    {  1,  1,  1,  1,  1,  1,  1 },  // ladder
    { 10,  1,  1,  1,  1,  1,  1 },  // ctf-flag
    {  1,  1,  1,  1,  1,  1,  1 },  // sound
    { 10,  5,  5,  5,  1, 10,  1 },  // clip
    { 10,  5,  5,  5,  1, 10,  1 },  // plclip
    {  1,  1,  1,  1,  1,  1,  1 }   // dummy
};

// see entity.h:61: struct itemstat { int add, start, max, sound; };
// Please update ./ac_website/htdocs/docs/introduction.html if these figures change.
itemstat ammostats[NUMGUNS] =
{
    {  1,  1,   1,  S_ITEMAMMO  },   // knife dummy
    { 20, 60, 100,  S_ITEMAMMO  },   // pistol
    { 15, 30,  30,  S_ITEMAMMO  },   // carbine
    { 14, 28,  21,  S_ITEMAMMO  },   // shotgun
    { 60, 90,  90,  S_ITEMAMMO  },   // subgun
    { 10, 20,  15,  S_ITEMAMMO  },   // sniper
    { 40, 60,  60,  S_ITEMAMMO  },   // assault
    { 30, 45,  75,  S_ITEMAMMO  },   // cpistol
    {  1,  0,   3,  S_ITEMAMMO  },   // grenade
    {100,  0, 100,  S_ITEMAKIMBO}    // akimbo
};

itemstat powerupstats[I_ARMOUR-I_HEALTH+1] =
{
    {33, 0, 100, S_ITEMHEALTH}, // 0 health
    {25, 0, 100, S_ITEMARMOUR}, // 1 helmet
    {50, 0, 100, S_ITEMARMOUR}, // 2 armour
};

guninfo guns[NUMGUNS] =
{
    // Please update ./ac_website/htdocs/docs/introduction.html if these figures change.
    //mKR: mdl_kick_rot && mKB: mdl_kick_back
    //reI: recoilincrease && reB: recoilbase && reM: maxrecoil && reF: recoilbackfade
    //pFX: pushfactor
    //modelname                 sound                reloadtime        damage    projspeed  spread     magsize    mKB      reB          reF        isauto
    //             title                      reload       attackdelay      piercing     part     recoil       mKR     reI        reM        pFX
    { "knife",   "Knife",        S_KNIFE,   S_NULL,     0,      500,    50, 100,     0,   0,  1,    1,   1,    0,  0,   0,   0,    0,    0,   1,   false },
    { "pistol",  "Pistol",       S_PISTOL,  S_RPISTOL,  1400,   160,    18,   0,     0,   0, 53,   10,   10,   6,  5,   6,  35,   58,   125,  1,   false },
    { "carbine", "TMP-M&A CB",   S_CARBINE, S_RCARBINE, 1800,   720,    60,  40,     0,   0, 10,   60,   10,   4,  4,  10,  60,   60,   150,  1,   false },
    { "shotgun", "V-19 CS",      S_SHOTGUN, S_RSHOTGUN, 2400,   880,    1,    0,     0,   0,  1,   35,    7,   9,  9,  10, 140,  140,   125,  1,   false },   // CAUTION dmg only sane for server!
    { "subgun",  "A-ARD/10 SMG", S_SUBGUN,  S_RSUBGUN,  1650,   80,     16,   0,     0,   0, 45,   15,   30,   1,  2,   5,  25,   50,   188,  1,   true  },
    { "sniper",  "AD-81 SR",     S_SNIPER,  S_RSNIPER,  1950,   1500,   82,  25,     0,   0, 50,   50,    5,   4,  4,  10,  85,   85,   100,  1,   false },
    { "assault", "MTP-57 AR",    S_ASSAULT, S_RASSAULT, 2000,   120,    22,   0,     0,   0, 18,   30,   20,   0,  2,   3,  25,   50,   115,  1,   true  },
    { "cpistol", "nop",          S_PISTOL,  S_RPISTOL,  1400,   120,    19,   0,     0,   0, 35,   10,   15,   6,  5,   6,  35,   50,   125,  1,   false },   // temporary
    { "grenade", "Grenades",     S_NULL,    S_NULL,     1000,   650,    200,  0,    20,   6,  1,    1,   1,    3,  1,   0,   0,    0,    0,   3,   false },
    { "pistol",  "Akimbo",       S_PISTOL,  S_RAKIMBO,  1400,   80,     19,   0,     0,   0, 50,   10,   20,   6,  5,   4,  15,   25,   115,  1,   true  },
};

const char *gunnames[NUMGUNS + 1];

const char *teamnames[] = {"CLA", "RVSF", "CLA-SPECT", "RVSF-SPECT", "SPECTATOR", "", "void"};
const char *teamnames_s[] = {"CLA", "RVSF", "CSPC", "RSPC", "SPEC", "", "void"};

const char *killmessages[2][NUMGUNS] =
{
    { "",        "busted", "picked off", "peppered",   "sprayed", "punctured", "shredded", "busted", "",       "busted" },
    { "slashed", "",       "",           "splattered", "",        "headshot",  "",         "",       "gibbed", ""       }
};

#define C(x) (1<<(SC_##x))
soundcfgitem soundcfg[S_NULL] =
{ //  name                      desc                    vol, loop, rad, key,                 flags
    { "player/jump",            "Jump",                    80, 0,  0, S_JUMP,                   C(MOVEMENT)      }, // 0
    { "player/step",            "Soft landing",            90, 0, 24, S_SOFTLAND,               C(MOVEMENT)      }, // 1
    { "player/land",            "Hard landing",            95, 0, 24, S_HARDLAND,               C(MOVEMENT)      }, // 2
    { "weapon/ric1",            "Ricochet air 1",           0, 0,  0, S_BULLETAIR1,             C(BULLET)        }, // 3
    { "weapon/ric2",            "Ricochet air 2",           0, 0,  0, S_BULLETAIR2,             C(BULLET)        }, // 4
    { "weapon/ric3",            "Ricochet hit",             0, 0,  0, S_BULLETHIT,              C(BULLET)        }, // 5
    { "weapon/waterimpact",     "Bullet (water impact)",    0, 0,  0, S_BULLETWATERHIT,         C(BULLET)        }, // 6
    { "weapon/knife",           "Knife",                    0, 0,  0, S_KNIFE,                  C(WEAPON)        }, // 7
    { "weapon/usp",             "Pistol",                   0, 0,  0, S_PISTOL,                 C(WEAPON)        }, // 8
    { "weapon/pistol_reload",   "Pistol reloading",         0, 0,  0, S_RPISTOL,                C(WEAPON)        }, // 9
    { "weapon/carbine",         "Carbine",                  0, 0,  0, S_CARBINE,                C(WEAPON)        }, // 10
    { "weapon/carbine_reload",  "Carbine reloading",        0, 0,  0, S_RCARBINE,               C(WEAPON)        }, // 11
    { "weapon/shotgun",         "Shotgun",                  0, 0,  0, S_SHOTGUN,                C(WEAPON)        }, // 12
    { "weapon/shotgun_reload",  "Shotgun reloading",        0, 0,  0, S_RSHOTGUN,               C(WEAPON)        }, // 13
    { "weapon/sub",             "Submachine gun",           0, 0,  0, S_SUBGUN,                 C(WEAPON)        }, // 14
    { "weapon/sub_reload",      "Submachine gun reloading", 0, 0,  0, S_RSUBGUN,                C(WEAPON)        }, // 15
    { "weapon/sniper",          "Sniper",                   0, 0,  0, S_SNIPER,                 C(WEAPON)        }, // 16
    { "weapon/sniper_reload",   "Sniper reloading",         0, 0,  0, S_RSNIPER,                C(WEAPON)        }, // 17
    { "weapon/auto",            "Assault rifle",            0, 0,  0, S_ASSAULT,                C(WEAPON)        }, // 18
    { "weapon/auto_reload",     "Assault rifle reloading",  0, 0,  0, S_RASSAULT,               C(WEAPON)        }, // 19
    { "misc/pickup_ammo_clip",  "Ammo pickup",              0, 0,  0, S_ITEMAMMO,               C(PICKUP)        }, // 20
    { "misc/pickup_health",     "Health pickup",            0, 0,  0, S_ITEMHEALTH,             C(PICKUP)        }, // 21
    { "misc/pickup_armour",     "Armour pickup",            0, 0,  0, S_ITEMARMOUR,             C(PICKUP)        }, // 22
    { "misc/tick9",             "Akimbo pickup",            0, 0,  0, S_ITEMAKIMBO,             C(PICKUP)        }, // 23
    { "weapon/clip_empty",      "Empty clip",               0, 0,  0, S_NOAMMO,                 C(WEAPON)        }, // 24
    { "misc/tick11",            "Akimbo finished",          0, 0,  0, S_AKIMBOOUT,              C(PICKUP)        }, // 25
    { "player/pain1",           "Pain 1",                   0, 0,  0, S_PAIN1,                  C(PAIN)          }, // 26
    { "player/pain2",           "Pain 2",                   0, 0,  0, S_PAIN2,                  C(PAIN)          }, // 27
    { "player/pain3",           "Pain 3",                   0, 0,  0, S_PAIN3,                  C(PAIN)          }, // 28
    { "player/pain4",           "Pain 4",                   0, 0,  0, S_PAIN4,                  C(PAIN)          }, // 29
    { "player/pain5",           "Pain 5",                   0, 0,  0, S_PAIN5,                  C(PAIN)          }, // 30
    { "player/pain6",           "Pain 6",                   0, 0,  0, S_PAIN6,                  C(PAIN)          }, // 31
    { "player/die1",            "Die 1",                    0, 0,  0, S_DIE1,                   C(PAIN)          }, // 32
    { "player/die2",            "Die 2",                    0, 0,  0, S_DIE2,                   C(PAIN)          }, // 33
    { "weapon/grenade_exp",     "Grenade explosion",        0, 0,  0, S_FEXPLODE,               C(BULLET)        }, // 34
    { "player/splash1",         "Splash 1",                 0, 0,  0, S_SPLASH1,                C(MOVEMENT)      }, // 35
    { "player/splash2",         "Splash 2",                 0, 0,  0, S_SPLASH2,                C(MOVEMENT)      }, // 36
    { "ctf/flagdrop",           "Flag drop",                0, 0,  0, S_FLAGDROP,               C(OTHER)         }, // 37
    { "ctf/flagpickup",         "Flag pickup",              0, 0,  0, S_FLAGPICKUP,             C(OTHER)         }, // 38
    { "ctf/flagreturn",         "Flag return",              0, 0,  0, S_FLAGRETURN,             C(OTHER)         }, // 39
    { "ctf/flagscore",          "Flag score",               0, 0,  0, S_FLAGSCORE,              C(OTHER)         }, // 40
    { "weapon/grenade_pull",    "Grenade pull",             0, 0,  0, S_GRENADEPULL,            C(WEAPON)        }, // 41
    { "weapon/grenade_throw",   "Grenade throw",            0, 0,  0, S_GRENADETHROW,           C(WEAPON)        }, // 42
    { "weapon/grenade_bounce1", "Grenade bounce 1",         0, 0,  0, S_GRENADEBOUNCE1,         C(WEAPON)        }, // 43
    { "weapon/grenade_bounce2", "Grenade bounce 2",         0, 0,  0, S_GRENADEBOUNCE2,         C(WEAPON)        }, // 44
    { "weapon/pistol_akreload", "Akimbo reload",            0, 0,  0, S_RAKIMBO,                C(WEAPON)        }, // 45
    { "misc/change_gun",        "Change weapon",            0, 0,  0, S_GUNCHANGE,              C(WEAPON)        }, // 46
    { "misc/impact_t",          "Hit sound",                0, 0,  0, S_HITSOUND,               C(BULLET)        }, // 47
    { "player/gib",             "Gib sounds",               0, 0,  0, S_GIB,                    C(PAIN)          }, // 48
    { "misc/headshot",          "Headshot",                10, 0,  0, S_HEADSHOT,               C(OTHER)         }, // 49
    { "vote/vote_call",         "Call vote",                0, 0,  0, S_CALLVOTE,               C(OTHER)         }, // 50
    { "vote/vote_pass",         "Pass vote",                0, 0,  0, S_VOTEPASS,               C(OTHER)         }, // 51
    { "vote/vote_fail",         "Fail vote",                0, 0,  0, S_VOTEFAIL,               C(OTHER)         }, // 52
    { "player/footsteps",       "Footsteps",               40, 1, 20, S_FOOTSTEPS,              C(MOVEMENT)      }, // 53
    { "player/crouch",          "Crouch",                  40, 1, 10, S_FOOTSTEPSCROUCH,        C(MOVEMENT)      }, // 54
    { "player/watersteps",      "Water footsteps",         40, 1, 20, S_WATERFOOTSTEPS,         C(MOVEMENT)      }, // 55
    { "player/watercrouch",     "Water crouching",         40, 1, 10, S_WATERFOOTSTEPSCROUCH,   C(MOVEMENT)      }, // 56
    { "player/crouch_in",       "Crouch-in",               85, 0, 14, S_CROUCH,                 C(MOVEMENT)      }, // 57
    { "player/crouch_out",      "Crouch-out",              75, 0, 14, S_UNCROUCH,               C(MOVEMENT)      }, // 58
    { "misc/menu_click1",       "Menu select",              0, 0,  0, S_MENUSELECT,             C(OTHER)         }, // 59
    { "misc/menu_click2",       "Menu enter",               0, 0,  0, S_MENUENTER,              C(OTHER)         }, // 60
    { "player/underwater",      "Underwater",               0, 0,  0, S_UNDERWATER,             C(MOVEMENT)      }, // 61
    { "misc/tinnitus",          "Tinnitus",                 2, 0,  0, S_TINNITUS,               C(OWNPAIN)       }, // 62
    { "voicecom/affirmative",   "Affirmative",              0, 0,  0, S_AFFIRMATIVE,            C(VOICECOM)|C(TEAM)                                 }, // 63
    { "voicecom/allrightsir",   "All-right sir",            0, 0,  0, S_ALLRIGHTSIR,            C(VOICECOM)|C(TEAM)                                 }, // 64
    { "voicecom/comeonmove",    "Come on, move",            0, 0,  0, S_COMEONMOVE,             C(VOICECOM)|C(TEAM)|C(PUBLIC)|C(FFA)                }, // 65
    { "voicecom/cominginwiththeflag", "Coming in with the flag", 0,0,0, S_COMINGINWITHTHEFLAG,  C(VOICECOM)|C(TEAM)                 |C(FLAGONLY)    }, // 66
    { "voicecom/coverme",       "Cover me",                 0, 0,  0, S_COVERME,                C(VOICECOM)|C(TEAM)                                 }, // 67
    { "voicecom/defendtheflag", "Defend the flag",          0, 0,  0, S_DEFENDTHEFLAG,          C(VOICECOM)|C(TEAM)                 |C(FLAGONLY)    }, // 68
    { "voicecom/enemydown",     "Enemy down",               0, 0,  0, S_ENEMYDOWN,              C(VOICECOM)|C(TEAM)|C(PUBLIC)                       }, // 69
    { "voicecom/gogetemboys",   "Go get 'em boys!",         0, 0,  0, S_GOGETEMBOYS,            C(VOICECOM)|C(TEAM)                                 }, // 70
    { "voicecom/goodjobteam",   "Good job team",            0, 0,  0, S_GOODJOBTEAM,            C(VOICECOM)|C(TEAM)                                 }, // 71
    { "voicecom/igotone",       "I got one!",               0, 0,  0, S_IGOTONE,                C(VOICECOM)|C(TEAM)|C(PUBLIC)                       }, // 72
    { "voicecom/imadecontact",  "I made contact",           0, 0,  0, S_IMADECONTACT,           C(VOICECOM)|C(TEAM)                                 }, // 73
    { "voicecom/imattacking",   "I'm attacking",            0, 0,  0, S_IMATTACKING,            C(VOICECOM)|C(TEAM)                                 }, // 74
    { "voicecom/imondefense",   "I'm on defense",           0, 0,  0, S_IMONDEFENSE,            C(VOICECOM)|C(TEAM)                                 }, // 75
    { "voicecom/imonyourteamman", "I'm on your team",       0, 0,  0, S_IMONYOURTEAMMAN,        C(VOICECOM)|C(TEAM)                                 }, // 76
    { "voicecom/negative",      "Negative",                 0, 0,  0, S_NEGATIVE,               C(VOICECOM)|C(TEAM)                                 }, // 77
    { "voicecom/nocando",       "No can do",                0, 0,  0, S_NOCANDO,                C(VOICECOM)|C(TEAM)                                 }, // 78
    { "voicecom/recovertheflag", "Recover the flag",        0, 0,  0, S_RECOVERTHEFLAG,         C(VOICECOM)|C(TEAM)                 |C(FLAGONLY)    }, // 79
    { "voicecom/sorry",         "Sorry",                    0, 0,  0, S_SORRY,                  C(VOICECOM)|C(TEAM)|C(PUBLIC)|C(FFA)                }, // 80
    { "voicecom/spreadout",     "Spread out",               0, 0,  0, S_SPREADOUT,              C(VOICECOM)|C(TEAM)                                 }, // 81
    { "voicecom/stayhere",      "Stay here",                0, 0,  0, S_STAYHERE,               C(VOICECOM)|C(TEAM)                                 }, // 82
    { "voicecom/staytogether",  "Stay together",            0, 0,  0, S_STAYTOGETHER,           C(VOICECOM)|C(TEAM)                                 }, // 83
    { "voicecom/theresnowaysir", "There's no way sir",      0, 0,  0, S_THERESNOWAYSIR,         C(VOICECOM)|C(TEAM)                                 }, // 84
    { "voicecom/wedidit",       "We did it!",               0, 0,  0, S_WEDIDIT,                C(VOICECOM)|C(TEAM)                                 }, // 85
    { "voicecom/yes",           "Yes",                      0, 0,  0, S_YES,                    C(VOICECOM)|C(TEAM)                                 }, // 86
    { "voicecom/onthemove_1",   "Under way",                0, 0,  0, S_ONTHEMOVE1,             C(VOICECOM)|C(TEAM)                                 }, // 87
    { "voicecom/onthemove_2",   "On the move",              0, 0,  0, S_ONTHEMOVE2,             C(VOICECOM)|C(TEAM)                                 }, // 88
    { "voicecom/igotyourback",  "Got your back",            0, 0,  0, S_GOTURBACK,              C(VOICECOM)|C(TEAM)                                 }, // 89
    { "voicecom/igotyoucovered", "Got you covered",         0, 0,  0, S_GOTUCOVERED,            C(VOICECOM)|C(TEAM)                                 }, // 90
    { "voicecom/inposition_1",  "In position",              0, 0,  0, S_INPOSITION1,            C(VOICECOM)|C(TEAM)                                 }, // 91
    { "voicecom/inposition_2",  "In position now",          0, 0,  0, S_INPOSITION2,            C(VOICECOM)|C(TEAM)                                 }, // 92
    { "voicecom/reportin",      "Report in!",               0, 0,  0, S_REPORTIN,               C(VOICECOM)|C(TEAM)                                 }, // 93
    { "voicecom/niceshot",      "Nice shot",                0, 0,  0, S_NICESHOT,               C(VOICECOM)|C(TEAM)|C(PUBLIC)|C(FFA)                }, // 94
    { "voicecom/thanks_1",      "Thanks",                   0, 0,  0, S_THANKS1,                C(VOICECOM)|C(TEAM)|C(PUBLIC)|C(FFA)                }, // 95
    { "voicecom/thanks_2",      "Thanks, man",              0, 0,  0, S_THANKS2,                C(VOICECOM)|C(TEAM)|C(PUBLIC)|C(FFA)                }, // 96
    { "voicecom/awesome_1",     "Awesome (1)",              0, 0,  0, S_AWESOME1,               C(VOICECOM)|C(TEAM)|C(PUBLIC)|C(FFA)                }, // 97
    { "voicecom/awesome_2",     "Awesome (2)",              0, 0,  0, S_AWESOME2,               C(VOICECOM)|C(TEAM)|C(PUBLIC)|C(FFA)                }, // 98
    { "misc/pickup_helmet",     "Helmet pickup",            0, 0,  0, S_ITEMHELMET,             C(PICKUP)        }, // 99
    { "player/heartbeat",       "Heartbeat",                0, 0,  0, S_HEARTBEAT,              C(OWNPAIN)       }, // 100
    { "ktf/flagscore",          "KTF score",                0, 0,  0, S_KTFSCORE,               C(OTHER)         }, // 101
    { "misc/camera",            "Screenshot",               0, 0,  0, S_CAMERA,                 C(OTHER)         }  // 102
};
#undef C
