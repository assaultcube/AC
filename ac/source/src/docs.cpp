// docs.cpp: ingame command documentation system

#include "cube.h"

struct docargument
{
    char *token, *desc, *values;
};

struct doccommand
{
    char *name, *desc, *remarks;
    vector<docargument> arguments;
};

hashtable<char *, doccommand> docs;
doccommand *lastdoc;

void adddoccommand(char *name, char *desc, char *remarks)
{
    if(!name || !desc) return;
    name = newstring(name);
    doccommand &c = docs[name];
    c.name = name;
    c.desc = newstring(desc);
    c.remarks = remarks && strlen(remarks) ? newstring(remarks) : NULL;
    lastdoc = &c;
}

void adddocargument(char *token, char *desc, char *values)
{
    if(!token || !desc || !lastdoc) return;
    docargument &a = lastdoc->arguments.add();
    a.token = newstring(token);
    a.desc = newstring(desc);
    a.values = values && strlen(values) ? newstring(values) : NULL;
}

COMMANDN(doccommand, adddoccommand, ARG_3STR);
COMMANDN(docargument, adddocargument, ARG_3STR);
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
            doccommand *doc = docs.access(cmd);
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