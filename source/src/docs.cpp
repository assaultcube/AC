// docs.cpp: ingame command documentation system

#include "cube.h"

void renderdocsection(void *menu, bool init);

extern hashtable<const char *, ident> *idents;

struct docargument
{
    char *token, *desc, *values;
    bool vararg;

    docargument() : token(NULL), desc(NULL), values(NULL) {};
    ~docargument()
    {
        DELETEA(token);
        DELETEA(desc);
        DELETEA(values);
    }
};

struct docexample
{
    char *code, *explanation;

    docexample() : code(NULL), explanation(NULL) {}
    ~docexample()
    {
        DELETEA(code);
        DELETEA(explanation);
    }
};

struct dockey
{
    char *alias, *name, *desc;

    dockey() : alias(NULL), name(NULL), desc(NULL) {}
    ~dockey()
    {
        DELETEA(alias);
        DELETEA(name);
        DELETEA(desc);
    }
};

struct docident
{
    char *name, *desc;
    vector<docargument> arguments;
    vector<char *> remarks;
    vector<char *> references;
    vector<docexample> examples;
    vector<dockey> keys;

    docident() : name(NULL), desc(NULL) {}
    ~docident()
    {
        DELETEA(name);
        DELETEA(desc);
    }
};

struct docsection
{
    char *name;
    vector<docident *> idents;
    void *menu;

    docsection() : name(NULL) {};
    ~docsection()
    {
        DELETEA(name);
    }
};

vector<docsection> sections;
hashtable<const char *, docident> docidents; // manage globally instead of a section tree to ensure uniqueness
docsection *lastsection = NULL;
docident *lastident = NULL;

void adddocsection(char *name)
{
    if(!name || !*name) { lastsection = NULL; return; }
    docsection &s = sections.add();
    s.name = newstring(name);
    s.menu = addmenu(s.name, NULL, true, renderdocsection);
    lastsection = &s;
}
COMMANDN(docsection, adddocsection, "s");

void adddocident(char *name, char *desc)
{
    if(!name || !desc || !lastsection || !*name) { lastident = NULL; return; }
    name = newstring(name);
    docident &c = docidents[name];
    lastsection->idents.add(&c);
    c.name = name;
    c.desc = newstring(desc);
    lastident = &c;
    DEBUGCODE(if(strlen(desc) > 111) clientlogf("docident: very long description for ident %s (%d)", name, (int)strlen(desc)));
}
COMMANDN(docident, adddocident, "ss");

void adddocargument(char *token, char *desc, char *values, char *vararg)
{
    if(!lastident || !token || !desc) return;
    if(*token) loopv(lastident->arguments) if(!strcmp(token, lastident->arguments[i].token)) clientlogf("docargument: double token %s in reference %s", token, lastident->name);
    docargument &a = lastident->arguments.add();
    a.token = newstring(token);
    a.desc = newstring(desc);
    a.values = values && strlen(values) ? newstring(values) : NULL;
    a.vararg = vararg && atoi(vararg) == 1 ? true : false;
}
COMMANDN(docargument, adddocargument, "ssss");

void adddocremark(char *remark)
{
    if(!lastident || !remark || !*remark) return;
    lastident->remarks.add(newstring(remark));
}
COMMANDN(docremark, adddocremark, "s");

void adddocref(char *refident)
{
    if(!lastident || !refident || !*refident) return;
#ifdef _DEBUG
    if(!strcasecmp(refident, lastident->name)) clientlogf("docref: circular reference for %s", refident);
    loopv(lastident->references) if(!strcasecmp(lastident->references[i], refident)) clientlogf("docref: double reference %s in %s", refident, lastident->name);
#endif
    lastident->references.add(newstring(refident));
}
COMMANDN(docref, adddocref, "s");

void adddocexample(char *code, char *explanation)
{
    if(!lastident || !code) return;
    docexample &e = lastident->examples.add();
    e.code = newstring(code);
    e.explanation = explanation && strlen(explanation) ? newstring(explanation) : NULL;
}
COMMANDN(docexample, adddocexample, "ss");

void adddockey(char *alias, char *name, char *desc)
{
    if(!lastident || !alias) return;
    dockey &k = lastident->keys.add();
    k.alias = newstring(alias);
    k.name = name && strlen(name) ? newstring(name) : NULL;
    k.desc = desc && strlen(desc) ? newstring(desc) : NULL;
}
COMMANDN(dockey, adddockey, "sss");


const char *docgetdesc(const char *name)
{
    docident *id = docidents.access(name);
    return id ? id->desc : NULL;
}

char *cvecstr(vector<char *> &cvec, const char *substr, int *rline = NULL)
{
    char *r = NULL;
    loopv(cvec) if(cvec[i]) if((r = strstr(cvec[i], substr)) != NULL)
    {
        if(rline) *rline = i;
        break;
    }
    return r;
}

void listundoneidents(vector<const char *> &inames, int allidents)
{
    enumeratekt(*idents, const char *, name, ident, id,
    {
        if((allidents > 0) || id.type != ID_ALIAS)
        {
            docident *id = docidents.access(name);
            if(id) // search for substrings that indicate undoneness
            {
                vector<char *> srch;
                srch.add(id->name);
                srch.add(id->desc);
                loopvj(id->remarks) srch.add(id->remarks[j]);
                loopvj(id->arguments)
                {
                    srch.add(id->arguments[j].token);
                    srch.add(id->arguments[j].desc);
                    srch.add(id->arguments[j].values);
                }
                loopvj(id->references) srch.add(id->references[j]);
                if(cvecstr(srch, "TODO") || cvecstr(srch, "UNDONE")) id = NULL;
            }
            if(!id) inames.add(name);
        }
    });
    inames.sort(stringsort);
}

void docundone(int *allidents)
{
    vector<const char *> inames;
    listundoneidents(inames, *allidents);
    inames.sort(stringsort);
    loopv(inames) conoutf("%s", inames[i]);
}
COMMAND(docundone, "i");

void docinvalid()
{
    vector<const char *> inames, irefs;
    enumerate(docidents, docident, d, if(!strchr(d.name, ' ') && !identexists(d.name)) inames.add(d.name););
    inames.sort(stringsort);
    if(inames.length()) conoutf("no such ident:");
    loopv(inames) conoutf(" %s", inames[i]);
    enumerate(docidents, docident, d,
    {
        loopv(d.references) if(!identexists(d.references[i]))
        {
            bool add = true;
            loopvj(inames) if(!strcasecmp(inames[j], d.references[i])) add = false; // don't list references to nonexisting but documented entries
            if(add) irefs.add(d.references[i]);
        }
    });
    irefs.sort(stringsort);
    if(irefs.length()) conoutf("no such reference:");
    loopv(irefs) conoutf(" %s", irefs[i]);
}
COMMAND(docinvalid, "");

void docfind(char *search, int *silent)
{
    vector<char> res;
    vector<const char *> inames;
    enumerate(docidents, docident, i, inames.add(i.name));
    inames.sort(stringsort);
    loopvj(inames)
    {
        docident &i = *docidents.access(inames[j]);
        vector<char *> srch;
        srch.add(i.name); // 0
        srch.add(i.desc); // 1
        loopvk(i.remarks) srch.add(i.remarks[k]);

        char *r;
        int rline;
        if((r = cvecstr(srch, search, &rline)))
        {
            cvecprintf(res, "%s %d\n", escapestring(i.name, false), rline);
            if(!rline) r = srch[(rline = 1)];
            if(!*silent)
            {
                const int showchars = 100;
                string match;
                int pos = r - srch[rline];
                copystring(match, srch[rline] + (pos < showchars - 10 ? 0 : pos - 10), showchars);
                conoutf("%-20s%s", i.name, match);
            }
        }
    }
    resultcharvector(res, -1);
}
COMMAND(docfind, "si");

void getdoc(char *name, int *line)
{
    const char *res = "";
    docident *d = docidents.access(name);
    if(d)
    {
        switch(*line)
        {
            case 0: res = d->name; break;
            case 1: res = d->desc; break;
            default:
                if(d->remarks.inrange(*line - 2)) res = d->remarks[*line - 2];
                break;
        }
    }
    else conoutf("\f3getdoc: \"%s\" not found", name);
    result(res);
}
COMMAND(getdoc, "si");

char *xmlstringenc(char *d, const char *s, size_t len)
{
    if(!d || !s) return NULL;
    struct spchar { char c; char repl[8]; } const spchars[] = { {'&', "&amp;"}, {'<', "&lt;"}, {'>', "gt;"}, {'"', "&quot;"}, {'\'', "&apos;"}};

    char *dc = d;
    const char *sc = s;

    while(*sc && (size_t)(dc - d) < len - 1)
    {
        bool specialc = false;
        loopi(sizeof(spchars)/sizeof(spchar)) if(spchars[i].c == *sc)
        {
            specialc = true;
            size_t rlen = strlen(spchars[i].repl);
            if(dc - d + rlen <= len - 1)
            {
                memcpy(dc, spchars[i].repl, rlen);
                dc += rlen;
                break;
            }
        }
        if(!specialc) memcpy(dc++, sc, 1);
        *dc = 0;
        sc++;
    }
    return d;
}

void docwriteref(int allidents, const char *ref, const char *schemalocation, const char *transformation)
{
    stream *f = openfile(path(allidents < 0 ? "docs/autogenerated_base_reference.xml" : "docs/autogenerated_todo_reference.xml", true), "w");
    if(!f) return;
    char desc[] = "<description>TODO: Description</description>";

    vector<const char *> inames;
    if(allidents < 0) identnames(inames, true);   // -1: all builtin
    else listundoneidents(inames, allidents);      // 0: todo builtin, 1: todo builtin + alias
    inames.sort(stringsort);

    if(allidents < 0)
    {
        f->printf("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
        f->printf("<?xml-stylesheet type=\"text/xsl\" href=\"%s\"?>\n", transformation && strlen(transformation) ? transformation : "transformations/cuberef2xhtml.xslt");
        f->printf("<cuberef xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" name=\"%s\" version=\"v0.1\" xsi:schemaLocation=\"%s\" xmlns=\"http://cubers.net/Schemas/CubeRef\">\n", ref && strlen(ref) ? ref : "Unnamed Reference", schemalocation && strlen(schemalocation) ? schemalocation : "http://cubers.net/Schemas/CubeRef schemas/cuberef.xsd");
        f->printf("\t%s\n", desc);
        f->printf("\t<sections>\n");
        f->printf("\t\t<section name=\"Main\">\n");
        f->printf("\t\t\t%s\n", desc);
        f->printf("\t\t\t<identifiers>\n");
    }

    string name;
    loopv(inames)
    {
        ident *id = idents->access(inames[i]);
        if(!id || id->type != ID_COMMAND) continue;
        f->printf("\t\t\t\t<command name=\"%s\">\n", xmlstringenc(name, id->name, MAXSTRLEN));
        f->printf("\t\t\t\t\t%s\n", desc);
        if(id->sig && id->sig[0])
        {
            f->printf("\t\t\t\t\t<arguments>\n");
            loopi(strlen(id->sig))
            {
                f->printf("\t\t\t\t\t\t<argument token=\"%c\" description=\"TODO\"/>\n", id->sig[i]);
            }
            f->printf("\t\t\t\t\t</arguments>\n");
        }
        f->printf("\t\t\t\t</command>\n");
    }
    loopv(inames)
    {
        ident *id = idents->access(inames[i]);
        if(!id || (id->type != ID_VAR && id->type != ID_FVAR)) continue;
        f->printf("\t\t\t\t<variable name=\"%s\">\n", xmlstringenc(name, id->name, MAXSTRLEN));
        f->printf("\t\t\t\t\t<description>TODO</description>\n");
        switch(id->type)
        {
            case ID_VAR:
                f->printf("\t\t\t\t\t<value %s description=\"TODO\" minValue=\"%d\" maxValue=\"%d\" defaultValue=\"%d\" %s/>\n", id->minval>id->maxval ? "" : "token=\"N\"", id->minval, id->maxval, *id->storage.i, id->minval>id->maxval ? "readOnly=\"true\"" : "");
                break;
            case ID_FVAR:
                f->printf("\t\t\t\t\t<value %s description=\"TODO\" minValue=\"%s\" maxValue=\"%s\" defaultValue=\"%s\" %s/>\n", id->minvalf>id->maxvalf ? "" : "token=\"N\"",
                    floatstr(id->minvalf), floatstr(id->maxvalf), floatstr(*id->storage.f),
                    id->minvalf>id->maxvalf ? "readOnly=\"true\"" : "");
                break;
        }
        f->printf("\t\t\t\t</variable>\n");
    }
    loopv(inames)
    {
        ident *id = idents->access(inames[i]);
        if(!id || id->type != ID_ALIAS) continue;
        f->printf("\t\t\t\t<command name=\"%s\">\n", xmlstringenc(name, id->name, MAXSTRLEN));
        f->printf("\t\t\t\t\t%s\n", desc);
        f->printf("\t\t\t\t</command>\n");
    }

    if(allidents < 0) f->printf("\t\t\t</identifiers>\n\t\t</section>\n\t</sections>\n</cuberef>\n");
    delete f;
}

void docwritebaseref(char *ref, char *schemalocation, char *transformation)
{
    docwriteref(-1, ref, schemalocation, transformation);
}
COMMAND(docwritebaseref, "sss");

void docwritetodoref(int *allidents)
{
    docwriteref(*allidents ? 1 : 0, "", "", "");
}
COMMAND(docwritetodoref, "i");

VAR(docvisible, 0, 1, 1);
VAR(docrefvisible, 0, 1, 1);
VAR(docskip, 0, 0, 1000);

void toggledoc() { docvisible = !docvisible; }
void scrolldoc(int i) { docskip += i; if(docskip < 0) docskip = 0; }

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
                case '(': if(*(t-1) != ')') continue; break;
                case '"': if(*(t-1) != '"') continue; break;
                default: break;
            }
            argstart = NULL;
        }
    }
    return argidx;
}

void renderdoc(int x, int y, int doch)
{
    if(!docvisible) return;

    char *exp = getcurcommand();
    if(!exp || !*exp) return;

    int o = 0; //offset
    int f = 0; //last found

    while (*exp)
    {exp++; o++; if (*exp == ';' || (*exp == ' ' && f == o-1)) f = o;} exp--;

    if (f > 0)
    {
        for (int i = o - f - 1; i > 0; i--) exp--;
        if (o > f + 1) exp++;
    }

    else {for (int i = o; i > 1; i--) exp--;}

    char *openblock = strrchr(exp+1, '('); //find last open parenthesis
    char *closeblock = strrchr(exp+1, ')'); //find last closed parenthesis
    char *temp = NULL;

    if (openblock)
    {
        if (!closeblock || closeblock < openblock) //open block
        temp = openblock + 1;
    }

    if(!exp || (*exp != '/' && f == 0) || strlen(exp) < 2) return;

    char *c = exp+1; if (f > 0) c = exp;
    char *d = NULL; if (temp) d = temp;

    size_t clen = strlen(c);
    size_t dlen = 0; if (d) dlen = strlen(d);

    bool nc = false; //tests if text after open parenthesis is not a command

    docident *curident = NULL;

    for(size_t i = 0; i < clen; i++) // search first matching cmd doc by stripping arguments of exp from right to left
    {
        char *end = c+clen-i;
        if(!*end || *end == ' ')
        {
            string cmd;
            string dmd;

            copystring(cmd, c, clen-i+1);

            if (d && !nc && dlen > 1)
            {
                for(size_t j = 0; j < dlen; j++) //test text after parenthesis
                {
                    char *dnd = d+dlen-j;
                    if(!*dnd || *dnd == ' ')
                    {
                        copystring(dmd, d, dlen-j+1);
                        curident = docidents.access(dmd);
                    }
                    if (j == dlen-1 && !curident)
                    nc = true;
                }
            }
            else
            {
                nc = true;
                curident = docidents.access(cmd);
            }

            if(curident)
            {
                vector<const char *> doclines;

                char *label = newstringbuf(); // label
                doclines.add(label);
                formatstring(label)("~%s", curident->name);
                loopvj(curident->arguments)
                {
                    concatstring(label, " ");
                    concatstring(label, curident->arguments[j].token);
                }
                doclines.add(NULL);

                doclines.add(curident->desc);
                doclines.add(NULL);

                if(curident->arguments.length() > 0) // args
                {
                    extern textinputbuffer cmdline;

                    if (d && dlen > 1) c = d;
                    char *args = strchr(c, ' ');

                    int arg = -1;

                    if(args)
                    {
                        args++;
                        if(cmdline.pos >= 0)
                        {
                            if(cmdline.pos >= args-c)
                            {
                                string a;
                                copystring(a, args, cmdline.pos-(args-c)+1);
                                args = a;
                                arg = numargs(args);
                            }
                        }
                        else arg = numargs(args);

                        if(arg >= 0) // multipart idents need a fixed argument offset
                        {
                            char *c = cmd;
                            if (!nc) c = dmd;
                            while((c = strchr(c, ' ')) && c++) arg--;
                        }

                        // fixes offset for var args
                        if(arg >= curident->arguments.length() && curident->arguments.last().vararg) arg = curident->arguments.length() - 1;
                    }

                    loopvj(curident->arguments)
                    {
                        docargument *a = &curident->arguments[j];
                        if(!a) continue;
                        formatstring(doclines.add(newstringbuf()))("\f%d%-8s%s %s%s%s", j == arg ? 4 : 5, a->token, a->desc,
                            a->values ? "(" : "", a->values ? a->values : "", a->values ? ")" : "");
                    }

                    doclines.add(NULL);
                }

                if(curident->remarks.length()) // remarks
                {
                    loopvj(curident->remarks) doclines.add(curident->remarks[j]);
                    doclines.add(NULL);
                }

                if(curident->examples.length()) // examples
                {
                    doclines.add(curident->examples.length() == 1 ? "Example:" : "Examples:");
                    loopvj(curident->examples)
                    {
                        doclines.add(curident->examples[j].code);
                        doclines.add(curident->examples[j].explanation);
                    }
                    doclines.add(NULL);
                }

                if(curident->keys.length()) // default keys
                {
                    doclines.add(curident->keys.length() == 1 ? "Default key:" : "Default keys:");
                    loopvj(curident->keys)
                    {
                        dockey &k = curident->keys[j];
                        defformatstring(line)("~%-10s %s", k.name ? k.name : k.alias, k.desc ? k.desc : "");
                        doclines.add(newstring(line));
                    }
                    doclines.add(NULL);
                }

                if(docrefvisible && curident->references.length()) // references
                {
                    string refs = "";
                    loopvj(curident->references) concatformatstring(refs, ", %s", curident->references[j]);
                    formatstring(doclines.add(newstringbuf()))("Related identifiers:%s", refs + 1);
                }

                while(doclines.length() && !doclines.last()) doclines.pop();

                doch -= 3*FONTH + FONTH/2;

                int offset = min(docskip, doclines.length()-1), maxl = offset, cury = 0;
                for(int j = offset; j < doclines.length(); j++)
                {
                    const char *str = doclines[j];
                    int width = 0, height = FONTH;
                    if(str) text_bounds(*str=='~' ? str+1 : str, width, height, *str=='~' ? -1 : VIRTW*4/3);
                    if(cury + height > doch) break;
                    cury += height;
                    maxl = j+1;
                }

                if(offset > 0 && maxl >= doclines.length())
                {
                    for(int j = offset-1; j >= 0; j--)
                    {
                        const char *str = doclines[j];
                        int width = 0, height = FONTH;
                        if(str) text_bounds(*str=='~' ? str+1 : str, width, height, *str=='~' ? -1 : VIRTW*4/3);
                        if(cury + height > doch) break;
                        cury += height;
                        offset = j;
                    }
                }

                cury = y;
                for(int j = offset; j < maxl; j++)
                {
                    const char *str = doclines[j];
                    if(str)
                    {
                        const char *text = *str=='~' ? str+1 : str;
                        draw_text(text, x, cury, 0xFF, 0xFF, 0xFF, 0xFF, -1, str==text ? VIRTW*4/3 : -1);
                        int width, height;
                        text_bounds(text, width, height, str==text ? VIRTW*4/3 : -1);
                        cury += height;
                        if(str!=text) delete[] str;
                    }
                    else cury += FONTH;
                }

                if(docskip > offset) docskip = offset;
                if(maxl < doclines.length()) draw_text("\f4more (F3)", x, y+doch); // footer
                if(offset > 0) draw_text("\f4less (F2)", x, y+doch+FONTH);
                draw_text("\f4disable doc reference (F1)", x, y+doch+2*FONTH);
                return;
            }
            else docskip = 0;
        }
    }
}

void *docmenu = NULL;

struct msection { char *name, *desc; string cmd; };

int msectionsort(const msection *a, const msection *b)
{
    return strcmp(a->name, b->name);
}

void renderdocsection(void *menu, bool init)
{
    static vector<msection> msections;
    msections.shrink(0);
    loopv(sections)
    {
        if(sections[i].menu != menu) continue;
        loopvj(sections[i].idents)
        {
            docident &id = *sections[i].idents[j];
            msection &s = msections.add();
            s.name = id.name;
            formatstring(s.cmd)("saycommand [/%s ]", id.name);
            s.desc = id.desc;
        }
        msections.sort(msectionsort);
        menureset(menu);
        loopv(msections) { menumanual(menu, msections[i].name, msections[i].cmd, NULL, msections[i].desc); }
        return;
    }
}

struct maction { string cmd; };

void renderdocmenu(void *menu, bool init)
{
    static vector<maction> actions;
    actions.shrink(0);
    menureset(menu);
    loopv(sections)
    {
        maction &a = actions.add();
        formatstring(a.cmd)("showmenu [%s]", sections[i].name);
        menumanual(menu, sections[i].name, a.cmd);
    }
}
