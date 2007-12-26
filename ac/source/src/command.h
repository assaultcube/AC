enum { ID_VAR, ID_COMMAND, ID_ALIAS };

struct ident
{
    int type;           // one of ID_* above
    const char *name;
    int min, max;       // ID_VAR
    int *storage;       // ID_VAR
    void (*fun)();      // ID_VAR, ID_COMMAND
    int narg;           // ID_VAR, ID_COMMAND
    char *action, *executing; // ID_ALIAS
    bool persist;
    int context;
};

enum    // function signatures for script functions, see command.cpp
{   
    ARG_1INT, ARG_2INT, ARG_3INT, ARG_4INT,
    ARG_NONE, 
    ARG_1STR, ARG_2STR, ARG_3STR, ARG_4STR, ARG_5STR, ARG_6STR, ARG_7STR, ARG_8STR,
    ARG_DOWN, 
    ARG_1EXP, ARG_2EXP,
    ARG_1EST, ARG_2EST,
    ARG_VARI
};

enum { IEXC_CORE = 0, IEXC_FOREIGN, IEXC_NUM }; // script execution context

// nasty macros for registering script functions, abuses globals to avoid excessive infrastructure
#define COMMANDN(name, fun, nargs) static bool __dummy_##fun = addcommand(#name, (void (*)())fun, nargs)
#define COMMAND(name, nargs) COMMANDN(name, name, nargs)
#define VARP(name, min, cur, max) int name = variable(#name, min, cur, max, &name, NULL, true)
#define VAR(name, min, cur, max)  int name = variable(#name, min, cur, max, &name, NULL, false)
#define VARF(name, min, cur, max, body)  void var_##name(); int name = variable(#name, min, cur, max, &name, var_##name, false); void var_##name() { body; }
#define VARFP(name, min, cur, max, body) void var_##name(); int name = variable(#name, min, cur, max, &name, var_##name, true); void var_##name() { body; }

#define ATOI(s) strtol(s, NULL, 0)      // supports hexadecimal numbers

