// menus.cpp: ingame menu system (also used for scores and serverlist)

#include "cube.h"

struct mitem { char *text, *action, *hoveraction; };

struct gmenu
{
    char *name, *title;
    vector<mitem> items;
    int mwidth;
    int menusel;
    char *mdl; // (optional) md2 mdl
    int anim, rotspeed, scale;
    bool allowinput, inited;
    void (__cdecl *refreshfunc)(void *, bool);
};

hashtable<char *, gmenu> menus;
gmenu *curmenu = NULL, *lastmenu = NULL;

vector<gmenu *> menustack;

void menuset(void *m)
{
    if(curmenu==m) return;
    curmenu = (gmenu *)m;
    if(curmenu)
    {
        curmenu->inited = false;
        if(curmenu->allowinput) player1->stopmoving();
        else curmenu->menusel = 0;
    }
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

void drawmenubg(int x1, int y1, int x2, int y2, bool border)
{
    static Texture *tex = NULL;
    if(!tex) tex = textureload("packages/textures/makke/menu.jpg");
    blendbox(x1, y1, x2, y2, border, tex->id);
};

#define MAXMENU 17

bool rendermenu()
{
    if(!curmenu) { menustack.setsize(0); return false; }
    
    setscope(false);
    
    gmenu &m = *curmenu;
    if(m.refreshfunc) 
    {
        (*m.refreshfunc)(curmenu, !m.inited);
        m.inited = true;
        if(m.menusel>=m.items.length()) m.menusel = max(m.items.length()-1, 0);
    }
    char *title = m.title;
    if(!title) { static string buf; s_sprintf(buf)("[ %s menu ]", m.name); title = buf; }
    int offset = m.menusel - (m.menusel%MAXMENU), mdisp = min(m.items.length(), MAXMENU), cdisp = min(m.items.length()-offset, MAXMENU);
    if(m.title) text_startcolumns();
    int w = 0;
    loopv(m.items)
    {
        int x = text_width(m.items[i].text);
        if(x>w) w = x;
    }
    int tw = text_width(title);
    if(tw>w) w = tw;
    int step = (FONTH*5)/4;
    int h = (mdisp+2)*step;
    int y = (VIRTH-h)/2;
    int x = (VIRTW-w)/2;
    drawmenubg(x-FONTH*3/2, y-FONTH, x+w+FONTH*3/2, y+h+FONTH, true);
    if(offset>0)                        drawarrow(1, x+w+FONTH*3/2-FONTH*5/6, y-FONTH*5/6, FONTH*2/3);
    if(offset+MAXMENU<m.items.length()) drawarrow(0, x+w+FONTH*3/2-FONTH*5/6, y+h+FONTH/6, FONTH*2/3);
    draw_text(title, x, y);
    y += step*2;
    if(m.allowinput)
    {
        int bh = y+(m.menusel%MAXMENU)*step;
        blendbox(x-FONTH, bh-FONTH/6, x+w+FONTH, bh+FONTH+FONTH/6, false);
    }
    loopj(cdisp)
    {
        draw_text(m.items[offset+j].text, x, y);
        y += step;
    }
    if(m.title) text_endcolumns();
    return true;
}

void rendermenumdl()
{
    if(!curmenu) { menustack.setsize(0); return; }
    gmenu &m = *curmenu;
    if(!m.mdl) return;
   
    glPushMatrix ();
   
    float t = (lastmillis-player1->lastaction)/1000.0f;
    if(t >= 1.6f) t = 1.6f;

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
	rendermodel(isplayermodel ? (char *)"playermodels" : m.mdl, m.anim, tex, 0, pos.x, pos.y, pos.z, yaw, 0, 0, 0, NULL, isplayermodel ? (char*)"weapons/subgun/world" : NULL, m.scale ? m.scale/25.0f : 1.0f);
	
    glPopMatrix();
}

void *addmenu(char *name, char *title, bool allowinput, void (__cdecl *refreshfunc)(void *, bool))
{
    name = newstring(name);
    gmenu &menu = menus[name];
    menu.name = name;
    menu.title = title ? newstring(title) : NULL;
    menu.menusel = 0;
    menu.mdl = NULL;
    menu.allowinput = allowinput;
    menu.inited = false;
    menu.refreshfunc = refreshfunc;
    lastmenu = &menu;
    return &menu;
}

void newmenu(char *name)
{
    addmenu(name);
}

void menumanual(void *menu, int n, char *text, char *action)
{
    gmenu &m = *(gmenu *)menu;
    if(!n) m.items.setsize(0);
    mitem &mitem = m.items.add();
    mitem.text = text;
	mitem.action = action;
	mitem.hoveraction = NULL;
}

void menuitem(char *text, char *action, char *hoveraction)
{
    if(!lastmenu) return;
    gmenu &menu = *lastmenu;
    mitem &mi = menu.items.add();
    mi.text = newstring(text);
    mi.action = action[0] ? newstring(action) : mi.text;
	mi.hoveraction = hoveraction[0] ? newstring(hoveraction) : NULL;
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
COMMAND(showmenu, ARG_1STR);
COMMAND(newmenu, ARG_1STR);
COMMAND(menumdl, ARG_5STR);
COMMAND(chmenumdl, ARG_6STR);

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
            char *haction = m.items[sel].hoveraction;
            if(sel!=oldsel && haction) execute(haction);
        }
    }
}

bool menukey(int code, bool isdown)
{   
    if(!curmenu) return false;
    int n = curmenu->items.length(), menusel = curmenu->menusel;
    if(isdown)
    {
        if(code==SDLK_PAGEUP) menusel -= MAXMENU;
        else if(code==SDLK_PAGEDOWN)
        {
            if(menusel+MAXMENU>=n && menusel/MAXMENU!=(n-1)/MAXMENU) menusel = n-1;
            else menusel += MAXMENU;
        }
        else if(!curmenu->allowinput) return false;

		setscope(false);

        if(code==SDLK_ESCAPE || code==-3)
        {
            menuset(menustack.empty() ? NULL : menustack.pop());
            return true;
        }
        else if(code==SDLK_UP || code==-4) menusel--;
        else if(code==SDLK_DOWN || code==-5) menusel++;

        menuselect(curmenu, menusel);
    }
    else
    {
        if(!curmenu->allowinput) return false;
        if(code==SDLK_RETURN || code==-1 || code==-2)
        {
			if(!curmenu->items.inrange(menusel)) { menuset(NULL); return true; }
            char *action = curmenu->items[menusel].action;
            menustack.add(curmenu);
            menuset(NULL);
            if(action) execute(action);
        }
    }
    return true;
}
