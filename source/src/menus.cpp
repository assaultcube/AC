// menus.cpp: ingame menu system (also used for scores and serverlist)

#include "pch.h"
#include "cube.h"

hashtable<const char *, gmenu> menus;
gmenu *curmenu = NULL, *lastmenu = NULL;

vector<gmenu *> menustack;

void menuset(void *m)
{
    if(curmenu==m) return;
    if(curmenu) curmenu->close();
    if((curmenu = (gmenu *)m)) curmenu->open();
}

void showmenu(char *name)
{
    if(!name)
    {
        curmenu = NULL;
        return;
    }
    gmenu *m = menus.access(name);
    if(!m) return;
    menuset(m);
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
                m.items[oldsel]->focus(false);
                m.items[sel]->focus(true);
            }
        }
    }
}

void drawarrow(int dir, int x, int y, int size, float r = 1.0f, float g = 1.0f, float b = 1.0f)
{
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glColor3f(r, g, b);
    
    glBegin(GL_POLYGON);
    glVertex2i(x, dir ? y+size : y);
    glVertex2i(x+size/2, dir ? y : y+size);
    glVertex2i(x+size, dir ? y+size : y);
    glEnd();
    xtraverts += 3;
   
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
}

#define MAXMENU 34

bool menuvisible() 
{ 
    if(!curmenu) 
    { 
        menustack.setsize(0); 
        return false;
    } 
    return true;
}

void rendermenu()
{
    setscope(false);
    gmenu &m = *curmenu;
    m.refresh();
    m.render();
}

void mitem::render(int x, int y, int w)
{
    if(isselection()) renderbg(x, y, w, NULL);
    else if(bgcolor) renderbg(x, y, w, bgcolor);
}

void mitem::renderbg(int x, int y, int w, color *c) 
{ 
    if(isselection()) blendbox(x-FONTH, y-FONTH/6, x+w+FONTH, y+FONTH+FONTH/6, false, -1, c); 
    else blendbox(x, y, x+w, y+FONTH, false, -1, c); 
};
bool mitem::isselection() { return parent->allowinput && parent->items.inrange(parent->menusel) && parent->items[parent->menusel]==this; }

color mitem::gray(0.2f, 0.2f, 0.2f);
color mitem::white(1.0f, 1.0f, 1.0f);
color mitem::whitepulse(1.0f, 1.0f, 1.0f);

// text item

struct mitemtext : mitem
{
    char *text, *action, *hoveraction;

    mitemtext(gmenu *parent, char *text, char *action, char *hoveraction, color *bgcolor) : mitem(parent, bgcolor), text(text), action(action), hoveraction(hoveraction) {}
    virtual ~mitemtext() 
    { 
        DELETEA(text); 
        DELETEA(action); 
        DELETEA(hoveraction);
    }

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
        if(action) 
        { 
            menustack.add(curmenu);
            menuset(NULL); 
            push("arg1", text);
            execute(action);
            pop("arg1");
        } 
    }
};

// text input item

struct mitemtextinput : mitemtext
{
    textinputbuffer input;
    char *defaultvalueexp;
    bool modified;

    mitemtextinput(gmenu *parent, char *text, char *value, char *action, char *hoveraction, color *bgcolor) : mitemtext(parent, text, action, hoveraction, bgcolor), defaultvalueexp(value), modified(false)
    {
        s_strcpy(input.buf, value);
    };

    virtual int width() { return text_width(input.buf) + text_width(text); }

    virtual void render(int x, int y, int w)
    {
        bool selection = isselection();
        int tw = text_width(text);
        if(selection) blendbox(x+tw, y-FONTH/6, x+w+FONTH, y+FONTH+FONTH/6, false);
        draw_text(text, x, y);
        int offset = text_width(input.buf, input.pos>=0 ? input.pos : -1);
        if(selection) rendercursor(x+tw+offset, y, char_width(input.pos>=0 ? input.buf[input.pos] : '_'));
        draw_text(input.buf, x+tw, y);
    }

    virtual void focus(bool on) 
    { 
        SDL_EnableUNICODE(on);
        if(!strlen(input.buf)) setdefaultvalue();
        if(!on && modified)
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

    void setdefaultvalue()
    {
        const char *p = defaultvalueexp;
        char *r = executeret(p);
        s_strcpy(input.buf, r ? r : "");
        if(r) delete[] r;
    }
};

// slider item

struct mitemslider : mitem
{
    int min_, max_, step, value, maxvaluewidth;
    char *text, *valueexp, *display, *action;
    string curval;
    static int sliderwidth;

    mitemslider(gmenu *parent, char *text, int min_, int max_, int step, char *value, char *display, char *action, color *bgcolor) : mitem(parent, bgcolor), min_(min_), max_(max_), step(step), value(min_), maxvaluewidth(0), text(text), valueexp(value), display(display), action(action)
    {
        if(sliderwidth==0) sliderwidth = VIRTW/4;
    }

    virtual ~mitemslider()
    {
        DELETEA(text);
        DELETEA(valueexp);
        DELETEA(display);
        DELETEA(action);
    }

    virtual int width() { return text_width(text) + sliderwidth + maxvaluewidth; }

    virtual void render(int x, int y, int w)
    {
        bool sel = isselection();
        int range = max_-min_;
        int cval = value-min_;

        int tw = text_width(text);
        if(sel) renderbg(x+w-sliderwidth, y, sliderwidth, NULL);
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
        value = min(max_, max(min_, value));
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
            s_strcpy(curval, val);
            delete[] val;
        }
        else itoa(curval, value);
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

    mitemkeyinput(gmenu *parent, char *text, char *bindcmd, color *bgcolor) : mitem(parent, bgcolor), text(text), bindcmd(bindcmd), keyname(NULL), capture(false){};
   
    ~mitemkeyinput()
    {
        DELETEA(text);
        DELETEA(bindcmd);
    }

    virtual int width() { return text_width(text)+(keyname ? text_width(keyname) : char_width('_')); }

    virtual void render(int x, int y, int w)
    {
        int tk = (keyname ? text_width(keyname) : char_width('_'));
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

    mitemcheckbox(gmenu *parent, char *text, char *value, char *action, color *bgcolor) : mitem(parent, bgcolor), text(text), valueexp(value), action(action), checked(false) {};

    ~mitemcheckbox()
    {
        DELETEA(text);
        DELETEA(valueexp);
        DELETEA(action);
    }

    virtual int width() { return text_width(text); }
    
    virtual void select() 
    { 
        checked = !checked; 
        push("arg1", checked ? "1" : "0");
        execute(action);
        pop("arg1");
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
        if(isselection()) renderbg(x+w-boxsize, y, boxsize, NULL);
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

void *addmenu(const char *name, const char *title, bool allowinput, void (__cdecl *refreshfunc)(void *, bool))
{
    name = newstring(name);
    gmenu &menu = menus[name];
    menu.name = name;
    menu.title = title ? newstring(title) : NULL;
    menu.header = menu.footer = NULL;
    menu.menusel = 0;
    menu.mdl = NULL;
    menu.allowinput = allowinput;
    menu.inited = false;
    menu.refreshfunc = refreshfunc;
    menu.dirlist = NULL;
    lastmenu = &menu;
    return &menu;
}

void newmenu(char *name)
{
    addmenu(name);
}

void menumanual(void *menu, int n, char *text, char *action, color *bgcolor)
{
    gmenu &m = *(gmenu *)menu;
    if(!n) m.items.setsize(0);
    m.items.add(new mitemtext(&m, text, action, NULL, bgcolor));
}

void menuheader(void *menu, char *header, char *footer)
{
    gmenu &m = *(gmenu *)menu;
    m.header = header && header[0] ? header : NULL;
    m.footer = footer && footer[0] ? footer : NULL;
}

void menuitem(char *text, char *action, char *hoveraction)
{
    if(!lastmenu) return;
    char *t = newstring(text);
    lastmenu->items.add(new mitemtext(lastmenu, t, action[0] ? newstring(action) : t, hoveraction[0] ? newstring(hoveraction) : NULL, NULL));
}

void menuitemtextinput(char *text, char *value, char *action, char *hoveraction)
{
    if(!lastmenu || !text || !value) return;
    lastmenu->items.add(new mitemtextinput(lastmenu, newstring(text), newstring(value), action[0] ? newstring(action) : NULL, hoveraction[0] ? newstring(hoveraction) : NULL, NULL));
}

void menuitemslider(char *text, char *min_, char *max_, char *value, char *step, char *display, char *action)
{
    if(!lastmenu) return;
    lastmenu->items.add(new mitemslider(lastmenu, newstring(text), atoi(min_), atoi(max_), atoi(step), value[0] ? newstring(value) : NULL, display[0] ? newstring(display) : NULL, action[0] ? newstring(action) : NULL, NULL));
}

void menuitemkeyinput(char *text, char *bindcmd)
{
    if(!lastmenu) return;
    lastmenu->items.add(new mitemkeyinput(lastmenu, newstring(text), newstring(bindcmd), NULL));
}

void menuitemcheckbox(char *text, char *value, char *action)
{
    if(!lastmenu) return;
    lastmenu->items.add(new mitemcheckbox(lastmenu, newstring(text), newstring(value), newstring(action), NULL));
}

void menumdl(char *mdl, char *anim, char *rotspeed, char *scale)
{
    if(!lastmenu || !mdl || !anim) return;
    gmenu &menu = *lastmenu;
    menu.mdl = newstring(mdl);
    menu.anim = findanim(anim)|ANIM_LOOP;
    menu.rotspeed = max(0, min(atoi(rotspeed), 100));
    menu.scale = max(0, min(atoi(scale), 100));
}

void menudirlist(char *dir, char *ext, char *action)
{
    if(!lastmenu) return;
    gmenu *menu = lastmenu;
    if(menu->dirlist) delete menu->dirlist;
    mdirlist *d = menu->dirlist = new mdirlist();
    d->dir = newstring(dir);
    d->ext = ext[0] ? newstring(ext): NULL;
    d->action = action[0] ? newstring(action) : NULL;
}

void chmenumdl(char *menu, char *mdl, char *anim, char *rotspeed, char *scale)
{
    if(!menu || !mdl || !menus.access(menu)) return;
    gmenu &m = menus[menu];
    DELETEA(m.mdl);
    m.mdl = newstring(mdl);
    m.anim = findanim(anim)|ANIM_LOOP;
    m.rotspeed = max(0, min(atoi(rotspeed), 100));
    m.scale = max(0, min(atoi(scale), 100));
}
    
COMMAND(menuitem, ARG_3STR);
COMMAND(menuitemtextinput, ARG_4STR);
COMMAND(menuitemslider, ARG_7STR);
COMMAND(menuitemkeyinput, ARG_4STR);
COMMAND(menuitemcheckbox, ARG_3STR);
COMMAND(showmenu, ARG_1STR);
COMMAND(newmenu, ARG_1STR);
COMMAND(menumdl, ARG_5STR);
COMMAND(menudirlist, ARG_3STR);
COMMAND(chmenumdl, ARG_6STR);

bool menukey(int code, bool isdown, int unicode)
{   
    if(!curmenu) return false;
    int n = curmenu->items.length(), menusel = curmenu->menusel;
    if(isdown)
    {
        int pagesize = MAXMENU - (curmenu->header ? 2 : 0) - (curmenu->footer ? 2 : 0);

        switch(code)
        {
            case SDLK_PAGEUP: menusel -= pagesize; break;
            case SDLK_PAGEDOWN:
                if(menusel+pagesize>=n && menusel/pagesize!=(n-1)/pagesize) menusel = n-1;
                else menusel += pagesize;
                break;
            case SDLK_ESCAPE:
            case -3:
                menuset(menustack.empty() ? NULL : menustack.pop());
                return true;
                break;
            case SDLK_UP:
            case -4: 
                menusel--;
                break;
            case SDLK_DOWN:
            case -5: 
                menusel++;
                break;
            default:
            {
                if(!curmenu->allowinput || !curmenu->items.inrange(menusel)) return false;
                mitem &m = *curmenu->items[menusel];
                m.key(code, isdown, unicode);
                return true;
            }
        }

        menuselect(curmenu, menusel);
        return true;
    }
    else
    {
        if(!curmenu->allowinput || !curmenu->items.inrange(menusel)) return false;
        mitem &m = *curmenu->items[menusel];
        if(code==SDLK_RETURN || code==SDLK_SPACE || code==-1 || code==-2)
        {
            if(!curmenu->items.inrange(menusel)) { menuset(NULL); return true; }
            m.select();
            return true;
        }
        return false;
    }
}

void rendermenumdl()
{
    if(!curmenu) { menustack.setsize(0); return; }
    gmenu &m = *curmenu;
    if(!m.mdl) return;
   
    glPushMatrix ();
   
    glLoadIdentity();
    glRotatef(90+180, 0, -1, 0);
    glRotatef(90, -1, 0, 0);
    glScalef(1, -1, 1);

	bool isplayermodel = !strncmp(m.mdl, "playermodels", strlen("playermodels"));

    vec pos;
    if(isplayermodel) pos = vec(2.0f, 1.2f, -0.4f);
    else pos = vec(2.0f, 0, 1.7f);

    float yaw = 1.0f;
    if(m.rotspeed) yaw += lastmillis/5.0f/100.0f*m.rotspeed;

    int tex = 0;
    if(isplayermodel)
    {
        s_sprintfd(skin)("packages/models/%s.jpg", m.mdl);
        tex = -(int)textureload(skin)->id;
    }
	rendermodel(isplayermodel ? (char *)"playermodels" : m.mdl, m.anim, tex, 0, pos, yaw, 0, 0, 0, NULL, isplayermodel ? (char*)"weapons/subgun/world" : NULL, m.scale ? m.scale/25.0f : 1.0f);
	
    glPopMatrix();
}

void gmenu::refresh()
{
    if(!refreshfunc) return;
    (*refreshfunc)(this, !inited);
    inited = true;
    if(menusel>=items.length()) menusel = max(items.length()-1, 0);
}

void gmenu::open()
{
    inited = false;
    if(allowinput) player1->stopmoving();
    else menusel = 0;
    if(items.inrange(menusel)) items[menusel]->focus(true);
    init();
}

void gmenu::close()
{
    if(items.inrange(menusel)) items[menusel]->focus(false);
}

void gmenu::init() 
{ 
    if(dirlist)
    {
        items.setsize(0);
        cvector files;
        listdir(dirlist->dir, dirlist->ext, files);
        loopv(files) 
        {
            char *f = files[i];
            if(!f || !f[0]) continue;
            items.add(new mitemtext(this, f, dirlist->action, NULL, NULL));
        }
    }
    loopv(items) items[i]->init(); 
}

void gmenu::render()
{
    const char *t = title;
    if(!t) { static string buf; s_sprintf(buf)("[ %s menu ]", name); t = buf; }
    int hitems = (header ? 2 : 0) + (footer ? 2 : 0),
        pagesize = MAXMENU - hitems,
        offset = menusel - (menusel%pagesize), 
        mdisp = min(items.length(), pagesize), 
        cdisp = min(items.length()-offset, pagesize);
    mitem::whitepulse.alpha = (sinf(lastmillis/200.0f)+1.0f)/2.0f;
    if(title) text_startcolumns();
    int w = 0;
    loopv(items)
    {
        int x = items[i]->width();
        if(x>w) w = x;
    }
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
    int x = (2*VIRTW-w)/2;
    renderbg(x-FONTH*3/2, y-FONTH, x+w+FONTH*3/2, y+h+FONTH, true);
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
    if(footer) 
    {
        y += ((mdisp-cdisp)+1)*step;
        draw_text(footer, x, y);
    }
}

void gmenu::renderbg(int x1, int y1, int x2, int y2, bool border)
{
    static Texture *tex = NULL;
    if(!tex) tex = textureload("packages/textures/makke/menu.jpg");
    blendbox(x1, y1, x2, y2, border, tex->id);
}

