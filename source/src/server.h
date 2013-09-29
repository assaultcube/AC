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
    "SV_EDITMODE", "SV_EDITH", "SV_EDITT", "SV_EDITS", "SV_EDITD", "SV_EDITE", "SV_NEWMAP",
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

const char *entnames[MAXENTTYPES] =
{
    "none?",
    "light", "playerstart", "pistol", "ammobox","grenades",
    "health", "helmet", "armour", "akimbo",
    "mapmodel", "trigger", "ladder", "ctf-flag", "sound", "clip", "plclip"
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
    {25, 0, 100, S_ITEMHELMET}, // 1 helmet
    {50, 0, 100, S_ITEMARMOUR}, // 2 armour
};

guninfo guns[NUMGUNS] =
{
    // Please update ./ac_website/htdocs/docs/introduction.html if these figures change.
    //mKR: mdl_kick_rot && mKB: mdl_kick_back
    //reI: recoilincrease && reB: recoilbase && reM: maxrecoil && reF: recoilbackfade
    //pFX: pushfactor
    //modelname                   reload       attackdelay      piercing     part     recoil       mKR       reI          reM           pFX
    //              sound                reloadtime        damage    projspeed  spread     magsize     mKB        reB             reF           isauto
    { "knife",      S_KNIFE,      S_NULL,     0,      500,    50, 100,     0,   0,  1,    1,   1,    0,  0,   0,   0,      0,      0,   1,      false },
    { "pistol",     S_PISTOL,     S_RPISTOL,  1400,   160,    18,   0,     0,   0, 53,   10,   10,   6,  5,   6,  35,     58,     125,  1,      false },
    { "carbine",    S_CARBINE,    S_RCARBINE, 1800,   720,    60,  40,     0,   0, 10,   60,   10,   4,  4,  10,  60,     60,     150,  1,      false },
    { "shotgun",    S_SHOTGUN,    S_RSHOTGUN, 2400,   880,    1,    0,     0,   0,  1,   35,    7,   9,  9,  10, 140,    140,    125,   1,      false },   // CAUTION dmg only sane for server!
    { "subgun",     S_SUBGUN,     S_RSUBGUN,  1650,   80,     16,   0,     0,   0, 45,   15,   30,   1,  2,   5,  25,     50,     188,  1,      true  },
    { "sniper",     S_SNIPER,     S_RSNIPER,  1950,   1500,   82,  25,     0,   0, 50,   50,    5,   4,  4,  10,  85,     85,     100,  1,      false },
    { "assault",    S_ASSAULT,    S_RASSAULT, 2000,   120,    22,   0,     0,   0, 18,   30,   20,   0,  2,   3,  25,     50,     115,  1,      true  },
    { "cpistol",    S_PISTOL,     S_RPISTOL,  1400,   120,    19,   0,     0,   0, 35,   10,   15,   6,  5,   6,  35,     50,     125,  1,      false },   // temporary
    { "grenade",    S_NULL,       S_NULL,     1000,   650,    200,  0,    20,  6,  1,    1,   1,    3,   1,    0,   0,      0,      0,   3,      false },
    { "pistol",     S_PISTOL,     S_RAKIMBO,  1400,   80,     19,   0,     0,   0, 50,   10,   20,   6,  5,   4,  15,     25,     115,  1,      true  },
};

const char *teamnames[TEAM_NUM+1] = {"CLA", "RVSF", "CLA-SPECT", "RVSF-SPECT", "SPECTATOR", "void"};
const char *teamnames_s[TEAM_NUM+1] = {"CLA", "RVSF", "CSPC", "RSPC", "SPEC", "void"};

// for both client and server
// default messages are hardcoded !
char killmessages[2][NUMGUNS][MAXKILLMSGLEN] = {{ "", "busted", "picked off", "peppered", "sprayed", "punctured", "shredded", "busted", "", "busted" }, { "slashed", "", "", "splattered", "", "headshot", "", "", "gibbed", "" }};
