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
    }
    SDL_ShowCursor(SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
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

SDL_Window *screen = NULL;
SDL_GLContext glcontext = 0;

VAR(screenw, 1, 0, 0);   // actual current resolution of screen/window
VAR(screenh, 1, 0, 0);
#define SCR_MINW 320
#define SCR_MINH 200
#define SCR_MAXW 10000
#define SCR_MAXH 10000
VARF(scr_w, SCR_MINW, -1, SCR_MAXW, initwarning("screen resolution")); // screen or window resolution, except for "fullscreen desktop" mode
VARF(scr_h, SCR_MINH, -1, SCR_MAXH, initwarning("screen resolution"));
VARF(depthbits, 0, 0, 32, initwarning("depth-buffer precision"));
VARF(stencilbits, 0, 0, 32, initwarning("stencil-buffer precision"));
VARF(fsaa, -1, -1, 16, initwarning("anti-aliasing"));
VARF(vsync, -1, -1, 1, initwarning("vertical sync"));

VAR(desktopw, 1, 0, 0); // resolution of desktop (assumed to be maximum resolution)
VAR(desktoph, 1, 0, 0);

#ifdef WIN32
// SDL_WarpMouseInWindow behaves erratically on Windows, so force relative mouse instead.
VARN(relativemouse, userelativemouse, 1, 1, 0);
#else
VARNP(relativemouse, userelativemouse, 0, 1, 1);
#endif

static bool shouldgrab = false, grabinput = false, minimized = false, centerwindow = true, canrelativemouse = true, relativemouse = false;

#ifdef SDL_VIDEO_DRIVER_X11
VAR(sdl_xgrab_bug, 0, 0, 1);
#endif

void inputgrab(bool on, bool delay = false)
{
#ifdef SDL_VIDEO_DRIVER_X11
    bool wasrelativemouse = relativemouse;
#endif
    if (on)
    {
        SDL_ShowCursor(SDL_FALSE);
        if (canrelativemouse && userelativemouse)
        {
            if (SDL_SetRelativeMouseMode(SDL_TRUE) >= 0)
            {
                SDL_SetWindowGrab(screen, SDL_TRUE);
                relativemouse = true;
            }
            else
            {
                SDL_SetWindowGrab(screen, SDL_FALSE);
                canrelativemouse = false;
                relativemouse = false;
            }
        }
    }
    else
    {
        SDL_ShowCursor(SDL_TRUE);
        if (relativemouse)
        {
            SDL_SetWindowGrab(screen, SDL_FALSE);
            SDL_SetRelativeMouseMode(SDL_FALSE);
            relativemouse = false;
        }
    }
    shouldgrab = delay;

#ifdef SDL_VIDEO_DRIVER_X11
    if ((relativemouse || wasrelativemouse) && sdl_xgrab_bug)
    {
        // Workaround for buggy SDL X11 pointer grabbing
        union { SDL_SysWMinfo info; uchar buf[sizeof(SDL_SysWMinfo) + 128]; };
        SDL_GetVersion(&info.version);
        if (SDL_GetWindowWMInfo(screen, &info) && info.subsystem == SDL_SYSWM_X11)
        {
            if (relativemouse)
            {
                uint mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask;
                XGrabPointer(info.info.x11.display, info.info.x11.window, True, mask, GrabModeAsync, GrabModeAsync, info.info.x11.window, None, CurrentTime);
            }
            else XUngrabPointer(info.info.x11.display, CurrentTime);
        }
    }
#endif
}


void setfullscreen(bool enable)
{
    if(!screen) return;
    if(enable == !(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN))
    {
        extern int fullscreendesktop;
        SDL_SetWindowFullscreen(screen, enable ? (fullscreendesktop ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN) : 0);
        inputgrab(grabinput = enable || grabinput);
        if(!enable) SDL_SetWindowSize(screen, scr_w, scr_h);
        if(!enable && centerwindow) SDL_SetWindowPosition(screen, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
}

VARF(fullscreen, 0, 1, 1, setfullscreen(fullscreen != 0));

void resetfullscreen()
{
    setfullscreen(false);
    setfullscreen(true);
}

VARF(fullscreendesktop, 0, 1, 1, if(fullscreen) resetfullscreen());

SVARFP(lang, "en", filterlang(lang, lang));

void writeinitcfg()
{
    if(!restoredinits) return;
    filerotate("config/init", "cfg", CONFIGROTATEMAX); // keep five old init config sets
    stream *f = openfile(path("config/init.cfg", true), "w");
    if(!f) return;
    f->printf("// automatically written on exit, DO NOT MODIFY\n// modify settings in game\n");
    f->printf("fullscreen %d\n", fullscreen);
    f->printf("fullscreendesktop %d\n", fullscreendesktop);
    f->printf("scr_w %d\n", scr_w);
    f->printf("scr_h %d\n", scr_h);
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

/* Convert pixel in pixel format pf to RGB values, storing 
 * them in r, g, and b.
 * If the pixel has an alpha channel, it is blended with
 * the background color, which is assumed to be {0, 0, 0}. 
 * This function does not check the arguments passed to it.
 * Be cautious when using it. */
/* This needs some optimization especially wrt the alpha multiplication. */
static inline void pix_to_rgb(SDL_PixelFormat *pf,   
                              unsigned long    pixel, /* up to 32 bits */
                              unsigned char   *r,     /* 8 bits  */
                              unsigned char   *g,
                              unsigned char   *b)
{
        /* palette lookup */
        if(pf->BytesPerPixel == 1)
        {
            SDL_Color *color;
            color = pf->palette->colors + pixel;
            *r = color->r * ((float)color->a/255); /* convert alpha to a value ranged 0..1 */
            *g = color->g * ((float)color->a/255);
            *b = color->b * ((float)color->a/255);
            return;
        }
        /* color values */
        else {
            /* no alpha */
            if(pf->Amask == 0) {            
                *r = ((pixel & pf->Rmask) >> pf->Rshift) << pf->Rloss;
                *g = ((pixel & pf->Gmask) >> pf->Gshift) << pf->Gloss;
                *b = ((pixel & pf->Bmask) >> pf->Bshift) << pf->Bloss;
                return;
            }
            /* has alpha */
            else {
                unsigned char a = ((pixel & pf->Amask) >> pf->Ashift) << pf->Aloss;
                *r = (((pixel & pf->Rmask) >> pf->Rshift) << pf->Rloss) * ((float)a/255);
                *g = (((pixel & pf->Gmask) >> pf->Gshift) << pf->Gloss) * ((float)a/255);
                *b = (((pixel & pf->Bmask) >> pf->Bshift) << pf->Bloss) * ((float)a/255);
                return;                
            }
        }
}

/* PPM is a simple uncompressed RGB image format. See http://netpbm.sourceforge.net */

/* Saves image in PPM format to the file filename. 
 * The scores are saved to the file as comments. 
 * This function does not check the arguments passed to it.
 * Be cautious when using it.*/
int save_ppm(const char *filename, SDL_Surface *image)
{

    unsigned in_row  = image->w,
             in_col  = image->h;
    const unsigned 
             maxval  = 255; /* SDL_Surface seems to only support a maximum of
                             * 8 Bits per component, which should make this fine. */
                 
    stream *f = openfile(filename, "wb");
    if(!f)
    {
        conoutf("save_ppm: could not open %s", filename);
        return -1;
    }

    /* write magic number and indication this is an AC screenshot */
    f->printf("P6\n# AssaultCube screenshot\n# Scores:\n");

    /* write ascii scores into ppm as comments */
    const char *scores = asciiscores(false);

    char *saveptr;
    char *m_scores = strdup(scores);

    if(!m_scores)
        return -1; /* epic malloc fail :c */


    /* Tokenize the string with strtok_r because once we write a
     * newline, we end the comment. This would spill into the data. */
    char *tmp = strtok(m_scores, "\n\r", &saveptr);

    do
    {
        f->write("# ", 2);
        f->write(tmp, strlen(tmp));
    } while((tmp = strtok(NULL, "\n", &saveptr)));

    free(m_scores);

    f->printf("%u %u\n%u\n", in_row, in_col, maxval);

    /* Now, we need to output the pixel values.
     * Luckily, we don't need to care about endianness right now,
     * since we only need to output single bytes. */

    
    SDL_LockSurface(image);

    /* this should be fine since every pointer type can alias to char * */
    unsigned char *data = (unsigned char *)image->pixels;

    /* Sadly, type punning with unions is undefined behavior in C++.
     * This means we need to use memcpy, but we can still use a
     * union to save stack space since we'll only be using one at a time. */

    union
    {
        unsigned int  two;
        unsigned long four;
    } t;

    /* R, G, B, in exactly that order. */
    /* To ease writing, this is an array. */
    unsigned char rgb[3];
    
    switch(image->format->BytesPerPixel)
    {
        case 1:
        for(unsigned i = 0; i < in_row * in_col; i++)
        {
            /* woohoo, strict aliasing rules working in our favor */
            pix_to_rgb(image->format, data[i], rgb, rgb + 1, rgb + 2);
            
            /* write the RGB values to the file */
            f->write(rgb, 3);
        }
        break;

        case 2:
        for(unsigned i = 0; i < in_row * in_col * 2; i += 2)
        {
            /* we need to type pun here. if int is 4 bytes,
             * this is still fine because the shift amounts 
             * should still be correct */
            memcpy(&t.two, data + i, 2);
            pix_to_rgb(image->format, t.two, rgb, rgb + 1, rgb + 2);
            f->write(rgb, 3);
        }
        break;

        case 3:
        for(unsigned i = 0; i < in_row * in_col * 3; i += 3)
        {
            memcpy(&t.four, data + i, 3);
            pix_to_rgb(image->format, t.four, rgb, rgb + 1, rgb + 2);
            f->write(rgb, 3);
        }
        break;

        case 4:
        for(unsigned i = 0; i < in_row * in_col * 4; i += 4)
        {
            memcpy(&t.four, data + i, 4);
            pix_to_rgb(image->format, t.four, rgb, rgb + 1, rgb + 2);
            f->write(rgb, 3);
        }
        break;

        default: /* can't happen */
        break;
    }

    SDL_UnlockSurface(image);
    
    delete f;
    return 0;

}

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
    int src_w = mapshot ? minimaplastsize : screenw;
    int src_h = mapshot ? minimaplastsize : screenh;
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
        case 3: // ppm
            if(save_ppm(imagepath, image) < 0) conoutf("\f3Error saving ppm file");
    }

    SDL_FreeSurface(image);
}

FVARP(screenshotscale, 0.1f, 1.0f, 1.0f);
VARP(jpegquality, 10, 85, 100);  // best: 100 - good: 85 [default] - bad: 70 - terrible: 50
VARP(screenshottype, 0, 1, 3);

const char *getscrext()
{
     const char *screxts[] = { ".bmp", ".jpg", ".png", ".ppm"};
     return screxts[screenshottype % 4];
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

void updatescreensize()
{
    SDL_GetWindowSize(screen, &screenw, &screenh);
    if(screenh < 1) screenh = 1;
    VIRTW = screenw * VIRTH / screenh;
    glViewport(0, 0, screenw, screenh);
}

void screenres(int w, int h)
{
    if(!screen) return;
    scr_w = clamp(w, SCR_MINW, SCR_MAXW);
    scr_h = clamp(h, SCR_MINH, SCR_MAXH);
    if(fullscreendesktop)
    {
        scr_w = min(scr_w, desktopw);
        scr_h = min(scr_h, desktoph);
    }
    if(!(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN)) SDL_SetWindowSize(screen, scr_w, scr_h);
    else if(!fullscreendesktop) initwarning("screen resolution");
}
#if defined(WIN32) || defined(__APPLE__) || !defined(WIN32)
void setresdata(char *s, enet_uint32 c)
{
    extern hashtable<char *, enet_uint32> &resdata;
    resdata[newstring(s)] = c;
}
#endif

COMMANDF(screenres, "ii", (int *w, int *h) { screenres(*w, *h); });

int setgamma(int newgamma) // replacement for SDL_SetGamma
{
    return screen ? SDL_SetWindowBrightness(screen, newgamma/100.0f) : -1;
}

static int curgamma = 100;

VARNFP(gamma, vgamma, 30, 100, 300,
{
    if(vgamma != curgamma)
    {
        if(setgamma(vgamma) == -1) conoutf("Could not set gamma: %s", SDL_GetError());
        curgamma = vgamma;
    }
});

void restoregamma()
{
    if(curgamma != 100)
    {
        setgamma(100);
        setgamma(curgamma);
    }
}

void getdisplayresolutions()
{
    string res = "";
    int lh = 0, lw = 0, n = SDL_GetNumDisplayModes(0);
    SDL_DisplayMode mode;
    loopi(n)
    {
        if(SDL_GetDisplayMode(0, i, &mode) == 0 && lw != mode.w && lh != mode.h) concatformatstring(res, "%s%d %d", *res ? " " : "", (lw = mode.w), (lh = mode.h));
    }
    result(res);
}
COMMAND(getdisplayresolutions, "");

void setupscreen(int &useddepthbits, int &usedfsaa)
{
    if(glcontext)
    {
        SDL_GL_DeleteContext(glcontext);
        glcontext = NULL;
    }
    if(screen)
    {
        SDL_DestroyWindow(screen);
        screen = NULL;
    }

    SDL_Rect desktop;
    if(SDL_GetDisplayBounds(0, &desktop) < 0) fatal("failed querying desktop bounds: %s", SDL_GetError());
    desktopw = desktop.w;
    desktoph = desktop.h;
    if(scr_w < 0 || scr_h < 0) { scr_w = desktopw; scr_h = desktoph; } // first run: set to fullscreen res

    int modes = SDL_GetNumDisplayModes(0);
    if(modes >= 1)
    {
        bool hasmode = false;
        SDL_DisplayMode mode;
        loopi(modes)
        {
            if(SDL_GetDisplayMode(0, i, &mode) == 0 && scr_w <= mode.w && scr_h <= mode.h) { hasmode = true; break; }
        }
        if(!hasmode && SDL_GetDisplayMode(0, 0, &mode) == 0) { scr_w = mode.w; scr_h = mode.h; }

    }
    int winw = fullscreen && fullscreendesktop ? desktopw : scr_w;
    int winh = fullscreen && fullscreendesktop ? desktoph : scr_h;

    int flags = SDL_WINDOW_RESIZABLE;
    if(fullscreen) flags |= fullscreendesktop ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN;

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
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        screen = SDL_CreateWindow("AssaultCube",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            winw, winh,
            SDL_WINDOW_OPENGL | flags);
        if(screen)
        {
            glcontext = SDL_GL_CreateContext(screen);
            if(glcontext) break;
            SDL_DestroyWindow(screen);
        }
    }
    if(!screen) fatal("Unable to create OpenGL screen");
    else
    {
        if(depthbits && (config&1)==0) conoutf("%d bit z-buffer not supported - disabling", depthbits);
        if(stencilbits && (config&2)==0) conoutf("%d bit stencil buffer not supported - disabling", stencilbits);
        if(fsaa>0 && (config&4)==0) conoutf("%dx anti-aliasing not supported - disabling", fsaa);
    }

    if(vsync>=0) SDL_GL_SetSwapInterval(vsync);

    updatescreensize();

    inputgrab(grabinput = fullscreen ? true : false);

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

    int useddepthbits = 0, usedfsaa = 0;
    setupscreen(useddepthbits, usedfsaa);
    gl_init(screenw, screenh, useddepthbits, usedfsaa);

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
    drawscope(true);
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

int keyrepeatmask = 0, textinputmask = 0; // bunches of flags for the same functions
Uint32 textinputtime = 0;

void keyrepeat(bool on, int mask)
{
    if(on) keyrepeatmask |= mask;
    else keyrepeatmask &= ~mask;
}

void textinput(bool on, int mask)
{
    if(on)
    {
        if(!textinputmask)
        {
            SDL_StartTextInput();
            textinputtime = SDL_GetTicks();
        }
        textinputmask |= mask;
    }
    else
    {
        textinputmask &= ~mask;
        if(!textinputmask) SDL_StopTextInput();
    }
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

static void resetmousemotion()
{
    if (grabinput && !relativemouse && !(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN))
    {
        SDL_WarpMouseInWindow(screen, screenw / 2, screenh / 2);
    }
}

static inline bool skipmousemotion(SDL_Event &event)
{
    if(event.type != SDL_MOUSEMOTION) return true;
    if(!(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN))
    {
        if(event.motion.x == screenw / 2 && event.motion.y == screenh / 2) return true;  // ignore any motion events generated SDL_WarpMouse
    }
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
#define EVENTDEBUG(x) x
//#define EVENTDEBUG(x)
EVENTDEBUG(VAR(debugevents, 0, 0, 2));

void checkinput()
{
    SDL_Event event;
    Uint32 lasttype = 0, lastbut = 0;
    int tdx=0,tdy=0;
    int focused = 0;
    while(events.length() || SDL_PollEvent(&event))
    {
        if(events.length()) event = events.remove(0);

        EVENTDEBUG(int thres = 1; defformatstring(eb)("EVENT %d", event.type));

        if (focused && event.type != SDL_WINDOWEVENT) { if (grabinput != (focused > 0)) inputgrab(grabinput = focused > 0, shouldgrab); focused = 0; }

        switch(event.type)
        {
            case SDL_QUIT:
                quit();
                break;

            case SDL_KEYDOWN:
                if(bootstrapentropy > 0 && (--bootstrapentropy & 2)) mapscreenshot(NULL, false, -1, 1000.0f / (1000 + rnd(400)), 0, 0);
            case SDL_KEYUP:
                EVENTDEBUG(thres = 2; concatstring(eb, event.type == SDL_KEYUP ? "(SDL_KEYUP)" : "(SDL_KEYDOWN)"));
                entropy_add_byte(event.key.keysym.sym ^ totalmillis);
                EVENTDEBUG(concatformatstring(eb, " sym %d (%Xh), scancode %d (%Xh), state %d, repeat %d", event.key.keysym.sym, event.key.keysym.sym, event.key.keysym.scancode, event.key.keysym.scancode, event.key.state, event.key.repeat));
                if(event.key.keysym.sym == SDLK_SCANCODE_MASK) event.key.keysym.sym |= event.key.keysym.scancode; // workaround SDL 2.0.5 bug which returns sym == 40000000h for all dead keys
                if(!event.key.repeat || keyrepeatmask) keypress(event.key.keysym.sym, event.key.keysym.scancode, event.key.state==SDL_PRESSED, (SDL_Keymod)event.key.keysym.mod);
                break;

            case SDL_TEXTINPUT:
                EVENTDEBUG(thres = 2; concatformatstring(eb, "(SDL_TEXTINPUT) %s %d %d", escapestring(event.text.text), textinputmask, int(event.text.timestamp - textinputtime)));
                if(textinputmask && int(event.text.timestamp - textinputtime) >= 5) // mute textinput for a few milliseconds, to avoid receiving the "t" that switched on the console
                {
                    processtextinput(event.text.text);
                }
                break;

            case SDL_WINDOWEVENT:
            {
                EVENTDEBUG(concatstring(eb, "(SDL_WINDOWEVENT) "));
                switch(event.window.event)
                {
                    case SDL_WINDOWEVENT_SHOWN: // window has been shown
                        EVENTDEBUG(concatstring(eb, " SDL_WINDOWEVENT_SHOWN"));
                        break;

                    case SDL_WINDOWEVENT_HIDDEN: // window has been hidden
                        EVENTDEBUG(concatstring(eb, " SDL_WINDOWEVENT_HIDDEN"));
                        break;

                    case SDL_WINDOWEVENT_EXPOSED: // window has been exposed and should be redrawn
                        EVENTDEBUG(concatstring(eb, " SDL_WINDOWEVENT_EXPOSED"));
                        break;

                    case SDL_WINDOWEVENT_MOVED: // window has been moved to data1, data2
                        EVENTDEBUG(concatformatstring(eb, " SDL_WINDOWEVENT_MOVED x %d, y %d", event.window.data1, event.window.data2));
                        if(!(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN)) centerwindow = false;
                        break;

                    case SDL_WINDOWEVENT_RESIZED: // window has been resized to data1 x data2; this is event is always preceded by SDL_WINDOWEVENT_SIZE_CHANGED
                        EVENTDEBUG(concatformatstring(eb, " SDL_WINDOWEVENT_RESIZED %d x %d", event.window.data1, event.window.data2));
                        if(!fullscreendesktop || !(SDL_GetWindowFlags(screen) & SDL_WINDOW_FULLSCREEN))
                        {
                            scr_w = clamp(event.window.data1, SCR_MINW, SCR_MAXW);
                            scr_h = clamp(event.window.data2, SCR_MINH, SCR_MAXH);
                        }
                        break;

                    case SDL_WINDOWEVENT_SIZE_CHANGED: // window size has changed, either as a result of an API call or through the system or user changing the window size; this event is followed by SDL_WINDOWEVENT_RESIZED if the size was changed by an external event, i.e. the user or the window manager
                        updatescreensize();
                        EVENTDEBUG(concatformatstring(eb, " SDL_WINDOWEVENT_SIZE_CHANGED %dx%d", screenw, screenh));
                        break;

                    case SDL_WINDOWEVENT_MINIMIZED: // window has been minimized
                        EVENTDEBUG(concatstring(eb, " SDL_WINDOWEVENT_MINIMIZED"));
                        inputgrab(false);
                        minimized = 1;
                        break;

                    case SDL_WINDOWEVENT_MAXIMIZED: // window has been maximized
                    case SDL_WINDOWEVENT_RESTORED: // window has been restored to normal size and position
                        EVENTDEBUG(concatstring(eb, event.window.event == SDL_WINDOWEVENT_RESTORED ? "SDL_WINDOWEVENT_RESTORED" : "SDL_WINDOWEVENT_MAXIMIZED"));
                        minimized = 0;
                        break;

                    case SDL_WINDOWEVENT_ENTER: // window has gained mouse focus
                        EVENTDEBUG(concatstring(eb, " SDL_WINDOWEVENT_ENTER"));
                        shouldgrab = false;
                        focused = 1;
                        break;

                    case SDL_WINDOWEVENT_LEAVE: // window has lost mouse focus
                        EVENTDEBUG(concatstring(eb, " SDL_WINDOWEVENT_LEAVE"));
                        shouldgrab = false;
                        focused = -1;
                        break;

                    case SDL_WINDOWEVENT_FOCUS_GAINED: // window has gained keyboard focus
                        EVENTDEBUG(concatstring(eb, " SDL_WINDOWEVENT_FOCUS_GAINED"));
                        shouldgrab = true;
                        break;

                    case SDL_WINDOWEVENT_FOCUS_LOST: // window has lost keyboard focus
                        EVENTDEBUG(concatstring(eb, " SDL_WINDOWEVENT_FOCUS_LOST"));
                        shouldgrab = false;
                        focused = -1;
                        break;
                }
                break;
            }

            case SDL_MOUSEMOTION:
                EVENTDEBUG(thres = 2; concatformatstring(eb, "(SDL_MOUSEMOTION) x %d, y %d, dx %d, dy %d", event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel));
                if(ignoremouse) { ignoremouse--; break; }
                if(grabinput && !skipmousemotion(event))
                {
                    int dx = event.motion.xrel, dy = event.motion.yrel;
                    entropy_add_byte(dx ^ (256 - dy));
                    checkmousemotion(dx, dy);
                    resetmousemotion();
                    tdx+=dx;tdy+=dy;
                }
                else if (shouldgrab) inputgrab(grabinput = true);
                break;

            case SDL_MOUSEBUTTONDOWN:
                if(!grabinput) // click-to-grab
                {
                    EVENTDEBUG(concatformatstring(eb, "(SDL_MOUSEBUTTONDOWN) button %d, state %d, clicks %d, x %d, y %d", event.button.button, event.button.state, event.button.clicks, event.button.x, event.button.y));
                    inputgrab(grabinput = true);
                    #ifdef WIN32
                    if(!fullscreen)
                    {
                        // If AC runs in windowed mode on Windows 10 and if you press WIN+D, then move mouse on desktop, press WIN+D again, click in the AC window,
                        // the mouse cursor remains visible and the mouse is not contrained to the AC window anymore. It is as if SDL2 does not properly give focus
                        // to the AC window and so our workaround is to explicitly trigger the focus by hiding and showing the window. This issue exists in
                        // SDL 2.0.12 and is fixed in 2.0.14 however we currently can not use 2.0.14 because it has problems with handling of ALT+TAB hotkey behavior.
                        // Please re-test and remove this workaround once a fixed version of SDL is available.
                        SDL_HideWindow(screen);
                        SDL_ShowWindow(screen);
                    }
                    #endif
                    break;
                }

            case SDL_MOUSEBUTTONUP:
                EVENTDEBUG(concatformatstring(eb, "(SDL_MOUSEBUTTON%s) button %d, state %d, clicks %d, x %d, y %d", event.type == SDL_MOUSEBUTTONUP ? "UP" : "DOWN", event.button.button, event.button.state, event.button.clicks, event.button.x, event.button.y));
                if(lasttype==event.type && lastbut==event.button.button) break;
                keypress(-(event.button.button > 3 ? (event.button.button + 4) : event.button.button), 0, event.button.state != SDL_RELEASED);
                lasttype = event.type;
                lastbut = event.button.button;
                break;

            case SDL_MOUSEWHEEL:
                EVENTDEBUG(concatformatstring(eb, "(SDL_MOUSEWHEEL) x %d, y %d", event.wheel.x, event.wheel.y));
                if(event.wheel.y)
                {
                    int key = event.wheel.y > 0 ? SDL_AC_BUTTON_WHEELUP : SDL_AC_BUTTON_WHEELDOWN;
                    keypress(key, 0, true); // Emulate SDL1-style mouse wheel events by immediately "releasing" the wheel "button"
                    keypress(key, 0, false);
                }
                if(event.wheel.x)
                {
                    int key = event.wheel.x > 0 ? SDL_AC_BUTTON_WHEELRIGHT : SDL_AC_BUTTON_WHEELLEFT;
                    keypress(key, 0, true);
                    keypress(key, 0, false);
                }
                break;
        }
        EVENTDEBUG(if(debugevents >= thres) conoutf("%s", eb));
    }
    if(tdx || tdy)
    {
        entropy_add_byte(tdy + 5 * tdx);
        mousemove(tdx, tdy);
    }
    if (focused) { if (grabinput != (focused > 0)) inputgrab(grabinput = focused > 0, shouldgrab); focused = 0; }
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

#ifdef THISISNOTDEFINED //WIN32
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
    // VS2017+ does not support %n in format strings by default due to security concerns.
    // However, currently we are making use of this functionality and therefore it is being enabled below.
    // In the future we should get rid of this. See also https://docs.microsoft.com/en-us/previous-versions/hf4y5e3w(v=vs.140)?redirectedfrom=MSDN
    _set_printf_count_output(1);
    #endif

    bool dedicated = false;
    bool quitdirectly = false;
    char *initscript = NULL;
    char *initdemo = NULL;

    setlocale(LC_ALL, "POSIX");
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

    #define SDLVERSIONSTRING  STRINGIFY(SDL_MAJOR_VERSION) "." STRINGIFY(SDL_MINOR_VERSION) "." STRINGIFY(SDL_PATCHLEVEL)
    initlog("sdl (" SDLVERSIONSTRING ")");
    int par = 0;
#ifdef _DEBUG
    par = SDL_INIT_NOPARACHUTE;
#endif
    if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO|par)<0) fatal("Unable to initialize SDL");
    SDL_version sdlver;
    SDL_GetVersion(&sdlver);
    if(SDL_COMPILEDVERSION != SDL_VERSIONNUM(sdlver.major, sdlver.minor, sdlver.patch))
        clientlogf("SDL: compiled version " SDLVERSIONSTRING ", linked version %u.%u.%u", sdlver.major, sdlver.minor, sdlver.patch);

#if 0
    if(highprocesspriority) setprocesspriority(true);
#endif

    if (!dedicated) initlog("net (" STRINGIFY(ENET_VERSION_MAJOR) "." STRINGIFY(ENET_VERSION_MINOR) "." STRINGIFY(ENET_VERSION_PATCH) ")");
    if(enet_initialize()<0) fatal("Unable to initialise network module");

    if (!dedicated) initclient();

    initserver(dedicated);  // never returns if dedicated

    initlog("world (" STRINGIFY(AC_VERSION) ")");
    empty_world(7, true);

    initlog("video: sdl");
    if(SDL_InitSubSystem(SDL_INIT_VIDEO)<0) fatal("Unable to initialize SDL Video");

    initlog("video: mode");
    int useddepthbits = 0, usedfsaa = 0;
    setupscreen(useddepthbits, usedfsaa);

    initlog("video: misc");

#ifndef __APPLE__
    SDL_Surface *icon = IMG_Load("packages/misc/icon.png");
    SDL_SetWindowIcon(screen, icon);
#endif

#ifdef __APPLE__
    // Fix https://github.com/assaultcube/AC/issues/300
    SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1", SDL_HINT_OVERRIDE);
#endif

    SDL_ShowCursor(0);

    initlog("gl");
    gl_checkextensions();
    gl_init(screenw, screenh, useddepthbits, usedfsaa);

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
        if(multiplayer(NULL)) 
        {
            if(ispaused) curtime = 0;
            else curtime = elapsed;
        }
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
            gl_drawframe(screenw, screenh, fps<lowfps ? fps/lowfps : (fps>highfps ? fps/highfps : 1.0f), fps, elapsed);
            if(frames>4) SDL_GL_SwapWindow(screen);
        }

        if(needsautoscreenshot)
        {
            showscores(true);
            // draw again after swapping buffers, to make sure menu is captured
            // in the screenshot regardless of which frame buffer is current
            if(!minimized)
            {
                gl_drawframe(screenw, screenh, fps<lowfps ? fps/lowfps : (fps>highfps ? fps/highfps : 1.0f), fps, elapsed);
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
