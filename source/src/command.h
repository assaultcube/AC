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
    int narg;           // ID_VAR, ID_COMMAND
    char *action, *executing; // ID_ALIAS
    bool persist;

    int context;


    ident() {}

    // ID_VAR
    ident(int type, const char *name, int minval, int maxval, int *i, void (*fun)(), bool persist, int context)
        : type(type), name(name), isconst(false), minval(minval), maxval(maxval), fun(fun),
          narg(0), action(NULL), executing(NULL), persist(persist), context(context)
    { storage.i = i; }

    // ID_FVAR
    ident(int type, const char *name, float minval, float maxval, float *f, void (*fun)(), bool persist, int context)
        : type(type), name(name), isconst(false), minvalf(minval), maxvalf(maxval), fun(fun),
          narg(0), action(NULL), executing(NULL), persist(persist), context(context)
    { storage.f = f; }

    // ID_SVAR
    ident(int type, const char *name, char **s, void (*fun)(), bool persist, int context)
        : type(type), name(name), isconst(false), minval(0), maxval(0), fun(fun),
          narg(0), action(NULL), executing(NULL), persist(persist), context(context)
    { storage.s = s; }

    // ID_ALIAS
    ident(int type, const char *name, char *action, bool persist, int context)
        : type(type), name(name), isconst(false), minval(0), maxval(0), stack(0),
          narg(0), action(action), executing(NULL), persist(persist), context(context)
    { storage.i = NULL; }

    // ID_COMMAND
    ident(int type, const char *name, void (*fun)(), int narg, int context)
        : type(type), name(name), isconst(false), minval(0), maxval(0), fun(fun),
          narg(narg), action(NULL), executing(NULL), persist(false), context(context)
    { storage.i = NULL; }
};

enum    // function signatures for script functions, see command.cpp
{
    ARG_1INT, ARG_2INT, ARG_3INT, ARG_4INT,
    ARG_NONE,
    ARG_1STR, ARG_2STR, ARG_3STR, ARG_4STR, ARG_5STR, ARG_6STR, ARG_7STR, ARG_8STR,
    ARG_DOWN,
    ARG_1EXP, ARG_2EXP,
    ARG_1EXPF, ARG_2EXPF,
    ARG_1EST, ARG_2EST,
    ARG_IVAL, ARG_FVAL, ARG_SVAL,
    ARG_CONC, ARG_CONCW,
    ARG_VARI
};

enum { IEXC_CORE = 0, IEXC_CFG, IEXC_PROMPT, IEXC_MAPCFG, IEXC_MDLCFG, IEXC_NUM }; // script execution context

// nasty macros for registering script functions, abuses globals to avoid excessive infrastructure
#define COMMANDN(name, fun, nargs) static bool __dummy_##fun = addcommand(#name, (void (*)())fun, nargs)
#define COMMAND(name, nargs) COMMANDN(name, name, nargs)
#define COMMANDF(name, nargs, inlinefunc) static void __dummy_##name inlinefunc ; COMMANDN(name, __dummy_##name, nargs)
#define ICOMMANDF(name, nargs, inlinefunc) static int __dummy_##name inlinefunc ; COMMANDN(name, __dummy_##name, nargs)

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

#define SVARP(name, cur) char *name = svariable(#name, cur, &name, NULL, true)
#define SVAR(name, cur)  char *name = svariable(#name, cur, &name, NULL, false)
#define SVARF(name, cur, body)  extern char *name; void var_##name() { body; } char *name = svariable(#name, cur, &name, var_##name, false)
#define SVARFP(name, cur, body) extern char *name; void var_##name() { body; } char *name = svariable(#name, cur, &name, var_##name, true)

#define ATOI(s) strtol(s, NULL, 0)      // supports hexadecimal numbers

