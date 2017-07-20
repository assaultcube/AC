// main.cpp: initialisation & main loop

#include "cube.h"

void cleanup(char *msg)         // single program exit point;
{
    if(clientlogfile) clientlogfile->fflush();
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
        if(clientlogfile)
        {
            clientlogfile->printf("%s\n", msg);
            dumpexecutionstack(clientlogfile);
        }
    }
    SDL_Quit();
}

VAR(resetcfg, 0, 0, 1);

void quit()                     // normal exit
{
    if(clientlogfile) clientlogfile->fflush();
    const char *onquit = getalias("onQuit");
    setcontext("hook", "onQuit");
    if(onquit && onquit[0]) execute(onquit);
    resetcontext();
    alias("onQuit", "");
    alias("mapstartonce", "");
    extern void writeinitcfg();
    writeinitcfg();
    writeservercfg();
    writepcksourcecfg();
    if(resetcfg) deletecfg();
    else writecfg();
    savehistory();
    writemapmodelattributes();
    entropy_save();
    writeallxmaps();
    cleanup(NULL);
    popscontext();
    DELETEP(clientlogfile);
    exit(EXIT_SUCCESS);
}
COMMAND(quit, "");

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
    DELETEP(clientlogfile);
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

SVARFP(lang, "en", filterlang(lang, lang));

void writeinitcfg()
{
    if(!restoredinits) return;
    filerotate("config/init", "cfg", CONFIGROTATEMAX); // keep five old init config sets
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
    extern int audio, soundchannels, igraphdefaultframetime;
    f->printf("audio %d\n", audio > 0 ? 1 : 0);
    f->printf("soundchannels %d\n", soundchannels);
    f->printf("igraphdefaultframetime %d\n", igraphdefaultframetime);
    if(lang && *lang) f->printf("lang %s\n", lang);
    extern void writezipmodconfig(stream *f);
    writezipmodconfig(f);
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

VARP(pngcompress, 0, 9, 9);

void writepngchunk(stream *f, const char *type, const void *data = NULL, uint len = 0)
{
    f->putbig<uint>(len);
    f->write(type, 4);
    if(data) f->write(data, len);

    uint crc = crc32(0, Z_NULL, 0);
    crc = ~crc32(crc, (const Bytef *)type, 4);
    if(data) loopi(len) enet_crc32_inc(&crc, ((const uchar *)data)[i]);
    f->putbig<uint>(~crc);
}

int save_png(const char *filename, SDL_Surface *image)
{
    uchar *data = (uchar *)image->pixels;
    int iw = image->w, ih = image->h, pitch = image->pitch;

    stream *f = openfile(filename, "wb");
    if(!f) { conoutf("could not write to %s", filename); return -1; }
    const char *scores = asciiscores(true);

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

    writepngchunk(f, "tEXt", "\123o\165rc\145\0\101ss\141" "\165\154\164\103\165\142\145", 18);
    writepngchunk(f, "tEXt", scores - 8, strlen(scores) + 8);
    writepngchunk(f, "IEND");

    delete f;
    return 0;

cleanuperror:
    deflateEnd(&z);

error:
    delete f;

    return -1;
}

#include "jpegenc.h"

void mapscreenshot(const char *imagepath, bool mapshot, int fileformat, float scale, int height, int quality)
{
    extern int minimaplastsize;
    int src_w = mapshot ? minimaplastsize : screen->w;
    int src_h = mapshot ? minimaplastsize : screen->h;
    int dst_w = mapshot ? src_w : src_w * scale, img_w = dst_w;
    int dst_h = mapshot ? src_h : src_h * scale, img_h = dst_h;
    if(height)
    { // create 4:3 of exact height
        if(dst_w >= (dst_h * 4) / 3)
        { // 4:3 or wider: crop width
            dst_w = (src_w * height) / src_h;
            img_h = dst_h = height;
            img_w = (img_h * 4) / 3;
        }
        else
        { // probably 5:4: crop height
            img_h = height;
            dst_w = img_w = (img_h * 4) / 3;
            dst_h = (src_h * dst_w) / src_w;
        }
    }

    SDL_Surface *image = creatergbsurface(img_w, img_h);
    if(!image) return;

    int tmpdstsize = dst_w * dst_h * 3, tmpdstpitch = dst_w * 3;
    uchar *tmpdst = new uchar[tmpdstsize], *dst = (uchar *)image->pixels;

    if(mapshot)
    {
        extern GLuint minimaptex;
        if(minimaptex)
        {
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glBindTexture(GL_TEXTURE_2D, minimaptex);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, tmpdst);
        }
        else
        {
            conoutf("no mapshot prepared!");
            SDL_FreeSurface(image);
            delete[] tmpdst;
            return;
        }
        loopi(img_h)
        { // copy image
            memcpy(dst, &tmpdst[tmpdstpitch * i], image->pitch);
            dst += image->pitch;
        }
    }
    else
    {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        if(src_w != dst_w || src_h != dst_h)
        { // scale image
            uchar *tmpsrc = new uchar[src_w * src_h * 3];
            glReadPixels(0, 0, src_w, src_h, GL_RGB, GL_UNSIGNED_BYTE, tmpsrc);
            scaletexture(tmpsrc, src_w, src_h, 3, tmpdst, dst_w, dst_h);
            delete[] tmpsrc;
        }
        else glReadPixels(0, 0, dst_w, dst_h, GL_RGB, GL_UNSIGNED_BYTE, tmpdst);
        int crop_w = height ? ((dst_w - img_w) / 2) * 3 : 0;
        int crop_h = height ? (dst_h - img_h) / 2 : 0;
        loopirev(img_h)
        { // flip image
            memcpy(dst, &tmpdst[tmpdstpitch * (i + crop_h) + crop_w], image->pitch);
            dst += image->pitch;
        }
    }
    entropy_add_block(tmpdst, tmpdstsize);
    delete[] tmpdst;
    if(!mapshot && imagepath) audiomgr.playsound(S_CAMERA);

    switch(fileformat)
    {
        case 0: // bmp
        {
            stream *file = openfile(imagepath, "wb");
            if(!file) conoutf("failed to create: %s", imagepath);
            else
            {
                SDL_SaveBMP_RW(image, file->rwops(), 1);
                delete file;
            }
            break;
        }
        case 1: // jpeg
        {
            jpegenc jpegencoder;
            jpegencoder.encode(imagepath, image, quality, asciiscores(true));
            break;
        }
        case 2: // png
            if(save_png(imagepath, image) < 0) conoutf("\f3Error saving png file");
            break;
    }

    SDL_FreeSurface(image);
}

FVARP(screenshotscale, 0.1f, 1.0f, 1.0f);
VARP(jpegquality, 10, 85, 100);  // best: 100 - good: 85 [default] - bad: 70 - terrible: 50
VARP(screenshottype, 0, 1, 2);

const char *getscrext()
{
    const char *screxts[] = { ".bmp", ".jpg", ".png" };
    return screxts[screenshottype % 3];
}
COMMANDF(getscrext, "", () { result(getscrext()); });

void screenshot(const char *filename)
{
    static string buf;
    if(filename && filename[0]) formatstring(buf)("screenshots/%s%s", filename, getscrext());
    else if(getclientmap()[0]) formatstring(buf)("screenshots/%s_%s_%s%s", timestring(), behindpath(getclientmap()), modestr(gamemode, true), getscrext());
    else formatstring(buf)("screenshots/%s%s", timestring(), getscrext());
    path(buf);
    mapscreenshot(buf, false, screenshottype, screenshotscale, 0, jpegquality);
}
COMMAND(screenshot, "s");

void mapshot()
{
    defformatstring(buf)("screenshots" PATHDIVS "mapshot_%s_%s%s", behindpath(getclientmap()), timestring(), getscrext());
    mapscreenshot(buf, true, screenshottype, screenshotscale, 0, jpegquality);
}
COMMAND(mapshot, "");

void screenshotpreview(int *res)
{
    static int lastres = 240;
    defformatstring(buf)("packages" PATHDIVS "maps" PATHDIVS "%spreview" PATHDIVS "%s.jpg", securemapcheck(getclientmap(), false) ? "official" PATHDIVS : "", behindpath(getclientmap()));
    mapscreenshot(buf, false, 1, 1.0f, (lastres = clamp(((*res ? *res : lastres) / 48) * 48, 144, 480)), 80);
    reloadtexture(*textureload(buf, 3));
}
COMMAND(screenshotpreview, "i");

bool needsautoscreenshot = false;

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

VAR(desktopw, 1, 0, 0);
VAR(desktoph, 1, 0, 0);

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

void getdisplayresolutions()
{
    string res = "";
    int lh = 0, lw = 0;
    SDL_Rect **modes = SDL_ListModes(NULL, SDL_OPENGL|SDL_RESIZABLE|SDL_FULLSCREEN);
    if(modes && modes!=(SDL_Rect **)-1)
    {
        for(int i = 0; modes[i]; i++)
        {
            if(lw != modes[i]->w && lh != modes[i]->h) concatformatstring(res, "%s%d %d", *res ? " " : "", (lw = modes[i]->w), (lh = modes[i]->h));
        }
    }
    result(res);
}
COMMAND(getdisplayresolutions, "");

void setupscreen(int &usedcolorbits, int &useddepthbits, int &usedfsaa)
{
    int flags = SDL_RESIZABLE;
    #if defined(WIN32) || defined(__APPLE__)
    flags = 0;
    putenv(newstring("SDL_VIDEO_CENTERED=1")); //Center window
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
    extern Texture *startscreen;
    c2skeepalive();
    if(!reloadtexture(*notexture) ||
       !reloadtexture(*startscreen))
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

int ignoremouse = 5, bootstrapentropy = 2;

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
                if(bootstrapentropy > 0 && (--bootstrapentropy & 2)) mapscreenshot(NULL, false, -1, 1000.0f / (1000 + rnd(400)), 0, 0);
            case SDL_KEYUP:
                entropy_add_byte(event.key.keysym.sym ^ totalmillis);
                keypress(event.key.keysym.sym, event.key.state==SDL_PRESSED, event.key.keysym.unicode, event.key.keysym.mod);
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
                    entropy_add_byte(dx ^ (256 - dy));
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
    if(tdx || tdy)
    {
        entropy_add_byte(tdy + 5 * tdx);
        mousemove(tdx, tdy);
    }
}

VARF(gamespeed, 10, 100, 1000, if(multiplayer()) gamespeed = 100);
VARF(paused, 0, 0, 1, if(multiplayer()) paused = 0);

bool inmainloop = false;
static int clockrealbase = 0, clockvirtbase = 0;
static void clockreset() { clockrealbase = SDL_GetTicks(); clockvirtbase = totalmillis; }
VARFP(clockerror, 990000, 1000000, 1010000, clockreset());
VARFP(clockfix, 0, 0, 1, clockreset());

const char *rndmapname()
{
    vector<char *> maps;
    listfiles("packages/maps/official", "cgz", maps);
    if (maps.length() > 0)
    {
        char *map = newstring(maps[rnd(maps.length())]);
        maps.deletearrays();
        return map;
    }
    else return "";
}

#define AUTOSTARTPATH "config" PATHDIVS "autostart" PATHDIVS

void autostartscripts(const char *prefix)
{
    static vector<char *> *files = NULL;
    if(!files)
    {  // first run: fetch file names and sort them
        files = new vector<char *>;
        listfiles(AUTOSTARTPATH, "cfg", *files, stringsort);
    }
    loopv(*files)
    {
        if(*prefix && strncmp((*files)[i], prefix, strlen(prefix))) continue;
        defformatstring(p)(AUTOSTARTPATH "%s.cfg", (*files)[i]);
        execfile(p);
        delstring(files->remove(i--));
    }
}

void createconfigtemplates(const char *templatezip)  // create customisable config files in homedir - only if missing
{
    vector<const char *> files;
    void *mz = zipmanualopen(openfile(templatezip, "rb"), files);
    if(mz)
    {
        loopv(files)
        {
            const char *filename = behindpath(files[i]); // only look for config files in the zip file, ignore all paths in the zip
            if(strlen(filename) > 4 && !strcmp(filename + strlen(filename) - 4, ".cfg"))
            {
                defformatstring(fname)("config" PATHDIVS "%s", files[i]);
                if(getfilesize(fname) <= 0) // config does not exist or is empty
                {
                    stream *zf = openfile(fname, "wb");
                    if(zf)
                    {
                        zipmanualread(mz, i, zf, MAXCFGFILESIZE); // fetch file content from zip, write to new config file
                        delete zf;
                        conoutf("created %s from template %s", fname, templatezip);
                    }
                }
            }
        }
        zipmanualclose(mz);
    }
    findfile(AUTOSTARTPATH "dummy", "w"); // create empty autostart directory, if it doesn't exist yet
}

void connectprotocol(char *protocolstring) // assaultcube://example.org[:28763][/][?[port=28763][&password=secret]]
{
    urlparse u;
    u.set(protocolstring);
    if(strcmp(u.scheme, "assaultcube") || !*u.domain) { conoutf("\f3bad commandline syntax (\"%s\")", protocolstring); return; }
    if(*u.userpassword) clientlogf(" ignoring user:password part of url (%s)", u.userpassword);
    if(*u.fragment) clientlogf(" ignoring fragment part of url (%s)", u.fragment);
    if(*u.path) clientlogf(" ignoring path part of url (%s)", u.path);
    int port = ATOI(u.port);
    const char *q = u.query, *passwd = NULL;
    while(q && *q)
    {
        if(*q == '&') q++;
        if(!strncmp(q, "port=", 5))
        {
            q += 5;
            port = ATOI(q);
            q = strchr(q, '&');
        }
        else if(!strncmp(q, "password=", 9))
        {
            q += 9;
            const char *e = strchr(q, '&');
            passwd = e ? newstring(q, e - q) : newstring(q);
            q = e;
        }
        else break;
    }
    defformatstring(cmd)("connect %s %d %s", u.domain, port, passwd ? passwd : "");
    DEBUGCODE(clientlogf("connectprotocol: %s", cmd));
    addsleep(5, cmd, true);
    DELSTRING(passwd);
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

void initclientlog()  // rotate old logfiles and create new one
{
    filerotate("clientlog", "txt", 5); // keep five old logfiles
    clientlogfile = openfile("clientlog.txt", "w");
    if(clientlogfile)
    {
        if(bootclientlog && bootclientlog->length()) clientlogfile->write(bootclientlog->getbuf(), bootclientlog->length());
    }
    else conoutf("could not create logfile \"%s\"", findfile("clientlog.txt", "w"));
    DELETEP(bootclientlog);
}

#ifdef _DEBUG
void sanitychecks()
{
    ASSERT((GMMASK__BOT ^ GMMASK__MP ^ GMMASK__TEAM ^ GMMASK__FFA ^ GMMASK__TEAMSPAWN ^ GMMASK__FFASPAWN) == GMMASK__ALL);
    float fa = 1 << LARGEST_FACTOR, fb = fa;
    while(fa != fa + fb) fb /= 2;
    ASSERT(2 * fb < NEARZERO);
}
#endif

#define DEFAULTPROFILEPATH "profile"

int main(int argc, char **argv)
{
    DEBUGCODE(sanitychecks());
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

    if(bootclientlog) cvecprintf(*bootclientlog, "######## start logging: %s\n", timestring(true));

    const char *initmap = rndmapname();
    loopi(NUMGUNS) crosshairnames[i] = gunnames[i] = guns[i].modelname;
    crosshairnames[GUN_AKIMBO] = gunnames[GUN_AKIMBO] = "akimbo";
    crosshairnames[CROSSHAIR_DEFAULT] = "default";
    crosshairnames[CROSSHAIR_TEAMMATE] = "teammate";
    crosshairnames[CROSSHAIR_SCOPE] = "scope";
    crosshairnames[CROSSHAIR_EDIT] = "edit";
    crosshairnames[CROSSHAIR_NUM] = gunnames[NUMGUNS] = "";

    pushscontext(IEXC_CFG);
    persistidents = false;

    #define initlog(s) clientlogf("init: " s)

    if(*stream_capabilities()) clientlogf("info: %s", stream_capabilities());

    initing = INIT_RESET;
    for(int i = 1; i<argc; i++)
    {
        clientlogf("parsing commandline argument %d: \"%s\"", i, argv[i]);
        // server: ufimNFTLAckyxpDWrXBKIoOnPMVC
        if(!scl.checkarg(argv[i]))
        {
            char *a = &argv[i][2];
            if(argv[i][0]=='-') switch(argv[i][1])
            {
                case '-':
                    if(!strcmp(argv[i], "--home"))
                    {
                        sethomedir(DEFAULTPROFILEPATH);
                    }
                    else if(!strncmp(argv[i], "--home=", 7))
                    {
                        sethomedir(&argv[i][7]);
                    }
                    else if(!strncmp(argv[i], "--mod=", 6))
                    {
                        addpackagedir(&argv[i][6]);
                    }
                    else if(!strcmp(argv[i], "--init"))
                    {
                        if(!havehomedir()) sethomedir(DEFAULTPROFILEPATH);
                        execfile((char *)"config/init.cfg");
                        restoredinits = true;
                    }
                    else if(!strncmp(argv[i], "--init=", 7))
                    {
                        if(!havehomedir()) sethomedir(DEFAULTPROFILEPATH);
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
                connectprotocol(argv[i]);
            }
            else conoutf("\f3unknown commandline argument: %c", argv[i][0]);
        }
    }
    if(!havehomedir()) sethomedir(DEFAULTPROFILEPATH);
    entropy_init(time(NULL) + (uint)(size_t)&initscript + (uint)(size_t)entropy_init);
    initclientlog();
    if(quitdirectly) return EXIT_SUCCESS;

    createconfigtemplates("config" PATHDIVS "configtemplates.zip");

    initing = NOT_INITING;

    #define STRINGIFY_(x) #x
    #define STRINGIFY(x) STRINGIFY_(x)
    #define SDLVERSIONSTRING  STRINGIFY(SDL_MAJOR_VERSION) "." STRINGIFY(SDL_MINOR_VERSION) "." STRINGIFY(SDL_PATCHLEVEL)
    initlog("sdl (" SDLVERSIONSTRING ")");
    int par = 0;
#ifdef _DEBUG
    par = SDL_INIT_NOPARACHUTE;
#endif
    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|par)<0) fatal("Unable to initialize SDL");
    const SDL_version *sdlver = SDL_Linked_Version();
    if(SDL_COMPILEDVERSION != SDL_VERSIONNUM(sdlver->major, sdlver->minor, sdlver->patch))
        clientlogf("SDL: compiled version " SDLVERSIONSTRING ", linked version %u.%u.%u", sdlver->major, sdlver->minor, sdlver->patch);

#if 0
    if(highprocesspriority) setprocesspriority(true);
#endif

    if (!dedicated) initlog("net (" STRINGIFY(ENET_VERSION_MAJOR) "." STRINGIFY(ENET_VERSION_MINOR) "." STRINGIFY(ENET_VERSION_PATCH) ")");
    if(enet_initialize()<0) fatal("Unable to initialise network module");

    if (!dedicated) initclient();
        //FIXME the server executed in this way does not catch the SIGTERM or ^C
    initserver(dedicated,argc,argv);  // never returns if dedicated

    initlog("world (" STRINGIFY(AC_VERSION) ")");
    empty_world(7, true);

    initlog("video: sdl");
    if(SDL_InitSubSystem(SDL_INIT_VIDEO)<0) fatal("Unable to initialize SDL Video");

#ifndef __APPLE__
    SDL_Surface *icon = IMG_Load("packages/misc/icon.png");
    SDL_WM_SetIcon(icon, NULL);
#endif

    initlog("video: mode");
    const SDL_VideoInfo *video = SDL_GetVideoInfo();
    if(video)
    {
        desktopw = video->current_w;
        desktoph = video->current_h;
    }
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

    initlog("console");
    updateigraphs();
    autostartscripts("_veryfirst_");
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
    extern void *scoremenu, *servmenu, *searchmenu, *serverinfomenu, *kickmenu, *banmenu, *forceteammenu, *giveadminmenu, *docmenu, *applymenu, *downloaddemomenu;
    scoremenu = addmenu("score", "columns", false, renderscores, NULL, false, true);
    servmenu = addmenu("server", NULL, true, refreshservers, serverskey);
    searchmenu = addmenu("search", NULL, true, refreshservers, serverskey);
    serverinfomenu = addmenu("serverinfo", "extended server information (F5: refresh)", true, refreshservers, serverinfokey);
    kickmenu = addmenu("kick player", NULL, true, refreshsopmenu);
    banmenu = addmenu("ban player", NULL, true, refreshsopmenu);
    forceteammenu = addmenu("force team", NULL, true, refreshsopmenu);
    giveadminmenu = addmenu("give admin", NULL, true, refreshsopmenu);
    docmenu = addmenu("reference", NULL, true, renderdocmenu);
    applymenu = addmenu("apply", "apply changes now?", true, refreshapplymenu);
    downloaddemomenu = addmenu("Download demo", NULL, true);

    exec("config/scontext.cfg");
    exec("config/keymap.cfg");
    execfile("config/mapmodelattributes.cfg");
    exec("config/menus.cfg");
    exec("config/scripts.cfg");
    exec("config/prefabs.cfg");
    registerdefaultsounds();
    exec("config/sounds.cfg");
    exec("config/securemaps.cfg");
    exec("config/admin.cfg");
    execfile("config/servers.cfg");
    execfile("config/pcksources.cfg");
    execfile("config/authkeys.cfg");
    execfile("config/authprivate.cfg");
    loadcertdir();
    loadhistory();
    setupautodownload();
    int xmn = loadallxmaps();
    if(xmn) conoutf("loaded %d xmaps", xmn);
    persistidents = true;

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

    autostartscripts("_beforesaved_");
    initing = INIT_LOAD;
    if(!execfile("config/saved.cfg"))
    {
        exec("config/defaults.cfg");
        bootstrapentropy += 5 + rnd(7);
    }
    autostartscripts("_aftersaved_");
    exechook(HOOK_SP_MP, "afterinit", "");
    autostartscripts("");    // all remaining scripts
    execfile("config/autoexec.cfg");
    exechook(HOOK_SP_MP, "autoexec", "");
    initing = NOT_INITING;
    uniformtexres = !hirestextures;

    initlog("models");
    preload_playermodels();
    preload_hudguns();
    preload_entmodels();

    initlog("docs");
    persistidents = false;
    execfile("config/docs.cfg");
    persistidents = true;

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

    initlog("mainloop");

    inputgrab(grabinput = true);

    inmainloop = true;
#ifdef _DEBUG
    int lastflush = 0;
#endif
    if(scl.logident[0])
    { // "-Nxxx" sets the number of client log lines to xxx (after init)
        extern int clientloglinesremaining;
        clientloglinesremaining = atoi(scl.logident);  // dual-use for scl.logident
    }
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
        entropy_add_byte(elapsed);
        if(multiplayer(NULL)) curtime = elapsed;
        else
        {
            static int timeerr = 0;
            int scaledtime = elapsed*gamespeed + timeerr;
            curtime = scaledtime/100;
            timeerr = scaledtime%100;
            if(!watchingdemo) skipmillis = 0;
            if(skipmillis && watchingdemo)
            {
                int skipmillisnow = skipmillis > 5000 ? skipmillis - 5000 : min(1000, skipmillis);
                curtime += skipmillisnow;
                skipmillis -= skipmillisnow;
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
            gl_drawframe(screen->w, screen->h, fps<lowfps ? fps/lowfps : (fps>highfps ? fps/highfps : 1.0f), fps, elapsed);
            if(frames>4) SDL_GL_SwapBuffers();
        }

        if(needsautoscreenshot)
        {
            showscores(true);
            // draw again after swapping buffers, to make sure menu is captured
            // in the screenshot regardless of which frame buffer is current
            if(!minimized)
            {
                gl_drawframe(screen->w, screen->h, fps<lowfps ? fps/lowfps : (fps>highfps ? fps/highfps : 1.0f), fps, elapsed);
            }
            addsleep(0, "screenshot");
            needsautoscreenshot = false;
        }
#ifdef _DEBUG
        if(millis>lastflush+60000) { fflush(stdout); lastflush = millis; }
#endif
        pollautodownloadresponse();
    }

    quit();
    return EXIT_SUCCESS;

#if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
    } __except(stackdumper(0, GetExceptionInformation()), EXCEPTION_CONTINUE_SEARCH) { return 0; }
#endif
}

VAR(version, 1, AC_VERSION, 0);
VAR(protocol, 1, PROTOCOL_VERSION, 0);
