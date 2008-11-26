// rendertext.cpp: font rendering

#include "pch.h"
#include "cube.h"

int VIRTW;

static hashtable<const char *, font> fonts;
static font *fontdef = NULL;

font *curfont = NULL;

void newfont(char *name, char *tex, char *defaultw, char *defaulth, char *offsetx, char *offsety, char *offsetw, char *offseth)
{
    font *f = fonts.access(name);
    if(!f)
    {
        name = newstring(name);
        f = &fonts[name];
        f->name = name;
    }

    f->tex = textureload(tex);
    f->chars.setsize(0);
    f->defaultw = ATOI(defaultw);
    f->defaulth = ATOI(defaulth);
    f->offsetx = ATOI(offsetx);
    f->offsety = ATOI(offsety);
    f->offsetw = ATOI(offsetw);
    f->offseth = ATOI(offseth);
    f->skip = 33;

    fontdef = f;
}

void fontchar(int x, int y, int w, int h)
{
    if(!fontdef) return;

    font::charinfo &c = fontdef->chars.add();
    c.x = x;
    c.y = y;
    c.w = w ? w : fontdef->defaultw;
    c.h = h ? h : fontdef->defaulth;
}

void fontskip(int n)
{
    if(!fontdef) return;

    fontdef->skip = n;
}

COMMANDN(font, newfont, ARG_8STR);
COMMAND(fontchar, ARG_4INT);
COMMAND(fontskip, ARG_1INT);

bool setfont(const char *name)
{
    font *f = fonts.access(name);
    if(!f) return false;
    curfont = f;
    return true;
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
    s_sprintfdlv(str, top, fstr);
    draw_text(str, left, top);
}

static int draw_char(int c, int x, int y)
{
    font::charinfo &info = curfont->chars[c-curfont->skip];
    float tc_left    = (info.x + curfont->offsetx) / float(curfont->tex->xs);
    float tc_top     = (info.y + curfont->offsety) / float(curfont->tex->ys);
    float tc_right   = (info.x + info.w + curfont->offsetw) / float(curfont->tex->xs);
    float tc_bottom  = (info.y + info.h + curfont->offseth) / float(curfont->tex->ys);

    glTexCoord2f(tc_left,  tc_top   ); glVertex2f(x,          y);
    glTexCoord2f(tc_right, tc_top   ); glVertex2f(x + info.w, y);
    glTexCoord2f(tc_right, tc_bottom); glVertex2f(x + info.w, y + info.h);
    glTexCoord2f(tc_left,  tc_bottom); glVertex2f(x,          y + info.h);

    xtraverts += 4;
    return info.w;
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
        else stack[sp] = c;
        switch(c)
        {
            case '0': color = bvec(64,  255, 128); break;   // green: player talk
            case '1': color = bvec(96,  160, 255); break;   // blue: team chat
            case '2': color = bvec(255, 192, 64);  break;   // yellow: gameplay action messages, only actions done by players
            case '3': color = bvec(255, 64,  64);  break;   // red: important errors and notes
            case '4': color = bvec(128, 128, 128); break;   // gray
            case '5': color = bvec(255, 255, 255); break;   // white
            case '6': color = bvec(96, 48, 0);     break;   // dark brown
            case '7': color = bvec(128, 48,  48);  break;   // dark red: dead admin
            // white (provided color): everything else
        }
        glColor4ub(color.x, color.y, color.z, a);
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
        else if(curfont->chars.inrange(c-curfont->skip))\
        {\
            if(maxwidth != -1)\
            {\
                int j = i;\
                int w = curfont->chars[c-curfont->skip].w;\
                for(; str[i+1]; i++)\
                {\
                    int c = str[i+1];\
                    if(c=='\f') { if(str[i+2]) i++; continue; }\
                    if(i-j > 16) break;\
                    if(!curfont->chars.inrange(c-curfont->skip)) break;\
                    int cw = curfont->chars[c-curfont->skip].w + 1;\
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
                    else { TEXTCHAR(j) }\
                }

int text_visible(const char *str, int hitx, int hity, int maxwidth)
{
    #define TEXTINDEX(idx)
    #define TEXTTAB(idx) TEXTGETCOLUMN
    #define TEXTWHITE(idx) if(y+FONTH > hity && x >= hitx) return idx;
    #define TEXTLINE(idx) if(y+FONTH > hity) return idx;
    #define TEXTCOLOR(idx)
    #define TEXTCHAR(idx) x += curfont->chars[c-curfont->skip].w+1; TEXTWHITE(idx)
    #define TEXTWORD TEXTWORDSKELETON
    TEXTSKELETON
    #undef TEXTINDEX
    #undef TEXTTAB
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
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
    #define TEXTCHAR(idx) x += draw_char(c, left+x, top+y)+1;
    #define TEXTWORD TEXTWORDSKELETON
    char colorstack[10];
    bvec color(r, g, b);
    int colorpos = 0, cx = INT_MIN, cy = 0, cc = ' ';
    colorstack[0] = 'c'; //indicate user color
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
        int cw = curfont->chars.inrange(cc-curfont->skip) ? curfont->chars[cc-curfont->skip].w + 1 : curfont->defaultw;
        rendercursor(left+cx, top+cy, cw);
    }
    #undef TEXTINDEX
    #undef TEXTTAB
    #undef TEXTWHITE
    #undef TEXTLINE
    #undef TEXTCOLOR
    #undef TEXTCHAR
    #undef TEXTWORD
}

void reloadfonts()
{
    enumerate(fonts, font, f,
        if(!reloadtexture(*f.tex)) fatal("failed to reload font texture");
    );
}

