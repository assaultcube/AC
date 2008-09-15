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

COMMANDN(font, newfont, ARG_8STR);
COMMANDN(fontchar, fontchar, ARG_4INT);

bool setfont(const char *name)
{
    font *f = fonts.access(name);
    if(!f) return false;
    curfont = f;
    return true;
}

int char_width(int c, int x)
{
    if(!curfont) return x;
    else if(c=='\t') x = (x+PIXELTAB)/PIXELTAB*PIXELTAB;
    else if(c==' ') x += curfont->defaultw;
    else if(curfont->chars.inrange(c-33))
    {
        c -= 33;
        x += curfont->chars[c].w + 1;
    }
    return x;
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

int text_width(const char *str, int limit)
{
    if(!str) return 0;
    int x = 0, col = 0, colx = 0;
    for(int i = 0; str[i] && (limit<0 || i<limit); i++)
    {
        switch(str[i])
        {
            case '\f':
                i++;
                break;

            case '\t':
                if(columns)
                {
                    while(col>=columns->length()) columns->add(0);
                    int w = char_width('\t', x) - colx;
                    w = max(w, (*columns)[col]);
                    (*columns)[col] = w;
                    col++;
                    colx += w;
                    x = colx;
                }
                else x = char_width('\t', x);
                break;

            default:
                x = char_width(str[i], x);
                break;
        }
    }
    return x;
}

int text_visible(const char *str, int max)
{
    int i = 0, x = 0;
    while(str[i])
    {
        if(str[i]=='\f')
        {
            i += 2;
            continue;
        }
        x = char_width(str[i], x);
        if(x > max) return i;
        ++i;
    }
    return i;
}

// cut strings to fit on screen
void text_block(const char *str, int max, vector<char *> &lines)
{
    if(!str) return;
    int visible;
    while((visible = max ? text_visible(str, max) : (int)strlen(str)))
    {
        const char *newline = (const char *)memchr(str, '\n', visible);
        if(newline) visible = newline+1-str;
        else if(str[visible]) // wrap words
        {
            int v = visible;
            while(v > 0 && str[v] != ' ') v--;
            if(v) visible = v+1;
        }
        char *t = lines.add(newstring((size_t)visible));
        s_strncpy(t, str, visible+1);
        str += visible;
    }
}

void draw_textf(const char *fstr, int left, int top, ...)
{
    s_sprintfdlv(str, top, fstr);
    draw_text(str, left, top);
}

void draw_text(const char *str, int left, int top)
{
    if(!curfont) return;

    glBlendFunc(GL_ONE, GL_ONE);
    glBindTexture(GL_TEXTURE_2D, curfont->tex->id);

    static float colorstack[8][4];
    int colorpos = 0, x = left, y = top, col = 0, colx = 0;

    glBegin(GL_QUADS);
    // ATI bug -- initial color must be set after glBegin
    glColor3ub(255, 255, 255);
    for(int i = 0; str[i]; i++)
    {
        int c = str[i];
        switch(c)
        {
            case '\t':
                if(columns && col<columns->length())
                {
                    colx += (*columns)[col++];
                    x = left + colx;
                }
                else x = (x-left+PIXELTAB)/PIXELTAB*PIXELTAB+left;
                continue;

            case '\f':
                switch(str[++i])
		        {
			        case '0': glColor3ub(64,  255, 128); continue;   // green: player talk
                    case '1': glColor3ub(96,  160, 255); continue;   // blue: team chat
			        case '2': glColor3ub(255, 192, 64);  continue;   // yellow: gameplay action messages, only actions done by players
			        case '3': glColor3ub(255, 64,  64);  continue;   // red: important errors and notes
                    case '4': glColor3ub(128, 128, 128); continue;   // gray
			        case '5': glColor3ub(255, 255, 255); continue;   // white: everything else
                    case '6': glColor3ub(96, 48, 0);     continue;   // dark brown
                    case '7': glColor3ub(128, 48,  48);  continue;   // dark red: dead admin
                    case 's': // save color
                        if((size_t)colorpos<sizeof(colorstack)/sizeof(colorstack[0]))
                        {
                            glEnd();
                            glGetFloatv(GL_CURRENT_COLOR, colorstack[colorpos++]);
                            glBegin(GL_QUADS);
                        }
                        continue;
                    case 'r': // restore color
                        if(colorpos>0)
                            glColor4fv(colorstack[--colorpos]);
                        continue;
                    default: continue;
		        }

            case ' ':
                x += curfont->defaultw;
                continue;
        }

        c -= 33;
        if(!curfont->chars.inrange(c)) continue;

        font::charinfo &info = curfont->chars[c];
        float tc_left    = (info.x + curfont->offsetx) / float(curfont->tex->xs);
        float tc_top     = (info.y + curfont->offsety) / float(curfont->tex->ys);
        float tc_right   = (info.x + info.w + curfont->offsetw) / float(curfont->tex->xs);
        float tc_bottom  = (info.y + info.h + curfont->offseth) / float(curfont->tex->ys);

        glTexCoord2f(tc_left,  tc_top   ); glVertex2f(x,          y);
        glTexCoord2f(tc_right, tc_top   ); glVertex2f(x + info.w, y);
        glTexCoord2f(tc_right, tc_bottom); glVertex2f(x + info.w, y + info.h);
        glTexCoord2f(tc_left,  tc_bottom); glVertex2f(x,          y + info.h);

        xtraverts += 4;
        x += info.w + 1;
    }
    glEnd();
}

void reloadfonts()
{
    enumerate(fonts, font, f,
        if(!reloadtexture(*f.tex)) fatal("failed to reload font texture");
    );
}

