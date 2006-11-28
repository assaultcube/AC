// menus.cpp: ingame menu system (also used for scores and serverlist)

#include "cube.h"

#define FIRSTMENU 120

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
};

void showmenu(char *name)
{
    if(!name)
    {
        curmenu = NULL;
        return;
    };
    gmenu *m = menus.access(name);
    if(!m) return;
    menuset(m);
};

int menucompare(mitem *a, mitem *b)
{
    int x = atoi(a->text);
    int y = atoi(b->text);
    if(x>y) return -1;
    if(x<y) return 1;
    return 0;
};

bool rendermenu()
{
    if(!curmenu) { menustack.setsize(0); return false; };
    
    setscope(false);
    
    gmenu &m = *curmenu;
    if(m.refreshfunc) (*m.refreshfunc)();
    s_sprintfd(title)(m.hastitle ? "[ %s menu ]" : "%s", m.name); // EDIT: AH
    int mdisp = m.items.length();
    int w = 0;
    if(!m.hastitle) text_startcolumns();
    loopi(mdisp)
    {
        int x = text_width(m.items[i].text);
        if(x>w) w = x;
    };
    int tw = text_width(title);
    if(tw>w) w = tw;
    int step = FONTH/4*5;
    int h = (mdisp+2)*step;
    int y = (VIRTH-h)/2;
    int x = (VIRTW-w)/2;
    static Texture *menutex = NULL;
    if(!menutex) menutex = textureload("packages/textures/makke/menu.jpg", false);
    blendbox(x-FONTH/2*3, y-FONTH, x+w+FONTH/2*3, y+h+FONTH, true, menutex->id);
    draw_text(title, x, y);
    y += FONTH*2;
    if(m.allowinput)
    {
        int bh = y+m.menusel*step;
        blendbox(x-FONTH, bh-10, x+w+FONTH, bh+FONTH+10, false);
    };
    loopj(mdisp)
    {
        draw_text(m.items[j].text, x, y);
        y += step;
    };
    if(!m.hastitle) text_endcolumns();
    return true;
};

void rendermenumdl()
{
    if(!curmenu) { menustack.setsize(0); return; };
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

	rendermodel(m.mdl, m.anim, 0, 0, pos.x, pos.z, pos.y, yaw, 0, 100, 0, NULL, isplayermodel ? (char*)"weapons/subgun/world" : NULL, m.scale ? m.scale/25.0f : 1.0f);
	
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
};

void newmenu(char *name)
{
    addmenu(name);
};

void menumanual(void *menu, int n, char *text, char *action)
{
    gmenu &m = *(gmenu *)menu;
    if(!n) m.items.setsize(0);
    mitem &mitem = m.items.add();
    mitem.text = text;
	mitem.action = action;
	mitem.hoveraction = NULL;
}

void sortmenu(void *menu, int start, int num)
{
    gmenu &m = *(gmenu *)menu;
    m.items.sort(menucompare, start, num);
};

void menuitem(char *text, char *action, char *hoveraction)
{
    if(!lastmenu) return;
    gmenu &menu = *lastmenu;
    mitem &mi = menu.items.add();
    mi.text = newstring(text);
    mi.action = action[0] ? newstring(action) : mi.text;
	mi.hoveraction = hoveraction[0] ? newstring(hoveraction) : NULL;
};

void menumdl(char *mdl, char *anim, char *rotspeed, char *scale)
{
    if(!lastmenu || !mdl || !anim) return;
    gmenu &menu = *lastmenu;
    menu.mdl = newstringbuf(mdl);
    menu.anim = findanim(anim)|ANIM_LOOP;
    menu.rotspeed = max(0, min(atoi(rotspeed), 100));
    menu.scale = max(0, min(atoi(scale), 100));
};

void chmenumdl(char *menu, char *mdl, char *anim, char *rotspeed, char *scale)
{
    if(!menu || !mdl || !menus.access(menu)) return;
    gmenu &m = menus[menu];
    if(m.mdl) s_strcpy(m.mdl, mdl);
    else m.mdl = newstringbuf(mdl);
    m.anim = findanim(anim)|ANIM_LOOP;
    m.rotspeed = max(0, min(atoi(rotspeed), 100));
    m.scale = max(0, min(atoi(scale), 100));
};
    

COMMAND(menuitem, ARG_3STR);
COMMAND(showmenu, ARG_1STR);
COMMAND(newmenu, ARG_1STR);
COMMAND(menumdl, ARG_5STR);
COMMAND(chmenumdl, ARG_6STR);

bool menukey(int code, bool isdown)
{   
    if(!curmenu || !curmenu->allowinput) return false;
    int menusel = curmenu->menusel;
    
    if(isdown)
    {
		setscope(false);
        int oldmenusel = menusel;
        if(code==SDLK_ESCAPE || code==-3)
        {
            menuset(menustack.empty() ? NULL : menustack.pop());
            return true;
        }
        else if(code==SDLK_UP || code==-4) menusel--;
        else if(code==SDLK_DOWN || code==-5) menusel++;
        int n = curmenu->items.length();
		if(menusel<0) menusel = n>0 ? n-1 : 0;
        else if(menusel>=n) menusel = 0;
        curmenu->menusel = menusel;
		char *haction = curmenu->items[menusel].hoveraction;
		if(menusel != oldmenusel && haction) execute(haction);
    }
    else
    {
        if(code==SDLK_RETURN || code==-1 || code==-2)
        {
			if(menusel<0 || menusel >= curmenu->items.length()) { menuset(NULL); return true; };
            char *action = curmenu->items[menusel].action;
            menustack.add(curmenu);
            menuset(NULL);
            if(action) execute(action);
        };
    };
    return true;
};
