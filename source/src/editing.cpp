// editing.cpp: most map editing commands go here, entity editing commands are in world.cpp

#include "cube.h"

bool editmode = false;

// the current selection, used by almost all editing commands
// invariant: all code assumes that these are kept inside MINBORD distance of the edge of the map

block sel =
{
    variable("selx",  0, 0, 4096, &sel.x,  NULL, false),
    variable("sely",  0, 0, 4096, &sel.y,  NULL, false),
    variable("selxs", 0, 0, 4096, &sel.xs, NULL, false),
    variable("selys", 0, 0, 4096, &sel.ys, NULL, false),
};

int selh = 0;
bool selset = false;

#define loopselxy(b) { makeundo(); loop(x,sel.xs) loop(y,sel.ys) { sqr *s = S(sel.x+x, sel.y+y); b; } remip(sel); }

int cx, cy, ch;

int curedittex[] = { -1, -1, -1 };

bool dragging = false;
int lastx, lasty, lasth;

int lasttype = 0, lasttex = 0;
sqr rtex;

VAR(editing, 1, 0, 0);

void toggleedit(bool force)
{
    if(player1->state==CS_DEAD) return;                   // do not allow dead players to edit to avoid state confusion
    if(!force && !editmode && !allowedittoggle()) return; // not in most multiplayer modes
    if(!(editmode = !editmode))
    {
        entinmap(player1);                                // find spawn closest to current floating pos
    }
    else
    {
        //put call to clear/restart gamemode
		player1->attacking = false;
    }
    keyrepeat(editmode);
    selset = false;
    editing = editmode ? 1 : 0;
    player1->state = editing ? CS_EDITING : CS_ALIVE;
    if(editing && player1->onladder) player1->onladder = false;
    if(editing && (player1->weaponsel->type == GUN_SNIPER && ((sniperrifle *)player1->weaponsel)->scoped)) ((sniperrifle *)player1->weaponsel)->onownerdies(); // or ondeselecting()
    if(!force) addmsg(SV_EDITMODE, "ri", editing);
}

void edittoggle() { toggleedit(false); }

COMMAND(edittoggle, ARG_NONE);

char *editinfo()
{
    static string info;
    if(!editmode) return NULL;
    int e = closestent();
    if(e<0) return NULL;
    entity &c = ents[e];
    formatstring(info)("closest entity = %s (%d, %d, %d, %d), selection = (%d, %d)", entnames[c.type], c.attr1, c.attr2, c.attr3, c.attr4, sel.xs, sel.ys);
    return info;
}

void correctsel()                                       // ensures above invariant
{
    selset = !OUTBORD(sel.x, sel.y);
    int bsize = ssize-MINBORD;
    if(sel.xs+sel.x>bsize) sel.xs = bsize-sel.x;
    if(sel.ys+sel.y>bsize) sel.ys = bsize-sel.y;
    if(sel.xs<=0 || sel.ys<=0) selset = false;
}

bool noteditmode(const char* func)
{
    correctsel();
    if(!editmode)
    {
        if(func && func[0]!='\0') conoutf("\f4[\f3%s\f4]\f5 is only allowed in edit mode", func);
        else conoutf("this function is only allowed in edit mode");
    }
    return !editmode;
}

bool noselection()
{
    if(!selset) conoutf("no selection");
    return !selset;
}

#define EDITSEL   if(noteditmode("EDITSEL") || noselection()) return
#define EDITSELMP if(noteditmode("EDITSELMP") || noselection() || multiplayer()) return
#define EDITMP    if(noteditmode("EDITMP") || multiplayer()) return

void selectpos(int x, int y, int xs, int ys)
{
    block s = { x, y, xs, ys };
    sel = s;
    selh = 0;
    correctsel();
}

void makesel()
{
    block s = { min(lastx,cx), min(lasty,cy), abs(lastx-cx)+1, abs(lasty-cy)+1 };
    sel = s;
    selh = max(lasth,ch);
    correctsel();
    if(selset) rtex = *S(sel.x, sel.y);
}

VAR(flrceil,0,0,2);
VAR(editaxis,0,0,13);

// VC8 optimizer screws up rendering somehow if this is an actual function
#define sheight(s,t,z) (!flrceil ? (s->type==FHF ? s->floor-t->vdelta/4.0f : (float)s->floor) : (s->type==CHF ? s->ceil+t->vdelta/4.0f : (float)s->ceil))

void cursorupdate()                                     // called every frame from hud
{
    flrceil = ((int)(camera1->pitch>=0))*2;
    int cyaw = ((int) camera1->yaw) % 180;
    editaxis = editmode ? (fabs(camera1->pitch) > 65 ? 13 : (cyaw < 45 || cyaw > 135 ? 12 : 11)) : 0;

    volatile float x = worldpos.x;                      // volatile needed to prevent msvc7 optimizer bug?
    volatile float y = worldpos.y;
    volatile float z = worldpos.z;

    cx = (int)x;
    cy = (int)y;

    if(OUTBORD(cx, cy)) return;
    sqr *s = S(cx,cy);

    if(fabs(sheight(s,s,z)-z)>1)                        // selected wall
    {
        x += x>camera1->o.x ? 0.5f : -0.5f;             // find right wall cube
        y += y>camera1->o.y ? 0.5f : -0.5f;

        cx = (int)x;
        cy = (int)y;

        if(OUTBORD(cx, cy)) return;
    }

    if(dragging) makesel();

    const int GRIDSIZE = 5;
    const float GRIDW = 0.5f;
    const float GRID8 = 2.0f;
    const float GRIDS = 2.0f;
    const int GRIDM = 0x7;

    // render editing grid

    for(int ix = cx-GRIDSIZE; ix<=cx+GRIDSIZE; ix++) for(int iy = cy-GRIDSIZE; iy<=cy+GRIDSIZE; iy++)
    {

        if(OUTBORD(ix, iy)) continue;
        sqr *s = S(ix,iy);
        if(SOLID(s)) continue;
        float h1 = sheight(s, s, z);
        float h2 = sheight(s, SWS(s,1,0,sfactor), z);
        float h3 = sheight(s, SWS(s,1,1,sfactor), z);
        float h4 = sheight(s, SWS(s,0,1,sfactor), z);
        if(s->tag) linestyle(GRIDW, 0xFF, 0x40, 0x40);
        else if(s->type==FHF || s->type==CHF) linestyle(GRIDW, 0x80, 0xFF, 0x80);
        else linestyle(GRIDW, 0x80, 0x80, 0x80);
        block b = { ix, iy, 1, 1 };
        box(b, h1, h2, h3, h4);
        linestyle(GRID8, 0x40, 0x40, 0xFF);
        if(!(ix&GRIDM))   line(ix,   iy,   h1, ix,   iy+1, h4);
        if(!((ix+1)&GRIDM)) line(ix+1, iy,   h2, ix+1, iy+1, h3);
        if(!(iy&GRIDM))   line(ix,   iy,   h1, ix+1, iy,   h2);
        if(!((iy+1)&GRIDM)) line(ix,   iy+1, h4, ix+1, iy+1, h3);
    }

    if(!SOLID(s))
    {
        float ih = sheight(s, s, z);
        linestyle(GRIDS, 0xFF, 0xFF, 0xFF);
        block b = { cx, cy, 1, 1 };
        box(b, ih, sheight(s, SWS(s,1,0,sfactor), z), sheight(s, SWS(s,1,1,sfactor), z), sheight(s, SWS(s,0,1,sfactor), z));
        linestyle(GRIDS, 0xFF, 0x00, 0x00);
        dot(cx, cy, ih);
        ch = (int)ih;
    }

    if(selset)
    {
        linestyle(GRIDS, 0xFF, 0x40, 0x40);
        box(sel, (float)selh, (float)selh, (float)selh, (float)selh);
    }

    glLineWidth(1);
}

vector<block *> undos;                                  // unlimited undo
VAR(undomegs, 0, 1, 10);                                // bounded by n megs

void pruneundos(int maxremain)                          // bound memory
{
    int t = 0;
    loopvrev(undos)
    {
        t += undos[i]->xs*undos[i]->ys*sizeof(sqr);
        if(t>maxremain) delete[] (uchar *)undos.remove(i);
    }
}

void makeundo()
{
    undos.add(blockcopy(sel));
    pruneundos(undomegs<<20);
}

void editundo()
{
    EDITMP;
    if(undos.empty()) { conoutf("nothing more to undo"); return; }
    block *p = undos.pop();
    blockpaste(*p);
    freeblock(p);
}

block *copybuf = NULL;

void copy()
{
    EDITSELMP;
    freeblock(copybuf);
    copybuf = blockcopy(sel);
}

void paste()
{
    EDITMP;
    if(!copybuf) { conoutf("nothing to paste"); return; }
    sel.xs = copybuf->xs;
    sel.ys = copybuf->ys;
    correctsel();
    if(!selset || sel.xs!=copybuf->xs || sel.ys!=copybuf->ys) { conoutf("incorrect selection"); return; }
    makeundo();
    copybuf->x = sel.x;
    copybuf->y = sel.y;
    blockpaste(*copybuf);
}

// Count the walls of type "type" contained in the current selection
int countwalls(int type)
{
	int counter = 0;
	EDITSELMP 0;
	if(type < 0 || type >= MAXTYPE)
	{
		conoutf("invalid type");
		return 0;
	}
	loopselxy(if(s->type==type) counter++)
	return counter;
}
 
void tofronttex()                                       // maintain most recently used of the texture lists when applying texture
{
    loopi(3)
    {
        int c = curedittex[i];
        if(c>=0)
        {
            uchar *p = hdr.texlists[i];
            int t = p[c];
            for(int a = c-1; a>=0; a--) p[a+1] = p[a];
            p[0] = t;
            curedittex[i] = -1;
        }
    }
}

void editdrag(bool isdown)
{
    if((dragging = isdown))
    {
        lastx = cx;
        lasty = cy;
        lasth = ch;
        selset = false;
        tofronttex();
    }
    makesel();
}

// the core editing function. all the *xy functions perform the core operations
// and are also called directly from the network, the function below it is strictly
// triggered locally. They all have very similar structure.

void editheightxy(bool isfloor, int amount, block &sel)
{
    loopselxy(if(isfloor)
    {
        s->floor += amount;
        if(s->floor>=s->ceil) s->floor = s->ceil-1;
    }
    else
    {
        s->ceil += amount;
        if(s->ceil<=s->floor) s->ceil = s->floor+1;
    });
}

void editheight(int flr, int amount)
{
    EDITSEL;
    bool isfloor = flr==0;
    editheightxy(isfloor, amount, sel);
    addmsg(SV_EDITH, "ri6", sel.x, sel.y, sel.xs, sel.ys, isfloor, amount);
}

COMMAND(editheight, ARG_2INT);

void edittexxy(int type, int t, block &sel)
{
    loopselxy(switch(type)
    {
        case 0: s->ftex = t; break;
        case 1: s->wtex = t; break;
        case 2: s->ctex = t; break;
        case 3: s->utex = t; break;
    });
}

void edittex(int type, int dir)
{
    EDITSEL;
    if(type<0 || type>3) return;
    if(type!=lasttype) { tofronttex(); lasttype = type; }
    int atype = type==3 ? 1 : type;
    int i = curedittex[atype];
    i = i<0 ? 0 : i+dir;
    curedittex[atype] = i = min(max(i, 0), 255);
    int t = lasttex = hdr.texlists[atype][i];
    edittexxy(type, t, sel);
    addmsg(SV_EDITT, "ri6", sel.x, sel.y, sel.xs, sel.ys, type, t);
}

void replace()
{
    EDITSELMP;
    loop(x,ssize) loop(y,ssize)
    {
        sqr *s = S(x, y);
        switch(lasttype)
        {
            case 0: if(s->ftex == rtex.ftex) s->ftex = lasttex; break;
            case 1: if(s->wtex == rtex.wtex) s->wtex = lasttex; break;
            case 2: if(s->ctex == rtex.ctex) s->ctex = lasttex; break;
            case 3: if(s->utex == rtex.utex) s->utex = lasttex; break;
        }
    }
    block b = { 0, 0, ssize, ssize };
    remip(b);
}

void edittypexy(int type, block &sel)
{
    loopselxy(s->type = type);
}

void edittype(int type)
{
    EDITSEL;
    if(type==CORNER && (sel.xs!=sel.ys || sel.xs==3 || (sel.xs>4 && sel.xs!=8)
                   || sel.x&~-sel.xs || sel.y&~-sel.ys))
                   { conoutf("corner selection must be power of 2 aligned"); return; }
    edittypexy(type, sel);
    addmsg(SV_EDITS, "ri5", sel.x, sel.y, sel.xs, sel.ys, type);
}

void heightfield(int t) { edittype(t==0 ? FHF : CHF); }
void solid(int t)       { edittype(t==0 ? SPACE : SOLID); }
void corner()           { edittype(CORNER); }

COMMAND(heightfield, ARG_1INT);
COMMAND(solid, ARG_1INT);
COMMAND(corner, ARG_NONE);

void editequalisexy(bool isfloor, block &sel)
{
    int low = 127, hi = -128;
    loopselxy(
    {
        if(s->floor<low) low = s->floor;
        if(s->ceil>hi) hi = s->ceil;
    });
    loopselxy(
    {
        if(isfloor) s->floor = low; else s->ceil = hi;
        if(s->floor>=s->ceil) s->floor = s->ceil-1;
    });
}

void equalize(int flr)
{
    bool isfloor = flr==0;
    EDITSEL;
    editequalisexy(isfloor, sel);
    addmsg(SV_EDITE, "ri5", sel.x, sel.y, sel.xs, sel.ys, isfloor);
}

COMMAND(equalize, ARG_1INT);

void setvdeltaxy(int delta, block &sel)
{
    loopselxy(s->vdelta = max(s->vdelta+delta, 0));
    remipmore(sel);
}

void setvdelta(int delta)
{
    EDITSEL;
    setvdeltaxy(delta, sel);
    addmsg(SV_EDITD, "ri5", sel.x, sel.y, sel.xs, sel.ys, delta);
}

const int MAXARCHVERT = 50;
int archverts[MAXARCHVERT][MAXARCHVERT];
bool archvinit = false;

void archvertex(int span, int vert, int delta)
{
    if(!archvinit)
    {
        archvinit = true;
        loop(s,MAXARCHVERT) loop(v,MAXARCHVERT) archverts[s][v] = 0;
    }
    if(span>=MAXARCHVERT || vert>=MAXARCHVERT || span<0 || vert<0) return;
    archverts[span][vert] = delta;
}

void arch(int sidedelta, int _a)
{
    EDITSELMP;
    sel.xs++;
    sel.ys++;
    if(sel.xs>MAXARCHVERT) sel.xs = MAXARCHVERT;
    if(sel.ys>MAXARCHVERT) sel.ys = MAXARCHVERT;
    loopselxy(s->vdelta =
        sel.xs>sel.ys
            ? (archverts[sel.xs-1][x] + (y==0 || y==sel.ys-1 ? sidedelta : 0))
            : (archverts[sel.ys-1][y] + (x==0 || x==sel.xs-1 ? sidedelta : 0)));
    remipmore(sel);
}

void slope(int xd, int yd)
{
    EDITSELMP;
    int off = 0;
    if(xd<0) off -= xd*sel.xs;
    if(yd<0) off -= yd*sel.ys;
    sel.xs++;
    sel.ys++;
    loopselxy(s->vdelta = xd*x+yd*y+off);
    remipmore(sel);
}

void perlin(int scale, int seed, int psize)
{
    EDITSELMP;
    sel.xs++;
    sel.ys++;
    makeundo();
    sel.xs--;
    sel.ys--;
    perlinarea(sel, scale, seed, psize);
    sel.xs++;
    sel.ys++;
    remipmore(sel);
    sel.xs--;
    sel.ys--;
}

VARF(fullbright, 0, 0, 1,
    if(fullbright)
    {
        if(noteditmode("fullbright")) return;
        fullbrightlight();
    }
    else calclight();
);

void edittag(int tag)
{
    EDITSELMP;
    loopselxy(s->tag = tag);
}

void newent(char *what, char *a1, char *a2, char *a3, char *a4)
{
    EDITSEL;
    newentity(-1, sel.x, sel.y, (int)camera1->o.z, what, ATOI(a1), ATOI(a2), ATOI(a3), ATOI(a4));
}

void movemap(int xo, int yo, int zo) // move whole map
{
    EDITMP;
    if(!worldbordercheck(MINBORD + max(-xo, 0), MINBORD + max(xo, 0), MINBORD + max(-yo, 0), MINBORD + max(yo, 0), max(zo, 0), max(-zo, 0)))
    {
        conoutf("not enough space to move the map");
        return;
    }
    if(xo || yo)
    {
        block b = { max(-xo, 0), max(-yo, 0), ssize - abs(xo), ssize - abs(yo) }, *cp = blockcopy(b);
        cp->x = max(xo, 0);
        cp->y = max(yo, 0);
        blockpaste(*cp);
        freeblock(cp);
    }
    if(zo)
    {
        loop(x, ssize) loop(y, ssize)
        {
            S(x,y)->floor = max(-128, S(x,y)->floor + zo);
            S(x,y)->ceil = min(127, S(x,y)->ceil + zo);
        }
        hdr.waterlevel += zo;
    }
    loopv(ents)
    {
        ents[i].x += xo;
        ents[i].y += yo;
        ents[i].z += zo;
        if(OUTBORD(ents[i].x, ents[i].y)) ents[i].type = NOTUSED;
    }
    player1->o.x += xo;
    player1->o.y += yo;
    entinmap(player1);
    calclight();
    resetmap(false);
}

void selfliprotate(int dir)
{
    makeundo();
    block *org = blockcopy(sel);
    const sqr *q = (const sqr *)(org + 1);
    int x1 = sel.x, x2 = sel.x + sel.xs - 1, y1 = sel.y, y2 = sel.y + sel.ys - 1, x, y;
    switch(dir)
    {
        case 1: // 90 degree clockwise
            for(x = x2; x >= x1; x--) for(y = y1; y <= y2; y++) *S(x,y) = *q++;
            break;
        case 2: // 180 degree
            for(y = y2; y >= y1; y--) for(x = x2; x >= x1; x--) *S(x,y) = *q++;
            break;
        case 3: // 90 degree counterclockwise
            for(x = x1; x <= x2; x++) for(y = y2; y >= y1; y--) *S(x,y) = *q++;
            break;
        case 11: // flip x-axis
            for(y = y1; y <= y2; y++) for(x = x2; x >= x1; x--) *S(x,y) = *q++;
            break;
        case 12: // flip y-axis
            for(y = y2; y >= y1; y--) for(x = x1; x <= x2; x++) *S(x,y) = *q++;
            break;
    }
    remipmore(sel);
    freeblock(org);
}

void selectionrotate(int dir)
{
    EDITSELMP;
    dir %= 4;
    if(dir < 0) dir += 4;
    if(!dir) return;
    if(sel.xs != sel.ys) dir = 2;
    selfliprotate(dir);
}

void selectionflip(char *axis)
{
    EDITSELMP;
    if(!axis || !*axis) return;
    switch(toupper(*axis))
    {
        case 'X': selfliprotate(11); break;
        case 'Y': selfliprotate(12); break;
    }
}

COMMANDN(select, selectpos, ARG_4INT);
COMMAND(edittag, ARG_1INT);
COMMAND(replace, ARG_NONE);
COMMAND(archvertex, ARG_3INT);
COMMAND(arch, ARG_2INT);
COMMAND(slope, ARG_2INT);
COMMANDN(vdelta, setvdelta, ARG_1INT);
COMMANDN(undo, editundo, ARG_NONE);
COMMAND(copy, ARG_NONE);
COMMAND(paste, ARG_NONE);
COMMAND(edittex, ARG_2INT);
COMMAND(newent, ARG_5STR);
COMMAND(perlin, ARG_3INT);
COMMAND(movemap, ARG_3INT);
COMMAND(selectionrotate, ARG_1INT);
COMMAND(selectionflip, ARG_1STR);
COMMAND(countwalls, ARG_1EXP);
