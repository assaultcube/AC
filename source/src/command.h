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
    int minval, maxval; // ID_VAR
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
        : type(type), name(name), minval(minval), maxval(maxval), fun(fun), 
          narg(0), action(NULL), executing(NULL), persist(persist), context(context) 
    { storage.i = i; }

    // ID_FVAR
    ident(int type, const char *name, float *f, void (*fun)(), bool persist, int context)
        : type(type), name(name), minval(0), maxval(0), fun(fun), 
          narg(0), action(NULL), executing(NULL), persist(persist), context(context) 
    { storage.f = f; }

    // ID_SVAR
    ident(int type, const char *name, char **s, void (*fun)(), bool persist, int context)
        : type(type), name(name), minval(0), maxval(0), fun(fun),    
          narg(0), action(NULL), executing(NULL), persist(persist), context(context)
    { storage.s = s; }

    // ID_ALIAS
    ident(int type, const char *name, char *action, bool persist, int context)
        : type(type), name(name), minval(0), maxval(0), stack(0),             
          narg(0), action(action), executing(NULL), persist(persist), context(context)  
    { storage.i = NULL; }

    // ID_COMMAND
    ident(int type, const char *name, void (*fun)(), int narg, int context)
        : type(type), name(name), minval(0), maxval(0), fun(fun),
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
    ARG_1EST, ARG_2EST,
    ARG_IVAL, ARG_SVAL,
    ARG_VARI, ARG_VARIW
};

enum { IEXC_CORE = 0, IEXC_CFG, IEXC_PROMPT, IEXC_MAPCFG, IEXC_NUM }; // script execution context

// nasty macros for registering script functions, abuses globals to avoid excessive infrastructure
#define COMMANDN(name, fun, nargs) static bool __dummy_##fun = addcommand(#name, (void (*)())fun, nargs)
#define COMMAND(name, nargs) COMMANDN(name, name, nargs)

#define VARP(name, min, cur, max) int name = variable(#name, min, cur, max, &name, NULL, true)
#define VAR(name, min, cur, max)  int name = variable(#name, min, cur, max, &name, NULL, false)
#define VARN(name, global, min, cur, max) int global = variable(#name, min, cur, max, &global, NULL, false)
#define VARF(name, min, cur, max, body)  void var_##name(); int name = variable(#name, min, cur, max, &name, var_##name, false); void var_##name() { body; }
#define VARFP(name, min, cur, max, body) void var_##name(); int name = variable(#name, min, cur, max, &name, var_##name, true); void var_##name() { body; }

#define FVARP(name, cur) float name = fvariable(#name, cur, &name, NULL, true)
#define FVAR(name, cur)  float name = fvariable(#name, cur, &name, NULL, false)
#define FVARF(name, cur, body)  void var_##name(); float name = fvariable(#name, cur, &name, var_##name, false); void var_##name() { body; }
#define FVARFP(name, cur, body) void var_##name(); float name = fvariable(#name, cur, &name, var_##name, true); void var_##name() { body; }

#define SVARP(name, cur) char *name = svariable(#name, cur, &name, NULL, true)
#define SVAR(name, cur)  char *name = svariable(#name, cur, &name, NULL, false)
#define SVARF(name, cur, body)  void var_##name(); char *name = svariable(#name, cur, &name, var_##name, false); void var_##name() { body; }
#define SVARFP(name, cur, body) void var_##name(); char *name = svariable(#name, cur, &name, var_##name, true); void var_##name() { body; }

#define ATOI(s) strtol(s, NULL, 0)      // supports hexadecimal numbers

