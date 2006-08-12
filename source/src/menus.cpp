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
    int tex; // (optional) bg tex
    char *mdl; // (optional) md2 mdl
    int frame, range, rotspeed, scale;
};

vector<gmenu> menus;

int vmenu = -1;

ivector menustack;

void menuset(int menu) // EDIT: AH
{
    if((vmenu = menu)>=1 && menu != 2) resetmovement(player1);
    if(vmenu==1) menus[1].menusel = 0;
};

void showmenu(char *name)
{
    loopv(menus) if(i>1 && strcmp(menus[i].name, name)==0)
    {
        menuset(i);
        return;
    };
};

int menucompare(mitem *a, mitem *b)
{
    int x = atoi(a->text);
    int y = atoi(b->text);
    if(x>y) return -1;
    if(x<y) return 1;
    return 0;
};

void sortmenu(int m, int start, int num)
{
    qsort(&menus[m].items[start], num, sizeof(mitem), (int (__cdecl *)(const void *,const void *))menucompare);
};

void refreshservers();

bool rendermenu()
{
    if(vmenu<0) { menustack.setsize(0); return false; };
    
    if(scoped) togglescope();
    
    if(vmenu==1) refreshservers();
    gmenu &m = menus[vmenu];
    sprintf_sd(title)(vmenu>0 && vmenu!=2 ? "[ %s menu ]" : "%s", m.name); // EDIT: AH
    int mdisp = m.items.length();
    int w = 0;
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
    blendbox(x-FONTH/2*3, y-FONTH, x+w+FONTH/2*3, y+h+FONTH, true, m.tex >= 0 ? FIRSTMENU + m.tex : -1);
    draw_text(title, x, y,2);
    y += FONTH*2;
    if(vmenu && vmenu!=2)
    {
        int bh = y+m.menusel*step;
        blendbox(x-FONTH, bh-10, x+w+FONTH, bh+FONTH+10, false);
    };
    loopj(mdisp)
    {
        draw_text(m.items[j].text, x, y, 2);
        y += step;
    };
    return true;
};

void rendermenumdl()
{
    if(vmenu<0) { menustack.setsize(0); return; };
    if(vmenu==1) refreshservers();
    gmenu &m = menus[vmenu];
    if(!m.mdl) return;
    
    glPushMatrix ();
   
    glTranslatef(player1->o.x, player1->o.z, player1->o.y);
    glRotatef(player1->yaw+90+180, 0, -1, 0);
    glRotatef(player1->pitch, 0, 0, 1);
   
	float z=0.0f;

    if(!strncmp(m.mdl, "playermodels", strlen("playermodels"))) 
	{
		glTranslatef(2.0f, 1.0f, 1.5f);
		z = -1.0f;
	}
    else 
	{
		glTranslatef(2.0f, 1.7f, 0.0f);
	}

    if(m.rotspeed) glRotatef(lastmillis/5.0f/100.0f*m.rotspeed, 0, 1, 0);

    rendermodel(m.mdl, m.frame, m.range, 0.0f, 0.0f, 0.0f, z, 0, 0, 0, false, (float)(m.scale ? m.scale/25.0f : 1), 100, 0, 0, false);
    glPopMatrix();
}

void newmenu(char *name)
{
    gmenu &menu = menus.add();
    menu.name = newstring(name);
    menu.menusel = 0;
    menu.tex = -1;
    menu.mdl = NULL;
};

void menumanual(int m, int n, char *text)
{
    if(!n) menus[m].items.setsize(0);
    mitem &mitem = menus[m].items.add();
    mitem.text = text;
    mitem.action = "";
}

void menuitem(char *text, char *action, char *hoveraction)
{
    gmenu &menu = menus.last();
    mitem &mi = menu.items.add();
    mi.text = newstring(text);
    mi.action = action[0] ? newstring(action) : mi.text;
    if(hoveraction) mi.hoveraction = newstring(hoveraction);
    else mi.hoveraction = NULL;
};

void menubg(char *tex)
{
    gmenu &menu = menus.last();
    int curmtex = 0;
    loopv(menus) curmtex = max(curmtex, menus[i].tex);
    if(curmtex>=100) return;
    sprintf_sd(path)("packages/%s", tex);
    int xs, ys;
    if(installtex(FIRSTMENU + curmtex, path, xs, ys, false, true))
        menu.tex = curmtex;
};

void menumdl(char *mdl, char *frame, char *range, char *rotspeed, char *scale)
{
    if(!mdl || !frame || !range) return;
    gmenu &menu = menus.last();
    menu.mdl = newstring(mdl, _MAXDEFSTR);
    menu.frame = max(0, atoi(frame));
    menu.range = max(1, atoi(range));
    menu.rotspeed = max(0, min(atoi(rotspeed), 100));
    menu.scale = max(0, min(atoi(scale), 100));
};

void chmenumdl(char *menu, char *mdl, char *frame, char *range, char *rotspeed, char *scale)
{
    if(!menu || !mdl) return;
    loopv(menus)
    {
        gmenu &m = menus[i];
        if(strcmp(m.name, menu) == 0)
        {
            if(m.mdl) strcpy_s(m.mdl, mdl);
            else m.mdl = newstring(mdl, _MAXDEFSTR);
            m.frame = atoi(frame);
            m.range = atoi(range);
            m.rotspeed = max(0, min(atoi(rotspeed), 100));
            m.scale = max(0, min(atoi(scale), 100));
            return;
        };
    };
};
    

COMMAND(menuitem, ARG_3STR);
COMMAND(showmenu, ARG_1STR);
COMMAND(newmenu, ARG_1STR);
COMMAND(menubg, ARG_1STR);
COMMAND(menumdl, ARG_5STR);
COMMAND(chmenumdl, ARG_6STR);

bool menukey(int code, bool isdown)
{   
    if(vmenu<=0 || vmenu == 2) return false; // EDIT: AH
    int menusel = menus[vmenu].menusel;
    
    if(isdown)
    {
		if(scoped) togglescope();
        int oldmenusel = menusel;
        if(code==SDLK_ESCAPE)
        {
            menuset(-1);
            if(!menustack.empty()) menuset(menustack.pop());
            return true;
        }
        else if(code==SDLK_UP || code==-4) menusel--;
        else if(code==SDLK_DOWN || code==-5) menusel++;
        int n = menus[vmenu].items.length();
        if(menusel<0) menusel = n-1;
        else if(menusel>=n) menusel = 0;
        menus[vmenu].menusel = menusel;
        if(menusel != oldmenusel)execute(menus[vmenu].items[menusel].hoveraction, true);
    }
    else
    {
        if(code==SDLK_RETURN || code==-2)
        {
            char *action = menus[vmenu].items[menusel].action;
            if(vmenu==1) connects(getservername(menusel));
            menustack.add(vmenu);
            menuset(-1);
            execute(action, true);
        };
    };
    return true;
};
