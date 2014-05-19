// editing.cpp: most map editing commands go here, entity editing commands are in world.cpp

#include "cube.h"

bool editmode = false;

// the current selections, used by almost all editing commands
// invariant: all code assumes that these are kept inside MINBORD distance of the edge of the map
// => selections are checked when they are made or when the world is reloaded

vector<block> sels;

#define loopselxy(sel, b) { makeundo(sel); loop(x,(sel).xs) loop(y,(sel).ys) { sqr *s = S((sel).x+x, (sel).y+y); b; } remip(sel); }
#define loopselsxy(b) { loopv(sels) loopselxy(sels[i], b); }

int cx, cy, ch;

int curedittex[] = { -1, -1, -1 };

bool dragging = false;
int lastx, lasty, lasth;

int lasttype = 0, lasttex = 0;
sqr rtex;

VAR(editing, 1, 0, 0);
VAR(unsavededits, 1, 0, 0);

bool editmetakeydown = false;
COMMANDF(editmeta, "d", (bool on) { editmetakeydown = on; } );

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
    editing = editmode ? 1 : 0;
    //2011oct16:flowtron:keep spectator state
    //spectators are ghosts, no toggling of editmode for them!
    player1->state = player1->state==CS_SPECTATE?CS_SPECTATE:(editing ? CS_EDITING : CS_ALIVE);
    if(editing && player1->onladder) player1->onladder = false;
    if(editing && (player1->weaponsel->type == GUN_SNIPER && ((sniperrifle *)player1->weaponsel)->scoped)) ((sniperrifle *)player1->weaponsel)->onownerdies(); // or ondeselecting()
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

char *editinfo()
{
    static string info;
    if(!editmode) return NULL;
    info[0] = '\0';
    int e = closestent();
    if(e >= 0)
    {
        entity &c = ents[e];
        formatstring(info)("closest entity = %s (%d, %d, %d, %d), ", entnames[c.type], c.attr1, c.attr2, c.attr3, c.attr4);
    }
    if(selset()) concatformatstring(info, "selection = (%d, %d)", (sels.last()).xs, (sels.last()).ys);
    else concatformatstring(info, "no selection");
    sqr *s;
    if(!OUTBORD(cx, cy) && (s = S(cx,cy)) && !editfocusdetails(s) && !SOLID(s) && s->tag) concatformatstring(info, ", tag 0x%02X", s->tag);
    return info;
}


#define EDITSEL   if(noteditmode("EDITSEL") || noselection()) return
#define EDITSELMP if(noteditmode("EDITSELMP") || noselection() || multiplayer()) return
#define EDITMP    if(noteditmode("EDITMP") || multiplayer()) return

// multiple sels

// add a selection to the list
void addselection(int x, int y, int xs, int ys, int h)
{
    block &s = sels.add();
    s.x = x; s.y = y; s.xs = xs; s.ys = ys; s.h = h;
    if(!correctsel(s)) { sels.drop(); }
}

// reset all selections
void resetselections()
{
    sels.shrink(0);
}

// reset all invalid selections
void checkselections()
{
    loopv(sels) if(!correctsel(sels[i])) sels.remove(i);
}

// update current selection, or add a new one
void makesel(bool isnew)
{
    block &cursel = sels.last(); //RR 10/12/12 - FIXEME, error checking should happen with "isnew", not here checking if it really is new.
    if(isnew || sels.length() == 0) addselection(min(lastx, cx), min(lasty, cy), abs(lastx-cx)+1, abs(lasty-cy)+1, max(lasth, ch));
    else
    {
        cursel.x = min(lastx, cx); cursel.y = min(lasty, cy);
        cursel.xs = abs(lastx-cx)+1; cursel.ys = abs(lasty-cy)+1;
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

VAR(flrceil,0,0,2);
VAR(editaxis,0,0,13);
VARP(showgrid,0,1,1);

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

    if(showgrid)
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

    if(selset())
    {
        linestyle(GRIDS, 0xFF, 0x40, 0x40);
        loopv(sels) box(sels[i], (float)sels[i].h, (float)sels[i].h, (float)sels[i].h, (float)sels[i].h);
    }

    if(!showtagclipfocus && showtagclips)
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

void editundo()
{
    EDITMP;
    if(undos.empty()) { conoutf("nothing more to undo"); return; }
    block *p = undos.pop();
    redos.add(blockcopy(*p));
    if(editmetakeydown) restoreposition(*p);
    blockpaste(*p);
    freeblock(p);
    unsavededits++;
}

void editredo()
{
    EDITMP;
    if(redos.empty()) { conoutf("nothing more to redo"); return; }
    block *p = redos.pop();
    undos.add(blockcopy(*p));
    if(editmetakeydown) restoreposition(*p);
    blockpaste(*p);
    freeblock(p);
    unsavededits++;
}

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

void resetcopybuffers()
{
    loopv(copybuffers) freeblock(copybuffers[i]);
    copybuffers.shrink(0);
}

void copy()
{
    EDITSELMP;
    resetcopybuffers();
    loopv(sels)
    {
        block *b = blockcopy(sels[i]);
        copybuffers.add(b);
    }

}

void paste()
{
    EDITSELMP;
    if(!copybuffers.length()) { conoutf("nothing to paste"); return; }

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
            blockpaste(*copyblock, sel.x, sel.y, true);
        }
    }
}

// Count the walls of type "type" contained in the current selection
void countwalls(int *type)
{
    int counter = 0;
    EDITSELMP;
    if(*type < 0 || *type >= MAXTYPE)
    {
        conoutf("invalid type");
        intret(0);
    }
    loopselsxy(if(s->type==*type) counter++)
    intret(counter);
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
    EDITSEL;
    bool isfloor = *flr==0;
    loopv(sels)
    {
        editheightxy(isfloor, *amount, sels[i]);
        addmsg(SV_EDITH, "ri6", sels[i].x, sels[i].y, sels[i].xs, sels[i].ys, isfloor, *amount);
    }
}

COMMAND(editheight, "ii");

// texture type : 0 floor, 1 wall, 2 ceil, 3 upper wall

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
    loopv(sels)
    {
        edittexxy(type, t, sels[i]);
        addmsg(SV_EDITT, "ri6", sels[i].x, sels[i].y, sels[i].xs, sels[i].ys, type, t);
    }
    unsavededits++;
}

void settex(int texture, int type)
{
    EDITSEL;
    if(type<0 || type>3) return;
    int atype = type==3 ? 1 : type;
    int t = -1;
    loopi(256) if(texture == (int)hdr.texlists[atype][i])
    {
        t = (int)hdr.texlists[atype][i];
        break;
    }
    if(t<0)
    {
        conoutf("invalid/unavaible texture");
        return;
    }
    loopv(sels)
    {
        edittexxy(type, t, sels[i]);
        addmsg(SV_EDITT, "ri6", sels[i].x, sels[i].y, sels[i].xs, sels[i].ys, type, t);
    }
    unsavededits++;
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
    unsavededits++;
}

void edittypexy(int type, block &sel)
{
    loopselxy(sel, s->type = type);
}

void edittype(int type)
{
    EDITSEL;
    loopv(sels)
    {
        block &sel = sels[i];
        if(type == CORNER && (sel.xs != sel.ys || (sel.xs != 1 && sel.xs != 2 && sel.xs != 4 && sel.xs != 8 && sel.xs != 16) || (sel.x | sel.y) & (sel.xs - 1)))
            conoutf("corner selection must be power of 2 aligned");
        else
        {
            edittypexy(type, sel);
            addmsg(SV_EDITS, "ri5", sel.x, sel.y, sel.xs, sel.ys, type);
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
    EDITSEL;
    loopv(sels)
    {
        block &sel = sels[i];
        editequalisexy(isfloor, sel);
        addmsg(SV_EDITE, "ri5", sel.x, sel.y, sel.xs, sel.ys, isfloor);
    }
}

COMMAND(equalize, "i");

void setvdeltaxy(int delta, block &sel)
{
    loopselxy(sel, s->vdelta = max(s->vdelta+delta, 0));
    remipmore(sel);
}

void setvdelta(int delta)
{
    EDITSEL;
    loopv(sels)
    {
        setvdeltaxy(delta, sels[i]);
        addmsg(SV_EDITD, "ri5", sels[i].x, sels[i].y, sels[i].xs, sels[i].ys, delta);
    }
}

const int MAXARCHVERT = 50;
int archverts[MAXARCHVERT][MAXARCHVERT];
bool archvinit = false;

void archvertex(int *span, int *vert, int *delta)
{
    if(!archvinit)
    {
        archvinit = true;
        loop(s,MAXARCHVERT) loop(v,MAXARCHVERT) archverts[s][v] = 0;
    }
    if(*span>=MAXARCHVERT || *vert>=MAXARCHVERT || *span<0 || *vert<0) return;
    archverts[*span][*vert] = *delta;
}

void arch(int *sidedelta)
{
    EDITSELMP;
    loopv(sels)
    {
        block &sel = sels[i];
        sel.xs++;
        sel.ys++;
        if(sel.xs>MAXARCHVERT) sel.xs = MAXARCHVERT;
        if(sel.ys>MAXARCHVERT) sel.ys = MAXARCHVERT;
        loopselxy(sel, s->vdelta =
            sel.xs>sel.ys
                ? (archverts[sel.xs-1][x] + (y==0 || y==sel.ys-1 ? *sidedelta : 0))
                : (archverts[sel.ys-1][y] + (x==0 || x==sel.xs-1 ? *sidedelta : 0)));
        remipmore(sel);
    }
}

void slope(int xd, int yd)
{
    EDITSELMP;
    loopv(sels)
    {
        block &sel = sels[i];
        int off = 0;
        if(xd<0) off -= xd*sel.xs;
        if(yd<0) off -= yd*sel.ys;
        sel.xs++;
        sel.ys++;
        loopselxy(sel, s->vdelta = xd*x+yd*y+off);
        remipmore(sel);
    }
}

void perlin(int scale, int seed, int psize)
{
    EDITSELMP;
    loopv(sels)
    {
        block sel = sels[i];
        sel.xs++;
        sel.ys++;
        makeundo(sel);
        sel.xs--;
        sel.ys--;
        perlinarea(sel, scale, seed, psize);
        sel.xs++;
        sel.ys++;
        remipmore(sel);
    }
}

VARF(fullbright, 0, 0, 1,
    if(fullbright)
    {
        if(noteditmode("fullbright")) return;
        fullbrightlight();
    }
    else calclight();
);

void edittag(int *tag)
{
    EDITSELMP;
    loopselsxy(s->tag = *tag);
}

void newent(char *what, int *a1, int *a2, int *a3, int *a4)
{
    EDITSEL;
    loopv(sels) newentity(-1, sels[i].x, sels[i].y, (int)camera1->o.z, what, *a1, *a2, *a3, *a4);
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
        setvar("waterlevel", (hdr.waterlevel += zo));
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
    remipmore(sel);
    freeblock(org);
}

void selectionrotate(int dir)
{
    EDITSELMP;
    dir &= 3;
    if(!dir) return;
    loopv(sels)
    {
        block &sel = sels[i];
        if(sel.xs == sel.ys || dir ==  2) selfliprotate(sel, dir);
    }
}

void selectionflip(char *axis)
{
    EDITSELMP;
    char c = toupper(*axis);
    if(c != 'X' && c != 'Y') return;
    loopv(sels) selfliprotate(sels[i], c == 'X' ? 11 : 12);
}

COMMANDF(select, "iiii", (int *x, int *y, int *xs, int *ys) { resetselections(); addselection(*x, *y, *xs, *ys, 0); });
COMMANDF(addselection, "iiii", (int *x, int *y, int *xs, int *ys) { addselection(*x, *y, *xs, *ys, 0); });
COMMAND(resetselections, "");
COMMAND(edittag, "i");
COMMAND(replace, "");
COMMAND(archvertex, "iii");
COMMAND(arch, "i");
COMMANDF(slope, "ii", (int *xd, int *yd) { slope(*xd, *yd); });
COMMANDF(vdelta, "i", (int *d) { setvdelta(*d); });
COMMANDN(undo, editundo, "");
COMMANDN(redo, editredo, "");
COMMAND(copy, "");
COMMAND(paste, "");
COMMANDF(edittex, "ii", (int *type, int *dir) { edittex(*type, *dir); });
COMMAND(newent, "siiii");
COMMANDF(perlin, "iii", (int *sc, int *se, int *ps) { perlin(*sc, *se, *ps); });
COMMANDF(movemap, "iii", (int *x, int *y, int *z) { movemap(*x, *y, *z); });
COMMANDF(selectionrotate, "i", (int *d) { selectionrotate(*d); });
COMMAND(selectionflip, "s");
COMMAND(countwalls, "i");
COMMANDF(settex, "ii", (int *texture, int *type) { settex(*texture, *type); });

void transformclipentities()  // transforms all clip entities to tag clips, if they are big enough (so, that no player could be above or below them)
{
    EDITMP;
    int total = 0, thisrun;
    do
    {
        thisrun = 0;
        loopv(ents)
        {
            entity &e = ents[i];
            if((e.type == CLIP || e.type == PLCLIP) && e.attr2 && e.attr3 && e.attr4)
            {
                int allowedspace = e.type == CLIP ? 1 : 4;
                int clipmask = e.type == CLIP ? TAGCLIP : TAGPLCLIP;
                int x1 = e.x - e.attr2, x2 = e.x + e.attr2 - 1, y1 = e.y - e.attr3, y2 = e.y + e.attr3 - 1;
                int z1 = S(e.x, e.y)->floor + e.attr1, z2 = z1 + e.attr4;
                bool bigenough = true;
                for(int xx = x1; xx <= x2; xx++) for(int yy = y1; yy <= y2; yy++)
                {
                    if(OUTBORD(xx,yy) || SOLID(S(xx,yy))) continue;
                    sqr *s[4] = { S(xx, yy), S(xx + 1, yy), S(xx, yy + 1), S(xx + 1, yy + 1) };
                    int vdeltamax = 0;
                    loopj(4) if(s[j]->vdelta > vdeltamax) vdeltamax = s[j]->vdelta;
                    int floor = s[0]->floor - (s[0]->type == FHF ? (vdeltamax + 3) / 4 : 0),
                        ceil = s[0]->ceil - (s[0]->type == CHF ? (vdeltamax + 3) / 4 : 0);
                    if((z1 - floor > allowedspace || ceil - z2 > allowedspace) && !(s[0]->tag & (TAGCLIP | clipmask))) bigenough = false;
                }
                if(bigenough)
                {
                    for(int xx = x1; xx <= x2; xx++) for(int yy = y1; yy <= y2; yy++)
                    {
                        if(!OUTBORD(xx,yy) && !SOLID(S(xx,yy))) S(xx, yy)->tag |= clipmask;
                    }
                    e.type = NOTUSED;
                    thisrun++;
                }
            }
        }
        total += thisrun;
    }
    while(thisrun);
    loopi(ssize) loopj(ssize) { sqr *s = S(i,j); if(s->tag & TAGCLIP) s->tag &= ~TAGPLCLIP; }
    conoutf("changed %d clip entities to tagged clip areas", total);
    if(total) unsavededits++;
}

COMMAND(transformclipentities, "");
