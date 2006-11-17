enum { ANIM_IDLE = 0, ANIM_RUN, ANIM_ATTACK, ANIM_PAIN, ANIM_JUMP, ANIM_LAND, ANIM_FLIPOFF, ANIM_SALUTE, ANIM_TAUNT, ANIM_WAVE, ANIM_POINT, ANIM_CROUCH_IDLE, ANIM_CROUCH_WALK, ANIM_CROUCH_ATTACK, ANIM_CROUCH_PAIN, ANIM_CROUCH_DEATH, ANIM_DEATH, ANIM_LYING_DEAD, ANIM_FLAG, ANIM_GUN_IDLE, ANIM_GUN_SHOOT, ANIM_GUN_RELOAD, ANIM_GUN_THROW, ANIM_MAPMODEL, ANIM_TRIGGER, ANIM_ALL, NUMANIMS };

#define ANIM_INDEX      0xFF
#define ANIM_LOOP       (1<<8)
#define ANIM_START      (1<<9)
#define ANIM_END        (1<<10)
#define ANIM_REVERSE    (1<<11)
#define ANIM_NOINTERP   (1<<12)
#define ANIM_MIRROR     (1<<13)

struct animstate                                // used for animation blending of animated characters
{
    int anim, frame, range, basetime;
    float speed;
    animstate() : anim(0), frame(0), range(0), basetime(0), speed(100.0f) { };

    bool operator==(const animstate &o) const { return frame==o.frame && range==o.range && basetime==o.basetime && speed==o.speed; };
    bool operator!=(const animstate &o) const { return frame!=o.frame || range!=o.range || basetime!=o.basetime || speed!=o.speed; };
};

enum { MDL_MD2 = 1, MDL_MD3 };

struct dynent;

struct model
{
    float scale;
    vec translate;

    model() : scale(1), translate(0, 0, 0) {};
    virtual ~model() {};
    virtual void render(int anim, int varseed, float speed, int basetime, float x, float y, float z, float yaw, float pitch, dynent *d, model *vwepmdl = NULL, float scale = 1.0f) = 0;
    virtual void setskin(int tex = 0) = 0;
    virtual bool load() = 0;
    virtual char *name() = 0;
    virtual int type() = 0;
};

struct mapmodelinfo { int rad, h, zoff; string name; model *m; };

