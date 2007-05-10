// protos for ALL external functions in cube... 

// command
extern bool persistidents;
extern int variable(char *name, int min, int cur, int max, int *storage, void (*fun)(), bool persist);
extern void setvar(char *name, int i);
extern int getvar(char *name);
extern bool identexists(char *name);
extern bool addcommand(char *name, void (*fun)(), int narg);
extern int execute(char *p);
extern void exec(char *cfgfile);
extern bool execfile(char *cfgfile);
extern void resetcomplete();
extern void complete(char *s);
extern void alias(char *name, char *action);
extern char *getalias(char *name);
extern void writecfg();
extern void identnames(vector<char *> &names, bool builtinonly);

// console
extern void keypress(int code, bool isdown, int cooked);
extern void rendercommand(int x, int y);
extern void renderconsole();
extern char *getcurcommand();
extern char *addreleaseaction(char *s);
extern void writebinds(FILE *f);
extern void conoutf(const char *s, ...);

// menus
extern bool rendermenu();
extern void menumanual(void *menu, int n, char *text, char *action=NULL);
extern bool menukey(int code, bool isdown);
extern void *addmenu(char *name, char *title = NULL, bool allowinput = true, void (__cdecl *refreshfunc)(void *, bool) = NULL);
extern void rendermenumdl();
extern void menuset(void *m);
extern void menuselect(void *menu, int sel);
extern void drawmenubg(int x1, int y1, int x2, int y2, bool border);

// serverbrowser
extern void addserver(char *servername);
extern char *getservername(int n);
extern bool resolverwait(const char *name, ENetAddress *address);
extern void writeservercfg();
extern void refreshservers(void *menu, bool init);

// rendergl
extern void gl_init(int w, int h, int bpp, int depth, int fsaa);
extern void cleangl();
extern void line(int x1, int y1, float z1, int x2, int y2, float z2);
extern void box(block &b, float z1, float z2, float z3, float z4);
extern void dot(int x, int y, float z);
extern void linestyle(float width, int r, int g, int b);
extern void blendbox(int x1, int y1, int x2, int y2, bool border, int tex = -1);
extern void quad(GLuint tex, float x, float y, float s, float tx, float ty, float tsx, float tsy = 0);
extern void circle(GLuint tex, float x, float y, float r, float tx, float ty, float tr, int subdiv = 32);
extern void gl_drawframe(int w, int h, float changelod, float curfps);
extern void clearminimap();

// texture
struct Texture
{
    char *name;
    int xs, ys;
    GLuint id;
};
extern Texture *crosshair;

extern void overbright(float amount);
extern void createtexture(int tnum, int w, int h, void *pixels, int clamp, bool mipmap, GLenum format);
extern Texture *textureload(const char *name, int clamp = 0);
extern int lookuptexture(int tex, int &xs, int &ys);
extern void draw_envbox(int fogdist);

// rendercubes
extern void mipstats(int a, int b, int c);
extern void render_flat(int tex, int x, int y, int size, int h, sqr *l1, sqr *l2, sqr *l3, sqr *l4, bool isceil);
extern void render_flatdelta(int wtex, int x, int y, int size, float h1, float h2, float h3, float h4, sqr *l1, sqr *l2, sqr *l3, sqr *l4, bool isceil);
extern void render_square(int wtex, float floor1, float floor2, float ceil1, float ceil2, int x1, int y1, int x2, int y2, int size, sqr *l1, sqr *l2, bool topleft);
extern void render_tris(int x, int y, int size, bool topleft, sqr *h1, sqr *h2, sqr *s, sqr *t, sqr *u, sqr *v);
extern void setwatercolor(char *r = "", char *g = "", char *b = "", char *a = "");
extern void addwaterquad(int x, int y, int size);
extern int renderwater(float hf, GLuint reflecttex, GLuint refracttex);
extern void resetcubes();
extern void setupstrips();
extern void renderstripssky();
extern void renderstrips();

// client
extern void localservertoclient(int chan, uchar *buf, int len);
extern void connects(char *servername, char *password = NULL);
extern void abortconnect();
extern void disconnect(int onlyclean = 0, int async = 0);
extern void toserver(char *text);
extern void addmsg(int type, const char *fmt = NULL, ...);
extern bool multiplayer();
extern bool allowedittoggle();
extern void sendpackettoserv(int chan, struct _ENetPacket *packet);
extern void gets2c();
extern void c2sinfo(playerent *d);
extern void c2skeepalive();
extern void neterr(char *s);
extern int getclientnum();
extern void changemapserv(char *name, int mode);
extern void changeteam(int team);
extern void newteam(char *name);
extern bool sendpwd;

// clientgame
extern flaginfo flaginfos[2];
extern bool autoteambalance;
extern void updateworld(int curtime, int lastmillis);
extern void startmap(char *name);
extern void changemap(char *name);
extern void initclient();
extern void deathstate(playerent *pl);
extern void spawnplayer(playerent *d);
extern void dodamage(int damage, int actor, playerent *act, bool gib = false, playerent *pl = player1);
extern playerent *newplayerent();
extern botent *newbotent();
extern void freebotent(botent *d);
extern char *getclientmap();
extern const char *modestr(int n);
extern void zapplayer(playerent *&d);
extern playerent *getclient(int cn);
extern playerent *newclient(int cn);
extern void timeupdate(int timeremain);
extern void respawnself();
extern void setskin(playerent *pl, uint skin);
//game mode extras
extern void flagpickup();
extern void tryflagdrop(bool reset = false);
extern void flagreturn();
extern void flagscore();
extern void flagstolen(int flag, int action, int act);
extern void flagdropped(int flag, int action, short x, short y, short z);
extern void flaginbase(int flag, int action, int act);
extern void flagmsg(int flag, int action);
extern void arenarespawn();
extern void respawn();
extern void mastercommand(int cmd, int arg1);
extern void refreshmastermenu(void *menu, bool init);

// scoreboard
extern void showscores(bool on);
extern void renderscores(void *menu, bool init);

// world
extern void setupworld(int factor);
extern void empty_world(int factor, bool force);
extern void remip(block &b, int level = 0);
extern void remipmore(block &b, int level = 0);
extern int closestent();
extern int findentity(int type, int index = 0);
extern int findentity(int type, int index, uchar attr2);
extern entity *newentity(int x, int y, int z, char *what, int v1, int v2, int v3, int v4);

// worldlight
extern void calclight();
extern void dodynlight(vec &vold, vec &v, int reach, int strength, dynent *owner);
extern void cleardlights();
extern block *blockcopy(block &b);
extern void blockpaste(block &b);
extern void freeblock(block *&b);

// worldrender
extern void render_world(float vx, float vy, float vh, float changelod, int yaw, int pitch, float widef, int w, int h);
extern int lod_factor();

// worldocull
extern void computeraytable(float vx, float vy);
extern int isoccluded(float vx, float vy, float cx, float cy, float csize);

// main
extern void fatal(char *s, char *o = "");
extern void keyrepeat(bool on);

// rendertext
extern void draw_text(const char *str, int left, int top);
extern void draw_textf(const char *fstr, int left, int top, ...);
extern int char_width(int c, int x = 0);
extern int text_width(const char *str, int limit = -1);
extern int text_visible(const char *str, int max);
extern void text_block(const char *str, int max, vector<char *> &lines);
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
extern void gl_drawhud(int w, int h, int curfps, int nquads, int curvert, bool underwater);
extern void damageblend(int n);
extern void loadingscreen(const char *fmt = NULL, ...);
extern void show_out_of_renderloop_progress(float bar1, const char *text1, float bar2 = 0, const char *text2 = NULL);

// renderparticles
extern void particleinit();
extern void particlereset();
extern void particle_splash(int type, int num, int fade, vec &p);
extern void particle_trail(int type, int fade, vec &from, vec &to);
extern void particle_fireball(int type, vec &o);
extern void addshotline(dynent *d, vec &from, vec &to);
extern bool addbullethole(vec &from, vec &to, float radius = 1);
extern void render_particles(int time);

// worldio
extern void save_world(char *fname);
extern void load_world(char *mname);
extern void writemap(char *mname, int msize, uchar *mdata);
extern uchar *readmap(char *mname, int *msize);
extern void loadgamerest();
extern void incomingdemodata(int chan, uchar *buf, int len, bool extras = false);
extern void demoplaybackstep();
extern void stop();
extern void stopifrecording();
extern void demodamage(int damage, vec &o);
extern void demoblend(int damage);

// physics
extern float raycube(const vec &o, const vec &ray, vec &surface);
extern void moveplayer(physent *pl, int moveres, bool local);
extern void moveplayer(physent *pl, int moveres, bool local, int curtime);
extern bool collide(physent *d, bool spawn, float drop, float rise);
extern void entinmap(physent *d);
extern void physicsframe();
extern void mousemove(int dx, int dy);
extern void fixcamerarange(physent *cam = camera1);

// sound
extern void playsound(int n, vec *loc = 0);
extern void playsoundc(int n);
extern void initsound();
extern void cleansound();
extern void music(char *name);

// rendermodel
extern void rendershadow(playerent *d);
extern void rendermodel(char *mdl, int anim, int tex, float rad, float x, float y, float z, float yaw, float pitch, float speed = 0, int basetime = 0, playerent *d = NULL, char *vwepmdl = NULL, float scale = 1.0f);
extern mapmodelinfo &getmminfo(int i);
extern int findanim(const char *name);
extern void loadskin(const char *dir, const char *altdir, Texture *&skin, model *m);
extern model *loadmodel(const char *name, int i = -1);
extern void preload_playermodels();
extern void preload_entmodels();
extern void preload_hudguns();
extern void preload_mapmodels();
extern void renderclients();
extern void renderclient(playerent *d);
extern void renderclient(playerent *d, char *mdlname, char *vwepname, int tex = 0);

// hudgun
extern char *hudgunnames[];

extern void renderhudgun();

// server
extern void initserver(bool dedicated, int uprate, char *sdesc, char *ip, char *master, char *passwd, int maxcl, char *maprot, char *masterpwd); // EDIT: AH
extern void cleanupserver();
extern void localconnect();
extern void localdisconnect();
extern void localclienttoserver(int chan, struct _ENetPacket *);
extern void serverslice(int seconds, unsigned int timeout);
extern void putint(ucharbuf &p, int n);
extern int getint(ucharbuf &p);
extern void putuint(ucharbuf &p, int n);
extern int getuint(ucharbuf &p);
extern void sendstring(const char *t, ucharbuf &p);
extern void getstring(char *t, ucharbuf &p, int len = MAXTRANS);
extern void startintermission();
extern void restoreserverstate(vector<entity> &ents);
extern uchar *retrieveservers(uchar *buf, int buflen);
extern char msgsizelookup(int msg);
extern void serverms(int mode, int numplayers, int minremain, char *smapname, int seconds);
extern void servermsinit(const char *master, char *sdesc, bool listen);
extern bool serverpickup(uint i, int sec, int sender);

// weapon
extern bool scoped;

extern void shoot(playerent *d, vec &to);
extern void shootv(int gun, vec &from, vec &to, playerent *d = 0, bool local = false, int nademillis=0);
extern void createrays(vec &from, vec &to);
extern void moveprojectiles(float time);
extern bounceent *newbounceent();
extern void movebounceents();
extern void clearbounceents();
extern void renderbounceents();
extern void addgib(playerent *d);
extern void projreset();
extern playerent *playerincrosshair();
extern int reloadtime(int gun);
extern void reload(playerent *d);
extern int attackdelay(int gun);
extern int magsize(int gun);
extern int kick_rot(int gun);
extern int kick_back(int gun);
extern bool gun_changed;
extern bool akimboside;
extern void checkweaponswitch();
extern void weaponswitch(int gun);
extern void setscope(bool activate);
// Added by Rick
extern bool intersect(dynent *d, vec &from, vec &to, vec *end = NULL);
// End add by Rick

// entities
extern char *entnames[];

extern void putitems(ucharbuf &p);
extern void realpickup(int n, playerent *d);
extern void renderentities();
extern void resetspawns();
extern void setspawn(int i, bool on);
extern void baseammo(int gun, playerent *d);
extern void checkitems(playerent *d);
extern void equip(playerent *d);
extern bool intersect(entity *e, vec &from, vec &to, vec *end=NULL);

// rndmap
extern void perlinarea(block &b, int scale, int seed, int psize);

// GL_ARB_multitexture
extern PFNGLACTIVETEXTUREARBPROC   glActiveTexture_;
extern PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2f_;
extern PFNGLMULTITEXCOORD3FARBPROC glMultiTexCoord3f_;

// doc
extern void renderdoc(int x, int y);
extern void renderdocmenu(void *menu, bool init);
extern void toggledoc();
extern void scrolldoc(int i);

