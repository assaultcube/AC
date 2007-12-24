// implementation of generic tools

#include "pch.h"
#include "cube.h"

///////////////////////// file system ///////////////////////

#ifndef WIN32
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#endif

string homedir = "";
vector<char *> packagedirs;

char *path(char *s, bool copy)
{
    if(copy)
    {
        static string tmp;
        s_strcpy(tmp, s);
        s = tmp;
    }
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
    size_t len = strlen(path);
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
    size_t len = strlen(dir);
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

gzFile opengzfile(const char *filename, const char *mode)
{
    const char *found = findfile(filename, mode);
    if(!found) return NULL;
    return gzopen(found, mode);
}

char *loadfile(const char *fn, int *size, const char *mode)
{
    FILE *f = openfile(fn, mode ? mode : "rb");
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

bool listdir(const char *dir, const char *ext, vector<char *> &files)
{
    int extsize = ext ? (int)strlen(ext)+1 : 0;
    #if defined(WIN32)
    s_sprintfd(pathname)("%s\\*.%s", dir, ext ? ext : "*");
    WIN32_FIND_DATA FindFileData;
    HANDLE Find = FindFirstFile(path(pathname), &FindFileData);
    if(Find != INVALID_HANDLE_VALUE)
    {
        do {
            files.add(newstring(FindFileData.cFileName, (int)strlen(FindFileData.cFileName) - extsize));
        } while(FindNextFile(Find, &FindFileData));
        return true;
    }
    #else
    string pathname;
    s_strcpy(pathname, dir);
    DIR *d = opendir(path(pathname));
    if(d)
    {
        struct dirent *de;
        while((de = readdir(d)) != NULL)
        {
            if(!ext) files.add(newstring(de->d_name));
            else
            {
                int namelength = (int)strlen(de->d_name) - extsize;
                if(namelength > 0 && de->d_name[namelength] == '.' && strncmp(de->d_name+namelength+1, ext, extsize-1)==0)
                    files.add(newstring(de->d_name, namelength));
            }
        }
        closedir(d);
        return true;
    }
    #endif
    else return false;
}

int listfiles(const char *dir, const char *ext, vector<char *> &files)
{
    int dirs = 0;
    if(listdir(dir, ext, files)) dirs++;
    string s;
    if(homedir[0])
    {
        s_sprintf(s)("%s%s", homedir, dir);
        if(listdir(s, ext, files)) dirs++;
    }
    loopv(packagedirs)
    {
        s_sprintf(s)("%s%s", packagedirs[i], dir);
        if(listdir(s, ext, files)) dirs++;
    }
    return dirs;
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
    static const int littleendian = 1;
    if(!*(const char *)&littleendian) loop(w, length) loop(i, stride/2)
    {
        uchar *p = (uchar *)memory+w*stride;
        uchar t = p[i];
        p[i] = p[stride-i-1];
        p[stride-i-1] = t;
    }
}
