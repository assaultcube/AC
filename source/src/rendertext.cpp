// rendertext.cpp: font rendering

#include "cube.h"

int VIRTW;
bool ignoreblinkingbit = false; // for remote-n-temp override of '\fb'
static hashtable<const char *, font> fonts;
static font *fontdef = NULL;

font *curfont = NULL;

VARP(allowblinkingtext, 0, 0, 1); // if you're so inclined
VARP(__fontsetting, 0, 0, 2);

void newfont(char *name, char *tex, int *defaultw, int *defaulth, int *offsetx, int *offsety, int *offsetw, int *offseth)
{
    font *f = fonts.access(name);
    if(!f)
    {
        name = newstring(name);
        f = &fonts[name];
        f->name = name;
    }

    f->tex = textureload(tex);
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

extern GLenum texformat(int bpp);

void fontchar(int *x, int *y, int *w, int *h)
{
    if(!fontdef) return;

    font::charinfo &c = fontdef->chars.add();
    c.x = *x;
    c.y = *y;
    c.w = *w ? *w : fontdef->defaultw;
    c.h = *h ? *h : fontdef->defaulth;
}

void fontskip(int *n)
{
    if(!fontdef) return;

    fontdef->skip = *n;
}

COMMANDN(font, newfont, "ssiiiiii");
COMMAND(fontchar, "iiii");
COMMAND(fontskip, "i");

string myfont = "default";
void newsetfont(const char *name)
{
    if ( setfont(name) ) copystring(myfont,name);
}

bool setfont(const char *name)
{
    font *f = fonts.access(name);
    if(!f) return false;
    int v = -1;
    if(strcmp(name, "default")==0)
        v = 0;
    else if(strcmp(name, "serif")==0)
        v = 1;
    else if(strcmp(name, "mono")==0)
        v = 2;
    if(v!=-1) __fontsetting = v;
    curfont = f;
    return true;
}
COMMANDN(setfont, newsetfont, "s");

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



extern int shdsize, outline, win32msg;


#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define RMASK 0xff000000
#define GMASK 0x00ff0000
#define BMASK 0x0000ff00
#define AMASK 0x000000ff
#else
#define RMASK 0x000000ff
#define GMASK 0x0000ff00
#define BMASK 0x00ff0000
#define AMASK 0xff000000
#endif
/*
// ringbuf for utf8 character storage
struct charringbuf : ringbuf<font::utf8charinfo, 32>
{
    // find by character code
    int findbycharcode(int code)
    {
        loopi(len)
        {
            if(data[i].code == code)
                return i;
        }

        return -1;
    }
};


TTF_Font *ttffont = NULL;
font utf8font;
charringbuf utf8chars;

void initfont()
{
    static bool initialized = false;
    if(!initialized)
    {
        TTF_Init();

        int fsize = 64;
        const char *fontname = "packages/misc/font.ttf";
        ttffont = TTF_OpenFont(findfile(path(fontname, true), "r"), fsize);

        utf8font.defaulth = 0;
        utf8font.defaultw = 0;
        utf8font.offsetw = 0;
        utf8font.offseth = 10;
        utf8font.offsetx = 0;
        utf8font.offsety = 0;

        initialized = true;
    }
}

void createutf8charset()
{
    int isize = 512;

    SDL_Surface *charsetsurface = SDL_CreateRGBSurface(SDL_SWSURFACE, isize, isize, 32, RMASK, GMASK, BMASK, AMASK);

    if(charsetsurface)
    {
        SDL_Color color;
        color.r = 255;
        color.g = 255;
        color.b = 255;

        int posx = 0;
        int posy = 0;

        loopv(utf8chars)
        {
            font::utf8charinfo &charinfo = utf8chars[i];
            int code = charinfo.code;

            char u[5] = {0,0,0,0,0};
            utf8::append(code, u);

            SDL_Surface *fontsurface = TTF_RenderUTF8_Blended(ttffont, u, color);

            // update row/column info
            if(posx + fontsurface->w > charsetsurface->w)
            {
                posx = 0;
                posy += fontsurface->h; // fixme
            }
            if(posy + fontsurface->h > charsetsurface->h)
                break;

            // blit onto charset
            SDL_SetAlpha(fontsurface, 0, 0);
            blitsurface(charsetsurface, fontsurface, posx, posy);

            // update charinfo properties
            charinfo.x = posx;
            charinfo.y = posy;
            charinfo.w = fontsurface->w;
            charinfo.h = fontsurface->h;

            posx += fontsurface->w;
            SDL_FreeSurface(fontsurface);
        }

        utf8font.tex = createtexturefromsurface("utf8charset", charsetsurface);

        //extern void savepng(SDL_Surface *s, const char *name);
        //savepng(t, "font.png");

        SDL_FreeSurface(charsetsurface);
    }
}

void addutf8char(int code)
{
    // add to buf
    font::utf8charinfo charinfo;
    charinfo.code = code;
    charinfo.x = charinfo.y = charinfo.w = charinfo.h = 0;
    utf8chars.add(charinfo);

    // update charset
    createutf8charset();
}

font::charinfo *loadchar(int code)
{
    int idx = utf8chars.findbycharcode(code);
    if(idx >= 0)
        return &utf8chars[idx];

    // add
    addutf8char(code);

    idx = utf8chars.findbycharcode(code);
    return &utf8chars[idx];
}
*/
int draw_char(font &f, font::charinfo &info, int charcode, int x, int y)
{
/*
    // fixme
    glEnd();
    glBindTexture(GL_TEXTURE_2D, f.tex->id);
    glBegin(GL_QUADS);
*/
    float tc_left    = (info.x + f.offsetx) / float(f.tex->xs);
    float tc_top     = (info.y + f.offsety) / float(f.tex->ys);
    float tc_right   = (info.x + info.w + f.offsetw) / float(f.tex->xs);
    float tc_bottom  = (info.y + info.h + f.offseth) / float(f.tex->ys);

    glTexCoord2f(tc_left,  tc_top   ); glVertex2f(x,          y);
    glTexCoord2f(tc_right, tc_top   ); glVertex2f(x + info.w, y);
    glTexCoord2f(tc_right, tc_bottom); glVertex2f(x + info.w, y + info.h);
    glTexCoord2f(tc_left,  tc_bottom); glVertex2f(x,          y + info.h);

    xtraverts += 4;
    return info.w;
}

/*
// fixme
font::charinfo &getcharinfo(int c)
{
    if(curfont->chars.inrange(c-curfont->skip))
    {
        font::charinfo &info = curfont->chars[c-curfont->skip];
        return info;
    }
    //else { font::charinfo &info = *loadchar(c); return info; }
    //return NULL;
    font::charinfo &info = curfont->chars[0]; // 0 || (FONTCHARS-1)
    return info;
}
*/

static int draw_char(int c, int x, int y)
{
    if(curfont->chars.inrange(c-curfont->skip))
    {
        font::charinfo &info = curfont->chars[c-curfont->skip];

        return draw_char(*curfont, info, c, x, y);
    }
    /*
    else
    {
        // fixme
        glEnd();
        font::charinfo &info = *loadchar(c);
        glBegin(GL_QUADS);

        return draw_char(utf8font, info, c, x, y);
    }
    */
    return 0;
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
            //extendeded color palette
            //case 'a': case 'A':color = bvec( 0xFF, 0xCC, 0xCC); break;   // some lowercase seem to have special meaning like 'b' (flashing text) so not yet using them
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
            // white (provided color): everything else
            //default: color = bvec( 255, 255, 255 ); break;
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

/** This is the 1.0.4 function
    It will substituted by draw_text_wip
    I am putting this temporarily here because it is impossible to test without colours : Brahma */
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
        int cw = curfont->chars.inrange(cc-33) ? curfont->chars[cc-33].w + 1 : curfont->defaultw;
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

/* WIP ALERT */
/*
void draw_text_wip(const char *str, int left, int top, int r, int g, int b, int a, int cursor, int maxwidth)
{
    char colorstack[10];
    bvec color(r, g, b);
    int colorpos = 0, cx = INT_MIN, cy = 0, cc = ' ';
    colorstack[0] = 'c'; //indicate user color

    glBlendFunc(GL_SRC_ALPHA, curfont->tex->bpp==32 ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE);
    glBindTexture(GL_TEXTURE_2D, curfont->tex->id);

    glBegin(GL_QUADS);
    glColor4ub(color.x, color.y, color.z, a);

    std::string text(str);
    std::string::iterator begin = text.begin();
    std::string::iterator end = text.end();
    std::string::iterator cursoriter = end;
    if(cursor >= 0 && cursor < utf8::distance(begin, end))
    {
        cursoriter = begin;
        utf8::advance(cursoriter, cursor, end);
    }

    int y = 0, x = 0, col = 0, colx = 0;

    for(std::string::iterator iter = text.begin(); iter != text.end(); utf8::next(iter, text.end()))
    {
        int c = utf8::peek_next(iter, text.end());

        if(iter == cursoriter)
        {
            cx = x;
            cy = y;
            cc = c;
        }

        if(c=='\t')
        {
            if(columns && col<columns->length())
            {
                colx += (*columns)[col++];
                x = colx;
            }
            else x = TABALIGN(x);
        }
        else if(c==' ')
        {
            x += curfont->defaultw;
        }
        else if(c=='\n')
        {
            x = 0;
            y += FONTH;
        }
        else if(c=='\f')
        {
            std::string::iterator test = iter;
            test++;
            if(test != end)
            {
                c = utf8::next(iter, end);
                text_color(c, colorstack, sizeof(colorstack), colorpos, color, a);
            }
        }
        else if(c=='\a')
        {
            std::string::iterator next = iter;
            next++;
            if(next != end)
            {
                iter++;
            }

        }
        else if(curfont->chars.inrange(c-curfont->skip))
        {
            font::charinfo &cinfo = getcharinfo(c);

            if(maxwidth != -1)
            {
                std::string::iterator next = iter;
                int w = cinfo.w;

                do
                {
                    std::string::iterator test = iter;
                    int c = utf8::next(test, end);
                    if(test == end) break;

                    if(c=='\f')
                    {
                        std::string::iterator test = iter;
                        utf8::advance(test, 2, end);
                        if(test == end) break;
                        utf8::next(iter, end);
                        continue;
                    }
                    if(utf8::distance(iter, next) > 16) break;
                    //if(!curfont->chars.inrange(c-curfont->skip)) fixme
                    if(c < curfont->skip) // fixme
                    {
                        break;
                    }
                    int cw = getcharinfo(c).w + 1;
                    if(w + cw >= maxwidth) break;
                    w += cw;

                    utf8::next(iter, end);

                } while(true);

                if(x + w >= maxwidth && next != begin) //fixme
                {
                    x = 0;
                    y += FONTH;
                }

                for(; next <= iter && next != end; )
                {
                    int c = utf8::peek_next(next, end);
                    if(next == cursoriter) { cx = x; cy = y; cc = c; }

                    if(c=='\f')
                    {
                        std::string::iterator test = next;
                        utf8::next(test, end);
                        if(test != end)
                        {
                            c = utf8::next(next, end);
                            text_color(c, colorstack, sizeof(colorstack), colorpos, color, a);
                        }
                    }
                    else
                    {
                        x += draw_char(c, left+x, top+y)+1;
                    }

                    utf8::next(next, end);
                }

            }
            else
            {
                x += draw_char(c, left+x, top+y)+1;
            }
        }
    }

    glEnd();
    if(cursor >= 0)
    {
        if(cx == INT_MIN) { cx = x; cy = y; }
        if(maxwidth != -1 && cx >= maxwidth) { cx = 0; cy += FONTH; }
        int cw = curfont->chars.inrange(cc-curfont->skip) ? curfont->chars[cc-curfont->skip].w + 1 : curfont->defaultw;
        rendercursor(left+cx, top+cy, cw);
    }
}
*/
void reloadfonts()
{
    //createutf8charset();

    enumerate(fonts, font, f,
        if(!reloadtexture(*f.tex)) fatal("failed to reload font texture");
    );
}
