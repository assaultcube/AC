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

#ifdef WIN32
char *getregszvalue(HKEY root, const char *keystr, const char *query)
{
    HKEY key;
    if(RegOpenKeyEx(HKEY_CURRENT_USER, keystr, 0, KEY_READ, &key)==ERROR_SUCCESS)
    {
        DWORD type = 0, len = 0;
        if(RegQueryValueEx(key, query, 0, &type, 0, &len)==ERROR_SUCCESS && type==REG_SZ)
        {
            char *val = new char[len];
            long result = RegQueryValueEx(key, query, 0, &type, (uchar *)val, &len);
            if(result==ERROR_SUCCESS) 
            {
                RegCloseKey(key);
                val[len-1] = '\0';
                return val;
            }
            delete[] val;
        }
        RegCloseKey(key);
    }
    return NULL;
}
#endif

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

char *path(const char *s, bool copy)
{
    static string tmp;
    s_strcpy(tmp, s);
    path(tmp);
    return tmp;
}

const char *behindpath(const char *s)
{
    const char *t = s;
    for( ; (s = strpbrk(s, "/\\")); t = ++s);
    return t;
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
    string tmpdir;
    s_strcpy(tmpdir, dir);

#ifdef WIN32
    const char substitute[] = "?MYDOCUMENTS?";
    if(!strncmp(dir, substitute, strlen(substitute)))
    {
        char *mydocuments = getregszvalue(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", "Personal");
        if(mydocuments)
        {
            s_sprintf(tmpdir)("%s%s", mydocuments, dir+strlen(substitute));
            delete[] mydocuments;
        }
    }
#endif

    printf("Using home directory: %s\n", tmpdir);
    fixdir(s_strcpy(homedir, tmpdir));
    createdir(homedir);
}

void addpackagedir(const char *dir)
{
    printf("Adding package directory: %s\n", dir);
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

int getfilesize(const char *filename)
{
    const char *found = findfile(filename, "rb");
    if(!found) return -1;
    FILE *fp = fopen(found, "rb");
    if(!fp) return -1;
    fseek(fp, 0, SEEK_END);
    int len = ftell(fp);
    fclose(fp);
    return len;
}

FILE *openfile(const char *filename, const char *mode)
{
    const char *found = findfile(filename, mode);
#ifndef STANDALONE
    if(mode && (mode[0]=='w' || mode[0]=='a')) conoutf("writing to file: %s", found);
#endif
    if(!found) return NULL;
    return fopen(found, mode);
}

gzFile opengzfile(const char *filename, const char *mode)
{
    const char *found = findfile(filename, mode);
#ifndef STANDALONE
    if(mode && (mode[0]=='w' || mode[0]=='a')) conoutf("writing to file: %s", found);
#endif
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
    int rlen = (int)fread(buf, 1, len, f);
    fclose(f);
    if(len!=rlen && (!mode || strchr(mode, 'b')))
    {
        delete[] buf;
        return NULL;
    }
    if(size!=NULL) *size = rlen;
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

bool delfile(const char *path)
{
    return !remove(path);
}

///////////////////////// debugging ///////////////////////

#if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
void stackdumper(unsigned int type, EXCEPTION_POINTERS *ep)
{
    if(!ep) fatal("unknown type");
    EXCEPTION_RECORD *er = ep->ExceptionRecord;
    CONTEXT *context = ep->ContextRecord;
    string out, t;
    s_sprintf(out)("Win32 Exception: 0x%x [0x%x]\n\n", er->ExceptionCode, er->ExceptionCode==EXCEPTION_ACCESS_VIOLATION ? er->ExceptionInformation[1] : -1);
    STACKFRAME sf = {{context->Eip, 0, AddrModeFlat}, {}, {context->Ebp, 0, AddrModeFlat}, {context->Esp, 0, AddrModeFlat}, 0};
    SymInitialize(GetCurrentProcess(), NULL, TRUE);

    while(::StackWalk(IMAGE_FILE_MACHINE_I386, GetCurrentProcess(), GetCurrentThread(), &sf, context, NULL, ::SymFunctionTableAccess, ::SymGetModuleBase, NULL))
    {
        struct { IMAGEHLP_SYMBOL sym; string n; } si = { { sizeof( IMAGEHLP_SYMBOL ), 0, 0, 0, sizeof(string) } };
        IMAGEHLP_LINE li = { sizeof( IMAGEHLP_LINE ) };
        DWORD off;
        if(SymGetSymFromAddr(GetCurrentProcess(), (DWORD)sf.AddrPC.Offset, &off, &si.sym) && SymGetLineFromAddr(GetCurrentProcess(), (DWORD)sf.AddrPC.Offset, &off, &li))
        {
            char *del = strrchr(li.FileName, '\\');
            s_sprintf(t)("%s - %s [%d]\n", si.sym.Name, del ? del + 1 : li.FileName, li.LineNumber);
            s_strcat(out, t);
        }
    }
    fatal(out);
}
#elif defined(linux) || defined(__linux) || defined(__linux__)

#include <execinfo.h>

// stack dumping on linux, inspired by Sachin Agrawal's sample code

struct signalbinder
{
    static void stackdumper(int sig)
    {
        printf("stacktrace:\n");

        const int BTSIZE = 25;
        void *array[BTSIZE];
        int n = backtrace(array, BTSIZE);
        char **symbols = backtrace_symbols(array, n);
        for(int i = 0; i < n; i++)
        {
            printf("%s\n", symbols[i]);
        }
        free(symbols);

        fatal("AssaultCube error (%d)", sig);

    }

    signalbinder()
    {
        // register signals to dump the stack if they are raised,
        // use constructor for early registering
        signal(SIGSEGV, stackdumper);
        signal(SIGFPE, stackdumper);
        signal(SIGILL, stackdumper);
        signal(SIGBUS, stackdumper);
        signal(SIGSYS, stackdumper);
        signal(SIGABRT, stackdumper);
    }
};

signalbinder sigbinder;

#endif


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

bool isbigendian()
{
    long one = 1;
    return !(*((char *)(&one)));
}

//////////////// geometry utils ////////////////

static inline float det2x2(float a, float b, float c, float d) { return a*d - b*c; }
static inline float det3x3(float a1, float a2, float a3,
                           float b1, float b2, float b3,
                           float c1, float c2, float c3)
{
    return a1 * det2x2(b2, b3, c2, c3)
         - b1 * det2x2(a2, a3, c2, c3)
         + c1 * det2x2(a2, a3, b2, b3);
}

float glmatrixf::determinant() const
{
    float a1 = v[0], a2 = v[1], a3 = v[2], a4 = v[3],
          b1 = v[4], b2 = v[5], b3 = v[6], b4 = v[7],
          c1 = v[8], c2 = v[9], c3 = v[10], c4 = v[11],
          d1 = v[12], d2 = v[13], d3 = v[14], d4 = v[15];

    return a1 * det3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4)
         - b1 * det3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4)
         + c1 * det3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4)
         - d1 * det3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4);
}

void glmatrixf::adjoint(const glmatrixf &m)
{
    float a1 = m.v[0], a2 = m.v[1], a3 = m.v[2], a4 = m.v[3],
          b1 = m.v[4], b2 = m.v[5], b3 = m.v[6], b4 = m.v[7],
          c1 = m.v[8], c2 = m.v[9], c3 = m.v[10], c4 = m.v[11],
          d1 = m.v[12], d2 = m.v[13], d3 = m.v[14], d4 = m.v[15];

    v[0]  =  det3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4);
    v[1]  = -det3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4);
    v[2]  =  det3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4);
    v[3]  = -det3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4);

    v[4]  = -det3x3(b1, b3, b4, c1, c3, c4, d1, d3, d4);
    v[5]  =  det3x3(a1, a3, a4, c1, c3, c4, d1, d3, d4);
    v[6]  = -det3x3(a1, a3, a4, b1, b3, b4, d1, d3, d4);
    v[7]  =  det3x3(a1, a3, a4, b1, b3, b4, c1, c3, c4);

    v[8]  =  det3x3(b1, b2, b4, c1, c2, c4, d1, d2, d4);
    v[9]  = -det3x3(a1, a2, a4, c1, c2, c4, d1, d2, d4);
    v[10] =  det3x3(a1, a2, a4, b1, b2, b4, d1, d2, d4);
    v[11] = -det3x3(a1, a2, a4, b1, b2, b4, c1, c2, c4);

    v[12] = -det3x3(b1, b2, b3, c1, c2, c3, d1, d2, d3);
    v[13] =  det3x3(a1, a2, a3, c1, c2, c3, d1, d2, d3);
    v[14] = -det3x3(a1, a2, a3, b1, b2, b3, d1, d2, d3);
    v[15] =  det3x3(a1, a2, a3, b1, b2, b3, c1, c2, c3);
}

bool glmatrixf::invert(const glmatrixf &m, float mindet)
{
    float det = m.determinant();
    if(fabs(det) < mindet) return false;
    adjoint(m);
    float invdet = 1/det;
    loopi(16) v[i] *= invdet;
    return true;
}

