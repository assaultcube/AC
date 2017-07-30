// rendertext.cpp: font rendering

#include "cube.h"

int VIRTW;
bool ignoreblinkingbit = false; // for remote-n-temp override of '\fb'
static hashtable<const char *, font> fonts;
static font *fontdef = NULL;

font *curfont = NULL;

VARP(allowblinkingtext, 0, 0, 1); // if you're so inclined

void newfont(char *name, char *tex, int *defaultw, int *defaulth, int *offsetx, int *offsety, int *offsetw, int *offseth)
{
    if(*defaulth < 10) return;          // (becomes FONTH)
    Texture *_tex = textureload(tex);
    if(_tex == notexture || !_tex->xs || !_tex->ys) return;
    font *f = fonts.access(name);
    if(!f)
    {
        name = newstring(name);
        f = &fonts[name];
        f->name = name;
    }

    f->tex = _tex;
    f->chars.shrink(0);
    f->defaultw = *defaultw;
    f->defaulth = *defaulth;
    f->offsetx = *offsetx;
    f->offsety = *offsety;
    f->offsetw = *offsetw;
    f->offseth = *offseth;
    f->skip = 33;

    fontdef = f;
}
COMMANDN(font, newfont, "ssiiiiii");

void fontchar(int *x, int *y, int *w, int *h)
{
    if(!fontdef) return;

    font::charinfo &c = fontdef->chars.add();
    c.w = *w ? *w : fontdef->defaultw;
    c.h = *h ? *h : fontdef->defaulth;
    c.left    = (*x + fontdef->offsetx) / float(fontdef->tex->xs);
    c.top     = (*y + fontdef->offsety) / float(fontdef->tex->ys);
    c.right   = (*x + c.w + fontdef->offsetw) / float(fontdef->tex->xs);
    c.bottom  = (*y + c.h + fontdef->offseth) / float(fontdef->tex->ys);
}
COMMAND(fontchar, "iiii");

void fontskip(int *n)
{
    if(!fontdef) return;

    fontdef->skip = *n;
}
COMMAND(fontskip, "i");

bool setfont(const char *name)
{
    font *f = fonts.access(name);
    if(!f) return false;
    curfont = f;
    return true;
}
COMMAND(setfont, "s");

COMMANDF(curfont, "", () { result(curfont && curfont->name ? curfont->name : ""); });

font *getfont(const char *name)
{
    return fonts.access(name);
}

static vector<font *> fontstack;

void pushfont(const char *name)
{
    fontstack.add(curfont);
    setfont(name);
}

void popfont()
{
    if(!fontstack.empty()) curfont = fontstack.pop();
}

int text_width(const char *str)
{
    int width, height;
    text_bounds(str, width, height);
    return width;
}

void draw_textf(const char *fstr, int left, int top, ...)
{
    defvformatstring(str, top, fstr);
    draw_text(str, left, top);
}

inline int draw_char_contd(font &f, font::charinfo &info, int charcode, int x, int y)
{
    glTexCoord2f(info.left,  info.top   ); glVertex2f(x,          y);
    glTexCoord2f(info.right, info.top   ); glVertex2f(x + info.w, y);
    glTexCoord2f(info.right, info.bottom); glVertex2f(x + info.w, y + info.h);
    glTexCoord2f(info.left,  info.bottom); glVertex2f(x,          y + info.h);

    xtraverts += 4;
    return info.w;
}

static int draw_char(int c, int x, int y)
{
    if(curfont->chars.inrange(c-curfont->skip))
    {
        font::charinfo &info = curfont->chars[c-curfont->skip];

        return draw_char_contd(*curfont, info, c, x, y);
    }
    return 0;
}

vector<threeint> igraphbatch;

void queue_igraph(int c, int x, int y)
{
    threeint v = { c, x, y };
    igraphbatch.add(v);
}

VARP(igraphsize, 80, 120, 300);
VARP(igraphsizehardcoded, 80, 106, 160);
VARP(igraphanimate, 0, 1, 1);

void render_igraphs()
{
    igraphbatch.sort(cmpintasc);
    int last = 0, w[2] = { (FONTH * igraphsize) / 100, (FONTH * igraphsizehardcoded) / 100 }, offs[2] = { (FONTH - w[0]) / 2, (FONTH - w[1]) / 2 }, ishc = 0;
    igraph *ig = NULL;
    glColor4ub(255, 255, 255, 255);
    loopv(igraphbatch)
    {
        if(last != igraphbatch[i].key) ig = getusedigraph((last = igraphbatch[i].key)), ishc = last > 0 && last < 10 ? 1 : 0;
        if(ig && ig->tex)
        {
            int &x = igraphbatch[i].val1, &y = igraphbatch[i].val2, fpos = ((totalmillis + x * x + VIRTW * y) % ig->frames.last() + ig->frames.last()) % ig->frames.last(), frame = 0;
            float fw = min(1.0f, float(ig->tex->ys) / ig->tex->xs);
            if(igraphanimate) loopvj(ig->frames) if(ig->frames[j] < fpos) frame++;
            quad(ig->tex->id, x + offs[ishc], y + offs[ishc], w[ishc], (frame % max(1, ig->tex->xs / ig->tex->ys)) * fw, 0, fw, 1);
        }
    }
}

//stack[sp] is current color index
static void text_color(char c, char *stack, int size, int &sp, bvec color, int a)
{
    if(c=='s') // save color
    {
        c = stack[sp];
        if(sp<size-1) stack[++sp] = c;
    }
    else
    {
        if(c=='r') c = stack[(sp > 0) ? --sp : sp]; // restore color
        else if(c == 'b') { if(allowblinkingtext && !ignoreblinkingbit) stack[sp] *= -1; } // blinking text - only if allowed
        else stack[sp] = c;
        switch(iabs(stack[sp]))
        {
            case '0': color = bvec( 2,  255,  128 ); break;   // green: player talk
            case '1': color = bvec( 96,  160, 255 ); break;   // blue: team chat
            case '2': color = bvec( 255, 192,  64 ); break;   // yellow: gameplay action messages, only actions done by players - 230 230 20 too bright
            case '3': color = bvec( 255,  64,  64 ); break;   // red: important errors and notes
            case '4': color = bvec( 128, 128, 128 ); break;   // gray
            case '5': color = bvec( 255, 255, 255 ); break;   // white
            case '6': color = bvec(  96,  48,   0 ); break;   // dark brown
            case '7': color = bvec( 153,  51,  51 ); break;   // dark red: dead admin
            case '8': color = bvec( 192,  64, 192 ); break;   // magenta
            case '9': color = bvec( 255, 102,   0 ); break;   // orange

            case 'A':color = bvec( 0xff, 0xb7, 0xb7); break;   // red set
            case 'B':color = bvec( 0xCC, 0x33, 0x33); break;   //
            case 'C':color = bvec( 0x66, 0x33, 0x33); break;   //
            case 'D':color = bvec( 0xF8, 0x98, 0x4E); break;   //

            case 'E':color = bvec( 0xFF, 0xFF, 0xB7); break;   // yellow set
            case 'F':color = bvec( 0xCC, 0xCC, 0x33); break;   //
            case 'G':color = bvec( 0x66, 0x66, 0x33); break;   //
            case 'H':color = bvec( 0xCC, 0xFC, 0x58); break;   //

            case 'I':color = bvec( 0xB7, 0xFF, 0xB7); break;   // green set
            case 'J':color = bvec( 0x33, 0xCC, 0x33); break;   //
            case 'K':color = bvec( 0x33, 0x66, 0x33); break;   //
            case 'L':color = bvec( 0x3F, 0xFF, 0x98); break;   //

            case 'M':color = bvec( 0xB7, 0xFF, 0xFF); break;   // cyan set
            case 'N':color = bvec( 0x33, 0xCC, 0xCC); break;   //
            case 'O':color = bvec( 0x33, 0x66, 0x66); break;   //
            case 'P':color = bvec( 0x4F, 0xCC, 0xF8); break;   //

            case 'Q':color = bvec( 0xB7, 0xB7, 0xFF); break;   // blue set
            case 'R':color = bvec( 0x33, 0x33, 0xCC); break;   //
            case 'S':color = bvec( 0x33, 0x33, 0x66); break;   //
            case 'T':color = bvec( 0xA0, 0x49, 0xFF); break;   //

            case 'U':color = bvec( 0xFF, 0xB7, 0xFF); break;   // magenta set
            case 'V':color = bvec( 0xCC, 0x33, 0xCC); break;   //
            case 'W':color = bvec( 0x66, 0x33, 0x66); break;   //
            case 'X':color = bvec( 0xFF, 0x01, 0xD5); break;   //

            case 'Y':color = bvec( 0xC7, 0xD1, 0xE2); break;   // lt gray
            case 'Z':color = bvec( 0x32, 0x32, 0x32); break;   // dark gray

            case 'u': color = bvec(120, 240, 120); break;   // stats: green
            case 'v': color = bvec(120, 120, 240); break;   // stats: blue
            case 'w': color = bvec(230, 230, 110); break;   // stats: yellow
            case 'x': color = bvec(250, 100, 100); break;   // stats: red
        }
        int b = (int) (sinf(lastmillis / 200.0f) * 115.0f);
        b = stack[sp] > 0 ? 100 : min(iabs(b), 100);
        glColor4ub(color.x, color.y, color.z, (a * b) / 100);
    }
}

static vector<int> *columns = NULL;

void text_startcolumns()
{
    if(!columns) columns = new vector<int>;
}

void text_endcolumns()
{
    DELETEP(columns);
}

#define TABALIGN(x) ((((x)+PIXELTAB)/PIXELTAB)*PIXELTAB)

#define TEXTGETCOLUMN \
    if(columns && col<columns->length()) \
    { \
        colx += (*columns)[col++]; \
        x = colx; \
    } \
    else x = TABALIGN(x);

#define TEXTSETCOLUMN \
    if(columns) \
    { \
        while(col>=columns->length()) columns->add(0); \
        int w = TABALIGN(x) - colx; \
        w = max(w, (*columns)[col]); \
        (*columns)[col] = w; \
        col++; \
        colx += w; \
        x = colx; \
    } \
    else x = TABALIGN(x);


#define TEXTSKELETON \
    int y = 0, x = 0, col = 0, colx = 0;\
    int i;\
    for(i = 0; str[i]; i++)\
    {\
        TEXTINDEX(i)\
        int c = str[i];\
        if(c=='\t')      { TEXTTAB(i); TEXTWHITE(i) }\
        else if(c==' ')  { x += curfont->defaultw; TEXTWHITE(i) }\
        else if(c=='\n') { TEXTLINE(i) x = 0; y += FONTH; }\
        else if(c=='\f') { if(str[i+1]) { i++; TEXTCOLOR(i) }}\
        else if(c=='\1') { if(str[i+1]) { i++; TEXTIGRAPH(i) }}\
        else if(c=='\a') { if(str[i+1]) { i++; }}\
        else if(curfont->chars.inrange(c-curfont->skip))\
        {\
            if(maxwidth != -1)\
            {\
                int j = i;\
                int w = curfont->chars[c-curfont->skip].w;\
                for(; str[i+1]; i++)\
                {\
                    int c = str[i+1];\
                    bool isig = c == '\1';\
                    if(c=='\f') { if(str[i+2]) i++; continue; }\
                    if(i-j > 16) break;\
                    if(!isig && !curfont->chars.inrange(c-curfont->skip)) break;\
                    int cw = isig ? FONTH + 1 : curfont->chars[c-curfont->skip].w + 1;\
                    if(isig && str[i + 2]) i++;\
                    if(w + cw >= maxwidth) break;\
                    w += cw;\
                }\
                if(x + w >= maxwidth && j!=0) { TEXTLINE(j-1) x = 0; y += FONTH; }\
                TEXTWORD\
            }\
            else\
            { TEXTCHAR(i) }\
        }\
    }

//all the chars are guaranteed to be either drawable or color commands
#define TEXTWORDSKELETON \
                for(; j <= i; j++)\
                {\
                    TEXTINDEX(j)\
                    int c = str[j];\
                    if(c=='\f') { if(str[j+1]) { j++; TEXTCOLOR(j) }}\
                    else if(c=='\1') { if(str[j+1]) { j++; TEXTIGRAPH(j) }}\
                    else { TEXTCHAR(j) }\
                }

int text_visible(const char *str, int hitx, int hity, int maxwidth)
{
    #define TEXTINDEX(idx)
    #define TEXTTAB(idx) TEXTGETCOLUMN
    #define TEXTWHITE(idx) if(y+FONTH > hity && x >= hitx) return idx;
    #define TEXTLINE(idx) if(y+FONTH > hity) return idx;
    #define TEXTCOLOR(idx)
    #define TEXTIGRAPH(idx) x += FONTH; TEXTWHITE(idx)
    #define TEXTCHAR(idx) x += curfont->chars[c-curfont->skip].w+1; TEXTWHITE(idx)
    #define TEXTWORD TEXTWORDSKELETON
    TEXTSKELETON
    #undef TEXTINDEX
    #undef TEXTTAB
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTIGRAPH
    #undef TEXTCHAR
    #undef TEXTWORD
    return i;
}

//inverse of text_visible
void text_pos(const char *str, int cursor, int &cx, int &cy, int maxwidth)
{
    #define TEXTINDEX(idx) if(idx == cursor) { cx = x; cy = y; break; }
    #define TEXTTAB(idx) TEXTGETCOLUMN
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx)
    #define TEXTCOLOR(idx)
    #define TEXTIGRAPH(idx) x += FONTH;
    #define TEXTCHAR(idx) x += curfont->chars[c-curfont->skip].w + 1;
    #define TEXTWORD TEXTWORDSKELETON if(i >= cursor) break;
    cx = INT_MIN;
    cy = 0;
    TEXTSKELETON
    if(cx == INT_MIN) { cx = x; cy = y; }
    #undef TEXTINDEX
    #undef TEXTTAB
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTIGRAPH
    #undef TEXTCHAR
    #undef TEXTWORD
}

void text_bounds(const char *str, int &width, int &height, int maxwidth)
{
    #define TEXTINDEX(idx)
    #define TEXTTAB(idx) TEXTSETCOLUMN
    #define TEXTWHITE(idx)
    #define TEXTLINE(idx) if(x > width) width = x;
    #define TEXTCOLOR(idx)
    #define TEXTIGRAPH(idx) x += FONTH + 1;
    #define TEXTCHAR(idx) x += curfont->chars[c-curfont->skip].w + 1;
    #define TEXTWORD x += w + 1;
    width = 0;
    TEXTSKELETON
    height = y + FONTH;
    TEXTLINE(_)
    #undef TEXTINDEX
    #undef TEXTTAB
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTIGRAPH
    #undef TEXTCHAR
    #undef TEXTWORD
}

void draw_text(const char *str, int left, int top, int r, int g, int b, int a, int cursor, int maxwidth)
{
#define TEXTINDEX(idx) if(idx == cursor) { cx = x; cy = y; cc = str[idx]; }
#define TEXTTAB(idx) TEXTGETCOLUMN
#define TEXTWHITE(idx)
#define TEXTLINE(idx)
#define TEXTCOLOR(idx) text_color(str[idx], colorstack, sizeof(colorstack), colorpos, color, a);
#define TEXTIGRAPH(idx) queue_igraph((uchar)str[idx], left+x, top+y), x += FONTH + 1;
#define TEXTCHAR(idx) x += draw_char(c, left+x, top+y) + 1;
#define TEXTWORD TEXTWORDSKELETON
    char colorstack[10];
    bvec color(r, g, b);
    int colorpos = 0, cx = INT_MIN, cy = 0, cc = ' ';
    colorstack[0] = 'c'; //indicate user color
    igraphbatch.setsize(0);
    glBlendFunc(GL_SRC_ALPHA, curfont->tex->bpp==32 ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE);
    glBindTexture(GL_TEXTURE_2D, curfont->tex->id);
    glBegin(GL_QUADS);
    glColor4ub(color.x, color.y, color.z, a);
    TEXTSKELETON
    glEnd();
    if(cursor >= 0)
    {
        if(cx == INT_MIN) { cx = x; cy = y; }
        if(maxwidth != -1 && cx >= maxwidth) { cx = 0; cy += FONTH; }
        int cw = curfont->chars.inrange(cc-33) ? curfont->chars[cc-33].w + 1 : curfont->defaultw;
        rendercursor(left+cx, top+cy, cw);
    }
    render_igraphs();
#undef TEXTINDEX
#undef TEXTTAB
#undef TEXTWHITE
#undef TEXTLINE
#undef TEXTCOLOR
#undef TEXTIGRAPH
#undef TEXTCHAR
#undef TEXTWORD
}

void reloadfonts()
{
    enumerate(fonts, font, f,
        if(!reloadtexture(*f.tex)) fatal("failed to reload font texture");
    );
}

void cutcolorstring(char *text, int maxlen)
{ // limit string length, ignore color codes
    if(!curfont) return;
    int len = 0;
    maxlen *= curfont->defaultw;
    while(*text)
    {
        if(*text == '\f' && text[1]) text++;
        else if(*text == '\1' && text[1]) text++, len += FONTH + 1;
        else if(*text == '\t') len = TABALIGN(len);
        else len += curfont->chars.inrange(*text - curfont->skip) ? curfont->chars[*text - curfont->skip].w : curfont->defaultw;
        if(len > maxlen) { *text = '\0'; break; }
        text++;
    }
}

bool filterunrenderables(char *s)
{
    bool res = false;
    char *d = s;
    while(*s)
    {
        if(!curfont->chars.inrange(*s - curfont->skip) && *s != ' ') res = true;
        else *d++ = *s;
        s++;
    }
    return res;
}

// animated inlined graphics

VAR(igraphdefaultframetime, 5, 200, 2000);
VARP(hideigraphs, 0, 0, 1);
#define IGRAPHPATH "packages" PATHDIVS "misc" PATHDIVS "igraph" PATHDIVS
hashtable<const char *, igraph> igraphs;    // keyed by shorthand
hashtable<const char *, char> igraphsi;     // known filenames
vector<igraph *> usedigraphs;               // char codes ('\1' + n)

void addigraph(char *fname)
{
    char *b, *s = newstring(fname), *mnem = strtok_r(s, "_", &b), *r;
    if(mnem && *mnem && !igraphs.access(mnem))
    { // new mnem
        defformatstring(filename)("%s%s.png", IGRAPHPATH, fname);
        Texture *tex = textureload(filename);
        if(tex != notexture && tex->xs && tex->ys)
        {
            igraph &ig = igraphs[newstring(mnem)];
            ig.fname = fname;
            ig.tex = tex;
            ig.used = !mnem[1] && *mnem >= '1' && *mnem <= '9' ? *mnem - '0' : 0;
            if(ig.used && usedigraphs.inrange(ig.used)) usedigraphs[ig.used] = &ig;     // hardcode "1".."9"
            while((r = strtok_r(NULL, "_", &b))) ig.frames.add(max(5, (int)ATOI(r)));
            if(ig.frames.empty()) ig.frames.add(igraphdefaultframetime);
            while(ig.frames.length() < tex->xs / tex->ys) ig.frames.add(ig.frames.last());
            loopv(ig.frames) if(i) ig.frames[i] += ig.frames[i - 1];
#ifdef _DEBUG
            clientlogf(" loaded igraph \"%s\", short \"%s\", %dx%d, %d frame%s", fname, mnem, tex->xs, tex->ys, ig.frames.length(), ig.frames.length() > 1 ? "s" : "");
#endif
        }
    }
    igraphsi[fname] = 0;
    delstring(s);
}

void updateigraphs()    // read all filenames and parse new ones (also preloads the textures)
{
    vector<char *> files;
    while(usedigraphs.length() <= (int)'\n') usedigraphs.add(NULL); // allocate hardcoded slots and skip slot 10 ('\n')
    listfiles(IGRAPHPATH, "png", files);
    loopvrev(files)
    {
        if(igraphsi.access(files[i])) delstring(files[i]);
        else addigraph(files[i]);
    }
}
COMMAND(updateigraphs, "");

igraph *getusedigraph(int i)
{
    if(usedigraphs.inrange(i)) return usedigraphs[i];
    return NULL;
}

int getigraph(const char *mnem)
{
    igraph *ig = igraphs.access(mnem);
    if(ig)
    {
        if(!ig->used && usedigraphs.length() < 255)
        {
            ig->used = usedigraphs.length();
            usedigraphs.add(ig);
        }
        return ig->used;
    }
    return 0;
}

void _getigraph(char *s)
{
    uchar res[3] = { '\1', (uchar)getigraph(s), '\0' };
    result(res[1] ? (char*)res : "");
}
COMMANDN(getigraph, _getigraph, "s");

void encodeigraphs(char *d, const char *s, int len) // find known igraphs in a string and substitute with rendercodes, only used for console messages
{
    if(hideigraphs) copystring(d, s, len);
    else
    {
        len--;
        string mnem;
        loopi(len)
        {
            if(*s == ':' && (!i || isspace(s[-1])) && i < len - 1)
            {
                int l = strcspn(s + 1, " \t\n."), u;
                if(l && l < MAXSTRLEN)
                {
                    copystring(mnem, s + 1, l + 1);
                    if((u = getigraph(mnem)))
                    { // found one: encode
                        *d++ = '\1';
                        *d++ = u;
                        s += l + 1;
                        i++;
                        continue;
                    }
                }
            }
            *d++ = *s++;
        }
        *d = '\0';
    }
}

void enumigraphs(vector<const char *> &igs, const char *s, int len)
{
    enumeratek(igraphs, const char *, mnem, if(!strncasecmp(mnem, s, len)) igs.add(mnem));
}
