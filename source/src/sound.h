// hardcoded sounds, defined in sounds.cfg
enum
{
    S_JUMP = 0,
    S_SOFTLAND, S_HARDLAND,
    S_BULLETAIR1, S_BULLETAIR2, S_BULLETHIT, S_BULLETWATERHIT,
    S_KNIFE, 
    S_PISTOL, S_RPISTOL,
    S_SHOTGUN, S_RSHOTGUN,
    S_SUBGUN, S_RSUBGUN,
    S_SNIPER, S_RSNIPER,
    S_ASSAULT, S_RASSAULT,
    S_ITEMAMMO, S_ITEMHEALTH,
    S_ITEMARMOUR, S_ITEMAKIMBO, 
    S_NOAMMO, S_AKIMBOOUT, 
    S_PAIN1, S_PAIN2, S_PAIN3, S_PAIN4, S_PAIN5, S_PAIN6,
    S_DIE1, S_DIE2, 
    S_FEXPLODE, 
    S_SPLASH1, S_SPLASH2,
    S_FLAGDROP, S_FLAGPICKUP, S_FLAGRETURN, S_FLAGSCORE,
    S_GRENADEPULL, S_GRENADETHROW, S_GRENADEBOUNCE1, S_GRENADEBOUNCE2, S_RAKIMBO,
    S_GUNCHANGE, 
    S_GIB, S_HEADSHOT,
    S_CALLVOTE, S_VOTEPASS, S_VOTEFAIL,
    S_FOOTSTEPS, S_FOOTSTEPSCROUCH, S_WATERFOOTSTEPS, S_WATERFOOTSTEPSCROUCH,
    S_CROUCH, S_UNCROUCH,
    S_MENUSELECT, S_MENUENTER,
    S_UNDERWATER,
    S_TINNITUS,

    S_AFFIRMATIVE,
    S_ALLRIGHTSIR,
    S_COMEONMOVE,
    S_COMINGINWITHTHEFLAG,
    S_COVERME,
    S_DEFENDTHEFLAG,
    S_ENEMYDOWN,
    S_GOGETEMBOYS,
    S_GOODJOBTEAM,
    S_IGOTONE,
    S_IMADECONTACT,
    S_IMATTACKING,
    S_IMONDEFENSE,
    S_IMONYOURTEAMMAN,
    S_NEGATIVE,
    S_NOCANDO,
    S_RECOVERTHEFLAG,
    S_SORRY,
    S_SPREADOUT,
    S_STAYHERE,
    S_STAYTOGETHER,
    S_THERESNOWAYSIR,
    S_WEDIDIT,
    S_YES, 
    S_NICESHOT,
    S_NULL
};

// sound priorities
enum
{
    SP_LOW = 0,
    SP_NORMAL,
    SP_HIGH,
    SP_HIGHEST
};

// hardcoded music
enum 
{
    M_FLAGGRAB = 0,
    M_LASTMINUTE1,
    M_LASTMINUTE2
};

// owner of an OpenAL source, used as callback interface
struct sourceowner
{
    virtual ~sourceowner() {}
    virtual void onsourcereassign(struct source *s) = 0;
};


// represents an OpenAL source, an audio emitter in the 3D world

struct source
{
    ALuint id;

    sourceowner *owner;
    bool locked, valid;
    int priority;

    source();
    ~source();

    void lock();
    void unlock();
    void reset();
    void init(sourceowner *o);
    void onreassign();

    bool generate();
    bool delete_();
    bool buffer(ALuint buf_id);
    bool looping(bool enable);
    bool queuebuffers(ALsizei n, const ALuint *buffer_ids);
    bool unqueueallbuffers();
    bool gain(float g);
    bool pitch(float p);
    bool position(const vec &pos);
    bool position(float x, float y, float z);
    bool velocity(float x, float y, float z);
    vec position();
    bool sourcerelative(bool enable);
    int state();
    bool secoffset(float secs);
    float secoffset();
    bool playing();
    bool play();
    bool stop();
    bool rewind();

    void printposition();
};


// represents an OpenAL sound buffer

struct sbuffer
{
    ALuint id;
    const char *name;

    sbuffer();
    ~sbuffer();

    bool load();
    void unload();
};

// buffer storage

struct bufferhashtable : hashtable<char *, sbuffer>
{
    virtual ~bufferhashtable();
    virtual sbuffer *find(char *name);
};

// manages audio channels

struct sourcescheduler
{
    vector<source *> sources;

    sourcescheduler();

    void init();
    void reset();
    source *newsource(int priority, const vec &o);
    void releasesource(source *src);
};

// audio streaming

struct oggstream : sourceowner
{
    string name;
    bool valid;

    // file stream
    OggVorbis_File oggfile;
    bool isopen;
    vorbis_info *info;
    double totalseconds;
    static const int BUFSIZE = 1024 * 512; // 512kb buffer

    // OpenAL resources
    ALuint bufferids[2];
    source *src;
    ALenum format;

    // settings
    float volume, gain;
    int startmillis, endmillis, startfademillis, endfademillis;
    bool looping;
 
    oggstream();
    ~oggstream(); 

    void reset();
    bool open(const char *f);
    void onsourcereassign(source *s);
    bool stream(ALuint bufid);
    bool update();
    bool playing(); 
    void updategain();
    void setgain(float g);
    void setvolume(float v);
    void fadein(int startmillis, int fademillis);
    void fadeout(int endmillis, int fademillis);
    bool playback(bool looping = false);
    void seek(double offset);
};


struct soundconfig
{
    sbuffer *buf;
    int vol, uses, maxuses;
    bool loop;
    bool muted;

    soundconfig(sbuffer *b, int vol, int maxuses, bool loop);
    void onattach();
    void ondetach();
};


struct worldobjreference
{
    enum worldobjtype { WR_CAMERA, WR_PHYSENT, WR_ENTITY, WR_STATICPOS };
    int type;

    worldobjreference(int t) : type(t) {}
    virtual ~worldobjreference() {}
    virtual worldobjreference *clone() const = 0;
    virtual const vec &currentposition() const = 0;
    virtual bool nodistance() = 0;
    virtual bool operator==(const worldobjreference &other) = 0;
    virtual bool operator!=(const worldobjreference &other) { return !(*this==other); }
    virtual void attach() {}
    virtual void detach() {}
};

struct camerareference : worldobjreference
{
    camerareference();
    worldobjreference *clone() const;
    const vec &currentposition() const;
    bool nodistance();
    bool operator==(const worldobjreference &other);
};

struct physentreference : worldobjreference
{
    struct physent *phys;

    physentreference(physent *ref);
    
    worldobjreference *clone() const;
    const vec &currentposition() const;
    bool nodistance();
    bool operator==(const worldobjreference &other);
};

struct entityreference : worldobjreference
{
    struct entity *ent;

    entityreference(entity *ref);

    worldobjreference *clone() const;
    const vec &currentposition() const;
    bool nodistance();
    bool operator==(const worldobjreference &other);
};

struct staticreference : worldobjreference
{
    vec pos;

    staticreference(const vec &ref);

    worldobjreference *clone() const;
    const vec &currentposition() const;
    bool nodistance();
    bool operator==(const worldobjreference &other);
};


struct location : sourceowner
{
    soundconfig *cfg;
    source *src;
    worldobjreference *ref;

    bool stale;
    int playmillis;

    location(int sound, const worldobjreference &r, int priority = SP_NORMAL);
    ~location();
    void attachworldobjreference(const worldobjreference &r);
    void evaluateworldobjref();
    void onsourcereassign(source *s);
    void updatepos();
    void update();
    void play(bool loop = false);
    void pitch(float p);
    void offset(float secs);
    float offset();
    void drop();
};

struct locvector : vector<location *>
{
    virtual ~locvector() {} 

    location *find(int sound, worldobjreference *ref/* = NULL*/, const vector<soundconfig> &soundcollection /* = gamesounds*/);
    void delete_(int i);
    void replaceworldobjreference(const worldobjreference &oldr, const worldobjreference &newr);
    void updatelocations();
    void forcepitch(float pitch);
    void deleteworldobjsounds();
};

// audio interface to the engine

struct audiomanager
{
    bool nosound;
    float currentpitch;
    ALCdevice *device;
    ALCcontext *context;

    bufferhashtable bufferpool;
    locvector locations;
    //sourcescheduler scheduler;
    oggstream *gamemusic;

    audiomanager();

    location *_playsound(int n, const worldobjreference &r, int priority, float offset = 0.0f, bool loop = false);
    void playsound(int n, int priority = SP_NORMAL);
    void playsound(int n, struct physent *p, int priority = SP_NORMAL);
    void playsound(int n, struct entity *e, int priority = SP_NORMAL);
    void playsound(int n, const vec *v, int priority = SP_NORMAL);
    void playsoundname(char *s, const vec *loc, int vol);
    void sound(int n);
    void playsoundc(int n, physent *p = NULL);
    void stopsound();

    void initsound();
    void soundcleanup();
    void mapsoundreset();
    void clearworldsounds(bool fullclean = true);

    void music(char *name, char *millis, char *cmd);
    void musicsuggest(int id, int millis, bool rndofs);
    void musicfadeout(int id);
    void registermusic(char *name);
    int addsound(char *name, int vol, int maxuses, bool loop, vector<soundconfig> &sounds, bool load);

    void updateplayerfootsteps(struct playerent *p);
    location *updateloopsound(int sound, bool active, float vol = 1.0f);
    void updateaudio();

    void detachsounds(struct playerent *owner);
    void preloadmapsound(entity &e);
    void preloadmapsounds();
    void applymapsoundchanges();
    void writesoundconfig(FILE *f);
};

void alclearerr();
bool alerr(bool msg = true, int line = 0);
#define ALERR alerr(true, __LINE__)

extern vector<soundconfig> gamesounds, mapsounds;
extern ov_callbacks oggcallbacks;
extern int soundvol;
extern int audiodebug;

extern sourcescheduler scheduler; // FIXME
extern audiomanager audiomgr;


