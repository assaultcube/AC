// main.cpp: initialisation & main loop

#include "cube.h"

void cleanup(char *msg)         // single program exit point;
{
    stop();
    disconnect(true);
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
    };
    SDL_Quit();
};

void quit()                     // normal exit
{
    writeservercfg();
    writecfg();
    cleanup(NULL);
	exit(EXIT_SUCCESS);
};

void fatal(char *s, char *o)    // failure exit
{
    s_sprintfd(msg)("%s%s (%s)\n", s, o, SDL_GetError());
    cleanup(msg);
	exit(EXIT_FAILURE);
};

int scr_w = 640;
int scr_h = 480;

void screenshot()
{
    SDL_Surface *image;
    SDL_Surface *temp;
    int idx;
    if((image = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0)))
    {
        if((temp  = SDL_CreateRGBSurface(SDL_SWSURFACE, scr_w, scr_h, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0)))
        {
            glReadPixels(0, 0, scr_w, scr_h, GL_RGB, GL_UNSIGNED_BYTE, image->pixels);
            for (idx = 0; idx<scr_h; idx++)
            {
                char *dest = (char *)temp->pixels+3*scr_w*idx;
                memcpy(dest, (char *)image->pixels+3*scr_w*(scr_h-1-idx), 3*scr_w);
                endianswap(dest, 3, scr_w);
            };
            s_sprintfd(buf)("screenshots/screenshot_%d.bmp", lastmillis);
            SDL_SaveBMP(temp, path(buf));
            SDL_FreeSurface(temp);
        };
        SDL_FreeSurface(image);
    };
};

COMMAND(screenshot, ARG_NONE);
COMMAND(quit, ARG_NONE);

static void bar(float bar, int w, int o, float r, float g, float b)
{
    int side = 50;
    glColor3f(r, g, b);
    glVertex2f(side,                  o*FONTH);
    glVertex2f(bar*(w*3-2*side)+side, o*FONTH);
    glVertex2f(bar*(w*3-2*side)+side, (o+2)*FONTH);
    glVertex2f(side,                  (o+2)*FONTH);
};

void show_out_of_renderloop_progress(float bar1, const char *text1, float bar2, const char *text2)   // also used during loading
{
    c2skeepalive();

    int w = scr_w, h = scr_h;

    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, w*3, h*3, 0, -1, 1);

    glBegin(GL_QUADS);

    if(text1)
    {
        bar(1,    w, 1, 0.1f, 0.1f, 0.1f);
        bar(bar1, w, 1, 0.2f, 0.2f, 0.2f);
    };

    if(bar2>0)
    {
        bar(1,    w, 3, 0.1f, 0.1f, 0.1f);
        bar(bar2, w, 3, 0.2f, 0.2f, 0.2f);
    };

    glEnd();

    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    if(text1) draw_text(text1, 70, 1*FONTH + FONTH/2);
    if(bar2>0) draw_text(text2, 70, 3*FONTH + FONTH/2);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_DEPTH_TEST);
    SDL_GL_SwapBuffers();
};

SDL_Surface *screen = NULL;

extern void setfullscreen(bool enable);

VARF(fullscreen, 0, 0, 1, setfullscreen(fullscreen!=0));

void setfullscreen(bool enable)
{
    if(enable == !(screen->flags&SDL_FULLSCREEN))
    {
#ifdef WIN32
        conoutf("\"fullscreen\" variable not supported on this platform. Use the -t command-line option.");
        fullscreen = !enable;
#else
        SDL_WM_ToggleFullScreen(screen);
        SDL_WM_GrabInput((screen->flags&SDL_FULLSCREEN) ? SDL_GRAB_ON : SDL_GRAB_OFF);
#endif
    };
};

void screenres(int w, int h, int bpp = 0)
{
#ifdef WIN32
    conoutf("\"screenres\" command not supported on this platform. Use the -w and -h command-line options.");
#else
    SDL_Surface *surf = SDL_SetVideoMode(w, h, bpp, SDL_OPENGL|SDL_RESIZABLE|(screen->flags&SDL_FULLSCREEN));
    if(!surf) return;
    scr_w = w;
    scr_h = h;
    screen = surf;
    glViewport(0, 0, w, h);
#endif
};

COMMAND(screenres, ARG_3INT);

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
        };
        if(delay > 0)
        {
            SDL_Delay(delay);
            millis += delay;
        };
    };
};

int lowfps = 30, highfps = 40;

void fpsrange(int low, int high)
{
    if(low>high || low<1) return;
    lowfps = low;
    highfps = high;
};

COMMAND(fpsrange, ARG_2INT);

void keyrepeat(bool on)
{
    SDL_EnableKeyRepeat(on ? SDL_DEFAULT_REPEAT_DELAY : 0,
                             SDL_DEFAULT_REPEAT_INTERVAL);
};

VARF(gamespeed, 10, 100, 1000, if(multiplayer()) gamespeed = 100);

int main(int argc, char **argv)
{    
    bool dedicated = false;
    int fs = SDL_FULLSCREEN, depth = 0, bpp = 0, fsaa = 0, par = 0, uprate = 0, maxcl = DEFAULTCLIENTS;
    char *sdesc = "", *ip = "", *master = NULL, *passwd = "", *maprot = NULL, *masterpwd = NULL;

    #define log(s) puts("init: " s)
    log("sdl");
    
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
            case 'u': uprate = atoi(a); break;
            case 'n': sdesc  = a; break;
            case 'i': ip     = a; break;
            case 'm': master = a; break;
            case 'p': passwd = a; break;
            case 'r': maprot = a; break; 
			case 'x' : masterpwd = a; break; // EDIT: AH
            case 'c': maxcl  = atoi(a); break; //EDIT: AH
            default:  conoutf("unknown commandline option");
        }
        else conoutf("unknown commandline argument");
    };
    
    #ifdef _DEBUG
    par = SDL_INIT_NOPARACHUTE;
    fs = 0;
    #endif

    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|par)<0) fatal("Unable to initialize SDL");

    log("net");
    if(enet_initialize()<0) fatal("Unable to initialise network module");

    initclient();
    initserver(dedicated, uprate, sdesc, ip, master, passwd, maxcl, maprot, masterpwd);  // never returns if dedicated
      
    log("world");
    empty_world(7, true);

    log("video: sdl");
    if(SDL_InitSubSystem(SDL_INIT_VIDEO)<0) fatal("Unable to initialize SDL Video");

    log("video: mode");
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    if(depth) SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depth);
    if(fsaa)
    {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, fsaa);
    };
    #ifdef WIN32
        #ifdef SDL_RESIZABLE
        #undef SDL_RESIZABLE
        #endif
        #define SDL_RESIZABLE 0
    #endif
    screen = SDL_SetVideoMode(scr_w, scr_h, bpp, SDL_OPENGL|SDL_RESIZABLE|fs);
    if(screen==NULL) fatal("Unable to create OpenGL screen");
    fullscreen = fs!=0;

    log("video: misc");
    SDL_WM_SetCaption("ActionCube", NULL);
    #ifndef WIN32
    if(fs)
    #endif
    SDL_WM_GrabInput(SDL_GRAB_ON);
    keyrepeat(false);
    SDL_ShowCursor(0);

    log("gl");
    gl_init(scr_w, scr_h, bpp, depth, fsaa);

    log("basetex");
    crosshair = textureload("packages/misc/crosshairs/default.png");
    if(!crosshair) fatal("could not find core textures (hint: run cube from the parent of the bin directory)");

	loadingscreen();

    log("hudgun models");
    preload_hudguns();

    log("sound");
    initsound();

    log("cfg");
    extern void *scoremenu, *servmenu, *ctfmenu, *kickmenu, *banmenu;
    scoremenu = addmenu("frags\tpj\tping\tteam\tname", false, false);
    extern void refreshservers();
    servmenu = addmenu("ping\tplr\tserver", true, false, refreshservers);
    ctfmenu = addmenu("flags\tfrags\tpj\tping\tteam\tname", false, false);
	kickmenu = addmenu("kick player", true, false);
	banmenu = addmenu("ban player", true, false);

    persistidents = false;
    exec("config/keymap.cfg");
    exec("config/menus.cfg");
    exec("config/prefabs.cfg");
    exec("config/sounds.cfg");
    execfile("config/servers.cfg");
    persistidents = true;

    if(!execfile("config/saved.cfg")) exec("config/defaults.cfg");
    execfile("config/autoexec.cfg");

    execute("start_game");

    log("localconnect");
    localconnect();
    changemap("maps/ac_complex");
   
    log("mainloop");
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
        if(lastmillis) updateworld(curtime, lastmillis);

        lastmillis += curtime;
        curmillis = millis;

        if(!demoplayback) serverslice((int)time(NULL), 0);

        frames++;
        fps = (1000.0f/elapsed+fps*10)/11;

        computeraytable(camera1->o.x, camera1->o.y);
        SDL_GL_SwapBuffers();
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

                #ifndef WIN32
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
                    if(ignore) { ignore--; break; };
                    if(!(screen->flags&SDL_FULLSCREEN) && grabmouse)
                    {
                        #ifdef __APPLE__
                        if(event.motion.y == 0) break;  //let mac users drag windows via the title bar
                        #endif
                        if(event.motion.x == scr_w / 2 && event.motion.y == scr_h / 2) break;
                        SDL_WarpMouse(scr_w / 2, scr_h / 2);
                    };
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
            };
        };
#ifdef _DEBUG
		if(millis>lastflush+60000) { 
			fflush(stdout); lastflush = millis; 
		}
#endif
    };
    quit();
    return EXIT_SUCCESS;
};

void loadcrosshair(char *c)
{
	s_sprintfd(p)("packages/misc/crosshairs/%s", c);
    crosshair = textureload(p);
};

COMMAND(loadcrosshair, ARG_1STR);

VAR(version, 1, 920, 0);

