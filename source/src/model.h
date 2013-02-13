enum { ANIM_IDLE = 0, ANIM_RUN, ANIM_ATTACK, ANIM_PAIN, ANIM_JUMP, ANIM_LAND, ANIM_FLIPOFF, ANIM_SALUTE, ANIM_TAUNT, ANIM_WAVE, ANIM_POINT, ANIM_CROUCH_IDLE, ANIM_CROUCH_WALK, ANIM_CROUCH_ATTACK, ANIM_CROUCH_PAIN, ANIM_CROUCH_DEATH, ANIM_DEATH, ANIM_LYING_DEAD, ANIM_FLAG, ANIM_GUN_IDLE, ANIM_GUN_SHOOT, ANIM_GUN_RELOAD, ANIM_GUN_THROW, ANIM_MAPMODEL, ANIM_TRIGGER, ANIM_DECAY, ANIM_ALL, NUMANIMS };

#define ANIM_INDEX       0xFF
#define ANIM_LOOP        (1<<8)
#define ANIM_START       (1<<9)
#define ANIM_END         (1<<10)
#define ANIM_REVERSE     (1<<11)
#define ANIM_NOINTERP    (1<<12)
#define ANIM_MIRROR      (1<<13)
#define ANIM_NOSKIN      (1<<14)
#define ANIM_TRANSLUCENT (1<<15)
#define ANIM_PARTICLE    (1<<16)
#define ANIM_DYNALLOC    (1<<17)

struct animstate                                // used for animation blending of animated characters
{
    int anim, frame, range, basetime;
    float speed;
    animstate() { reset(); }
    void reset() { anim = frame = range = basetime = 0; speed = 100.0f; };

    bool operator==(const animstate &o) const { return frame==o.frame && range==o.range && basetime==o.basetime && speed==o.speed; }
    bool operator!=(const animstate &o) const { return frame!=o.frame || range!=o.range || basetime!=o.basetime || speed!=o.speed; }
};

enum { MDL_MD2 = 1, MDL_MD3 };

struct model;
struct modelattach
{
    const char *tag, *name;
    vec *pos;
    model *m;

    modelattach() : tag(NULL), name(NULL), pos(NULL), m(NULL) {}
    modelattach(const char *tag, const char *name) : tag(tag), name(name), pos(NULL), m(NULL) {}
    modelattach(const char *tag, vec *pos) : tag(tag), name(NULL), pos(pos), m(NULL) {}
};

class dynent;

struct model
{
    bool cullface, vertexlight, alphablend;  //ALX Alpha channel models
    float alphatest, translucency, scale, radius, shadowdist;
    vec translate;
    int cachelimit, batch;
    
    //model() : cullface(true), vertexlight(false), alphatest(0.9f), translucency(0.25f), scale(1), radius(0), shadowdist(0), translate(0, 0, 0), cachelimit(8), batch(-1) {}
    model() : cullface(true), vertexlight(false),  alphablend(false), alphatest(0.9f), translucency(0.25f), scale(1), radius(0), shadowdist(0), translate(0, 0, 0), cachelimit(8), batch(-1) {}
    virtual ~model() {}

    virtual bool load() = 0;
    virtual char *name() = 0;
    virtual int type() = 0;

    virtual void cleanup() = 0;

    virtual void render(int anim, int varseed, float speed, int basetime, const vec &o, float yaw, float pitch, dynent *d, modelattach *a = NULL, float scale = 1.0f) = 0;
    virtual void setskin(int tex = 0) = 0;

    virtual void genshadows(float height, float rad) {}
    virtual void rendershadow(int anim, int varseed, float speed, int basetime, const vec &o, float yaw, modelattach *a = NULL) {}
    virtual bool hasshadows() { return false; }

    virtual void startrender() {}
    virtual void endrender() {}
};

struct mapmodelinfo { int rad, h, zoff; string name; model *m; };
