// console.cpp: the console buffer, its display, and command line control

#include "pch.h"
#include "cube.h"

#define CONSPAD (FONTH/3)

VARP(consize, 0, 6, 100);
VARP(confade, 0, 20, 60);

struct console : consolebuffer
{    
    int conskip;
    void setconskip(int n)
    {
        conskip += n;
        if(conskip<0) conskip = 0;
    }

    static const int WORDWRAP = 80;

    bool fullconsole;
    void toggleconsole() { fullconsole = !fullconsole; }

    void addline(const char *sf, bool highlight) { consolebuffer::addline(sf, highlight, lastmillis); }

    vector<char *> visible;

    void render()
    {
        if(fullconsole)
        {
            int w = VIRTW*2, h = VIRTH*2;
            int numl = (h*2/5)/(FONTH*5/4);
            int offset = min(conskip, max(conlines.length() - numl, 0));
            blendbox(CONSPAD, CONSPAD, w-CONSPAD, 2*CONSPAD+numl*FONTH*5/4+2*FONTH/3, true);
            loopi(numl) draw_text(offset+i>=conlines.length() ? "" : conlines[offset+i].cref, CONSPAD+FONTH/3, CONSPAD+(FONTH*5/4)*(numl-i-1)+FONTH/3);
        }
        else
        {
            visible.setsizenodelete(0);
            loopv(conlines) if(conskip ? i>=conskip-1 || i>=conlines.length()-consize : (!confade || lastmillis-conlines[i].millis<confade*1000))
            {
                visible.add(conlines[i].cref);
                if(visible.length()>=consize) break;
            }
            loopvj(visible) draw_text(visible[j], CONSPAD+FONTH/3, CONSPAD+(FONTH*5/4)*(visible.length()-j-1)+FONTH/3);
        }
    }

    console() : fullconsole(false) {}
};

console con;
textinputbuffer cmdline;
bool saycommandon = false;

void setconskip(int n) { con.setconskip(n); }
COMMANDN(conskip, setconskip, ARG_1INT);

void toggleconsole() { con.toggleconsole(); }
COMMANDN(toggleconsole, toggleconsole, ARG_NONE);

void renderconsole() { con.render(); }

void conoutf(const char *s, ...)
{
    s_sprintfdv(sf, s);
    string sp;
    filtertext(sp, sf);
    puts(sp);
    s = sf;
    vector<char *> lines;
    text_block(s, curfont ? VIRTW*2-2*CONSPAD-2*FONTH/3 : 0, lines);
    loopv(lines) con.addline(lines[i], i!=0);
    lines.deletecontentsa();
}

void rendercommand(int x, int y)
{
    s_sprintfd(s)("> %s", cmdline.buf);
    int offset = text_width(s, cmdline.pos>=0 ? cmdline.pos+2 : -1);
    rendercursor(x+offset, y, char_width(cmdline.pos>=0 ? cmdline.buf[cmdline.pos] : '_'));
    draw_text(s, x, y);
}

// keymap is defined externally in keymap.cfg

vector<keym> keyms;

void keymap(char *code, char *key, char *action)
{
    keym &km = keyms.add();
    km.code = atoi(code);
    km.name = newstring(key);
    km.action = newstring(action);
}

COMMAND(keymap, ARG_3STR);

keym *findbind(const char *key)
{
    loopv(keyms) if(!strcasecmp(keyms[i].name, key)) return &keyms[i];
    return NULL;
}

keym *findbinda(const char *action)
{
    loopv(keyms) if(!strcasecmp(keyms[i].action, action)) return &keyms[i];
    return NULL;
}

keym *findbindc(int code)
{
    loopv(keyms) if(keyms[i].code==code) return &keyms[i];
    return NULL;
}

keym *keypressed = NULL;
char *keyaction = NULL;

bool bindkey(keym *km, const char *action)
{
    if(!km) return false;
    if(!keypressed || keyaction!=km->action) delete[] km->action;
    km->action = newstring(action);
    return true;
}

void bindk(const char *key, const char *action)
{
    keym *km = findbind(key);
    if(!km) { conoutf("unknown key \"%s\"", key); return; }
    bindkey(km, action);
}

bool bindc(int code, const char *action)
{
    keym *km = findbindc(code);
    if(km) return bindkey(km, action); 
    else return false;
}

COMMANDN(bind, bindk, ARG_2STR);

struct releaseaction
{
    keym *key;
    char *action;
};
vector<releaseaction> releaseactions;

char *addreleaseaction(const char *s)
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
    s_strcpy(cmdline.buf, init ? init : "");
    cmdline.pos = -1;
    player1->stopmoving(); // prevent situations where player presses direction key, open command line, then releases key
}

void mapmsg(char *s) { s_strncpy(hdr.maptitle, s, 128); }

COMMAND(saycommand, ARG_VARI);
COMMAND(mapmsg, ARG_1STR);

#if !defined(WIN32) && !defined(__APPLE__)
#include <X11/Xlib.h>
#include <SDL_syswm.h>
#endif

void pasteconsole(char *dst)
{
    #ifdef WIN32
    if(!IsClipboardFormatAvailable(CF_TEXT)) return; 
    if(!OpenClipboard(NULL)) return;
    char *cb = (char *)GlobalLock(GetClipboardData(CF_TEXT));
    s_strcat(dst, cb);
    GlobalUnlock(cb);
    CloseClipboard();
	#elif defined(__APPLE__)
	extern void mac_pasteconsole(char *commandbuf);
	
	mac_pasteconsole(dst);
	#else
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version); 
    wminfo.subsystem = SDL_SYSWM_X11;
    if(!SDL_GetWMInfo(&wminfo)) return;
    int cbsize;
    char *cb = XFetchBytes(wminfo.info.x11.display, &cbsize);
    if(!cb || !cbsize) return;
    int commandlen = strlen(dst);
    for(char *cbline = cb, *cbend; commandlen + 1 < _MAXDEFSTR && cbline < &cb[cbsize]; cbline = cbend + 1)
    {
        cbend = (char *)memchr(cbline, '\0', &cb[cbsize] - cbline);
        if(!cbend) cbend = &cb[cbsize];
        if(commandlen + cbend - cbline + 1 > _MAXDEFSTR) cbend = cbline + _MAXDEFSTR - commandlen - 1;
        memcpy(&dst[commandlen], cbline, cbend - cbline);
        commandlen += cbend - cbline;
        dst[commandlen] = '\n';
        if(commandlen + 1 < _MAXDEFSTR && cbend < &cb[cbsize]) ++commandlen;
        dst[commandlen] = '\0';
    }
    XFree(cb);
    #endif
}

cvector vhistory;
int histpos = 0;

void history(int n)
{
    static bool inhistory = false; 
    if(!inhistory && vhistory.inrange(n))
    {
        inhistory = true;
        char *buf = vhistory[vhistory.length()-n-1];
        if(buf[0]=='/') execute(buf+1);
        else toserver(buf);
        inhistory = false;
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
                case SDLK_F1:
                    toggledoc();
                    break;

                case SDLK_F2:
                    scrolldoc(-4);
                    break;

                case SDLK_F3:
                    scrolldoc(4);
                    break;

                case SDLK_UP:
                    if(histpos) s_strcpy(cmdline.buf, vhistory[--histpos]);
                    break;

                case SDLK_DOWN:
                    if(histpos<vhistory.length()) s_strcpy(cmdline.buf, vhistory[histpos++]);
                    break;

                case SDLK_TAB:
                    complete(cmdline.buf);
                    if(cmdline.pos>=0 && cmdline.pos>=(int)strlen(cmdline.buf)) cmdline.pos = -1;
                    break;

                default:
                    resetcomplete();
                    cmdline.key(code, isdown, cooked);
            }
        }
        else
        {
            if(code==SDLK_RETURN)
            {
                string buf; // use a copy to safely close the commandline
                s_strcpy(buf, cmdline.buf);
                saycommand(NULL);

                if(buf[0])
                {                    
                    if(buf[0]=='/') execute(buf+1);
                    else toserver(buf);

                    if(vhistory.empty() || strcmp(vhistory.last(), buf))
                    {
                        vhistory.add(newstring(buf));  // cap this?
                    }
                    histpos = vhistory.length();
                }
            }
            else if(code==SDLK_ESCAPE)
            {
                saycommand(NULL);
            }
        }
    }
    else if(!menukey(code, isdown, cooked))                 // keystrokes go to menu
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
    return saycommandon ? cmdline.buf : NULL;
}

void writebinds(FILE *f)
{
    loopv(keyms)
    {
        if(*keyms[i].action) fprintf(f, "bind \"%s\" [%s]\n",     keyms[i].name, keyms[i].action);
    }
}

