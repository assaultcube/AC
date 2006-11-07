// console.cpp: the console buffer, its display, and command line control

#include "cube.h"
#include <ctype.h>

struct cline { char *cref; int outtime; };
vector<cline> conlines;

const int ndraw = 6;
const int WORDWRAP = 80;
int conskip = 0;

bool saycommandon = false;
string commandbuf;

void setconskip(int n)
{
    conskip += n;
    if(conskip<0) conskip = 0;
};

COMMANDN(conskip, setconskip, ARG_1INT);

void conline(const char *sf, bool highlight)        // add a line to the console buffer
{
    cline cl;
    cl.cref = conlines.length()>100 ? conlines.pop().cref : newstringbuf("");   // constrain the buffer size
    cl.outtime = lastmillis;                        // for how long to keep line on screen
    conlines.insert(0,cl);
    if(highlight)                                   // show line in a different colour, for chat etc.
    {
        cl.cref[0] = '\f';
        cl.cref[1] = 0;
        s_strcat(cl.cref, sf);
    }
    else
    {
        s_strcpy(cl.cref, sf);
    };
};

void conoutf(const char *s, ...)
{
    s_sprintfdv(sf, s);
    puts(sf);
    s = sf;
    int n = 0, visible;
    while((visible = text_visible(s, VIRTW*4/3 - FONTH))) // cut strings to fit on screen
    {
        const char *newline = (const char *)memchr(s, '\n', visible);
        if(newline) visible = newline+1-s;
        string t;
        s_strncpy(t, s, visible+1);
        conline(t, n++!=0);
        s += visible;
    };
};

bool fullconsole = false;
void toggleconsole() { fullconsole = !fullconsole; };
COMMAND(toggleconsole, ARG_NONE);

void renderconsole()                                // render buffer taking into account time & scrolling
{
    if(fullconsole)
    {
        int w = VIRTW*4/3, h = VIRTH*4/3;
        int numl = h/3/FONTH;
        int offset = min(conskip, max(conlines.length() - numl, 0));
        blendbox(0, 0, w, (numl+1)*FONTH*5/4, true);
        loopi(numl) draw_text(offset+i>=conlines.length() ? "" : conlines[offset+i].cref, FONTH/3, (FONTH*5/4)*(numl-i-1)+FONTH/3);
    }
    else
    {
        int nd = 0;
        char *refs[ndraw];
        loopv(conlines) if(conskip ? i>=conskip-1 || i>=conlines.length()-ndraw : lastmillis-conlines[i].outtime<20000)
        {
            refs[nd++] = conlines[i].cref;
            if(nd==ndraw) break;
        };
        loopj(nd) draw_text(refs[j], FONTH/3, (FONTH*5/4)*(nd-j-1)+FONTH/3);
    };
};

// keymap is defined externally in keymap.cfg

struct keym { int code; char *name; char *action; } keyms[256];
int numkm = 0;                                     

void keymap(char *code, char *key, char *action)
{
    keyms[numkm].code = atoi(code);
    keyms[numkm].name = newstring(key);
    keyms[numkm++].action = newstringbuf(action);
};

COMMAND(keymap, ARG_3STR);

void bindkey(char *key, char *action)
{
    for(char *x = key; *x; x++) *x = toupper(*x);
    loopi(numkm) if(strcmp(keyms[i].name, key)==0)
    {
        s_strcpy(keyms[i].action, action);
        return;
    };
    conoutf("unknown key \"%s\"", key);   
};

COMMANDN(bind, bindkey, ARG_2STR);

void saycommand(char *init)                         // turns input to the command line on or off
{
    SDL_EnableUNICODE(saycommandon = (init!=NULL));
	setscope(false);
    if(!editmode) keyrepeat(saycommandon);
    if(!init) init = "";
    s_strcpy(commandbuf, init);
};

void mapmsg(char *s) { s_strncpy(hdr.maptitle, s, 128); };

COMMAND(saycommand, ARG_VARI);
COMMAND(mapmsg, ARG_1STR);

#if !defined(WIN32) && !defined(__APPLE__)
#include <X11/Xlib.h>
#include <SDL_syswm.h>
#endif

void pasteconsole()
{
    #ifdef WIN32
    if(!IsClipboardFormatAvailable(CF_TEXT)) return; 
    if(!OpenClipboard(NULL)) return;
    char *cb = (char *)GlobalLock(GetClipboardData(CF_TEXT));
    s_strcat(commandbuf, cb);
    GlobalUnlock(cb);
    CloseClipboard();
    #elif !defined(__APPLE__)
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version); 
    wminfo.subsystem = SDL_SYSWM_X11;
    if(!SDL_GetWMInfo(&wminfo)) return;
    int cbsize;
    char *cb = XFetchBytes(wminfo.info.x11.display, &cbsize);
    if(!cb || !cbsize) return;
    int commandlen = strlen(commandbuf);
    for(char *cbline = cb, *cbend; commandlen + 1 < _MAXDEFSTR && cbline < &cb[cbsize]; cbline = cbend + 1)
    {
        cbend = (char *)memchr(cbline, '\0', &cb[cbsize] - cbline);
        if(!cbend) cbend = &cb[cbsize];
        if(commandlen + cbend - cbline + 1 > _MAXDEFSTR) cbend = cbline + _MAXDEFSTR - commandlen - 1;
        memcpy(&commandbuf[commandlen], cbline, cbend - cbline);
        commandlen += cbend - cbline;
        commandbuf[commandlen] = '\n';
        if(commandlen + 1 < _MAXDEFSTR && cbend < &cb[cbsize]) ++commandlen;
        commandbuf[commandlen] = '\0';
    };
    XFree(cb);
    #endif
};

cvector vhistory;
int histpos = 0;

void history(int n)
{
    static bool rec = false;
    if(!rec && n>=0 && n<vhistory.length())
    {
        rec = true;
        execute(vhistory[vhistory.length()-n-1]);
        rec = false;
    };
};

COMMAND(history, ARG_1INT);

void keypress(int code, bool isdown, int cooked)
{
    if(saycommandon)                                // keystrokes go to commandline
    {
        if(isdown)
        {
            switch(code)
            {
                case SDLK_RETURN:
                    break;

                case SDLK_BACKSPACE:
                case SDLK_LEFT:
                {
                    for(int i = 0; commandbuf[i]; i++) if(!commandbuf[i+1]) commandbuf[i] = 0;
                    resetcomplete();
                    break;
                };
                    
                case SDLK_UP:
                    if(histpos) s_strcpy(commandbuf, vhistory[--histpos]);
                    break;
                
                case SDLK_DOWN:
                    if(histpos<vhistory.length()) s_strcpy(commandbuf, vhistory[histpos++]);
                    break;
                    
                case SDLK_TAB:
                    complete(commandbuf);
                    break;

                case SDLK_v:
                    if(SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL)) { pasteconsole(); return; };

                default:
                    resetcomplete();
                    if(cooked) { char add[] = { cooked, 0 }; s_strcat(commandbuf, add); };
            };
        }
        else
        {
            if(code==SDLK_RETURN)
            {
                if(commandbuf[0])
                {
                    if(vhistory.empty() || strcmp(vhistory.last(), commandbuf))
                    {
                        vhistory.add(newstring(commandbuf));  // cap this?
                    };
                    histpos = vhistory.length();
                    if(commandbuf[0]=='/') execute(commandbuf, true);
                    else toserver(commandbuf);
                };
                saycommand(NULL);
            }
            else if(code==SDLK_ESCAPE)
            {
                saycommand(NULL);
            };
        };
    }
    else if(!menukey(code, isdown))                 // keystrokes go to menu
    {
        loopi(numkm) if(keyms[i].code==code)        // keystrokes go to game, lookup in keymap and execute
        {
            string temp;
            s_strcpy(temp, keyms[i].action);
            execute(temp, isdown); 
            return;
        };
    };
};

char *getcurcommand()
{
    return saycommandon ? commandbuf : NULL;
};

