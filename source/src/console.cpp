// console.cpp: the console buffer, its display, and command line control

#include "cube.h"

#define CONSPAD (FONTH/3)

VARP(altconsize, 0, 0, 100);
VARP(fullconsize, 0, 40, 100);
VARP(consize, 0, 6, 100);
VARP(confade, 0, 20, 60);
VARP(conalpha, 0, 255, 255);
VAR(conopen, 1, 0, 0);
VAR(numconlines, 0, 0, 1);

struct console : consolebuffer<cline>
{
    int conskip;
    void setconskip(int n)
    {
        int visible_lines = (int)(min(fullconsole ? ((VIRTH*2 - 2*CONSPAD - 2*FONTH/3)*(fullconsole==1 ? altconsize : fullconsize))/100 : FONTH*consize, (VIRTH*2 - 2*CONSPAD - 2*FONTH/3))/ (CONSPAD + 2*FONTH/3)) - 1;
        conskip = clamp(conskip + n, 0, clamp(conlines.length()-visible_lines, 0, conlines.length()));
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
            draw_text(line, CONSPAD+FONTH/3, y, 0xFF, 0xFF, 0xFF, fullconsole ? 0xFF : conalpha, -1, conwidth);
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
bool saycommandon = false, storeinputcommand = false;

VARFP(maxcon, 10, 200, 1000, con.setmaxlines(maxcon));

void setconskip(int *n) { con.setconskip(*n); }
COMMANDN(conskip, setconskip, "i");

void toggleconsole() { con.toggleconsole(); }
COMMANDN(toggleconsole, toggleconsole, "");

void renderconsole() { con.render(); }

stream *clientlogfile = NULL;
vector<char> *bootclientlog = new vector<char>;
int clientloglinesremaining = INT_MAX;

void clientlogf(const char *s, ...)
{
    defvformatstring(sp, s, s);
    filtertext(sp, sp, FTXT__LOG);
    extern struct servercommandline scl;
    const char *ts = scl.logtimestamp ? timestring(true, "%b %d %H:%M:%S ") : "";
    char *p, *l = sp;
    do
    { // break into single lines first
        if((p = strchr(l, '\n'))) *p = '\0';
        printf("%s%s\n", ts, l);
        if(clientlogfile)
        {
            clientlogfile->printf("%s%s\n", ts, l);
            if(--clientloglinesremaining <= 0) DELETEP(clientlogfile);
        }
        else if(bootclientlog) cvecprintf(*bootclientlog, "%s%s\n", ts, l);
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

int rendercommand(int x, int y, int w)
{
    const char *useprompt = cmdprompt ? cmdprompt : "#";
    defformatstring(s)("%s %s", useprompt, cmdline.buf);
    int width, height;
    text_bounds(s, width, height, w);
    y -= height - FONTH;
    if (x >= 0) draw_text(s, x, y, 0xFF, 0xFF, 0xFF, 0xFF, cmdline.pos>=0 ? cmdline.pos + strlen(useprompt) + 1  : (int)strlen(s), w);
    return height;
}

// keymap is defined externally in keymap.cfg

vector<keym> keyms;

const char *keycmds[keym::NUMACTIONS] = { "bind", "editbind", "specbind" };
inline const char *keycmd(int type) { return type >= 0 && type < keym::NUMACTIONS ? keycmds[type] : ""; }

void keymap(int *code, char *key)
{
    keym &km = keyms.add();
    km.code = *code;
    km.name = newstring(key);
}

COMMAND(keymap, "is");

keym *findbind(const char *key)
{
    loopv(keyms) if(!strcasecmp(keyms[i].name, key)) return &keyms[i];
    return NULL;
}

keym **findbinda(const char *action, int type)
{
    static vector<keym *> res;
    res.setsize(0);
    loopv(keyms) if(!strcasecmp(keyms[i].actions[type], action)) res.add(&keyms[i]);
    res.add(NULL);
    return res.getbuf();
}

keym *findbindc(int code)
{
    loopv(keyms) if(keyms[i].code==code) return &keyms[i];
    return NULL;
}

void findkey(int *code)
{
    const char *res = "-255";
    loopv(keyms)
    {
        if(keyms[i].code == *code)
        {
            res = keyms[i].name;
            break;
        }
    }
    result(res);
}
COMMAND(findkey, "i");

void findkeycode(const char* s)
{
    int res = -255;
    loopv(keyms)
    {
        if(!strcmp(s, keyms[i].name))
        {
            res = keyms[i].code;
            break;
        }
    }
    intret(res);
}
COMMAND(findkeycode, "s");

COMMANDF(modkeypressed, "", () { intret((SDL_GetModState() & MOD_KEYS_CTRL) ? 1 : 0); });

keym *keypressed = NULL;
char *keyaction = NULL;

VAR(_defaultbinds, 0, 0, 1);

COMMANDF(_resetallbinds, "", () { if(_defaultbinds) loopv(keyms) loopj(keym::NUMACTIONS) bindkey(&keyms[i], "", j); });

bool bindkey(keym *km, const char *action, int type)
{
    if(!km || type < keym::ACTION_DEFAULT || type >= keym::NUMACTIONS) return false;
    if(strcmp(action, km->actions[type]))
    {
        if(!keypressed || keyaction!=km->actions[type]) delete[] km->actions[type];
        km->actions[type] = newstring(action);
        km->unchangeddefault[type] = _defaultbinds != 0;
    }
    return true;
}

void bindk(const char *key, const char *action, int type)
{
    keym *km = findbind(key);
    if(!km) { conoutf("unknown key \"%s\"", key); return; }
    bindkey(km, action, type);
}

void keybind(const char *key, int type)
{
    keym *km = findbind(key);
    if(!km) { conoutf("unknown key \"%s\"", key); return; }
    if(type < keym::ACTION_DEFAULT || type >= keym::NUMACTIONS) { conoutf("invalid bind type \"%d\"", type); return; }
    result(km->actions[type]);
}

bool bindc(int code, const char *action, int type)
{
    keym *km = findbindc(code);
    if(km) return bindkey(km, action, type);
    else return false;
}

void searchbinds(const char *action, int type)
{
    if(!action || !action[0]) return;
    if(type < keym::ACTION_DEFAULT || type >= keym::NUMACTIONS) { conoutf("invalid bind type \"%d\"", type); return; }
    vector<char> names;
    loopv(keyms)
    {
        if(!strcmp(keyms[i].actions[type], action))
        {
            if(names.length()) names.add(' ');
            names.put(keyms[i].name, strlen(keyms[i].name));
        }
    }
    resultcharvector(names, 0);
}

COMMANDF(keybind, "s", (const char *key) { keybind(key, keym::ACTION_DEFAULT); } );
COMMANDF(keyspecbind, "s", (const char *key) { keybind(key, keym::ACTION_SPECTATOR); } );
COMMANDF(keyeditbind, "s", (const char *key) { keybind(key, keym::ACTION_EDITING); } );

COMMANDF(bind, "ss", (const char *key, const char *action) { bindk(key, action, keym::ACTION_DEFAULT); } );
COMMANDF(specbind, "ss", (const char *key, const char *action) { bindk(key, action, keym::ACTION_SPECTATOR); } );
COMMANDF(editbind, "ss", (const char *key, const char *action) { bindk(key, action, keym::ACTION_EDITING); } );

COMMANDF(searchbinds, "s", (const char *action) { searchbinds(action, keym::ACTION_DEFAULT); });
COMMANDF(searchspecbinds, "s", (const char *action) { searchbinds(action, keym::ACTION_SPECTATOR); });
COMMANDF(searcheditbinds, "s", (const char *action) { searchbinds(action, keym::ACTION_EDITING); });

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

COMMAND(onrelease, "s");

void saycommand(char *init)                         // turns input to the command line on or off
{
    SDL_EnableUNICODE(saycommandon = (init!=NULL));
    setscope(false);
    setburst(false);
    if(!editmode) keyrepeat(saycommandon);
    copystring(cmdline.buf, init ? escapestring(init, false, true) : "");
    DELETEA(cmdaction);
    DELETEA(cmdprompt);
    cmdline.pos = -1;
}
COMMAND(saycommand, "c");

void inputcommand(char *init, char *action, char *prompt, int *nopersist)
{
    saycommand(init);
    if(action[0]) cmdaction = newstring(action);
    if(prompt[0]) cmdprompt = newstring(prompt);
    storeinputcommand = cmdaction && !*nopersist;
}
COMMAND(inputcommand, "sssi");

SVARFF(mapmsg,
{ // read mapmsg
    hdr.maptitle[127] = '\0';
    mapmsg = exchangestr(mapmsg, hdr.maptitle);
},
{ // set new mapmsg
    string text;
    filtertext(text, mapmsg, FTXT__MAPMSG);
    copystring(hdr.maptitle, text, 128);
    if(editmode) unsavededits++;
});

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
        storeinputcommand = false;
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
            push("cmdbuf", buf);
            execute(action);
            pop("cmdbuf");
        }
        else if(buf[0]=='/') execute(buf+1);
        else toserver(buf);
        popscontext();
    }
};
vector<hline *> history;
int histpos = 0;

VARP(maxhistory, 0, 1000, 10000);

void history_(int *n)
{
    static bool inhistory = false;
    if(!inhistory && history.inrange(*n))
    {
        inhistory = true;
        history[history.length() - *n - 1]->run();
        inhistory = false;
    }
}

COMMANDN(history, history_, "i");

void savehistory()
{
    stream *f = openfile(path("config/history", true), "w");
    if(!f) return;
    loopv(history)
    {
        hline *h = history[i];
        if(!h->action && !h->prompt && strncmp(h->buf, "/ ", 2))
            f->printf("%s\n",h->buf);
    }
    delete f;
}

void loadhistory()
{
    char *histbuf = loadfile(path("config/history", true), NULL);
    if(!histbuf) return;
    char *line = NULL, *b;
    line = strtok_r(histbuf, "\n", &b);
    while(line)
    {
        history.add(new hline)->buf = newstring(line);
        line = strtok_r(NULL, "\n", &b);
    }
    DELETEA(histbuf);
    histpos = history.length();
}

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
        int state = keym::ACTION_DEFAULT;
        if(editmode) state = keym::ACTION_EDITING;
        else if(player1->isspectating()) state = keym::ACTION_SPECTATOR;
        char *&action = k.actions[state][0] ? k.actions[state] : k.actions[keym::ACTION_DEFAULT];
        keyaction = action;
        keypressed = &k;
        execute(keyaction);
        keypressed = NULL;
        if(keyaction!=action) delete[] keyaction;
    }
    k.pressed = isdown;
}

void consolekey(int code, bool isdown, int cooked, SDLMod mod)
{
    static char *beforecomplete = NULL;
    static bool ignoreescup = false;
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

            case SDL_AC_BUTTON_WHEELUP:
            case SDLK_UP:
                if(histpos > history.length()) histpos = history.length();
                if(histpos > 0) history[--histpos]->restore();
                break;

            case SDL_AC_BUTTON_WHEELDOWN:
            case SDLK_DOWN:
                if(histpos + 1 < history.length()) history[++histpos]->restore();
                break;

            case SDLK_TAB:
                if(!cmdaction)
                {
                    if(!beforecomplete) beforecomplete = newstring(cmdline.buf);
                    complete(cmdline.buf, (mod & KMOD_LSHIFT) ? true : false);
                    if(cmdline.pos >= 0 && cmdline.pos >= (int)strlen(cmdline.buf)) cmdline.pos = -1;
                }
                break;

            case SDLK_ESCAPE:
                if(mod & KMOD_LSHIFT)    // LSHIFT+ESC restores buffer from before complete()
                {
                    if(beforecomplete) copystring(cmdline.buf, beforecomplete);
                    ignoreescup = true;
                }
                else ignoreescup = false;
                break;

            default:
                resetcomplete();
                DELETEA(beforecomplete);
            case SDLK_LSHIFT:
                cmdline.key(code, isdown, cooked);
                break;
        }
    }
    else
    {
        if(code==SDLK_RETURN || code==SDLK_KP_ENTER || code==SDL_AC_BUTTON_LEFT || code==SDL_AC_BUTTON_MIDDLE)
        {
            // make laptop users happy; LMB shall only work with history
            if(code == SDL_AC_BUTTON_LEFT && histpos == history.length()) return;

            hline *h = NULL;
            if(cmdline.buf[0] || cmdaction)
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
            if(h)
            {
                h->run();
                if(h->action && !storeinputcommand) history.drop();
            }
        }
        else if((code==SDLK_ESCAPE && !ignoreescup) || code== SDL_AC_BUTTON_RIGHT)
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
    else if(saycommandon) consolekey(code, isdown, cooked, mod);  // keystrokes go to commandline
    else if(!menukey(code, isdown, cooked, mod))                  // keystrokes go to menu
    {
        if(haskey) execbind(*haskey, isdown);
    }
    if(isdown) exechook(HOOK_SP, "KEYPRESS", "KEYPRESS %d", code);
    else exechook(HOOK_SP, "KEYRELEASE", "%d", code);
}

char *getcurcommand(int *pos)
{
    if(pos && saycommandon) *pos = cmdline.pos;
    return saycommandon ? cmdline.buf : NULL;
}

VARP(omitunchangeddefaultbinds, 0, 1, 2);

void writebinds(stream *f)
{
    loopv(keyms)
    {
        keym *km = &keyms[i];
        loopj(3) if(*km->actions[j] && (!km->unchangeddefault[j] || omitunchangeddefaultbinds < 2))
            f->printf("%s%s \"%s\" %s\n", km->unchangeddefault[j] && omitunchangeddefaultbinds ? "//" : "", keycmd(j), km->name, escapestring(km->actions[j]));
    }
}

