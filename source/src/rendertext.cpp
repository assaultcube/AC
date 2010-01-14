// rendertext.cpp: font rendering

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

extern GLenum texformat(int bpp);

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

int draw_char(font &f, font::charinfo &info, int charcode, int x, int y)
{
	// fixme
	glEnd();
	glBindTexture(GL_TEXTURE_2D, f.tex->id);
	glBegin(GL_QUADS);

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

// fixme
font::charinfo &getcharinfo(int c)
{
	if(curfont->chars.inrange(c-curfont->skip))
	{
		font::charinfo &info = curfont->chars[c-curfont->skip];
		return info;
	}
	else
	{
		font::charinfo &info = *loadchar(c);
		return info;
	}
}

static int draw_char(int c, int x, int y)
{
	if(curfont->chars.inrange(c-curfont->skip))
	{
		font::charinfo &info = curfont->chars[c-curfont->skip];

		return draw_char(*curfont, info, c, x, y);
	}
	else
	{
		// fixme
		glEnd();
		font::charinfo &info = *loadchar(c);
		glBegin(GL_QUADS);

		return draw_char(utf8font, info, c, x, y);
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
        else if(c == 'b') stack[sp] *= -1;
        else stack[sp] = c;
        switch(abs(stack[sp]))
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
        int b = (int) (sinf(lastmillis / 200.0f) * 115.0f);
        b = stack[sp] > 0 ? 100 : min(abs(b), 100);
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

	std::string text(str);
	std::string::iterator begin = text.begin();
	std::string::iterator end = text.end();
	std::string::iterator cursoriter = end;
	if(cursor >= 0 && cursor < utf8::distance(begin, end))
	{
		cursoriter = begin;
		utf8::advance(cursoriter, cursor, end);
	}

	/* WIP ALERT */

    int y = 0, x = 0, col = 0, colx = 0;
	
	for(std::string::iterator iter = text.begin(); iter != text.end(); utf8::next(iter, text.end()))
    {
        //TEXTINDEX(i)
		//if(idx == cursor) { cx = x; cy = y; cc = str[idx]; }

		int c = utf8::peek_next(iter, text.end());
		//int c = str[i];

		if(iter == cursoriter) 
		{ 
			cx = x; 
			cy = y; 
			cc = c; 
		}

        if(c=='\t')      
		{ 
			//TEXTTAB(i); 
			if(columns && col<columns->length()) 
			{
				colx += (*columns)[col++];
				x = colx;
			}
			else x = TABALIGN(x);

			//TEXTWHITE(i) 
		}
        else if(c==' ')  
		{ 
			x += curfont->defaultw; 
			//TEXTWHITE(i) 
		}
        else if(c=='\n') 
		{ 
			//TEXTLINE(i) 
			x = 0; 
			y += FONTH; 
		}
        else if(c=='\f') 
		{ 
			/*
			if(str[i+1]) 
			{ 
				i++; 
				//TEXTCOLOR(i) 
				text_color(str[i], colorstack, sizeof(colorstack), colorpos, color, a);
			}
			*/
			
			std::string::iterator test = iter;
			test++;
			if(test != end)
			{ 
				//TEXTCOLOR(i)
				c = utf8::next(iter, end);
				text_color(c, colorstack, sizeof(colorstack), colorpos, color, a);
			}
		}
        else if(c=='\a') 
		{ 
			/*
			if(str[i+1]) 
			{ 
				i++; 
			}
			*/
			
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
				/* ORIGINAL, UNWRAPPED CPP TEMPLATES
                int j = i;
                int w = cinfo.w; //curfont->chars[c-curfont->skip].w;

                for(; str[i+1]; i++)
                {
                    int c = str[i+1];
                    if(c=='\f') { if(str[i+2]) i++; continue; }
                    if(i-j > 16) break;
                    if(!curfont->chars.inrange(c-curfont->skip)) break;
                    int cw = curfont->chars[c-curfont->skip].w + 1;
                    if(w + cw >= maxwidth) break;
                    w += cw;
                }
                if(x + w >= maxwidth && j!=0) 
				{ 
					//TEXTLINE(j-1) 
					x = 0; y += FONTH; 
				}

                //TEXTWORD
                for(; j <= i; j++)
                {
                    //TEXTINDEX(j)
					if(j == cursor) { cx = x; cy = y; cc = str[j]; }
                    int c = str[j];
                    if(c=='\f') 
					{ 
						if(str[j+1]) 
						{ 
							j++; 
							//TEXTCOLOR(j) 
							text_color(str[j], colorstack, sizeof(colorstack), colorpos, color, a);
						}
					}
                    else 
					{ 
						//TEXTCHAR(j) 
						x += draw_char(c, left+x, top+y)+1;
					}
                }
				*/
			
				std::string::iterator next = iter;
                int w = cinfo.w; //curfont->chars[c-curfont->skip].w;

				do
                {
					std::string::iterator test = iter;
					int c = utf8::next(test, end);
					if(test == end) break;

                    if(c=='\f') 
					{ 
						std::string::iterator test = iter;
						utf8::advance(test, 2, end);
						if(test != end)
						{
							utf8::next(iter, end);
						}
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
					//int test = utf8::distance(next, iter);
					
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
