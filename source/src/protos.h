// protos for ALL external functions in cube...

#ifndef STANDALONE

extern bool hasTE, hasMT, hasMDA, hasDRE, hasstencil, hasST2, hasSTW, hasSTS;

// GL_ARB_multitexture
extern PFNGLACTIVETEXTUREARBPROC       glActiveTexture_;
extern PFNGLCLIENTACTIVETEXTUREARBPROC glClientActiveTexture_;
extern PFNGLMULTITEXCOORD2FARBPROC     glMultiTexCoord2f_;
extern PFNGLMULTITEXCOORD3FARBPROC     glMultiTexCoord3f_;

// GL_EXT_multi_draw_arrays
extern PFNGLMULTIDRAWARRAYSEXTPROC   glMultiDrawArrays_;
extern PFNGLMULTIDRAWELEMENTSEXTPROC glMultiDrawElements_;

// GL_EXT_draw_range_elements
extern PFNGLDRAWRANGEELEMENTSEXTPROC glDrawRangeElements_;

// GL_EXT_stencil_two_side
extern PFNGLACTIVESTENCILFACEEXTPROC glActiveStencilFace_;

// GL_ATI_separate_stencil
extern PFNGLSTENCILOPSEPARATEATIPROC   glStencilOpSeparate_;
extern PFNGLSTENCILFUNCSEPARATEATIPROC glStencilFuncSeparate_;

struct color
{
    float r, g, b, alpha;
    color(){}
    color(float r, float g, float b) : r(r), g(g), b(b), alpha(1.0f) {}
    color(float r, float g, float b, float a) : r(r), g(g), b(b), alpha(a) {}
};

// command
extern bool persistidents;
extern int variable(const char *name, int min, int cur, int max, int *storage, void (*fun)(), bool persist);
extern float fvariable(const char *name, float cur, float *storage, void (*fun)(), bool persist);
extern char *svariable(const char *name, const char *cur, char **storage, void (*fun)(), bool persist);
extern void setvar(const char *name, int i, bool dofunc = false);
extern void setfvar(const char *name, float f, bool dofunc = false);
extern void setsvar(const char *name, const char *str, bool dofunc = false);
extern int getvar(const char *name);
extern bool identexists(const char *name);
extern bool addcommand(const char *name, void (*fun)(), int narg);
extern int execute(const char *p);
extern char *executeret(const char *p);
extern void intret(int v);
extern void result(const char *s);
extern void exec(const char *cfgfile);
extern bool execfile(const char *cfgfile);
extern void resetcomplete();
extern void complete(char *s);
extern void push(const char *name, const char *action);
extern void pop(const char *name);
extern void alias(const char *name, const char *action);
extern const char *getalias(const char *name);
extern void writecfg();
extern void deletecfg();
extern void identnames(vector<const char *> &names, bool builtinonly);
extern void changescriptcontext(int newcontext);
extern void explodelist(const char *s, vector<char *> &elems);
extern char *indexlist(const char *s, int pos);
extern char *parseword(const char *&p);
extern void pushscontext(int newcontext);
extern int popscontext();
extern int curscontext();
extern int execcontext;

// console
extern void keypress(int code, bool isdown, int cooked, SDLMod mod = KMOD_NONE);
extern int rendercommand(int x, int y, int w);
extern void renderconsole();
extern char *getcurcommand();
extern char *addreleaseaction(const char *s);
extern void writebinds(FILE *f);
extern void pasteconsole(char *dst);
extern void conoutf(const char *s, ...);

struct keym
{
    int code;
    char *name, *action;
    bool pressed;

    keym() : code(-1), name(NULL), action(NULL), pressed(false) {}
    ~keym() { DELETEA(name); DELETEA(action); }
};

extern bool bindkey(keym *km, const char *action);
extern keym *findbinda(const char *action);
extern bool bindc(int code, const char *action);

// menus
extern void rendermenu();
extern bool menuvisible();
extern void menureset(void *menu);
extern void menumanual(void *menu, char *text, char *action = NULL, color *bgcolor = NULL, const char *desc = NULL);
extern void menuheader(void *menu, char *header = NULL, char *footer = NULL);
extern bool menukey(int code, bool isdown, int unicode, SDLMod mod = KMOD_NONE);
extern void *addmenu(const char *name, const char *title = NULL, bool allowinput = true, void (__cdecl *refreshfunc)(void *, bool) = NULL, bool hotkeys = false, bool forwardkeys = false);
extern void rendermenumdl();
extern void menuset(void *m, bool save = true);
extern void menuselect(void *menu, int sel);
extern void showmenu(const char *name, bool top = true);
extern void closemenu(const char *name);
extern void addchange(const char *desc, int type);
extern void clearchanges(int type);
extern void refreshapplymenu(void *menu, bool init);

struct mitem
{
    struct gmenu *parent;
    color *bgcolor;

    mitem(gmenu *parent, color *bgcolor) : parent(parent), bgcolor(bgcolor) {}
    virtual ~mitem() {}

    virtual void render(int x, int y, int w);
    virtual int width() = 0;
    virtual void select() {}
    virtual void focus(bool on) { }
    virtual void key(int code, bool isdown, int unicode) { }
    virtual void init() {}
    virtual const char *getdesc() { return NULL; }
    bool isselection();
    void renderbg(int x, int y, int w, color *c);
    static color gray, white, whitepulse;
};

struct mdirlist
{
    char *dir, *ext, *action;
    ~mdirlist()
    {
        DELETEA(dir);
        DELETEA(ext);
        DELETEA(action);
    }
};

struct gmenu
{
    const char *name, *title, *header, *footer;
    vector<mitem *> items;
    int mwidth;
    int menusel;
    bool allowinput, inited, hotkeys, forwardkeys;
    void (__cdecl *refreshfunc)(void *, bool);
    char *initaction;

    const char *mdl;
    int anim, rotspeed, scale;
    mdirlist *dirlist;

    gmenu() : name(0), title(0), header(0), footer(0), initaction(0), mdl(0), dirlist(0) {}
    virtual ~gmenu()
    {
        DELETEA(name);
        DELETEA(title);
        DELETEA(mdl);
        DELETEP(dirlist);
        DELETEA(initaction);
        items.deletecontentsp();
    }

    void render();
    void renderbg(int x1, int y1, int x2, int y2, bool border);
    void refresh();
    void open();
    void close();
    void init();
};

// serverbrowser
extern void addserver(char *servername, char *serverport);
extern char *getservername(int n);
extern bool resolverwait(const char *name, ENetAddress *address);
extern int connectwithtimeout(ENetSocket sock, const char *hostname, ENetAddress &remoteaddress);
extern void writeservercfg();
extern void refreshservers(void *menu, bool init);

struct serverinfo
{
    enum { UNRESOLVED = 0, RESOLVING, RESOLVED };

    string name;
    string full;
    string map;
    string sdesc;
    string cmd;
    int mode, numplayers, maxclients, ping, protocol, minremain, resolved, port, lastpingmillis;
    ENetAddress address;

    serverinfo()
     : mode(0), numplayers(0), maxclients(0), ping(9999), protocol(0), minremain(0), resolved(UNRESOLVED), port(-1), lastpingmillis(0)
    {
        name[0] = full[0] = map[0] = sdesc[0] = '\0';
    }
};

extern serverinfo *getconnectedserverinfo();
extern void pingservers();

// rendergl
extern glmatrixf mvmatrix, projmatrix, mvpmatrix, invmvmatrix, invmvpmatrix;

extern void gl_checkextensions();
extern void gl_init(int w, int h, int bpp, int depth, int fsaa);
extern void cleangl();
extern void enablepolygonoffset(GLenum type);
extern void disablepolygonoffset(GLenum type, bool restore = true);
extern void line(int x1, int y1, float z1, int x2, int y2, float z2);
extern void line(int x1, int y1, int x2, int y2, color *c = NULL);
extern void box(block &b, float z1, float z2, float z3, float z4);
extern void dot(int x, int y, float z);
extern void linestyle(float width, int r, int g, int b);
extern void blendbox(int x1, int y1, int x2, int y2, bool border, int tex = -1, color *c = NULL);
extern void quad(GLuint tex, float x, float y, float s, float tx, float ty, float tsx, float tsy = 0);
extern void quad(GLuint tex, vec &c1, vec &c2, float tx, float ty, float tsx, float tsy);
extern void circle(GLuint tex, float x, float y, float r, float tx, float ty, float tr, int subdiv = 32);
extern void sethudgunperspective(bool on);
extern void gl_drawframe(int w, int h, float changelod, float curfps);
extern void clearminimap();
extern void rendercursor(int x, int y, int w);
extern void renderaboveheadicon(playerent *p);
extern void drawcrosshair(playerent *p, bool showteamwarning);
extern void drawscope();
extern float dynfov();

// shadow
extern bool addshadowbox(const vec &bbmin, const vec &bbmax, const vec &extrude, const glmatrixf &mat);
extern void drawstencilshadows();

// texture
struct Texture
{
    char *name;
    int xs, ys, bpp, clamp;
    GLuint id;
};
extern Texture *notexture, *noworldtexture;

extern void createtexture(int tnum, int w, int h, void *pixels, int clamp, bool mipmap, GLenum format);
extern Texture *textureload(const char *name, int clamp = 0);
extern Texture *lookuptexture(int tex, Texture *failtex = notexture);
extern bool reloadtexture(Texture &t);
extern bool reloadtexture(const char *name);
extern void reloadtextures();

static inline Texture *lookupworldtexture(int tex)
{ return lookuptexture(tex, noworldtexture); }

extern float skyfloor;
extern void draw_envbox(int fogdist);

extern int maxtmus;
extern void inittmus();
extern void resettmu(int n);
extern void scaletmu(int n, int rgbscale, int alphascale = 0);
extern void colortmu(int n, float r = 0, float g = 0, float b = 0, float a = 0);
extern void setuptmu(int n, const char *rgbfunc = NULL, const char *alphafunc = NULL);

// rendercubes
extern void mipstats(int a, int b, int c);
extern void render_flat(int tex, int x, int y, int size, int h, sqr *l1, sqr *l2, sqr *l3, sqr *l4, bool isceil);
extern void render_flatdelta(int wtex, int x, int y, int size, float h1, float h2, float h3, float h4, sqr *l1, sqr *l2, sqr *l3, sqr *l4, bool isceil);
extern void render_square(int wtex, float floor1, float floor2, float ceil1, float ceil2, int x1, int y1, int x2, int y2, int size, sqr *l1, sqr *l2, bool topleft);
extern void render_tris(int x, int y, int size, bool topleft, sqr *h1, sqr *h2, sqr *s, sqr *t, sqr *u, sqr *v);
extern void resetcubes();
extern void setupstrips();
extern void renderstripssky();
extern void renderstrips();

// water
extern void setwatercolor(const char *r = "", const char *g = "", const char *b = "", const char *a = "");
extern void calcwaterscissor();
extern void addwaterquad(int x, int y, int size);
extern int renderwater(float hf, GLuint reflecttex, GLuint refracttex);
extern void resetwater();

// client
extern void connects(char *servername, char *serverport = NULL, char *password = NULL);
extern void abortconnect();
extern void disconnect(int onlyclean = 0, int async = 0);
extern void cleanupclient();
extern void toserver(char *text);
extern void addmsg(int type, const char *fmt = NULL, ...);
extern bool multiplayer(bool msg = true);
extern bool allowedittoggle();
extern void sendpackettoserv(int chan, ENetPacket *packet);
extern void gets2c();
extern void c2sinfo(playerent *d);
extern void c2skeepalive();
extern void neterr(const char *s);
extern int getclientnum();
extern void changemapserv(char *name, int mode, bool download = false);
extern void changeteam(int team, bool respawn = true);
extern void getmap();
extern void newteam(char *name);
extern bool securemapcheck(char *map, bool msg = true);
extern void sendintro();
extern void getdemo(int i);
extern void listdemos();

// clientgame
extern flaginfo flaginfos[2];
extern bool autoteambalance;
extern void updateworld(int curtime, int lastmillis);
extern void resetmap();
extern void startmap(const char *name);
extern void changemap(const char *name);
extern void initclient();
extern void deathstate(playerent *pl);
extern void spawnplayer(playerent *d);
extern void dodamage(int damage, playerent *pl, playerent *actor, bool gib = false, bool local = true);
extern void dokill(playerent *pl, playerent *act, bool gib = false);
extern playerent *newplayerent();
extern botent *newbotent();
extern void freebotent(botent *d);
extern char *getclientmap();
extern void zapplayer(playerent *&d);
extern playerent *getclient(int cn);
extern playerent *newclient(int cn);
extern void timeupdate(int timeremain);
extern void respawnself();
extern void setskin(playerent *pl, uint skin);
extern void callvote(int type, char *arg1 = NULL, char *arg2 = NULL);
extern void resetsleep();
//game mode extras
extern void flagpickup(int fln);
extern void tryflagdrop(bool manual = false);
extern void flagreturn(int fln);
extern void flagscore(int fln);
extern void flagstolen(int flag, int act);
extern void flagdropped(int flag, short x, short y, short z);
extern void flaginbase(int flag);
extern void flagidle(int flag);
extern void flagmsg(int flag, int message, int actor, int flagtime);
extern void arenarespawn();
extern bool tryrespawn();
extern void findplayerstart(playerent *d, bool mapcenter=false, int arenaspawn=-1);
extern void serveropcommand(int cmd, int arg1);
extern void refreshsopmenu(void *menu, bool init);
extern char *colorname(playerent *d, int num = 0, char *name = NULL, const char *prefix = "");
extern char *colorping(int ping);
extern void togglespect();
extern playerent *updatefollowplayer(int shiftdirection = 0);
extern void spectate(int mode);

struct votedisplayinfo
{
    playerent *owner;
    int type, stats[VOTE_NUM], result, millis;
    string desc;
    bool localplayervoted;
    votedisplayinfo() : owner(NULL), result(VOTE_NEUTRAL), millis(0), localplayervoted(false) { loopi(VOTE_NUM) stats[i] = VOTE_NEUTRAL; }
};

extern votedisplayinfo *newvotedisplayinfo(playerent *owner, int type, char *arg1, char *arg2);
extern void callvotesuc();
extern void callvoteerr(int e);
extern void displayvote(votedisplayinfo *v);
extern void voteresult(int v);
extern void votecount(int v);
extern void clearvote();

// scoreboard
extern void showscores(bool on);
extern void renderscores(void *menu, bool init);
extern void consolescores();

// world
extern void setupworld(int factor);
extern void empty_world(int factor, bool force);
extern void remip(const block &b, int level = 0);
extern void remipmore(const block &b, int level = 0);
extern int closestent();
extern int findentity(int type, int index = 0);
extern int findentity(int type, int index, uchar attr2);
extern entity *newentity(int x, int y, int z, char *what, int v1, int v2, int v3, int v4);

// worldlight
extern int lastcalclight;

extern void fullbrightlight(int level);
extern void calclight();
extern void adddynlight(physent *owner, const vec &o, int reach, int expire, int fade, uchar r, uchar g = 0, uchar b = 0);
extern void dodynlights();
extern void undodynlights();
extern void cleardynlights();
extern void removedynlights(physent *owner);
extern block *blockcopy(const block &b);
extern void blockpaste(const block &b);
extern void freeblock(block *&b);

// worldrender
extern void render_world(float vx, float vy, float vh, float changelod, int yaw, int pitch, float fov, float fovy, int w, int h);
extern int lod_factor();

// worldocull
extern void computeraytable(float vx, float vy, float fov);
extern int isoccluded(float vx, float vy, float cx, float cy, float csize);

// main
extern SDL_Surface *screen;
extern int colorbits, depthbits, stencilbits;

extern time_t now_utc;
extern struct tm *systemtime();

extern void keyrepeat(bool on);
extern bool firstrun;

enum
{
    NOT_INITING = 0,
    INIT_LOAD,
    INIT_RESET
};
enum
{
    CHANGE_GFX   = 1<<0,
    CHANGE_SOUND = 1<<1
};
extern bool initwarning(const char *desc, int level = INIT_RESET, int type = CHANGE_GFX);

// rendertext
struct font
{
    struct charinfo
    {
        short x, y, w, h;
    };

    char *name;
    Texture *tex;
    vector<charinfo> chars;
    short defaultw, defaulth;
    short offsetx, offsety, offsetw, offseth;
};

#define VIRTH 1800
#define FONTH (curfont->defaulth)
#define PIXELTAB (8*curfont->defaultw)

extern int VIRTW; // virtual screen size for text & HUD
extern font *curfont;

extern bool setfont(const char *name);
extern void draw_text(const char *str, int left, int top, int r = 255, int g = 255, int b = 255, int a = 255, int cursor = -1, int maxwidth = -1);
extern void draw_textf(const char *fstr, int left, int top, ...);
extern int text_width(const char *str);
extern int text_visible(const char *str, int max);
extern void text_bounds(const char *str, int &width, int &height, int maxwidth = -1);
extern int text_visible(const char *str, int hitx, int hity, int maxwidth);
extern void text_pos(const char *str, int cursor, int &cx, int &cy, int maxwidth);
extern void text_startcolumns();
extern void text_endcolumns();

// editing
extern void cursorupdate();
extern void toggleedit();
extern char *editinfo();
extern void editdrag(bool isdown);
extern void setvdeltaxy(int delta, block &sel);
extern void editequalisexy(bool isfloor, block &sel);
extern void edittypexy(int type, block &sel);
extern void edittexxy(int type, int t, block &sel);
extern void editheightxy(bool isfloor, int amount, block &sel);
extern bool noteditmode();
extern void pruneundos(int maxremain = 0);

// renderhud
enum
{
    HUDMSG_INFO = 0,
    HUDMSG_TIMER,

    HUDMSG_TYPE = 0xFF,
    HUDMSG_OVERWRITE = 1<<8
};
extern void gl_drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater);
extern void loadingscreen(const char *fmt = NULL, ...);
extern void hudoutf(const char *s, ...);
extern void hudonlyf(const char *s, ...);
extern void hudeditf(int type, const char *s, ...);
extern void show_out_of_renderloop_progress(float bar1, const char *text1, float bar2 = 0, const char *text2 = NULL);
extern void updatedmgindicator(vec &attack);

// renderparticles
enum
{
    PT_PART = 0,
    PT_FIREBALL,
    PT_SHOTLINE,
    PT_DECAL,
    PT_BULLETHOLE,
    PT_BLOOD,
    PT_STAIN,
    PT_FLASH,
    PT_HUDFLASH
};

#define PT_DECAL_MASK ((1<<PT_DECAL)|(1<<PT_BULLETHOLE)|(1<<PT_STAIN))

extern void particleinit();
extern void particlereset();
extern void particle_flash(int type, float scale, float angle, const vec &p);
extern void particle_splash(int type, int num, int fade, const vec &p);
extern void particle_trail(int type, int fade, const vec &from, const vec &to);
extern void particle_emit(int type, int *args, int basetime, int seed, const vec &p);
extern void particle_fireball(int type, const vec &o);
extern void addshotline(dynent *d, const vec &from, const vec &to);
extern bool addbullethole(dynent *d, const vec &from, const vec &to, float radius = 1, bool noisy = true);
extern bool addscorchmark(vec &o, float radius = 7);

extern void render_particles(int time, int typemask = ~0);

// worldio
extern void save_world(char *fname);
extern bool load_world(char *mname);
extern void writemap(char *name, int size, uchar *data);
extern void writecfggz(char *name, int size, int sizegz, uchar *data);
extern uchar *readmap(char *name, int *size);
extern uchar *readmcfggz(char *name, int *size, int *sizegz);

// physics
extern float raycube(const vec &o, const vec &ray, vec &surface);
extern bool raycubelos(const vec &from, const vec &to, float margin = 0);
extern void moveplayer(physent *pl, int moveres, bool local);
extern void moveplayer(physent *pl, int moveres, bool local, int curtime);
extern void movebounceent(bounceent *p, int moveres, bool local);
extern bool collide(physent *d, bool spawn, float drop, float rise);
extern void entinmap(physent *d);
extern void physicsframe();
extern void mousemove(int dx, int dy);
extern void fixcamerarange(physent *cam = camera1);
extern float floor(short x, short y);
extern void updatecrouch(playerent *p, bool on);
extern bool objcollide(physent *d, vec &objpos, float objrad, float objheight);
extern bool collide(physent *d, bool spawn, float drop, float rise);

// sound
extern void playsound(int n, int priority = SP_NORMAL);
extern void playsound(int n, physent *p, int priority = SP_NORMAL);
extern void playsound(int n, entity *e, int priority = SP_NORMAL);
extern void playsound(int n, const vec *v, int priority = SP_NORMAL);
extern void playsoundc(int n, physent *p = NULL);
extern void initsound();
extern void soundcleanup();
extern void musicsuggest(int id, int millis = 0, bool rndofs = false);
extern void musicfadeout(int id);
extern void clearworldsounds();
extern void detachsounds(playerent *owner);
extern void updateaudio();
extern void preloadmapsound(entity &e);
extern void preloadmapsounds();

// rendermodel
extern void rendermodel(const char *mdl, int anim, int tex, float rad, const vec &o, float yaw, float pitch, float speed = 0, int basetime = 0, playerent *d = NULL, modelattach *a = NULL, float scale = 1.0f);
extern void startmodelbatches();
extern void endmodelbatches(bool flush = true);
extern void clearmodelbatches();
extern mapmodelinfo &getmminfo(int i);
extern int findanim(const char *name);
extern void loadskin(const char *dir, const char *altdir, Texture *&skin);
extern model *loadmodel(const char *name, int i = -1);
extern void preload_playermodels();
extern void preload_entmodels();
extern void preload_hudguns();
extern void preload_mapmodels();
extern void renderclients();
extern void renderclient(playerent *d);
extern void renderclient(playerent *d, const char *mdlname, const char *vwepname, int tex = 0);

// weapon
extern void shoot(playerent *d, vec &to);
extern void createrays(vec &from, vec &to);
extern void removebounceents(playerent *owner);
extern void movebounceents();
extern void clearbounceents();
extern void renderbounceents();
extern void addgib(playerent *d);
extern playerent *playerincrosshair();
extern int magsize(int gun);
extern void checkweaponswitch();
extern void setscope(bool activate);
extern int intersect(playerent *d, const vec &from, const vec &to, vec *end = NULL);
extern bool intersect(entity *e, const vec &from, const vec &to, vec *end = NULL);
extern void damageeffect(int damage, playerent *d);
extern void tryreload(playerent *p);
extern void checkakimbo();
extern struct projectile *newprojectile(vec &from, vec &to, float speed, bool local, playerent *owner, int gun, int id = lastmillis);

// entities
extern const char *entnames[];

extern void putitems(ucharbuf &p);
extern void pickupeffects(int n, playerent *d);
extern void renderentities();
extern void resetspawns();
extern void setspawn(int i, bool on);
extern void checkitems(playerent *d);

// rndmap
extern void perlinarea(block &b, int scale, int seed, int psize);

// doc
extern void renderdoc(int x, int y, int doch);
extern void renderdocmenu(void *menu, bool init);
extern void toggledoc();
extern void scrolldoc(int i);
#endif

// server
extern int modeacronyms;
extern void localservertoclient(int chan, uchar *buf, int len);
extern const char *modestr(int n, bool acronyms = false);
extern const char *voteerrorstr(int n);
extern void fatal(const char *s, ...);
extern void initserver(bool dedicated, int uprate, const char *sdesc, const char *sdesc_pre, const char *sdesc_suf, const char *ip, int serverport, const char *master, const char *passwd, int maxcl, const char *maprot, const char *adminpwd, const char *pwdfile, const char *blfile, const char *srvmsg, int scthreshold);
extern void cleanupserver();
extern void localconnect();
extern void localdisconnect();
extern void localclienttoserver(int chan, ENetPacket *);
extern void serverslice(uint timeout);
extern void putint(ucharbuf &p, int n);
extern int getint(ucharbuf &p);
extern void putuint(ucharbuf &p, int n);
extern int getuint(ucharbuf &p);
extern void sendstring(const char *t, ucharbuf &p);
extern void getstring(char *t, ucharbuf &p, int len = MAXTRANS);
extern void filtertext(char *dst, const char *src, int whitespace = 1, int len = sizeof(string)-1);
extern void startintermission();
extern void restoreserverstate(vector<entity> &ents);
extern uchar *retrieveservers(uchar *buf, int buflen);
extern void serverms(int mode, int numplayers, int minremain, char *smapname, int millis, const ENetAddress &localaddr); 
extern char msgsizelookup(int msg);
extern void servermsdesc(const char *sdesc);
extern void servermsinit(const char *master, const char *ip, int serverport, const char *sdesc, bool listen);
extern bool serverpickup(int i, int sender);
extern bool valid_client(int cn);
extern void extinfo_cnbuf(ucharbuf &p, int cn);
extern void extinfo_statsbuf(ucharbuf &p, int pid, int bpos, ENetSocket &pongsock, ENetAddress &addr, ENetBuffer &buf, int len);
extern void extinfo_teamscorebuf(ucharbuf &p);
extern char *votestring(int type, char *arg1, char *arg2);

// demo
#define DHDR_DESCCHARS 80
struct demoheader
{
    char magic[16];
    int version, protocol;
    char desc[DHDR_DESCCHARS];
};

// logging

struct log
{
    bool console, enabled;
    enum level { info = 0, warning, error };

    log() : console(true), enabled(false) {};
    virtual ~log() {};

    virtual void writeline(int level, const char *msg, ...) = 0;
    virtual void open() = 0;
    virtual void close() = 0;
};

extern struct log *newlogger(const char *identity);

