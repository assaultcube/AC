// command.cpp: implements the parsing and execution of a tiny script language which
// is largely backwards compatible with the quake console language.

#include "pch.h"
#include "cube.h"

char *exchangestr(char *o, char *n) { delete[] o; return newstring(n); }

int execcontext = IEXC_CORE;

hashtable<const char *, ident> *idents = NULL;        // contains ALL vars/commands/aliases

bool persistidents = true;

void clearstack(ident &id)
{
    identstack *stack = id.stack;
    while(stack)
    {
        delete[] stack->action;
        identstack *tmp = stack;
        stack = stack->next;
        delete tmp;
    }
    id.stack = NULL;
}

void pushident(ident &id, char *val)
{
    if(id.type != ID_ALIAS) return;
    identstack *stack = new identstack;
    stack->action = id.executing==id.action ? newstring(id.action) : id.action;
    stack->context = id.context;
    stack->next = id.stack;
    id.stack = stack;
    id.action = val;
    id.context = execcontext;
}

void popident(ident &id)
{
    if(id.type != ID_ALIAS || !id.stack) return;
    if(id.action != id.executing) delete[] id.action;
    identstack *stack = id.stack;
    id.action = stack->action;
    id.stack = stack->next;
    id.context = stack->context;
    delete stack;
}

ident *newident(const char *name)
{
    ident *id = idents->access(name);
    if(!id)
    {
        ident init(ID_ALIAS, newstring(name), newstring(""), persistidents, IEXC_CORE);
        id = idents->access(init.name, &init);
    }
    return id;
}

void pusha(const char *name, char *action)
{
    pushident(*newident(name), action);
}

void push(const char *name, const char *action)
{
    pusha(name, newstring(action));
}

void pop(const char *name)
{
    ident *id = idents->access(name);
    if(id) popident(*id);
}

COMMAND(push, ARG_2STR);
COMMAND(pop, ARG_1STR);

void alias(const char *name, const char *action)
{
    ident *b = idents->access(name);
    if(!b)
    {
        ident b(ID_ALIAS, newstring(name), newstring(action), persistidents, execcontext);
        idents->access(b.name, &b);
    }
    else if(b->type==ID_ALIAS)
    {
        if(b->action!=b->executing) delete[] b->action;
        b->action = newstring(action);
        if(b->persist!=persistidents) b->persist = persistidents;
    }
    else conoutf("cannot redefine builtin %s with an alias", name);
}

COMMAND(alias, ARG_2STR);

// variable's and commands are registered through globals, see cube.h

int variable(const char *name, int min, int cur, int max, int *storage, void (*fun)(), bool persist)
{
    if(!idents) idents = new hashtable<const char *, ident>;
    ident v(ID_VAR, name, min, max, storage, fun, persist, IEXC_CORE);
    idents->access(name, &v);
    return cur;
}

float fvariable(const char *name, float cur, float *storage, void (*fun)(), bool persist)
{
    if(!idents) idents = new hashtable<const char *, ident>;
    ident v(ID_FVAR, name, storage, fun, persist, IEXC_CORE);
    idents->access(name, &v);
    return cur;
}

char *svariable(const char *name, const char *cur, char **storage, void (*fun)(), bool persist)
{
    if(!idents) idents = new hashtable<const char *, ident>;
    ident v(ID_SVAR, name, storage, fun, persist, IEXC_CORE);
    idents->access(name, &v);
    return newstring(cur);
}

void setvar(const char *name, int i) { *idents->access(name)->storage.i = i; }
int getvar(const char *name) { return *idents->access(name)->storage.i; }
bool identexists(const char *name) { return idents->access(name)!=NULL; }

const char *getalias(const char *name)
{
    ident *i = idents->access(name);
    return i && i->type==ID_ALIAS ? i->action : NULL;
}

bool addcommand(const char *name, void (*fun)(), int narg)
{
    if(!idents) idents = new hashtable<const char *, ident>;
    ident c(ID_COMMAND, name, fun, narg, IEXC_CORE);
    idents->access(name, &c);
    return false;
}

char *parseexp(const char *&p, int right)             // parse any nested set of () or []
{
    int left = *p++;
    const char *word = p;
    for(int brak = 1; brak; )
    {
        int c = *p++;
        if(c==left) brak++;
        else if(c==right) brak--;
        else if(!c) { p--; conoutf("missing \"%c\"", right); return NULL; }
    }
    char *s = newstring(word, p-word-1);
    if(left=='(') s = exchangestr(s, executeret(s)); // evaluate () exps directly, and substitute result
    return s;
}

char *lookup(char *n)                           // find value of ident referenced with $ in exp
{
    ident *id = idents->access(n+1);
    if(id) switch(id->type)
    {
        case ID_VAR: { string t; itoa(t, *id->storage.i); return exchangestr(n, t); }
        case ID_FVAR: { s_sprintfd(t)("%f", *id->storage.f); return exchangestr(n, t); }
        case ID_SVAR: return exchangestr(n, *id->storage.s);
        case ID_ALIAS: return exchangestr(n, id->action);
    }
    conoutf("unknown alias lookup: %s", n+1);
    return n;
}

char *parseword(const char *&p)                       // parse single argument, including expressions
{
    p += strspn(p, " \t\r");
    if(p[0]=='/' && p[1]=='/') p += strcspn(p, "\n\0");  
    if(*p=='\"')
    {
        p++;
        const char *word = p;
        p += strcspn(p, "\"\r\n\0");
        char *s = newstring(word, p-word);
        if(*p=='\"') p++;
        return s;
    }
    if(*p=='(') return parseexp(p, ')');
    if(*p=='[') return parseexp(p, ']');
    const char *word = p;
    p += strcspn(p, "; \t\r\n\0");
    if(p-word==0) return NULL;
    char *s = newstring(word, p-word);
    if(*s=='$') return lookup(s);
    return s;
}

char *conc(char **w, int n, bool space)
{
    int len = space ? max(n-1, 0) : 0;
    loopj(n) len += (int)strlen(w[j]);
    char *r = newstring("", len);
    loopi(n)
    {
        strcat(r, w[i]);  // make string-list out of all arguments
        if(i==n-1) break;
        if(space) strcat(r, " ");
    }
    return r;
}

VARN(numargs, _numargs, 0, 0, 25);

char *commandret = NULL;

void intret(int v) 
{ 
    string t;
    itoa(t, v);
    commandret = newstring(t);
}

void result(const char *s) { commandret = newstring(s); }

char *executeret(const char *p)                            // all evaluation happens here, recursively
{
    const int MAXWORDS = 25;                    // limit, remove
    char *w[MAXWORDS];
    char *retval = NULL;
    #define setretval(v) { char *rv = v; if(rv) retval = rv; commandret = NULL; }
    for(bool cont = true; cont;)                // for each ; seperated statement
    {
        int numargs = MAXWORDS;
        loopi(MAXWORDS)                         // collect all argument values
        {
            w[i] = (char *)"";
            if(i>numargs) continue;
            char *s = parseword(p);             // parse and evaluate exps
            if(s) w[i] = s;
            else numargs = i;
        }
        
        p += strcspn(p, ";\n\0");
        cont = *p++!=0;                         // more statements if this isn't the end of the string
        char *c = w[0];
        if(!*c) continue;                       // empty statement
    
        DELETEA(retval);
   
        ident *id = idents->access(c);
        if(!id)
        {
            if(!isdigit(*c) && ((*c!='+' && *c!='-') || !isdigit(c[1]))) conoutf("unknown command: %s", c);
            setretval(newstring(c));
        }
        else 
        {
            if(execcontext > id->context)
            {
                conoutf("not allowed in this execution context: %s", id->name);
                continue;
            }

            switch(id->type)
            {
                case ID_COMMAND:                    // game defined commands       
                    switch(id->narg)                // use very ad-hoc function signature, and just call it
                    { 
                        case ARG_1INT: ((void (__cdecl *)(int))id->fun)(ATOI(w[1])); break;
                        case ARG_2INT: ((void (__cdecl *)(int, int))id->fun)(ATOI(w[1]), ATOI(w[2])); break;
                        case ARG_3INT: ((void (__cdecl *)(int, int, int))id->fun)(ATOI(w[1]), ATOI(w[2]), ATOI(w[3])); break;
                        case ARG_4INT: ((void (__cdecl *)(int, int, int, int))id->fun)(ATOI(w[1]), ATOI(w[2]), ATOI(w[3]), ATOI(w[4])); break;
                        case ARG_NONE: ((void (__cdecl *)())id->fun)(); break;
                        case ARG_1STR: ((void (__cdecl *)(char *))id->fun)(w[1]); break;
                        case ARG_2STR: ((void (__cdecl *)(char *, char *))id->fun)(w[1], w[2]); break;
                        case ARG_3STR: ((void (__cdecl *)(char *, char *, char*))id->fun)(w[1], w[2], w[3]); break;
                        case ARG_4STR: ((void (__cdecl *)(char *, char *, char*, char*))id->fun)(w[1], w[2], w[3], w[4]); break;
                        case ARG_5STR: ((void (__cdecl *)(char *, char *, char*, char*, char*))id->fun)(w[1], w[2], w[3], w[4], w[5]); break;
                        case ARG_6STR: ((void (__cdecl *)(char *, char *, char*, char*, char*, char*))id->fun)(w[1], w[2], w[3], w[4], w[5], w[6]); break;
                        case ARG_7STR: ((void (__cdecl *)(char *, char *, char*, char*, char*, char*, char*))id->fun)(w[1], w[2], w[3], w[4], w[5], w[6], w[7]); break;
                        case ARG_8STR: ((void (__cdecl *)(char *, char *, char*, char*, char*, char*, char*, char*))id->fun)(w[1], w[2], w[3], w[4], w[5], w[6], w[7], w[8]); break;
                        case ARG_DOWN: ((void (__cdecl *)(bool))id->fun)(addreleaseaction(id->name)!=NULL); break;
                        case ARG_1EXP: intret(((int (__cdecl *)(int))id->fun)(execute(w[1]))); break;
                        case ARG_2EXP: intret(((int (__cdecl *)(int, int))id->fun)(execute(w[1]), execute(w[2]))); break;
                        case ARG_1EST: intret(((int (__cdecl *)(char *))id->fun)(w[1])); break;
                        case ARG_2EST: intret(((int (__cdecl *)(char *, char *))id->fun)(w[1], w[2])); break;
                        case ARG_VARI:
                        case ARG_VARIW:
                        {
                            char *r = conc(w+1, numargs-1, id->narg==ARG_VARI);
                            ((void (__cdecl *)(char *))id->fun)(r);
                            delete[] r;
                            break;
                        }
                    }
                    setretval(commandret);
                    break;
           
                case ID_VAR:                        // game defined variables
                    if(!w[1][0]) conoutf("%s = %d", c, *id->storage.i);      // var with no value just prints its current value
                    else if(id->min>id->max) conoutf("variable %s is read-only", id->name);
                    else 
                    {
                        int i1 = ATOI(w[1]);
                        if(i1<id->min || i1>id->max)
                        {
                            i1 = i1<id->min ? id->min : id->max;                // clamp to valid range
                            conoutf("valid range for %s is %d..%d", id->name, id->min, id->max);
                        }
                        *id->storage.i = i1;
                        if(id->fun) ((void (__cdecl *)())id->fun)();            // call trigger function if available
                    }
                    break;
                   
                case ID_FVAR:                        // game defined variables
                    if(!w[1][0]) conoutf("%s = %f", c, *id->storage.f);      // var with no value just prints its current value
                    else
                    {
                        *id->storage.f = atof(w[1]);
                        if(id->fun) ((void (__cdecl *)())id->fun)();            // call trigger function if available
                    }
                    break;

                case ID_SVAR:                        // game defined variables
                    if(!w[1][0]) conoutf(strchr(*id->storage.s, '"') ? "%s = [%s]" : "%s = \"%s\"", c, *id->storage.s); // var with no value just prints its current value
                    else
                    {
                        *id->storage.s = exchangestr(*id->storage.s, newstring(w[1]));
                        if(id->fun) ((void (__cdecl *)())id->fun)();            // call trigger function if available
                    }
                    break;
 
                case ID_ALIAS:                              // alias, also used as functions and (global) variables
                    static vector<ident *> argids;
                    for(int i = 1; i<numargs; i++)
                    {
                        if(i > argids.length())
                        {
                            s_sprintfd(argname)("arg%d", i);
                            argids.add(newident(argname));
                        }
                        pushident(*argids[i-1], w[i]); // set any arguments as (global) arg values so functions can access them
                    }
                    _numargs = numargs-1;
                    char *wasexecuting = id->executing;
                    id->executing = id->action;
                    setretval(executeret(id->action));
                    if(id->executing!=id->action && id->executing!=wasexecuting) delete[] id->executing;
                    id->executing = wasexecuting;
                    for(int i = 1; i<numargs; i++) popident(*argids[i-1]);
                    continue;
            }
        }
        loopj(numargs) if(w[j]) delete[] w[j];
    }
    return retval;
}

int execute(const char *p)
{
    char *ret = executeret(p);
    int i = 0;
    if(ret) { i = ATOI(ret); delete[] ret; }
    return i;
}

// tab-completion of all idents

int completesize = 0, completeidx = 0;

void resetcomplete() { completesize = 0; }

void complete(char *s)
{
    if(*s!='/')
    {
        string t;
        s_strcpy(t, s);
        s_strcpy(s, "/");
        s_strcat(s, t);
    }
    if(!s[1]) return;
    if(!completesize) { completesize = (int)strlen(s)-1; completeidx = 0; }
    int idx = 0;
    enumerate(*idents, ident, id,
        if(strncmp(id.name, s+1, completesize)==0 && idx++==completeidx)
        {
            s_strcpy(s, "/");
            s_strcat(s, id.name);
        }
    );
    completeidx++;
    if(completeidx>=idx) completeidx = 0;
}

bool execfile(const char *cfgfile)
{
    string s;
    s_strcpy(s, cfgfile);
    char *buf = loadfile(path(s), NULL);
    if(!buf) return false;
    execute(buf);
    delete[] buf;
    return true;
}

void exec(const char *cfgfile)
{
    if(!execfile(cfgfile)) conoutf("could not read \"%s\"", cfgfile);
}

// below the commands that implement a small imperative language. thanks to the semantics of
// () and [] expressions, any control construct can be defined trivially.

void ifthen(char *cond, char *thenp, char *elsep) { commandret = executeret(cond[0]!='0' ? thenp : elsep); }
void loopa(char *var, char *times, char *body) 
{ 
    int t = ATOI(times); 
    if(t<=0) return;
    ident *id = newident(var);
    if(id->type!=ID_ALIAS) return;
    loopi(t) 
    { 
        if(i) itoa(id->action, i);
        else pushident(*id, newstring("0", 16));
        execute(body); 
    } 
    popident(*id);
}
void whilea(char *cond, char *body) { while(execute(cond)) execute(body); }    // can't get any simpler than this :)

void concat(char *s) { result(s); }
void concatword(char *s) { result(s); }

#define whitespaceskip s += strspn(s, "\n\t ")
#define elementskip *s=='"' ? (++s, s += strcspn(s, "\"\n\0"), s += *s=='"') : s += strcspn(s, "\n\t \0")

void explodelist(const char *s, vector<char *> &elems)
{
    whitespaceskip;
    while(*s)
    {
        const char *elem = s;
        elementskip;
        elems.add(*elem=='"' ? newstring(elem+1, s-elem-(s[-1]=='"' ? 2 : 1)) : newstring(elem, s-elem));
        whitespaceskip;
    }
}

char *indexlist(const char *s, int pos)
{
    whitespaceskip;
    loopi(pos) elementskip, whitespaceskip;
    const char *e = s;
    elementskip;
    if(*e=='"')
    {
        e++;
        if(s[-1]=='"') --s;
    }
    return newstring(e, s-e);
}

int listlen(char *s)
{
    int n = 0;
    whitespaceskip;
    for(; *s; n++) elementskip, whitespaceskip;
    return n;
}

void at(char *s, char *pos)
{
    commandret = indexlist(s, ATOI(pos));
}

COMMANDN(loop, loopa, ARG_3STR);
COMMANDN(while, whilea, ARG_2STR);
COMMANDN(if, ifthen, ARG_3STR); 
COMMAND(exec, ARG_1STR);
COMMAND(concat, ARG_VARI);
COMMAND(concatword, ARG_VARIW);
COMMAND(result, ARG_1STR);
COMMAND(at, ARG_2STR);
COMMAND(listlen, ARG_1EST);

int add(int a, int b)   { return a+b; }         COMMANDN(+, add, ARG_2EXP);
int mul(int a, int b)   { return a*b; }         COMMANDN(*, mul, ARG_2EXP);
int sub(int a, int b)   { return a-b; }         COMMANDN(-, sub, ARG_2EXP);
int divi(int a, int b)  { return b ? a/b : 0; } COMMANDN(div, divi, ARG_2EXP);
int mod(int a, int b)   { return b ? a%b : 0; } COMMAND(mod, ARG_2EXP);
int equal(int a, int b) { return (int)(a==b); } COMMANDN(=, equal, ARG_2EXP);
int lt(int a, int b)    { return (int)(a<b); }  COMMANDN(<, lt, ARG_2EXP);
int gt(int a, int b)    { return (int)(a>b); }  COMMANDN(>, gt, ARG_2EXP);

int strcmpa(char *a, char *b) { return strcmp(a,b)==0; }  COMMANDN(strcmp, strcmpa, ARG_2EST);

int rndn(int a)    { return a>0 ? rnd(a) : 0; }  COMMANDN(rnd, rndn, ARG_1EXP);

void writecfg()
{
    FILE *f = openfile(path("config/saved.cfg", true), "w");
    if(!f) return;
    fprintf(f, "// automatically written on exit, DO NOT MODIFY\n// delete this file to have defaults.cfg overwrite these settings\n// modify settings in game, or put settings in autoexec.cfg to override anything\n\n");
    fprintf(f, "name %s\nskin %d\n", player1->name, player1->nextskin);
    extern Texture *crosshair;
    fprintf(f, "loadcrosshair %s\n", crosshair->name+strlen("packages/misc/crosshairs/"));
    extern int lowfps, highfps;
    fprintf(f, "fpsrange %d %d\n", lowfps, highfps);
    fprintf(f, "\n");
    enumerate(*idents, ident, id,
        if(!id.persist) continue;
        switch(id.type)
        {
            case ID_VAR: fprintf(f, "%s %d\n", id.name, *id.storage.i); break;
            case ID_FVAR: fprintf(f, "%s %f\n", id.name, *id.storage.f); break;
            case ID_SVAR: fprintf(f, "%s [%s]\n", id.name, *id.storage.s); break;
        }
    );
    fprintf(f, "\n");
    writebinds(f);
    fprintf(f, "\n");
    enumerate(*idents, ident, id,
        if(id.type==ID_ALIAS && id.persist && id.action[0])
        {
            fprintf(f, "alias \"%s\" [%s]\n", id.name, id.action);
        }
    );
    fprintf(f, "\n");
    fclose(f);
}

COMMAND(writecfg, ARG_NONE);

void identnames(vector<const char *> &names, bool builtinonly)
{
    enumerateht(*idents) if(!builtinonly || idents->enumc->data.type != ID_ALIAS) names.add(idents->enumc->key);
}

void changescriptcontext(int newcontext)
{
    if(newcontext < execcontext) // clean up aliases created in the old context
    {
        enumerateht(*idents)
        {
            ident *id = &idents->enumc->data;
            if(id->type == ID_ALIAS && id->context > newcontext)
            {
                idents->remove(idents->enumc->key);
                i--;
            }
        }
    }
    execcontext = newcontext;
}


bool contextsealed = false;

void scriptcontext(char *context, char *idname)
{
    if(contextsealed) return;
    ident *id = idents->access(idname);
    if(!id) return;
    int c = atoi(context);
    if(c >= 0 && c < IEXC_NUM) id->context = c;
}

void sealcontexts() { contextsealed = true; }

COMMAND(scriptcontext, ARG_2STR);
COMMAND(sealcontexts, ARG_NONE);

