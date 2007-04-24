// docs.cpp: ingame command documentation system

#include "cube.h"

void renderdocsection(void *menu, bool init);

struct docargument
{
    char *token, *desc, *values;
};

struct docident
{
    char *name, *desc, *remarks;
    vector<docargument> arguments;
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

void adddocident(char *name, char *desc, char *remarks)
{
    if(!name || !desc || !lastsection) return;
    name = newstring(name);
    docident &c = docidents[name];
    lastsection->idents.add(&c);
    c.name = name;
    c.desc = newstring(desc);
    c.remarks = remarks && strlen(remarks) ? newstring(remarks) : NULL;
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

int stringsort(const char **a, const char **b) { return strcmp(*a, *b); }

void doctodo()
{
    vector<char *> inames = identnames();
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
        char *searchstrings[] = { i.name, i.desc, i.remarks };
        loopi(3)
        {
            char *r = strstr(searchstrings[i], search);
            if(!r) continue;
            const int matchchars = 200;
            string match;
            s_strncpy(match, r-searchstrings[i] > matchchars/2 ? r-matchchars/2 : searchstrings[i], matchchars/2);
            conoutf("%s\t%s", docidents.enumc->data, match);
            break;
        }
    }
}

COMMANDN(docsection, adddocsection, ARG_1STR);
COMMANDN(docident, adddocident, ARG_3STR);
COMMANDN(docargument, adddocargument, ARG_3STR);
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
            docident *doc = docidents.access(cmd);
            if(doc)
            {
                const int doclinewidth = (VIRTW*2)/FONTH;
                int numlines = 0;

                // draw label
                string label;
                s_strcpy(label, doc->name);
                loopv(doc->arguments) 
                { 
                    s_strcat(label, " ");
                    s_strcat(label, doc->arguments[i].token); 
                }
                draw_textf("%s", x, y, label);
                numlines+=2;

                // draw description
                numlines += draw_textblock(doc->desc, x, y+numlines*FONTH, doclinewidth)+1;

                // draw arguments
                if(doc->arguments.length() > 0)
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

                    loopv(doc->arguments) 
                    {
                        docargument *a = &doc->arguments[i];
                        if(!a) continue;
                        draw_textf("\f%d%s%s%s %s%s%s", x, y+numlines++*FONTH, i == arg ? 4 : 5, a->token, 
                            strlen(a->token) >= 6 ? "\t" : "\t\t",
                            a->desc,
                            a->values ? "(" : "", a->values ? a->values : "", a->values ? ")" : "");
                    }
                    numlines++;
                }

                // draw remarks
                if(doc->remarks) draw_textblock(doc->remarks, x, y+numlines++*FONTH, doclinewidth);
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
    struct msection { char *name; string cmd; };
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

void renderdocmenu(void *menu, bool init)
{
    struct action { string cmd; };
    static vector<action> actions;
    actions.setsize(0);

    loopv(sections)
    {
        action &a = actions.add();
        s_sprintf(a.cmd)("showmenu [%s]", sections[i].name);
        menumanual(menu, i, sections[i].name, a.cmd);
    }
}

