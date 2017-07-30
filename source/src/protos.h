// protos for ALL external functions in cube...

struct authkey
{
    const char *name;
    uchar sk[64];

    authkey(const char *aname, const char *privkey);

    ~authkey()
    {
        if(name) printf("deleting %s\n", name);
        DELSTRING(name);
    }
};

enum { CERT_MISC = 0, CERT_DEV, CERT_MASTER, CERT_SERVER, CERT_PLAYER, CERT_CLANBOSS, CERT_CLAN, CERT_NUM };   // keep the lists of certtype-attributes in crypto.cpp in sync with this!

struct certline { char *key, *val, *comment; };

struct cert
{
    char *orgmsg,               // original file content, unchanged
         *workmsg,              // copy of orgmsg, with changes
         *name,                 // name of the cert (player name, clan name, etc), needs to be unique (in combination with type)
         *orgfilename;          // name of the file, the cert was loaded from - without path or extension
    uchar *signedby,            // public key the cert was signed with
          *pubkey;              // public key of the cert
    int orglen,                 // original filesize
        signeddate,             // date, the cert was signed, in _minutes_ since the epoch
        days2expire, days2renew;

    struct cert *parent,        // cert of the signedby-key, if in tree
                *next;          // used to chain certs

    vector<certline> lines;

    uchar rank,                 // hierarchy level of the cert, 0 if not in tree
          type;                 // parsed type (value has to be in range all the time!)
    bool isvalid,               // signature is valid and cert is not expired
         ischecked,             // signature is checked valid
         needsrenewal,          // cert is valid, but should be renewed
         expired,               // true, if the cert type does expire and the cert is properly signed, but the expired-date is passed
         superseded,            // a newer valid cert of same type and name exists
         blacklisted,           // pubkey of the cert is listed on the blacklist
         illegal;               // cert has too high rank for its type

    bool parse();
    struct certline *getline(const char *key);
    const char *getval(const char *key);
    char *getcertfilename(const char *subpath);
    char *getnewcertfilename(const char *subpath);
    void movecertfile(const char *subpath);

    cert(const char *filename) : orgmsg(NULL), workmsg(NULL), name(NULL), signedby(NULL), pubkey(NULL),
                                 signeddate(0),
                                 parent(NULL), next(NULL),
                                 rank(0), type(CERT_MISC),
                                 isvalid(false), ischecked(false), needsrenewal(false), expired(false), superseded(false), blacklisted(false), illegal(false)
    {
        orgfilename = filename ? newstring(filename) : NULL;
        if(orgfilename)
        {
            char *ofn = getcertfilename(NULL);
            orgmsg = loadfile(ofn, &orglen);
            if(orgmsg) parse();
            if(!ischecked) movecertfile("invalid");   // move invalid certs out of the way
            delstring(ofn);
        }
    }

    ~cert()
    {
        DELETEA(workmsg);
        DELETEA(orgmsg);
        DELSTRING(orgfilename);
    }
};

struct makecert
{
    char *buf, *curbuf, *endbuf;
    cert *c;

    makecert(int bufsize = 10);
    ~makecert();
    void addline(const char *key, const char *val, const char *com);
    char *sign(const uchar *keypair, const char *com);
};

#ifndef STANDALONE

extern bool hasTE, hasMT, hasMDA, hasDRE, hasstencil, hasST2, hasSTW, hasSTS, hasAF;

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

// console
extern stream *clientlogfile;
extern vector<char> *bootclientlog;

extern void keypress(int code, bool isdown, int cooked, SDLMod mod = KMOD_NONE);
extern int rendercommand(int x, int y, int w);
extern void renderconsole();
extern char *getcurcommand(int *pos);
extern char *addreleaseaction(const char *s);
extern void savehistory();
extern void loadhistory();
extern void writebinds(stream *f);
extern void pasteconsole(char *dst);
extern void clientlogf(const char *s, ...) PRINTFARGS(1, 2);

struct keym
{
    int code;

    enum
    {
        ACTION_DEFAULT = 0,
        ACTION_EDITING,
        ACTION_SPECTATOR,
        NUMACTIONS
    };

    char *name, *actions[NUMACTIONS];
    bool pressed, unchangeddefault[NUMACTIONS];

    keym() : code(-1), name(NULL), pressed(false) { loopi(NUMACTIONS) { actions[i] = newstring(""); unchangeddefault[i] = false; } }
    ~keym() { DELETEA(name); loopi(NUMACTIONS) DELETEA(actions[i]); }
};

extern keym *keypressed;

extern bool bindkey(keym *km, const char *action, int state = keym::ACTION_DEFAULT);
extern keym **findbinda(const char *action, int type = keym::ACTION_DEFAULT);
extern bool bindc(int code, const char *action, int type = keym::ACTION_DEFAULT);
extern keym *findbindc(int code);

// menus
extern void rendermenu();
extern bool menuvisible();
extern void menureset(void *menu);
extern void menuitemmanual(void *menu, char *text, char *action = NULL, color *bgcolor = NULL, const char *desc = NULL);
extern void menuimagemanual(void *menu, const char *filename1, const char *filename2, char *text, char *action = NULL, color *bgcolor = NULL, const char *desc = NULL);
extern void menutitlemanual(void *menu, const char *title);
extern bool needscoresreorder;
extern void menuheader(void *menu, char *header, char *footer, bool heap = false);
extern bool menukey(int code, bool isdown, int unicode, SDLMod mod = KMOD_NONE);
extern void *addmenu(const char *name, const char *title = NULL, bool allowinput = true, void (__cdecl *refreshfunc)(void *, bool) = NULL, bool (__cdecl *keyfunc)(void *, int, bool, int) = NULL, bool hotkeys = false, bool forwardkeys = false);
extern bool rendermenumdl();
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
    bool greyedout;
    static bool menugreyedout;

    mitem(gmenu *parent, color *bgcolor, int type) : parent(parent), bgcolor(bgcolor), greyedout(menugreyedout), mitemtype(type) {}
    virtual ~mitem() {}

    virtual void render(int x, int y, int w);
    virtual int width() = 0;
    virtual int select() { return 0; }
    virtual void focus(bool on) { }
    virtual void key(int code, bool isdown, int unicode) { }
    virtual void init() {}
    virtual const char *getdesc() { return NULL; }
    virtual const char *gettext() { return NULL; }
    virtual const char *getaction() { return NULL; }
    bool isselection();
    void renderbg(int x, int y, int w, color *c);
    int execaction(const char *arg1);
    static color gray, white, whitepulse;
    int mitemtype;

    enum { TYPE_TEXTINPUT, TYPE_KEYINPUT, TYPE_CHECKBOX, TYPE_MANUAL, TYPE_SLIDER };
};

struct mdirlist
{
    char *dir, *ext, *action, *searchfile, *subdiraction;
    bool image, subdirdots;
    ~mdirlist()
    {
        DELETEA(dir); DELETEA(ext); DELETEA(action); DELETEA(searchfile); DELETEA(subdiraction);
    }
};

struct gmenu
{
    const char *name, *title, *header, *footer;
    vector<mitem *> items;
    int menusel, menuselinit;
    bool allowinput, inited, hotkeys, forwardkeys;
    void (__cdecl *refreshfunc)(void *, bool);
    bool (__cdecl *keyfunc)(void *, int, bool, int);
    char *initaction;
    char *usefont;
    bool allowblink;
    bool persistentselection;
    bool synctabstops;
    bool hasdesc;
    bool headfootheap;
    const char *mdl;
    int anim, rotspeed, scale;
    int footlen;
    int xoffs, yoffs;
    char *previewtexture, *previewtexturetitle;
    mdirlist *dirlist;

    gmenu() : name(NULL), title(NULL), header(NULL), footer(NULL), menuselinit(-1), initaction(NULL), usefont(NULL),
              allowblink(false), persistentselection(false), synctabstops(false), hasdesc(false), headfootheap(false),
              mdl(NULL), footlen(0), xoffs(0), yoffs(0), previewtexture(NULL), previewtexturetitle(NULL), dirlist(NULL) {}
    virtual ~gmenu()
    {
        DELETEA(name);
        DELETEA(mdl);
        DELETEP(dirlist);
        DELETEA(initaction);
        DELETEA(usefont);
        items.deletecontents();
    }

    void render();
    void renderbg(int x1, int y1, int x2, int y2, bool border);
    void conprintmenu();
    void refresh();
    void open();
    void close();
    void init();
    int headeritems();
};

struct mline { string name, cmd; };

// serverbrowser
extern void addserver(const char *servername, int serverport, int weight);
extern bool resolverwait(const char *name, ENetAddress *address);
extern int connectwithtimeout(ENetSocket sock, const char *hostname, ENetAddress &remoteaddress);
extern void writeservercfg();
extern void refreshservers(void *menu, bool init);
extern bool serverskey(void *menu, int code, bool isdown, int unicode);
extern bool serverinfokey(void *menu, int code, bool isdown, int unicode);

struct serverinfo
{
    enum { UNRESOLVED = 0, RESOLVING, RESOLVED };

    string name;
    string full;
    string map;
    string sdesc;
    string description;
    string cmd;
    int mode, numplayers, maxclients, ping, protocol, minremain, resolved, port, lastpingmillis, pongflags, getinfo, menuline_from, menuline_to;
    ENetAddress address;
    vector<const char *> playernames;
    uchar namedata[MAXTRANS];
    vector<char *> infotexts;
    uchar textdata[MAXTRANS];
    char lang[3];
    color *bgcolor;
    int favcat, msweight, weight;

    int uplinkqual, uplinkqual_age;
    unsigned char uplinkstats[MAXCLIENTS + 1];

    serverinfo()
     : mode(0), numplayers(0), maxclients(0), ping(9999), protocol(0), minremain(0),
       resolved(UNRESOLVED), port(-1), lastpingmillis(0), pongflags(0), getinfo(EXTPING_NOP),
       bgcolor(NULL), favcat(-1), msweight(0), weight(0), uplinkqual(0), uplinkqual_age(0)
    {
        name[0] = full[0] = map[0] = sdesc[0] = description[0] = '\0';
        loopi(3) lang[i] = '\0';
        loopi(MAXCLIENTS + 1) uplinkstats[i] = 0;
    }
};

extern serverinfo *getconnectedserverinfo();
extern void pingservers();
extern void updatefrommaster(int *force);

// http
struct httpget
{
    // parameters
    const char *hostname, *url;                        // set by set_host() and get()
    const char *useragent, *referrer;                  // can be set manually with "newstrings", deleted automatically
    const char *err;                                   // read-only
    ENetAddress ip;                                    // set by set_host()
    int maxredirects, maxtransfer, maxsize, connecttimeout;  // set manually
    int (__cdecl *callbackfunc)(void *data, float progress);
    void *callbackdata;

    // internal
    ENetSocket tcp;
    stopwatch tcp_age;
    vector<char> rawsnd, rawrec, datarec;

    // result
    char *header;
    int response, chunked, gzipped, contentlength, offset, traffic;
    uint elapsedtime;
    vector<uchar> *outvec;                             // receives data, if not NULL (must be set up manually)
    stream *outstream;                                 // receives data, if not NULL (must be set up manually)

    httpget() { memset(&hostname, 0, sizeof(struct httpget)); tcp = ENET_SOCKET_NULL; reset(); }
    ~httpget() { reset(); DELSTRING(useragent); DELSTRING(referrer); DELETEP(outvec); DELETEP(outstream); }
    void reset(int keep = 0);                          // cleanup
    bool set_host(const char *newhostname);            // set and resolve host
    void set_port(int port);                           // set port (disconnect, if different to previous)
    bool execcallback(float progress);
    void disconnect();
    bool connect(bool force = false);
    int get(const char *url1, uint timeout, uint totaltimeout, int range = 0, bool head = false); // get web ressource
};

struct urlparse
{
    char *buf;
    // result (scheme:[//[user:password@]domain[:port]][/]path[?query][#fragment])
    const char *scheme, *userpassword, *domain, *port, *path, *query, *fragment;

    urlparse() { memset(&buf, 0, sizeof(struct urlparse)); }
    ~urlparse() { DELSTRING(buf); }
    void set(const char *newurl);      // set and parse url
};

extern char *urlencode(const char *s, bool strict = false);

struct packetqueue
{
    ringbuf<ENetPacket *, 8> packets;

    packetqueue();
    ~packetqueue();
    void queue(ENetPacket *p);
    bool flushtolog(const char *logfile);
    void clear();
};

// rendergl
extern glmatrixf mvmatrix, projmatrix, clipmatrix, mvpmatrix, invmvmatrix, invmvpmatrix;
extern void resetcamera();

extern void gl_checkextensions();
extern void gl_init(int w, int h, int bpp, int depth, int fsaa);
extern void enablepolygonoffset(GLenum type);
extern void disablepolygonoffset(GLenum type, bool restore = true);
extern void line(int x1, int y1, float z1, int x2, int y2, float z2);
extern void line(int x1, int y1, int x2, int y2, color *c = NULL);
extern void box(block &b, float z1, float z2, float z3, float z4);
extern void box2d(int x1, int y1, int x2, int y2, int gray);
extern void dot(int x, int y, float z);
extern void linestyle(float width, int r, int g, int b);
extern void blendbox(int x1, int y1, int x2, int y2, bool border, int tex = -1, color *c = NULL);
extern void quad(GLuint tex, float x, float y, float s, float tx, float ty, float tsx, float tsy = 0);
extern void quad(GLuint tex, vec &c1, vec &c2, float tx, float ty, float tsx, float tsy);
extern void circle(GLuint tex, float x, float y, float r, float tx, float ty, float tr, int subdiv = 32);
extern void framedquadtexture(GLuint tex, int x, int y, int xs, int ys, int border = 0, int backlight = 255, bool blend = false);
extern void setperspective(float fovy, float aspect, float nearplane, float farplane);
extern void sethudgunperspective(bool on);
extern void gl_drawframe(int w, int h, float changelod, float curfps, int elapsed);
extern void clearminimap();
extern void resetzones();
extern void rendercursor(int x, int y, int w);
extern void renderaboveheadicon(playerent *p);
extern void drawscope(bool preload = false);
extern float dynfov();
extern int fog;

// shadow
extern bool addshadowbox(const vec &bbmin, const vec &bbmax, const vec &extrude, const glmatrixf &mat);
extern void drawstencilshadows();

// texture
struct Texture
{
    char *name;
    int xs, ys, bpp, clamp;
    float scale;
    bool mipmap, canreduce;
    GLuint id;
};
extern Texture *notexture, *noworldtexture;
extern bool silent_texture_load;
extern bool uniformtexres;

extern void scaletexture(uchar *src, uint sw, uint sh, uint bpp, uchar *dst, uint dw, uint dh);
extern void createtexture(int tnum, int w, int h, void *pixels, int clamp, bool mipmap, bool canreduce, GLenum format);
extern SDL_Surface *wrapsurface(void *data, int width, int height, int bpp);
extern SDL_Surface *creatergbsurface(int width, int height);
extern SDL_Surface *creatergbasurface(int width, int height);
extern SDL_Surface *forcergbsurface(SDL_Surface *os);
extern SDL_Surface *forcergbasurface(SDL_Surface *os);
extern Texture *textureload(const char *name, int clamp = 0, bool mipmap = true, bool canreduce = false, float scale = 1.0f, bool trydl = false);
extern Texture *lookuptexture(int tex, Texture *failtex = notexture, bool trydl = false);
extern const char *gettextureslot(int i);
extern bool reloadtexture(Texture &t);
extern void reloadtextures();
extern bool loadskymap(bool reload);
extern void *texconfig_copy();
extern void texconfig_delete(void *s);
extern uchar *texconfig_paste(void *_s, uchar *usedslots);

static inline Texture *lookupworldtexture(int tex, bool trydl = true)
{ return lookuptexture(tex, noworldtexture, trydl); }

extern float skyfloor;
extern void draw_envbox(int fogdist);

extern int maxtmus;
extern void inittmus();
extern void resettmu(int n);
extern void scaletmu(int n, int rgbscale, int alphascale = 0);
extern void colortmu(int n, float r = 0, float g = 0, float b = 0, float a = 0);
extern void setuptmu(int n, const char *rgbfunc = NULL, const char *alphafunc = NULL);

struct igraph
{
    const char *fname;
    struct Texture *tex;
    vector<int> frames;
    int used;
};
extern void updateigraphs();
extern igraph *getusedigraph(int i);
extern int getigraph(const char *mnem);
extern void encodeigraphs(char *d, const char *s, int len);
extern void enumigraphs(vector<const char *> &igs, const char *s, int len);

// renderhud
enum
{
    CROSSHAIR_DEFAULT = NUMGUNS,
    CROSSHAIR_TEAMMATE,
    CROSSHAIR_SCOPE,
    CROSSHAIR_EDIT,
    CROSSHAIR_NUM
};

extern const char *crosshairnames[];
extern Texture *crosshairs[];
extern void drawcrosshair(playerent *p, int n, struct color *c = NULL, float size = -1.0f);

// autodownload
enum { PCK_TEXTURE = 0, PCK_SKYBOX, PCK_MAPMODEL, PCK_AUDIO, PCK_MAP, PCK_MOD, PCK_NUM };
extern int autodownload;
extern void setupautodownload();
extern void pollautodownloadresponse();
extern bool requirepackage(int type, const char *name, const char *host = NULL);
extern int downloadpackages(bool loadscr = true);
extern void sortpckservers();
extern void writepcksourcecfg();

struct zone { int x1, x2, y1, y2, color; }; // zones (drawn on the minimap)

// rendercubes
extern void mipstats(const int a[]);
extern const char *cubetypenames[];
extern bool editfocusdetails(sqr *s);
extern int lighterror;
extern void render_flat(int tex, int x, int y, int size, int h, sqr *l1, sqr *l2, sqr *l3, sqr *l4, bool isceil);
extern void render_flatdelta(int wtex, int x, int y, int size, float h1, float h2, float h3, float h4, sqr *l1, sqr *l2, sqr *l3, sqr *l4, bool isceil);
extern void render_square(int wtex, float floor1, float floor2, float ceil1, float ceil2, int x1, int y1, int x2, int y2, int size, sqr *l1, sqr *l2, bool topleft, int dir);
extern void render_tris(int x, int y, int size, bool topleft, sqr *h1, sqr *h2, sqr *s, sqr *t, sqr *u, sqr *v);
extern void resetcubes();
extern void setupstrips();
extern void renderstripssky();
extern void renderstrips();
extern void rendershadow(int x, int y, int xs, int ys, const vec &texgenS, const vec &texgenT);

// water
extern void setwatercolor(const char *r = "", const char *g = "", const char *b = "", const char *a = "");
extern void calcwaterscissor();
extern void addwaterquad(int x, int y, int size);
extern int renderwater(float hf, GLuint reflecttex, GLuint refracttex);
extern void resetwater();

// client
extern void abortconnect();
extern void disconnect(int onlyclean = 0, int async = 0);
extern void cleanupclient();
extern void toserver(char *text);
extern void addmsg(int type, const char *fmt = NULL, ...);
extern bool multiplayer(const char *op = "");
extern bool allowedittoggle();
extern void sendpackettoserv(int chan, ENetPacket *packet);
extern void gets2c();
extern void c2sinfo(playerent *d);
extern void c2skeepalive();
extern void neterr(const char *s);
extern int getclientnum();
extern void getmap(char *name = NULL, char *callback = NULL);
extern void newteam(char *name);
extern bool securemapcheck(const char *map, bool msg = true);
extern int getbuildtype();
extern void sendintro();
extern void getdemo(int *idx, char *dsp);
extern void listdemos();

// serverms
bool requestmasterf(const char *fmt, ...); // for AUTH et al

// clientgame
extern flaginfo flaginfos[2];
extern entitystats_s clentstats;
extern mapdim_s clmapdims;
extern int sessionid;
extern int gametimecurrent;
extern int gametimemaximum;
extern int lastgametimeupdate;
struct serverstate { int autoteam; int mastermode; int matchteamsize; void reset() { autoteam = mastermode = matchteamsize = 0; }};
extern struct serverstate servstate;
extern void updateworld(int curtime, int lastmillis);
extern void resetmap(bool mrproper = true);
extern void startmap(const char *name, bool reset = true, bool norespawn = false);
extern void changemap(const char *name);
extern void initclient();
extern void deathstate(playerent *pl);
extern void spawnplayer(playerent *d);
extern void dodamage(int damage, playerent *pl, playerent *actor, int gun = -1, bool gib = false, bool local = true);
extern void dokill(playerent *pl, playerent *act, bool gib = false, int gun = 0);
extern playerent *newplayerent();
extern botent *newbotent();
extern void freebotent(botent *d);
extern char *getclientmap();
extern int getclientmode();
extern int teamatoi(const char *name);
extern void zapplayer(playerent *&d);
extern playerent *getclient(int cn);
extern playerent *newclient(int cn);
extern void timeupdate(int milliscur, int millismax); // was (int timeremain);
extern void respawnself();
extern void setskin(playerent *pl, int skin, int team = -1);
extern void callvote(int type, const char *arg1 = NULL, const char *arg2 = NULL, const char *arg3 = NULL);
extern void addsleep(int msec, const char *cmd, bool persist = false);
extern void resetsleep(bool force = false);
extern int lastpm;
//game mode extras
extern void flagpickup(int fln);
extern void tryflagdrop(bool manual = false);
extern void flagreturn(int fln);
extern void flagscore(int fln);
extern void flagstolen(int flag, int act);
extern void flagdropped(int flag, float x, float y, float z);
extern void flaginbase(int flag);
extern void flagidle(int flag);
extern void flagmsg(int flag, int message, int actor, int flagtime);
extern bool tryrespawn();
extern void gotoplayerstart(playerent *d, entity *e);
extern void findplayerstart(playerent *d, bool mapcenter = false, int arenaspawn = -1);
extern void refreshsopmenu(void *menu, bool init);
extern char *colorname(playerent *d, char *name = NULL, const char *prefix = "");
extern char *colorping(int ping);
extern char *colorpj(int pj);
extern const char *highlight(const char *text);
extern void togglespect();
extern playerent *updatefollowplayer(int shiftdirection = 0);
extern void spectatemode(int mode);

struct votedisplayinfo
{
    playerent *owner;
    int type, stats[VOTE_NUM], result, millis;
    string desc;
    bool localplayervoted;
    votedisplayinfo() : owner(NULL), result(VOTE_NEUTRAL), millis(0), localplayervoted(false) { loopi(VOTE_NUM) stats[i] = VOTE_NEUTRAL; }
};

extern votedisplayinfo *newvotedisplayinfo(playerent *owner, int type, const char *arg1, const char *arg2, const char *arg3 = "");
extern void callvotesuc();
extern void callvoteerr(int e);
extern void displayvote(votedisplayinfo *v);
extern void voteresult(int v);
extern void votecount(int v);
extern void clearvote();

// scoreboard
struct discscore { int team, flags, frags, deaths, points; char name[MAXNAMELEN + 1]; };
extern vector<discscore> discscores;
extern void showscores(bool on);
extern void renderscores(void *menu, bool init);
extern const char *asciiscores(bool destjpg = false);
extern void consolescores();
extern void calcteamscores(int scores[4]);

// world
extern void setupworld(int factor);
extern void sqrdefault(sqr *s);
extern bool worldbordercheck(int x1, int x2, int y1, int y2, int z1, int z2);
extern bool empty_world(int factor, bool force);
extern void remip(const block &b, int level = 0);
extern void remipmore(const block &b, int level = 0);
extern void remipgenerous(const block &b);
extern bool pinnedclosestent;
extern int closestent();
extern void deletesoundentity(entity &e);
extern vector<int> todoents;
extern vector<char *> todoentdescs;
extern void cleartodoentities(const char *n = NULL);
extern void addtodoentity(int n, const char *desc);
extern int findtype(const char *what);
extern int findentity(int type, int index = 0);
extern int findentity(int type, int index, uchar attr2);
extern void newentity(int index, int x, int y, int z, const char *what, float v1, float v2, float v3, float v4);
extern void mapmrproper(bool manual);
extern void clearworldvisibility();
extern void calcworldvisibility();
extern vector<persistent_entity> deleted_ents;

// worldlight
extern int lastcalclight;

extern void fullbrightlight(int level = -1);
extern void calclight();
extern void adddynlight(physent *owner, const vec &o, int reach, int expire, int fade, uchar r, uchar g = 0, uchar b = 0);
extern void dodynlights();
extern void undodynlights();
extern void cleardynlights();
extern void removedynlights(physent *owner);
extern block *blockcopy(const block &b);
extern void blocktexusage(const block &b, uchar *used);
extern void blockpaste(const block &b, int bx, int by, bool light, uchar *texmap);
extern void blockpaste(const block &b);
extern void freeblockp(block *b);
extern void freeblock(block *&b);

// worldrender
extern void render_world(float vx, float vy, float vh, float changelod, int yaw, int pitch, float fov, float fovy, int w, int h);
extern int lod_factor();

// worldocull
extern void disableraytable();
extern void computeraytable(float vx, float vy, float fov);
extern int isoccluded(float vx, float vy, float cx, float cy, float csize);

// main
extern char *lang;
extern SDL_Surface *screen;
extern int colorbits, depthbits, stencilbits;

extern void keyrepeat(bool on);
extern bool interceptkey(int sym);
extern bool inmainloop;

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
        short w, h;
        float left, right, top, bottom;
    };

    char *name;
    Texture *tex;
    vector<charinfo> chars;
    int defaultw, defaulth;
    int offsetx, offsety, offsetw, offseth;
    int skip;
};

#define VIRTH 1800
#define FONTH (curfont->defaulth)
#define PIXELTAB (10*curfont->defaultw)

extern int VIRTW; // virtual screen size for text & HUD
extern bool ignoreblinkingbit;
extern font *curfont;
extern bool setfont(const char *name);
extern font *getfont(const char *name);
extern void pushfont(const char *name);
extern void popfont();
extern void draw_text(const char *str, int left, int top, int r = 255, int g = 255, int b = 255, int a = 255, int cursor = -1, int maxwidth = -1);
extern void draw_textf(const char *fstr, int left, int top, ...) PRINTFARGS(1, 4);
extern int text_width(const char *str);
extern int text_visible(const char *str, int max);
extern void text_bounds(const char *str, int &width, int &height, int maxwidth = -1);
extern int text_visible(const char *str, int hitx, int hity, int maxwidth);
extern void text_pos(const char *str, int cursor, int &cx, int &cy, int maxwidth);
extern void text_startcolumns();
extern void text_endcolumns();
extern void cutcolorstring(char *text, int len);
extern bool filterunrenderables(char *s);

// editing
#define EDITSEL(x)   if(noteditmode(x) || noselection()) return
#define EDITSELMP(x) if(noteditmode(x) || noselection() || multiplayer(x)) return
#define EDITMP(x)    if(noteditmode(x) || multiplayer(x)) return
#define EDIT(x)      if(noteditmode(x)) return
extern void cursorupdate();
extern void toggleedit(bool force = false);
extern void reseteditor();
extern char *editinfo();
extern void makeundo(block &sel);
extern void editdrag(bool isdown);
extern void checkselections();
extern void netblockpastexy(ucharbuf *p, int bx, int by, int bxs, int bys, int light);
extern void setvdeltaxy(int delta, block &sel);
extern void editequalisexy(bool isfloor, block &sel);
extern void edittypexy(int type, block &sel);
extern void edittexxy(int type, int t, block &sel);
extern void editheightxy(bool isfloor, int amount, block &sel);
extern void archxy(int sidedelta, int *averts, block &sel);
extern void slopexy(int xd, int yd, block &sel);
extern void stairsxy(int xd, int yd, block &sel);
extern void edittagxy(int orv, int andv, block &sel);
extern void selfliprotate(block &sel, int dir);
extern bool noteditmode(const char* func = NULL);
extern void storeposition(short p[]);
extern void restoreposition(short p[]);
extern void restoreeditundo(ucharbuf &q);
extern int backupeditundo(vector<uchar> &buf, int undolimit, int redolimit);

// renderhud
#define HUDPOS_X_BOTTOMLEFT 20
#define HUDPOS_Y_BOTTOMLEFT 1570
#define HUDPOS_ICONSPACING 235
#define HUDPOS_HEALTH (HUDPOS_X_BOTTOMLEFT / 2)
#define HUDPOS_ARMOUR (HUDPOS_HEALTH + HUDPOS_ICONSPACING)
#define HUDPOS_WEAPON (HUDPOS_ARMOUR + HUDPOS_ICONSPACING)
#define HUDPOS_GRENADE (HUDPOS_WEAPON + HUDPOS_ICONSPACING)
#define HUDPOS_NUMBERSPACING 70

enum
{
    HUDMSG_INFO = 0,
    HUDMSG_TIMER,
    HUDMSG_MIPSTATS,
    HUDMSG_EDITFOCUS,

    HUDMSG_TYPE = 0xFF,
    HUDMSG_OVERWRITE = 1<<8
};
extern int dimeditinfopanel;
extern void damageblend(int n, void *p);
extern void gl_drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater, int elapsed);
extern void loadingscreen(const char *fmt = NULL, ...) PRINTFARGS(1, 2);
extern void hudoutf(const char *s, ...) PRINTFARGS(1, 2);
extern void hudonlyf(const char *s, ...) PRINTFARGS(1, 2);
extern void hudeditf(int type, const char *s, ...) PRINTFARGS(2, 3);
extern void show_out_of_renderloop_progress(float bar1, const char *text1, float bar2 = 0, const char *text2 = NULL);
extern void updatedmgindicator(playerent *p, vec &attack);
extern vec getradarpos();

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

enum
{
    PART_SPARK = 0,
    PART_SMOKE,
    PART_ECLOSEST,
    PART_BLOOD,
    PART_DEMOTRACK,
    PART_FIREBALL,
    PART_SHOTLINE,
    PART_BULLETHOLE,
    PART_BLOODSTAIN,
    PART_SCORCH,
    PART_HUDMUZZLEFLASH,
    PART_MUZZLEFLASH,
    PART_ELIGHT,
    PART_ESPAWN,
    PART_EAMMO,
    PART_EPICKUP,
    PART_EMODEL,
    PART_ECARROT,
    PART_ELADDER,
    PART_EFLAG,
    MAXPARTYPES
};

extern void particleinit();
extern void particlereset();
extern void particle_flash(int type, float scale, float angle, const vec &p);
extern void particle_splash(int type, int num, int fade, const vec &p);
extern void particle_cube(int type, int num, int fade, int x, int y);
extern void particle_trail(int type, int fade, const vec &from, const vec &to);
extern void particle_emit(int type, int *args, int basetime, int seed, const vec &p);
extern void particle_fireball(int type, const vec &o);
extern void addshotline(dynent *d, const vec &from, const vec &to);
extern bool addbullethole(dynent *d, const vec &from, const vec &to, float radius = 1, bool noisy = true);
extern bool addscorchmark(vec &o, float radius = 7);
extern void render_particles(int time, int typemask = ~0);
extern const char *particletypenames[MAXPARTYPES + 1];

// worldio
enum { LWW_ENTATTROVERFLOW = 0x1, LWW_DECODEERR = 0x10, LWW_WORLDERROR = 0x100, LWW_MISSINGMEDIA = 0x1000, LWW_CONFIGERR = 0x10000, LWW_MODELERR = 0x100000, LWW_SCRIPTERR = 0x1000000 };
extern const char *setnames(const char *name);
extern void save_world(char *mname, bool skipoptimise = false, bool addcomfort = false);
extern int _ignoreillegalpaths;
extern int load_world(char *mname);
extern char *getfiledesc(const char *dir, const char *name, const char *ext);
extern void writemap(char *name, int size, uchar *data);
extern void writecfggz(char *name, int size, int sizegz, uchar *data);
extern uchar *readmap(char *name, int *size, int *revision);
extern uchar *readmcfggz(char *name, int *size, int *sizegz);
extern void rlencodecubes(vector<uchar> &f, sqr *s, int len, bool preservesolids);
extern bool rldecodecubes(ucharbuf &f, sqr *s, int len, int version, bool silent);
extern void clearheaderextras();
extern void automapconfig();
extern void flagmapconfigchange();
extern void flagmapconfigerror(int what);
extern void getcurrentmapconfig(vector<char> &f, bool onlysounds);
extern char *mapinfo_license, *mapinfo_comment;
extern void setmapinfo(const char *newlicense = NULL, const char *newcomment = NULL);
extern void xmapbackup(const char *nickprefix, const char *nick);
extern void writeallxmaps();
extern int loadallxmaps();

// physics
extern float raycube(const vec &o, const vec &ray, vec &surface);
extern bool raycubelos(const vec &from, const vec &to, float margin = 0);
extern int cornertest(int x, int y, int &bx, int &by, int &bs, sqr *&s, sqr *&h);
extern void moveplayer(physent *pl, int moveres, bool local);
extern void moveplayer(physent *pl, int moveres, bool local, int curtime);
extern void movebounceent(bounceent *p, int moveres, bool local);
extern void entinmap(physent *d);
extern void physicsframe();
extern void mousemove(int idx, int idy);
extern void fixcamerarange(physent *cam = camera1);
extern void updatecrouch(playerent *p, bool on);
extern bool objcollide(physent *d, const vec &objpos, float objrad, float objheight);
extern bool collide(physent *d, bool spawn = false, float drop = 0, float rise = 0);
extern void attack(bool on);

// rendermodel
extern void rendermodel(const char *mdl, int anim, int tex, float rad, const vec &o, float roll, float yaw, float pitch, float speed = 0, int basetime = 0, playerent *d = NULL, modelattach *a = NULL, float scale = 1.0f);
extern void startmodelbatches();
extern void endmodelbatches(bool flush = true);
extern void clearmodelbatches();
extern mapmodelinfo *getmminfo(int i);
extern int findanim(const char *name);
extern void loadskin(const char *dir, const char *altdir, Texture *&skin);
extern model *loadmodel(const char *name, int i = -1, bool trydl = false);
extern void resetmdlnotfound();
extern void preload_playermodels();
extern void preload_entmodels();
extern void preload_hudguns();
extern bool preload_mapmodels(bool trydl = false);
extern void renderclients();
extern void renderclientp(playerent *d);
extern void renderclient(playerent *d, const char *mdlname, const char *vwepname, int tex = 0);
extern void updateclientname(playerent *d);
extern void updatemapmodeldependencies();
extern void writemapmodelattributes();
extern const char *mmshortname(const char *name);

// weapon
extern void shoot(playerent *d, vec &to);
extern void createrays(const vec &from, const vec &to);
extern void removebounceents(playerent *owner);
extern void movebounceents();
extern void clearbounceents();
extern void renderbounceents();
extern void addgib(playerent *d);
extern playerent *playerincrosshair();
extern int magsize(int gun);
extern void setscope(bool activate);
extern void setburst(bool activate);
extern void intersectgeometry(const vec &from, vec &to);
extern int intersect(playerent *d, const vec &from, const vec &to, vec *end = NULL);
extern bool intersect(entity *e, const vec &from, const vec &to, vec *end = NULL);
extern void damageeffect(int damage, playerent *d);
extern void tryreload(playerent *p);
extern void checkweaponstate();
extern int burstshotssettings[NUMGUNS];

// entities
extern int edithideentmask;
extern void pickupeffects(int n, playerent *d);
extern void renderentities();
extern void rendermapmodels();
extern void resetpickups(int type = -1);
extern void setpickupspawn(int i, bool on);
extern void checkitems(playerent *d);
extern vector<int> changedents;
extern void syncentchanges(bool force = false);
extern void clampentityattributes(persistent_entity &e);
extern const char *formatentityattributes(const persistent_entity &e, bool withcomma = false);

// rndmap
extern void perlinarea(block &b, int scale, int seed, int psize);

// doc
extern void renderdoc(int x, int y, int doch);
extern void renderdocmenu(void *menu, bool init);
extern void toggledoc();
extern void scrolldoc(int i);
extern const char *docgetdesc(const char *name);
#endif

// protocol [client and server]
extern const char *acronymmodestr(int n);
extern const char *fullmodestr(int n);
extern int defaultgamelimit(int gamemode);

// crypto
#define TIGERHASHSIZE 24
extern void tigerhash(uchar *hash, const uchar *msg, int len);
extern void *tigerhash_init(uchar *hash);
extern void tigerhash_add(uchar *hash, const void *msg, int len, void *state);
extern void tigerhash_finish(uchar *hash, void *state);
extern void loadcertdir();     // load all certs in "config/certs"
#if 0
// crypto // for AUTH
extern void genprivkey(const char *seed, vector<char> &privstr, vector<char> &pubstr);
extern bool hashstring(const char *str, char *result, int maxlen);
extern void answerchallenge(const char *privstr, const char *challenge, vector<char> &answerstr);
extern void *parsepubkey(const char *pubstr);
extern void freepubkey(void *pubkey);
extern void *genchallenge(void *pubkey, const void *seed, int seedlen, vector<char> &challengestr);
extern void freechallenge(void *answer);
extern bool checkchallenge(const char *answerstr, void *correct);
#endif

// console
extern void conoutf(const char *s, ...) PRINTFARGS(1, 2);

// command
extern bool persistidents;
extern char *exchangestr(char *o, const char *n);
extern int variable(const char *name, int min, int cur, int max, int *storage, void (*fun)(), bool persist);
extern float fvariable(const char *name, float min, float cur, float max, float *storage, void (*fun)(), bool persist);
extern char *svariable(const char *name, const char *cur, char **storage, void (*fun)(), void (*getfun)(), bool persist);
extern void setvar(const char *name, int i, bool dofunc = false);
extern void setfvar(const char *name, float f, bool dofunc = false);
extern void setsvar(const char *name, const char *str, bool dofunc = false);
extern bool identexists(const char *name);
extern bool addcommand(const char *name, void (*fun)(), const char *sig);
extern int execute(const char *p);
enum { HOOK_MP = 1, HOOK_SP, HOOK_SP_MP, HOOK_FLAGMASK = 0xff, HOOK_TEAM = 0x100, HOOK_NOTEAM = 0x200, HOOK_BOTMODE = 0x400, HOOK_FLAGMODE = 0x800, HOOK_ARENA = 0x1000 };
extern bool exechook(int context, const char *ident, const char *body,...) PRINTFARGS(3, 4);  // execute cubescript hook if available and allowed in current context/gamemode
extern void identhash(uint64_t *d);
extern char *executeret(const char *p);
extern char *conc(const char **w, int n, bool space);
extern void intret(int v);
extern const char *floatstr(float v, bool neat = false);
extern void floatret(float v, bool neat = false);
extern void result(const char *s);
extern void resultcharvector(const vector<char> &res, int adj);
extern void exec(const char *cfgfile);
extern bool execfile(const char *cfgfile);
extern int listlen(const char *s);
extern void resetcomplete();
extern void complete(char *s, bool reversedirection);
extern void push(const char *name, const char *action);
extern void pop(const char *name);
extern void alias(const char *name, const char *action, bool temp = false, bool constant = false);
extern const char *getalias(const char *name);
extern void writecfg();
extern void deletecfg();
extern void identnames(vector<const char *> &names, bool builtinonly);
extern void explodelist(const char *s, vector<char *> &elems);
extern const char *escapestring(const char *s, bool force = true, bool noquotes = false);
extern int execcontext;
extern void pushscontext(int newcontext);
extern int popscontext();
extern void scripterr();
extern void setcontext(const char *context, const char *info);
extern void resetcontext();
extern void dumpexecutionstack(stream *f);
extern const char *currentserver(int i);

// server
extern int modeacronyms;
extern void servertoclient(int chan, uchar *buf, int len, bool demo = false);
extern void localservertoclient(int chan, uchar *buf, int len, bool demo = false);
extern const char *modestr(int n, bool acronyms = false);
extern const char *voteerrorstr(int n);
extern const char *mmfullname(int n);
extern void fatal(const char *s, ...) PRINTFARGS(1, 2);
extern void initserver(bool dedicated, int argc = 0, char **argv = NULL);
extern void cleanupserver();
extern void localconnect();
extern void localdisconnect();
extern void localclienttoserver(int chan, ENetPacket *);
extern void serverslice(uint timeout);
extern void putint(ucharbuf &p, int n);
extern void putint(packetbuf &p, int n);
extern void putint(vector<uchar> &p, int n);
extern int getint(ucharbuf &p);
extern void putaint(ucharbuf &p, int n);
extern void putaint(packetbuf &p, int n);
extern void putaint(vector<uchar> &p, int n);
extern int getaint(ucharbuf &p);
extern void putuint(ucharbuf &p, int n);
extern void putuint(packetbuf &p, int n);
extern void putuint(vector<uchar> &p, int n);
extern int getuint(ucharbuf &p);
extern void putfloat(ucharbuf &p, float f);
extern void putfloat(packetbuf &p, float f);
extern void putfloat(vector<uchar> &p, float f);
extern float getfloat(ucharbuf &p);
extern void sendstring(const char *t, ucharbuf &p);
extern void sendstring(const char *t, packetbuf &p);
extern void sendstring(const char *t, vector<uchar> &p);
extern void getstring(char *t, ucharbuf &p, int len = MAXTRANS);
extern void putgzbuf(vector<uchar> &d, vector<uchar> &s); // zips a vector into a stream, stored in another vector
extern ucharbuf *getgzbuf(ucharbuf &p); // fetch a gzipped buffer; needs to be freed properly later
extern void freegzbuf(ucharbuf *p);  // free a ucharbuf created by getgzbuf()
extern char *filtertext(char *dst, const char *src, int flags, int len = sizeof(string)-1);
extern void filterrichtext(char *dst, const char *src, int len = sizeof(string)-1);
extern void filterlang(char *d, const char *s);
extern void trimtrailingwhitespace(char *s);
extern string mastername;
extern int masterport;
extern ENetSocket connectmaster();
extern void serverms(int mode, int numplayers, int minremain, char *smapname, int millis, const ENetAddress &localaddr, int *mnum, int *msend, int *mrec, int *cnum, int *csend, int *crec, int protocol_version);
extern int msgsizelookup(int msg);
extern const char *genpwdhash(const char *name, const char *pwd, int salt);
extern void servermsinit(const char *master, const char *ip, int serverport, bool listen);
extern bool serverpickup(int i, int sender);
extern bool valid_client(int cn);
extern void extinfo_cnbuf(ucharbuf &p, int cn);
extern void extinfo_statsbuf(ucharbuf &p, int pid, int bpos, ENetSocket &pongsock, ENetAddress &addr, ENetBuffer &buf, int len, int *csend);
extern void extinfo_teamscorebuf(ucharbuf &p);
extern char *votestring(int type, char *arg1, char *arg2, char *arg3);
extern int wizardmain(int argc, char **argv);

// some tools
extern servsqr *createservworld(const sqr *s, int _cubicsize);
extern int calcmapdims(mapdim_s &md, const servsqr *s, int _ssize);
extern int calcmapareastats(mapareastats_s &ms, servsqr *s, int _ssize, const mapdim_s &md);
extern void calcentitystats(entitystats_s &es, const persistent_entity *pents, int pentsize);

// demo
#define DHDR_DESCCHARS 80
#define DHDR_PLISTCHARS 322
struct demoheader
{
    char magic[16];
    int version, protocol;
    char desc[DHDR_DESCCHARS];
    char plist[DHDR_PLISTCHARS];
};
#define DEFDEMOFILEFMT "%w_%h_%n_%Mmin_%G"
#define DEFDEMOTIMEFMT "%Y%m%d_%H%M"

extern bool watchingdemo;
extern int demoprotocol;
extern void enddemoplayback();

// logging

enum { ACLOG_DEBUG = 0, ACLOG_VERBOSE, ACLOG_INFO, ACLOG_WARNING, ACLOG_ERROR, ACLOG_NUM };

extern bool initlogging(const char *identity, int facility_, int consolethres, int filethres, int syslogthres, bool logtimestamp);
extern void exitlogging();
extern bool logline(int level, const char *msg, ...) PRINTFARGS(2, 3);

// server config

struct serverconfigfile
{
    string filename;
    int filelen;
    char *buf;
    serverconfigfile() : filelen(0), buf(NULL) { filename[0] = '\0'; }
    virtual ~serverconfigfile() { DELETEA(buf); }

    virtual void read() {}
    void init(const char *name);
    bool load();
};

// server commandline parsing
struct servercommandline
{
    int uprate, serverport, syslogfacility, filethres, syslogthres, maxdemos, maxclients, kickthreshold, banthreshold, verbose, incoming_limit, afk_limit, ban_time, demotimelocal;
    const char *ip, *master, *logident, *serverpassword, *adminpasswd, *demopath, *maprot, *pwdfile, *blfile, *nbfile, *infopath, *motdpath, *forbidden, *demofilenameformat, *demotimestampformat;
    bool logtimestamp, demo_interm, loggamestatus;
    string motd, servdesc_full, servdesc_pre, servdesc_suf, voteperm, mapperm;
    int clfilenesting;
    vector<const char *> adminonlymaps;

    servercommandline() :   uprate(0), serverport(CUBE_DEFAULT_SERVER_PORT), syslogfacility(6), filethres(-1), syslogthres(-1), maxdemos(5),
                            maxclients(DEFAULTCLIENTS), kickthreshold(-5), banthreshold(-6), verbose(0), incoming_limit(10), afk_limit(45000), ban_time(20*60*1000), demotimelocal(0),
                            ip(""), master(NULL), logident(""), serverpassword(""), adminpasswd(""), demopath(""),
                            maprot("config/maprot.cfg"), pwdfile("config/serverpwd.cfg"), blfile("config/serverblacklist.cfg"), nbfile("config/nicknameblacklist.cfg"),
                            infopath("config/serverinfo"), motdpath("config/motd"), forbidden("config/forbidden.cfg"),
                            logtimestamp(false), demo_interm(false), loggamestatus(true),
                            clfilenesting(0)
    {
        motd[0] = servdesc_full[0] = servdesc_pre[0] = servdesc_suf[0] = voteperm[0] = mapperm[0] = '\0';
        demofilenameformat = DEFDEMOFILEFMT;
        demotimestampformat = DEFDEMOTIMEFMT;
    }

    bool checkarg(const char *arg)
    {
        if(!strncmp(arg, "assaultcube://", 13)) return false;
        else if(arg[0] != '-' || arg[1] == '\0') return false;
        const char *a = arg + 2 + strspn(arg + 2, " ");
        int ai = atoi(a);
        // client: dtwhzbsave
        switch(arg[1])
        { // todo: gjlqEGHJQUYZ
            case '-':
                    if(!strncmp(arg, "--demofilenameformat=", 21))
                    {
                        demofilenameformat = arg+21;
                    }
                    else if(!strncmp(arg, "--demotimestampformat=", 22))
                    {
                        demotimestampformat = arg+22;
                    }
                    else if(!strncmp(arg, "--demotimelocal=", 16))
                    {
                        int ai = atoi(arg+16);
                        demotimelocal = ai == 0 ? 0 : 1;
                    }
                    else if(!strncmp(arg, "--masterport=", 13))
                    {
                        int ai = atoi(arg+13);
                        masterport = ai == 0 ? AC_MASTER_PORT : ai;
                    }
                    else return false;
                    break;
            case 'u': uprate = ai; break;
            case 'f': if(ai > 0 && ai < 65536) serverport = ai; break;
            case 'i': ip     = a; break;
            case 'm': master = a; break;
            case 'N': logident = a; break;
            case 'l': loggamestatus = ai != 0; break;
            case 'F': if(isdigit(*a) && ai >= 0 && ai <= 7) syslogfacility = ai; break;
            case 'T': logtimestamp = true; break;
            case 'L':
                switch(*a)
                {
                    case 'F': filethres = atoi(a + 1); break;
                    case 'S': syslogthres = atoi(a + 1); break;
                }
                break;
            case 'A': if(*a) adminonlymaps.add(a); break;
            case 'c': if(ai > 0) maxclients = min(ai, MAXCLIENTS); break;
            case 'k':
            {
                if(arg[2]=='A' && arg[3]!='\0')
                {
                    if ((ai = atoi(&arg[3])) >= 30) afk_limit = ai * 1000;
                    else afk_limit = 0;
                }
                else if(arg[2]=='B' && arg[3]!='\0')
                {
                    if ((ai = atoi(&arg[3])) >= 0) ban_time = ai * 60 * 1000;
                    else ban_time = 0;
                }
                else if(ai < 0) kickthreshold = ai;
                break;
            }
            case 'y': if(ai < 0) banthreshold = ai; break;
            case 'x': adminpasswd = a; break;
            case 'p': serverpassword = a; break;
            case 'D':
            {
                if(arg[2]=='I')
                {
                    demo_interm = true;
                }
                else if(ai > 0) maxdemos = ai;
                break;
            }
            case 'W': demopath = a; break;
            case 'r': maprot = a; break;
            case 'X': pwdfile = a; break;
            case 'B': blfile = a; break;
            case 'K': nbfile = a; break;
            case 'I': infopath = a; break;
            case 'o': filterrichtext(motd, a); break;
            case 'O': motdpath = a; break;
            case 'g': forbidden = a; break;
            case 'n':
            {
                char *t = servdesc_full;
                switch(*a)
                {
                    case '1': t = servdesc_pre; a += 1 + strspn(a + 1, " "); break;
                    case '2': t = servdesc_suf; a += 1 + strspn(a + 1, " "); break;
                }
                filterrichtext(t, a);
                filtertext(t, t, FTXT__SERVDESC);
                break;
            }
            case 'P': concatstring(voteperm, a); break;
            case 'M': concatstring(mapperm, a); break;
            case 'Z': if(ai >= 0) incoming_limit = ai; break;
            case 'V': verbose++; break;
            case 'C': if(*a && clfilenesting < 3)
            {
                serverconfigfile cfg;
                cfg.init(a);
                cfg.load();
                int line = 1;
                clfilenesting++;
                if(cfg.buf)
                {
                    printf("reading commandline parameters from file '%s'\n", a);
                    for(char *p = cfg.buf, *l; p < cfg.buf + cfg.filelen; line++)
                    {
                        l = p; p += strlen(p) + 1;
                        for(char *c = p - 2; c > l; c--) { if(*c == ' ') *c = '\0'; else break; }
                        l += strspn(l, " \t");
                        if(*l && !this->checkarg(newstring(l)))
                            printf("unknown parameter in file '%s', line %d: '%s'\n", cfg.filename, line, l);
                    }
                }
                else printf("failed to read file '%s'\n", a);
                clfilenesting--;
                break;
            }
            default: return false;
        }
        return true;
    }
};

