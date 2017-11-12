// menus.cpp: ingame menu system (also used for scores and serverlist)

#include "cube.h"

hashtable<const char *, gmenu> menus;
gmenu *curmenu = NULL, *lastmenu = NULL;
color *menuselbgcolor, *menuseldescbgcolor = NULL;
static int menurighttabwidth = 888; // width of sliders and text input fields (is adapted to font and screen size later)

vector<gmenu *> menustack;

COMMANDF(curmenu, "", () {result(curmenu ? curmenu->name : "");} );

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
    menurighttabwidth = max(VIRTW/4, 15 * text_width("w")); // adapt to screen and font size every time a menu opens
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
                if(!m.items[sel]->greyedout) m.items[sel]->focus(true);
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
}

void mitem::renderbg(int x, int y, int w, color *c)
{
    if(isselection()) blendbox(x-FONTH/4, y-FONTH/6, x+w+FONTH/4, y+FONTH+FONTH/6, false, -1, c);
    else blendbox(x, y, x+w, y+FONTH, false, -1, c);
}

int mitem::execaction(const char *arg1)
{
    int result = 0;
    if(getaction())
    {
        if(curmenu) setcontext("menu", curmenu->name);
        push("arg1", arg1);
        result = execute(getaction());
        pop("arg1");
        resetcontext();
    }
    return result;
}

bool mitem::isselection() { return parent->allowinput && !parent->hotkeys && parent->items.inrange(parent->menusel) && parent->items[parent->menusel]==this; }

color mitem::gray(0.2f, 0.2f, 0.2f);
color mitem::white(1.0f, 1.0f, 1.0f);
color mitem::whitepulse(1.0f, 1.0f, 1.0f);

bool mitem::menugreyedout = false;

// text item

struct mitemmanual : mitem
{
    const char *text, *action, *hoveraction, *desc;

    mitemmanual(gmenu *parent, char *text, char *action, char *hoveraction, color *bgcolor, const char *desc) : mitem(parent, bgcolor, mitem::TYPE_MANUAL), text(text), action(action), hoveraction(hoveraction), desc(desc) {}

    virtual int width() { return text_width(text) + (strchr(text, '\n') ? menurighttabwidth : 0); }

    virtual void render(int x, int y, int w)
    {
        int c = greyedout ? 128 : 255;
        mitem::render(x, y, w);
        const char *nl = strchr(text, '\n');
        if(nl)
        {
            string l;
            copystring(l, text, min(MAXSTRLEN, (int)strcspn(text, "\n") + 1));
            draw_text(l, x, y, c, c, c);
            draw_text(nl + 1, x + w - max(menurighttabwidth, text_width(nl + 1)), y, c, c, c);
        }
        else draw_text(text, x, y, c, c, c);
    }

    virtual void focus(bool on)
    {
        if(on && hoveraction) execute(hoveraction);
    }

    virtual int select()
    {
        int result = 0;
        if(action && action[0])
        {
            gmenu *oldmenu = curmenu;
            result = execaction(text);
            if(result >= 0 && oldmenu == curmenu)
            {
                menuset(NULL, false);
                menustack.shrink(0);
            }
        }
        return result;
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
            int xs = 0, c = greyedout ? 128 : 255;
            if(image)
            {
                xs = (FONTH*image->xs)/image->ys;
                framedquadtexture(image->id, x, y, xs, FONTH, 0, 255, true);
            }
            draw_text(text, !image || *text == '\t' ? x : x+xs + FONTH/2, y, c, c, c);
            if(altfont && strchr(text, '\a'))
            {
                char *r = newstring(text), *re, *l = r;
                while((re = strchr(l, '\a')) && re[1])
                {
                    *re = '\0';
                    x += text_width(l);
                    l = re + 2;
                    pushfont(altfont->name);
                    draw_textf("%s%c", x, y, greyedout ? "\f4" : "", re[1]);
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

VARP(browsefiledesc, 0, 1, 1);

struct mitemmapload : mitemmanual
{
    const char *filename, *mapmessage;
    Texture *image;

    mitemmapload(gmenu *parent, const char *filename, char *text, char *action, char *hoveraction, const char *desc) : mitemmanual(parent, text, action, hoveraction, NULL, desc), filename(filename), mapmessage(NULL), image(NULL) {}

    virtual ~mitemmapload()
    {
        delstring(filename);
        DELETEA(mapmessage);
        DELETEA(text);
        DELETEA(action);
        DELETEA(hoveraction);
        DELETEA(desc);
    }

    virtual void key(int code, bool isdown, int unicode)
    {
        if(code == SDLK_LEFT && parent->xoffs > -50) parent->xoffs -= 2;
        else if(code == SDLK_RIGHT && parent->xoffs < 50) parent->xoffs += 2;
    }

    virtual void render(int x, int y, int w)
    {
        int c = greyedout ? 128 : 255;
        mitem::render(x, y, w);
        draw_text(text, x, y, c, c, c);
        if(isselection())
        {
            if(!image && !mapmessage)
            { // load map description and preview picture
                silent_texture_load = true;
                const char *cgzpath = "packages" PATHDIVS "maps";
                if(browsefiledesc)
                { // checking map messages is slow and may be disabled
                    mapmessage = getfiledesc(cgzpath, behindpath(filename), "cgz"); // check for regular map
                    if(!mapmessage) mapmessage = getfiledesc((cgzpath = "packages" PATHDIVS "maps" PATHDIVS "official"), behindpath(filename), "cgz"); // check for official map
                }
                defformatstring(p2p)("%s/preview/%s.jpg", cgzpath, filename);
                if(!hidebigmenuimages) image = textureload(p2p, 3);
                if(!image || image == notexture) image = textureload("packages/misc/nopreview.jpg", 3);
                silent_texture_load = false;
            }
            w += FONTH;
            int xs = (2 * VIRTW - w - x) / 2, xp = x + w + xs / 2, ym = VIRTH - FONTH/2;
            if(image && !hidebigmenuimages && image->ys > FONTH)
            {
                int ys = (xs * image->ys) / image->xs, yp = VIRTH - ys / 2;
                ym = yp + ys + 2 * FONTH;
                framedquadtexture(image->id, xp, yp, xs, ys, FONTH);
                draw_text(text, xp + xs/2 - text_width(text)/2, yp + ys);
            }
            if(mapmessage && *mapmessage) draw_text(mapmessage, xp - FONTH, ym, 0xFF, 0xFF, 0xFF, 0xFF, -1, 2 * VIRTW - (xp - FONTH) - FONTH/2);
        }
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
        input.max = maxchars > 0 ? min(maxchars, MAXSTRLEN - 1) : 15;
        hideinput = (maskinput != 0);
    }

    virtual int width()
    {
        return text_width(text) + menurighttabwidth;
    }

    virtual void render(int x, int y, int w)
    {
        int c = greyedout ? 128 : 255;
        bool sel = isselection();
        if(sel)
        {
            renderbg(x+w-menurighttabwidth, y-FONTH/6, menurighttabwidth, menuselbgcolor);
            renderbg(x, y-FONTH/6, w-menurighttabwidth-FONTH/2, menuseldescbgcolor);
        }
        draw_text(text, x, y, c, c, c);
        int cibl = (int)strlen(input.buf); // current input-buffer length
        int iboff = input.pos > 14 ? (input.pos < cibl ? input.pos - 14 : cibl - 14) : (input.pos == -1 ? (cibl > 14 ? cibl - 14 : 0) : 0); // input-buffer offset
        string showinput, tempinputbuf; int sc = 14;
        copystring(tempinputbuf, input.buf);
        if(hideinput && !(SDL_GetModState() & MOD_KEYS_CTRL))
        { // "mask" user input with asterisks, use for menuitemtextinputs that take passwords
            for(char *c = tempinputbuf; *c; c++)
                *c = '*';
        }
        while(iboff > 0)
        {
            copystring(showinput, tempinputbuf + iboff - 1, sc + 2);
            if(text_width(showinput) > menurighttabwidth) break;
            iboff--; sc++;
        }
        while(iboff + sc < cibl)
        {
            copystring(showinput, tempinputbuf + iboff, sc + 2);
            if(text_width(showinput) > menurighttabwidth) break;
            sc++;
        }
        copystring(showinput, tempinputbuf + iboff, sc + 1);
        draw_text(showinput, x+w-menurighttabwidth, y, c, c, c, 255, sel && !greyedout ? (input.pos>=0 ? (input.pos > sc ? sc : input.pos) : cibl) : -1);
    }

    virtual void focus(bool on)
    {
        if(on && hoveraction) execute(hoveraction);

        SDL_EnableUNICODE(on);
        if(action && !on && modified && parent->items.find(this) != parent->items.length() - 1)
        {
            modified = false;
            execaction(input.buf);
        }
    }

    virtual void key(int code, bool isdown, int unicode)
    {
        if(input.key(code, isdown, unicode)) modified = true;
        if(action && code == SDLK_RETURN && modified && parent->items.find(this) != parent->items.length() - 1)
        {
            modified = false;
            execaction(input.buf);
        }
    }

    virtual void init()
    {
        setdefaultvalue();
        modified = false;
    }

    virtual int select()
    {
        int result = 0;
        if(parent->menusel == parent->items.length() - 1)
        {
            const char *tmp = text;
            text = input.buf;
            result = mitemmanual::select();
            text = tmp;
        }
        return result;
    }

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
    char *text, *valueexp, *action;
    string curval;
    vector<char *> opts;
    bool wrap, isradio;

    mitemslider(gmenu *parent, char *text, int _min, int _max, char *value, char *display, char *action, color *bgcolor, bool wrap, bool isradio) : mitem(parent, bgcolor, mitem::TYPE_SLIDER),
                               min_(_min), max_(_max), value(_min), maxvaluewidth(0), text(text), valueexp(value), action(action), wrap(wrap), isradio(isradio)
    {
        char *enddigits;
        step = (int) strtol(display, &enddigits, 10);
        if(!*display || *enddigits)
        {
            step = 1;
            explodelist(display, opts);
            if(max_ - min_ + 1 != opts.length())
            {
                if(max_ != -1) clientlogf("menuitemslider: display string length (%d) doesn't match max-min (%d) \"%s\" [%s]", opts.length(), max_ - min_ + 1, text, display);
                max_ = min_ + opts.length() - 1;
            }
        }
        getmaxvaluewidth();
    }

    virtual ~mitemslider()
    {
        opts.deletearrays();
        DELETEA(text);
        DELETEA(valueexp);
        DELETEA(action);
    }

    virtual int width() { return text_width(text) + (isradio ? 0 : menurighttabwidth) + maxvaluewidth + 2*FONTH; }

    virtual void render(int x, int y, int w)
    {
        bool sel = isselection();
        int range = max_-min_;
        int cval = value-min_;

        int tw = text_width(text), ow = isradio ? text_width(curval) : menurighttabwidth, pos = !isradio || ow < menurighttabwidth ? menurighttabwidth : ow;
        if(sel)
        {
            renderbg(x + w - pos, y, ow, menuselbgcolor);
            renderbg(x, y, w - pos - FONTH/2, menuseldescbgcolor);
            if(pos - ow > FONTH/2) renderbg(x + w - pos + ow + FONTH/2, y, pos - ow - FONTH/2, menuseldescbgcolor);
        }
        int c = greyedout ? 128 : 255;
        draw_text(text, x, y, c, c, c);
        draw_text(curval, x + (isradio ? w - pos : tw), y, c, c, c);

        if(!isradio)
        {
            blendbox(x+w-menurighttabwidth, y+FONTH/3, x+w, y+FONTH*2/3, false, -1, &gray);
            int offset = (int)(cval/((float)range)*menurighttabwidth);
            blendbox(x+w-menurighttabwidth+offset-FONTH/6, y, x+w-menurighttabwidth+offset+FONTH/6, y+FONTH, false, -1, greyedout ? &gray : (sel ? &whitepulse : &white));
        }
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
            value = clamp(int(ATOI(v)), min_, max_);
            delete[] v;
        }
        displaycurvalue();
    }

    void slide(bool right)
    {
        value += right ? step : -step;
        if (wrapslider || wrap)
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
            execaction(v);
        }
    }

    void displaycurvalue()
    {
        if(isradio)
        {
            *curval = '\0';
            loopv(opts) concatformatstring(curval, "%s\1%c %s", i ? "  " : "", (i + min_ == value) ? '\11' : '\10', opts[i]);
        }
        else if(opts.length()) // extract display name from list
        {
            int idx = value - min_;
            copystring(curval, opts.inrange(idx) ? opts[idx] : "");
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

    virtual const char *getaction() { return action; }
};

// key input item

struct mitemkeyinput : mitem
{
    char *text, *bindcmd;
    const char *keyname;
    vector<int> newkeys;
    int isup, bindtype;
    string keynames;
    static const char *unknown, *empty;
    bool capture;

    mitemkeyinput(gmenu *parent, char *text, char *bindcmd, color *bgcolor, int bindtype) : mitem(parent, bgcolor, mitem::TYPE_KEYINPUT), text(text), bindcmd(bindcmd),
                                                                              keyname(NULL), isup(0), bindtype(bindtype), capture(false) {};

    ~mitemkeyinput()
    {
        DELETEA(text);
        DELETEA(bindcmd);
    }

    virtual int width() { return text_width(text)+text_width(keyname ? keyname : " "); }

    virtual void render(int x, int y, int w)
    {
        int tw = text_width(keyname ? keyname : " "), c = greyedout ? 128 : 255;
        static color capturec(0.4f, 0, 0);
        if(isselection())
        {
            blendbox(x+w-tw-FONTH, y-FONTH/6, x+w+FONTH, y+FONTH+FONTH/6, false, -1, capture ? &capturec : NULL);
            blendbox(x, y-FONTH/6, x+w-tw-FONTH, y+FONTH+FONTH/6, false, -1, menuseldescbgcolor);
        }
        draw_text(text, x, y, c, c, c);
        draw_text(keyname, x+w-tw, y, c, c, c);
    }

    virtual void init()
    {
        keynames[0] = 0;
        for(keym **km = findbinda(bindcmd, bindtype); *km; km++) concatformatstring(keynames, " or %s", (*km)->name);
        keyname = *keynames ? keynames + 4 : unknown;
        capture = false;
        menuheader(parent, NULL, NULL);
    }

    virtual int select()
    {
        capture = true;
        isup = 0;
        keyname = empty;
        newkeys.setsize(0);
        keynames[0] = '\0';
        return 0;
    }

    virtual void key(int code, bool isdown, int unicode)
    {
        keym *km;
        if(!capture || code < -5 || code > SDLK_MENU || !((km = findbindc(code)))) return;
        if(code == SDLK_ESCAPE)
        {
            capture = false;
            parent->init();
            return;
        }
        bool known = newkeys.find(code) >= 0;
        if(isdown)
        {
            if(!known)
            {
                if(km->actions[bindtype][0] && strcmp(km->actions[bindtype], bindcmd))
                {
                    static defformatstring(keybound)("\f3Warning: \"%s\" key is already in use. You can press ESC to cancel.", km->name);
                    menuheader(parent, NULL, keybound);
                }
                newkeys.add(code);
                concatformatstring(keynames, "%s or ", km->name);
                keyname = keynames;
            }
        }
        else
        {
            if(known && ++isup == newkeys.length())
            {
                for(keym **km = findbinda(bindcmd, bindtype); *km; km++) bindkey(*km, "", bindtype);  // clear existing binds to this cmd
                bool bindfailed = false;
                loopv(newkeys) bindfailed = !bindc(newkeys[i], bindcmd, bindtype) || bindfailed;
                if(bindfailed) conoutf("\f3could not bind key");
                else parent->init(); // show new keys
            }
        }
    }

};

const char *mitemkeyinput::unknown = "?";
const char *mitemkeyinput::empty = " ";

// checkbox menu item

struct mitemcheckbox : mitem
{
    char *text, *valueexp, *action;
    int pos;
    bool checked;

    mitemcheckbox(gmenu *parent, char *text, char *value, char *action, int pos, color *bgcolor) : mitem(parent, bgcolor, mitem::TYPE_CHECKBOX), text(text), valueexp(value), action(action), pos(pos), checked(false) {}

    ~mitemcheckbox()
    {
        DELETEA(text);
        DELETEA(valueexp);
        DELETEA(action);
    }

    virtual int width() { return text_width(text) + (pos ? menurighttabwidth : FONTH + FONTH/3); }

    virtual int select()
    {
        checked = !checked;
        return execaction(checked ? "1" : "0");
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
        int offs = ((menurighttabwidth - FONTH) * ((msctrl % 41) ? pos : (50 + 50 * sinf(totalmillis / 300.0f + y)))) / 100;
        int c = greyedout ? 128 : 255;

        if(sel)
        {
            renderbg(x, y, w-boxsize-offs-FONTH/2, menuseldescbgcolor);
            renderbg(x+w-boxsize-offs, y, boxsize, menuselbgcolor);
            if(offs > FONTH/2) renderbg(x+w-offs+FONTH/2, y, offs-FONTH/2, menuseldescbgcolor);
        }
        draw_text(text, x, y, c, c, c);
        x -= offs;
        blendbox(x+w-boxsize, y, x+w, y+boxsize, false, -1, &gray);
        color *col = greyedout ? &gray : (sel ? &whitepulse : &white);
        if(checked)
        {
            int x1 = x+w-boxsize-FONTH/6, x2 = x+w+FONTH/6, y1 = y-FONTH/6, y2 = y+boxsize+FONTH/6;
            line(x1, y1, x2, y2, col);
            line(x2, y1, x1, y2, col);
        }
    }

    virtual const char *getaction() { return action; }
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
    DELSTRING(m->mdl);
    m->allowinput = allowinput;
    m->inited = false;
    m->hotkeys = hotkeys;
    m->refreshfunc = refreshfunc;
    m->keyfunc = keyfunc;
    DELSTRING(m->usefont);
    m->allowblink = false;
    m->forwardkeys = forwardkeys;
    lastmenu = m;
    mitem::menugreyedout = false;
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

void menuitemmanual(void *menu, char *text, char *action, color *bgcolor, const char *desc)
{
    gmenu &m = *(gmenu *)menu;
    m.items.add(new mitemmanual(&m, text, action, NULL, bgcolor, desc));
}

void menuimagemanual(void *menu, const char *filename, const char *altfontname, char *text, char *action, color *bgcolor, const char *desc)
{
    gmenu &m = *(gmenu *)menu;
    m.items.add(new mitemimagemanual(&m, filename, altfontname, text, action, NULL, bgcolor, desc));
}

void menutitlemanual(void *menu, const char *title)
{
    gmenu &m = *(gmenu *)menu;
    m.title = title;
}

void menuheader(void *menu, char *header, char *footer, bool heap)
{
    gmenu &m = *(gmenu *)menu;
    if(m.headfootheap) { DELSTRING(m.header); DELSTRING(m.footer); }
    m.header = (header && *header) || heap ? header : NULL;
    m.footer = (footer && *footer) || heap ? footer : NULL;
    m.headfootheap = heap; // false: "manual"
}

COMMANDF(menuheader, "ss", (char *header, char *footer) { if(lastmenu) menuheader(lastmenu, *header ? newstring(header) : NULL, *footer ? newstring(footer) : NULL, true); });

void menufont(char *menu, const char *usefont)
{
    gmenu *m = *menu ? menus.access(menu) : lastmenu;
    if(m)
    {
        DELSTRING(m->usefont);
        m->usefont = usefont && *usefont ? newstring(usefont) : NULL;
    }
}
COMMAND(menufont, "ss");

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
    DELSTRING(lastmenu->initaction);
    lastmenu->initaction = newstring(initaction);
}
COMMAND(menuinit, "s");

void menuinitselection(int *line)
{
    if(!lastmenu) return;
    if(lastmenu->items.inrange(*line)) lastmenu->menusel = *line;
    else lastmenu->menuselinit = *line;
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

void menusynctabstops(int *onoff)
{
    if(curmenu) curmenu->synctabstops = *onoff != 0;
}
COMMAND(menusynctabstops, "i");

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

void menuitemgreyedout(int *onoff)
{
    mitem::menugreyedout = *onoff != 0;
}
COMMAND(menuitemgreyedout, "i");

void menuitem(char *text, char *action, char *hoveraction, char *desc)
{
    if(!lastmenu) return;
    lastmenu->items.add(new mitemtext(lastmenu, newstring(text), newstring(action[0] ? action : text), hoveraction[0] ? newstring(hoveraction) : NULL, NULL, *desc ? newstring(desc) : NULL));
}
COMMAND(menuitem, "ssss");

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

void menuitemmapload(char *name, char *action, char *hoveraction, char *desc)
{
    if(!lastmenu) return;
    lastmenu->items.add(new mitemmapload(lastmenu, newstring(name), newstring(name), *action ? newstring(action) : NULL, *hoveraction ? newstring(hoveraction) : NULL, *desc ? newstring(desc) : NULL));
}
COMMAND(menuitemmapload, "ssss");

void menuitemtextinput(char *text, char *value, char *action, char *hoveraction, int *maxchars, int *maskinput)
{
    if(!lastmenu || !text || !value) return;
    lastmenu->items.add(new mitemtextinput(lastmenu, newstring(text), newstring(value), action[0] ? newstring(action) : NULL, hoveraction[0] ? newstring(hoveraction) : NULL, NULL, *maxchars, *maskinput));
}
COMMAND(menuitemtextinput, "ssssii");

void menuitemslider(char *text, int *min_, int *max_, char *value, char *display, char *action, int *wrap)
{
    if(!lastmenu) return;
    lastmenu->items.add(new mitemslider(lastmenu, newstring(text), *min_, *max_, newstring(value), display, action[0] ? newstring(action) : NULL, NULL, *wrap != 0, false));
}
COMMAND(menuitemslider, "siisssi");

void menuitemradio(char *text, int *min_, int *max_, char *value, char *display, char *action)
{
    if(!lastmenu) return;
    lastmenu->items.add(new mitemslider(lastmenu, newstring(text), *min_, *max_, newstring(value), display, action[0] ? newstring(action) : NULL, NULL, true, true));
}
COMMAND(menuitemradio, "siisss");

void menuitemcheckbox(char *text, char *value, char *action, int *pos)
{
    if(!lastmenu) return;
    lastmenu->items.add(new mitemcheckbox(lastmenu, newstring(text), newstring(value), action[0] ? newstring(action) : NULL, clamp(*pos, 0, 100), NULL));
}
COMMAND(menuitemcheckbox, "sssi");

void menuitemkeyinput(char *text, char *bindcmd)
{
    if(!lastmenu) return;
    lastmenu->items.add(new mitemkeyinput(lastmenu, newstring(text), newstring(bindcmd), NULL, keym::ACTION_DEFAULT));
}
COMMAND(menuitemkeyinput, "ss");

void menuitemeditkeyinput(char *text, char *bindcmd)
{
    if(!lastmenu) return;
    lastmenu->items.add(new mitemkeyinput(lastmenu, newstring(text), newstring(bindcmd), NULL, keym::ACTION_EDITING));
}
COMMAND(menuitemeditkeyinput, "ss");

void menuitemspectkeyinput(char *text, char *bindcmd)
{
    if(!lastmenu) return;
    lastmenu->items.add(new mitemkeyinput(lastmenu, newstring(text), newstring(bindcmd), NULL, keym::ACTION_SPECTATOR));
}
COMMAND(menuitemspectkeyinput, "ss");

void menumdl(char *menu, char *mdl, char *anim, int *rotspeed, int *scale)
{
    gmenu *m = *menu ? menus.access(menu) : lastmenu;
    if(!m) return;
    DELETEA(m->mdl);
    if(!*mdl) return;
    m->mdl = newstring(mdl);
    m->anim = findanim(anim)|ANIM_LOOP;
    m->rotspeed = clamp(*rotspeed, 0, 100);
    m->scale = clamp(*scale, 0, 100);
}
COMMAND(menumdl, "sssii");

void menudirlist(char *dir, char *ext, char *action, int *image, char *searchfile)
{
    if(!lastmenu || !*action) return;
    if(lastmenu->dirlist) delete lastmenu->dirlist;
    mdirlist *d = lastmenu->dirlist = new mdirlist;
    d->dir = newstring(dir);
    d->ext = ext[0] ? newstring(ext): NULL;
    d->action = action[0] ? newstring(action) : NULL;
    d->image = *image!=0;
    d->searchfile = searchfile[0] ? newstring(searchfile) : NULL;
    d->subdiraction = NULL;
    d->subdirdots = false;
}
COMMAND(menudirlist, "sssis");

void menudirlistsub(char *action, int *dots)
{
    if(!lastmenu || !lastmenu->dirlist || !*action) return;
    mdirlist *d = lastmenu->dirlist;
    d->subdirdots = *dots != 0;
    DELETEA(d->subdiraction);
    d->subdiraction = *action ? newstring(action) : NULL;
}
COMMAND(menudirlistsub, "si");

void chmenutexture(char *menu, char *texname, char *title)
{
    gmenu *m = *menu ? menus.access(menu) : NULL;
    if(!m) return;
    DELETEA(m->previewtexture);
    if(*texname) m->previewtexture = newstring(texname);
    DELETEA(m->previewtexturetitle);
    if(*title) m->previewtexturetitle = newstring(title);
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

    if(curmenu->items.inrange(menusel) && !curmenu->items[menusel]->greyedout)
    {
        mitem *m = curmenu->items[menusel];
        if(m->mitemtype == mitem::TYPE_KEYINPUT && ((mitemkeyinput *)m)->capture)
        {
            m->key(code, isdown, unicode);
            return true;
        }
    }

    if(isdown)
    {
        int pagesize = MAXMENU - curmenu->headeritems();

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
                    if(curmenu->items.inrange(idx) && !curmenu->items[idx]->greyedout)
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
                if(!m.greyedout) m.key(code, isdown, unicode);
                if(code == SDLK_HOME && m.mitemtype != mitem::TYPE_TEXTINPUT) menuselect(curmenu, (menusel = 0));
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
                    extern void screenshot(const char *filename);
                    screenshot(NULL);
                }
                break;
        }
        if(!curmenu->allowinput || !curmenu->items.inrange(menusel)) return false;
        mitem &m = *curmenu->items[menusel];
        if(code==SDLK_RETURN || code==SDLK_SPACE || code==SDL_AC_BUTTON_LEFT || code==SDL_AC_BUTTON_MIDDLE)
        {
            if(!m.greyedout && m.select() != -1) audiomgr.playsound(S_MENUENTER, SP_HIGHEST);
            return true;
        }
        return false;
    }
}

bool rendermenumdl()
{
    if(!curmenu) return false;
    gmenu &m = *curmenu;
    if(!m.mdl) return false;

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

    float yaw = 1.0f, pitch = isplayermodel || isweapon ? 0.0f : camera1->pitch;
    if(m.rotspeed) yaw += lastmillis/5.0f/100.0f*m.rotspeed;
    if(fabs(pitch) < 10.0f) pitch = 0.0f;
    else pitch += 10.0f * (pitch < 0 ? 1 : -1);

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
    rendermodel(isplayermodel ? "playermodels" : m.mdl, m.anim|ANIM_DYNALLOC, tex, -1, pos, 0, yaw, pitch, 0, 0, NULL, a, m.scale ? m.scale/25.0f : 1.0f);

    glPopMatrix();
    dimeditinfopanel = 0;
    return !(isplayermodel || isweapon);
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
    if(initaction)
    {
        gmenu *oldlastmenu = lastmenu;
        lastmenu = this;
        execute(initaction);
        lastmenu = oldlastmenu;
    }
    if(items.inrange(menuselinit)) { menusel = menuselinit; menuselinit = -1; }
    if(items.inrange(menusel) && !items[menusel]->greyedout) items[menusel]->focus(true);
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
    if(!menuseldescbgcolor) menuseldescbgcolor = new color(0.2f, 0.2f, 0.2f, 0.2f);
    if(dirlist && dirlist->dir && dirlist->ext)
    {
        items.deletecontents();

        if(dirlist->subdiraction)
        {
            vector<char *> subdirs;
            if(dirlist->subdirdots) subdirs.add(newstring(".."));
            listsubdirs(dirlist->dir, subdirs, stringsort);
            loopv(subdirs)
            {
                defformatstring(cdir)("\fs\f1[%s]\fr", subdirs[i]);
                defformatstring(act)("%s %s", dirlist->subdiraction, escapestring(subdirs[i]));
                items.add(new mitemtext(this, newstring(cdir), newstring(act), NULL, NULL, NULL));
            }
        }

        defformatstring(sortorderalias)("menufilesort_%s", dirlist->ext);
        int sortorderindex = 0;
        const char *customsortorder = getalias(sortorderalias);
        if(customsortorder) sortorderindex = getlistindex(customsortorder, menufilesortorders, true, 0);

        vector<char *> files;
        listfiles(dirlist->dir, dirlist->ext, files, menufilesortcmp[sortorderindex]);

        string searchfileuc;
        if(dirlist->searchfile)
        {
            const char *searchfilealias = getalias(dirlist->searchfile);
            copystring(searchfileuc, searchfilealias ? searchfilealias : dirlist->searchfile);
            strtoupper(searchfileuc);
        }

        loopv(files)
        {
            char *f = files[i], *d = (strcmp(dirlist->ext, "cgz") || dirlist->searchfile) && browsefiledesc ? getfiledesc(dirlist->dir, f, dirlist->ext) : NULL;
            bool filefound = false;
            if(dirlist->searchfile)
            {
                string fuc, duc;
                copystring(fuc, f);
                strtoupper(fuc);
                copystring(duc, d ? d : "");
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
                    items.add(new mitemmapload(this, newstring(f), newstring(f), newstring(dirlist->action), NULL, NULL));
                }
                DELSTRING(d);
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

int gmenu::headeritems()  // returns number of header and footer lines (and also recalculates hasdesc)
{
    hasdesc = false;
    loopv(items) if(items[i]->getdesc()) { hasdesc = true; break; }
    return (header ? 2 : 0) + (footer || hasdesc ? (footlen ? (footlen + 1) : 2) : 0);
}

FVAR(menutexturesize, 0.1f, 1.0f, 5.0f);
FVAR(menupicturesize, 0.1f, 1.6f, 5.0f);

void rendermenutexturepreview(char *previewtexture, int w, const char *title)
{
    static Texture *pt = NULL;
    static uint last_pt = 0;
    bool ispicture = title != NULL;
    uint cur_pt = hthash(previewtexture);
    if(cur_pt != last_pt)
    {
        silent_texture_load = ispicture;
        defformatstring(texpath)("packages/textures/%s", previewtexture);
        pt = textureload(texpath, ispicture ? 3 : 0);
        last_pt = cur_pt;
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
    bool synctabs = title != NULL || synctabstops;
    const char *t = title;
    if(!t)
    {
        static string buf;
        if(hotkeys) formatstring(buf)("%s hotkeys", name);
        else formatstring(buf)("[ %s menu ]", name);
        t = buf;
    }
    int w = 0, footermaxw = 2 * VIRTW - 6 * FONTH, hitems = headeritems(), footwidth, footheight, maxfootheight = 0;
    if(hasdesc) loopv(items) if(items[i]->getdesc())
    {
        text_bounds(items[i]->getdesc(), footwidth, footheight, footermaxw);
        w = max(w, footwidth);
        maxfootheight = max(maxfootheight, footheight);
    }
    if(synctabs) text_startcolumns();
    loopv(items) w = max(w, items[i]->width());
    int pagesize = MAXMENU - hitems,
        offset = menusel - (menusel%pagesize),
        mdisp = min(items.length(), pagesize),
        cdisp = min(items.length()-offset, pagesize),
        pages = (items.length() - 1) / pagesize;
    mitem::whitepulse.alpha = (sinf(lastmillis/200.0f)+1.0f)/2.0f;
    defformatstring(menupages)("%d/%d", (offset / pagesize) + 1, pages + 1);
    int menupageswidth = pages ? text_width(menupages) : 0; // adds to topmost line, either menu title or header
    w = max(w, text_width(t) + (header ? 0 : menupageswidth));
    if(header) w = max(w, text_width(header) + menupageswidth);

    int step = (FONTH*5)/4;
    int h = (mdisp+hitems+2)*step;
    int y = (2*VIRTH-h)/2;
    int x = hotkeys ? (2*VIRTW-w)/6 : (2*VIRTW-w)/2;
    x = clamp(x + (VIRTW * xoffs) / 100, 3 * FONTH, 2 * VIRTW - w - 3 * FONTH);
    y = clamp(y + (VIRTH * yoffs) / 100, 3 * FONTH, 2 * VIRTH - h - 3 * FONTH);

    if(!hotkeys) renderbg(x - FONTH*3/2, y - FONTH, x + w + FONTH*3/2, y + h + FONTH, true);
    if(pages)
    {
        if(offset > 0) drawarrow(1, x + w + FONTH*3/2 - FONTH*5/6, y - FONTH*5/6, FONTH*2/3);
        if(offset + pagesize < items.length()) drawarrow(0, x + w + FONTH*3/2 - FONTH*5/6, y + h + FONTH/6, FONTH*2/3);
        draw_text(menupages, x + w + FONTH*3/2 - FONTH*5/6 - menupageswidth, y);
    }

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
    if(synctabs) text_endcolumns();
    if(footer || hasdesc)
    {
        y += ((mdisp-cdisp)+1)*step;
        const char *usefoot = hasdesc && items.inrange(menusel) ? items[menusel]->getdesc() : footer;
        if(!hasdesc) text_bounds(footer, footwidth, maxfootheight, w);
        if(maxfootheight) footlen = min(MAXMENU / 4, (maxfootheight + step / 2) / step);
        if(usefoot) draw_text(usefoot, x, y, 0xFF, 0xFF, 0xFF, 0xFF, -1, w);
    }
    if(previewtexture && *previewtexture) rendermenutexturepreview(previewtexture, w, previewtexturetitle);
    if(usefont) popfont(); // setfont("default");
    ignoreblinkingbit = false;
}

void gmenu::renderbg(int x1, int y1, int x2, int y2, bool border)
{
    static Texture *tex = NULL;
    if(!tex) tex = textureload("packages/misc/menu.jpg");
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
