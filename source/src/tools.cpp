// implementation of generic tools

#include "cube.h"

///////////////////////// file system ///////////////////////

#ifndef WIN32
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

string homedir = "";
vector<char *> packagedirs;

char *path(char *s)
{
    for(char *t = s; (t = strpbrk(t, "/\\")); *t++ = PATHDIV);
    for(char *prevdir = NULL, *curdir = s;;)
    {
        prevdir = curdir[0]==PATHDIV ? curdir+1 : curdir;
        curdir = strchr(prevdir, PATHDIV);
        if(!curdir) break;
        if(prevdir+1==curdir && prevdir[0]=='.')
        {
            memmove(prevdir, curdir+1, strlen(curdir+1)+1);
            curdir = prevdir;
        }
        else if(curdir[1]=='.' && curdir[2]=='.' && curdir[3]==PATHDIV)
        {
            if(prevdir+2==curdir && prevdir[0]=='.' && prevdir[1]=='.') continue;
            memmove(prevdir, curdir+4, strlen(curdir+4)+1);
            curdir = prevdir;
        }
    }
    return s;
}

const char *parentdir(const char *directory)
{
    const char *p = strrchr(directory, '/');
    if(!p) p = strrchr(directory, '\\');
    if(!p) p = directory;
    static string parent;
    size_t len = p-directory+1;
    s_strncpy(parent, directory, len);
    return parent;
}

bool fileexists(const char *path, const char *mode)
{
    bool exists = true;
    if(mode[0]=='w' || mode[0]=='a') path = parentdir(path);
#ifdef WIN32
    if(GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES) exists = false;
#else
    if(access(path, R_OK | (mode[0]=='w' || mode[0]=='a' ? W_OK : 0)) == -1) exists = false;
#endif
    return exists;
}

bool createdir(const char *path)
{
    int len = strlen(path);
    if(path[len-1]==PATHDIV)
    {
        static string strip;
        path = s_strncpy(strip, path, len);
    }
#ifdef WIN32
    return CreateDirectory(path, NULL)!=0;
#else
    return mkdir(path, 0777)==0;
#endif
}

static void fixdir(char *dir)
{
    path(dir);
    int len = strlen(dir);
    if(dir[len-1]!=PATHDIV)
    {
        dir[len] = PATHDIV;
        dir[len+1] = '\0';
    }
}

void sethomedir(const char *dir)
{
    fixdir(s_strcpy(homedir, dir));
}

void addpackagedir(const char *dir)
{
    fixdir(packagedirs.add(newstringbuf(dir)));
}

const char *findfile(const char *filename, const char *mode)
{
    static string s;
    if(homedir[0])
    {
        s_sprintf(s)("%s%s", homedir, filename);
        if(fileexists(s, mode)) return s;
        if(mode[0]=='w' || mode[0]=='a')
        {
            string dirs;
            s_strcpy(dirs, s);
            char *dir = strchr(dirs[0]==PATHDIV ? dirs+1 : dirs, PATHDIV);
            while(dir)
            {
                *dir = '\0';
                if(!fileexists(dirs, "r") && !createdir(dirs)) return s;
                *dir = PATHDIV;
                dir = strchr(dir+1, PATHDIV);
            }
            return s;
        }
    }
    if(mode[0]=='w' || mode[0]=='a') return filename;
    loopv(packagedirs)
    {
        s_sprintf(s)("%s%s", packagedirs[i], filename);
        if(fileexists(s, mode)) return s;
    }
    return filename;
}

FILE *openfile(const char *filename, const char *mode)
{
    const char *found = findfile(filename, mode);
    if(!found) return NULL;
    return fopen(found, mode);
}

#ifndef STANDALONE
gzFile opengzfile(const char *filename, const char *mode)
{
    const char *found = findfile(filename, mode);
    if(!found) return NULL;
    return gzopen(found, mode);
}
#endif

char *loadfile(const char *fn, int *size)
{
    FILE *f = openfile(fn, "rb");
    if(!f) return NULL;
    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    if(len<=0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = new char[len+1];
    if(!buf) { fclose(f); return NULL; }
    buf[len] = 0;
    size_t rlen = fread(buf, 1, len, f);
    fclose(f);
    if(size_t(len)!=rlen)
    {
        delete[] buf;
        return NULL;
    }
    if(size!=NULL) *size = len;
    return buf;
}

///////////////////////// misc tools ///////////////////////

bool cmpb(void *b, int n, enet_uint32 c)
{
    ENetBuffer buf;
    buf.data = b;
    buf.dataLength = n;
    return enet_crc32(&buf, 1)==c;
}

bool cmpf(char *fn, enet_uint32 c)
{
    int n = 0;
    char *b = loadfile(fn, &n);
    bool r = cmpb(b, n, c);
    delete[] b;
    return r;
}

void endianswap(void *memory, int stride, int length)   // little endian as storage format
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    loop(w, length) loop(i, stride/2)
    {
        uchar *p = (uchar *)memory+w*stride;
        uchar t = p[i];
        p[i] = p[stride-i-1];
        p[stride-i-1] = t;
    }
#endif
}
