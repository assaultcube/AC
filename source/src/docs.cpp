// docs.cpp: ingame command documentation system

#include "cube.h"

void renderdocsection(void *menu, bool init);

struct docargument
{
    char *token, *desc, *values;
};

struct docref
{
    char *name, *ident, *url;
};

struct docident
{
    char *name, *desc;
    vector<docargument> arguments;
    vector<char *> remarks;
    vector<docref> references;
};

struct docsection
{
    char *name;
    vector<docident *> idents;
};

vector<docsection> sections;
hashtable<char *, docident> docidents; // manage globally instead of a section tree because cmds must be unique
docsection *lastsection = NULL;
docident *lastident = NULL;

void adddocsection(char *name)
{
    if(!name) return;
    docsection &s = sections.add();
    s.name = newstring(name);
    lastsection = &s;
    addmenu(s.name, NULL, true, renderdocsection);
}

void adddocident(char *name, char *desc)
{
    if(!name || !desc || !lastsection) return;
    name = newstring(name);
    docident &c = docidents[name];
    lastsection->idents.add(&c);
    c.name = name;
    c.desc = newstring(desc);
    lastident = &c;
}

void adddocargument(char *token, char *desc, char *values)
{
    if(!token || !desc || !lastident) return;
    docargument &a = lastident->arguments.add();
    a.token = newstring(token);
    a.desc = newstring(desc);
    a.values = values && strlen(values) ? newstring(values) : NULL;
}

void adddocremark(char *remark)
{
    if(!lastident || !remark) return;
    lastident->remarks.add(newstring(remark));
}

void adddocref(char *name, char *ident, char *url)
{
    if(!lastident || !name) return;
    docref &r = lastident->references.add();
    r.name = newstring(name);
    r.ident = ident && strlen(ident) ? newstring(ident) : NULL;
    r.url = url && strlen(url) ? newstring(url) : NULL;
}

int stringsort(const char **a, const char **b) { return strcmp(*a, *b); }

void doctodo()
{
    vector<char *> inames;
    identnames(inames);
    inames.sort(stringsort);
    conoutf("documentation TODO");
    conoutf("undocumented identifiers:");
    loopv(inames) if(!docidents.access(inames[i])) conoutf(inames[i]);
    conoutf("documented but unimplemented identifiers:");
    enumerateht(docidents) if(!strchr(docidents.enumc->data.name, ' ') && !identexists(docidents.enumc->data.name)) 
        conoutf(docidents.enumc->data.name);
    conoutf("done");
}

void docfind(char *search)
{
    enumerateht(docidents)
    {
        docident &i = docidents.enumc->data;
        vector<char *> searchstrings;
        searchstrings.add(i.name);
        searchstrings.add(i.desc);
        loopvk(i.remarks) searchstrings.add(i.remarks[k]);
        loopvk(searchstrings)
        {
            char *r = strstr(searchstrings[k], search);
            if(!r) continue;
            const int matchchars = 200;
            string match;
            s_strncpy(match, r-searchstrings[k] > matchchars/2 ? r-matchchars/2 : searchstrings[k], matchchars/2);
            conoutf("%s\t%s", i.name, match);
        }
    }
}

COMMANDN(docsection, adddocsection, ARG_1STR);
COMMANDN(docident, adddocident, ARG_3STR);
COMMANDN(docargument, adddocargument, ARG_3STR);
COMMANDN(docremark, adddocremark, ARG_1STR);
COMMANDN(docref, adddocref, ARG_3STR);
COMMAND(doctodo, ARG_NONE);
COMMAND(docfind, ARG_1STR);
VAR(loaddoc, 0, 1, 1);

int numargs(char *args)
{
    if(!args || !strlen(args)) return -1;

    int argidx = -1;
    char *argstart = NULL;

    for(char *t = args; *t; t++)
    {
        if(!argstart && *t != ' ') { argstart = t; argidx++; }
        else if(argstart && *t == ' ') if(t-1 >= args) 
        {
            switch(*argstart)
            {
                case '[': if(*(t-1) != ']') continue; break;
                case '"': if(*(t-1) != '"') continue; break;
                default: break;
            }
            argstart = NULL;
        }
    }
    return argidx;
}

void renderdoc(int x, int y)
{
    char *exp = getcurcommand();
    if(!exp || *exp != '/' || strlen(exp) < 2) return;

    char *c = exp+1;
    size_t clen = strlen(c);
    for(size_t i = 0; i < clen; i++) // search first matching cmd doc by stripping arguments of exp from right to left
    {
        char *end = c+clen-i;
        if(!*end || *end == ' ')
        {
            string cmd;
            s_strncpy(cmd, c, clen-i+1);
            docident *ident = docidents.access(cmd);
            if(ident)
            {
                const int linemax = VIRTW*4/3;
                vector<char *> doclines;

                char *label = doclines.add(newstringbuf(ident->name)); // label
                loopvj(ident->arguments)
                {
                    s_strcat(label, " ");
                    s_strcat(label, ident->arguments[j].token);
                }
                doclines.add(NULL);
                
                vector<char *> desc;
                text_block(ident->desc, linemax, desc); // desc
                loopvj(desc) doclines.add(desc[j]);
                doclines.add(NULL);

                if(ident->arguments.length() > 0) // args
                {
                    extern int commandpos;
                    char *args = strchr(c, ' ');
                    int arg = -1;

                    if(args)
                    {
                        args++;
                        if(commandpos >= 0)
                        {
                            if(commandpos >= args-c)
                            {
                                string a;
                                s_strncpy(a, args, commandpos-(args-c)+1);
                                args = a;
                                arg = numargs(args);
                            }
                        }
                        else arg = numargs(args);
                        
                        if(arg >= 0)
                        {
                            char *c = cmd;
                            while((c = strchr(c, ' ')) && c++) arg--; // multipart idents need a fixed argument offset
                        }
                    }

                    loopvj(ident->arguments) 
                    {
                        docargument *a = &ident->arguments[j];
                        if(!a) continue;
                        char *argstr = doclines.add(new string);
                        s_sprintf(argstr)("\f%d%-8s%s %s%s%s", j == arg ? 4 : 5, a->token, a->desc,
                            a->values ? "(" : "", a->values ? a->values : "", a->values ? ")" : "");
                    }
                }
                doclines.add(NULL);

                if(ident->remarks.length()) loopvj(ident->remarks) // remarks
                {
                     vector<char *> remarks;
                     text_block(ident->remarks[j], linemax, remarks);
                     loopvk(remarks) doclines.add(remarks[k]);
                }
                doclines.add(NULL);

                if(ident->references.length()) // refs
                {
                    struct category { string label; string refs; }
                    categories[] = {{"related identifiers", ""} , {"web resources", ""}, {"other", ""}};
                    loopvj(ident->references)
                    {
                        docref &r = ident->references[j];
                        char *ref = r.ident ? categories[0].refs : (r.url ? categories[1].refs : categories[2].refs);
                        s_strcat(ref, r.name);
                        if(j < ident->references.length()-1) s_strcat(ref, ", ");
                    }
                    loopj(sizeof(categories)/sizeof(category))
                    {
                        if(!strlen(categories[j].refs)) continue;
                        char *line = doclines.add(newstringbuf(categories[j].label));
                        s_strcat(line, ": ");
                        s_strcat(line, categories[j].refs);
                    }
                }
                doclines.add(NULL);

                loopvj(doclines) if(doclines[j]) draw_textf("%s", x, y+j*FONTH, doclines[j]);
                doclines.deletecontentsa();
                return;
            }
        }
    }
}

void *docmenu = NULL;

struct msection { char *name; string cmd; };

int msectionsort(const msection *a, const msection *b)
{
    return strcmp(a->name, b->name);
}

void renderdocsection(void *menu, bool init)
{
    static vector<msection> msections;
    msections.setsize(0);
    
    enumerateht(docidents)
    {
        msection &s = msections.add();
        s.name = docidents.enumc->key;
        s_sprintf(s.cmd)("saycommand [/%s ]", docidents.enumc->key);
    }
    msections.sort(msectionsort);
    loopv(msections) { menumanual(menu, i, msections[i].name, msections[i].cmd); }
}

struct maction { string cmd; };

void renderdocmenu(void *menu, bool init)
{
    static vector<maction> actions;
    actions.setsize(0);

    loopv(sections)
    {
        maction &a = actions.add();
        s_sprintf(a.cmd)("showmenu [%s]", sections[i].name);
        menumanual(menu, i, sections[i].name, a.cmd);
    }
}

