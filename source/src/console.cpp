// console.cpp: the console buffer, its display, and command line control

#include "cube.h"

struct cline { char *cref; int outtime; };
vector<cline> conlines;

const int ndraw = 6;
const int WORDWRAP = 80;
int conskip = 0;

bool saycommandon = false;
string commandbuf;
int commandpos = -1;

void setconskip(int n)
{
    conskip += n;
    if(conskip<0) conskip = 0;
}

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
        cl.cref[1] = '0';
        cl.cref[2] = 0;
        s_strcat(cl.cref, sf);
    }
    else
    {
        s_strcpy(cl.cref, sf);
    }
}

void conoutf(const char *s, ...)
{
    s_sprintfdv(sf, s);
    puts(sf);
    s = sf;
    int n = 0, visible;
    while((visible = text_visible(s, (VIRTW-VIRTH/6-10-FONTH)*2))) // cut strings to fit on screen
    {
        const char *newline = (const char *)memchr(s, '\n', visible);
        if(newline) visible = newline+1-s;
        string t;
        s_strncpy(t, s, visible+1);
        conline(t, n++!=0);
        s += visible;
    }
}

bool fullconsole = false;
void toggleconsole() { fullconsole = !fullconsole; }
COMMAND(toggleconsole, ARG_NONE);

void rendercommand(int x, int y)
{
    s_sprintfd(s)("> %s", commandbuf);
    int offset = text_width(s, commandpos>=0 ? commandpos+2 : -1);
    blendbox(x+offset, y, x+offset+char_width(commandpos>=0 ? commandbuf[commandpos] : '_'), y+FONTH, true);
    draw_text(s, x, y);
}

void renderconsole()                                // render buffer taking into account time & scrolling
{
    if(fullconsole)
    {
        int w = VIRTW*4/3, h = VIRTH*4/3;
        int numl = h/2/(FONTH*5/4);
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
        }
        loopj(nd) draw_text(refs[j], FONTH/3, (FONTH*5/4)*(nd-j-1)+FONTH/3);
    }
}

// keymap is defined externally in keymap.cfg

struct keym
{
    int code;
    char *name, *action;

    ~keym() { DELETEA(name); DELETEA(action); }
};
vector<keym> keyms;

keym *keypressed = NULL;
char *keyaction = NULL;

void keymap(char *code, char *key, char *action)
{
    keym &km = keyms.add();
    km.code = atoi(code);
    km.name = newstring(key);
    km.action = newstring(action);
}

COMMAND(keymap, ARG_3STR);

void bindkey(char *key, char *action)
{
    for(char *x = key; *x; x++) *x = toupper(*x);
    loopv(keyms) if(!strcmp(keyms[i].name, key))
    {
        keym &km = keyms[i];
        if(!keypressed || keyaction!=km.action) delete[] km.action;
        km.action = newstring(action);
        return;
    }
    conoutf("unknown key \"%s\"", key);   
}

COMMANDN(bind, bindkey, ARG_2STR);

struct releaseaction
{
    keym *key;
    char *action;
};
vector<releaseaction> releaseactions;

char *addreleaseaction(char *s)
{
    if(!keypressed) return NULL;
    releaseaction &ra = releaseactions.add();
    ra.key = keypressed;
    ra.action = newstring(s);
    return keypressed->name;
}

void onrelease(char *s)
{
    addreleaseaction(s);
}

COMMAND(onrelease, ARG_1STR);

void saycommand(char *init)                         // turns input to the command line on or off
{
    SDL_EnableUNICODE(saycommandon = (init!=NULL));
	setscope(false);
    if(!editmode) keyrepeat(saycommandon);
    if(!init) init = "";
    s_strcpy(commandbuf, init);
    commandpos = -1;
}

void mapmsg(char *s) { s_strncpy(hdr.maptitle, s, 128); }

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
    }
    XFree(cb);
    #endif
}

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
    }
}

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
                case SDLK_KP_ENTER:
                    break;

                case SDLK_DELETE:
                {
                    int len = (int)strlen(commandbuf);
                    if(commandpos<0) break;
                    memmove(&commandbuf[commandpos], &commandbuf[commandpos+1], len - commandpos);
                    resetcomplete();
                    if(commandpos>=len-1) commandpos = -1;
                    break;
                }

                case SDLK_BACKSPACE:
                {
                    int len = (int)strlen(commandbuf), i = commandpos>=0 ? commandpos : len;
                    if(i<1) break;
                    memmove(&commandbuf[i-1], &commandbuf[i], len - i + 1);
                    resetcomplete();
                    if(commandpos>0) commandpos--;
                    else if(!commandpos && len<=1) commandpos = -1;
                    break;
                }

                case SDLK_LEFT:
                    if(commandpos>0) commandpos--;
                    else if(commandpos<0) commandpos = (int)strlen(commandbuf)-1;
                    break;

                case SDLK_RIGHT:
                    if(commandpos>=0 && ++commandpos>=(int)strlen(commandbuf)) commandpos = -1;
                    break;

                case SDLK_UP:
                    if(histpos) s_strcpy(commandbuf, vhistory[--histpos]);
                    break;

                case SDLK_DOWN:
                    if(histpos<vhistory.length()) s_strcpy(commandbuf, vhistory[histpos++]);
                    break;

                case SDLK_TAB:
                    complete(commandbuf);
                    if(commandpos>=0 && commandpos>=(int)strlen(commandbuf)) commandpos = -1;
                    break;

                case SDLK_v:
                    if(SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL)) { pasteconsole(); return; }

                default:
                    resetcomplete();
                    if(cooked)
                    {
                        size_t len = (int)strlen(commandbuf);
                        if(len+1<sizeof(commandbuf))
                        {
                            if(commandpos<0) commandbuf[len] = cooked;
                            else
                            {
                                memmove(&commandbuf[commandpos+1], &commandbuf[commandpos], len - commandpos);
                                commandbuf[commandpos++] = cooked;
                            }
                            commandbuf[len+1] = '\0';
                        }
                    }

            }
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
                    }
                    histpos = vhistory.length();
                    if(commandbuf[0]=='/') execute(commandbuf);
                    else toserver(commandbuf);
                }
                saycommand(NULL);
            }
            else if(code==SDLK_ESCAPE)
            {
                saycommand(NULL);
            }
        }
    }
    else if(!menukey(code, isdown))                 // keystrokes go to menu
    {
        loopv(keyms) if(keyms[i].code==code)        // keystrokes go to game, lookup in keymap and execute
        {
            keym &k = keyms[i];
            loopv(releaseactions)
            {
                releaseaction &ra = releaseactions[i];
                if(ra.key==&k)
                {
                    if(!isdown) execute(ra.action);
                    delete[] ra.action;
                    releaseactions.remove(i--);
                }
            }
            if(isdown)
            {
                keyaction = k.action;
                keypressed = &k;
                execute(keyaction);
                keypressed = NULL;
                if(keyaction!=k.action) delete[] keyaction;
            }
            break;
        }
    }
}

char *getcurcommand()
{
    return saycommandon ? commandbuf : NULL;
}

void writebinds(FILE *f)
{
    loopv(keyms)
    {
        if(*keyms[i].action) fprintf(f, "bind \"%s\" [%s]\n",     keyms[i].name, keyms[i].action);
    }
}

