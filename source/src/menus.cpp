// menus.cpp: ingame menu system (also used for scores and serverlist)

#include "cube.h"

hashtable<const char *, gmenu> menus;
gmenu *curmenu = NULL, *lastmenu = NULL;
color *menuselbgcolor, *menuseldescbgcolor = NULL;

vector<gmenu *> menustack;

COMMANDF(curmenu, "", () {result(curmenu ? curmenu->name : "");} );
VARP(browsefiledesc, 0, 1, 1);

char *getfiledesc(const char *dir, const char *name, const char *ext)
{
    if(!browsefiledesc || !dir || !name || !ext) return NULL;
    defformatstring(fn)("%s/%s.%s", dir, name, ext);
    path(fn);
    string text, demodescalias;
    if(!strcmp(ext, "dmo"))
    {
        stream *f = opengzfile(fn, "rb");
        if(!f) return NULL;
        demoheader hdr;
        if(f->read(&hdr, sizeof(demoheader))!=sizeof(demoheader) || memcmp(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic))) { delete f; return NULL; }
        delete f;
        lilswap(&hdr.version, 1);
        lilswap(&hdr.protocol, 1);
        const char *tag = "(incompatible file) ";
        if(hdr.version == DEMO_VERSION)
        {
            if(hdr.protocol == PROTOCOL_VERSION) tag = "";
            else if(hdr.protocol == -PROTOCOL_VERSION) tag = "(recorded on modded server) ";
        }
        formatstring(text)("%s%s", tag, hdr.desc);
        text[DHDR_DESCCHARS - 1] = '\0';
        formatstring(demodescalias)("demodesc_%s", name);
        const char *customdesc = getalias(demodescalias);
        if(customdesc)
        {
            int textlen = strlen(text);
            concatformatstring(text, " \n\f4(Description: \f0%s\f4)", customdesc);
            ASSERT(MAXSTRLEN > 2 * DHDR_DESCCHARS);
            text[textlen + DHDR_DESCCHARS - 1] = '\0';
        }
        return newstring(text);
    }
    else if(!strcmp(ext, "cgz"))
    {
        stream *f = opengzfile(fn, "rb");
        if(!f) return NULL;
        header hdr;
        if(f->read(&hdr, sizeof(header))!=sizeof(header) || (strncmp(hdr.head, "CUBE", 4) && strncmp(hdr.head, "ACMP",4))) { delete f; return NULL; }
        delete f;
        lilswap(&hdr.version, 1);
        // hdr.maprevision, hdr.maptitle ... hdr.version, hdr.headersize,
        formatstring(text)("%s%s", (hdr.version>MAPVERSION) ? "(incompatible file) " : "", hdr.maptitle);
        text[DHDR_DESCCHARS - 1] = '\0';
        return newstring(text);
    }
    return NULL;
}

inline gmenu *setcurmenu(gmenu *newcurmenu)      // only change curmenu through here!
{
    curmenu = newcurmenu;
    extern bool saycommandon;
    if(!editmode && !saycommandon) keyrepeat(curmenu && curmenu->allowinput && !curmenu->hotkeys);
    return curmenu;
}

void menuset(void *m, bool save)
{
    if(curmenu==m) return;
    if(curmenu)
    {
        if(save && curmenu->allowinput) menustack.add(curmenu);
        else curmenu->close();
    }
    if(setcurmenu((gmenu *)m)) curmenu->open();
}

void showmenu(const char *name, bool top)
{
    if(!name)
    {
        setcurmenu(NULL);
        return;
    }
    gmenu *m = menus.access(name);
    if(!m) return;
    if(!top && curmenu)
    {
        if(curmenu==m) return;
        loopv(menustack) if(menustack[i]==m) return;
        menustack.insert(0, m);
        return;
    }
    menuset(m);
}

void closemenu(const char *name)
{
    gmenu *m;
    if(!name)
    {
        if(curmenu) curmenu->close();
        while(!menustack.empty())
        {
            m = menustack.pop();
            if(m) m->close();
        }
        setcurmenu(NULL);
        return;
    }
    m = menus.access(name);
    if(!m) return;
    if(curmenu==m) menuset(menustack.empty() ? NULL : menustack.pop(), false);
    else loopv(menustack)
    {
        if(menustack[i]==m)
        {
            menustack.remove(i);
            return;
        }
    }
}
COMMAND(closemenu, "s");

void showmenu_(const char *name)
{
    showmenu(name, true);
}
COMMANDN(showmenu, showmenu_, "s");

const char *persistentmenuselectionalias(const char *name)
{
    static defformatstring(al)("__lastmenusel_%s", name);
    filtertext(al, al, FTXT__ALIAS);
    return al;
}

void menuselect(void *menu, int sel)
{
    gmenu &m = *(gmenu *)menu;

    if(sel<0) sel = m.items.length()>0 ? m.items.length()-1 : 0;
    else if(sel>=m.items.length()) sel = 0;

    if(m.items.inrange(sel))
    {
        int oldsel = m.menusel;
        m.menusel = sel;
        if(m.allowinput)
        {
            if(sel!=oldsel)
            {
                if(m.items.inrange(oldsel)) m.items[oldsel]->focus(false);
                m.items[sel]->focus(true);
                audiomgr.playsound(S_MENUSELECT, SP_HIGHEST);
                if(m.persistentselection)
                {
                    defformatstring(val)("%d", sel);
                    alias(persistentmenuselectionalias(m.name), val);
                }
            }
        }
    }
}

void drawarrow(int dir, int x, int y, int size, float r = 1.0f, float g = 1.0f, float b = 1.0f)
{
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glColor3f(r, g, b);

    glBegin(GL_TRIANGLES);
    glVertex2f(x, dir ? y+size : y);
    glVertex2f(x+size/2, dir ? y : y+size);
    glVertex2f(x+size, dir ? y+size : y);
    glEnd();
    xtraverts += 3;

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
}

#define MAXMENU 34

bool menuvisible()
{
    if(!curmenu) return false;
    return true;
}

extern void *scoremenu;

void rendermenu()
{
    if(curmenu != scoremenu) setscope(false);
    gmenu &m = *curmenu;
    m.refresh();
    m.render();
}

void mitem::render(int x, int y, int w)
{
    if(isselection()) renderbg(x, y, w, menuselbgcolor);
    else if(bgcolor) renderbg(x, y, w, bgcolor);
    if(!menuseldescbgcolor)
    {
        static color seldbcolor(0.2f, 0.2f, 0.2f, 0.2f);
        menuseldescbgcolor = &seldbcolor;
    }
}

void mitem::renderbg(int x, int y, int w, color *c)
{
    if(isselection()) blendbox(x-FONTH/4, y-FONTH/6, x+w+FONTH/4, y+FONTH+FONTH/6, false, -1, c);
    else blendbox(x, y, x+w, y+FONTH, false, -1, c);
}

bool mitem::isselection() { return parent->allowinput && !parent->hotkeys && parent->items.inrange(parent->menusel) && parent->items[parent->menusel]==this; }

color mitem::gray(0.2f, 0.2f, 0.2f);
color mitem::white(1.0f, 1.0f, 1.0f);
color mitem::whitepulse(1.0f, 1.0f, 1.0f);

// text item

struct mitemmanual : mitem
{
    const char *text, *action, *hoveraction, *desc;

    mitemmanual(gmenu *parent, char *text, char *action, char *hoveraction, color *bgcolor, const char *desc) : mitem(parent, bgcolor, mitem::TYPE_MANUAL), text(text), action(action), hoveraction(hoveraction), desc(desc) {}

    virtual int width() { return text_width(text); }

    virtual void render(int x, int y, int w)
    {
        mitem::render(x, y, w);
        draw_text(text, x, y);
    }

    virtual void focus(bool on)
    {
        if(on && hoveraction) execute(hoveraction);
    }

    virtual void select()
    {
        if(action && action[0])
        {
            gmenu *oldmenu = curmenu;
            push("arg1", text);
            setcontext("menu", curmenu->name);
            int result = execute(action);
            resetcontext();
            pop("arg1");
            if(result >= 0 && oldmenu == curmenu)
            {
                menuset(NULL, false);
                menustack.shrink(0);
            }
        }
    }
    virtual const char *getdesc() { return desc; }
    virtual const char *gettext() { return text; }
    virtual const char *getaction() { return action; }
};

struct mitemtext : mitemmanual
{
    mitemtext(gmenu *parent, char *text, char *action, char *hoveraction, color *bgcolor, const char *desc = NULL) : mitemmanual(parent, text, action, hoveraction, bgcolor, desc) {}
    virtual ~mitemtext()
    {
        DELETEA(text);
        DELETEA(action);
        DELETEA(hoveraction);
        DELETEA(desc);
    }
};

struct mitemtextvar : mitemmanual
{
    string dtext;
    const char *textexp;
    mitemtextvar(gmenu *parent, const char *evalp, char *action, char *hoveraction) : mitemmanual(parent, dtext, action, hoveraction, NULL, NULL)
    {
        dtext[0] = '\0';
        textexp = evalp;
    }
    virtual ~mitemtextvar()
    {
        DELETEA(textexp);
        DELETEA(action);
        DELETEA(hoveraction);
    }
    virtual void init()
    {
        char *r = executeret(textexp);
        copystring(dtext, r ? r : "");
        if(r) delete[] r;
    }
};

VARP(hidebigmenuimages, 0, 0, 1);

struct mitemimagemanual : mitemmanual
{
    Texture *image;
    font *altfont;

    mitemimagemanual(gmenu *parent, const char *filename, const char *altfontname, char *text, char *action, char *hoveraction, color *bgcolor, const char *desc = NULL) : mitemmanual(parent, text, action, hoveraction, bgcolor, desc)
    {
        image = filename ? textureload(filename, 3) : NULL;
        altfont = altfontname ? getfont(altfontname) : NULL;
    }
    virtual ~mitemimagemanual() {}
    virtual int width()
    {
        if(image && *text != '\t') return (FONTH*image->xs)/image->ys + FONTH/2 + mitemmanual::width();
        return mitemmanual::width();
    }
    virtual void render(int x, int y, int w)
    {
        mitem::render(x, y, w);
        if(image || altfont)
        {
            int xs = 0;
            if(image)
            {
                xs = (FONTH*image->xs)/image->ys;
                framedquadtexture(image->id, x, y, xs, FONTH, 0, 255, true);
            }
            draw_text(text, !image || *text == '\t' ? x : x+xs + FONTH/2, y);
            if(altfont && strchr(text, '\a'))
            {
                char *r = newstring(text), *re, *l = r;
                while((re = strchr(l, '\a')) && re[1])
                {
                    *re = '\0';
                    x += text_width(l);
                    l = re + 2;
                    pushfont(altfont->name);
                    draw_textf("%c", x, y, re[1]);
                    popfont();
                }
                delete[] r;
            }
            if(image && isselection() && !hidebigmenuimages && image->ys > FONTH)
            {
                w += FONTH;
                int xs = (2 * VIRTW - w) / 5, ys = (xs * image->ys) / image->xs;
                x = (6 * VIRTW + w - 2 * xs) / 4; y = VIRTH - ys / 2;
                framedquadtexture(image->id, x, y, xs, ys, FONTH);
            }
        }
        else mitemmanual::render(x, y, w);
    }
};

struct mitemimage : mitemimagemanual
{
    mitemimage(gmenu *parent, const char *filename, char *text, char *action, char *hoveraction, color *bgcolor, const char *desc = NULL, const char *altfont = NULL) : mitemimagemanual(parent, filename, altfont, text, action, hoveraction, bgcolor, desc) {}
    virtual ~mitemimage()
    {
        DELETEA(text);
        DELETEA(action);
        DELETEA(hoveraction);
        DELETEA(desc);
    }
};
VARP(maploaditemlength, 0, 46, 255);
struct mitemmaploadmanual : mitemmanual
{
    const char *filename;
    string maptitle;
    string mapstats;
    Texture *image;

    mitemmaploadmanual(gmenu *parent, const char *filename, const char *altfontname, char *text, char *action, char *hoveraction, color *bgcolor, const char *desc = NULL) : mitemmanual(parent, text, action, NULL,        NULL,    NULL), filename(filename)
    {
        image = NULL;
        copystring(maptitle, filename ? filename : "-n/a-");
        if(filename)
        {
            // see worldio.cpp:setnames()
            string pakname, mapname, cgzpath;
            const char *slash = strpbrk(filename, "/\\");
            if(slash)
            {
                copystring(pakname, filename, slash-filename+1);
                copystring(mapname, slash+1);
            }
            else
            {
                copystring(pakname, "maps");
                copystring(mapname, filename);
            }
            formatstring(cgzpath)("packages/%s", pakname);
            char *d = getfiledesc(cgzpath, mapname, "cgz");
            if( d ) { formatstring(maptitle)("%s", d[0] ? d : "-n/a-"); }
            else
            {
                copystring(pakname, "maps/official");
                formatstring(cgzpath)("packages/%s", pakname);
                char *d = getfiledesc("packages/maps/official", mapname, "cgz");
                if( d ) { formatstring(maptitle)("%s", d[0] ? d : "-n/a-"); }
                else formatstring(maptitle)("-n/a-:%s", mapname);
            }
            defformatstring(p2p)("%s/preview/%s.jpg", cgzpath, mapname);
            silent_texture_load = true;
            image = textureload(p2p, 3);
            if(image==notexture) image = textureload("packages/misc/nopreview.jpg", 3);
            silent_texture_load = false;
        }
        else { formatstring(maptitle)("-n/a-:%s", filename); image = textureload("packages/misc/nopreview.png", 3); }
        copystring(mapstats, "");
    }
    virtual ~mitemmaploadmanual() {}
    virtual int width()
    {
        if(image && *text != '\t') return (FONTH*image->xs)/image->ys + FONTH/2 + mitemmanual::width();
        return mitemmanual::width();
    }
    virtual void render(int x, int y, int w)
    {
        mitem::render(x, y, w);
        if(image)
        {
            //int xs = 0;
            draw_text(text, x, y); // !image || *text == '\t' ? x : x+xs + FONTH/2
            if(image && isselection() && !hidebigmenuimages && image->ys > FONTH)
            {
                w += FONTH;
                int xs = (2 * VIRTW - w) / 5, ys = (xs * image->ys) / image->xs;
                x = (6 * VIRTW + w - 2 * xs) / 4; y = VIRTH - ys / 2;
                framedquadtexture(image->id, x, y, xs, ys, FONTH);
                if(maptitle[0])
                {
                    filtertext(maptitle, maptitle, FTXT__MAPMSG);
                    draw_text(maptitle, FONTH/2, VIRTH - FONTH/2, 0xFF, 0xFF, 0xFF, 0xFF, -1, maploaditemlength*FONTH/2);
                }
                //if(mapstats[0]) draw_text(mapstats, x, y+ys+5*FONTH/2);
            }
        }
        else mitemmanual::render(x, y, w);
    }
};

struct mitemmapload : mitemmaploadmanual
{
    mitemmapload(gmenu *parent, const char *filename, char *text, char *action, char *hoveraction, color *bgcolor, const char *desc = NULL) : mitemmaploadmanual(parent, filename, NULL, text, action, hoveraction, bgcolor, desc) {}
//  mitemimage  (gmenu *parent, const char *filename, char *text, char *action, char *hoveraction, color *bgcolor, const char *desc = NULL) : mitemimagemanual  (parent, filename, NULL, text, action, hoveraction, bgcolor, desc) {}
//  mitemmapload(gmenu *parent, const char *filename, char *text, char *action, char *hoveraction, color *bgcolor, const char *desc = NULL) : mitemmaploadmanual(parent, filename, NULL, text, action, hoveraction, bgcolor, desc) {}
//  mitemmanual (gmenu *parent, char *text, char *action, char *hoveraction, color *bgcolor, const char *desc) : mitem(parent, bgcolor), text(text), action(action), hoveraction(hoveraction), desc(desc) {}

    virtual ~mitemmapload()
    {
        DELETEA(filename);
        //DELETEA(maptitle);
        //DELETEA(mapstats);
        DELETEA(text);
        DELETEA(action);
    }
};

// text input item

struct mitemtextinput : mitemtext
{
    textinputbuffer input;
    char *defaultvalueexp;
    bool modified, hideinput;

    mitemtextinput(gmenu *parent, char *text, char *value, char *action, char *hoveraction, color *bgcolor, int maxchars, int maskinput) : mitemtext(parent, text, action, hoveraction, bgcolor), defaultvalueexp(value), modified(false), hideinput(false)
    {
        mitemtype = TYPE_TEXTINPUT;
        copystring(input.buf, value);
        input.max = maxchars > 0 ? maxchars : 15;
        hideinput = (maskinput != 0);
    }

    virtual int width()
    {
        int labelw = text_width(text);
        int maxw = min(input.max, 15)*text_width("w"); // w is broadest, not a - but limit to 15*w
        return labelw + maxw;
    }

    virtual void render(int x, int y, int w)
    {
        bool sel = isselection();
        int tw = max(VIRTW/4, 15*text_width("w"));
        if(sel)
        {
            renderbg(x+w-tw, y-FONTH/6, tw, NULL);
            renderbg(x, y-FONTH/6, w-tw-FONTH/2, menuseldescbgcolor);
        }
        draw_text(text, x, y);
        int cibl = (int)strlen(input.buf); // current input-buffer length
        int iboff = input.pos > 14 ? (input.pos < cibl ? input.pos - 14 : cibl - 14) : input.pos==-1 ? (cibl > 14 ? cibl - 14 : 0) : 0; // input-buffer offset
        string showinput; int sc = 14;
        while(iboff > 0)
        {
            copystring(showinput, input.buf + iboff - 1, sc + 2);
            if(text_width(showinput) > 15 * text_width("w")) break;
            iboff--; sc++;
        }
        while(iboff + sc < cibl)
        {
            copystring(showinput, input.buf + iboff, sc + 2);
            if(text_width(showinput) > 15 * text_width("w")) break;
            sc++;
        }
        copystring(showinput, input.buf + iboff, sc + 1);

        if(hideinput) // "mask" user input with asterisks, use for menuitemtextinputs that take passwords
        {
            for(char *c = showinput; *c; c++)
                *c = '*';
        }

        draw_text(showinput, x+w-tw, y, 255, 255, 255, 255, sel ? (input.pos>=0 ? (input.pos > sc ? sc : input.pos) : cibl) : -1);
    }

    virtual void focus(bool on)
    {
        if(on && hoveraction) execute(hoveraction);

        SDL_EnableUNICODE(on);
        if(!strlen(input.buf)) setdefaultvalue();
        if(action && !on && modified)
        {
            modified = false;
            push("arg1", input.buf);
            execute(action);
            pop("arg1");
        }
    }

    virtual void key(int code, bool isdown, int unicode)
    {
        if(input.key(code, isdown, unicode)) modified = true;
    }

    virtual void init()
    {
        setdefaultvalue();
        modified = false;
    }

    virtual void select() { }

    void setdefaultvalue()
    {
        const char *p = defaultvalueexp;
        char *r = executeret(p);
        copystring(input.buf, r ? r : "");
        if(r) delete[] r;
    }
};

// slider item
VARP(wrapslider, 0, 0, 1);

struct mitemslider : mitem
{
    int min_, max_, step, value, maxvaluewidth;
    char *text, *valueexp, *display, *action;
    string curval;
    static int sliderwidth;

    mitemslider(gmenu *parent, char *text, int min_, int max_, int step, char *value, char *display, char *action, color *bgcolor) : mitem(parent, bgcolor, mitem::TYPE_SLIDER), min_(min_), max_(max_), step(step), value(min_), maxvaluewidth(0), text(text), valueexp(value), display(display), action(action)
    {
        if(sliderwidth==0) sliderwidth = max(VIRTW/4, 15*text_width("w"));  // match slider width with width of text input fields
    }

    virtual ~mitemslider()
    {
        DELETEA(text);
        DELETEA(valueexp);
        DELETEA(display);
        DELETEA(action);
    }

    virtual int width() { return text_width(text) + sliderwidth + maxvaluewidth + 2*FONTH; }

    virtual void render(int x, int y, int w)
    {
        bool sel = isselection();
        int range = max_-min_;
        int cval = value-min_;

        int tw = text_width(text);
        if(sel)
        {
            renderbg(x+w-sliderwidth, y, sliderwidth, NULL);
            renderbg(x, y, w-sliderwidth-FONTH/2, menuseldescbgcolor);
        }
        draw_text(text, x, y);
        draw_text(curval, x+tw, y);

        blendbox(x+w-sliderwidth, y+FONTH/3, x+w, y+FONTH*2/3, false, -1, &gray);
        int offset = (int)(cval/((float)range)*sliderwidth);
        blendbox(x+w-sliderwidth+offset-FONTH/6, y, x+w-sliderwidth+offset+FONTH/6, y+FONTH, false, -1, sel ? &whitepulse : &white);
    }

    virtual void key(int code, bool isdown, int unicode)
    {
        if(code == SDLK_LEFT) slide(false);
        else if(code == SDLK_RIGHT) slide(true);
    }

    virtual void init()
    {
        const char *p = valueexp;
        char *v = executeret(p);
        if(v)
        {
            value = min(max_, max(min_, atoi(v)));
            delete[] v;
        }
        displaycurvalue();
        getmaxvaluewidth();
    }

    void slide(bool right)
    {
        value += right ? step : -step;
        if (wrapslider)
        {
            if (value > max_) value = min_;
            if (value < min_) value = max_;
        }
        else value = min(max_, max(min_, value));
        displaycurvalue();
        if(action)
        {
            string v;
            itoa(v, value);
            push("arg1", v);
            execute(action);
            pop("arg1");
        }
    }

    void displaycurvalue()
    {
        if(display) // extract display name from list
        {
            char *val = indexlist(display, value-min_);
            copystring(curval, val);
            delete[] val;
        }
        else itoa(curval, value); // display number only
    }

    void getmaxvaluewidth()
    {
        int oldvalue = value;
        maxvaluewidth = 0;
        for(int v = min_; v <= max_; v++)
        {
            value = v;
            displaycurvalue();
            maxvaluewidth = max(text_width(curval), maxvaluewidth);
        }
        value = oldvalue;
        displaycurvalue();
    }
};

int mitemslider::sliderwidth = 0;

// key input item

struct mitemkeyinput : mitem
{
    char *text, *bindcmd;
    const char *keyname;
    static const char *unknown, *empty;
    bool capture;

    mitemkeyinput(gmenu *parent, char *text, char *bindcmd, color *bgcolor) : mitem(parent, bgcolor, mitem::TYPE_KEYINPUT), text(text), bindcmd(bindcmd), keyname(NULL), capture(false) {};

    ~mitemkeyinput()
    {
        DELETEA(text);
        DELETEA(bindcmd);
    }

    virtual int width() { return text_width(text)+text_width(keyname ? keyname : " "); }

    virtual void render(int x, int y, int w)
    {
        int tk = text_width(keyname ? keyname : " ");
        static color capturec(0.4f, 0, 0);
        if(isselection()) blendbox(x+w-tk-FONTH, y-FONTH/6, x+w+FONTH, y+FONTH+FONTH/6, false, -1, capture ? &capturec : NULL);
        draw_text(text, x, y);
        draw_text(keyname, x+w-tk, y);
    }

    virtual void init()
    {
        displaycurrentbind();
        capture = false;
    }

    virtual void select()
    {
        capture = true;
        keyname = empty;
    }

    virtual void key(int code, bool isdown, int unicode)
    {
        if(!capture || code < -5 || code > SDLK_MENU) return;
        keym *km;
        while((km = findbinda(bindcmd))) { bindkey(km, ""); } // clear existing binds to this cmd
        if(bindc(code, bindcmd)) parent->init(); // re-init all bindings
        else conoutf("\f3could not bind key");
    }

    void displaycurrentbind()
    {
        keym *km = findbinda(bindcmd);
        keyname = km ? km->name : unknown;
    }
};

const char *mitemkeyinput::unknown = "?";
const char *mitemkeyinput::empty = " ";

// checkbox menu item

struct mitemcheckbox : mitem
{
    char *text, *valueexp, *action;
    bool checked;

    mitemcheckbox(gmenu *parent, char *text, char *value, char *action, color *bgcolor) : mitem(parent, bgcolor, mitem::TYPE_CHECKBOX), text(text), valueexp(value), action(action), checked(false) {};

    ~mitemcheckbox()
    {
        DELETEA(text);
        DELETEA(valueexp);
        DELETEA(action);
    }

    virtual int width() { return text_width(text) + FONTH + FONTH/3; }

    virtual void select()
    {
        checked = !checked;
        if(action && action[0])
        {
            push("arg1", checked ? "1" : "0");
            execute(action);
            pop("arg1");
        }
    }

    virtual void init()
    {
        const char *p = valueexp;
        char *r = executeret(p);
        checked = (r && atoi(r) > 0);
        if(r) delete[] r;
    }

    virtual void render(int x, int y, int w)
    {
        bool sel = isselection();
        const static int boxsize = FONTH;
        draw_text(text, x, y);
        if(sel)
        {
            renderbg(x+w-boxsize, y, boxsize, NULL);
            renderbg(x, y, w-boxsize-FONTH/2, menuseldescbgcolor);
        }
        blendbox(x+w-boxsize, y, x+w, y+boxsize, false, -1, &gray);
        if(checked)
        {
            int x1 = x+w-boxsize-FONTH/6, x2 = x+w+FONTH/6, y1 = y-FONTH/6, y2 = y+boxsize+FONTH/6;
            line(x1, y1, x2, y2, sel ? &whitepulse : &white);
            line(x2, y1, x1, y2, sel ? &whitepulse : &white);
        }
    }
};


// console iface

void *addmenu(const char *name, const char *title, bool allowinput, void (__cdecl *refreshfunc)(void *, bool), bool (__cdecl *keyfunc)(void *, int, bool, int), bool hotkeys, bool forwardkeys)
{
    gmenu *m = menus.access(name);
    if(!m)
    {
        name = newstring(name);
        m = &menus[name];
        m->name = name;
        m->initaction = NULL;
        m->header = m->footer = NULL;
        m->menusel = 0;
    }
    m->title = title;
    m->mdl = NULL;
    m->allowinput = allowinput;
    m->inited = false;
    m->hotkeys = hotkeys;
    m->refreshfunc = refreshfunc;
    m->keyfunc = keyfunc;
    m->usefont = NULL;
    m->allowblink = false;
    m->dirlist = NULL;
    m->forwardkeys = forwardkeys;
    lastmenu = m;
    return m;
}

void newmenu(char *name, int *hotkeys, int *forwardkeys)
{
    addmenu(name, NULL, true, NULL, NULL, *hotkeys > 0, *forwardkeys > 0);
}
COMMAND(newmenu, "sii");

void menureset(void *menu)
{
    gmenu &m = *(gmenu *)menu;
    m.items.deletecontents();
}

void delmenu(const char *name)
{
    if (!name) return;
    gmenu *m = menus.access(name);
    if (!m) return;
    else menureset(m);
}
COMMAND(delmenu, "s");

void menumanual(void *menu, char *text, char *action, color *bgcolor, const char *desc)
{
    gmenu &m = *(gmenu *)menu;
    m.items.add(new mitemmanual(&m, text, action, NULL, bgcolor, desc));
}

void menuimagemanual(void *menu, const char *filename, const char *altfontname, char *text, char *action, color *bgcolor, const char *desc)
{
    gmenu &m = *(gmenu *)menu;
    m.items.add(new mitemimagemanual(&m, filename, altfontname, text, action, NULL, bgcolor, desc));
}

void menutitle(void *menu, const char *title)
{
    gmenu &m = *(gmenu *)menu;
    m.title = title;
}

void menuheader(void *menu, char *header, char *footer)
{
    gmenu &m = *(gmenu *)menu;
    m.header = header && header[0] ? header : NULL;
    m.footer = footer && footer[0] ? footer : NULL;
}

void lastmenu_header(char *header, char *footer)
{
    if(lastmenu)
    {
        menuheader(lastmenu, newstring(header), newstring(footer));
    }
    else conoutf("no last menu to apply to");
}
COMMANDN(menuheader, lastmenu_header, "ss");

void menufont(void *menu, const char *usefont)
{
    gmenu &m = *(gmenu *)menu;
    if(usefont==NULL)
    {
        DELETEA(m.usefont);
        m.usefont = NULL;
    } else m.usefont = newstring(usefont);
}

void setmenufont(char *usefont)
{
    if(!lastmenu) return;
    menufont(lastmenu, usefont);
}
COMMANDN(menufont, setmenufont, "s");

void setmenublink(int *truth)
{
    if(!lastmenu) return;
    gmenu &m = *(gmenu *)lastmenu;
    m.allowblink = *truth != 0;
}
COMMANDN(menucanblink, setmenublink, "i");

void menuinit(char *initaction)
{
    if(!lastmenu) return;
    lastmenu->initaction = newstring(initaction);
}
COMMAND(menuinit, "s");

void menuinitselection(int *line)
{
    if(!lastmenu) return;
    if(lastmenu->items.inrange(*line)) lastmenu->menusel = *line;
}
COMMAND(menuinitselection, "i");

void menuselectionpersistent()
{
    if(!curmenu) return;
    curmenu->persistentselection = true;
    const char *val = getalias(persistentmenuselectionalias(curmenu->name));
    if(val) menuselect(curmenu, ATOI(val));
}
COMMAND(menuselectionpersistent, "");

void menurenderoffset(int *xoff, int *yoff)
{
    if(!lastmenu) return;
    lastmenu->xoffs = *xoff;
    lastmenu->yoffs = *yoff;
}
COMMAND(menurenderoffset, "ii");

void menuselection(char *menu, int *line)
{
    if(!menu || !menus.access(menu)) return;
    gmenu &m = menus[menu];
    menuselect(&m, *line);
}
COMMAND(menuselection, "si");

void menuitem(char *text, char *action, char *hoveraction, char *desc)
{
    if(!lastmenu) return;
    char *t = newstring(text);
    lastmenu->items.add(new mitemtext(lastmenu, t, newstring(action[0] ? action : text), hoveraction[0] ? newstring(hoveraction) : NULL, NULL, *desc ? newstring(desc) : NULL));
}
COMMAND(menuitem, "ssss");

void menuitemvar(char *eval, char *action, char *hoveraction)
{
    if(!lastmenu) return;
    char *t = newstring(eval);
    lastmenu->items.add(new mitemtextvar(lastmenu, t, action[0] ? newstring(action) : NULL, hoveraction[0] ? newstring(hoveraction) : NULL));
}
COMMAND(menuitemvar, "sss");

void menuitemimage(char *name, char *text, char *action, char *hoveraction)
{
    if(!lastmenu) return;
    if(fileexists(name, "r") || findfile(name, "r") != name)
        lastmenu->items.add(new mitemimage(lastmenu, name, newstring(text), action[0] ? newstring(action) : NULL, hoveraction[0] ? newstring(hoveraction) : NULL, NULL));
    else
        lastmenu->items.add(new mitemtext(lastmenu, newstring(text), newstring(action[0] ? action : text), hoveraction[0] ? newstring(hoveraction) : NULL, NULL));
}
COMMAND(menuitemimage, "ssss");

void menuitemaltfont(char *altfont, char *text, char *action, char *hoveraction, char *desc)
{
    if(!lastmenu) return;
    lastmenu->items.add(new mitemimage(lastmenu, NULL, newstring(text), action[0] ? newstring(action) : NULL, hoveraction[0] ? newstring(hoveraction) : NULL, NULL, desc[0] ? newstring(desc) : NULL, altfont));
}
COMMAND(menuitemaltfont, "sssss");

void menuitemmapload(char *name, char *text)
{
    if(!lastmenu) return;
    string caction;
    if(!text || text[0]=='\0') formatstring(caction)("map %s", name);
    else formatstring(caction)("%s", text);
    lastmenu->items.add(new mitemmapload(lastmenu, newstring(name), newstring(name), newstring(caction), NULL, NULL, NULL));
}
COMMAND(menuitemmapload, "ss");

void menuitemtextinput(char *text, char *value, char *action, char *hoveraction, int *maxchars, int *maskinput)
{
    if(!lastmenu || !text || !value) return;
    lastmenu->items.add(new mitemtextinput(lastmenu, newstring(text), newstring(value), action[0] ? newstring(action) : NULL, hoveraction[0] ? newstring(hoveraction) : NULL, NULL, *maxchars, *maskinput));
}
COMMAND(menuitemtextinput, "ssssii");

void menuitemslider(char *text, int *min_, int *max_, char *value, int *step, char *display, char *action)
{
    if(!lastmenu) return;
    lastmenu->items.add(new mitemslider(lastmenu, newstring(text), *min_, *max_, *step, newstring(value), display[0] ? newstring(display) : NULL, action[0] ? newstring(action) : NULL, NULL));
}
COMMAND(menuitemslider, "siisiss");

void menuitemkeyinput(char *text, char *bindcmd)
{
    if(!lastmenu) return;
    lastmenu->items.add(new mitemkeyinput(lastmenu, newstring(text), newstring(bindcmd), NULL));
}
COMMAND(menuitemkeyinput, "ss");

void menuitemcheckbox(char *text, char *value, char *action)
{
    if(!lastmenu) return;
    lastmenu->items.add(new mitemcheckbox(lastmenu, newstring(text), newstring(value), action[0] ? newstring(action) : NULL, NULL));
}
COMMAND(menuitemcheckbox, "sss");

void menumdl(char *mdl, char *anim, int *rotspeed, int *scale)
{
    if(!lastmenu || !mdl || !anim) return;
    gmenu &menu = *lastmenu;
    menu.mdl = newstring(mdl);
    menu.anim = findanim(anim)|ANIM_LOOP;
    menu.rotspeed = clamp(*rotspeed, 0, 100);
    menu.scale = clamp(*scale, 0, 100);
}
COMMAND(menumdl, "ssii");

void menudirlist(char *dir, char *ext, char *action, int *image, char *searchfile)
{
    if(!lastmenu) return;
    if(!action || !action[0]) return;
    gmenu *menu = lastmenu;
    if(menu->dirlist) delete menu->dirlist;
    mdirlist *d = menu->dirlist = new mdirlist;
    d->dir = newstring(dir);
    d->ext = ext[0] ? newstring(ext): NULL;
    d->action = action[0] ? newstring(action) : NULL;
    d->image = *image!=0;
    d->searchfile = searchfile[0] ? newstring(searchfile) : NULL;
}
COMMAND(menudirlist, "sssis");

void chmenumdl(char *menu, char *mdl, char *anim, int *rotspeed, int *scale)
{
    if(!menu || !menus.access(menu)) return;
    gmenu &m = menus[menu];
    DELETEA(m.mdl);
    if(!mdl ||!*mdl) return;
    m.mdl = newstring(mdl);
    m.anim = findanim(anim)|ANIM_LOOP;
    m.rotspeed = clamp(*rotspeed, 0, 100);
    m.scale = clamp(*scale, 0, 100);
}
COMMAND(chmenumdl, "sssii");

void chmenutexture(char *menu, char *texname, char *title)
{
    if(!*menu || !menus.access(menu)) return;
    gmenu &m = menus[menu];
    char *newtex = *texname ? newstring(texname) : NULL;
    DELETEA(m.previewtexture);
    m.previewtexture = newtex;
    DELETEA(m.previewtexturetitle);
    m.previewtexturetitle = *title ? newstring(title) : NULL;
}
COMMAND(chmenutexture, "sss");

bool parsecolor(color *col, const char *r, const char *g, const char *b, const char *a)
{
    if(!r[0] || !col) return false;    // four possible parameter schemes:
    if(!g[0]) g = b = r;               // grey
    if(!b[0]) { a = g; g = b = r; }    // grey alpha
    col->r = ((float) atoi(r)) / 100;  // red green blue
    col->g = ((float) atoi(g)) / 100;  // red green blue alpha
    col->b = ((float) atoi(b)) / 100;
    col->alpha = a[0] ? ((float) atoi(a)) / 100 : 1.0;
    return true;
}

void menuselectionbgcolor(char *r, char *g, char *b, char *a)
{
    if(!menuselbgcolor) menuselbgcolor = new color;
    if(!r[0]) { DELETEP(menuselbgcolor); return; }
    parsecolor(menuselbgcolor, r, g, b, a);
}
COMMAND(menuselectionbgcolor, "ssss");

void menuselectiondescbgcolor(char *r, char *g, char *b, char *a)
{
    if(!menuseldescbgcolor) menuseldescbgcolor = new color;
    if(!r[0]) { DELETEP(menuseldescbgcolor); return; }
    parsecolor(menuseldescbgcolor, r, g, b, a);
}
COMMAND(menuselectiondescbgcolor, "ssss");

static bool iskeypressed(int key)
{
    int numkeys = 0;
    Uint8* state = SDL_GetKeyState(&numkeys);
    return key < numkeys && state[key] != 0;
}

bool menukey(int code, bool isdown, int unicode, SDLMod mod)
{
    if(!curmenu) return false;
    int n = curmenu->items.length(), menusel = curmenu->menusel;
    if(isdown)
    {
        bool hasdesc = false;
        loopv(curmenu->items) if(curmenu->items[i]->getdesc()) { hasdesc = true; break;}
        //int pagesize = MAXMENU - (curmenu->header ? 2 : 0) - (curmenu->footer || hasdesc ? 2 : 0); // FIXME: footer-length
        int pagesize = MAXMENU - (curmenu->header ? 2 : 0) - (curmenu->footer ? (curmenu->footlen?(curmenu->footlen+1):2) : (hasdesc ? 2 : 0)); // FIXME: footer-length

        if(curmenu->items.inrange(menusel))
        {
            mitem *m = curmenu->items[menusel];
            if(m->mitemtype == mitem::TYPE_KEYINPUT && ((mitemkeyinput *)m)->capture && code != SDLK_ESCAPE)
            {
                m->key(code, isdown, unicode);
                return true;
            }
        }

        switch(code)
        {
            case SDLK_PAGEUP: menusel -= pagesize; break;
            case SDLK_PAGEDOWN:
                if(menusel+pagesize>=n && menusel/pagesize!=(n-1)/pagesize) menusel = n-1;
                else menusel += pagesize;
                break;
            case SDLK_ESCAPE:
            case SDL_AC_BUTTON_RIGHT:
                if(!curmenu->allowinput) return false;
                menuset(menustack.empty() ? NULL : menustack.pop(), false);
                return true;
                break;
            case SDLK_UP:
            case SDL_AC_BUTTON_WHEELUP:
                if(iskeypressed(SDLK_LCTRL)) return menukey(SDLK_LEFT, isdown, 0);
                if(iskeypressed(SDLK_LALT)) return menukey(SDLK_RIGHTBRACKET, isdown, 0);
                if(!curmenu->allowinput) return false;
                menusel--;
                break;
            case SDLK_DOWN:
            case SDL_AC_BUTTON_WHEELDOWN:
                if(iskeypressed(SDLK_LCTRL)) return menukey(SDLK_RIGHT, isdown, 0);
                if(iskeypressed(SDLK_LALT)) return menukey(SDLK_LEFTBRACKET, isdown, 0);
                if(!curmenu->allowinput) return false;
                menusel++;
                break;
            case SDLK_TAB:
                if(!curmenu->allowinput) return false;
                if(mod & KMOD_LSHIFT) menusel--;
                else menusel++;
                break;

            case SDLK_1:
            case SDLK_2:
            case SDLK_3:
            case SDLK_4:
            case SDLK_5:
            case SDLK_6:
            case SDLK_7:
            case SDLK_8:
            case SDLK_9:
                if(curmenu->allowinput && curmenu->hotkeys)
                {
                    int idx = code-SDLK_1;
                    if(curmenu->items.inrange(idx))
                    {
                        menuselect(curmenu, idx);
                        mitem &m = *curmenu->items[idx];
                        m.select();
                    }
                    return true;
                }
            default:
            {
                if(!curmenu->allowinput) return false;
                if(curmenu->keyfunc && (*curmenu->keyfunc)(curmenu, code, isdown, unicode)) return true;
                if(!curmenu->items.inrange(menusel)) return false;
                mitem &m = *curmenu->items[menusel];
                m.key(code, isdown, unicode);
                return !curmenu->forwardkeys;
            }
        }

        if(!curmenu->hotkeys) menuselect(curmenu, menusel);
        return true;
    }
    else
    {
        switch(code)   // action on keyup to avoid repeats
        {
            case SDLK_PRINT:
                curmenu->conprintmenu();
                return true;

            case SDLK_F12:
                if(curmenu->allowinput)
                {
                    extern void screenshot(const char *imagepath);
                    screenshot(NULL);
                }
                break;
        }
        if(!curmenu->allowinput || !curmenu->items.inrange(menusel)) return false;
        mitem &m = *curmenu->items[menusel];
        if(code==SDLK_RETURN || code==SDLK_SPACE || code==SDL_AC_BUTTON_LEFT || code==SDL_AC_BUTTON_MIDDLE)
        {
            m.select();
            if(m.getaction()!=NULL && !strcmp(m.getaction(), "-1")) return true; // don't playsound S_MENUENTER if menuitem action == -1 (null/blank/text only item) - Bukz 2013feb13
            audiomgr.playsound(S_MENUENTER, SP_HIGHEST);
            return true;
        }
        return false;
    }
}

void rendermenumdl()
{
    if(!curmenu) return;
    gmenu &m = *curmenu;
    if(!m.mdl) return;

    glPushMatrix();
    glLoadIdentity();
    glRotatef(90+180, 0, -1, 0);
    glRotatef(90, -1, 0, 0);
    glScalef(1, -1, 1);

    bool isplayermodel = !strncmp(m.mdl, "playermodels", strlen("playermodels"));
    bool isweapon = !strncmp(m.mdl, "weapons", strlen("weapons"));

    vec pos;
    if(!isweapon) pos = vec(2.0f, 1.2f, -0.4f);
    else pos = vec(2.0f, 0, 1.7f);

    float yaw = 1.0f;
    if(m.rotspeed) yaw += lastmillis/5.0f/100.0f*m.rotspeed;

    int tex = 0;
    if(isplayermodel)
    {
        defformatstring(skin)("packages/models/%s.jpg", m.mdl);
        tex = -(int)textureload(skin)->id;
    }
    modelattach a[2];
    if(isplayermodel)
    {
        a[0].name = "weapons/subgun/world";
        a[0].tag = "tag_weapon";
    }
    rendermodel(isplayermodel ? "playermodels" : m.mdl, m.anim|ANIM_DYNALLOC, tex, -1, pos, 0, yaw, 0, 0, 0, NULL, a, m.scale ? m.scale/25.0f : 1.0f);

    glPopMatrix();
}

void gmenu::refresh()
{
    if(refreshfunc)
    {
        (*refreshfunc)(this, !inited);
        inited = true;
    }
    if(menusel>=items.length()) menusel = max(items.length()-1, 0);
}

void gmenu::open()
{
    inited = false;
    if(!allowinput) menusel = 0;
    if(!forwardkeys) player1->stopmoving();
    setcontext("menu", name);
    if(initaction) execute(initaction);
    if(items.inrange(menusel)) items[menusel]->focus(true);
    init();
    resetcontext();
}

void gmenu::close()
{
    if(items.inrange(menusel)) items[menusel]->focus(false);
}

void gmenu::conprintmenu()
{
    if(title) conoutf( "[::  %s  ::]", title);//if(items.length()) conoutf( " %03d items", items.length());
    loopv(items) { conoutf("%03d: %s%s%s", 1+i, items[i]->gettext(), items[i]->getaction()?"|\t|":"", items[i]->getaction()?items[i]->getaction():""); }
}

const char *menufilesortorders[] = { "normal", "reverse", "ignorecase", "ignorecase_reverse", "" };
int (*menufilesortcmp[])(const char **, const char **) = { stringsort, stringsortrev, stringsortignorecase, stringsortignorecaserev };

void gmenu::init()
{
    if(dirlist && dirlist->dir && dirlist->ext)
    {
        items.deletecontents();
        vector<char *> files;
        listfiles(dirlist->dir, dirlist->ext, files);
        defformatstring(sortorderalias)("menufilesort_%s", dirlist->ext);
        int sortorderindex = 0;
        const char *customsortorder = getalias(sortorderalias);
        if(customsortorder) sortorderindex = getlistindex(customsortorder, menufilesortorders, true, 0);
        files.sort(menufilesortcmp[sortorderindex]);

        string searchfileuc;
        if(dirlist->searchfile)
        {
            const char *searchfilealias = getalias(dirlist->searchfile);
            copystring(searchfileuc, searchfilealias ? searchfilealias : dirlist->searchfile);
            strtoupper(searchfileuc);
        }

        loopv(files)
        {
            char *f = files[i];
            if(!f || !f[0]) continue;
            char *d = getfiledesc(dirlist->dir, f, dirlist->ext);
            defformatstring(jpgname)("%s/preview/%s.jpg", dirlist->dir, f);
            bool filefound = false;
            if(dirlist->searchfile)
            {
                string fuc, duc;
                copystring(fuc, f);
                strtoupper(fuc);
                copystring(duc, d);
                strtoupper(duc);
                if(strstr(fuc, searchfileuc) || strstr(duc, searchfileuc)) filefound = true;
            }

            if(dirlist->image)
            {
                string fullname = "";
                if(dirlist->dir[0]) formatstring(fullname)("%s/%s", dirlist->dir, f);
                else copystring(fullname, f);
                if(dirlist->ext)
                {
                    concatstring(fullname, ".");
                    concatstring(fullname, dirlist->ext);
                }
                items.add(new mitemimage  (this, newstring(fullname), f, newstring(dirlist->action), NULL, NULL, d));
            }
            else if(!strcmp(dirlist->ext, "cgz"))
            {
                if(!dirlist->searchfile || filefound)
                {
                    int diroffset = 0;
                    if(dirlist->dir[0])
                    {
                        unsigned int ddsl = strlen("packages/");
                        if(strlen(dirlist->dir)>ddsl)
                        {
                            string prefix;
                            copystring(prefix, dirlist->dir, ddsl+1);
                            if(!strcmp(prefix,"packages/")) diroffset = ddsl;
                        }
                    }
                    defformatstring(fullname)("%s%s%s", dirlist->dir[0]?dirlist->dir+diroffset:"", dirlist->dir[0]?"/":"", f);
                    defformatstring(title)("%s", f);
                    items.add(new mitemmapload(this, newstring(fullname), newstring(title), newstring(dirlist->action), NULL, NULL, NULL));
                }
            }
            else if(!strcmp(dirlist->ext, "dmo"))
            {
                if(!dirlist->searchfile || filefound) items.add(new mitemtext(this, f, newstring(dirlist->action), NULL, NULL, d));
            }
            else items.add(new mitemtext(this, f, newstring(dirlist->action), NULL, NULL, d));
        }
    }
    loopv(items) items[i]->init();
}

FVAR(menutexturesize, 0.1f, 1.0f, 5.0f);
FVAR(menupicturesize, 0.1f, 1.6f, 5.0f);

void rendermenutexturepreview(char *previewtexture, int w, const char *title)
{
    static Texture *pt = NULL;
    static char *last_pt = NULL;
    bool ispicture = title != NULL;
    if(previewtexture != last_pt)
    {
        silent_texture_load = ispicture;
        defformatstring(texpath)("packages/textures/%s", previewtexture);
        pt = textureload(texpath, ispicture ? 3 : 0);
        last_pt = previewtexture;
        silent_texture_load = false;
    }
    if(pt && pt != notexture && pt->xs && pt->ys)
    {
        int xs = (VIRTW * (ispicture ? menupicturesize : menutexturesize)) / 4, ys = (xs * pt->ys) / pt->xs, ysmax = (3 * VIRTH) / 2;
        if(ys > ysmax) ys = ysmax, xs = (ys * pt->xs) / pt->ys;
        int x = (6 * VIRTW + w - 2 * xs) / 4, y = VIRTH - ys / 2 - 2 * FONTH;
        extern int fullbrightlevel;
        framedquadtexture(pt->id, x, y, xs, ys, FONTH / 2, fullbrightlevel);
        defformatstring(res)("%dx%d", pt->xs, pt->ys);
        draw_text(title ? title : res, x, y + ys + 2 * FONTH);
    }
}

void gmenu::render()
{
    extern bool ignoreblinkingbit;
    if(usefont) pushfont(usefont);
    if(!allowblink) ignoreblinkingbit = true;
    const char *t = title;
    if(!t)
    {
        static string buf;
        if(hotkeys) formatstring(buf)("%s hotkeys", name);
        else formatstring(buf)("[ %s menu ]", name);
        t = buf;
    }
    bool hasdesc = false;
    if(title) text_startcolumns();
    int w = 0;
    loopv(items)
    {
        int x = items[i]->width();
        if(x>w) w = x;
        if(items[i]->getdesc())
        {
            hasdesc = true;
            x = text_width(items[i]->getdesc());
            if(x>w) w = x;
        }
    }
    //int hitems = (header ? 2 : 0) + (footer || hasdesc ? 2 : 0), // FIXME: footer-length
    int hitems = (header ? 2 : 0) + (footer ? (footlen?(footlen+1):2) : (hasdesc ? 2 : 0)),
        pagesize = MAXMENU - hitems,
        offset = menusel - (menusel%pagesize),
        mdisp = min(items.length(), pagesize),
        cdisp = min(items.length()-offset, pagesize);
    mitem::whitepulse.alpha = (sinf(lastmillis/200.0f)+1.0f)/2.0f;
    int tw = text_width(t);
    if(tw>w) w = tw;
    if(header)
    {
        int hw = text_width(header);
        if(hw>w) w = hw;
    }

    int step = (FONTH*5)/4;
    int h = (mdisp+hitems+2)*step;
    int y = (2*VIRTH-h)/2;
    int x = hotkeys ? (2*VIRTW-w)/6 : (2*VIRTW-w)/2;
    x = clamp(x + (VIRTW * xoffs) / 100, 3 * FONTH, 2 * VIRTW - w - 3 * FONTH);
    y = clamp(y + (VIRTH * yoffs) / 100, 3 * FONTH, 2 * VIRTH - h - 3 * FONTH);
    if(!hotkeys) renderbg(x-FONTH*3/2, y-FONTH, x+w+FONTH*3/2, y+h+FONTH, true);
    if(offset>0)                        drawarrow(1, x+w+FONTH*3/2-FONTH*5/6, y-FONTH*5/6, FONTH*2/3);
    if(offset+pagesize<items.length()) drawarrow(0, x+w+FONTH*3/2-FONTH*5/6, y+h+FONTH/6, FONTH*2/3);
    if(header)
    {
        draw_text(header, x, y);
        y += step*2;
    }
    draw_text(t, x, y);
    y += step*2;
    loopj(cdisp)
    {
        items[offset+j]->render(x, y, w);
        y += step;
    }
    if(title) text_endcolumns();
    if(footer || hasdesc)
    {
        y += ((mdisp-cdisp)+1)*step;
        if(!hasdesc)
        {
            footlen = 0;
            if(text_width(footer)>w)
            {
                if(w) footlen = (int)ceil((double)text_width(footer) / w);
                draw_text(footer, x, y, 0xFF, 0xFF, 0xFF, 0xFF, -1, w);
            }
            else draw_text(footer, x, y);
        }
        else if(items.inrange(menusel) && items[menusel]->getdesc())
            draw_text(items[menusel]->getdesc(), x, y);

    }
    if(previewtexture && *previewtexture) rendermenutexturepreview(previewtexture, w, previewtexturetitle);
    if(usefont) popfont(); // setfont("default");
    ignoreblinkingbit = false;
}

void gmenu::renderbg(int x1, int y1, int x2, int y2, bool border)
{
    static Texture *tex = NULL;
    if(!tex) tex = textureload("packages/textures/makke/menu.jpg");
    static color transparent(1, 1, 1, 0.75f);
    blendbox(x1, y1, x2, y2, border, tex->id, allowinput ? NULL : &transparent);
}

// apply changes menu

void *applymenu = NULL;
static vector<const char *> needsapply;
VARP(applydialog, 0, 1, 1);

void addchange(const char *desc, int type)
{
    if(!applydialog) return;
    if(type!=CHANGE_GFX)
    {
        conoutf("..restart AssaultCube for this setting to take effect");
        return;
    }
    bool changed = false;
    loopv(needsapply) if(!strcmp(needsapply[i], desc)) { changed = true; break; }
    if(!changed) needsapply.add(desc);
    showmenu("apply", false);
}

void clearchanges(int type)
{
    if(type!=CHANGE_GFX) return;
    needsapply.shrink(0);
    closemenu("apply");
}

void refreshapplymenu(void *menu, bool init)
{
    gmenu *m = (gmenu *) menu;
    if(!m || (!init && needsapply.length() != m->items.length()-3)) return;
    m->items.deletecontents();
    loopv(needsapply) m->items.add(new mitemtext(m, newstring(needsapply[i]), NULL, NULL, NULL));
    m->items.add(new mitemtext(m, newstring(""), NULL, NULL, NULL));
    m->items.add(new mitemtext(m, newstring("Yes"), newstring("resetgl"), NULL, NULL));
    m->items.add(new mitemtext(m, newstring("No"), newstring("echo [..restart AssaultCube to apply the new settings]"), NULL, NULL));
    if(init) m->menusel = m->items.length()-2; // select OK
}

void setscorefont();
VARFP(scorefont, 0, 0, 1, setscorefont());
void setscorefont()
{
    switch(scorefont)
    {
        case 1: menufont(scoremenu, "mono"); break;

        case 0:
        default: menufont(scoremenu, NULL); break;
    }
}
