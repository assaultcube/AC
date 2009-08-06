// server.h

#define gamemode smode   // allows the gamemode macros to work with the server mode

#define SERVER_PROTOCOL_VERSION    (PROTOCOL_VERSION)    // server without any gameplay modification
//#define SERVER_PROTOCOL_VERSION   (-PROTOCOL_VERSION)  // server with gameplay modification but compatible to vanilla client (using /modconnect)
//#define SERVER_PROTOCOL_VERSION  (PROTOCOL_VERSION)    // server with incompatible protocol (change PROTOCOL_VERSION in file protocol.h to a negative number!)

#define valid_flag(f) (f >= 0 && f < 2)

enum { GE_NONE = 0, GE_SHOT, GE_EXPLODE, GE_HIT, GE_AKIMBO, GE_RELOAD, GE_SCOPING, GE_SUICIDE, GE_PICKUP };
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

struct scopeevent
{
    int type;
    int millis, id;
    bool scoped;
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
    scopeevent scoping;
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
    int lastdeath, lastspawn, lifesequence;
    int lastshot;
    projectilestate<8> grenades;
    int akimbomillis;
    bool scoped;
    int flagscore, frags, teamkills, deaths, shotdamage, damage;

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
        scoped = false;
        flagscore = frags = teamkills = deaths = shotdamage = damage = 0;
        respawn();
    }

    void respawn()
    {
        playerstate::respawn();
        o = vec(-1e10f, -1e10f, -1e10f);
        lastdeath = 0;
        lastspawn = -1;
        lastshot = 0;
        akimbomillis = 0;
        scoped = false;
    }
};

struct savedscore
{
    string name;
    uint ip;
    int frags, flagscore, deaths, teamkills, shotdamage, damage, team;
    bool valid;

    void save(clientstate &cs, int t)
    {
        frags = cs.frags;
        flagscore = cs.flagscore;
        deaths = cs.deaths;
        teamkills = cs.teamkills;
        shotdamage = cs.shotdamage;
        damage = cs.damage;
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
    int connectmillis;
    int acversion, acbuildtype;
    bool isauthed; // for passworded servers
    bool haswelcome;
    bool isonrightmap, loggedwrongmap, freshgame;
    bool timesync;
    int gameoffset, lastevent, lastvotecall;
    int demoflags;
    clientstate state;
    int spectcn;
    vector<gameevent> events;
    vector<uchar> position, messages;
    string lastsaytext;
    int saychars, lastsay, spamcount;
    int at3_score, at3_lastforce;
    bool at3_dontmove;
    int spawnindex;
    int spawnperm, spawnpermsent;
    int salt;
    string pwd;
    int mapcollisions, farpickups;
    enet_uint32 bottomRTT;

    gameevent &addevent()
    {
        static gameevent dummy;
        if(events.length()>100) return dummy;
        return events.add();
    }

    void mapchange(bool getmap = false)
    {
        state.reset();
        events.setsizenodelete(0);
        timesync = false;
        isonrightmap = false;
        spawnperm = SP_WRONGMAP;
        spawnpermsent = servmillis;
        if(!getmap)
        {
            loggedwrongmap = false;
            freshgame = true;         // permission to spawn at once
        }
        lastevent = 0;
        at3_lastforce = 0;
        mapcollisions = farpickups = 0;
    }

    void reset()
    {
        name[0] = demoflags = 0;
        bottomRTT = ping = 9999;
        team = TEAM_SPECT;
        loopi(2) skin[i] = 0;
        position.setsizenodelete(0);
        messages.setsizenodelete(0);
        isauthed = haswelcome = false;
        role = CR_DEFAULT;
        lastvotecall = 0;
        vote = VOTE_NEUTRAL;
        lastsaytext[0] = '\0';
        saychars = 0;
        spawnindex = -1;
        spectcn = FPCN_VOID;
        mapchange();
        freshgame = false;         // don't spawn into running games
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
    int millis;
};

struct worldstate
{
    enet_uint32 uses;
    vector<uchar> positions, messages;
};

struct server_entity            // server side version of "entity" type
{
    int type;
    bool spawned, legalpickup;
    int spawntime;
    short x, y;
};

struct demofile
{
    string info;
    string file;
    uchar *data;
    int len;
};

void startgame(const char *newname, int newmode, int newtime = -1, bool notify = true);
void disconnect_client(int n, int reason = -1);
int clienthasflag(int cn);
bool refillteams(bool now = false, int ftr = FTR_AUTOTEAM);
void changeclientrole(int client, int role, char *pwd = NULL, bool force=false);
mapstats *getservermapstats(const char *mapname, bool getlayout = false, int *maploc = NULL);
int findmappath(const char *mapname, char *filename = NULL);
int calcscores();
void recordpacket(int chan, void *data, int len);
void senddisconnectedscores(int cn);
void process(ENetPacket *packet, int sender, int chan);
void welcomepacket(ucharbuf &p, int n, ENetPacket *packet);
void sendwelcome(client *cl, int chan = 1);
int numclients();
bool updateclientteam(int cln, int newteam, int ftr);
void forcedeath(client *cl);
void sendf(int cn, int chan, const char *format, ...);

extern bool isdedicated;
extern string smapname;
extern mapstats smapstats;
extern char *maplayout;

