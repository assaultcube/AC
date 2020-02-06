// main.cpp: initialisation & main loop

#include "cube.h"

void cleanup(char *msg)         // single program exit point;
{
    if(!msg)
    {
        cleanupclient();
        audiomgr.soundcleanup();
        cleanupserver();

        extern void cleargamma();
        cleargamma();
    }
    SDL_ShowCursor(1);
    if(msg)
    {
        #ifdef WIN32
        MessageBox(NULL, msg, "AssaultCube fatal error", MB_OK|MB_SYSTEMMODAL|MB_ICONERROR);
        #else
        printf("%s", msg);
        #endif
    }
    SDL_Quit();
}

VAR(resetcfg, 0, 0, 1);

void quit()                     // normal exit
{
    const char *onquit = getalias("onQuit");
    if(onquit && onquit[0]) { execute(onquit); alias("onQuit", ""); }
    extern void writeinitcfg();
    writeinitcfg();
    writeservercfg();
    writepcksourcecfg();
    if(resetcfg) deletecfg();
    else writecfg();
    cleanup(NULL);
    popscontext();
    exit(EXIT_SUCCESS);
}

void fatal(const char *s, ...)    // failure exit
{
    static int errors = 0;
    errors++;

    if(errors <= 2)
    {
        defvformatstring(msg,s,s);
        if(errors <= 1)
        {
            defvformatstring(msg,s,s);
            defformatstring(msgerr)("%s (%s)\n", msg, SDL_GetError());
            cleanup(msgerr);
        }
        else puts(msg);
    }
    exit(EXIT_FAILURE);
}

SDL_Surface *screen = NULL;

static int initing = NOT_INITING;
static bool restoredinits = false;

bool initwarning(const char *desc, int level, int type)
{
    if(initing < level)
    {
        addchange(desc, type);
        return true;
    }
    return false;
}

VARF(scr_w, 320, 1024, 10000, initwarning("screen resolution"));
VARF(scr_h, 200, 768, 10000, initwarning("screen resolution"));
VARF(colorbits, 0, 0, 32, initwarning("color depth"));
VARF(depthbits, 0, 0, 32, initwarning("depth-buffer precision"));
VARF(stencilbits, 0, 0, 32, initwarning("stencil-buffer precision"));
VARF(fsaa, -1, -1, 16, initwarning("anti-aliasing"));
VARF(vsync, -1, -1, 1, initwarning("vertical sync"));

static bool grabinput = false, minimized = false;

void inputgrab(bool on)
{
#ifndef WIN32
    if(!(screen->flags & SDL_FULLSCREEN)) SDL_WM_GrabInput(SDL_GRAB_OFF);
    else
#endif
    SDL_WM_GrabInput(on ? SDL_GRAB_ON : SDL_GRAB_OFF);
    SDL_ShowCursor(on ? SDL_DISABLE : SDL_ENABLE);
}

void setfullscreen(bool enable)
{
    if(!screen) return;
#if defined(WIN32) || defined(__APPLE__)
    initwarning(enable ? "fullscreen" : "windowed");
#else
    if(enable == !(screen->flags&SDL_FULLSCREEN))
    {
        SDL_WM_ToggleFullScreen(screen);
        inputgrab(grabinput);
    }
#endif
}

#ifdef _DEBUG
VARF(fullscreen, 0, 0, 1, setfullscreen(fullscreen!=0));
#else
VARF(fullscreen, 0, 1, 1, setfullscreen(fullscreen!=0));
#endif

void writeinitcfg()
{
    if(!restoredinits) return;
    stream *f = openfile(path("config/init.cfg", true), "w");
    if(!f) return;
    f->printf("// automatically written on exit, DO NOT MODIFY\n// modify settings in game\n");
    extern int fullscreen;
    f->printf("fullscreen %d\n", fullscreen);
    f->printf("scr_w %d\n", scr_w);
    f->printf("scr_h %d\n", scr_h);
    f->printf("colorbits %d\n", colorbits);
    f->printf("depthbits %d\n", depthbits);
    f->printf("stencilbits %d\n", stencilbits);
    f->printf("fsaa %d\n", fsaa);
    f->printf("vsync %d\n", vsync);
    extern int audio, soundchannels;
    f->printf("audio %d\n", audio > 0 ? 1 : 0);
    f->printf("soundchannels %d\n", soundchannels);
    f->printf("lang %s\n", lang);
    delete f;
}

#if 0
VARP(highprocesspriority, 0, 1, 1);

void setprocesspriority(bool high)
{
#if defined(WIN32) && !defined(_DEBUG)
    SetPriorityClass(GetCurrentProcess(), high && highprocesspriority && fullscreen ? HIGH_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS);
#endif
}
#endif

const char *screenshotpath(const char *imagepath, const char *suffix)
{
    static string buf;
    if(imagepath && imagepath[0]) copystring(buf, imagepath);
    else
    {
        if(getclientmap()[0])
            formatstring(buf)("screenshots/%s_%s_%s.%s", timestring(), behindpath(getclientmap()), modestr(gamemode, true), suffix);
        else
            formatstring(buf)("screenshots/%s.%s", timestring(), suffix);
    }
    path(buf);
    return buf;
}

FVARP(screenshotscale, 0.1f, 1.0f, 1.0f);

void bmp_screenshot(const char *imagepath, bool mapshot = false)
{
    extern int minimaplastsize;
    int iw = mapshot?minimaplastsize:screen->w;
    int ih = mapshot?minimaplastsize:screen->h;
    int tw = mapshot ? iw : iw*screenshotscale;
    int th = mapshot ? ih : ih*screenshotscale;
    SDL_Surface *image = creatergbsurface(tw, th);
    if(!image) return;
    uchar *tmp = new uchar[iw*ih*3];
    uchar *dst = (uchar *)image->pixels;
    if(mapshot)
    {
        extern GLuint minimaptex;
        if(minimaptex)
        {
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glBindTexture(GL_TEXTURE_2D, minimaptex);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, tmp);
        }
        else
        {
            conoutf("no mapshot prepared!");
            return;
        }
    }
    else
    {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        if(screenshotscale != 1.0f)
        {
            uchar *buf = new uchar[iw*ih*3];
            glReadPixels(0, 0, iw, ih, GL_RGB, GL_UNSIGNED_BYTE, buf); //screen->w//screen->h
            scaletexture(buf, iw, ih, 3, tmp, tw, th);
            delete[] buf;
        }
        else glReadPixels(0, 0, tw, th, GL_RGB, GL_UNSIGNED_BYTE, tmp);
    }
    loopi(th)
    {
        memcpy(dst, &tmp[3*tw*(th-i-1)], 3*tw);
        dst += image->pitch;
    }
    delete[] tmp;
    const char *filename = screenshotpath(imagepath, "bmp");
    stream *file = openfile(filename, "wb");
    if(!file) conoutf("failed to create: %s", filename);
    else
    {
        SDL_SaveBMP_RW(image, file->rwops(), 1);
        delete file;
    }
    SDL_FreeSurface(image);
}

// best: 100 - good: 85 [default] - bad: 70 - terrible: 50
VARP(jpegquality, 10, 85, 100);

#include "jpegenc.h"

void jpeg_screenshot(const char *imagepath, bool mapshot = false)
{
    extern int minimaplastsize;
    int iw = mapshot?minimaplastsize:screen->w;
    int ih = mapshot?minimaplastsize:screen->h;
    int tw = mapshot ? iw : iw*screenshotscale;
    int th = mapshot ? ih : ih*screenshotscale;

    uchar *pixels = new uchar[3*tw*th];

    if(mapshot)
    {
        extern GLuint minimaptex;
        if(minimaptex)
        {
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glBindTexture(GL_TEXTURE_2D, minimaptex);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_BGR, GL_UNSIGNED_BYTE, pixels);
        }
        else
        {
            conoutf("no mapshot prepared!");
            delete[] pixels;
            return;
        }
    }
    else
    {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        if(screenshotscale != 1.0f)
        {
            uchar *buf = new uchar[iw*ih*3];
            glReadPixels(0, 0, iw, ih, GL_BGR, GL_UNSIGNED_BYTE, buf);
            scaletexture(buf, iw, ih, 3, pixels, tw, th);
            delete[] buf;
        }
        else glReadPixels(0, 0, tw, th, GL_BGR, GL_UNSIGNED_BYTE, pixels);

        int stride = 3*tw;

        GLubyte *swapline = (GLubyte *) malloc(stride);
        for(int row = 0; row < th/2; row++)
        {
            memcpy(swapline, pixels + row * stride, stride);
            memcpy(pixels + row * stride, pixels + (th - row - 1) * stride, stride);
            memcpy(pixels + (th - row -1) * stride, swapline, stride);
        }
        free(swapline);
    }

    const char *filename = findfile(screenshotpath(imagepath, "jpg"), "wb");
    conoutf("writing to file: %s", filename);

    jpegenc *jpegencoder = new jpegenc;
    jpegencoder->encode(filename, (colorRGB *)pixels, tw, th, jpegquality);
    delete jpegencoder;

    delete[] pixels;
}

VARP(pngcompress, 0, 9, 9);

void writepngchunk(stream *f, const char *type, uchar *data = NULL, uint len = 0)
{
    f->putbig<uint>(len);
    f->write(type, 4);
    f->write(data, len);

    uint crc = crc32(0, Z_NULL, 0);
    crc = crc32(crc, (const Bytef *)type, 4);
    if(data) crc = crc32(crc, data, len);
    f->putbig<uint>(crc);
}

int save_png(const char *filename, SDL_Surface *image)
{
    uchar *data = (uchar *)image->pixels;
    int iw = image->w, ih = image->h, pitch = image->pitch;

    stream *f = openfile(filename, "wb");
    if(!f) { conoutf("could not write to %s", filename); return -1; }

    uchar signature[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    f->write(signature, sizeof(signature));

    struct pngihdr
    {
        uint width, height;
        uchar bitdepth, colortype, compress, filter, interlace;
    } ihdr = { bigswap<uint>(iw), bigswap<uint>(ih), 8, 2, 0, 0, 0 };
    writepngchunk(f, "IHDR", (uchar *)&ihdr, 13);

    int idat = f->tell();
    uint len = 0;
    f->write("\0\0\0\0IDAT", 8);
    uint crc = crc32(0, Z_NULL, 0);
    crc = crc32(crc, (const Bytef *)"IDAT", 4);

    z_stream z;
    z.zalloc = NULL;
    z.zfree = NULL;
    z.opaque = NULL;

    if(deflateInit(&z, pngcompress) != Z_OK)
        goto error;

    uchar buf[1<<12];
    z.next_out = (Bytef *)buf;
    z.avail_out = sizeof(buf);

    loopi(ih)
    {
        uchar filter = 0;
        loopj(2)
        {
            z.next_in = j ? (Bytef *)data + i*pitch : (Bytef *)&filter;
            z.avail_in = j ? iw*3 : 1;
            while(z.avail_in > 0)
            {
                if(deflate(&z, Z_NO_FLUSH) != Z_OK) goto cleanuperror;
                #define FLUSHZ do { \
                    int flush = sizeof(buf) - z.avail_out; \
                    crc = crc32(crc, buf, flush); \
                    len += flush; \
                    f->write(buf, flush); \
                    z.next_out = (Bytef *)buf; \
                    z.avail_out = sizeof(buf); \
                } while(0)
                FLUSHZ;
            }
        }
    }

    for(;;)
    {
        int err = deflate(&z, Z_FINISH);
        if(err != Z_OK && err != Z_STREAM_END) goto cleanuperror;
        FLUSHZ;
        if(err == Z_STREAM_END) break;
    }

    deflateEnd(&z);

    f->seek(idat, SEEK_SET);
    f->putbig<uint>(len);
    f->seek(0, SEEK_END);
    f->putbig<uint>(crc);

    writepngchunk(f, "IEND");

    delete f;
    return 0;

cleanuperror:
    deflateEnd(&z);

error:
    delete f;

    return -1;
}

void png_screenshot(const char *imagepath, bool mapshot = false)
{
    extern int minimaplastsize;
    int iw = mapshot?minimaplastsize:screen->w;
    int ih = mapshot?minimaplastsize:screen->h;
    int tw = mapshot ? iw : iw*screenshotscale;
    int th = mapshot ? ih : ih*screenshotscale;

    SDL_Surface *image = creatergbsurface(tw, th);
    if(!image) return;

    uchar *tmp = new uchar[tw*th*3];
    uchar *dst = (uchar *)image->pixels;

    if(mapshot)
    {
        extern GLuint minimaptex;
        if(minimaptex)
        {
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glBindTexture(GL_TEXTURE_2D, minimaptex);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, tmp);
        }
        else
        {
            conoutf("no mapshot prepared!");
            return;
        }
    }
    else
    {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        if(screenshotscale != 1.0f)
        {
            uchar *buf = new uchar[iw*ih*3];
            glReadPixels(0, 0, iw, ih, GL_RGB, GL_UNSIGNED_BYTE, buf);
            scaletexture(buf, iw, ih, 3, tmp, tw, th);
            delete[] buf;
        }
        else glReadPixels(0, 0, tw, th, GL_RGB, GL_UNSIGNED_BYTE, tmp);
    }
    loopi(th)
    {
        memcpy(dst, &tmp[3*tw*(th-i-1)], 3*tw);
        dst += image->pitch;
    }
    delete[] tmp;

    const char *filename = screenshotpath(imagepath, "png");
    if(save_png(filename, image) < 0) conoutf("\f3Error saving png file");

    SDL_FreeSurface(image);
}

VARP(screenshottype, 0, 1, 2);
void screenshot(const char *imagepath)
{
    switch(screenshottype)
    {
        case 2:  png_screenshot(imagepath,false); break;
        case 1: jpeg_screenshot(imagepath,false); break;
        case 0:
        default: bmp_screenshot(imagepath,false); break;
    }
}

void mapshot()
{
    string suffix;
    switch(screenshottype)
    {
        case 2: copystring(suffix, "png"); break;
        case 1: copystring(suffix, "jpg"); break;
        case 0:
        default: copystring(suffix, "bmp"); break;
    }
    defformatstring(buf)("screenshots/mapshot_%s_%s.%s", behindpath(getclientmap()), timestring(), suffix);
    switch(screenshottype)
    {
        case 2: png_screenshot(buf,true); break;
        case 1: jpeg_screenshot(buf,true); break;
        case 0:
        default: bmp_screenshot(buf,true); break;
    }
}

bool needsautoscreenshot = false;

COMMAND(screenshot, "s");
COMMAND(mapshot, "");
COMMAND(quit, "");

void screenres(int w, int h)
{
#if !defined(WIN32) && !defined(__APPLE__)
    if(initing >= INIT_RESET)
    {
#endif
        scr_w = w;
        scr_h = h;
#if defined(WIN32) || defined(__APPLE__)
        initwarning("screen resolution");
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

COMMANDF(screenres, "ii", (int *w, int *h) { screenres(*w, *h); });

static int curgamma = 100;
VARNFP(gamma, vgamma, 30, 100, 300,
{
    if(vgamma == curgamma) return;
    curgamma = vgamma;
    float f = vgamma/100.0f;
    if(SDL_SetGamma(f,f,f)==-1) conoutf("Could not set gamma: %s", SDL_GetError());
});

void cleargamma()
{
    if(curgamma != 100) SDL_SetGamma(1, 1, 1);
}

void restoregamma()
{
    if(curgamma == 100) return;
    float f = curgamma/100.0f;
    SDL_SetGamma(1, 1, 1);
    SDL_SetGamma(f, f, f);
}

void setupscreen(int &usedcolorbits, int &useddepthbits, int &usedfsaa)
{
    int flags = SDL_RESIZABLE;
    #if defined(WIN32) || defined(__APPLE__)
    flags = 0;
    putenv("SDL_VIDEO_CENTERED=1"); //Center window
    #endif
    if(fullscreen) flags |= SDL_FULLSCREEN;
    SDL_Rect **modes = SDL_ListModes(NULL, SDL_OPENGL|flags);
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
        hasbpp = SDL_VideoModeOK(modes!=(SDL_Rect **)-1 ? modes[0]->w : scr_w, modes!=(SDL_Rect **)-1 ? modes[0]->h : scr_h, colorbits, SDL_OPENGL|flags)==colorbits;

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#if SDL_VERSION_ATLEAST(1, 2, 11)
    if(vsync>=0) SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, vsync);
#endif
    static int configs[] =
    {
        0x7, /* try everything */
        0x6, 0x5, 0x3, /* try disabling one at a time */
        0x4, 0x2, 0x1, /* try disabling two at a time */
        0 /* try disabling everything */
    };
    int config = 0;
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    if(!depthbits) SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    if(!fsaa)
    {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    }
    loopi(sizeof(configs)/sizeof(configs[0]))
    {
        config = configs[i];
        if(!depthbits && config&1) continue;
        if(!stencilbits && config&2) continue;
        if(fsaa<=0 && config&4) continue;
        if(depthbits) SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, config&1 ? depthbits : 16);
        if(stencilbits)
        {
            SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, config&2 ? stencilbits : 0);
            hasstencil = (config&2)!=0;
        }
        else hasstencil = false;
        if(fsaa>0)
        {
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, config&4 ? 1 : 0);
            SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, config&4 ? fsaa : 0);
        }
        screen = SDL_SetVideoMode(scr_w, scr_h, hasbpp ? colorbits : 0, SDL_OPENGL|flags);
        if(screen) break;
    }
    if(!screen) fatal("Unable to create OpenGL screen");
    else
    {
        if(!hasbpp) conoutf("%d bit color buffer not supported - disabling", colorbits);
        if(depthbits && (config&1)==0) conoutf("%d bit z-buffer not supported - disabling", depthbits);
        if(stencilbits && (config&2)==0) conoutf("%d bit stencil buffer not supported - disabling", stencilbits);
        if(fsaa>0 && (config&4)==0) conoutf("%dx anti-aliasing not supported - disabling", fsaa);
    }

    scr_w = screen->w;
    scr_h = screen->h;
    VIRTW = scr_w*VIRTH/scr_h;

    #ifdef WIN32
    SDL_WM_GrabInput(SDL_GRAB_ON);
    #else
    SDL_WM_GrabInput(fullscreen ? SDL_GRAB_ON : SDL_GRAB_OFF);
    #endif

    usedcolorbits = hasbpp ? colorbits : 0;
    useddepthbits = config&1 ? depthbits : 0;
    usedfsaa = config&2 ? fsaa : 0;
}

extern int hirestextures;

void resetgl()
{
    clearchanges(CHANGE_GFX);

    loadingscreen();

    extern void cleanupparticles();
    extern void cleanupmodels();
    extern void cleanuptextures();
    extern void cleanuptmus();
    extern void cleanupgl();
    cleanupparticles();
    cleanupmodels();
    cleanuptextures();
    cleanuptmus();
    cleanupgl();
    uniformtexres = !hirestextures;
    c2skeepalive();

    SDL_SetVideoMode(0, 0, 0, 0);

    int usedcolorbits = 0, useddepthbits = 0, usedfsaa = 0;
    setupscreen(usedcolorbits, useddepthbits, usedfsaa);
    gl_init(scr_w, scr_h, usedcolorbits, useddepthbits, usedfsaa);

    extern void reloadfonts();
    extern void reloadtextures();
    c2skeepalive();
    if(!reloadtexture(*notexture) ||
       !reloadtexture("packages/misc/startscreen.png"))
        fatal("failed to reload core texture");
    loadingscreen();
    c2skeepalive();
    restoregamma();
    c2skeepalive();
    reloadfonts();
    reloadtextures();
    c2skeepalive();
    drawscope(true); // 2011feb05:ft: preload scope.png
    preload_playermodels();
    c2skeepalive();
    preload_hudguns();
    c2skeepalive();
    preload_entmodels();
    c2skeepalive();
    preload_mapmodels();
    c2skeepalive();
}

COMMAND(resetgl, "");

VARFP(maxfps, 0, 200, 1000, if(maxfps && maxfps < 25) maxfps = 25);

void limitfps(int &millis, int curmillis)
{
    if(!maxfps) return;
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

void fpsrange(int *low, int *high)
{
    if(*low>*high || *low<1) return;
    lowfps = *low;
    highfps = *high;
}

COMMAND(fpsrange, "ii");

void keyrepeat(bool on)
{
    SDL_EnableKeyRepeat(on ? SDL_DEFAULT_REPEAT_DELAY : 0,
                             SDL_DEFAULT_REPEAT_INTERVAL);
}

vector<SDL_Event> events;

void pushevent(const SDL_Event &e)
{
    events.add(e);
}

bool interceptkey(int sym)
{
    static int lastintercept = SDLK_UNKNOWN;
    int len = lastintercept == sym ? events.length() : 0;
    SDL_Event event;
    while(SDL_PollEvent(&event)) switch(event.type)
    {
        case SDL_MOUSEMOTION: break;
        default: pushevent(event); break;
    }
    lastintercept = sym;
    if(sym != SDLK_UNKNOWN) for(int i = len; i < events.length(); i++)
    {
        if(events[i].type == SDL_KEYDOWN && events[i].key.keysym.sym == sym) { events.remove(i); return true; }
    }
    return false;
}

void togglegrab()
{
    if (SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GrabMode(0))
    {
        SDL_WM_GrabInput(SDL_GRAB_ON);
        conoutf("mouse input locked");
    }
    else
    {
        SDL_WM_GrabInput(SDL_GrabMode(0));
        conoutf("mouse input released");
    }
}

COMMAND(togglegrab, "");

static void resetmousemotion()
{
#ifndef WIN32
    if(!(screen->flags&SDL_FULLSCREEN))
    {
        SDL_WarpMouse(screen->w / 2, screen->h / 2);
    }
#endif
}

static inline bool skipmousemotion(SDL_Event &event)
{
    if(event.type != SDL_MOUSEMOTION) return true;
#ifndef WIN32
    if(!(screen->flags&SDL_FULLSCREEN))
    {
        #ifdef __APPLE__
        if(event.motion.y == 0) return true;  // let mac users drag windows via the title bar
        #endif
        if(event.motion.x == screen->w / 2 && event.motion.y == screen->h / 2) return true;  // ignore any motion events generated SDL_WarpMouse
    }
#endif
    return false;
}

static void checkmousemotion(int &dx, int &dy)
{
    loopv(events)
    {
        SDL_Event &event = events[i];
        if(skipmousemotion(event))
        {
            if(i > 0) events.remove(0, i);
            return;
        }
        dx += event.motion.xrel;
        dy += event.motion.yrel;
    }
    events.setsize(0);
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        if(skipmousemotion(event))
        {
            events.add(event);
            return;
        }
        dx += event.motion.xrel;
        dy += event.motion.yrel;
    }
}

static int ignoremouse = 5;

void checkinput()
{
    SDL_Event event;
    int lasttype = 0, lastbut = 0;
    int tdx=0,tdy=0;
    while(events.length() || SDL_PollEvent(&event))
    {
        if(events.length()) event = events.remove(0);

        switch(event.type)
        {
            case SDL_QUIT:
                quit();
                break;

            #if !defined(WIN32) && !defined(__APPLE__)
            case SDL_VIDEORESIZE:
                screenres(event.resize.w, event.resize.h);
                break;
            #endif

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                extern bool senst;
                if (event.key.keysym.sym <= SDLK_5 && event.key.keysym.sym >= SDLK_1 && senst)
                {
                    if (event.key.state==SDL_PRESSED)
                    {
                        extern int tsens(int x);
                        tsens(event.key.keysym.sym);
                    }
                }
                else
                {
                    keypress(event.key.keysym.sym, event.key.state==SDL_PRESSED, event.key.keysym.unicode, event.key.keysym.mod);
                }
                break;

            case SDL_ACTIVEEVENT:
                if(event.active.state & SDL_APPINPUTFOCUS)
                    inputgrab(grabinput = event.active.gain!=0);
                if(event.active.state & SDL_APPACTIVE)
                    minimized = !event.active.gain;
#if 0
                if(event.active.state==SDL_APPMOUSEFOCUS) setprocesspriority(event.active.gain > 0); // switch priority on focus change
#endif
                break;

            case SDL_MOUSEMOTION:
                if(ignoremouse) { ignoremouse--; break; }
                if(grabinput && !skipmousemotion(event))
                {
                    int dx = event.motion.xrel, dy = event.motion.yrel;
                    checkmousemotion(dx, dy);
                    resetmousemotion();
                    tdx+=dx;tdy+=dy;
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                if(lasttype==event.type && lastbut==event.button.button) break;
                keypress(-event.button.button, event.button.state!=0, 0);
                lasttype = event.type;
                lastbut = event.button.button;
                break;
        }
    }
    mousemove(tdx, tdy);
}

VARF(gamespeed, 10, 100, 1000, if(multiplayer()) gamespeed = 100);
VARF(paused, 0, 0, 1, if(multiplayer()) paused = 0);

bool firstrun = false, inmainloop = false;
static int clockrealbase = 0, clockvirtbase = 0;
static void clockreset() { clockrealbase = SDL_GetTicks(); clockvirtbase = totalmillis; }
VARFP(clockerror, 990000, 1000000, 1010000, clockreset());
VARFP(clockfix, 0, 0, 1, clockreset());
VARP(gamestarts, 0, 0, INT_MAX);

const char *rndmapname()
{
    vector<char *> maps;
    listfiles("packages/maps/official", "cgz", maps);
    char *map = newstring(maps[rnd(maps.length())]);
    maps.deletearrays();
    return map;
}

extern void connectserv(char *, int *, char *);

void connectprotocol(char *protocolstring, string &servername, int &serverport, string &password, bool &direct_connect)
{
    const char *c = &protocolstring[14], *p = c;
    int len = 0;
    direct_connect = false;
    string sp;
    servername[0] = password[0] = '\0';
    serverport = 0;
    while(*c && *c!='/' && *c!='?' && *c!=':') { len++; c++; }
    if(!len) { conoutf("\f3bad commandline syntax", protocolstring); return; }
    copystring(servername, p, min(len+1, MAXSTRLEN));
    direct_connect = true;
    if(*c && *c==':')
    {
        c++; p = c; len = 0;
        while(*c && *c!='/' && *c!='?') { len++; c++; }
        if(len)
        {
            copystring(sp, p, min(len+1, MAXSTRLEN));
            serverport = atoi(sp);
        }
    }
    if(*c && *c=='/') c++;
    if(!*c || *c!='?') return;
    do
    {
        if(*c) c++;
        if(!strncmp(c, "port=", 5))
        {
            c += 5; p = c; len = 0;
            while(*c && *c!='&' && *c!='/') { len++; c++; }
            if(len)
            {
                copystring(sp, p, min(len+1, MAXSTRLEN));
                serverport = atoi(sp);
            }
        }
        else if(!strncmp(c, "password=", 9))
        {
            c += 9; p = c; len = 0;
            while(*c && *c!='&' && *c!='/') { len++, c++; }
            if(len) copystring(password, p, min(len+1, MAXSTRLEN));
        }
        else break;
    } while(*c && *c=='&' && *c!='/');
}

#ifdef WIN32
static char *parsecommandline(const char *src, vector<char *> &args)
{
    char *buf = new char[strlen(src) + 1], *dst = buf;
    for(;;)
    {
        while(isspace(*src)) src++;
        if(!*src) break;
        args.add(dst);
        for(bool quoted = false; *src && (quoted || !isspace(*src)); src++)
        {
            if(*src != '"') *dst++ = *src;
            else if(dst > buf && src[-1] == '\\') dst[-1] = '"';
            else quoted = !quoted;
        }
        *dst++ = '\0';
    }
    args.add(NULL);
    return buf;
}


int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
{
    vector<char *> args;
    char *buf = parsecommandline(GetCommandLine(), args);
    SDL_SetModuleHandle(hInst);
    int status = SDL_main(args.length()-1, args.getbuf());
    delete[] buf;
    exit(status);
    return 0;
}
#endif

VARP(compatibilitymode, 0, 1, 1); // FIXME : find a better place to put this ?

int main(int argc, char **argv)
{
    extern struct servercommandline scl;
    #ifdef WIN32
    //atexit((void (__cdecl *)(void))_CrtDumpMemoryLeaks);
    #ifndef _DEBUG
    #ifndef __GNUC__
    __try {
    #endif
    #endif
    #endif

    bool dedicated = false;
    bool quitdirectly = false;
    char *initscript = NULL;
    char *initdemo = NULL;
    bool direct_connect = false;               // to connect via assaultcube:// browser switch
    string servername, password;
    int serverport;

    const char *initmap = rndmapname();

    pushscontext(IEXC_CFG);

    #define initlog(s) clientlogf("init: " s)

    initing = INIT_RESET;
    for(int i = 1; i<argc; i++)
    {
        // server: ufimNFTLAckyxpDWrXBKIoOnPMVC
        if(!scl.checkarg(argv[i]))
        {
            char *a = &argv[i][2];
            if(argv[i][0]=='-') switch(argv[i][1])
            {
                case '-':
                    if(!strncmp(argv[i], "--home=", 7))
                    {
                        sethomedir(&argv[i][7]);
                    }
                    else if(!strncmp(argv[i], "--mod=", 6))
                    {
                        addpackagedir(&argv[i][6]);
                    }
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
                    else if(!strcmp(argv[i], "--version"))
                    {
                        printf("%.3f\n", AC_VERSION/1000.0f);
                        quitdirectly = true;
                    }
                    else if(!strcmp(argv[i], "--protocol"))
                    {
                        printf("%d\n", PROTOCOL_VERSION);
                        quitdirectly = true;
                    }
                    else if(!strncmp(argv[i], "--loadmap=", 10))
                    {
                        initmap = &argv[i][10];
                    }
                    else if(!strncmp(argv[i], "--loaddemo=", 11))
                    {
                        initdemo = &argv[i][11];
                    }
                    else conoutf("\f3unknown commandline switch: %s", argv[i]);
                    break;
                case 'd': dedicated = true; break;
                case 't': fullscreen = atoi(a); break;
                case 'w': scr_w  = atoi(a); break;
                case 'h': scr_h  = atoi(a); break;
                case 'z': depthbits = atoi(a); break;
                case 'b': colorbits = atoi(a); break;
                case 's': stencilbits = atoi(&argv[i][2]); break;
                case 'a': fsaa = atoi(a); break;
                case 'v': vsync = atoi(a); break;
                case 'e': initscript = &argv[i][2]; break;
                default:  conoutf("\f3unknown commandline option: -%c", argv[i][1]);
            }
            else if(!strncmp(argv[i], "assaultcube://", 14)) // browser direct connection
            {
                connectprotocol(argv[i], servername, serverport, password, direct_connect);
            }
            else conoutf("\f3unknown commandline argument: %c", argv[i][0]);
        }
    }
    if(quitdirectly) return EXIT_SUCCESS;

    i18nmanager i18n("AC", path("packages/locale", true));

    initing = NOT_INITING;

    initlog("sdl");
    int par = 0;
#ifdef _DEBUG
    par = SDL_INIT_NOPARACHUTE;
#endif
    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|par)<0) fatal("Unable to initialize SDL");

#if 0
    if(highprocesspriority) setprocesspriority(true);
#endif

    if (!dedicated) initlog("net");
    if(enet_initialize()<0) fatal("Unable to initialise network module");

    if (!dedicated) initclient();
        //FIXME the server executed in this way does not catch the SIGTERM or ^C
    initserver(dedicated,argc,argv);  // never returns if dedicated

    initlog("world");
    empty_world(7, true);

    initlog("video: sdl");
    if(SDL_InitSubSystem(SDL_INIT_VIDEO)<0) fatal("Unable to initialize SDL Video");

    initlog("video: mode");
    int usedcolorbits = 0, useddepthbits = 0, usedfsaa = 0;
    setupscreen(usedcolorbits, useddepthbits, usedfsaa);

    // no more TTF ATM.
    //initlog("font");
    //initfont();

    initlog("video: misc");
    SDL_WM_SetCaption("AssaultCube", NULL);
    keyrepeat(false);
    SDL_ShowCursor(0);

    initlog("gl");
    gl_checkextensions();
    gl_init(scr_w, scr_h, usedcolorbits, useddepthbits, usedfsaa);

    notexture = noworldtexture = textureload("packages/misc/notexture.jpg");
    if(!notexture) fatal("could not find core textures (hint: run AssaultCube from the parent of the bin directory)");

    nomodel = loadmodel("misc/gib01", -1);      //FIXME: need actual placeholder model
    if(!notexture) fatal("could not find core models");

    initlog("console");
    per_idents = false;
    // Main font file, all other font files execute from here.
    if(!execfile("config/font.cfg")) fatal("cannot find default font definitions");
    // Check these 2 standard fonts have been executed.
    if(!setfont("mono")) fatal("no mono font specified");
    if(!setfont("default")) fatal("no default font specified");

    loadingscreen();

    particleinit();

    initlog("sound");
    audiomgr.initsound();

    initlog("cfg");
    extern void *scoremenu, *servmenu, *searchmenu, *serverinfomenu, *kickmenu, *banmenu, *forceteammenu, *giveadminmenu, *docmenu, *applymenu;
    scoremenu = addmenu("score", "columns", false, renderscores, NULL, false, true);
    servmenu = addmenu("server", NULL, true, refreshservers, serverskey);
    searchmenu = addmenu("search", NULL, true, refreshservers, serverskey);
    serverinfomenu = addmenu("serverinfo", NULL, true, refreshservers, serverinfokey);
    kickmenu = addmenu("kick player", NULL, true, refreshsopmenu);
    banmenu = addmenu("ban player", NULL, true, refreshsopmenu);
    forceteammenu = addmenu("force team", NULL, true, refreshsopmenu);
    giveadminmenu = addmenu("give admin", NULL, true, refreshsopmenu);
    docmenu = addmenu("reference", NULL, true, renderdocmenu);
    applymenu = addmenu("apply", "apply changes now?", true, refreshapplymenu);

    exec("config/scontext.cfg");
    exec("config/locale.cfg");
    exec("config/keymap.cfg");
    exec("config/menus.cfg");
    exec("config/scripts.cfg");
    exec("config/prefabs.cfg");
    exec("config/sounds.cfg");
    exec("config/securemaps.cfg");
    exec("config/admin.cfg");
    execfile("config/servers.cfg");
    per_idents = true;

    static char resdata[] = { 112, 97, 99, 107, 97, 103, 101, 115, 47, 116, 101, 120, 116, 117, 114, 101, 115, 47, 107, 117, 114, 116, 47, 107, 108, 105, 116, 101, 50, 46, 106, 112, 103, 0 };
    stream *f = opengzfile(resdata, "rb");
    if(f)
    {
        int n = f->getlil<int>();
        loopi(n)
        {
            string s;
            f->read(s, sizeof(string));
            enet_uint32 c = f->getlil<enet_uint32>();
            setresdata(s, c);
        }
        delete f;
    }

    initing = INIT_LOAD;
    if(!execfile("config/saved.cfg"))
    {
        exec("config/defaults.cfg");
        firstrun = true;
    }
    if(identexists("afterinit")) execute("afterinit");
    if(compatibilitymode)
    {
        per_idents = false;
        exec("config/compatibility.cfg"); // exec after saved.cfg to get "compatibilitymode", but before user scripts..
        per_idents = true;
    }
    execfile("config/autoexec.cfg");
    execfile("config/auth.cfg");
    execute("addallfavcatmenus");  // exec here, to add all categories (including those defined in autoexec.cfg)
    initing = NOT_INITING;
    uniformtexres = !hirestextures;

    initlog("models");
    preload_playermodels();
    preload_hudguns();
    initlog("curl");
    setupcurl();

    preload_entmodels();

    initlog("docs");
    per_idents = false;
    execfile("config/docs.cfg");
    per_idents = true;

    initlog("localconnect");
    extern string clientmap;

    if(initdemo)
    {
        extern int gamemode;
        gamemode = -1;
        copystring(clientmap, initdemo);
    }
    else
    copystring(clientmap, initmap); // ac_complex for 1.0, ac_shine for 1.1, ..

    localconnect();

    if(initscript) execute(initscript);

    gamestarts = max(1, gamestarts+1);
    if(autodownload && (gamestarts % 100 == 1)) sortpckservers();

    initlog("mainloop");

    inputgrab(grabinput = true);

    inmainloop = true;
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
        if(multiplayer(false)) curtime = elapsed;
        else
        {
            static int timeerr = 0;
            int scaledtime = elapsed*gamespeed + timeerr;
            curtime = scaledtime/100;
            timeerr = scaledtime%100;
            if(nextmillis && watchingdemo)
            {
                curtime += nextmillis;
                nextmillis = 0;
            }
            if(paused) curtime = 0;
        }
        lastmillis += curtime;
        totalmillis = millis;

        checkinput();

        if(lastmillis) updateworld(curtime, lastmillis);

        if(needsautoscreenshot) showscores(true);

        serverslice(0);

        if(elapsed) fps = (1000.0f/elapsed+fps*10)/11; // avoid DIV-by-0
        frames++;

        audiomgr.updateaudio();

        computeraytable(camera1->o.x, camera1->o.y, dynfov());
        if(frames>3 && !minimized)
        {
            gl_drawframe(screen->w, screen->h, fps<lowfps ? fps/lowfps : (fps>highfps ? fps/highfps : 1.0f), fps);
            if(frames>4) SDL_GL_SwapBuffers();
        }

        if(needsautoscreenshot)
        {
            showscores(true);
            // draw again after swapping buffers, to make sure menu is captured
            // in the screenshot regardless of which frame buffer is current
            if (!minimized)
            {
                gl_drawframe(screen->w, screen->h, fps<lowfps ? fps/lowfps : (fps>highfps ? fps/highfps : 1.0f), fps);           
            }
            addsleep(0, "screenshot");
            needsautoscreenshot = false;
        }
#ifdef _DEBUG
        if(millis>lastflush+60000) { fflush(stdout); lastflush = millis; }
#endif
        if (direct_connect)
        {
            direct_connect = false;
            connectserv(servername, &serverport, password);
        }
    }

    quit();
    return EXIT_SUCCESS;

#if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
    } __except(stackdumper(0, GetExceptionInformation()), EXCEPTION_CONTINUE_SEARCH) { return 0; }
#endif
}

VAR(version, 1, AC_VERSION, 0);
VAR(protocol, 1, PROTOCOL_VERSION, 0);
