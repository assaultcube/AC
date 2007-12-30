// docs.cpp: ingame command documentation system

#include "pch.h"
#include "cube.h"

void renderdocsection(void *menu, bool init);

extern hashtable<const char *, ident> *idents;

struct docargument
{
    char *token, *desc, *values;
    bool vararg;
};

struct docref
{
    char *name, *ident, *url, *article;
};

struct docexample
{
    char *code, *explanation;
};

struct dockey
{
    char *alias, *name, *desc;
};

struct docident
{
    char *name, *desc;
    vector<docargument> arguments;
    cvector remarks;
    vector<docref> references;
    vector<docexample> examples;
    vector<dockey> keys;
};

struct docsection
{
    char *name;
    vector<docident *> idents;
    void *menu;
};

vector<docsection> sections;
hashtable<const char *, docident> docidents; // manage globally instead of a section tree to ensure uniqueness
docsection *lastsection = NULL;
docident *lastident = NULL;

void adddocsection(char *name)
{
    if(!name) return;
    docsection &s = sections.add();
    s.name = newstring(name);
    s.menu = addmenu(s.name, NULL, true, renderdocsection);
    lastsection = &s;
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

void adddocargument(char *token, char *desc, char *values, char *vararg)
{
    if(!lastident || !token || !desc) return;
    docargument &a = lastident->arguments.add();
    a.token = newstring(token);
    a.desc = newstring(desc);
    a.values = values && strlen(values) ? newstring(values) : NULL;
    a.vararg = vararg && atoi(vararg) == 1 ? true : false;
}

void adddocremark(char *remark)
{
    if(!lastident || !remark) return;
    lastident->remarks.add(newstring(remark));
}

void adddocref(char *name, char *ident, char *url, char *article)
{
    if(!lastident || !name) return;
    docref &r = lastident->references.add();
    r.name = newstring(name);
    r.ident = ident && strlen(ident) ? newstring(ident) : NULL;
    r.url = url && strlen(url) ? newstring(url) : NULL;
    r.article = article && strlen(article) ? newstring(article) : NULL;
}

void adddocexample(char *code, char *explanation)
{
    if(!lastident || !code) return;
    docexample &e = lastident->examples.add();
    e.code = newstring(code);
    e.explanation = explanation && strlen(explanation) ? newstring(explanation) : NULL;
}

void adddockey(char *alias, char *name, char *desc)
{
    if(!lastident || !alias) return;
    dockey &k = lastident->keys.add();
    k.alias = newstring(alias);
    k.name = name && strlen(name) ? newstring(name) : NULL;
    k.desc = desc && strlen(desc) ? newstring(desc) : NULL;
}

COMMANDN(docsection, adddocsection, ARG_1STR);
COMMANDN(docident, adddocident, ARG_2STR);
COMMANDN(docargument, adddocargument, ARG_4STR);
COMMANDN(docremark, adddocremark, ARG_1STR);
COMMANDN(docref, adddocref, ARG_3STR);
COMMANDN(docexample, adddocexample, ARG_2STR);
COMMANDN(dockey, adddockey, ARG_3STR);

int stringsort(const char **a, const char **b) { return strcmp(*a, *b); }

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

void docundone(int allidents)
{
    vector<const char *> inames;
    identnames(inames, !(allidents > 0));
    inames.sort(stringsort);
    loopv(inames)
    {
        docident *id = docidents.access(inames[i]);
        if(id) // search for substrings that indicate undoneness
        {
            cvector srch;
            srch.add(id->name);
            srch.add(id->desc);
            loopvj(id->remarks) srch.add(id->remarks[j]);
            loopvj(id->arguments) 
            { 
                srch.add(id->arguments[j].token);
                srch.add(id->arguments[j].desc);
                srch.add(id->arguments[j].values);
            }
            loopvj(id->references)
            {
                srch.add(id->references[j].ident);
                srch.add(id->references[j].name);
                srch.add(id->references[j].url);
            }
            if(!cvecstr(srch, "TODO") && !cvecstr(srch, "UNDONE")) continue;
        }
        conoutf(inames[i]);
    }
}

void docinvalid()
{
    vector<const char *> inames;
    identnames(inames, true);
    inames.sort(stringsort);
    enumerateht(docidents) if(!strchr(docidents.enumc->data.name, ' ') && !identexists(docidents.enumc->data.name)) 
        conoutf(docidents.enumc->data.name);
}

void docfind(char *search)
{
    enumerateht(docidents)
    {
        docident &i = docidents.enumc->data;
        cvector srch;
        srch.add(i.name);
        srch.add(i.desc);
        loopvk(i.remarks) srch.add(i.remarks[k]);
        
        char *r;
        int rline;
        if((r = cvecstr(srch, search, &rline)))
        {
            const int matchchars = 200;
            string match;
            s_strncpy(match, r-srch[rline] > matchchars/2 ? r-matchchars/2 : srch[rline], matchchars/2);
            conoutf("%-20s%s", i.name, match);
        }
    }
}

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

void docwritebaseref(char *ref, char *schemalocation, char *transformation)
{
    s_sprintfd(outfile)("docs%cautogenerated_base_reference.xml", PATHDIV);
    FILE *f = openfile(outfile, "w");
    if(!f) return;
    char desc[] = "<description>TODO: Description</description>";

    fprintf(f, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");    
    fprintf(f, "<?xml-stylesheet type=\"text/xsl\" href=\"%s\"?>\n", transformation && strlen(transformation) ? transformation : "transformations/cuberef2xhtml.xslt");
    fprintf(f, "<cuberef name=\"%s\" version=\"v0.1\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"%s\" xmlns=\"http://cubers.net/Schemas/CubeRef\">\n", ref && strlen(ref) ? ref : "Unnamed Reference", schemalocation && strlen(schemalocation) ? schemalocation : "http://cubers.net/Schemas/CubeRef schemas/cuberef.xsd");
    fprintf(f, "\t%s\n", desc);
    fprintf(f, "\t<sections>\n");
    fprintf(f, "\t\t<section name=\"Main\">\n");
    fprintf(f, "\t\t\t%s\n", desc);
    fprintf(f, "\t\t\t<identifiers>\n");
    
    string name;
    enumerateht(*idents)
    {
        ident &id = idents->enumc->data;
        if(id.type != ID_COMMAND) continue;
        fprintf(f, "\t\t\t\t<command name=\"%s\">\n", xmlstringenc(name, id.name, _MAXDEFSTR));
        fprintf(f, "\t\t\t\t\t%s\n", desc);
        if(id.narg != ARG_NONE && id.narg != ARG_DOWN)
        {
            fprintf(f, "\t\t\t\t\t<arguments>\n");
            if(id.narg == ARG_VARI) fprintf(f, "\t\t\t\t\t\t<variableArgument token=\"...\" description=\"TODO\"/>\n");
            else
            {
                int base = id.narg >= ARG_1EST ? ARG_1EST : (id.narg >= ARG_1EXP ? ARG_1EXP : (id.narg >= ARG_1STR ? ARG_1STR : ARG_1INT));
                loopj(id.narg-base+1) fprintf(f, "\t\t\t\t\t\t<argument token=\"%c\" description=\"TODO\"/>\n", (char)(*"A")+j);
            }
            fprintf(f, "\t\t\t\t\t</arguments>\n");
        }
        fprintf(f, "\t\t\t\t</command>\n");
    }
    enumerateht(*idents)
    {
        ident &id = idents->enumc->data;
        if(id.type != ID_VAR) continue;
        fprintf(f, "\t\t\t\t<variable name=\"%s\">\n", xmlstringenc(name, id.name, _MAXDEFSTR));
        fprintf(f, "\t\t\t\t\t<description>TODO</description>\n");
        fprintf(f, "\t\t\t\t\t<value %s description=\"TODO\" minValue=\"%i\" maxValue=\"%i\" defaultValue=\"%i\" %s/>\n", id.min>id.max ? "" : "token=\"N\"", id.min, id.max, *id.storage.i, id.min>id.max ? "readOnly=\"true\"" : "");
        fprintf(f, "\t\t\t\t</variable>\n");
    }

    fprintf(f, "\t\t\t</identifiers>\n\t\t</section>\n\t</sections>\n</cuberef>\n");
    fclose(f);
}

COMMAND(docundone, ARG_1INT);
COMMAND(docinvalid, ARG_NONE);
COMMAND(docfind, ARG_1STR);
COMMAND(docwritebaseref, ARG_3STR);
VAR(docvisible, 0, 1, 1);
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
    if(!docvisible) return;

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
                cvector doclines;

                char *label = doclines.add(newstringbuf(ident->name)); // label
                loopvj(ident->arguments)
                {
                    s_strcat(label, " ");
                    s_strcat(label, ident->arguments[j].token);
                }
                doclines.add(NULL);
                
                cvector desc;
                text_block(ident->desc, linemax, desc); // desc
                loopvj(desc) doclines.add(desc[j]);
                doclines.add(NULL);

                if(ident->arguments.length() > 0) // args
                {
                    extern textinputbuffer cmdline;
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
                                s_strncpy(a, args, cmdline.pos-(args-c)+1);
                                args = a;
                                arg = numargs(args);
                            }
                        }
                        else arg = numargs(args);
                        
                        if(arg >= 0) // multipart idents need a fixed argument offset
                        {
                            char *c = cmd;
                            while((c = strchr(c, ' ')) && c++) arg--;
                        }
                        
                        // fixes offset for var args
                        if(arg >= ident->arguments.length() && ident->arguments.last().vararg) arg = ident->arguments.length() - 1;
                    }

                    loopvj(ident->arguments) 
                    {
                        docargument *a = &ident->arguments[j];
                        if(!a) continue;
                        char *argstr = doclines.add(new string);
                        s_sprintf(argstr)("\f%d%-8s%s %s%s%s", j == arg ? 4 : 5, a->token, a->desc,
                            a->values ? "(" : "", a->values ? a->values : "", a->values ? ")" : "");
                    }

                    doclines.add(NULL);
                }

                if(ident->remarks.length()) // remarks
                {
                    loopvj(ident->remarks) 
                    {
                         cvector remarks;
                         text_block(ident->remarks[j], linemax, remarks);
                         loopvk(remarks) doclines.add(remarks[k]);
                    }
                    doclines.add(NULL);
                }

                if(ident->examples.length()) // examples
                {   
                    doclines.add(newstring(ident->examples.length() == 1 ? "Example:" : "Examples:"));
                    loopvj(ident->examples)
                    {
                        cvector lines;
                        text_block(ident->examples[j].code, linemax, lines);
                        text_block(ident->examples[j].explanation, linemax, lines);
                        loopvk(lines) doclines.add(lines[k]);
                    }
                    doclines.add(NULL);
                }

                if(ident->keys.length()) // default keys
                {
                    doclines.add(newstring(ident->keys.length() == 1 ? "Default key:" : "Default keys:"));
                    loopvj(ident->keys)
                    {
                        dockey &k = ident->keys[j];
                        s_sprintfd(line)("%-10s %s", k.name ? k.name : k.alias, k.desc ? k.desc : "");
                        doclines.add(newstring(line));
                    }
                    doclines.add(NULL);
                }

                if(ident->references.length()) // references
                {
                    struct category { string label; string refs; }
                    categories[] = {{"related identifiers", ""} , {"web resources", ""}, {"wiki articles", ""}, {"other", ""}};
                    loopvj(ident->references)
                    {
                        docref &r = ident->references[j];
                        char *ref = r.ident ? categories[0].refs : (r.url ? categories[1].refs : (r.article ? categories[2].refs : categories[3].refs));
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

                int screenlines = (VIRTH*2/3/FONTH)-1;
                if(docskip) docskip = min(docskip, doclines.length() - screenlines); // normalize
                bool more = docskip < doclines.length() - screenlines;
               
                for(int j = docskip; j < min(doclines.length(), docskip+screenlines); j++)
                {
                    if(doclines[j]) draw_textf("%s", x, y+j*FONTH, doclines[j]);
                }
                doclines.deletecontentsa();

                if(more) draw_textf("\f4more (F3)", x, y+screenlines*FONTH); // footer
                if(docskip > 0) draw_textf("\f4less (F2)", x, y+(screenlines+1)*FONTH);
                draw_textf("\f4disable doc reference (F1)", x, y+(screenlines+2)*FONTH);
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
    
    loopv(sections)
    {
        if(sections[i].menu != menu) continue;
        loopvj(sections[i].idents)
        {
            docident &id = *sections[i].idents[j];
            msection &s = msections.add();
            s.name = id.name;
            s_sprintf(s.cmd)("saycommand [/%s ]", id.name);
        }
        msections.sort(msectionsort);
        loopv(msections) { menumanual(menu, i, msections[i].name, msections[i].cmd); }
        return;
    }
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
