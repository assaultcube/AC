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

hashtable<int, keym> keyms(256);

const char *keycmds[keym::NUMACTIONS] = { "bind", "editbind", "specbind" };
inline const char *keycmd(int type) { return type >= 0 && type < keym::NUMACTIONS ? keycmds[type] : ""; }

void keymap(int *code, char *key)
{
    if(findbindc(*code)) { clientlogf("keymap: double assignment for code %d ignored", *code); return; }
    keym &km = keyms[*code];
    km.code = *code;
    km.name = newstring(key);
}
COMMAND(keymap, "is");

keym *findbind(const char *key)
{
    enumerate(keyms, keym, km, if(!strcasecmp(km.name, key)) return &km;);
    return NULL;
}

keym **findbinda(const char *action, int type)
{
    static vector<keym *> res;
    res.setsize(0);
    enumerate(keyms, keym, km, if(!strcasecmp(km.actions[type], action)) res.add(&km););
    res.add(NULL);
    return res.getbuf();
}

keym *findbindc(int code)
{
    return keyms.access(code);
}

keym *autokeymap(int keycode, int scancode)
{
    static bool scdone[SDL_NUM_SCANCODES] = { false };
    int sc = keycode <= SDLK_SCANCODE_MASK ? scancode : (keycode & (SDLK_SCANCODE_MASK - 1));
    if(sc < 0 || sc >= SDL_NUM_SCANCODES || scdone[sc]) return NULL;
    scdone[sc] = true;
    string text;
    filtertext(text, SDL_GetScancodeName((SDL_Scancode) sc), FTXT__AUTOKEYMAPNAME);
    if(!*text || (int)SDL_GetScancodeFromName(text) != sc || findbind(text)) return NULL; // name is not unique (collision probably caused by filtertext) or can't be properly resolved in reverse
    clientlogf("autokeymap: create keymap entry for scancode %d, \"%s\"", sc, text);
    sc |= SDLK_SCANCODE_MASK;
    keymap(&sc, text);
    return keyms.access(sc);
}

void findkey(int *code)
{
    const char *res = "-255";
    enumerate(keyms, keym, km,
        if(km.code == *code)
        {
            res = km.name;
            break;
        }
    );
    result(res);
}
COMMAND(findkey, "i");

void findkeycode(const char* s)
{
    int res = -255;
    enumerate(keyms, keym, km,
        if(!strcmp(s, km.name))
        {
            res = km.code;
            break;
        }
    );
    intret(res);
}
COMMAND(findkeycode, "s");

COMMANDF(modkeypressed, "", () { intret((SDL_GetModState() & MOD_KEYS_CTRL) ? 1 : 0); });

keym *keypressed = NULL;
char *keyaction = NULL;

VAR(_defaultbinds, 0, 0, 1);

COMMANDF(_resetallbinds, "", () { if(_defaultbinds) enumerate(keyms, keym, km, loopj(keym::NUMACTIONS) bindkey(&km, "", j);); });

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
    if(!km)
    {
        string text;
        filtertext(text, key, FTXT__AUTOKEYMAPNAME);
        int sc = *text && !strcmp(key, text) ? (int)SDL_GetScancodeFromName(text) : SDL_SCANCODE_UNKNOWN;
        if(sc != SDL_SCANCODE_UNKNOWN)
        {
            clientlogf("autokeymap: re-create keymap entry for scancode %d, \"%s\"", sc, text);
            sc |= SDLK_SCANCODE_MASK;
            keymap(&sc, text);
            km = findbind(key);
        }
    }
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
    enumerate(keyms, keym, km,
        if(!strcmp(km.actions[type], action))
        {
            if(names.length()) names.add(' ');
            names.put(km.name, strlen(km.name));
        }
    );
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
    saycommandon = (init != NULL);
    textinput(saycommandon, TI_CONSOLE);
    keyrepeat(saycommandon, KR_CONSOLE);
    setscope(false);
    setburst(false);
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

void consolekey(int code, bool isdown, SDL_Keymod mod)
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
                cmdline.key(code);
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

void processtextinput(const char *text)
{
    if(saycommandon)
    {
        cmdline.say(text);
    }
    else
    {
        menusay(text);
    }
}

void keypress(int keycode, int scancode, bool isdown, SDL_Keymod mod)
{
    keym *haskey = keyms.access(keycode);
    if(!haskey && !(keycode & SDLK_SCANCODE_MASK) && scancode) haskey = keyms.access(scancode | SDLK_SCANCODE_MASK); // keycode not found: maybe we know the scancode
    if(!haskey && isdown) haskey = autokeymap(keycode, scancode);
    if(haskey && haskey->pressed) execbind(*haskey, isdown); // allow pressed keys to release
    else if(saycommandon) consolekey(keycode, isdown, mod); // keystrokes go to commandline
    else if(!menukey(keycode, isdown)) // keystrokes go to menu
    {
        if(haskey) execbind(*haskey, isdown);
    }
    if(keycode & SDLK_SCANCODE_MASK) scancode = keycode & ~SDLK_SCANCODE_MASK;
    if(isdown) exechook(HOOK_SP, "KEYPRESS", "KEYPRESS %d %d", keycode, scancode);
    else exechook(HOOK_SP, "KEYRELEASE", "%d %d", keycode, scancode);
}

char *getcurcommand(int *pos)
{
    if(pos && saycommandon) *pos = cmdline.pos;
    return saycommandon ? cmdline.buf : NULL;
}

VARP(omitunchangeddefaultbinds, 0, 1, 2);

void writebinds(stream *f)
{
    enumerate(keyms, keym, km,
        loopj(3) if(*km.actions[j] && (!km.unchangeddefault[j] || omitunchangeddefaultbinds < 2))
            f->printf("%s%s \"%s\" %s\n", km.unchangeddefault[j] && omitunchangeddefaultbinds ? "//" : "", keycmd(j), km.name, escapestring(km.actions[j]));
    );
}

bool textinputbuffer::say(const char *c)
{
    string txt;
    copystring(txt, c);
    if(filterunrenderables(txt)) audiomgr.playsound(S_PAIN1, SP_LOW);
    int buflen = (int)strlen(buf), txtlen = (int)strlen(txt);
    if(txtlen && buflen < maxlen() &&  buflen + txtlen < (int)sizeof(buf))
    {
        if(pos < 0) strncpy(buf + buflen, txt, txtlen);
        else
        {
            memmove(&buf[pos + txtlen], &buf[pos], buflen - pos);
            memcpy(&buf[pos], txt, txtlen);
            pos += txtlen;
        }
        buf[buflen + txtlen] = '\0';
        return true;
    }
    return false;
}

void textinputbuffer::pasteclipboard()
{
    const char *s = SDL_GetClipboardText();
    if(s)
    {
        string f;
        filtertext(f, s, FTXT__CONSOLEPASTE);
        say(f);
        SDL_free((void*)s);
    }
}

#ifdef _DEBUG
void writekeymap() // create keymap.cfg with proper constants from SDL2 header files
{
    const struct _keymaptab { int keycode; const char *keyname; } keymaptab[] = {
        { -1, "MOUSE1" },
        { -3, "MOUSE2" },
        { -2, "MOUSE3" },
        { -4, "MOUSE4" },
        { -5, "MOUSE5" },
        { -6, "MOUSE6" },
        { -7, "MOUSE7" },
        { -8, "MOUSE8" },
        { -9, "MOUSE9" },
        { -10, "MOUSE10" },
        { -11, "MOUSE11" },
        { -12, "MOUSE12" },
        { -13, "MOUSE13" },
        { -14, "MOUSE14" },
        { -15, "MOUSE15" },
        { -16, "MOUSE16" },
        { SDLK_BACKSPACE, "BACKSPACE" },
        { SDLK_TAB, "TAB" },
        { SDLK_CLEAR, "CLEAR" },
        { SDLK_RETURN, "RETURN" },
        { SDLK_PAUSE, "PAUSE" },
        { SDLK_ESCAPE, "ESCAPE" },
        { SDLK_SPACE, "SPACE" },
        { SDLK_EXCLAIM, "EXCLAIM" },
        { SDLK_QUOTEDBL, "QUOTEDBL" },
        { SDLK_HASH, "HASH" },
        { SDLK_DOLLAR, "DOLLAR" },
        { SDLK_AMPERSAND, "AMPERSAND" },
        { SDLK_QUOTE, "QUOTE" },
        { SDLK_LEFTPAREN, "LEFTPAREN" },
        { SDLK_RIGHTPAREN, "RIGHTPAREN" },
        { SDLK_ASTERISK, "ASTERISK" },
        { SDLK_PLUS, "PLUS" },
        { SDLK_COMMA, "COMMA" },
        { SDLK_MINUS, "MINUS" },
        { SDLK_PERIOD, "PERIOD" },
        { SDLK_SLASH, "SLASH" },
        { SDLK_0, "0" },
        { SDLK_1, "1" },
        { SDLK_2, "2" },
        { SDLK_3, "3" },
        { SDLK_4, "4" },
        { SDLK_5, "5" },
        { SDLK_6, "6" },
        { SDLK_7, "7" },
        { SDLK_8, "8" },
        { SDLK_9, "9" },
        { SDLK_COLON, "COLON" },
        { SDLK_SEMICOLON, "SEMICOLON" },
        { SDLK_LESS, "LESS" },
        { SDLK_EQUALS, "EQUALS" },
        { SDLK_GREATER, "GREATER" },
        { SDLK_QUESTION, "QUESTION" },
        { SDLK_AT, "AT" },
        { SDLK_LEFTBRACKET, "LEFTBRACKET" },
        { SDLK_BACKSLASH, "BACKSLASH" },
        { SDLK_RIGHTBRACKET, "RIGHTBRACKET" },
        { SDLK_CARET, "CARET" },
        { SDLK_UNDERSCORE, "UNDERSCORE" },
        { SDLK_BACKQUOTE, "BACKQUOTE" },
        { SDLK_a, "A" },
        { SDLK_b, "B" },
        { SDLK_c, "C" },
        { SDLK_d, "D" },
        { SDLK_e, "E" },
        { SDLK_f, "F" },
        { SDLK_g, "G" },
        { SDLK_h, "H" },
        { SDLK_i, "I" },
        { SDLK_j, "J" },
        { SDLK_k, "K" },
        { SDLK_l, "L" },
        { SDLK_m, "M" },
        { SDLK_n, "N" },
        { SDLK_o, "O" },
        { SDLK_p, "P" },
        { SDLK_q, "Q" },
        { SDLK_r, "R" },
        { SDLK_s, "S" },
        { SDLK_t, "T" },
        { SDLK_u, "U" },
        { SDLK_v, "V" },
        { SDLK_w, "W" },
        { SDLK_x, "X" },
        { SDLK_y, "Y" },
        { SDLK_z, "Z" },
        { SDLK_DELETE, "DELETE" },
        { SDLK_KP_0, "KP0" },
        { SDLK_KP_1, "KP1" },
        { SDLK_KP_2, "KP2" },
        { SDLK_KP_3, "KP3" },
        { SDLK_KP_4, "KP4" },
        { SDLK_KP_5, "KP5" },
        { SDLK_KP_6, "KP6" },
        { SDLK_KP_7, "KP7" },
        { SDLK_KP_8, "KP8" },
        { SDLK_KP_9, "KP9" },
        { SDLK_KP_PERIOD, "KP_PERIOD" },
        { SDLK_KP_DIVIDE, "KP_DIVIDE" },
        { SDLK_KP_MULTIPLY, "KP_MULTIPLY" },
        { SDLK_KP_MINUS, "KP_MINUS" },
        { SDLK_KP_PLUS, "KP_PLUS" },
        { SDLK_KP_ENTER, "KP_ENTER" },
        { SDLK_KP_EQUALS, "KP_EQUALS" },
        { SDLK_UP, "UP" },
        { SDLK_DOWN, "DOWN" },
        { SDLK_RIGHT, "RIGHT" },
        { SDLK_LEFT, "LEFT" },
        { SDLK_INSERT, "INSERT" },
        { SDLK_HOME, "HOME" },
        { SDLK_END, "END" },
        { SDLK_PAGEUP, "PAGEUP" },
        { SDLK_PAGEDOWN, "PAGEDOWN" },
        { SDLK_F1, "F1" },
        { SDLK_F2, "F2" },
        { SDLK_F3, "F3" },
        { SDLK_F4, "F4" },
        { SDLK_F5, "F5" },
        { SDLK_F6, "F6" },
        { SDLK_F7, "F7" },
        { SDLK_F8, "F8" },
        { SDLK_F9, "F9" },
        { SDLK_F10, "F10" },
        { SDLK_F11, "F11" },
        { SDLK_F12, "F12" },
        { SDLK_F13, "F13" },
        { SDLK_F14, "F14" },
        { SDLK_F15, "F15" },
        { SDLK_NUMLOCKCLEAR, "NUMLOCK" },
        { SDLK_CAPSLOCK, "CAPSLOCK" },
        { SDLK_SCROLLLOCK, "SCROLLOCK" },
        { SDLK_RSHIFT, "RSHIFT" },
        { SDLK_LSHIFT, "LSHIFT" },
        { SDLK_RCTRL, "RCTRL" },
        { SDLK_LCTRL, "LCTRL" },
        { SDLK_RALT, "RALT" },
        { SDLK_LALT, "LALT" },
        { SDLK_RGUI, "RMETA" },
        { SDLK_LGUI, "LMETA" },
        { SDLK_MODE, "MODE" },
        { SDLK_APPLICATION, "COMPOSE" },
        { SDLK_HELP, "HELP" },
        { SDLK_PRINTSCREEN, "PRINT" },
        { SDLK_SYSREQ, "SYSREQ" },
        { SDLK_MENU, "MENU" },
        { 223, "SZ" }, // appear to work, but are undocumented...
        { 252, "UE" },
        { 246, "OE" },
        { 228, "AE" },
        { SDL_SCANCODE_GRAVE | SDLK_SCANCODE_MASK, "LEFTOF1" }, // may not produce a proper keycode if the layout has dead keys
        { SDL_SCANCODE_EQUALS | SDLK_SCANCODE_MASK, "LEFTOFDEL" },
        { -1, " " }, // keys below are commented out by default
        { SDLK_PERCENT, "PERCENT" },
        { SDLK_POWER, "POWER" },
        { SDLK_F16, "F16" },
        { SDLK_F17, "F17" },
        { SDLK_F18, "F18" },
        { SDLK_F19, "F19" },
        { SDLK_F20, "F20" },
        { SDLK_F21, "F21" },
        { SDLK_F22, "F22" },
        { SDLK_F23, "F23" },
        { SDLK_F24, "F24" },
        { SDLK_EXECUTE, "EXECUTE" },
        { SDLK_SELECT, "SELECT" },
        { SDLK_STOP, "STOP" },
        { SDLK_AGAIN, "AGAIN" },
        { SDLK_UNDO, "UNDO" },
        { SDLK_CUT, "CUT" },
        { SDLK_COPY, "COPY" },
        { SDLK_PASTE, "PASTE" },
        { SDLK_FIND, "FIND" },
        { SDLK_MUTE, "MUTE" },
        { SDLK_VOLUMEUP, "VOLUMEUP" },
        { SDLK_VOLUMEDOWN, "VOLUMEDOWN" },
        { SDLK_KP_COMMA, "KP_COMMA" },
        { SDLK_KP_EQUALSAS400, "KP_EQUALSAS400" },
        { SDLK_ALTERASE, "ALTERASE" },
        { SDLK_CANCEL, "CANCEL" },
        { SDLK_PRIOR, "PRIOR" },
        { SDLK_RETURN2, "RETURN2" },
        { SDLK_SEPARATOR, "SEPARATOR" },
        { SDLK_OUT, "OUT" },
        { SDLK_OPER, "OPER" },
        { SDLK_CLEARAGAIN, "CLEARAGAIN" },
        { SDLK_CRSEL, "CRSEL" },
        { SDLK_EXSEL, "EXSEL" },
        { SDLK_KP_00, "KP_00" },
        { SDLK_KP_000, "KP_000" },
        { SDLK_THOUSANDSSEPARATOR, "THOUSANDSSEPARATOR" },
        { SDLK_DECIMALSEPARATOR, "DECIMALSEPARATOR" },
        { SDLK_CURRENCYUNIT, "CURRENCYUNIT" },
        { SDLK_CURRENCYSUBUNIT, "CURRENCYSUBUNIT" },
        { SDLK_KP_LEFTPAREN, "KP_LEFTPAREN" },
        { SDLK_KP_RIGHTPAREN, "KP_RIGHTPAREN" },
        { SDLK_KP_LEFTBRACE, "KP_LEFTBRACE" },
        { SDLK_KP_RIGHTBRACE, "KP_RIGHTBRACE" },
        { SDLK_KP_TAB, "KP_TAB" },
        { SDLK_KP_BACKSPACE, "KP_BACKSPACE" },
        { SDLK_KP_A, "KP_A" },
        { SDLK_KP_B, "KP_B" },
        { SDLK_KP_C, "KP_C" },
        { SDLK_KP_D, "KP_D" },
        { SDLK_KP_E, "KP_E" },
        { SDLK_KP_F, "KP_F" },
        { SDLK_KP_XOR, "KP_XOR" },
        { SDLK_KP_POWER, "KP_POWER" },
        { SDLK_KP_PERCENT, "KP_PERCENT" },
        { SDLK_KP_LESS, "KP_LESS" },
        { SDLK_KP_GREATER, "KP_GREATER" },
        { SDLK_KP_AMPERSAND, "KP_AMPERSAND" },
        { SDLK_KP_DBLAMPERSAND, "KP_DBLAMPERSAND" },
        { SDLK_KP_VERTICALBAR, "KP_VERTICALBAR" },
        { SDLK_KP_DBLVERTICALBAR, "KP_DBLVERTICALBAR" },
        { SDLK_KP_COLON, "KP_COLON" },
        { SDLK_KP_HASH, "KP_HASH" },
        { SDLK_KP_SPACE, "KP_SPACE" },
        { SDLK_KP_AT, "KP_AT" },
        { SDLK_KP_EXCLAM, "KP_EXCLAM" },
        { SDLK_KP_MEMSTORE, "KP_MEMSTORE" },
        { SDLK_KP_MEMRECALL, "KP_MEMRECALL" },
        { SDLK_KP_MEMCLEAR, "KP_MEMCLEAR" },
        { SDLK_KP_MEMADD, "KP_MEMADD" },
        { SDLK_KP_MEMSUBTRACT, "KP_MEMSUBTRACT" },
        { SDLK_KP_MEMMULTIPLY, "KP_MEMMULTIPLY" },
        { SDLK_KP_MEMDIVIDE, "KP_MEMDIVIDE" },
        { SDLK_KP_PLUSMINUS, "KP_PLUSMINUS" },
        { SDLK_KP_CLEAR, "KP_CLEAR" },
        { SDLK_KP_CLEARENTRY, "KP_CLEARENTRY" },
        { SDLK_KP_BINARY, "KP_BINARY" },
        { SDLK_KP_OCTAL, "KP_OCTAL" },
        { SDLK_KP_DECIMAL, "KP_DECIMAL" },
        { SDLK_KP_HEXADECIMAL, "KP_HEXADECIMAL" },
        { SDLK_AUDIONEXT, "AUDIONEXT" },
        { SDLK_AUDIOPREV, "AUDIOPREV" },
        { SDLK_AUDIOSTOP, "AUDIOSTOP" },
        { SDLK_AUDIOPLAY, "AUDIOPLAY" },
        { SDLK_AUDIOMUTE, "AUDIOMUTE" },
        { SDLK_MEDIASELECT, "MEDIASELECT" },
        { SDLK_WWW, "WWW" },
        { SDLK_MAIL, "MAIL" },
        { SDLK_CALCULATOR, "CALCULATOR" },
        { SDLK_COMPUTER, "COMPUTER" },
        { SDLK_AC_SEARCH, "AC_SEARCH" },
        { SDLK_AC_HOME, "AC_HOME" },
        { SDLK_AC_BACK, "AC_BACK" },
        { SDLK_AC_FORWARD, "AC_FORWARD" },
        { SDLK_AC_STOP, "AC_STOP" },
        { SDLK_AC_REFRESH, "AC_REFRESH" },
        { SDLK_AC_BOOKMARKS, "AC_BOOKMARKS" },
        { SDLK_BRIGHTNESSDOWN, "BRIGHTNESSDOWN" },
        { SDLK_BRIGHTNESSUP, "BRIGHTNESSUP" },
        { SDLK_DISPLAYSWITCH, "DISPLAYSWITCH" },
        { SDLK_KBDILLUMTOGGLE, "KBDILLUMTOGGLE" },
        { SDLK_KBDILLUMDOWN, "KBDILLUMDOWN" },
        { SDLK_KBDILLUMUP, "KBDILLUMUP" },
        { SDLK_EJECT, "EJECT" },
        { SDLK_SLEEP, "SLEEP" },
        { 0, "" }
        };

    const char *c = "";
    stream *f = openfile("config" PATHDIVS "keymap.cfg", "w");
    if(f)
    {
        f->printf("push sc [ result (|b 0x40000000 $arg1) ]\n\n");
        for(int i = 0; keymaptab[i].keycode; i++)
        {
            if(keymaptab[i].keyname[0] == ' ') c = "//";
            else f->printf(keymaptab[i].keycode < SDLK_SCANCODE_MASK ? "%skeymap %d %s\n" : "%skeymap (sc %d) %s\n", c, keymaptab[i].keycode & (keymaptab[i].keycode > 0 ? (SDLK_SCANCODE_MASK-1) : -1), keymaptab[i].keyname);
        }
        f->printf("\npop sc\nexec config/resetbinds.cfg\n");
        delete f;
    }
}
COMMAND(writekeymap, "");
#endif
