// console.cpp: the console buffer, its display, and command line control

#include "cube.h"

#define CONSPAD (FONTH/3)

VARP(altconsize, 0, 0, 100);
VARP(fullconsize, 0, 40, 100);
VARP(consize, 0, 6, 100);
VARP(confade, 0, 20, 60);
VAR(conopen, 0, 0, 1);
VAR(numconlines, 0, 0, 1);

struct console : consolebuffer<cline>
{
    int conskip;
    void setconskip(int n)
    {
        conskip = clamp(conskip + n, 0, conlines.length());
    }

    static const int WORDWRAP = 80;

    int fullconsole;
    void toggleconsole()
    {
        if(!fullconsole) fullconsole = altconsize ? 1 : 2;
        else fullconsole = (fullconsole + 1) % 3;
        conopen = fullconsole;
    }

    void addline(const char *sf) { consolebuffer<cline>::addline(sf, totalmillis); numconlines++; }

    void render()
    {
        int conwidth = (fullconsole ? VIRTW : int(floor(getradarpos().x)))*2 - 2*CONSPAD - 2*FONTH/3;
        int h = VIRTH*2 - 2*CONSPAD - 2*FONTH/3;
        int conheight = min(fullconsole ? (h*(fullconsole==1 ? altconsize : fullconsize))/100 : FONTH*consize, h);

        if(fullconsole) blendbox(CONSPAD, CONSPAD, conwidth+CONSPAD+2*FONTH/3, conheight+CONSPAD+2*FONTH/3, true);

        int numl = conlines.length(), offset = min(conskip, numl);

        if(!fullconsole && confade)
        {
            if(!conskip)
            {
                numl = 0;
                loopvrev(conlines) if(totalmillis-conlines[i].millis < confade*1000) { numl = i+1; break; }
            }
            else offset--;
        }

        int y = 0;
        loopi(numl) //determine visible height
        {
            // shuffle backwards to fill if necessary
            int idx = offset+i < numl ? offset+i : --offset;
            char *line = conlines[idx].line;
            int width, height;
            text_bounds(line, width, height, conwidth);
            y += height;
            if(y > conheight) { numl = i; if(offset == idx) ++offset; break; }
        }
        y = CONSPAD+FONTH/3;
        loopi(numl)
        {
            int idx = offset + numl-i-1;
            char *line = conlines[idx].line;
            draw_text(line, CONSPAD+FONTH/3, y, 0xFF, 0xFF, 0xFF, 0xFF, -1, conwidth);
            int width, height;
            text_bounds(line, width, height, conwidth);
            y += height;
        }
    }

    console() : consolebuffer<cline>(200), fullconsole(false) {}
};

console con;
textinputbuffer cmdline;
char *cmdaction = NULL, *cmdprompt = NULL;
bool saycommandon = false;

VARFP(maxcon, 10, 200, 1000, con.setmaxlines(maxcon));

void setconskip(int n) { con.setconskip(n); }
COMMANDN(conskip, setconskip, ARG_1INT);

void toggleconsole() { con.toggleconsole(); }
COMMANDN(toggleconsole, toggleconsole, ARG_NONE);

void renderconsole() { con.render(); }

void clientlogf(const char *s, ...)
{
    defvformatstring(sp, s, s);
    filtertext(sp, sp, 2);
    extern struct servercommandline scl;
    const char *ts = scl.logtimestamp ? timestring(true, "%b %d %H:%M:%S ") : "";
    char *p, *l = sp;
    do
    { // break into single lines first
        if((p = strchr(l, '\n'))) *p = '\0';
        printf("%s%s\n", ts, l);
        l = p + 1;
    }
    while(p);
}
SVAR(conline,"n/a");
void conoutf(const char *s, ...)
{
    defvformatstring(sf, s, s);
    clientlogf("%s", sf);
    con.addline(sf);
    delete[] conline; conline=newstring(sf);
}
void strstra(const char *a,const char *b) {
    intret(strstr(a,b)?1:0);
    return;
}
COMMANDN(strstr,strstra,ARG_2STR);
/** This is the 1.0.4 function
    It will substituted by rendercommand_wip
    I am putting this temporarily here because it is very difficult to chat in game with the current cursor behavior,
    and chatting in this test period is extremelly important : Brahma */
int rendercommand(int x, int y, int w)
{
    defformatstring(s)("# %s", cmdline.buf); /** I changed the symbol here to differentiate from the > (new talk symbol),
                                             and make clear the console changed to the old players (like me) : Brahma */
    int width, height;
    text_bounds(s, width, height, w);
    y -= height - FONTH;
    draw_text(s, x, y, 0xFF, 0xFF, 0xFF, 0xFF, cmdline.pos>=0 ? cmdline.pos+2 : (int)strlen(s), w);
    return height;
}

const char *getCONprefix(int n)
{
    const char* CONpreSTR[] = {
        ">", // ">>>", // "TALK" // "T" // ">"
        "/", // "CFG", // "EXEC" // "!" // "!"
        "%", // "TEAM" // ">" // "T"
    };
    return (n>=0 && size_t(n) < sizeof(CONpreSTR)/sizeof(CONpreSTR[0])) ? CONpreSTR[n] : "#";
}

int getCONlength(int n)
{
    const char* CURpreSTR = getCONprefix(n);
    return strlen(CURpreSTR);
}

/** WIP ALERT */
int rendercommand_wip(int x, int y, int w)
{
    int width, height;
    if( strlen(cmdline.buf) > 0 )
    {
        int ctx = -1;
        switch( cmdline.buf[0] )
        {
            case '>': ctx = 0; break;
            case '/': ctx = 1; break;
            case '%': ctx = 2; break;
            default: break;
        }
        defformatstring(s)("%s %s", getCONprefix(ctx), cmdline.buf+1);
        text_bounds(s, width, height, w);
        y -= height - FONTH;
        draw_text(s, x, y, 0xFF, 0xFF, 0xFF, 0xFF, cmdline.pos>=0 ? cmdline.pos/*+1*/+getCONlength(ctx) : (int)strlen(s), w);
    }
    return height;
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
void keybind(const char *key)
{
    keym *km = findbind(key);
    if(!km) { conoutf("unknown key \"%s\"", key); return; }
    result(km->action);
}
bool bindc(int code, const char *action)
{
    keym *km = findbindc(code);
    if(km) return bindkey(km, action);
    else return false;
}

COMMANDN(bind, bindk, ARG_2STR);
COMMAND(keybind, ARG_1STR);
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
    setburst(false);
    if(!editmode) keyrepeat(saycommandon);
    copystring(cmdline.buf, init ? init : ">"); // ALL cmdline.buf[0] ARE flag-chars ! ">" is for talk - the previous "no flag-char" item
    DELETEA(cmdaction);
    DELETEA(cmdprompt);
    cmdline.pos = -1;
}

void inputcommand(char *init, char *action, char *prompt)
{
    saycommand(init);
    if(action[0]) cmdaction = newstring(action);
    if(prompt[0]) cmdprompt = newstring(prompt);
}

void mapmsg(char *s)
{
    string text;
    filterrichtext(text, s);
    filterservdesc(text, text);
    copystring(hdr.maptitle, text, 128);
}

void getmapmsg(void)
{
    string text;
    copystring(text, hdr.maptitle, 128);
    result(text);
}

COMMAND(saycommand, ARG_CONC);
COMMAND(inputcommand, ARG_3STR);
COMMAND(mapmsg, ARG_1STR);
COMMAND(getmapmsg, ARG_NONE);

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
    concatstring(dst, cb);
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
    for(char *cbline = cb, *cbend; commandlen + 1 < MAXSTRLEN && cbline < &cb[cbsize]; cbline = cbend + 1)
    {
        cbend = (char *)memchr(cbline, '\0', &cb[cbsize] - cbline);
        if(!cbend) cbend = &cb[cbsize];
        if(commandlen + cbend - cbline + 1 > MAXSTRLEN) cbend = cbline + MAXSTRLEN - commandlen - 1;
        memcpy(&dst[commandlen], cbline, cbend - cbline);
        commandlen += cbend - cbline;
        dst[commandlen] = '\n';
        if(commandlen + 1 < MAXSTRLEN && cbend < &cb[cbsize]) ++commandlen;
        dst[commandlen] = '\0';
    }
    XFree(cb);
    #endif
}

struct hline
{
    char *buf, *action, *prompt;

    hline() : buf(NULL), action(NULL), prompt(NULL) {}
    ~hline()
    {
        DELETEA(buf);
        DELETEA(action);
        DELETEA(prompt);
    }

    void restore()
    {
        copystring(cmdline.buf, buf);
        if(cmdline.pos >= (int)strlen(cmdline.buf)) cmdline.pos = -1;
        DELETEA(cmdaction);
        DELETEA(cmdprompt);
        if(action) cmdaction = newstring(action);
        if(prompt) cmdprompt = newstring(prompt);
    }

    bool shouldsave()
    {
        return strcmp(cmdline.buf, buf) ||
               (cmdaction ? !action || strcmp(cmdaction, action) : action!=NULL) ||
               (cmdprompt ? !prompt || strcmp(cmdprompt, prompt) : prompt!=NULL);
    }

    void save()
    {
        buf = newstring(cmdline.buf);
        if(cmdaction) action = newstring(cmdaction);
        if(cmdprompt) prompt = newstring(cmdprompt);
    }

    void run()
    {
        pushscontext(IEXC_PROMPT);
        if(action)
        {
            alias("cmdbuf", buf);
            execute(action);
        }
        else if(buf[0]=='/') execute(buf+1);
        else if(buf[0]=='>') toserver(buf+1);
        else if(buf[0]=='%') toserver(buf);
        else toserver(buf); // execute(buf); // still default to simple "say".
        popscontext();
    }
};
vector<hline *> history;
int histpos = 0;

VARP(maxhistory, 0, 1000, 10000);

void history_(int n)
{
    static bool inhistory = false;
    if(!inhistory && history.inrange(n))
    {
        inhistory = true;
        history[history.length()-n-1]->run();
        inhistory = false;
    }
}

COMMANDN(history, history_, ARG_1INT);

void execbind(keym &k, bool isdown)
{
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
    k.pressed = isdown;
}

void consolekey(int code, bool isdown, int cooked)
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
                if(histpos > history.length()) histpos = history.length();
                if(histpos > 0) history[--histpos]->restore();
                break;

            case SDLK_DOWN:
                if(histpos + 1 < history.length()) history[++histpos]->restore();
                break;

            case SDLK_TAB:
                if(!cmdaction)
                {
                    complete(cmdline.buf);
                    if(cmdline.pos>=0 && cmdline.pos>=(int)strlen(cmdline.buf)) cmdline.pos = -1;
                }
                break;

            default:
                resetcomplete();
                cmdline.key(code, isdown, cooked);
                break;
        }
    }
    else
    {
        if(code==SDLK_RETURN)
        {
            hline *h = NULL;
            if(cmdline.buf[0])
            {
                if(history.empty() || history.last()->shouldsave())
                {
                    if(maxhistory && history.length() >= maxhistory)
                    {
                        loopi(history.length()-maxhistory+1) delete history[i];
                        history.remove(0, history.length()-maxhistory+1);
                    }
                    history.add(h = new hline)->save();
                }
                else h = history.last();
            }
            histpos = history.length();
            saycommand(NULL);
            if(h) h->run();
        }
        else if(code==SDLK_ESCAPE)
        {
            histpos = history.length();
            saycommand(NULL);
        }
    }
}

void keypress(int code, bool isdown, int cooked, SDLMod mod)
{
    keym *haskey = NULL;
    loopv(keyms) if(keyms[i].code==code) { haskey = &keyms[i]; break; }
    if(haskey && haskey->pressed) execbind(*haskey, isdown); // allow pressed keys to release
    else if(saycommandon) consolekey(code, isdown, cooked);  // keystrokes go to commandline
    else if(!menukey(code, isdown, cooked, mod))                  // keystrokes go to menu
    {
        if(haskey) execbind(*haskey, isdown);
    }
}

char *getcurcommand()
{
    return saycommandon ? cmdline.buf : NULL;
}

void writebinds(stream *f)
{
    loopv(keyms)
    {
        if(*keyms[i].action) f->printf("bind %s [%s]\n",     keyms[i].name, keyms[i].action);
    }
}

