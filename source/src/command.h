enum { ID_VAR, ID_FVAR, ID_SVAR, ID_COMMAND, ID_ALIAS };

struct identstack
{
    char *action;
    int context;
    identstack *next;
};

struct ident
{
    int type;           // one of ID_* above
    const char *name;
    bool isconst;
    union
    {
        int minval;    // ID_VAR
        float minvalf; // ID_FVAR
    };
    union
    {
        int maxval;    // ID_VAR
        float maxvalf; // ID_FVAR
    };
    union
    {
        int *i;         // ID_VAR
        float *f;       // ID_FVAR
        char **s;       // ID_SVAR;
    } storage;
    union
    {
        void (*fun)();      // ID_VAR, ID_COMMAND
        identstack *stack;  // ID_ALIAS
    };
    const char *sig;        // command signature
    char *action;           // ID_ALIAS
    union
    {
        void (*getfun)();    // ID_SVAR   (called /before/ reading the value string, as a chance for last-minute updates)
        char *executing;     // ID_ALIAS
    };
    short context;
    bool persist;

    ident() {}

    // ID_VAR
    ident(int type, const char *name, int minval, int maxval, int *i, void (*fun)(), bool persist, int context)
        : type(type), name(name), isconst(false), minval(minval), maxval(maxval), fun(fun),
          sig(NULL), action(NULL), executing(NULL), context(context), persist(persist)
    { storage.i = i; }

    // ID_FVAR
    ident(int type, const char *name, float minval, float maxval, float *f, void (*fun)(), bool persist, int context)
        : type(type), name(name), isconst(false), minvalf(minval), maxvalf(maxval), fun(fun),
          sig(NULL), action(NULL), executing(NULL), context(context), persist(persist)
    { storage.f = f; }

    // ID_SVAR
    ident(int type, const char *name, char **s, void (*fun)(), void (*getfun)(), bool persist, int context)
        : type(type), name(name), isconst(false), minval(0), maxval(0), fun(fun),
          sig(NULL), action(NULL), getfun(getfun), context(context), persist(persist)
    { storage.s = s; }

    // ID_ALIAS
    ident(int type, const char *name, char *action, bool persist, int context)
        : type(type), name(name), isconst(false), minval(0), maxval(0), stack(0),
          sig(NULL), action(action), executing(NULL), context(context), persist(persist)
    { storage.i = NULL; }

    // ID_COMMAND
    ident(int type, const char *name, void (*fun)(), const char *sig, int context)
        : type(type), name(name), isconst(false), minval(0), maxval(0), fun(fun),
          sig(sig), action(NULL), executing(NULL), context(context), persist(false)
    { storage.i = NULL; }
};

enum { IEXC_CORE = 0, IEXC_CFG, IEXC_PROMPT, IEXC_MAPCFG, IEXC_MDLCFG, IEXC_NUM }; // script execution context

// nasty macros for registering script functions, abuses globals to avoid excessive infrastructure
#define COMMANDN(name, fun, sig) static bool __dummy_##fun = addcommand(#name, (void (*)())fun, sig)
#define COMMAND(name, sig) COMMANDN(name, name, sig)
#define COMMANDF(name, sig, inlinefunc) static void __dummy_##name inlinefunc ; COMMANDN(name, __dummy_##name, sig)

#define VARP(name, min, cur, max) int name = variable(#name, min, cur, max, &name, NULL, true)
#define VAR(name, min, cur, max)  int name = variable(#name, min, cur, max, &name, NULL, false)
#define VARN(name, global, min, cur, max) int global = variable(#name, min, cur, max, &global, NULL, false)
#define VARNP(name, global, min, cur, max) int global = variable(#name, min, cur, max, &global, NULL, true)
#define VARF(name, min, cur, max, body)  extern int name; void var_##name() { body; } int name = variable(#name, min, cur, max, &name, var_##name, false)
#define VARFP(name, min, cur, max, body) extern int name; void var_##name() { body; } int name = variable(#name, min, cur, max, &name, var_##name, true)

#define FVARP(name, min, cur, max) float name = fvariable(#name, min, cur, max, &name, NULL, true)
#define FVAR(name, min, cur, max)  float name = fvariable(#name, min, cur, max, &name, NULL, false)
#define FVARF(name, min, cur, max, body)  extern float name; void var_##name() { body; } float name = fvariable(#name, min, cur, max, &name, var_##name, false)
#define FVARFP(name, min, cur, max, body) extern float name; void var_##name() { body; } float name = fvariable(#name, min, cur, max, &name, var_##name, true)

// SVARs are represented by "char *name" which has to be a valid "newstring()" all the time
#define SVARP(name, cur) char *name = svariable(#name, cur, &name, NULL, NULL, true)
#define SVAR(name, cur)  char *name = svariable(#name, cur, &name, NULL, NULL, false)
#define SVARF(name, cur, body)  extern char *name; void var_##name() { body; } char *name = svariable(#name, cur, &name, var_##name, NULL, false)
#define SVARFP(name, cur, body) extern char *name; void var_##name() { body; } char *name = svariable(#name, cur, &name, var_##name, NULL, true)
#define SVARFF(name, getb, checkb)  extern char *name; void var_get##name() { getb; } void var_check##name() { checkb; } char *name = svariable(#name, "", &name, var_check##name, var_get##name, false)

#define ATOI(s) strtol(s, NULL, 0)      // supports hexadecimal numbers

