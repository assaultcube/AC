// command.cpp: implements the parsing and execution of a tiny script language which
// is largely backwards compatible with the quake console language.

#include "cube.h"

#define CSLIMIT_LOOKUPNESTING 3         // fails silently
#define CSLIMIT_RECURSION 333           // rarely depths over 20 will be reached and setting this one over 10000 won't prevent crashes anymore
#define CSLIMIT_PUSHLEVEL 33            // beware: also grows with alias execution nesting as arg1 gets pushed
#define CSLIMIT_ITERATION 33333         // determines length of endlees loop (while 1)
#define CSLIMIT_STRINGLEN 333333        // yet another arbitrary limit

inline bool identaccessdenied(ident *id);
void cslimiterr(const char*msg);

char *exchangestr(char *o, const char *n) { delete[] o; return newstring(n); }

vector<const char *> executionstack;                    // keep history of recursive command execution (to write to log in case of a crash)
char *commandret = NULL;

bool loop_break = false, loop_skip = false;             // break or continue (skip) current loop
int loop_level = 0;                                     // avoid bad calls of break & continue

hashtable<const char *, ident> *idents = NULL;          // contains ALL vars/commands/aliases

bool persistidents = false;
bool currentcontextisolated = false;                    // true for map and model config files

COMMANDF(persistidents, "s", (char *on)
{
    if(on && *on) persistidents = ATOI(on) != 0;
    intret(persistidents ? 1 : 0);
});

void clearstack(identstack *&stack)
{
    while(stack)
    {
        delete[] stack->action;
        identstack *tmp = stack;
        stack = stack->next;
        delete tmp;
    }
}

void pushident(ident &id, char *val, int context = execcontext)
{
    if(id.type != ID_ALIAS) { delete[] val; return; }
    int d = 0;
    for(identstack *s = id.stack; s; s = s->next, d++) { if(d > CSLIMIT_PUSHLEVEL) { cslimiterr("push level"); delete[] val; return; } }
    identstack *stack = new identstack;
    stack->action = id.executing==id.action ? newstring(id.action) : id.action;
    stack->context = id.context;
    stack->next = id.stack;
    id.stack = stack;
    id.action = val;
    id.context = context;
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

ident *newident(const char *name, int context = execcontext)
{
    ident *id = idents->access(name);
    if(!id)
    {
        ident init(ID_ALIAS, newstring(name), newstring(""), persistidents, context);
        id = &idents->access(init.name, init);
    }
    return id;
}

void pusha(const char *name, char *action)
{
    ident *id = newident(name, execcontext);
    if(identaccessdenied(id))
    {
        conoutf("cannot redefine alias %s in this execution context", id->name);
        scripterr();
        return;
    }
    pushident(*id, action);
}

void push(const char *name, const char *action)
{
    pusha(name, newstring(action));
}
COMMAND(push, "ss");

void pop(const char *name)
{
    ident *id = idents->access(name);
    if(!id || id->type != ID_ALIAS) conoutf("unknown alias %s", name);
    else if(identaccessdenied(id))  conoutf("cannot redefine alias %s in this execution context", name);
    else if(!id->stack)             conoutf("ident stack exhausted");
    else
    {
        popident(*id);
        return;
    }
    scripterr();
}

COMMANDF(pop, "v", (char **args, int numargs)
{
    if(numargs > 0)
    {
        const char *beforepopval = getalias(args[0]);
        if(beforepopval) commandret = newstring(beforepopval);
    }
    loopi(numargs) pop(args[i]);
});

void delalias(const char *name)
{
    ident *id = idents->access(name);
    if(!id || id->type != ID_ALIAS) conoutf("unknown alias %s", name);
    else if(identaccessdenied(id))  conoutf("cannot remove alias %s in this execution context", id->name);
    else
    {
        clearstack(id->stack);
        idents->remove(name);
        return;
    }
    scripterr();
}
COMMAND(delalias, "s");

void alias(const char *name, const char *action, bool temp, bool constant)
{
    ident *b = idents->access(name);
    if(!b)
    { // new alias
        ident b(ID_ALIAS, newstring(name), newstring(action), persistidents && !constant && !temp, execcontext);
        b.isconst = constant;
        b.istemp = temp;
        idents->access(b.name, b);
        return;
    }
    else if(b->type != ID_ALIAS)  conoutf("cannot redefine builtin %s with an alias", name);
    else if(identaccessdenied(b)) conoutf("cannot redefine alias %s in this execution context", name);
    else if(b->isconst)           conoutf("alias %s is a constant and cannot be redefined", name);
    else
    { // new action
        b->isconst = constant;
        if(temp) b->istemp = true;
        if(!constant || (action && action[0]))
        {
            if(b->action != b->executing) delete[] b->action;
            b->action = newstring(action);
            if(!b->stack) b->persist = persistidents != 0;
        }
        if(b->istemp) b->persist = false;
        return;
    }
    scripterr();
}

COMMANDF(alias, "ss", (const char *name, const char *action) { alias(name, action, false, false); });
COMMANDF(tempalias, "ss", (const char *name, const char *action) { alias(name, action, true, false); });
COMMANDF(const, "ss", (const char *name, const char *action) { alias(name, action, false, true); });

COMMANDF(checkalias, "s", (const char *name) { intret(getalias(name) ? 1 : 0); });
COMMANDF(isconst, "s", (const char *name) { ident *id = idents->access(name); intret(id && id->isconst ? 1 : 0); });

// variable's and commands are registered through globals, see cube.h

int variable(const char *name, int minval, int cur, int maxval, int *storage, void (*fun)(), bool persist)
{
    if(!idents) idents = new hashtable<const char *, ident>;
    ident v(ID_VAR, name, minval, maxval, storage, cur, fun, persist, IEXC_CORE);
    idents->access(name, v);
    return cur;
}

float fvariable(const char *name, float minval, float cur, float maxval, float *storage, void (*fun)(), bool persist)
{
    if(!idents) idents = new hashtable<const char *, ident>;
    ident v(ID_FVAR, name, minval, maxval, storage, cur, fun, persist, IEXC_CORE);
    idents->access(name, v);
    return cur;
}

char *svariable(const char *name, const char *cur, char **storage, void (*fun)(), void (*getfun)(), bool persist)
{
    if(!idents) idents = new hashtable<const char *, ident>;
    ident v(ID_SVAR, name, storage, fun, getfun, persist, IEXC_CORE);
    idents->access(name, v);
    return newstring(cur);
}

#define GETVAR(id, vartype, name) \
    ident *id = idents->access(name); \
    ASSERT(id && id->type == vartype); \
    if(!id || id->type!=vartype) return;
void setvar(const char *name, int i, bool dofunc)
{
    GETVAR(id, ID_VAR, name);
    *id->storage.i = clamp(i, id->minval, id->maxval);
    if(dofunc && id->fun) ((void (__cdecl *)())id->fun)();            // call trigger function if available
}
void setfvar(const char *name, float f, bool dofunc)
{
    GETVAR(id, ID_FVAR, name);
    *id->storage.f = clamp(f, id->minvalf, id->maxvalf);
    if(dofunc && id->fun) ((void (__cdecl *)())id->fun)();            // call trigger function if available
}
void setsvar(const char *name, const char *str, bool dofunc)
{
    GETVAR(id, ID_SVAR, name);
    *id->storage.s = exchangestr(*id->storage.s, str);
    if(dofunc && id->fun) ((void (__cdecl *)())id->fun)();            // call trigger function if available
}

bool identexists(const char *name) { return idents->access(name)!=NULL; }

const char *getalias(const char *name)
{
    ident *i = idents->access(name);
    return i && i->type==ID_ALIAS ? i->action : NULL;
}
void _getalias(char *name)
{
    string o = "";
    const char *res = o;
    ident *id = idents->access(name);
    if(id)
    {
        switch(id->type)
        {
            case ID_VAR:
                formatstring(o)("%d", *id->storage.i);
                break;
            case ID_FVAR:
                formatstring(o)("%.3f", *id->storage.f);
                break;
            case ID_SVAR:
                if(id->getfun) ((void (__cdecl *)())id->getfun)();
                res = *id->storage.s;
                break;
            case ID_ALIAS:
                if(id->action) res = id->action;
                break;
        }
    }
    result(res);
}
COMMANDN(getalias, _getalias, "s");

#ifndef STANDALONE
void getvarrange(char *_what, char *name)
{
    ident *id = idents->access(name);
    const char *attrs[] = { "min", "max", "default", "" };
    int what = getlistindex(_what, attrs, false, -1);
    if(id)
    {
        int *i = NULL;
        switch(what)
        {
            case 0: i = &(id->minval); break;
            case 1: i = &(id->maxval); break;
            case 2: i = &(id->defaultval); break;
        }
        if(i) switch(id->type)
        {
            case ID_VAR: intret(*i); return;
            case ID_FVAR: floatret(*((float *) i), true); return;
        }
    }
    result("");
}
COMMAND(getvarrange, "ss");
#endif

COMMANDF(isIdent, "s", (char *name) { intret(identexists(name) ? 1 : 0); });

bool addcommand(const char *name, void (*fun)(), const char *sig)
{
    ASSERT(strlen(sig) < 9);
    if(!idents) idents = new hashtable<const char *, ident>;
    ident c(ID_COMMAND, name, fun, sig, IEXC_CORE);
    idents->access(name, c);
    return false;
}

inline char *parsequotes(const char *&p)
{
    const char *word = p + 1;
    do
    {
        p++;
        p += strcspn(p, "\"\n\r");
    }
    while(*p == '\"' && p[-1] == '\\');                                 // skip escaped quotes
    char *s = newstring(word, p - word);
#ifndef STANDALONE
    filterrichtext(s, s, p - word);
#endif
    if(*p=='\"') p++;
    return s;
}

char *lookup(char *n, int levels);

char *parseexp(const char *&p, int right, int rec)             // parse any nested set of () or []
{
    if(rec > CSLIMIT_RECURSION) { cslimiterr("recursion depth"); return NULL; }
    vector<char> res;
    int left = *p++;
    const char *word = p;
    char *s;
    bool quot = false, issq = left == '[';
    for(int brak = 1; brak; )
    {
        p += strcspn(p, "([\"])@");
        int c = *p++;
        if(c==left && !quot) brak++;
        else if(c=='"') quot = !quot;
        else if(c==right && !quot) brak--;
        else if(!c)
        {
            p--;
            conoutf("missing \"%c\"", right);
            goto screrr;
        }
        else if(issq && c == '@' && !quot)
        {
            const char *sq = p;
            while(*p == '@') p++;
            int level = p - sq + 1;
            if(level == brak)
            {
                res.put(word, sq - word - 1);
                char *n, a = *p;
                switch(a)
                {
                    case '(': n = parseexp(p, ')', rec + 1); break;
                    case '"': n = parsequotes(p); break;
                    case '[':
                        n = parseexp(p, ']', rec + 1);
                        if(n) n = lookup(n, 1);
                        break;
                    default:
                    {
                        const char *w = p;
                        p += strcspn(p, "])@; \t\n\r");
                        n = lookup(newstring(w, p - w), 1);
                        break;
                    }
                }
                if(n) res.put(n, strlen(n)), delete[] n;
                word = p;
            }
            else if(level > brak) { conoutf("too many @"); goto screrr; }
        }
    }
    if(res.length())
    {
        res.put(word, p - word - 1);
        if(res.length() > CSLIMIT_STRINGLEN) { cslimiterr("string length"); return newstring(""); }
        return newstring(res.getbuf(), res.length());
    }
    s = newstring(word, p-word-1);
    if(left=='(')
    {
        char *ret = executeret(s); // evaluate () exps directly, and substitute result
        delete[] s;
        s = ret ? ret : newstring("");
    }
    return s;

screrr:
    scripterr();
    flagmapconfigerror(LWW_SCRIPTERR * 4);
    return NULL;
}

char *lookup(char *n, int levels)                           // find value of ident referenced with $ in exp
{
    if(levels > 1) n = exchangestr(n, lookup(newstring(n), min(levels - 1, CSLIMIT_LOOKUPNESTING - 1))); // nested ("$$var"), limit to three levels
    ident *id = idents->access(n);
    if(id) switch(id->type)
    {
        case ID_VAR: { string t; itoa(t, *id->storage.i); return exchangestr(n, t); }
        case ID_FVAR: return exchangestr(n, floatstr(*id->storage.f));
        case ID_SVAR: { { if(id->getfun) ((void (__cdecl *)())id->getfun)(); } return exchangestr(n, *id->storage.s); }
        case ID_ALIAS: return exchangestr(n, id->action);
    }
    conoutf("unknown alias lookup: %s", n);
    scripterr();
    flagmapconfigerror(LWW_SCRIPTERR * 4);
    return n;
}

char *parseword(const char *&p, int arg, int *infix, int rec)                       // parse single argument, including expressions
{
    p += strspn(p, " \t");
    if(p[0]=='/' && p[1]=='/') p += strcspn(p, "\n\r\0");
    if(*p=='"') return parsequotes(p);
    if(*p=='(' && !currentcontextisolated) return parseexp(p, ')', rec + 1);
    if(*p=='[' && !currentcontextisolated) return parseexp(p, ']', rec + 1);
    int lvls = currentcontextisolated ? 0 : strspn(p, "$");
    if(lvls && (p[lvls]=='(' || p[lvls]=='['))
    { // $() $[]
        p += lvls;
        char *b = parseexp(p, *p == '(' ? ')' : ']', rec + 1);
        if(b) return lookup(b, lvls);
    }
    const char *word = p;
    p += strcspn(p, "; \t\n\r\0");
    if(p-word==0) return NULL;
    if(arg == 1 && *word == '=' && p - word == 1) *infix = *word;
    return lvls ? lookup(newstring(word + lvls, p - word - lvls), lvls) : newstring(word, p-word);
}

char *conc(const char **w, int n, bool space)
{
    if(n < 0)
    {  // auto-determine number of strings
        n = 0;
        while(w[n] && w[n][0]) n++;
    }
    static vector<int> wlen;
    wlen.setsize(0);
    int len = space ? max(n-1, 0) : 0;
    loopj(n) len += wlen.add((int)strlen(w[j]));
    if(len > CSLIMIT_STRINGLEN) { cslimiterr("string length"); return newstring(""); }
    char *r = newstring("", len), *res = r;
    loopi(n)
    {
        strncpy(r, w[i], wlen[i]);  // make string-list out of all arguments
        r += wlen[i];
        if(space) *r++ = ' ';
    }
    if(space && n) --r;
    *r = '\0';
    return res;
}

VARN(numargs, _numargs, MAXWORDS, 0, 0);

void intret(int v)
{
    string t;
    itoa(t, v);
    commandret = newstring(t);
}

const char *floatstr(float v, bool neat)
{
    static char s[2 * MAXSTRLEN];
    static int i = 0;
    if(i > MAXSTRLEN - 10) i = 0;
    char *t = s + i;
    formatstring(t)(!neat && (v) == int(v) ? "%.1f" : "%.7g", v);  // was ftoa()
    i += strlen(t) + 1;
    return t;
}

void floatret(float v, bool neat)
{
    commandret = newstring(floatstr(v, neat));
}

void result(const char *s) { commandret = newstring(s); }
COMMAND(result, "s");

void resultcharvector(const vector<char> &res, int adj)     // use char vector as result, optionally remove some bytes at the end
{
    adj += res.length();
    if(adj > CSLIMIT_STRINGLEN) { cslimiterr("string length"); commandret = newstring(""); return; }
    commandret = adj <= 0 ? newstring("", 0) : newstring(res.getbuf(), (size_t) adj);
}

char *executeret(const char *p)                 // all evaluation happens here, recursively
{
    if(!p || !p[0]) return NULL;
    if(executionstack.length() > CSLIMIT_RECURSION) { cslimiterr("recursion depth"); return NULL; }
    executionstack.add(p);
    char *w[MAXWORDS], emptychar = '\0';
    char *retval = NULL;
    #define setretval(v) { char *rv = v; if(rv) retval = rv; }
    for(bool cont = true; cont;)                // for each ; seperated statement
    {
        if(loop_level && loop_skip) break;
        int numargs = MAXWORDS, infix = 0;
        loopi(MAXWORDS)                         // collect all argument values
        {
            w[i] = &emptychar;
            if(i>numargs) continue;
            char *s = parseword(p, i, &infix, executionstack.length());   // parse and evaluate exps
            if(s) w[i] = s;
            else numargs = i;
        }

        p += strcspn(p, ";\n\r\0");
        cont = *p++!=0;                         // more statements if this isn't the end of the string
        const char *c = w[0];
        if(!*c) continue;                       // empty statement

        DELETEA(retval);

        if(infix == '=')
        {
            DELETEA(w[1]);
            swap(w[0], w[1]);
            c = "alias";
        }

        ident *id = idents->access(c);
        if(!id)
        {
            if(!isdigit(*c) && ((*c!='+' && *c!='-' && *c!='.') || !isdigit(c[1])))
            {
                conoutf("unknown command: %s", c);
                scripterr();
                flagmapconfigerror(LWW_SCRIPTERR * 4);
            }
            setretval(newstring(c));
        }
        else if(identaccessdenied(id))
        {
            conoutf("not allowed in this execution context: %s", id->name);
            scripterr();
            flagmapconfigerror(LWW_SCRIPTERR * 4);
        }
        else
        {
            switch(id->type)
            {
                case ID_COMMAND:                    // game defined commands
                {

                    if(strstr(id->sig, "v")) ((void (__cdecl *)(char **, int))id->fun)(&w[1], numargs-1);
                    else if(strstr(id->sig, "c") || strstr(id->sig, "w"))
                    {
                        char *r = conc((const char **)w+1, numargs-1, strstr(id->sig, "c") != NULL);
                        ((void (__cdecl *)(char *))id->fun)(r);
                        delete[] r;
                    }
                    else if(strstr(id->sig, "d"))
                    {
#ifndef STANDALONE
                        ((void (__cdecl *)(bool))id->fun)(addreleaseaction(id->name)!=NULL);
#endif
                    }
                    else
                    {
                        int ib1, ib2, ib3, ib4, ib5, ib6, ib7, ib8;
                        float fb1, fb2, fb3, fb4, fb5, fb6, fb7, fb8;
                        #define ARG(i) (id->sig[i-1] == 'i' ? ((void *)&(ib##i=strtol(w[i], NULL, 0))) : (id->sig[i-1] == 'f' ? ((void *)&(fb##i=atof(w[i]))) : (void *)w[i]))

                        switch(strlen(id->sig))                // use very ad-hoc function signature, and just call it
                        {
                            case 0: ((void (__cdecl *)())id->fun)(); break;
                            case 1: ((void (__cdecl *)(void*))id->fun)(ARG(1)); break;
                            case 2: ((void (__cdecl *)(void*, void*))id->fun)(ARG(1), ARG(2)); break;
                            case 3: ((void (__cdecl *)(void*, void*, void*))id->fun)(ARG(1), ARG(2), ARG(3)); break;
                            case 4: ((void (__cdecl *)(void*, void*, void*, void*))id->fun)(ARG(1), ARG(2), ARG(3), ARG(4)); break;
                            case 5: ((void (__cdecl *)(void*, void*, void*, void*, void*))id->fun)(ARG(1), ARG(2), ARG(3), ARG(4), ARG(5)); break;
                            case 6: ((void (__cdecl *)(void*, void*, void*, void*, void*, void*))id->fun)(ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6)); break;
                            case 7: ((void (__cdecl *)(void*, void*, void*, void*, void*, void*, void*))id->fun)(ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7)); break;
                            case 8: ((void (__cdecl *)(void*, void*, void*, void*, void*, void*, void*, void*))id->fun)(ARG(1), ARG(2), ARG(3), ARG(4), ARG(5), ARG(6), ARG(7), ARG(8)); break;
                            default: fatal("command %s has too many arguments (signature: %s)", id->name, id->sig); break;
                        }
                        #undef ARG
                    }

                    setretval(commandret);
                    commandret = NULL;
                    break;
                }

                case ID_VAR:                        // game defined variables
                    if(!w[1][0]) conoutf("%s = %d", c, *id->storage.i);      // var with no value just prints its current value
                    else if(id->minval>id->maxval) conoutf("variable %s is read-only", id->name);
                    else
                    {
                        int i1 = ATOI(w[1]);
                        if(i1<id->minval || i1>id->maxval)
                        {
                            i1 = i1<id->minval ? id->minval : id->maxval;       // clamp to valid range
                            conoutf("valid range for %s is %d..%d", id->name, id->minval, id->maxval);
                            flagmapconfigerror(LWW_SCRIPTERR);
                        }
                        *id->storage.i = i1;
                        if(id->fun) ((void (__cdecl *)())id->fun)();            // call trigger function if available
                    }
                    break;

                case ID_FVAR:                        // game defined variables
                    if(!w[1][0]) conoutf("%s = %s", c, floatstr(*id->storage.f));      // var with no value just prints its current value
                    else if(id->minvalf>id->maxvalf) conoutf("variable %s is read-only", id->name);
                    else
                    {
                        float f1 = atof(w[1]);
                        if(f1<id->minvalf || f1>id->maxvalf)
                        {
                            f1 = f1<id->minvalf ? id->minvalf : id->maxvalf;       // clamp to valid range
                            conoutf("valid range for %s is %s..%s", id->name, floatstr(id->minvalf), floatstr(id->maxvalf));
                            flagmapconfigerror(LWW_SCRIPTERR * 2);
                        }
                        *id->storage.f = f1;
                        if(id->fun) ((void (__cdecl *)())id->fun)();            // call trigger function if available
                    }
                    break;

                case ID_SVAR:                        // game defined variables
                    if(!w[1][0])
                    {
                        if(id->getfun) ((void (__cdecl *)())id->getfun)();
                        conoutf(strchr(*id->storage.s, '"') ? "%s = [%s]" : "%s = \"%s\"", c, *id->storage.s); // var with no value just prints its current value
                    }
                    else
                    {
                        *id->storage.s = exchangestr(*id->storage.s, newstring(w[1]));
                        if(id->fun) ((void (__cdecl *)())id->fun)();            // call trigger function if available
                    }
                    break;

                case ID_ALIAS:                              // alias, also used as functions and (global) variables
                    delete[] w[0];
                    static vector<ident *> argids;
                    for(int i = 1; i<numargs; i++)
                    {
                        if(i > argids.length())
                        {
                            defformatstring(argname)("arg%d", i);
                            argids.add(newident(argname, IEXC_CORE));
                        }
                        pushident(*argids[i-1], w[i]); // set any arguments as (global) arg values so functions can access them
                    }
                    int old_numargs = _numargs;
                    _numargs = numargs-1;
                    char *wasexecuting = id->executing;
                    id->executing = id->action;
                    setretval(executeret(id->action));
                    if(id->executing!=id->action && id->executing!=wasexecuting) delete[] id->executing;
                    id->executing = wasexecuting;
                    _numargs = old_numargs;
                    for(int i = 1; i<numargs; i++) popident(*argids[i-1]);
                    continue;
            }
        }
        loopj(numargs) if(w[j]) delete[] w[j];
    }
    executionstack.pop();
    return retval;
}

int execute(const char *p)
{
    char *ret = executeret(p);
    int i = 0;
    if(ret) { i = ATOI(ret); delete[] ret; }
    return i;
}

#ifndef STANDALONE
bool exechook(int context, const char *ident, const char *body,...)  // execute cubescript hook if available and allowed in current context/gamemode
{ // always use one of HOOK_SP_MP, HOOK_SP or HOOK_MP and then OR them (as needed) with HOOK_TEAM, HOOK_NOTEAM, HOOK_BOTMODE, HOOK_FLAGMODE, HOOK_ARENA
    if(multiplayer(NULL) && (context & HOOK_FLAGMASK) != HOOK_MP && (context & HOOK_FLAGMASK) != HOOK_SP_MP) return false; // hook is singleplayer-only
    if(((context & HOOK_TEAM) && !m_teammode) ||
       ((context & HOOK_NOTEAM) && m_teammode) ||
       ((context & HOOK_BOTMODE) && !m_botmode) ||
       ((context & HOOK_FLAGMODE) && m_flags) ||
       ((context & HOOK_ARENA) && m_arena)) return false; // wrong gamemode
    if(identexists(ident))
    {
        defvformatstring(arglist, body, body);
        defformatstring(execbody)("%s%c%s", ident, *arglist ? ' ' : '\0', arglist);
        setcontext("hook", execbody);
        execute(execbody);
        resetcontext();
        return true;
    }
    return false;
}

void identhash(uint64_t *d)
{
    enumerate(*idents, ident, id, if(id.type == ID_COMMAND || id.type == ID_VAR) { *d ^= id.type == ID_VAR ? (size_t)"" - (size_t)id.name : (size_t)identhash - (size_t)id.fun; *d *= 16777619; });
}

// tab-completion of all idents
// always works at the end of the command line - the cursor position does not matter

static int completesize = -1, nickcompletesize = -1;

void resetcomplete()
{
    nickcompletesize = completesize = -1;
}

bool nickcomplete(char *s, bool reversedirection)
{
    static int nickcompleteidx;

    char *cp = strrchr(s, ' '); // find last space
    cp = cp ? cp + 1 : s;

    if(nickcompletesize < 0)
    {
        nickcompletesize = (int)strlen(cp);
        nickcompleteidx = reversedirection ? 0 : -1;
    }

    vector<int> matchingnames;
    vector<const char *> matchingigs;
    loopv(players) if(players[i] && !strncasecmp(players[i]->name, cp, nickcompletesize)) matchingnames.add(i);   // find all matching player names first
    if(nickcompletesize > 0 && *cp == ':') enumigraphs(matchingigs, cp + 1, nickcompletesize - 1); // find all matching igraphs
    int totalmatches = matchingnames.length() + matchingigs.length();
    if(totalmatches)
    {
        nickcompleteidx += reversedirection ? totalmatches - 1 : 1;
        nickcompleteidx %= totalmatches;
        bool isig = nickcompleteidx >= matchingnames.length();
        const char *fillin = isig ? matchingigs[nickcompleteidx - matchingnames.length()] : players[matchingnames[nickcompleteidx]]->name;
        if(*fillin == '/' && cp == s) *cp++ = ' ';
        if(isig) cp++;
        *cp = '\0';
        concatstring(s, fillin);
        return true;
    }
    return false;
}

enum { COMPLETE_FILE = 0, COMPLETE_LIST, COMPLETE_NICK };

struct completekey
{
    int type;
    const char *dir, *ext;

    completekey() {}
    completekey(int type, const char *dir, const char *ext) : type(type), dir(dir), ext(ext) {}
};

struct completeval
{
    int type;
    char *dir, *ext;
    vector<char *> dirlist;
    vector<char *> list;

    completeval(int type, const char *dir, const char *ext) : type(type), dir(dir && dir[0] ? newstring(dir) : NULL), ext(ext && ext[0] ? newstring(ext) : NULL) {}
    ~completeval() { DELETEA(dir); DELETEA(ext); dirlist.deletearrays(); list.deletearrays(); }
};

static inline bool htcmp(const completekey &x, const completekey &y)
{
    return x.type==y.type && (x.dir == y.dir || (x.dir && y.dir && !strcmp(x.dir, y.dir))) && (x.ext == y.ext || (x.ext && y.ext && !strcmp(x.ext, y.ext)));
}

static inline uint hthash(const completekey &k)
{
    return k.dir ? hthash(k.dir) + k.type : k.type;
}

static hashtable<completekey, completeval *> completedata;
static hashtable<char *, completeval *> completions;

void addcomplete(char *command, int type, char *dir, char *ext)
{
    if(type==COMPLETE_FILE)
    {
        int dirlen = (int)strlen(dir);
        while(dirlen > 0 && (dir[dirlen-1] == '/' || dir[dirlen-1] == '\\'))
            dir[--dirlen] = '\0';
        if(ext)
        {
            if(strchr(ext, '*')) ext[0] = '\0';
            if(!ext[0]) ext = NULL;
        }
    }
    completekey key(type, dir, ext);
    completeval **val = completedata.access(key);
    if(!val)
    {
        completeval *f = new completeval(type, dir, ext);
        if(type==COMPLETE_LIST) explodelist(dir, f->list);
        if(type==COMPLETE_FILE)
        {
            explodelist(dir, f->dirlist);
            loopv(f->dirlist)
            {
                char *dir = f->dirlist[i];
                int dirlen = (int)strlen(dir);
                while(dirlen > 0 && (dir[dirlen-1] == '/' || dir[dirlen-1] == '\\'))
                    dir[--dirlen] = '\0';
            }
        }
        val = &completedata[completekey(type, f->dir, f->ext)];
        *val = f;
    }
    completeval **hascomplete = completions.access(command);
    if(hascomplete) *hascomplete = *val;
    else completions[newstring(command)] = *val;
}

void addfilecomplete(char *command, char *dir, char *ext)
{
    addcomplete(command, COMPLETE_FILE, dir, ext);
}

void addlistcomplete(char *command, char *list)
{
    addcomplete(command, COMPLETE_LIST, list, NULL);
}

void addnickcomplete(char *command)
{
    addcomplete(command, COMPLETE_NICK, NULL, NULL);
}

COMMANDN(complete, addfilecomplete, "sss");
COMMANDN(listcomplete, addlistcomplete, "ss");
COMMANDN(nickcomplete, addnickcomplete, "s");

void commandcomplete(char *s, bool reversedirection)
{ // s is required to be of size "string"!
    static int completeidx;
    if(*s != '/')
    {
        string t;
        copystring(t, s);
        copystring(s, "/");
        concatstring(s, t);
    }
    if(!s[1]) return;

    // find start position of last command
    char *cmd = strrchr(s, ';'); // find last ';' (this will not always work properly, because it doesn't take quoted texts into account)
    if(!cmd) cmd = s;  // no ';' found: command starts after '/'

    char *openblock = strrchr(cmd + 1, '('), *closeblock = strrchr(cmd + 1, ')'); // find last open and closed parenthesis
    if(openblock && (!closeblock || closeblock < openblock)) cmd = openblock; // found opening parenthesis inside the command: assume, a new command starts here

    cmd += strspn(cmd + 1, " ") + 1;  // skip blanks and one of "/;( ", cmd now points to the first char of the command

    // check, if the command is complete, and we want argument completion instead
    char *arg = strrchr(cmd, ' ');  // find last space in command -> if there is one, we use argument completion

    completeval *cdata = NULL;
    if(arg)  // full command is present
    { // extract command name to find argument list
        string command;
        copystring(command, cmd);
        command[strcspn(cmd, " ")] = '\0';
        completeval **hascomplete = completions.access(command);
        if(hascomplete) cdata = *hascomplete;

        if(completesize < 0 && cdata && cdata->type == COMPLETE_FILE)
        { // get directory contents on first run
           cdata->list.deletearrays();
           vector<char *> files;
           loopv(cdata->dirlist)
           {
               listfiles(cdata->dirlist[i], cdata->ext, files, stringsort);
               loopv(files) cdata->list.add(files[i]);
               files.setsize(0);
           }
        }
    }

    char *cp = arg ? arg + 1 : cmd; // part of string to complete
    bool firstrun = false;
    if(completesize < 0)
    { // first run since resetcomplete()
        completesize = (int)strlen(cp);
        completeidx = reversedirection ? 0 : -1;
        firstrun = true;
    }

    if(!arg)
    { // commandname completion
        vector<const char *> matchingidents;
        enumerate(*idents, ident, id,
            if(!strncasecmp(id.name, cp, completesize) && (id.type != ID_ALIAS || *id.action)) matchingidents.add(id.name);     // find all matching possibilities to get the list length (and give an opportunity to sort the list first)
        );
        if(matchingidents.length())
        {
            completeidx += reversedirection ? matchingidents.length() - 1 : 1;
            completeidx %= matchingidents.length();
            matchingidents.sort(stringsortignorecase);
            if(firstrun && !reversedirection && !strcmp(matchingidents[completeidx], cp)) completeidx = min(completeidx + 1, matchingidents.length() - 1);
            *cp = '\0';
            concatstring(s, matchingidents[completeidx]);
        }
    }
    else if(!cdata) return;
    else if(cdata->type == COMPLETE_NICK) nickcomplete(s, reversedirection);
    else
    { // argument completion
        vector<int> matchingargs;
        loopv(cdata->list) if(!strncasecmp(cdata->list[i], cp, completesize)) matchingargs.add(i);   // find all matching args first
        if(matchingargs.length())
        {
            completeidx += reversedirection ? matchingargs.length() - 1 : 1;
            completeidx %= matchingargs.length();
            *cp = '\0';
            concatstring(s, cdata->list[matchingargs[completeidx]]);
        }
    }
}

void complete(char *s, bool reversedirection)
{
    if(*s == '/' || !nickcomplete(s, reversedirection))
    {
        commandcomplete(s, reversedirection);
    }
}
#endif

void cleancubescript(char *buf) // drop easy to spot surplus whitespace and comments
{
    if(!buf || !*buf) return;
    char *src = buf, c;
    bool quot = false;
    for(char l = 0; (c = *src++); l = c)
    {
        if(c == '\r') c = '\n';
        if(quot)
        {
            if((c == '"' && l != '\\') || c == '\n') quot = false;
        }
        else
        {
            if(c == l && isspace(c)) continue;
            if(c == ' ' && l == '\n') continue;
            if(c == '/' && *src == '/') { src += strcspn(src, "\n\r\0"); continue; }
            if(c == '"') quot = true;
        }
        *buf++ = c;
    }
    *buf = '\0';
}

bool execfile(const char *cfgfile)
{
    string s;
    copystring(s, cfgfile);
    setcontext("file", cfgfile);
    bool oldpersist = persistidents;
    char *buf = loadfile(path(s), NULL);
    if(!buf)
    {
        resetcontext();
        return false;
    }
    if(!currentcontextisolated && !strstr(s, "docs.cfg")) cleancubescript(buf);
    execute(buf);
    delete[] buf;
    resetcontext();
    persistidents = oldpersist;
    return true;
}

void exec(const char *cfgfile)
{
    if(!execfile(cfgfile)) conoutf("could not read \"%s\"", cfgfile);
}

COMMANDF(exec, "v", (char **args, int numargs)
{
    defformatstring(buf)("%d", numargs - 1);
    push("execnumargs", buf);
    for(int i = 1; i <= MAXWORDS - 2; i++)
    {
        formatstring(buf)("execarg%d", i);
        push(buf, i < numargs ? args[i] : "");
    }
    push("execresult", "");
    exec(numargs > 0 ? args[0] : "");
    const char *res = getalias("execresult");
    if(res) result(res);
    pop("execresult");
    for(int i = MAXWORDS - 2; i >= 1; i--)
    {
        formatstring(buf)("execarg%d", i);
        pop(buf);
    }
    pop("execnumargs");
});

COMMANDF(execute, "s", (char *s) { intret(execute(s)); });

void execdir(const char *dir)
{
    if(dir[0])
    {
        vector<char *> files;
        listfiles(dir, "cfg", files, stringsort);
        loopv(files)
        {
            defformatstring(d)("%s/%s.cfg",dir,files[i]);
            exec(d);
            delstring(files[i]);
        }
    }
}
COMMAND(execdir, "s");

// below the commands that implement a small imperative language. thanks to the semantics of
// () and [] expressions, any control construct can be defined trivially.

void ifthen(char *cond, char *thenp, char *elsep) { commandret = executeret(cond[0]!='0' ? thenp : elsep); }
bool __dummy_ifthen = addcommand("if", (void (*)())ifthen, "sss");
//COMMANDN(if, ifthen, "sss");  // CB seriously trips over this one ;)

void loopa(char *var, int *times, char *body)
{
    if(*times > CSLIMIT_ITERATION) { cslimiterr("loop iterations"); return; }
    int t = *times;
    if(t<=0) return;
    ident *id = newident(var, execcontext);
    if(id->type!=ID_ALIAS) return;
    char *buf = newstring("0", 16);
    pushident(*id, buf);
    loop_level++;
    execute(body);
    loop_skip = false;
    if(loop_break) loop_break = false;
    else
    {
        loopi(t-1)
        {
            if(buf != id->action)
            {
                if(id->action != id->executing) delete[] id->action;
                id->action = buf = newstring(16);
            }
            itoa(id->action, i+1);
            execute(body);
            loop_skip = false;
            if(loop_break)
            {
                loop_break = false;
                break;
            }
        }
    }
    popident(*id);
    loop_level--;
}
COMMANDN(loop, loopa, "sis");

void whilea(char *cond, char *body)
{
    loop_level++;
    int its = 0;
    while(execute(cond))
    {
        execute(body);
        loop_skip = false;
        if(loop_break)
        {
            loop_break = false;
            break;
        }
        if(++its > CSLIMIT_ITERATION) { cslimiterr("loop iterations"); return; }
    }
    loop_level--;
}
COMMANDN(while, whilea, "ss");

COMMANDF(break, "", () { if(loop_level) loop_skip = loop_break = true; });
COMMANDF(continue, "", () { if(loop_level) loop_skip = true; });

COMMANDF(concat, "c", (const char *s) { result(s); });
COMMANDF(concatword, "w", (const char *s) { result(s); });

void format(char **args, int numargs)
{
    vector<char> s;
    const char *f = numargs > 0 ? args[0] : "";
    while(*f)
    {
        int c = *f++;
        if(c == '%')
        {
            int i = *f++;
            bool twodigit = i == '0' && isdigit(f[0]) && isdigit(f[1]);
            if((i >= '1' && i <= '9') || twodigit)
            {
                if(twodigit) i = (f[0] - '0') * 10 + f[1] - '0', f += 2;
                else i -= '0';
                const char *sub = i < numargs ? args[i] : "";
                while(*sub) s.add(*sub++);
            }
            else s.add(i);
        }
        else s.add(c);
    }
    resultcharvector(s, 0);
}
COMMAND(format, "v");

void format2(char **args, int numargs)
{
    if(numargs > 0)
    {
        vector<char *> pars;
        pars.add(newstring(args[0]));
        for(int i = 1; i < numargs ; i++) explodelist(args[i], pars);
        format(pars.getbuf(), pars.length());
        loopv(pars) delstring(pars[i]);
    }
}
COMMAND(format2, "v");

#define whitespaceskip do { s += strspn(s, "\n\r\t "); } while(s[0] == '/' && s[1] == '/' && (s += strcspn(s, "\n\r\0")))
#define elementskip { if(*s=='"') { do { ++s; s += strcspn(s, "\"\n\r"); } while(*s == '\"' && s[-1] == '\\'); s += *s=='"'; } else s += strcspn(s, "\n\r\t "); }

void explodelist(const char *s, vector<char *> &elems)
{
    whitespaceskip;
    while(*s)
    {
        const char *elem = s;
        elementskip;
#ifdef STANDALONE
        char *newelem = *elem == '"' ? newstring(elem + 1, s - elem - (s[-1]=='"' ? 2 : 1)) : newstring(elem, s-elem);
#else
        size_t len = 0;
        char *newelem = *elem == '"' ? newstring(elem + 1, (len = s - elem - (s[-1]=='"' ? 2 : 1))) : newstring(elem, s-elem);
        if(*elem == '"') filterrichtext(newelem, newelem, len);
#endif
        elems.add(newelem);
        whitespaceskip;
    }
}

void looplist(char *list, char *varlist, char *body, bool withi)
{
    vector<char *> vars;
    explodelist(varlist, vars);
    if(vars.length() < 1) return;
    int columns = vars.length();
    if(withi) vars.add(newstring("i"));
    vector<ident *> ids;
    bool ok = true;
    loopv(vars) if(ids.add(newident(vars[i]))->type != ID_ALIAS) { conoutf("looplist error: \"%s\" is readonly", vars[i]); ok = false; }
    if(ok)
    {
        int ii = 0;
        vector<char *> elems;
        explodelist(list, elems);
        loopv(ids) pushident(*ids[i], newstring(""));
        loop_level++;
        if(elems.length() / columns > CSLIMIT_ITERATION) cslimiterr("loop iterations");
        else for(int i = 0; i <= elems.length() - columns; i += columns)
        {
            loopj(vars.length())
            {
                if(ids[j]->action != ids[j]->executing) delete[] ids[j]->action;
                if(j < columns)
                {
                    ids[j]->action = elems[i + j];
                    elems[i + j] = NULL;
                }
                else
                {
                    defformatstring(sii)("%d", ii++);
                    ids[j]->action = newstring(sii);
                }
            }
            execute(body);
            loop_skip = false;
            if(loop_break) break;
        }
        loopv(ids) popident(*ids[i]);
        loopv(elems) if(elems[i]) delete[] elems[i];
        loop_break = false;
        loop_level--;
    }
    loopv(vars) delete[] vars[i];
}
COMMANDF(looplist, "sss", (char *list, char *varlist, char *body) { looplist(list, varlist, body, false); });
COMMANDF(looplisti, "sss", (char *list, char *varlist, char *body) { looplist(list, varlist, body, true); });

char *indexlist(const char *s, int pos)
{
    DEBUGCODE(if(pos < 0) conoutf("\f3warning: negative index for 'at' \"%s\" %d", s, pos);) // FIXME remove this warning before release
    if(pos < 0) return newstring("");
    whitespaceskip;
    loopi(pos)
    {
        elementskip;
        whitespaceskip;
        if(!*s) break;
    }
    const char *e = s;
    char *res;
    elementskip;
    if(*e=='"')
    {
        e++;
        if(s[-1]=='"') --s;
        res = newstring(e, s - e);
#ifndef STANDALONE
        filterrichtext(res, res, s - e);
#endif
    }
    else res = newstring(e, s-e);
    return res;
}
COMMANDF(at, "si", (char *s, int *pos) { commandret = indexlist(s, *pos); });

int listlen(const char *s)
{
    int n = 0;
    whitespaceskip;
    for(; *s; n++) { elementskip; whitespaceskip; }
    return n;
}
COMMANDF(listlen, "s", (char *l) { intret(listlen(l)); });

int find(char *s, const char *key)
{
    whitespaceskip;
    int len = strlen(key);
    for(int i = 0; *s; i++)
    {
        char *e = s;
        elementskip;
        char *a = s;
        if(*e == '"')
        {
            e++;
            if(s[-1] == '"') --s;
            if(s - e >= len)
            {
                *s = '\0';
#ifndef STANDALONE
                filterrichtext(e, e, s - e);
#endif
                if(int(strlen(e)) == len && !strncmp(e, key, len)) return i;
                *s = ' ';
            }
        }
        else if(s - e == len && !strncmp(e, key, s - e)) return i;
        s = a;
        whitespaceskip;
    }
    return -1;
}
COMMANDF(findlist, "ss", (char *s, char *key) { intret(find(s, key)); });

#ifndef STANDALONE
COMMANDF(l0, "ii", (int *p, int *v) { defformatstring(f)("%%0%dd", clamp(*p, 0, 200)); defformatstring(r)(f, *v); result(r); });
COMMANDF(h0, "ii", (int *p, int *v) { defformatstring(f)("%%0%dx", clamp(*p, 0, 200)); defformatstring(r)(f, *v); result(r); });
COMMANDF(strlen, "s", (char *s) { intret(strlen(s)); });
COMMANDF(strstr, "ss", (char *h, char *n) { char *r = strstr(h, n); intret(r ? r - h + 1 : 0); });

void substr_(char *text, int *_start, int *_len)
{
    int start = *_start, len = *_len, textlen = (int)strlen(text);
    if(start < 0) start += textlen; // negative positions count from the end
    if(len < 0) len += textlen; // negative len subtracts from total
    if(start > textlen || start < 0 || len < 0) return;

    if(!len) len = textlen - start; // zero len gets all
    if(len >= 0 && len < int(strlen(text + start))) (text + start)[len] = '\0'; // cut text at len, if too long
    result(text + start);
}
COMMANDN(substr, substr_, "sii");

void strreplace(char *text, char *search, char *replace)
{
    vector<char> buf;
    char *o = text;
    while(*search && (o = strstr(text, search)))
    {
        buf.put(text, o - text);
        buf.put(replace, strlen(replace));
        text = o + strlen(search);
    }
    buf.put(text, strlen(text));
    resultcharvector(buf, 0);
}
COMMAND(strreplace, "sss");

const char *punctnames[] = { "QUOTES", "BRACKETS", "PARENTHESIS", "_$_", "QUOTE", "PERCENT", "" };

void addpunct(char *s, char *type) // Easily inject a string into various CubeScript punctuations
{
    int t = getlistindex(type, punctnames, true, 0);
    const char *puncts[] = { "\"%s\"", "[%s]", "(%s)", "$%s", "\"", "%" }, *punct = puncts[t];
    if(strchr(punct, 's'))
    {
        defformatstring(res)(punct, s);
        result(res);
    }
    else result(punct);
}
COMMAND(addpunct, "ss");
#endif

void toany(char *s, int (*c)(int)) { while(*s) { *s = (*c)(*s); s++; } }

COMMANDF(tolower, "s", (char *s) { toany(s, tolower); result(s); });
COMMANDF(toupper, "s", (char *s) { toany(s, toupper); result(s); });

void testchar(char *s, int *type)
{
    int (*funcs[8])(int) = { isdigit, isalpha, isalnum, islower, isupper, isprint, ispunct, isspace };
    if(*type < 1 || *type > 7) *type = 0; // default to isdigit
    intret((funcs[*type])(*s) ? 1 : 0);
}
COMMAND(testchar, "si");

void sortlist(char *list)
{
    vector<char *> elems;
    explodelist(list, elems);
    elems.sort(stringsort);
    commandret = conc((const char **)elems.getbuf(), elems.length(), true);
    elems.deletearrays();
}
COMMAND(sortlist, "c");

void modifyvar(const char *name, int arg, char op)
{
    ident *id = idents->access(name);
    if(!id) return;
    if(identaccessdenied(id))
    {
        conoutf("not allowed in this execution context: %s", id->name);
        scripterr();
        return;
    }
    if((id->type == ID_VAR && id->minval > id->maxval) || (id->type == ID_FVAR && id->minvalf > id->maxvalf)) { conoutf("variable %s is read-only", id->name); return; }
    int val = 0;
    switch(id->type)
    {
        case ID_VAR: val = *id->storage.i; break;
        case ID_FVAR: val = int(*id->storage.f); break;
        case ID_SVAR: { { if(id->getfun) ((void (__cdecl *)())id->getfun)(); } val = ATOI(*id->storage.s); break; }
        case ID_ALIAS: val = ATOI(id->action); break;
        default: return;
    }
    switch(op)
    {
        case '+': val += arg; break;
        case '-': val -= arg; break;
        case '*': val *= arg; break;
        case '/': val = arg ? val / arg : 0; break;
    }
    switch(id->type)
    {
        case ID_VAR: *id->storage.i = clamp(val, id->minval, id->maxval); break;
        case ID_FVAR: *id->storage.f = clamp((float)val, id->minvalf, id->maxvalf); break;
        case ID_SVAR:  { string str; itoa(str, val); *id->storage.s = exchangestr(*id->storage.s, str); break; }
        case ID_ALIAS: { string str; itoa(str, val); alias(name, str); return; }
    }
    if(id->fun) ((void (__cdecl *)())id->fun)();
}

void addeq(char *name, int *arg) { modifyvar(name, *arg, '+'); }    COMMANDN(+=, addeq, "si");
void subeq(char *name, int *arg) { modifyvar(name, *arg, '-'); }    COMMANDN(-=, subeq, "si");
void muleq(char *name, int *arg) { modifyvar(name, *arg, '*'); }    COMMANDN(*=, muleq, "si");
void diveq(char *name, int *arg) { modifyvar(name, *arg, '/'); }    COMMANDN(div=, diveq, "si");

void modifyfvar(const char *name, float arg, char op)
{
    ident *id = idents->access(name);
    if(!id) return;
    if(identaccessdenied(id))
    {
        conoutf("not allowed in this execution context: %s", id->name);
        scripterr();
        return;
    }
    if((id->type == ID_VAR && id->minval > id->maxval) || (id->type == ID_FVAR && id->minvalf > id->maxvalf)) { conoutf("variable %s is read-only", id->name); return; }
    float val = 0;
    switch(id->type)
    {
        case ID_VAR: val = *id->storage.i; break;
        case ID_FVAR: val = *id->storage.f; break;
        case ID_SVAR: { { if(id->getfun) ((void (__cdecl *)())id->getfun)(); } val = atof(*id->storage.s); break; }
        case ID_ALIAS: val = atof(id->action); break;
        default: return;
    }
    switch(op)
    {
        case '+': val += arg; break;
        case '-': val -= arg; break;
        case '*': val *= arg; break;
        case '/': val = (arg == 0.0f) ? 0 : val / arg; break;
    }
    switch(id->type)
    {
        case ID_VAR: *id->storage.i = clamp((int)val, id->minval, id->maxval); break;
        case ID_FVAR: *id->storage.f = clamp(val, id->minvalf, id->maxvalf); break;
        case ID_SVAR: *id->storage.s = exchangestr(*id->storage.s, floatstr(val)); break;
        case ID_ALIAS: alias(name, floatstr(val)); return;
    }
    if(id->fun) ((void (__cdecl *)())id->fun)();
}

void addeqf(char *name, float *arg) { modifyfvar(name, *arg, '+'); }    COMMANDN(+=f, addeqf, "sf");
void subeqf(char *name, float *arg) { modifyfvar(name, *arg, '-'); }    COMMANDN(-=f, subeqf, "sf");
void muleqf(char *name, float *arg) { modifyfvar(name, *arg, '*'); }    COMMANDN(*=f, muleqf, "sf");
void diveqf(char *name, float *arg) { modifyfvar(name, *arg, '/'); }    COMMANDN(div=f, diveqf, "sf");

void add_(char **args, int numargs)
{
    int sum = 0;
    loopi(numargs) sum += ATOI(args[i]);
    intret(sum);
}
COMMANDN(+, add_, "v");

void mul_(char **args, int numargs)
{
    int prod = 1;
    if(numargs < 2) numargs = 2;  // emulate classic (* ) = 0, (* 2) = 0, (* 2 3) = 6
    loopi(numargs) prod *= ATOI(args[i]);
    intret(prod);
}
COMMANDN(*, mul_, "v");

void addf_(char **args, int numargs)
{
    float sum = 0;
    loopi(numargs) sum += atof(args[i]);
    floatret(sum);
}
COMMANDN(+f, addf_, "v");

void mulf_(char **args, int numargs)
{
    float prod = 1;
    if(numargs < 2) numargs = 2;  // emulate classic (* ) = 0, (* 2) = 0, (* 2 3) = 6
    loopi(numargs) prod *= atof(args[i]);
    floatret(prod);
}
COMMANDN(*f, mulf_, "v");

void sub_(int *a, int *b)      { intret(*a - *b); }             COMMANDN(-, sub_, "ii");
void div_(int *a, int *b)      { intret(*b ? (*a)/(*b) : 0); }  COMMANDN(div, div_, "ii");
void mod_(int *a, int *b)      { intret(*b ? (*a)%(*b) : 0); }  COMMANDN(mod, mod_, "ii");
void subf_(float *a, float *b) { floatret(*a - *b); }           COMMANDN(-f, subf_, "ff");
void divf_(float *a, float *b) { floatret(*b ? (*a)/(*b) : 0); }    COMMANDN(divf, divf_, "ff");
void modf_(float *a, float *b) { floatret(*b ? fmod(*a, *b) : 0); } COMMANDN(modf, modf_, "ff");
void powf_(float *a, float *b) { floatret(powf(*a, *b)); }    COMMANDN(powf, powf_, "ff");
void not_(int *a)              { intret((int)(!(*a))); }      COMMANDN(!, not_, "i");
void equal_(int *a, int *b)    { intret((int)(*a == *b)); }   COMMANDN(=, equal_, "ii");
void notequal_(int *a, int *b) { intret((int)(*a != *b)); }   COMMANDN(!=, notequal_, "ii");
void lt_(int *a, int *b)       { intret((int)(*a < *b)); }    COMMANDN(<, lt_, "ii");
void gt_(int *a, int *b)       { intret((int)(*a > *b)); }    COMMANDN(>, gt_, "ii");
void lte_(int *a, int *b)      { intret((int)(*a <= *b)); }   COMMANDN(<=, lte_, "ii");
void gte_(int *a, int *b)      { intret((int)(*a >= *b)); }   COMMANDN(>=, gte_, "ii");

void round_(float *a) { intret(int(*a + 0.5f)); }   COMMANDN(round, round_, "f");
void ceil_(float *a)  { intret((int)ceil(*a)); }    COMMANDN(ceil, ceil_, "f");
void floor_(float *a) { intret((int)floor(*a)); }   COMMANDN(floor, floor_, "f");

#define COMPAREF(opname, func, op) \
    void func(float *a, float *b) { intret((int)((*a) op (*b))); } \
    COMMANDN(opname, func, "ff")
COMPAREF(=f, equalf_, ==);
COMPAREF(!=f, notequalf_, !=);
COMPAREF(<f, ltf_, <);
COMPAREF(>f, gtf_, >);
COMPAREF(<=f, ltef_, <=);
COMPAREF(>=f, gtef_, >=);

void anda_(char *a, char *b) { intret(execute(a)!=0 && execute(b)!=0); }   COMMANDN(&&, anda_, "ss");
void ora_(char *a, char *b)  { intret(execute(a)!=0 || execute(b)!=0); }   COMMANDN(||, ora_, "ss");

void band_(int *a, int *b) { intret((*a) & (*b)); }   COMMANDN(&b, band_, "ii");
void bor_(int *a, int *b)  { intret((*a) | (*b)); }   COMMANDN(|b, bor_, "ii");
void bxor_(int *a, int *b) { intret((*a) ^ (*b)); }   COMMANDN(^b, bxor_, "ii");
void bnot_(int *a)         { intret(~(*a)); }         COMMANDN(!b, bnot_, "i");

COMMANDF(strcmp, "ss", (char *a, char *b) { intret((strcmp(a, b) == 0) ? 1 : 0); });

COMMANDF(rnd, "i", (int *a) { intret(*a>0 ? rnd(*a) : 0); });

#ifndef STANDALONE

const char *escapestring(const char *s, bool force, bool noquotes)
{
    static vector<char> strbuf[3];
    static int stridx = 0;
    if(noquotes) force = false;
    if(!s) return force ? "\"\"" : "";
    if(!force && !*(s + strcspn(s, "\"/\\;()[] \f\t\n\r$"))) return s;
    stridx = (stridx + 1) % 3;
    vector<char> &buf = strbuf[stridx];
    buf.setsize(0);
    if(!noquotes) buf.add('"');
    for(; *s; s++) switch(*s)
    {
        case '\n': buf.put("\\n", 2); break;
        case '\r': buf.put("\\n", 2); break;
        case '\t': buf.put("\\t", 2); break;
        case '\a': buf.put("\\a", 2); break;
        case '\f': buf.put("\\f", 2); break;
        case '"': buf.put("\\\"", 2); break;
        case '\\': buf.put("\\\\", 2); break;
        default: buf.add(*s); break;
    }
    if(!noquotes) buf.add('"');
    buf.add(0);
    return buf.getbuf();
}
COMMANDF(escape, "s", (const char *s) { result(escapestring(s));});

int sortident(ident **a, ident **b) { return strcasecmp((*a)->name, (*b)->name); }

void enumalias(char *prefix)
{
    vector<char> res;
    vector<ident *> sids;
    size_t np = strlen(prefix);
    enumerate(*idents, ident, id, if(id.type == ID_ALIAS && id.persist && !id.isconst && !id.istemp && !strncmp(id.name, prefix, np)) sids.add(&id); );
    sids.sort(sortident);
    loopv(sids) cvecprintf(res, "%s %s\n", escapestring(sids[i]->name, false), escapestring(sids[i]->name + np, false));
    resultcharvector(res, -1);
}
COMMAND(enumalias, "s");

VARP(omitunchangeddefaults, 0, 0, 1);
VAR(groupvariables, 0, 4, 10);

void writecfg()
{
    filerotate("config/saved", "cfg", CONFIGROTATEMAX); // keep five old config sets
    stream *f = openfile(path("config/saved.cfg", true), "w");
    if(!f) return;
    f->printf("// automatically written on exit, DO NOT MODIFY\n// delete this file to have defaults.cfg overwrite these settings\n// modify settings in game, or put settings in autoexec.cfg to override anything\n\n");
    f->printf("// basic settings\n\n");
    f->printf("name %s\n", escapestring(player1->name, false));
    f->printf("skin_cla %d\nskin_rvsf %d\n", player1->skin(TEAM_CLA), player1->skin(TEAM_RVSF));
    for(int i = CROSSHAIR_DEFAULT; i < CROSSHAIR_NUM; i++) if(crosshairs[i] && crosshairs[i] != notexture)
    {
        f->printf("loadcrosshair %s %s\n", crosshairnames[i], behindpath(crosshairs[i]->name));
    }
    extern int lowfps, highfps;
    f->printf("fpsrange %d %d\n", lowfps, highfps);
    if(curfont && curfont->name) f->printf("setfont %s\n", curfont->name);
    f->printf("\n");
    audiomgr.writesoundconfig(f);
    f->printf("\n");
    f->printf("// crosshairs for each weapon\n\nlooplist [\n");
    loopi(NUMGUNS) f->printf("  %-7s %s\n", gunnames[i], crosshairs[i] && crosshairs[i] != notexture ? behindpath(crosshairs[i]->name) : "\"\"");
    f->printf("] [ w cc ] [ loadcrosshair $w $cc ]\n");
    f->printf("\n\n// client variables (unchanged default values %s)\n", omitunchangeddefaults ? "omitted" : "commented out");
    vector<ident *> sids;
    enumerate(*idents, ident, id,
        if(id.persist) switch(id.type)
        {
            case ID_VAR:
            case ID_FVAR:
            case ID_SVAR:
                sids.add(&id);
                break;
        }
    );
    sids.sort(sortident);
    const char *rep = "";
    int repn = 0;
    bool lastdef = false, curdef;
    loopv(sids)
    {
        ident &id = *sids[i];
        curdef = (id.type == ID_VAR && *id.storage.i == id.defaultval) || (id.type == ID_FVAR && *id.storage.f == id.defaultvalf);
        if(curdef && omitunchangeddefaults) continue;
        f->printf("%s", !strncmp(rep, id.name, curdef ? 1 : 3) && ++repn < groupvariables && lastdef == curdef ? " ; " : (repn = 0, "\n"));
        rep = id.name;
        lastdef = curdef;
        if(curdef && repn == 0) f->printf("// ");
        switch(id.type)
        {
            case ID_VAR:  f->printf("%s %d", id.name, *id.storage.i); break;
            case ID_FVAR: f->printf("%s %s", id.name, floatstr(*id.storage.f)); break;
            case ID_SVAR: f->printf("%s %s", id.name, escapestring(*id.storage.s, false)); break;
        }
        if(!groupvariables)
        {
            if(id.type == ID_VAR) f->printf("  // min: %d, max: %d, def: %d", id.minval, id.maxval, id.defaultval);
            if(id.type == ID_FVAR) f->printf("  // min: %s, max: %s, def: %s", floatstr(id.minvalf), floatstr(id.maxvalf), floatstr(id.defaultvalf));
            const char *doc = docgetdesc(id.name);
            if(doc) f->printf(id.type == ID_SVAR ? "  // %s" : ",  %s", doc);
        }
    }
    f->printf("\n\n// weapon settings\n\n");
    loopi(NUMGUNS) if(guns[i].isauto)
    {
        f->printf("burstshots %s %d\n", gunnames[i], burstshotssettings[i]);
    }
    f->printf("\n// key binds\n\n");
    writebinds(f);
    f->printf("\n// aliases\n\n");
    sids.setsize(0);
    enumerate(*idents, ident, id, if(id.type == ID_ALIAS && id.persist) sids.add(&id); );
    sids.sort(sortident);
    loopv(sids)
    {
        ident &id = *sids[i];
        if(strncmp(id.name, "demodesc_", 9))
        {
            const char *action = id.action;
            for(identstack *s = id.stack; s; s = s->next) action = s->action;
            if(action[0]) f->printf("alias %s %s\n", escapestring(id.name, false), escapestring(action, false));
            sids.remove(i--);
        }
    }
    if(sids.length())
    {
        f->printf("\n// demo descriptions\n\n");
        loopv(sids)
        {
            ident &id = *sids[i];
            const char *action = id.action;
            for(identstack *s = id.stack; s; s = s->next) action = s->action;
            if(action[0]) f->printf("alias %s %s\n", escapestring(id.name, false), escapestring(action, false));
        }
    }
    f->printf("\n");
    delete f;
}

COMMAND(writecfg, "");

void deletecfg()
{
    string configs[] = { "config/saved.cfg", "config/init.cfg" };
    loopj(2) // delete files in homedir and basedir if possible
    {
        loopi(sizeof(configs)/sizeof(configs[0]))
        {
            const char *file = findfile(path(configs[i], true), "r");
            if(!file || findfilelocation == FFL_ZIP) continue;
            delfile(file);
        }
    }
}
#endif

void identnames(vector<const char *> &names, bool builtinonly)
{
    enumeratekt(*idents, const char *, name, ident, id,
    {
        if(!builtinonly || id.type != ID_ALIAS) names.add(name);
    });
}

// script execution context management

const char *contextnames[IEXC_NUM + 1] = { "CORE", "CFG", "PROMPT", "MAPCFG", "MDLCFG", "" };
bool contextisolated[IEXC_NUM] = { false };
bool contextsealed = false;
vector<int> contextstack;
int execcontext;

void pushscontext(int newcontext)
{
    ASSERT(newcontext >= 0 && newcontext < IEXC_NUM);
    contextstack.add(execcontext);
    execcontext = newcontext;
    currentcontextisolated = contextsealed && contextisolated[execcontext];
}

int popscontext()
{
    ASSERT(contextstack.length() > 0);
    int old = execcontext;
    execcontext = contextstack.pop();
    currentcontextisolated = contextsealed && contextisolated[execcontext];

    if(execcontext < old && old >= IEXC_MAPCFG) // clean up aliases created in the old (map cfg) context
    {
        int limitcontext = max(execcontext + 1, (int) IEXC_MAPCFG);  // don't clean up below IEXC_MAPCFG
        enumeratekt(*idents, const char *, name, ident, id,
        {
            if(id.type == ID_ALIAS && id.context >= limitcontext)
            {
                while(id.stack && id.stack->context >= limitcontext)
                    popident(id);
                if(id.context >= limitcontext)
                {
                    if(id.action != id.executing) delete[] id.action;
                    idents->remove(name);
                }
            }
        });
    }
    return execcontext;
}

void scriptcontext(char *context, char *idname)
{
    if(contextsealed) return;
    ident *id = idents->access(idname);
    if(!id) return;
    int c = getlistindex(context, contextnames, true, -1);
    if(c >= 0 && c < IEXC_NUM) id->context = c;
}
COMMAND(scriptcontext, "ss");

void isolatecontext(char *context)
{
    int c = getlistindex(context, contextnames, true, -1);
    if(c >= 0 && c < IEXC_NUM && !contextsealed) contextisolated[c] = true;
}
COMMAND(isolatecontext, "s");

COMMANDF(sealcontexts, "",() { contextsealed = true; });

inline bool identaccessdenied(ident *id) // check if ident is allowed in current context
{
    ASSERT(execcontext >= 0 && execcontext < IEXC_NUM && id);
    return contextisolated[execcontext] && execcontext > id->context;
}

// script origin tracking

const char *curcontext = NULL, *curinfo = NULL;

void scripterr()
{
    ASSERT(execcontext >= 0 && execcontext < IEXC_NUM);
    if(curcontext) conoutf("(%s: %s [%s])", curcontext, curinfo, contextnames[execcontext]);
    else conoutf("(from console or builtin [%s])", contextnames[execcontext]);
    clientlogf("exec nesting level: %d", executionstack.length());
    clientlogf("%s", executionstack.length() ? executionstack.last() : ":::nevermind:::");
}

void setcontext(const char *context, const char *info)
{
    curcontext = context;
    curinfo = info;
}

void resetcontext()
{
    curcontext = curinfo = NULL;
}

void dumpexecutionstack(stream *f)
{
    if(f && executionstack.length())
    {
        f->printf("cubescript execution stack (%d levels)\n", executionstack.length());
        if(curcontext) f->printf("  cubescript context:  %s %s\n", curcontext, curinfo ? curinfo : "");
        loopvrev(executionstack) f->printf("%4d:  %s\n", i + 1, executionstack[i]);
        f->fflush();
    }
}

void cslimiterr(const char*msg)
{
    conoutf("\f3[ERROR] cubescript hard limit reached: max %s exceeded (check log for details)", msg);
    scripterr();
}

#ifndef STANDALONE

void listoptions(char *s)
{
    extern const char *menufilesortorders[], *texturestacktypes[], *soundprioritynames[], *soundcategories[];
    const char *optionnames[] = { "entities", "ents", "weapons", "teamnames", "teamnames-abbrv", "punctuations", "crosshairnames", "menufilesortorders", "texturestacktypes", "cubetypes", "soundpriorities", "soundcategories", "" };
    const char **optionlists[] = { optionnames, entnames + 1, entnames + 1, gunnames, teamnames, teamnames_s, punctnames, crosshairnames, menufilesortorders, texturestacktypes, cubetypenames, soundprioritynames, soundcategories };
    const char **listp = optionlists[getlistindex(s, optionnames, true, -1) + 1];
    commandret = conc(listp, -1, true);
}
COMMAND(listoptions, "s");

#endif

void debugargs(char **args, int numargs)
{
    printf("debugargs: ");
    loopi(numargs)
    {
        if(i) printf(", ");
        printf("\"%s\"", args[i]);
    }
    printf("\n");
}

COMMAND(debugargs, "v");

#ifndef STANDALONE
#ifdef _DEBUG
void debugline(char *fname, char *line) // print one line to a logfile
{
    if(!multiplayer())
    {
        stream *f = openfile(behindpath(fname), "a");
        if(f) f->printf("%s\n", line);
        if(f) delete f;
    }
}
COMMAND(debugline, "ss");
#endif
#endif
