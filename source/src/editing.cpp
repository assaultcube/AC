// editing.cpp: most map editing commands go here, entity editing commands are in world.cpp

#include "cube.h"

bool editmode = false;

// the current selections, used by almost all editing commands
// invariant: all code assumes that these are kept inside MINBORD distance of the edge of the map
// => selections are checked when they are made or when the world is reloaded

vector<block> sels;

#define loopselxy(sel, b) { makeundo(sel); loop(x,(sel).xs) loop(y,(sel).ys) { sqr *s = S((sel).x+x, (sel).y+y); b; } remipgenerous(sel); }
#define loopselsxy(b) { loopv(sels) loopselxy(sels[i], b); }

int cx, cy, ch;

int curedittex[] = { -1, -1, -1 };

bool dragging = false;
int lastx, lasty, lasth;

int lasttype = 0, lasttex = 0;
sqr rtex;

VAR(editing, 1, 0, 0);
VAR(editing_sp, 1, 0, 0);
VAR(unsavededits, 1, 0, 0);

VAR(editmetakeydown, 1, 0, 0);
COMMANDF(editmeta, "d", (bool on) { editmetakeydown = on ? 1 : 0; } );

void toggleedit(bool force)
{
    if(player1->state==CS_DEAD) return;                   // do not allow dead players to edit to avoid state confusion
    if(!force && !editmode && !allowedittoggle()) return; // not in most multiplayer modes
    if(player1->state == CS_SPECTATE)
    {
        if(watchingdemo)
        {
            if(player1->spectatemode != SM_FLY) spectatemode(SM_FLY); // release in a place of followed player
        }
        else return;
    }

    if(!(editmode = !editmode))
    {
        float oldz = player1->o.z;
        entinmap(player1);                                // find spawn closest to current floating pos
        player1->timeinair = player1->o.z == oldz ? 0: 300;
    }
    else
    {
        //put call to clear/restart gamemode
        player1->attacking = false;
    }
    keyrepeat(editmode);
    editing = editmode ? 1 : 0;
    editing_sp = editmode && !multiplayer(NULL) ? 1 : 0;
    player1->state = editing ? CS_EDITING : (watchingdemo ? CS_SPECTATE : CS_ALIVE);
    if(editing && player1->onladder) player1->onladder = false;
    if(editing && (player1->weaponsel->type == GUN_SNIPER && ((sniperrifle *)player1->weaponsel)->scoped)) ((sniperrifle *)player1->weaponsel)->onownerdies(); // or ondeselecting()
    if(editing && (player1->weaponsel->type == GUN_GRENADE)) ((grenades *)player1->weaponsel)->onownerdies();
    if(!force) addmsg(SV_EDITMODE, "ri", editing);
}

void edittoggle() { toggleedit(false); }

COMMAND(edittoggle, "");

bool correctsel(block &s)                                       // ensures above invariant
{
    int bsize = ssize - MINBORD;
    if(s.xs+s.x>bsize) s.xs = bsize-s.x;
    if(s.ys+s.y>bsize) s.ys = bsize-s.y;
    if(s.xs<=0 || s.ys<=0) return false;
    else return !OUTBORD(s.x, s.y);
}

bool noteditmode(const char* func)
{
    if(!editmode)
    {
        if(func && func[0]!='\0') conoutf("\f4[\f3%s\f4]\f5 is only allowed in edit mode", func);
        else conoutf("this function is only allowed in edit mode");
    }
    return !editmode;
}

inline bool selset()
{
    return (sels.length() > 0);
}

bool noselection()
{
    if(!selset()) conoutf("no selection");
    return !selset();
}

VARP(hideeditslotinfo, 0, 0, 2);

char *editinfo()
{
    static string info;
    if(!editmode) return NULL;
    info[0] = '\0';
    int e = closestent();
    if(e >= 0)
    {
        entity &c = ents[e];
        int t = c.type < MAXENTTYPES ? c.type : 0;
        formatstring(info)("%s entity: %s (%s)", pinnedclosestent ? "\fs\f3pinned\fr" : "closest", entnames[t], formatentityattributes(c, true));
        const char *unassigned = "unassigned slot", *slotinfo = unassigned;
        if(t == MAPMODEL)
        {
            mapmodelinfo *mmi = getmminfo(c.attr2);
            if(mmi) slotinfo = mmshortname(mmi->name);
        }
        else if(t == SOUND)
        {
            if(mapconfigdata.mapsoundlines.inrange(c.attr1))  slotinfo = mapconfigdata.mapsoundlines[c.attr1].name;
        }
        else slotinfo = "";
        if(*slotinfo && (hideeditslotinfo == 0 || (hideeditslotinfo == 1 && slotinfo == unassigned))) concatformatstring(info, " %s", slotinfo);
    }
    if(selset())
    {
        concatformatstring(info, "\n\tselection = (%d, %d)", (sels.last()).xs, (sels.last()).ys);
        if(sels.length() > 1) concatformatstring(info, " and %d more", sels.length() - 1);
    }
    else concatformatstring(info, "\n\tno selection");
    sqr *s;
    if(!OUTBORD(cx, cy) && (s = S(cx,cy)) && !editfocusdetails(s) && !SOLID(s) && s->tag) concatformatstring(info, ", tag 0x%02X", s->tag);
    return info;
}


// multiple sels

// add a selection to the list
void addselection(int x, int y, int xs, int ys, int h)
{
    block &s = sels.add();
    if(h == -999 && !OUTBORD(x + xs / 2, y + ys / 2)) h = S(x + xs / 2, y + ys / 2)->floor;
    s.x = x; s.y = y; s.xs = xs; s.ys = ys; s.h = h;
    if(!correctsel(s)) { sels.drop(); }
}

// reset all selections
void resetselections()
{
    sels.shrink(0);
}

COMMANDF(select, "iiii", (int *x, int *y, int *xs, int *ys) { resetselections(); addselection(*x, *y, *xs, *ys, -999); });
COMMANDF(addselection, "iiii", (int *x, int *y, int *xs, int *ys) { addselection(*x, *y, *xs, *ys, -999); });
COMMAND(resetselections, "");

// reset all invalid selections
void checkselections()
{
    loopv(sels) if(!correctsel(sels[i])) sels.remove(i);
}

// update current selection, or add a new one
void makesel(bool isnew)
{
    if(isnew || sels.length() == 0) addselection(min(lastx, cx), min(lasty, cy), iabs(lastx-cx)+1, iabs(lasty-cy)+1, max(lasth, ch));
    else
    {
        block &cursel = sels.last();
        cursel.x = min(lastx, cx); cursel.y = min(lasty, cy);
        cursel.xs = iabs(lastx-cx)+1; cursel.ys = iabs(lasty-cy)+1;
        cursel.h = max(lasth, ch);
        correctsel(cursel);
    }
    if(selset()) rtex = *S((sels.last()).x, (sels.last()).y);
}

#define SEL_ATTR(attr) { string buf = ""; loopv(sels) { concatformatstring(buf, "%d ", sels[i].attr); } result(buf); }
COMMANDF(selx, "", (void) { SEL_ATTR(x); });
COMMANDF(sely, "", (void) { SEL_ATTR(y); });
COMMANDF(selxs, "", (void) { SEL_ATTR(xs); });
COMMANDF(selys, "", (void) { SEL_ATTR(ys); });
#undef SEL_ATTR

VAR(flrceil, 2, 0, 0);
VAR(editaxis, 113, 0, 0);
VARP(showgrid, 0, 1, 1);

// VC8 optimizer screws up rendering somehow if this is an actual function
#define sheight(s,t,z) (!flrceil ? (s->type==FHF ? s->floor-t->vdelta/4.0f : (float)s->floor) : (s->type==CHF ? s->ceil+t->vdelta/4.0f : (float)s->ceil))

// remove those two after playing with the values a bit :)
VAR(tagnum, 1, 14, 100);
VAR(tagnumfull, 0, 0, 100);
VAR(taglife, 1, 30, 1000);
// and have mercy with old graphics cards...

vector<int> tagclipcubes;
bool showtagclipfocus = false;
COMMANDF(showtagclipfocus, "d", (bool on) { showtagclipfocus = on; } );
VAR(showtagclips, 0, 1, 1);
FVAR(tagcliplinewidth, 0.2, 1, 3);

inline void forceinsideborders(int &xy)
{
    if(xy < MINBORD) xy = MINBORD;
    if(xy >= ssize - MINBORD) xy = ssize - MINBORD - 1;
}

void cursorupdate()                                     // called every frame from hud
{
    ASSERT(editmode);
    flrceil = camera1->pitch >= 0 ? 2 : 0;
    editaxis = fabs(camera1->pitch) > 60 ? "\161\15\15"[flrceil] : "\160\13\14\157"[(int(camera1->yaw + 45) / 90) & 3];

    volatile float x = worldpos.x;                      // volatile needed to prevent msvc7 optimizer bug?
    volatile float y = worldpos.y;
    volatile float z = worldpos.z;

    cx = (int)x;
    cy = (int)y;

    if(OUTBORD(cx, cy))
    {
        forceinsideborders(cx);
        forceinsideborders(cy);
        return;
    }

    sqr *s = S(cx,cy);

    if(fabs(sheight(s,s,z)-z)>1)                        // selected wall
    {
        x += x>camera1->o.x ? 0.5f : -0.5f;             // find right wall cube
        y += y>camera1->o.y ? 0.5f : -0.5f;

        cx = (int)x;
        cy = (int)y;

        if(OUTBORD(cx, cy))
        {
            forceinsideborders(cx);
            forceinsideborders(cy);
            return;
        }
    }

    if(dragging) makesel(false);

    const int GRIDSIZE = 5;
    const float GRIDW = 0.5f;
    const float GRID8 = 2.0f;
    const float GRIDS = 2.0f;
    const int GRIDM = 0x7;

    static int lastsparkle = 0;
    bool sparkletime = lastmillis - lastsparkle >= 20;
    if(sparkletime) lastsparkle = lastmillis - (lastmillis%20);    // clip adding sparklies at 50 fps

    // render editing grid
    extern int blankouthud;

    if(showgrid && !blankouthud)
    {
        for(int ix = cx-GRIDSIZE; ix<=cx+GRIDSIZE; ix++) for(int iy = cy-GRIDSIZE; iy<=cy+GRIDSIZE; iy++)
        {

            if(OUTBORD(ix, iy)) continue;
            sqr *s = S(ix,iy);
            if(SOLID(s)) continue;
            float h1 = sheight(s, s, z);
            float h2 = sheight(s, SWS(s,1,0,sfactor), z);
            float h3 = sheight(s, SWS(s,1,1,sfactor), z);
            float h4 = sheight(s, SWS(s,0,1,sfactor), z);
            if(sparkletime && showtagclips && showtagclipfocus && s->tag & TAGANYCLIP)
                particle_cube(s->tag & TAGCLIP ? PART_EPICKUP : PART_EMODEL, tagnum, taglife, ix, iy);
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
    }
    else ch = (int) sheight(s, s, z);

    if(selset() && !blankouthud)
    {
        linestyle(GRIDS, 0xFF, 0x40, 0x40);
        loopv(sels) box(sels[i], (float)sels[i].h, (float)sels[i].h, (float)sels[i].h, (float)sels[i].h);
    }

    if(!showtagclipfocus && showtagclips && !blankouthud)
    {
        const int xo[] = { 0, 0, 1, 1, 0 }, yo[] = {0, 1, 1, 0, 0 };
        loopv(tagclipcubes) // all non-solid & have clips
        {
            int x = tagclipcubes[i] & 0xFFFF, y = tagclipcubes[i] >> 16;
            ASSERT(!OUTBORD(x, y));
            sqr *s = S(x,y), *o[9];
            if(s->tag & TAGCLIP) linestyle(tagcliplinewidth, 0xFF, 0xFF, 0); // yello
            else linestyle(tagcliplinewidth, 0xFF, 0, 0xFF); // magenta
            o[8] = SWS(s,-1,-1,sfactor);
            o[3] = o[8] + 1;                    // 837
            o[7] = o[3] + 1;                    // 0s2
            o[0] = o[4] = SWS(s,-1,0,sfactor);  // 516
            o[2] = o[0] + 2;
            o[5] = SWS(s,-1,1,sfactor);
            o[1] = o[5] + 1;
            o[6] = o[1] + 1;
            bool clipped[9];
            loopj(9) clipped[j] = !SOLID(o[j]) && (o[j]->tag & TAGANYCLIP) > 0;
            int h = s->floor - (s->type == FHF ? (s->vdelta + 3) / 4 : 0), c = s->ceil + (s->type == CHF ? (s->vdelta + 3) / 4 : 0);
            loopk(4) if((clipped[k] == clipped[k+1]) && !(clipped[k] && clipped[k + 5])) line(x + xo[k+1], y + yo[k+1], h, x + xo[k+1], y + yo[k+1], c);
            for( ; h < c; h++)
            {
                int k = (h + x + y) & 3;
                if(k < 2 && !clipped[k]) line(x + xo[k], y + yo[k], h + 1, x + xo[k+1], y + yo[k+1], h);
                k = (h - x - y) & 3;
                if(k > 1 && !clipped[k]) line(x + xo[k], y + yo[k], h + 1, x + xo[k+1], y + yo[k+1], h);
            }
            if(sparkletime && tagnumfull) particle_cube(s->tag & TAGCLIP ? PART_EPICKUP : PART_EMODEL, tagnumfull, 30, x, y);
        }
    }

    glLineWidth(1);
}

vector<block *> undos, redos;                           // unlimited undo
VAR(undomegs, 0, 5, 50);                                // bounded by n megs
int undolevel = 0;

void pruneundos(int maxremain)                          // bound memory
{
    int u = 0, r = 0;
    maxremain /= sizeof(sqr);
    loopvrev(undos)
    {
        u += undos[i]->xs * undos[i]->ys;
        if(u > maxremain) freeblockp(undos.remove(i));
    }
    loopvrev(redos)
    {
        r += redos[i]->xs * redos[i]->ys;
        if(r > maxremain) freeblockp(redos.remove(i));
    }
}

void storeposition(short p[])
{
    loopi(3) p[i] = player1->o.v[i] * DMF;
    p[3] = player1->yaw;
    p[4] = player1->pitch;
}

void makeundo(block &sel)
{
    storeposition(sel.p);
    undos.add(blockcopy(sel));
    pruneundos(undomegs<<20);
    unsavededits++;
    undolevel++;
}

void restoreposition(short p[])
{
    loopi(3) player1->o.v[i] = float(p[i]) / DMF;
    player1->yaw = p[3];
    player1->pitch = p[4];
    player1->resetinterp();
}

void restoreposition(block &sel)
{
    restoreposition(sel.p);
    resetselections();
    addselection(sel.x, sel.y, sel.xs, sel.ys, sel.h); // select undone area
    checkselections();
}

void gotoentity(int *n)
{
    if(noteditmode("gotoentity")) return;
    if(ents.inrange(*n) && ents[*n].type != NOTUSED)
    {
        player1->o.x = ents[*n].x;
        player1->o.y = ents[*n].y;
        player1->o.z = ents[*n].z;
        player1->yaw = ((ents[*n].attr1 / ENTSCALE10) % 360 + 360) % 360; // works for most ;)
        player1->pitch = 0;
        player1->resetinterp();
    }
}
COMMAND(gotoentity, "i");

void gotoposition(char *x, char *y, char *z, char *yaw, char *pitch)
{
    if(noteditmode("gotoposition")) return;
    if(*x) player1->o.x = atof(x);
    if(*y) player1->o.y = atof(y);
    if(*z) player1->o.z = atof(z);
    if(*yaw) player1->yaw = (atoi(yaw) % 360 + 360) % 360;
    if(*pitch) player1->pitch = atoi(pitch) % 90;
    player1->resetinterp();
    defformatstring(res)("%s %s %s %d %d", floatstr(player1->o.x), floatstr(player1->o.y), floatstr(player1->o.z), int(player1->yaw), int(player1->pitch));
    result(res);
}
COMMAND(gotoposition, "sssss");

const int MAXNETBLOCKSQR = MAXGZMSGSIZE / sizeof(sqr);

void netblockpaste(const block &b, int bx, int by, bool light)  // transmit block to be pasted at bx,by
{
    ASSERT(b.xs * b.ys <= MAXNETBLOCKSQR);
    vector<uchar> t, tmp;
    putuint(t, bx);
    putuint(t, by);
    putuint(t, b.xs);
    putuint(t, b.ys);
    putuint(t, light ? 1 : 0);
    tmp.put((uchar *)((&b)+1), b.xs * b.ys * (int)sizeof(sqr));
    putgzbuf(t, tmp);
    packetbuf p(t.length() + 10, ENET_PACKET_FLAG_RELIABLE);
    putint(p, SV_EDITBLOCK);
    p.put(t.getbuf(), t.length());
    sendpackettoserv(1, p.finalize());
}

void netblockpastexy(ucharbuf *p, int bx, int by, int bxs, int bys, int light)
{
    if(bxs < 0 || bys < 0 || bxs > ssize || bys > ssize || bxs * bys > MAXNETBLOCKSQR || OUTBORD(bx, by) || OUTBORD(bx+bxs-1, by+bys-1)) return;
    block *b = (block *)new uchar[sizeof(block) + bxs * bys * sizeof(sqr)];
    b->x = bx; b->y = by; b->xs = bxs; b->ys = bys;
    memcpy(b + 1, p->buf, p->maxlen);
    makeundo(*b);
    blockpaste(*b, bx, by, light != 0, NULL);
    freeblockp(b);
}

void editundo()
{
    EDIT("undo");
    bool mp = multiplayer(NULL);
    if(mp && undos.length() && undos.last()->xs * undos.last()->ys > MAXNETBLOCKSQR)
    {
        conoutf("\f3next undo area too big for multiplayer editing");
        return;
    }
    if(undos.empty()) { conoutf("nothing more to undo"); return; }
    block *p = undos.pop();
    undolevel--;
    redos.add(blockcopy(*p));
    if(editmetakeydown) restoreposition(*p);
    blockpaste(*p);
    if(mp) netblockpaste(*p, p->x, p->y, true);
    freeblock(p);
    unsavededits++;
}
COMMANDN(undo, editundo, "");

void gotoundolevel(char *lev)
{
    if(*lev && isdigit(*lev))
    {
        EDITMP("undolevel");
        int n = ATOI(lev);
        while(undolevel > n && undos.length() > 0) editundo();
    }
    intret(undolevel);
}
COMMANDN(undolevel, gotoundolevel, "s");

void editredo()
{
    EDIT("redo");
    bool mp = multiplayer(NULL);
    if(mp && redos.length() && redos.last()->xs * redos.last()->ys > MAXNETBLOCKSQR)
    {
        conoutf("\f3next redo area too big for multiplayer editing");
        return;
    }
    if(redos.empty()) { conoutf("nothing more to redo"); return; }
    block *p = redos.pop();
    undos.add(blockcopy(*p));
    undolevel++;
    if(editmetakeydown) restoreposition(*p);
    blockpaste(*p);
    if(mp) netblockpaste(*p, p->x, p->y, true);
    freeblock(p);
    unsavededits++;
}
COMMANDN(redo, editredo, "");

extern int worldiodebug;

void restoreeditundo(ucharbuf &q)
{
    int type, len, explen;
    while(!q.overread() && (type = getuint(q)))
    {
        int bx = getuint(q), by = getuint(q), bxs = getuint(q), bys = getuint(q);
        short pp[5];
        loopi(5) pp[i] = getint(q);
        len = getuint(q);
        if((bx | by | (bx + bxs) | (by + bys)) & ~(ssize - 1)) continue;
        explen = bxs * bys;
        block *b = (block *)new uchar[sizeof(block) + explen * sizeof(sqr)];
        b->x = bx; b->y = by; b->xs = bxs; b->ys = bys;
        loopi(5) b->p[i] = pp[i];
        ucharbuf p = q.subbuf(len);
        rldecodecubes(p, (sqr *)(b+1), explen, 6, true);
        switch(type)
        {
            case 10: undos.insert(0, b); break;
            case 20: redos.insert(0, b); break;
            default: freeblock(b); break;
        }
        #ifdef _DEBUG
        if(worldiodebug) switch(type)
        {
            case 10:
            case 20:
                clientlogf("  got %sdo x %d, y %d, xs %d, ys %d, remaining %d, overread %d", type == 10 ? "un" : "re", b->x, b->y, b->xs, b->ys, q.remaining(), int(q.overread()));
                break;
        }
        #endif
    }
    if(undos.length() || redos.length()) conoutf("restored editing history: %d undos and %d redos", undos.length(), redos.length());
}

int rlencodeundo(int type, vector<uchar> &t, block *s)
{
    putuint(t, type);
    putuint(t, s->x);
    putuint(t, s->y);
    putuint(t, s->xs);
    putuint(t, s->ys);
    loopi(5) putint(t, s->p[i]);
    vector<uchar> tmp;
    rlencodecubes(tmp, (sqr *)(s+1), s->xs * s->ys, true);
    putuint(t, tmp.length());
    t.put(tmp.getbuf(), tmp.length());
    #ifdef _DEBUG
    if(worldiodebug) clientlogf("    compressing redo/undo x %d, y %d, xs %d, ys %d, compressed length %d, cubes %d", s->x, s->y, s->xs, s->ys, tmp.length(), s->xs * s->ys);
    #endif
    return t.length();
}

int backupeditundo(vector<uchar> &buf, int undolimit, int redolimit)
{
    int numundo = 0;
    vector<uchar> tmp;
    loopvrev(undos)
    {
        tmp.setsize(0);
        undolimit -= rlencodeundo(10, tmp, undos[i]);
        if(undolimit < 0) break;
        buf.put(tmp.getbuf(), tmp.length());
        numundo++;
        #ifdef _DEBUG
        if(worldiodebug) clientlogf("  written undo x %d, y %d, xs %d, ys %d, compressed length %d", undos[i]->x, undos[i]->y, undos[i]->xs, undos[i]->ys, tmp.length());
        #endif
    }
    loopvrev(redos)
    {
        tmp.setsize(0);
        redolimit -= rlencodeundo(20, tmp, redos[i]);
        if(redolimit < 0) break;
        buf.put(tmp.getbuf(), tmp.length());
        #ifdef _DEBUG
        if(worldiodebug) clientlogf("  written redo x %d, y %d, xs %d, ys %d, compressed length %d", redos[i]->x, redos[i]->y, redos[i]->xs, redos[i]->ys, tmp.length());
        #endif
    }
    putuint(buf, 0);
    return numundo;
}

vector<block *> copybuffers;
void *copytexconfig = NULL;

void resetcopybuffers()
{
    loopv(copybuffers) freeblock(copybuffers[i]);
    copybuffers.shrink(0);
    if(copytexconfig)
    {
        texconfig_delete(copytexconfig);
        copytexconfig = NULL;
    }
}

void copy()
{
    EDITSEL("copy");
    resetcopybuffers();
    copytexconfig = texconfig_copy();

    loopv(sels)
    {
        block *b = blockcopy(sels[i]);
        copybuffers.add(b);
    }

}
COMMAND(copy, "");

void paste()
{
    EDITSEL("paste");
    bool mp = multiplayer(NULL);
    if(mp)
    {
        loopv(copybuffers) if(copybuffers[i]->xs * copybuffers[i]->ys > MAXNETBLOCKSQR)
        {
            conoutf("\f3copied area too big for multiplayer pasting");
            return;
        }
    }
    if(!copybuffers.length()) { conoutf("nothing to paste"); return; }

    uchar usedslots[256] = { 0 }, *texmap = NULL;
    if(!mp)
    {
        loopv(copybuffers) blocktexusage(*copybuffers[i], usedslots);
        if(sels.length() && copytexconfig) texmap = texconfig_paste(copytexconfig, usedslots);
    }

    loopv(sels)
    {
        block sel = sels[i];
        int selx = sel.x;
        int sely = sel.y;

        loopvj(copybuffers)
        {
            block *copyblock = copybuffers[j];
            int dx = copyblock->x - copybuffers[0]->x, dy = copyblock->y - copybuffers[0]->y;

            sel.xs = copyblock->xs;
            sel.ys = copyblock->ys;
            sel.x = selx + dx;
            sel.y = sely + dy;
            if(!correctsel(sel) || sel.xs!=copyblock->xs || sel.ys!=copyblock->ys) { conoutf("incorrect selection"); return; }
            makeundo(sel);
            blockpaste(*copyblock, sel.x, sel.y, true, texmap);
            if(mp) netblockpaste(*copyblock, sel.x, sel.y, true);
        }
    }
}
COMMAND(paste, "");

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

const char *texturestacktypes[] = { "floor", "wall", "ceiling", "" };

void edittexturestack(char *what, int *slot) // manually manipulate the "last-used" texture lists to put certain textures up front
{
    tofronttex(); // keep laste edited texture
    int n = getlistindex(what, texturestacktypes, true, -1);
    if(n >= 0)
    {
        loopi(256) if(hdr.texlists[n][i] == *slot) curedittex[n] = i; // find stack index of wanted texture slot
    }
    tofronttex();
}
COMMAND(edittexturestack, "si");

void editdrag(bool isdown)
{
    if((dragging = isdown))
    {
        lastx = cx;
        lasty = cy;
        lasth = ch;
        tofronttex();

        if(!editmetakeydown) resetselections();
    }
    makesel(isdown);
    if(!isdown) for(int i = sels.length() - 2; i >= 0; i--)
    {
        block &a = sels.last(), &b = sels[i];
        if(a.x == b.x && a.y == b.y && a.xs == b.xs && a.ys == b.ys)
        { // making a selection twice will deselect both of it
            sels.drop();
            sels.remove(i);
            break;
        }
    }
}

// the core editing function. all the *xy functions perform the core operations
// and are also called directly from the network, the function below it is strictly
// triggered locally. They all have very similar structure.

void editheightxy(bool isfloor, int amount, block &sel)
{
    loopselxy(sel, if(isfloor)
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

void editheight(int *flr, int *amount)
{
    EDITSEL("editheight");
    bool isfloor = *flr==0;
    loopv(sels)
    {
        editheightxy(isfloor, *amount, sels[i]);
        addmsg(SV_EDITXY, "ri7", EDITXY_HEIGHT, sels[i].x, sels[i].y, sels[i].xs, sels[i].ys, isfloor, *amount);
    }
}

COMMAND(editheight, "ii");

// texture type : 0 floor, 1 wall, 2 ceil, 3 upper wall

void renderhudtexturepreview(int slot, int pos, bool highlight)
{
    Texture *pt = slot != DEFAULT_SKY ? lookupworldtexture(slot, false) : NULL;
    int hs = (highlight ? 0 : (FONTH / 3) * 2), bs = VIRTW / 8 - hs, border = FONTH / (highlight ? 2 : 4), x = 2 * VIRTW - VIRTW / 8 - FONTH + hs / 2, y = VIRTH * (6 - pos) / 4, xs, ys;
    blendbox(x - border, y - border, x + bs + border, y + bs + border, false);
    if(pt)
    {
        if(pt->xs > pt->ys) xs = bs, ys = (xs * pt->ys) / pt->xs;  // keep aspect ratio of texture
        else ys = bs, xs = (ys * pt->xs) / pt->ys;
        int xt = x + (bs - xs) / 2, yt = y + (bs - ys) / 2;
        extern int fullbrightlevel;
        framedquadtexture(pt->id, xt, yt, xs, ys, 1, fullbrightlevel);
    }
    else
    { // sky slot: just show blue instead of the texture
        color c(0, 0, 0.6f, 1);
        blendbox(x, y, x + bs, y + bs, false, -1, &c);
    }
    if(highlight)
    {
        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
        border /= 2;
        box2d(x - border, y - border, x + bs + border, y + bs + border, 200);
        glEnable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
    }
}

static int lastedittex = 0;
VARP(hudtexttl, 0, 2500, 10000);

void renderhudtexturepreviews()
{
    if(lastedittex && hudtexttl && (lastmillis - lastedittex) < hudtexttl)
    {
        int atype = lasttype == 3 ? 1 : lasttype;
        int idx = curedittex[atype];
        if(idx < 0) idx = 0;
        int startidx = clamp(idx - 2, 0, 251);
        loopi(5) renderhudtexturepreview(hdr.texlists[atype][startidx + i], i, startidx + i == idx);
    }
}

void edittexxy(int type, int t, block &sel)
{
    loopselxy(sel, switch(type)
    {
        case 0: s->ftex = t; break;
        case 1: s->wtex = t; break;
        case 2: s->ctex = t; break;
        case 3: s->utex = t; break;
    });
}

void edittex(int *type, int *dir)
{
    EDITSEL("edittex");
    if(*type < 0 || *type > 3) return;
    if(*type != lasttype) { tofronttex(); lasttype = *type; }
    int atype = *type == 3 ? 1 : *type;
    int i = curedittex[atype];
    i = i < 0 ? 0 : i + *dir;
    curedittex[atype] = i = min(max(i, 0), 255);
    int t = lasttex = hdr.texlists[atype][i];
    loopv(sels)
    {
        edittexxy(*type, t, sels[i]);
        addmsg(SV_EDITXY, "ri7", EDITXY_TEX, sels[i].x, sels[i].y, sels[i].xs, sels[i].ys, *type, t, 0);
    }
    unsavededits++;
    lastedittex = lastmillis;
}
COMMAND(edittex, "ii");

void settex(int *texture, int *type)
{
    EDITSEL("settex");
    if(*type < 0 || *type > 3) return;
    int atype = *type == 3 ? 1 : *type;
    int t = -1;
    loopi(256) if(*texture == (int)hdr.texlists[atype][i])
    {
        t = (int)hdr.texlists[atype][i];
        break;
    }
    if(t < 0)
    {
        conoutf("invalid/unavaible texture");
        return;
    }
    loopv(sels)
    {
        edittexxy(*type, t, sels[i]);
        addmsg(SV_EDITXY, "ri7", EDITXY_TEX, sels[i].x, sels[i].y, sels[i].xs, sels[i].ys, *type, t, 0);
    }
    unsavededits++;
}
COMMAND(settex, "ii");

void replace()
{
    EDITSELMP("replace");
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
    unsavededits++;
}
COMMAND(replace, "");

void edittypexy(int type, block &sel)
{
    loopselxy(sel, s->type = type);
}

void edittype(int type)
{
    EDITSEL("solid|corner|heightfield");
    loopv(sels)
    {
        block &sel = sels[i];
        if(type == CORNER && (sel.xs != sel.ys || (sel.xs != 1 && sel.xs != 2 && sel.xs != 4 && sel.xs != 8 && sel.xs != 16) || (sel.x | sel.y) & (sel.xs - 1)))
            conoutf("corner selection must be power of 2 aligned");
        else
        {
            edittypexy(type, sel);
            addmsg(SV_EDITXY, "ri7", EDITXY_TYPE, sel.x, sel.y, sel.xs, sel.ys, type, 0);
        }
    }
}

void heightfield(int *t) { edittype(*t==0 ? FHF : CHF); }
void solid(int *t)       { edittype(*t==0 ? SPACE : SOLID); }
void corner()           { edittype(CORNER); }

COMMAND(heightfield, "i");
COMMAND(solid, "i");
COMMAND(corner, "");

void editequalisexy(bool isfloor, block &sel)
{
    int low = 127, hi = -128;
    loopselxy(sel,
    {
        if(s->floor<low) low = s->floor;
        if(s->ceil>hi) hi = s->ceil;
    });
    loopselxy(sel,
    {
        if(isfloor) s->floor = low; else s->ceil = hi;
        if(s->floor>=s->ceil) s->floor = s->ceil-1;
    });
}

void equalize(int *flr)
{
    bool isfloor = *flr==0;
    EDITSEL("equalize");
    loopv(sels)
    {
        block &sel = sels[i];
        editequalisexy(isfloor, sel);
        addmsg(SV_EDITXY, "ri7", EDITXY_EQUALISE, sel.x, sel.y, sel.xs, sel.ys, isfloor, 0);
    }
}
COMMAND(equalize, "i");

void setvdeltaxy(int delta, block &sel)
{
    loopselxy(sel, s->vdelta = max(s->vdelta+delta, 0));
}

void setvdelta(int *delta)
{
    EDITSEL("vdelta");
    loopv(sels)
    {
        setvdeltaxy(*delta, sels[i]);
        addmsg(SV_EDITXY, "ri7", EDITXY_VDELTA, sels[i].x, sels[i].y, sels[i].xs, sels[i].ys, *delta, 0);
    }
}
COMMANDN(vdelta, setvdelta, "i");

VARP(enlargearchslopeselections, 0, 0, 1); // "1" is classic behaviour

const int MAXARCHVERT = 50;
int archverts[MAXARCHVERT][MAXARCHVERT];
bool archvinit = false;

void archvertex(int *span, int *vert, char *delta)
{
    if(!archvinit)
    {
        archvinit = true;
        loop(s,MAXARCHVERT) loop(v,MAXARCHVERT) archverts[s][v] = 0;
    }
    if(*span>=MAXARCHVERT || *vert>=MAXARCHVERT || *span<0 || *vert<0) return;
    intret(archverts[*span][*vert]);
    if(*delta) archverts[*span][*vert] = ATOI(delta);
}
COMMAND(archvertex, "iis");

void archxy(int sidedelta, int *averts, block &sel)
{
    loopselxy(sel, s->vdelta =
        sel.xs>sel.ys
            ? (averts[x] + (y==0 || y==sel.ys-1 ? sidedelta : 0))
            : (averts[y] + (x==0 || x==sel.xs-1 ? sidedelta : 0)));
}

void arch(int *sidedelta)
{
    EDITSEL("arch");
    loopv(sels)
    {
        block &sel = sels[i];
        sel.xs++;
        sel.ys++;
        if(sel.xs>MAXARCHVERT) sel.xs = MAXARCHVERT;
        if(sel.ys>MAXARCHVERT) sel.ys = MAXARCHVERT;
        int *averts = sel.xs > sel.ys ? &archverts[sel.xs-1][0] : &archverts[sel.ys-1][0];
        addmsg(SV_EDITARCH, "ri5v", sel.x, sel.y, sel.xs, sel.ys, *sidedelta, MAXARCHVERT, averts);
        archxy(*sidedelta, averts, sel);
        if(!enlargearchslopeselections) { sel.xs--; sel.ys--; }
    }
}
COMMAND(arch, "i");

void slopexy(int xd, int yd, block &sel)
{
    int off = 0;
    if(xd < 0) off -= xd * sel.xs;
    if(yd < 0) off -= yd * sel.ys;
    sel.xs++;
    sel.ys++;
    loopselxy(sel, s->vdelta = xd * x + yd * y + off);
}

void slope(int *xd, int *yd)
{
    EDITSEL("slope");
    loopv(sels)
    {
        block &sel = sels[i];
        addmsg(SV_EDITXY, "ri7", EDITXY_SLOPE, sel.x, sel.y, sel.xs, sel.ys, *xd, *yd);
        slopexy(*xd, *yd, sel); // (changes xs and ys)
        if(!enlargearchslopeselections) { sel.xs--; sel.ys--; }
    }
}
COMMAND(slope, "ii");

void stairsxy(int xd, int yd, block &sel)
{
    int off = xd || yd ? 1 : 0, xo = xd < 0 ? 1 - sel.xs : 0, yo = yd < 0 ? 1 - sel.ys : 0;
    loopselxy(sel, s->floor += (xd ? (x + xo) / xd : 0) + (yd ? (y + yo) / yd : 0) + off);
}

void stairs(int *xd, int *yd)
{
    EDITSEL("stairs");
    loopv(sels)
    {
        block &sel = sels[i];
        addmsg(SV_EDITXY, "ri7", EDITXY_STAIRS, sel.x, sel.y, sel.xs, sel.ys, *xd, *yd);
        stairsxy(*xd, *yd, sel);
    }
}
COMMAND(stairs, "ii");

void perlin(int *scale, int *seed, int *psize)
{
    EDITSELMP("perlin");
    loopv(sels)
    {
        block sel = sels[i];
        sel.xs++;
        sel.ys++;
        makeundo(sel);
        sel.xs--;
        sel.ys--;
        perlinarea(sel, *scale, *seed, *psize);
        sel.xs++;
        sel.ys++;
        remipgenerous(sel);
        if(!enlargearchslopeselections) { sel.xs--; sel.ys--; }
    }
}
COMMAND(perlin, "iii");

VARF(fullbright, 0, 0, 1,
    if(fullbright)
    {
        if(noteditmode("fullbright")) return;
        fullbrightlight();
    }
    else calclight();
);

void edittagxy(int orv, int andv, block &sel)
{
    loopselxy(sel, s->tag = (s->tag & andv) | orv);
}

void edittag(int *tag)
{
    EDITSEL("edittag");
    loopv(sels)
    {
        edittagxy(*tag, 0, sels[i]);
        addmsg(SV_EDITXY, "ri7", EDITXY_TAG, sels[i].x, sels[i].y, sels[i].xs, sels[i].ys, *tag, 0);
    }
}
COMMAND(edittag, "i");

void edittagclip(char *tag)
{
    int nt = ((int) strtol(tag, NULL, 0)) & TAGANYCLIP;
    if(tolower(*tag) == 'n') nt = 0; // "none", "nil, "nop"
    else if(!strncasecmp(tag, "pl", 2)) nt = TAGPLCLIP; // "playerclip", "plclip", "pl"
    else if(isalpha(*tag)) nt = TAGCLIP; // "clip", "all", "full", "hippo"
    EDITSEL("edittagclip");
    loopv(sels)
    {
        edittagxy(nt, TAGTRIGGERMASK, sels[i]);
        addmsg(SV_EDITXY, "ri7", EDITXY_TAG, sels[i].x, sels[i].y, sels[i].xs, sels[i].ys, nt, TAGTRIGGERMASK);
    }
}
COMMAND(edittagclip, "s");

void newent(char *what, float *a1, float *a2, float *a3, float *a4)
{
    EDITSEL("newent");
    loopv(sels) newentity(-1, sels[i].x, sels[i].y, (int)camera1->o.z, what, *a1, *a2, *a3, *a4);
}
COMMAND(newent, "sffff");

void movemap(int *xop, int *yop, int *zop) // move whole map
{
    EDITMP("movemap");
    int xo = *xop, yo = *yop, zo = *zop;
    if(!worldbordercheck(MINBORD + max(-xo, 0), MINBORD + max(xo, 0), MINBORD + max(-yo, 0), MINBORD + max(yo, 0), max(zo, 0), max(-zo, 0)))
    {
        conoutf("not enough space to move the map");
        return;
    }
    if(xo || yo)
    {
        block b = { max(-xo, 0), max(-yo, 0), ssize - iabs(xo), ssize - iabs(yo) }, *cp = blockcopy(b);
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
        setfvar("waterlevel", waterlevel + zo, true);
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
    unsavededits++;
}
COMMAND(movemap, "iii");

void selfliprotate(block &sel, int dir)
{
    makeundo(sel);
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
    remipgenerous(sel);
    freeblock(org);
}

void selectionrotate(int *dir)
{
    EDITSEL("selectionrotate");
    *dir &= 3;
    if(!*dir) return;
    loopv(sels)
    {
        block &sel = sels[i];
        if(sel.xs == sel.ys || *dir == 2)
        {
            selfliprotate(sel, *dir);
            addmsg(SV_EDITXY, "ri7", EDITXY_FLIPROT, sel.x, sel.y, sel.xs, sel.ys, *dir, 0);
        }
    }
}
COMMAND(selectionrotate, "i");

void selectionflip(char *axis)
{
    EDITSEL("selectionflip");
    char c = toupper(*axis);
    if(c != 'X' && c != 'Y') return;
    int dir =  c == 'X' ? 11 : 12;
    loopv(sels)
    {
        selfliprotate(sels[i], dir);
        addmsg(SV_EDITXY, "ri7", EDITXY_FLIPROT, sels[i].x, sels[i].y, sels[i].xs, sels[i].ys, dir, 0);
    }
}
COMMAND(selectionflip, "s");

void selectionwalk(char *action, char *beginsel, char *endsel)
{
    EDITSEL("selectionwalk");
    bool mp = multiplayer(NULL);
    const char *localvars[] = { "sw_cursel", "sw_abs_x", "sw_abs_y", "sw_rel_x", "sw_rel_y", "sw_type", "sw_floor", "sw_ceil", "sw_wtex", "sw_ftex", "sw_ctex", "sw_utex", "sw_vdelta", "sw_tag", "sw_r", "sw_g", "sw_b", "" };
    for(int i = 0; *localvars[i]; i++) push(localvars[i], "");
    int maxsels = sels.length();
    loopv(sels)
    {
        if(i >= maxsels) break; // don't allow the script to add selections
        block sel = sels[i]; // (copy, not reference)
        defformatstring(tmp)("%d %d %d %d", sel.x, sel.y, sel.xs, sel.ys);
        alias("sw_cursel", tmp);
        if(*beginsel) execute(beginsel);
        bool haveundo = false, isro = mp && sel.xs * sel.ys > MAXNETBLOCKSQR, didwarn = false;
        loop(x, sel.xs) loop(y, sel.ys)
        {
            sqr *s = S(sel.x + x, sel.y + y), so = *s;
            formatstring(tmp)("%d", sel.x + x); alias("sw_abs_x", tmp);
            formatstring(tmp)("%d", sel.y + y); alias("sw_abs_y", tmp);
            formatstring(tmp)("%d", x); alias("sw_rel_x", tmp);
            formatstring(tmp)("%d", y); alias("sw_rel_y", tmp);
            #define AATTR(x) formatstring(tmp)("%d", s->x); alias("sw_"#x, tmp)
            AATTR(type); AATTR(floor); AATTR(ceil); AATTR(wtex); AATTR(ftex); AATTR(ctex); AATTR(utex); AATTR(vdelta); AATTR(tag); AATTR(r); AATTR(g); AATTR(b);
            #undef AATTR
            if(*action) execute(action);
            so.type = getlistindex(getalias("sw_type"), cubetypenames, true, int(SPACE));
            #define GETA(x) so.x = ATOI(getalias("sw_"#x))
            GETA(floor); GETA(ceil); GETA(wtex); GETA(ftex); GETA(ctex); GETA(utex); GETA(vdelta); GETA(tag); GETA(r); GETA(g); GETA(b);
            #undef GETA
            if(memcmp(s, &so, sizeof(sqr)))
            {
                if(isro)
                {
                    if(!didwarn) conoutf("\f3selectionwalk: selected area %dx%d too big for multiplayer editing, changes ignored!", sel.xs, sel.ys);
                    didwarn = true;
                }
                else
                {
                    if(!haveundo) makeundo(sel);
                    haveundo = true;
                    *s = so;
                }
            }
        }
        if(haveundo)
        {
            if(mp)
            {
                block *b = blockcopy(sel);
                netblockpaste(*b, sel.x, sel.y, true);
                freeblockp(b);
            }
            remipgenerous(sel);
        }
        if(*endsel) execute(endsel);
    }
    for(int i = 0; *localvars[i]; i++) pop(localvars[i]);
}
COMMAND(selectionwalk, "sss");

void transformclipentities()  // transforms all clip entities to tag clips, if they are big enough (so, that no player could be above or below them)
{ // (hardcoded factor ENTSCALE10 for attr1 and ENTSCALE5 for attr2-4)
    EDITMP("transformclipentities");
    int total = 0, thisrun, bonus = 5;
    do
    {
        thisrun = 0;
        loopv(ents)
        {
            entity &e = ents[i];
            if((e.type == CLIP || e.type == PLCLIP) && e.attr2 / ENTSCALE5 && e.attr3 / ENTSCALE5 && e.attr4 / ENTSCALE5)
            {
                int allowedspace = e.type == CLIP ? 1 : 4;
                int clipmask = e.type == CLIP ? TAGCLIP : TAGPLCLIP;
                int i2 = e.attr2 / ENTSCALE5, i3 = e.attr3 / ENTSCALE5; // floor values
                int r2 = (e.attr2 % ENTSCALE5) > 0, r3 = (e.attr3 % ENTSCALE5) > 0; // fractions
                int x1i = e.x - i2, x2i = e.x + i2 - 1, y1i = e.y - i3, y2i = e.y + i3 - 1; // inner rectangle (fully covered cubes)
                int x1o = x1i - r2, x2o = x2i + r2, y1o = y1i - r3, y2o = y2i + r3; // outer rectangle (partially covered cubes)
                float z1 = S(e.x, e.y)->floor + float(e.attr1) / ENTSCALE10, z2 = z1 + float(e.attr4) / ENTSCALE5;
                bool bigenough = true, nodelete = false;
                for(int xx = x1o; xx <= x2o; xx++) for(int yy = y1o; yy <= y2o; yy++) // loop over outer rectangle to check, if a clip has the required height
                {
                    if(OUTBORD(xx,yy) || SOLID(S(xx,yy))) continue;
                    bool inner = xx >= x1i && xx <= x2i && yy >= y1i && yy <= y2i; // flag: xx|yy is inner rectangle
                    sqr *s[4] = { S(xx, yy), S(xx + 1, yy), S(xx, yy + 1), S(xx + 1, yy + 1) };
                    int vdeltamax = 0;
                    loopj(4) if(s[j]->vdelta > vdeltamax) vdeltamax = s[j]->vdelta;
                    int floor = s[0]->floor - (s[0]->type == FHF ? (vdeltamax + 3) / 4 : 0),
                        ceil = s[0]->ceil - (s[0]->type == CHF ? (vdeltamax + 3) / 4 : 0);
                    bool alreadytagged = (s[0]->tag & (TAGCLIP | clipmask)) != 0;
                    if((z1 - floor > allowedspace || ceil - z2 > allowedspace) && !alreadytagged) bigenough = false;
                    if(!inner && !alreadytagged) nodelete = true; // fractional part of the clip would not be covered: entity can not be deleted
                }
                if(bigenough)
                {
                    for(int xx = x1i; xx <= x2i; xx++) for(int yy = y1i; yy <= y2i; yy++) // only convert inner rectangle to tag clips
                    {
                        if(!OUTBORD(xx,yy) && !SOLID(S(xx,yy))) S(xx, yy)->tag |= clipmask;
                    }
                    if(!nodelete)
                    {
                        deleted_ents.add(e);
                        e.type = NOTUSED; // only delete entity, if it is now fully covered in tag clips
                        thisrun++;
                    }
                }
            }
        }
        total += thisrun;
    }
    while(thisrun || bonus-- > 0);
    loopi(ssize) loopj(ssize) { sqr *s = S(i,j); if(s->tag & TAGCLIP) s->tag &= ~TAGPLCLIP; }
    conoutf("changed %d clip entities to tagged clip areas", total);
    if(total) unsavededits++;
}

COMMAND(transformclipentities, "");

void reseteditor() // reset only stuff that would cause trouble editing the next map (don't reset selections, for example)
{
    loopk(3) curedittex[k] = -1;
    pruneundos(0);
    undolevel = 0;
    pinnedclosestent = false;
}
