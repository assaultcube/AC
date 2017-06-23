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

struct docident
{
    char *name, *desc;
    vector<docargument> arguments;
    vector<char *> remarks;
    vector<char *> references;
    vector<docexample> examples;
    vector<char *> keylines;
    int a, b, c;
    char *label, *related;

    docident() : name(NULL), desc(NULL), label(NULL), related(NULL) {}
    ~docident()
    {
        DELETEA(name);
        DELETEA(desc);
        DELETEA(label);
        DELETEA(related);
    }
};

struct docsection
{
    char *name;
    vector<docident *> sidents;
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
    lastsection->sidents.add(&c);
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
    defformatstring(line)("~%-10s %s", *name ? name : alias, desc);
    lastident->keylines.add(newstring(line));
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
VAR(docidentverbose, 0, 1, 3);

void toggledoc() { docvisible = !docvisible; }
void scrolldoc(int i) { docskip += i; if(docskip < 0) docskip = 0; }

int docs_parsecmd(const char *p, const char *pos, const char *w[MAXWORDS], int wl[MAXWORDS], bool outer, int *unmatched)
{
    vector<const char *> brak;
    for(;;) // loop statements
    {
        int nargs = MAXWORDS, curarg = -1;
        loopi(MAXWORDS)
        {
            w[i] = NULL; wl[i] = 0;
            if(i >  nargs) continue;
            w[i] = (p += strspn(p, " \t")); // skip whitespace
            if(p[0]=='/' && p[1]=='/') p += strcspn(p, "\n\r\0"); // skip comment
            if(*p == '"')
            { // find matching "
                do
                {
                    ++p;
                    p += strcspn(p, "\"\n\r");
                } while(*p == '\"' && p[-1] == '\\');
                p++;
            }
            else if(*p == '[' || *p == '(')
            { // find matching ) or ]
                bool quot = false;
                char right = *p == '[' ? ']' : ')', left = *p++;
                brak.add(p);
                while(brak.length())
                {
                    p += strcspn(p, "([\"])");
                    int c = *p++;
                    if(c == left && !quot) brak.add(p);
                    else if(c == '"') quot = !quot;
                    else if(c == right && !quot) brak.drop();
                    else if(!c)
                    { // unmatched braces, interpret from there (in first pass)
                        *unmatched = right;
                        if(w[0] == brak.last() || brak.last() == p - 1 || outer) break;
                        p = brak.last();
                        brak.setsize(0);
                        i = -1;
                    }
                }
            }
            else p += strcspn(p, "; \t\n\r\0"); // skip regular word
            if(i < 0) continue;
            if(!(wl[i] = (int) (p - w[i]))) w[i] = NULL, nargs = i;
            else if(w[i] <= pos) curarg = i - 1;
        }
        p += strcspn(p, ";\n\r\0"); // skip statement delimiter
        if(!*p++ || pos < p) return curarg; // was last statement, or the one we wanted
    }
}

void renderdoc(int x, int y, int doch)
{
    if(!docvisible) return;
    doch -= 3*FONTH + FONTH/2;

    int cmdpos;
    const char *exp = getcurcommand(&cmdpos); // get command buffer and cursor position
    if(!exp || *exp != '/') return;

    const char *pos = exp + (cmdpos < 0 ? strlen(exp) : cmdpos), *w[MAXWORDS];
    int wl[MAXWORDS], carg, unmatched = 0;
    docident *curident = NULL;
    string buf;
    loopi(2) // two scan passes, first one retries on unmatched braces
    {
        carg = docs_parsecmd(exp + 1, pos, w, wl, i != 0, &unmatched); // break into words
        if(!w[0]) return;
        copystring(buf, w[0], wl[0] + 1);
        curident = docidents.access(buf); // get doc entry
        if(curident) break;
    }
    if(unmatched) draw_textf("\f3missing '%c'", x + 1111, y + doch + 2*FONTH, unmatched);

    ident *csident = idents->access(buf); // check for cs ident

    if(!curident && !csident && docidentverbose < 3) { docskip = 0; return; }

    vector<const char *> doclines;
    vector<char *> heaplines;

    if(curident)
    {
        if(!curident->label)
        {
            formatstring(buf)("~%s", curident->name); // label
            loopvj(curident->arguments) concatformatstring(buf, " %s", curident->arguments[j].token);
            curident->label = newstring(buf);
        }
        doclines.add(curident->label);

        doclines.add(NULL);
        doclines.add(curident->desc);
        doclines.add(NULL);

        if(curident->arguments.length()) // args
        {
            if(carg >= curident->arguments.length() && curident->arguments.last().vararg) carg = curident->arguments.length() - 1; // vararg spans the line

            loopvj(curident->arguments)
            {
                docargument *a = &curident->arguments[j];
                formatstring(doclines.add(heaplines.add(newstringbuf())))("\f%c%-8s%s", j == carg ? '4' : '5', a->token, a->desc);
                if(a->values) concatformatstring(heaplines.last(), " (%s)", a->values);
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

        if(curident->keylines.length()) // default keys
        {
            doclines.add(curident->keylines.length() == 1 ? "Default key:" : "Default keys:");
            loopvj(curident->keylines) doclines.add(curident->keylines[j]); // stored preformatted
            doclines.add(NULL);
        }

        if(docrefvisible && curident->references.length()) // references
        {
            if(!curident->related)
            {
                string refs = "";
                loopvj(curident->references) concatformatstring(refs, ", %s", curident->references[j]);
                formatstring(buf)("Related identifiers:%s", refs + 1);
                curident->related = newstring(buf);
            }
            doclines.add(curident->related);
            doclines.add(NULL);
        }
    }

    if(csident && docidentverbose) // cs ident details
    {
        doclines.add("\f4Ident info:");
        formatstring(buf)("\fs\f1%s\fr: ", csident->name);
        switch(csident->type)
        {
            case ID_VAR:
                if(csident->maxval < csident->minval) concatformatstring(buf, "integer variable, current %d, read-only", *csident->storage.i);
                else concatformatstring(buf, "integer variable, current %d, min %d, max %d, default %d", *csident->storage.i, csident->minval, csident->maxval, csident->defaultval);
                break;
            case ID_FVAR:
                if(csident->maxvalf < csident->minvalf) concatformatstring(buf, "float variable, current %s, read-only", floatstr(*csident->storage.f));
                else concatformatstring(buf, "float variable, current %s, min %s, max %s, default %s", floatstr(*csident->storage.f), floatstr(csident->minvalf), floatstr(csident->maxvalf), floatstr(csident->defaultvalf));
                break;
            case ID_SVAR:
                concatstring(buf, "string variable");
                break;
            case ID_COMMAND:
                concatformatstring(buf, "builtin command%s", strchr("sif", *csident->sig) ? ", arguments:" : "");
                loopi(strlen(csident->sig)) switch(csident->sig[i])
                {
                    case 's': concatstring(buf, i == carg ? " \fs\f4string\fr": " string"); break;
                    case 'i': concatstring(buf, i == carg ? " \fs\f4int\fr"   : " int");    break;
                    case 'f': concatstring(buf, i == carg ? " \fs\f4float\fr" : " float");  break;
                    case 'd': concatstring(buf, ", keybind-only"); break;
                    case 'v': concatstring(buf, ", argument list varies");  break;
                    case 'c': concatstring(buf, ", concatenates arguments with spaces");  break;
                    case 'w': concatstring(buf, ", concatenates arguments without spaces"); break;
                }
                break;
            case ID_ALIAS:
                concatformatstring(buf, "alias, %s", csident->istemp ? "temp" : (csident->isconst ? "const" : (csident->persist ? "persistent" : "non-persistent")));
                break;
        }
        doclines.add(heaplines.add(newstring(buf)));
        doclines.add(NULL);
    }

    if(docidentverbose >= 2)
    {
        doclines.add("\f4Cubescript statement broken into words:");
        int n;
        loopi(MAXWORDS) if(w[i])
        {
            if(i) formatstring(buf)("\fs\f4arg%d: \fr%n%s", i, &n, w[i]);
            else formatstring(buf)("\fs\f4cmd: \fr%n%s", &n, w[i]);
            doclines.add(heaplines.add(newstring(buf, n + wl[i])));
        }
    }

    while(doclines.length() && !doclines.last()) doclines.pop();

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
        }
        else cury += FONTH;
    }
    heaplines.deletearrays();

    if(docskip > offset) docskip = offset;
    if(maxl < doclines.length()) draw_text("\f4more (F3)", x, y+doch); // footer
    if(offset > 0) draw_text("\f4less (F2)", x, y+doch+FONTH);
    draw_text("\f4disable doc reference (F1)", x, y+doch+2*FONTH);
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
        loopvj(sections[i].sidents)
        {
            docident &id = *sections[i].sidents[j];
            msection &s = msections.add();
            s.name = id.name;
            formatstring(s.cmd)("saycommand [/%s ]", id.name);
            s.desc = id.desc;
        }
        msections.sort(msectionsort);
        menureset(menu);
        loopv(msections) { menuitemmanual(menu, msections[i].name, msections[i].cmd, NULL, msections[i].desc); }
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
        menuitemmanual(menu, sections[i].name, a.cmd);
    }
}
