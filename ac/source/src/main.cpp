// main.cpp: initialisation & main loop

#include "cube.h"

void cleanup(char *msg)         // single program exit point;
{
    abortconnect();
    disconnect(1);
    cleangl();
    cleansound();
    cleanupserver();
    SDL_ShowCursor(1);
    if(msg)
    {
        #ifdef WIN32
        MessageBox(NULL, msg, "cube fatal error", MB_OK|MB_SYSTEMMODAL);
        #else
        printf(msg);
        #endif
    }
    SDL_Quit();
}

void quit()                     // normal exit
{
    extern void writeinitcfg();
    writeinitcfg();
    writeservercfg();
    writecfg();
    cleanup(NULL);
	exit(EXIT_SUCCESS);
}

void fatal(char *s, char *o)    // failure exit
{
    s_sprintfd(msg)("%s%s (%s)\n", s, o, SDL_GetError());
    cleanup(msg);
	exit(EXIT_FAILURE);
}

SDL_Surface *screen = NULL;

static bool initing = false, restoredinits = false;
bool initwarning()
{
    if(!initing)
    {
        if(restoredinits) conoutf("Please restart AssaultCube for this setting to take effect.");
        else conoutf("Please restart AssaultCube with the --init command-line option for this setting to take effect.");
    }
    return !initing;
}

VARF(scr_w, 0, 1024, 10000, initwarning());
VARF(scr_h, 0, 768, 10000, initwarning());
VARF(colorbits, 0, 0, 32, initwarning());
VARF(depthbits, 0, 0, 32, initwarning());
VARF(fsaa, 0, 0, 16, initwarning());
VARF(vsync, -1, -1, 1, initwarning());

void writeinitcfg()
{
    s_sprintfd(fn)("config%cinit.cfg", PATHDIV);
    FILE *f = openfile(fn, "w");
    if(!f) return;
    fprintf(f, "// automatically written on exit, DO NOT MODIFY\n// modify settings in game\n");
    fprintf(f, "scr_w %d\n", scr_w);
    fprintf(f, "scr_h %d\n", scr_h);
    fprintf(f, "colorbits %d\n", colorbits);
    fprintf(f, "depthbits %d\n", depthbits);
    fprintf(f, "fsaa %d\n", fsaa);
    fprintf(f, "vsync %d\n", vsync);
    extern int soundchans, soundfreq, soundbufferlen;
    fprintf(f, "soundchans %d\n", soundchans);
    fprintf(f, "soundfreq %d\n", soundfreq);
    fprintf(f, "soundbufferlen %d\n", soundbufferlen);
    fclose(f);
}

void screenshot(char *imagepath)
{
    SDL_Surface *image = SDL_CreateRGBSurface(SDL_SWSURFACE, screen->w, screen->h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
    if(!image) return;
    uchar *tmp = new uchar[screen->w*screen->h*3];
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, screen->w, screen->h, GL_RGB, GL_UNSIGNED_BYTE, tmp);
    uchar *dst = (uchar *)image->pixels;
    loopi(scr_h)
    {
        memcpy(dst, &tmp[3*screen->w*(screen->h-i-1)], 3*screen->w);
        endianswap(dst, 3, screen->w);
        dst += image->pitch;
    }
    delete[] tmp;
    if(!imagepath[0])
    {
        static string buf;
        s_sprintf(buf)("screenshots/screenshot_%d.bmp", lastmillis);
        imagepath = buf;
    }
    SDL_SaveBMP(image, findfile(path(imagepath), "wb"));
    SDL_FreeSurface(image);
}

COMMAND(screenshot, ARG_1STR);
COMMAND(quit, ARG_NONE);

void setfullscreen(bool enable)
{
    if(enable == !(screen->flags&SDL_FULLSCREEN))
    {
#if defined(WIN32) || defined(__APPLE__)
        conoutf("\"fullscreen\" variable not supported on this platform. Use the -t command-line option.");
        extern int fullscreen;
        fullscreen = !enable;
#else
        SDL_WM_ToggleFullScreen(screen);
        SDL_WM_GrabInput((screen->flags&SDL_FULLSCREEN) ? SDL_GRAB_ON : SDL_GRAB_OFF);
#endif
    }
}

void screenres(int w, int h)
{
#if !defined(WIN32) && !defined(__APPLE__)
    if(initing)
    {
#endif
        scr_w = w;
        scr_h = h;
#if defined(WIN32) || defined(__APPLE__)
        initwarning();
#else
        return;
    }
    SDL_Surface *surf = SDL_SetVideoMode(w, h, 0, SDL_OPENGL|SDL_RESIZABLE|(screen->flags&SDL_FULLSCREEN));
    if(!surf) return;
    screen = surf;
    scr_w = screen->w;
    scr_h = screen->h;
    glViewport(0, 0, scr_w, scr_h);
    VIRTW = scr_w*VIRTH/scr_h;
#endif
}
#if defined(WIN32) || defined(__APPLE__) || !defined(WIN32)
void setresdata(char *s, enet_uint32 c)
{
    extern hashtable<char *, enet_uint32> &resdata;
    resdata[newstring(s)] = c;
}
#endif

VARF(fullscreen, 0, 0, 1, setfullscreen(fullscreen!=0));

COMMAND(screenres, ARG_2INT);

VARFP(gamma, 30, 100, 300,
{
    float f = gamma/100.0f;
    if(SDL_SetGamma(f,f,f)==-1)
    {
        conoutf("Could not set gamma (card/driver doesn't support it?)");
        conoutf("sdl: %s", SDL_GetError());
    }
});

VARP(maxfps, 5, 200, 500);

void limitfps(int &millis, int curmillis)
{
    static int fpserror = 0;
    int delay = 1000/maxfps - (millis-curmillis);
    if(delay < 0) fpserror = 0;
    else
    {
        fpserror += 1000%maxfps;
        if(fpserror >= maxfps)
        {
            ++delay;
            fpserror -= maxfps;
        }
        if(delay > 0)
        {
            SDL_Delay(delay);
            millis += delay;
        }
    }
}

int lowfps = 30, highfps = 40;

void fpsrange(int low, int high)
{
    if(low>high || low<1) return;
    lowfps = low;
    highfps = high;
}

COMMAND(fpsrange, ARG_2INT);

void keyrepeat(bool on)
{
    SDL_EnableKeyRepeat(on ? SDL_DEFAULT_REPEAT_DELAY : 0,
                             SDL_DEFAULT_REPEAT_INTERVAL);
}

VARF(gamespeed, 10, 100, 1000, if(multiplayer()) gamespeed = 100);

static int clockrealbase = 0, clockvirtbase = 0;
static void clockreset() { clockrealbase = SDL_GetTicks(); clockvirtbase = totalmillis; }
VARFP(clockerror, 990000, 1000000, 1010000, clockreset());
VARFP(clockfix, 0, 0, 1, clockreset());

int main(int argc, char **argv)
{    
    bool dedicated = false;
    int fs = SDL_FULLSCREEN, par = 0, uprate = 0, maxcl = DEFAULTCLIENTS, scthreshold = -5, port = 0;
    char *sdesc = "", *ip = "", *master = NULL, *passwd = "", *maprot = NULL, *adminpwd = NULL, *srvmsg = NULL;

    #define initlog(s) puts("init: " s)
    initlog("sdl");
    
    initing = true;
    for(int i = 1; i<argc; i++)
    {
        char *a = &argv[i][2];
        if(argv[i][0]=='-') switch(argv[i][1])
        {
            case '-':
                if(!strncmp(argv[i], "--home=", 7)) sethomedir(&argv[i][7]);
                else if(!strncmp(argv[i], "--mod=", 6)) addpackagedir(&argv[i][6]);
                else if(!strcmp(argv[i], "--init"))
                {
                    execfile((char *)"config/init.cfg");
                    restoredinits = true;
                }
                else if(!strncmp(argv[i], "--init=", 7))
                {
                    execfile(&argv[i][7]);
                    restoredinits = true;
                }
                break;
            case 'd': dedicated = true; break;
            case 't': fs     = 0; break;
            case 'w': scr_w  = atoi(a); break;
            case 'h': scr_h  = atoi(a); break;
            case 'z': depthbits = atoi(a); break;
            case 'b': colorbits = atoi(a); break;
            case 'a': fsaa = atoi(a); break;
            case 'v': vsync = atoi(a); break;
            case 'u': uprate = atoi(a); break;
            case 'n': sdesc  = a; break;
            case 'i': ip     = a; break;
            case 'm': master = a; break;
            case 'p': passwd = a; break;
            case 'r': maprot = a; break; 
			case 'x': adminpwd = a; break;
            case 'c': maxcl  = atoi(a); break;
            case 'o': srvmsg = a; break;
            case 'k': scthreshold = atoi(a); break;
            case 'f': port = atoi(a); break;
            default:  conoutf("unknown commandline option");
        }
        else conoutf("unknown commandline argument");
    }
    initing = false;

    #ifdef _DEBUG
    par = SDL_INIT_NOPARACHUTE;
    fs = 0;
    #endif

    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|par)<0) fatal("Unable to initialize SDL");

    initlog("net");
    if(enet_initialize()<0) fatal("Unable to initialise network module");

    initclient();
    initserver(dedicated, uprate, sdesc, ip, port, master, passwd, maxcl, maprot, adminpwd, srvmsg, scthreshold);  // never returns if dedicated
      
    initlog("world");
    empty_world(7, true);

    initlog("video: sdl");
    if(SDL_InitSubSystem(SDL_INIT_VIDEO)<0) fatal("Unable to initialize SDL Video");

    initlog("video: mode");
    int resize = SDL_RESIZABLE;
    #if defined(WIN32) || defined(__APPLE__)
    resize = 0;
    #endif
    SDL_Rect **modes = SDL_ListModes(NULL, SDL_OPENGL|resize|fs);
    if(modes && modes!=(SDL_Rect **)-1)
    {
        bool hasmode = false;
        for(int i = 0; modes[i]; i++)
        {
            if(scr_w <= modes[i]->w && scr_h <= modes[i]->h) { hasmode = true; break; }
        }
        if(!hasmode) { scr_w = modes[0]->w; scr_h = modes[0]->h; }
    }
    bool hasbpp = true;
    if(colorbits && modes) 
        hasbpp = SDL_VideoModeOK(modes!=(SDL_Rect **)-1 ? modes[0]->w : scr_w, modes!=(SDL_Rect **)-1 ? modes[0]->h : scr_h, colorbits, SDL_OPENGL|resize|fs)==colorbits;

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); 
#if SDL_VERSION_ATLEAST(1, 2, 11)
    if(vsync>=0) SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, vsync);
#endif
    static int configs[] =
    {
        0x3, /* try everything */
        0x2, 0x1, /* try disabling one at a time */
        0 /* try disabling everything */
    };
    int config = 0;
    loopi(sizeof(configs)/sizeof(configs[0]))
    {
        config = configs[i];
        if(!depthbits && config&1) continue;
        if(!fsaa && config&2) continue;
        if(depthbits) SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, config&1 ? depthbits : 0);
        if(fsaa)
        {
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, config&2 ? 1 : 0);
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, config&2 ? fsaa : 0);
        }
        screen = SDL_SetVideoMode(scr_w, scr_h, hasbpp ? colorbits : 0, SDL_OPENGL|resize|fs);
        if(screen) break;
    }
    if(!screen) fatal("Unable to create OpenGL screen: ", SDL_GetError());
    else
    {
        if(!hasbpp) conoutf("%d bit color buffer not supported - disabling", colorbits);
        if(depthbits && (config&1)==0) conoutf("%d bit z-buffer not supported - disabling", depthbits);
        if(fsaa && (config&2)==0) conoutf("%dx anti-aliasing not supported - disabling", fsaa);
    }

    scr_w = screen->w;
    scr_h = screen->h;

    fullscreen = fs!=0;
    VIRTW = scr_w*VIRTH/scr_h;

    initlog("video: misc");
    SDL_WM_SetCaption("AssaultCube", NULL);
    #ifndef WIN32
    if(fs)
    #endif
    SDL_WM_GrabInput(SDL_GRAB_ON);
    keyrepeat(false);
    SDL_ShowCursor(0);

    initlog("gl");

    gl_init(scr_w, scr_h, hasbpp ? colorbits : 0, config&1 ? depthbits : 0, config&2 ? fsaa : 0);
    
    notexture = textureload("packages/misc/notexture.png");
    if(!notexture) fatal("could not find core textures (hint: run AssaultCube from the parent of the bin directory)");

    initlog("console");
    if(!execfile("config/font.cfg")) fatal("cannot find font definitions");
    if(!setfont("default")) fatal("no default font specified");

	loadingscreen();

    particleinit();

    initlog("sound");
    initsound();

    initlog("cfg");
    extern void *scoremenu, *teammenu, *ctfmenu, *servmenu, *kickmenu, *banmenu, *forceteammenu, *givemastermenu, *docmenu;
    scoremenu = addmenu("score", "frags\tdeath\tpj\tping\tname\tcn", false, renderscores);
    teammenu = addmenu("team score", "frags\tdeath\tpj\tping\tteam\tname\tcn", false, renderscores);
    ctfmenu = addmenu("ctf score", "flags\tfrags\tdeath\tpj\tping\tteam\tname\tcn", false, renderscores);
    servmenu = addmenu("server", "ping\tplr\tserver", true, refreshservers);
	kickmenu = addmenu("kick player", NULL, true, refreshsopmenu);
	banmenu = addmenu("ban player", NULL, true, refreshsopmenu);
    forceteammenu = addmenu("force team", NULL, true, refreshsopmenu);
    givemastermenu = addmenu("give master", NULL, true, refreshsopmenu);
    docmenu = addmenu("reference", NULL, true, renderdocmenu);

    persistidents = false;
    exec("config/keymap.cfg");
    exec("config/menus.cfg");
    exec("config/prefabs.cfg");
    exec("config/sounds.cfg");
    exec("config/securemaps.cfg");
    execfile("config/servers.cfg");
    persistidents = true;

    static char resdata[] = { 112, 97, 99, 107, 97, 103, 101, 115, 47, 116, 101, 120, 116, 117, 114, 101, 115, 47, 107, 117, 114, 116, 47, 107, 108, 105, 116, 101, 50, 46, 106, 112, 103, 0 };
    gzFile f = gzopen(resdata, "rb9");
    if(f)
    {
        int n;
        gzread(f, &n, sizeof(int));
        endianswap(&n, sizeof(int), 1);
        loopi(n)
        {
            string s;
            gzread(f, s, sizeof(string));
            enet_uint32 c;
            gzread(f, &c, sizeof(enet_uint32));
            setresdata(s, c);
        }
        gzclose(f);
    }

    if(!execfile("config/saved.cfg")) exec("config/defaults.cfg");
    execfile("config/autoexec.cfg");

    initlog("models");
    preload_playermodels();
    preload_hudguns();
    preload_entmodels();

    initlog("docs");
    execfile("config/docs.cfg");

    initlog("localconnect");
    localconnect();
    changemap("maps/ac_complex");
   
    initlog("mainloop");
    int ignore = 5, grabmouse = 0;
#ifdef _DEBUG
	int lastflush = 0;
#endif
    for(;;)
    {
        static int frames = 0;
        static float fps = 10.0f;
        int millis = SDL_GetTicks() - clockrealbase;
        if(clockfix) millis = int(millis*(double(clockerror)/1000000));
        millis += clockvirtbase;
        if(millis<totalmillis) millis = totalmillis;
        limitfps(millis, totalmillis);
        int elapsed = millis-totalmillis;
        curtime = elapsed*gamespeed/100;
        //if(curtime>200) curtime = 200;
        //else if(curtime<1) curtime = 1;

        cleardlights();

        if(lastmillis) updateworld(curtime, lastmillis);

        lastmillis += curtime;
        totalmillis = millis;

        serverslice(0);

        frames++;

        computeraytable(camera1->o.x, camera1->o.y);
        if(frames>4) SDL_GL_SwapBuffers();
        extern void updatevol(); updatevol();
        if(frames>3) 
        {
            fps = (1000.0f/elapsed+fps*10)/11;
            gl_drawframe(screen->w, screen->h, fps<lowfps ? fps/lowfps : (fps>highfps ? fps/highfps : 1.0f), fps);
        }
        //SDL_Delay(100);
        SDL_Event event;
        int lasttype = 0, lastbut = 0;
        while(SDL_PollEvent(&event))
        {
            switch(event.type)
            {
                case SDL_QUIT:
                    break;

                #if !defined(WIN32) && !defined(__APPLE__)
                case SDL_VIDEORESIZE:
                    screenres(event.resize.w, event.resize.h);
                    break;
                #endif

                case SDL_KEYDOWN: 
                case SDL_KEYUP: 
                    keypress(event.key.keysym.sym, event.key.state==SDL_PRESSED, event.key.keysym.unicode);
                    break;

                case SDL_ACTIVEEVENT:
                    if(event.active.state & SDL_APPINPUTFOCUS)
                        grabmouse = event.active.gain;
                    else
                    if(event.active.gain)
                        grabmouse = 1;
                    break;

                case SDL_MOUSEMOTION:
                    if(ignore) { ignore--; break; }
                    #ifndef WIN32
                    if(!(screen->flags&SDL_FULLSCREEN) && grabmouse)
                    {
                        #ifdef __APPLE__
                        if(event.motion.y == 0) break;  //let mac users drag windows via the title bar
                        #endif
                        if(event.motion.x == screen->w / 2 && event.motion.y == screen->h / 2) break;
                        SDL_WarpMouse(screen->w / 2, screen->h / 2);
                    }
                    if((screen->flags&SDL_FULLSCREEN) || grabmouse)
                    #endif
                    mousemove(event.motion.xrel, event.motion.yrel);
                    break;

                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                    if(lasttype==event.type && lastbut==event.button.button) break; // why?? get event twice without it
                    keypress(-event.button.button, event.button.state!=0, 0);
                    lasttype = event.type;
                    lastbut = event.button.button;
                    break;
            }
        }
#ifdef _DEBUG
		if(millis>lastflush+60000) { 
			fflush(stdout); lastflush = millis; 
		}
#endif
    }
    quit();
    return EXIT_SUCCESS;
}

VAR(version, 1, AC_VERSION, 0);

