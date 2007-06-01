// main.cpp: initialisation & main loop

#include "cube.h"

void cleanup(char *msg)         // single program exit point;
{
    stop();
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

int scr_w = 1024;
int scr_h = 768;
int VIRTW;

void screenshot(char *imagepath)
{
    SDL_Surface *image = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
    if(!image) return;
    uchar *tmp = new uchar[scr_w*scr_h*3];
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, scr_w, scr_h, GL_RGB, GL_UNSIGNED_BYTE, tmp);
    uchar *dst = (uchar *)image->pixels;
    loopi(scr_h)
    {
        memcpy(dst, &tmp[3*scr_w*(scr_h-i-1)], 3*scr_w);
        endianswap(dst, 3, scr_w);
        dst += image->pitch;
    }
    delete[] tmp;
    if(!imagepath[0])
    {
        static string buf;
        s_sprintf(buf)("screenshots/screenshot_%d.bmp", lastmillis);
        imagepath = buf;
    }
    SDL_SaveBMP(image, path(imagepath));
    SDL_FreeSurface(image);
}

COMMAND(screenshot, ARG_1STR);
COMMAND(quit, ARG_NONE);

SDL_Surface *screen = NULL;

extern void setfullscreen(bool enable);

VARF(fullscreen, 0, 0, 1, setfullscreen(fullscreen!=0));

void setfullscreen(bool enable)
{
    if(enable == !(screen->flags&SDL_FULLSCREEN))
    {
#if defined(WIN32) || defined(__APPLE__)
        conoutf("\"fullscreen\" variable not supported on this platform. Use the -t command-line option.");
        fullscreen = !enable;
#else
        SDL_WM_ToggleFullScreen(screen);
        SDL_WM_GrabInput((screen->flags&SDL_FULLSCREEN) ? SDL_GRAB_ON : SDL_GRAB_OFF);
#endif
    }
}

void screenres(int w, int h, int bpp = 0)
{
#if defined(WIN32) || defined(__APPLE__)
    conoutf("\"screenres\" command not supported on this platform. Use the -w and -h command-line options.");
#else
    SDL_Surface *surf = SDL_SetVideoMode(w, h, bpp, SDL_OPENGL|SDL_RESIZABLE|(screen->flags&SDL_FULLSCREEN));
    if(!surf) return;
    scr_w = w;
    scr_h = h;
    VIRTW = scr_w*VIRTH/scr_h;
    screen = surf;
    glViewport(0, 0, w, h);
#endif
}

COMMAND(screenres, ARG_3INT);

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

int main(int argc, char **argv)
{    
    bool dedicated = false;
    int fs = SDL_FULLSCREEN, depth = 0, bpp = 0, fsaa = 0, vsync = -1, par = 0, uprate = 0, maxcl = DEFAULTCLIENTS, scthreshold = -5;
    char *sdesc = "", *ip = "", *master = NULL, *passwd = "", *maprot = NULL, *adminpwd = NULL, *srvmsg = NULL;

    #define initlog(s) puts("init: " s)
    initlog("sdl");
    
    for(int i = 1; i<argc; i++)
    {
        char *a = &argv[i][2];
        if(argv[i][0]=='-') switch(argv[i][1])
        {
            case 'd': dedicated = true; break;
            case 't': fs     = 0; break;
            case 'w': scr_w  = atoi(a); break;
            case 'h': scr_h  = atoi(a); break;
            case 'z': depth = atoi(a); break;
            case 'b': bpp = atoi(a); break;
            case 'a': fsaa = atoi(a); break;
            case 'v': vsync = atoi(a); break;
            case 'u': uprate = atoi(a); break;
            case 'n': sdesc  = a; break;
            case 'i': ip     = a; break;
            case 'm': master = a; break;
            case 'p': passwd = a; break;
            case 'r': maprot = a; break; 
			case 'x' : adminpwd = a; break;
            case 'c': maxcl  = atoi(a); break;
            case 'o': srvmsg = a; break;
            case 'k': scthreshold = atoi(a); break;
            default:  conoutf("unknown commandline option");
        }
        else conoutf("unknown commandline argument");
    }
    
    #ifdef _DEBUG
    par = SDL_INIT_NOPARACHUTE;
    fs = 0;
    #endif

    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|par)<0) fatal("Unable to initialize SDL");

    initlog("net");
    if(enet_initialize()<0) fatal("Unable to initialise network module");

    initclient();
    initserver(dedicated, uprate, sdesc, ip, master, passwd, maxcl, maprot, adminpwd, srvmsg, scthreshold);  // never returns if dedicated
      
    initlog("world");
    empty_world(7, true);

    initlog("video: sdl");
    if(SDL_InitSubSystem(SDL_INIT_VIDEO)<0) fatal("Unable to initialize SDL Video");

    initlog("video: mode");
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    if(depth) SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depth);
    if(fsaa)
    {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, fsaa);
    }
    if(vsync>=0) SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, vsync);

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
    screen = SDL_SetVideoMode(scr_w, scr_h, bpp, SDL_OPENGL|resize|fs);
    if(screen==NULL) fatal("Unable to create OpenGL screen");
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

    initlog("console");
    if(!execfile("config/font.cfg")) fatal("cannot find font definitions");
    if(!setfont("default")) fatal("no default font specified");

    initlog("gl");
    gl_init(scr_w, scr_h, bpp, depth, fsaa);

    crosshair = textureload("packages/misc/crosshairs/default.png");
    if(!crosshair) fatal("could not find core textures (hint: run AssaultCube from the parent of the bin directory)");

	loadingscreen();

    particleinit();

    initlog("sound");
    initsound();

    initlog("cfg");
    extern void *scoremenu, *teammenu, *ctfmenu, *servmenu, *kickmenu, *banmenu, *docmenu;
    scoremenu = addmenu("score", "frags\tdeath\tpj\tping\tname\tcn", false, renderscores);
    teammenu = addmenu("team score", "frags\tdeath\tpj\tping\tteam\tname\tcn", false, renderscores);
    ctfmenu = addmenu("ctf score", "flags\tfrags\tdeath\tpj\tping\tteam\tname\tcn", false, renderscores);
    servmenu = addmenu("server", "ping\tplr\tserver", true, refreshservers);
	kickmenu = addmenu("kick player", NULL, true, refreshmastermenu);
	banmenu = addmenu("ban player", NULL, true, refreshmastermenu);
    docmenu = addmenu("reference", NULL, true, renderdocmenu);

    persistidents = false;
    exec("config/keymap.cfg");
    exec("config/menus.cfg");
    exec("config/prefabs.cfg");
    exec("config/sounds.cfg");
    exec("config/securemaps.cfg");
    execfile("config/servers.cfg");
    persistidents = true;

    if(!execfile("config/saved.cfg")) exec("config/defaults.cfg");
    execfile("config/autoexec.cfg");

    initlog("models");
    preload_playermodels();
    preload_hudguns();
    preload_entmodels();

    initlog("docs");
    execfile("config/docs.cfg");

    execute("start_game");

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
        static int curmillis = 0, frames = 0;
        static float fps = 10.0f;
        int millis = SDL_GetTicks();
        limitfps(millis, curmillis);
        int elapsed = millis-curmillis;
        curtime = elapsed*gamespeed/100;
        if(curtime>200) curtime = 200;
        else if(curtime<1) curtime = 1;

        cleardlights();

        if(demoplayback && demopaused)
        {
            curtime = 0;
            millis = lastmillis;
        }
        else if(lastmillis) updateworld(curtime, lastmillis);

        lastmillis += curtime;
        curmillis = millis;

        if(!demoplayback) serverslice((int)time(NULL), 0);

        frames++;
        fps = (1000.0f/elapsed+fps*10)/11;

        computeraytable(camera1->o.x, camera1->o.y);
        if(frames>4) SDL_GL_SwapBuffers();
        extern void updatevol(); updatevol();
        if(frames>3) gl_drawframe(scr_w, scr_h, fps<lowfps ? fps/lowfps : (fps>highfps ? fps/highfps : 1.0f), fps);
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
                    if(!(screen->flags&SDL_FULLSCREEN) && grabmouse)
                    {
                        #ifdef __APPLE__
                        if(event.motion.y == 0) break;  //let mac users drag windows via the title bar
                        #endif
                        if(event.motion.x == scr_w / 2 && event.motion.y == scr_h / 2) break;
                        SDL_WarpMouse(scr_w / 2, scr_h / 2);
                    }
                    #ifndef WIN32
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

void loadcrosshair(char *c)
{
	s_sprintfd(p)("packages/misc/crosshairs/%s", c);
    crosshair = textureload(p);
}

COMMAND(loadcrosshair, ARG_1STR);

VAR(version, 1, AC_VERSION, 0);

