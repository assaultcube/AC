// menus.cpp: ingame menu system (also used for scores and serverlist)

#include "cube.h"

struct mitem { char *text, *action, *hoveraction; };

struct gmenu
{
    char *name;
    vector<mitem> items;
    int mwidth;
    int menusel;
    char *mdl; // (optional) md2 mdl
    int anim, rotspeed, scale;
    bool allowinput, hastitle;
    void (__cdecl *refreshfunc)();
};

hashtable<char *, gmenu> menus;
gmenu *curmenu = NULL, *lastmenu = NULL;

vector<gmenu *> menustack;

void menuset(void *m)
{
    curmenu = (gmenu *)m;
    if(!curmenu) return;
    if(curmenu->allowinput) player1->stopmoving();
    else curmenu->menusel = 0;
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

#define MAXMENU 17

bool rendermenu()
{
    if(!curmenu) { menustack.setsize(0); return false; }
    
    setscope(false);
    
    gmenu &m = *curmenu;
    if(m.refreshfunc) (*m.refreshfunc)();
    s_sprintfd(title)(m.hastitle ? "[ %s menu ]" : "%s", m.name);
    int offset = m.menusel - (m.menusel%MAXMENU), mdisp = min(m.items.length(), MAXMENU), cdisp = min(m.items.length()-offset, MAXMENU);
    if(!m.hastitle) text_startcolumns();
    int w = 0;
    loopv(m.items)
    {
        int x = text_width(m.items[i].text);
        if(x>w) w = x;
    }
    int tw = text_width(title);
    if(tw>w) w = tw;
    int step = FONTH/4*5;
    int h = (mdisp+2)*step;
    int y = (VIRTH-h)/2;
    int x = (VIRTW-w)/2;
    static Texture *menutex = NULL;
    if(!menutex) menutex = textureload("packages/textures/makke/menu.jpg");
    blendbox(x-FONTH*3/2, y-FONTH, x+w+FONTH*3/2, y+h+FONTH, true, menutex->id);
    draw_text(title, x, y);
    if(offset>0)                        drawarrow(1, x+w+FONTH*3/2-FONTH*5/6, y-FONTH*5/6, FONTH*2/3);
    if(offset+MAXMENU<m.items.length()) drawarrow(0, x+w+FONTH*3/2-FONTH*5/6, y+h+FONTH/6, FONTH*2/3);
    y += FONTH*2;
    if(m.allowinput)
    {
        int bh = y+(m.menusel%MAXMENU)*step;
        blendbox(x-FONTH, bh-10, x+w+FONTH, bh+FONTH+10, false);
    }
    loopj(cdisp)
    {
        draw_text(m.items[offset+j].text, x, y);
        y += step;
    }
    if(!m.hastitle) text_endcolumns();
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

    glTranslatef(camera1->o.x, camera1->o.z, camera1->o.y);
    glRotatef(camera1->yaw+90+180, 0, -1, 0);
	glRotatef(camera1->pitch, 0, 0, 1);
   
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
        tex = -textureload(skin)->id;
    }
	rendermodel(isplayermodel ? (char *)"playermodels" : m.mdl, m.anim, tex, 0, pos.x, pos.z, pos.y, yaw, 0, 100, 0, NULL, isplayermodel ? (char*)"weapons/subgun/world" : NULL, m.scale ? m.scale/25.0f : 1.0f);
	
    glPopMatrix();
}

void *addmenu(char *name, bool allowinput, bool hastitle, void (__cdecl *refreshfunc)())
{
    name = newstring(name);
    gmenu &menu = menus[name];
    menu.name = name;
    menu.menusel = 0;
    menu.mdl = NULL;
    menu.allowinput = allowinput;
    menu.hastitle = hastitle;
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
    menu.mdl = newstringbuf(mdl);
    menu.anim = findanim(anim)|ANIM_LOOP;
    menu.rotspeed = max(0, min(atoi(rotspeed), 100));
    menu.scale = max(0, min(atoi(scale), 100));
}

void chmenumdl(char *menu, char *mdl, char *anim, char *rotspeed, char *scale)
{
    if(!menu || !mdl || !menus.access(menu)) return;
    gmenu &m = menus[menu];
    if(m.mdl) s_strcpy(m.mdl, mdl);
    else m.mdl = newstringbuf(mdl);
    m.anim = findanim(anim)|ANIM_LOOP;
    m.rotspeed = max(0, min(atoi(rotspeed), 100));
    m.scale = max(0, min(atoi(scale), 100));
}
    

COMMAND(menuitem, ARG_3STR);
COMMAND(showmenu, ARG_1STR);
COMMAND(newmenu, ARG_1STR);
COMMAND(menumdl, ARG_5STR);
COMMAND(chmenumdl, ARG_6STR);

bool menukey(int code, bool isdown)
{   
    if(!curmenu) return false;
    int n = curmenu->items.length(), menusel = curmenu->menusel, oldmenusel = menusel;
    if(!curmenu->allowinput)
    {
        if(!isdown) return false;
        if(code==SDLK_PAGEUP) menusel -= MAXMENU;
        else if(code==SDLK_PAGEDOWN)
        {
            if(menusel+MAXMENU>=n && menusel/MAXMENU!=(n-1)/MAXMENU) menusel = n-1;
            else menusel += MAXMENU;
        }
        else return false;
        setscope(false);
        if(menusel<0) menusel = n>0 ? n-1 : 0;
        else if(menusel>=n) menusel = 0;
        if(curmenu->items.inrange(menusel)) curmenu->menusel = menusel;
        return true;
    } 
    else if(isdown)
    {
		setscope(false);
        if(code==SDLK_ESCAPE || code==-3)
        {
            menuset(menustack.empty() ? NULL : menustack.pop());
            return true;
        }
        else if(code==SDLK_UP || code==-4) menusel--;
        else if(code==SDLK_DOWN || code==-5) menusel++;
        else if(code==SDLK_PAGEUP) menusel -= MAXMENU;
        else if(code==SDLK_PAGEDOWN)
        {
            if(menusel+MAXMENU>=n && menusel/MAXMENU!=(n-1)/MAXMENU) menusel = n-1;
            else menusel += MAXMENU;
        }

		if(menusel<0) menusel = n>0 ? n-1 : 0;
        else if(menusel>=n) menusel = 0;
		if(curmenu->items.inrange(menusel))
		{
			curmenu->menusel = menusel;
			char *haction = curmenu->items[menusel].hoveraction;
			if(menusel!=oldmenusel && haction) execute(haction);
		}
    }
    else
    {
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
