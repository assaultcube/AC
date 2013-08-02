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

char *makerelpath(const char *dir, const char *file, const char *prefix, const char *cmd)
{
    static string tmp;
    if(prefix) copystring(tmp, prefix);
    else tmp[0] = '\0';
    if(file[0]=='<')
    {
        const char *end = strrchr(file, '>');
        if(end)
        {
            size_t len = strlen(tmp);
            copystring(&tmp[len], file, min(sizeof(tmp)-len, size_t(end+2-file)));
            file = end+1;
        }
    }
    if(cmd) concatstring(tmp, cmd);
    defformatstring(pname)("%s/%s", dir, file);
    concatstring(tmp, pname);
    return tmp;
}


char *path(char *s)
{
    for(char *curpart = s;;)
    {
        char *endpart = strchr(curpart, '&');
        if(endpart) *endpart = '\0';
        if(curpart[0]=='<')
        {
            char *file = strrchr(curpart, '>');
            if(!file) return s;
            curpart = file+1;
        }
        for(char *t = curpart; (t = strpbrk(t, "/\\")); *t++ = PATHDIV);
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
        if(endpart)
        {
            *endpart = '&';
            curpart = endpart+1;
        }
        else break;
    }
    return s;
}

char *unixpath(char *s)
{
    for(char *t = s; (t = strchr(t, '\\')); *t++ = '/');
    return s;
}

char *path(const char *s, bool copy)
{
    static string tmp;
    copystring(tmp, s);
    path(tmp);
    return tmp;
}

const char *parentdir(const char *directory)
{
    const char *p = directory + strlen(directory);
    while(p > directory && *p != '/' && *p != '\\') p--;
    static string parent;
    size_t len = p-directory+1;
    copystring(parent, directory, len);
    return parent;
}

const char *behindpath(const char *s)
{
    const char *t = s;
    for( ; (s = strpbrk(s, "/\\")); t = ++s);
    return t;
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
        path = copystring(strip, path, len);
    }
#ifdef WIN32
    return CreateDirectory(path, NULL)!=0;
#else
    return mkdir(path, 0777)==0;
#endif
}

size_t fixpackagedir(char *dir)
{
    path(dir);
    size_t len = strlen(dir);
    if(len > 0 && dir[len-1] != PATHDIV)
    {
        dir[len] = PATHDIV;
        dir[len+1] = '\0';
    }
    return len;
}

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

void sethomedir(const char *dir)
{
    string tmpdir;
    copystring(tmpdir, dir);

#ifdef WIN32
    const char substitute[] = "?MYDOCUMENTS?";
    if(!strncmp(dir, substitute, strlen(substitute)))
    {
        const char *regpath = "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders";
        char *mydocuments = getregszvalue(HKEY_CURRENT_USER, regpath, "Personal");
        if(mydocuments)
        {
            formatstring(tmpdir)("%s%s", mydocuments, dir+strlen(substitute));
            delete[] mydocuments;
        }
        else
        {
            printf("failed to retrieve 'Personal' path from '%s'\n", regpath);
        }
    }
#endif

#ifndef STANDALONE
    clientlogf("Using home directory: %s", tmpdir);
#endif

    if(fixpackagedir(tmpdir) > 0)
    {
        copystring(homedir, tmpdir);
        createdir(homedir);
    }
}

void addpackagedir(const char *dir)
{
#ifndef STANDALONE
    clientlogf("Adding package directory: %s", dir);
#endif

    string pdir;
    copystring(pdir, dir);
    if(fixpackagedir(pdir) > 0) packagedirs.add(newstring(pdir));
}

const char *findfile(const char *filename, const char *mode)
{
    static string s;
    if(homedir[0])
    {
        formatstring(s)("%s%s", homedir, filename);
        if(fileexists(s, mode)) return s;
        if(mode[0]=='w' || mode[0]=='a')
        {
            string dirs;
            copystring(dirs, s);
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
        formatstring(s)("%s%s", packagedirs[i], filename);
        if(fileexists(s, mode)) return s;
    }
    return filename;
}

bool listdir(const char *dir, const char *ext, vector<char *> &files)
{
    int extsize = ext ? (int)strlen(ext)+1 : 0;
    #if defined(WIN32)
    defformatstring(pathname)("%s\\*.%s", dir, ext ? ext : "*");
    WIN32_FIND_DATA FindFileData;
    HANDLE Find = FindFirstFile(path(pathname), &FindFileData);
    if(Find != INVALID_HANDLE_VALUE)
    {
        do {
            files.add(newstring(FindFileData.cFileName, (int)strlen(FindFileData.cFileName) - extsize));
        } while(FindNextFile(Find, &FindFileData));
        FindClose(Find);
        return true;
    }
    #else
    string pathname;
    copystring(pathname, dir);
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
        formatstring(s)("%s%s", homedir, dir);
        if(listdir(s, ext, files)) dirs++;
    }
    loopv(packagedirs)
    {
        formatstring(s)("%s%s", packagedirs[i], dir);
        if(listdir(s, ext, files)) dirs++;
    }
#ifndef STANDALONE
    dirs += listzipfiles(dir, ext, files);
#endif
    return dirs;
}

bool delfile(const char *path)
{
    return !remove(path);
}

bool copyfile(const char *source, const char *destination)
{
    FILE *from = fopen(source, "rb");
    FILE *dest = fopen(destination, "wb");

    if(!from || !dest) return false;
    size_t len;
    uchar buf[1024];
    while((len = fread(&buf, sizeof(uchar), 1024, from)))
    {
        fwrite(&buf, sizeof(uchar), len, dest);
    }
    fclose(from);
    fclose(dest);
    return true;
}

bool preparedir(const char *destination)
{
    string dir;
    copystring(dir, parentdir(destination));
    vector<char *> dirs;
    while(!fileexists(dir, "r"))
    {
        dirs.add(newstring(dir));
        copystring(dir, parentdir(dir));
    }
    
    loopvrev(dirs) if(!createdir(dirs[i])) return false;
    return true;
}

#ifndef STANDALONE
static int rwopsseek(SDL_RWops *rw, int offset, int whence)
{
    stream *f = (stream *)rw->hidden.unknown.data1;
    if((!offset && whence==SEEK_CUR) || f->seek(offset, whence)) return f->tell();
    return -1;
}

static int rwopsread(SDL_RWops *rw, void *buf, int size, int nmemb)
{
    stream *f = (stream *)rw->hidden.unknown.data1;
    return f->read(buf, size*nmemb)/size;
}

static int rwopswrite(SDL_RWops *rw, const void *buf, int size, int nmemb)
{
    stream *f = (stream *)rw->hidden.unknown.data1;
    return f->write(buf, size*nmemb)/size;
}

static int rwopsclose(SDL_RWops *rw)
{
    return 0;
}

SDL_RWops *stream::rwops()
{
    SDL_RWops *rw = SDL_AllocRW();
    if(!rw) return NULL;
    rw->hidden.unknown.data1 = this;
    rw->seek = rwopsseek;
    rw->read = rwopsread;
    rw->write = rwopswrite;
    rw->close = rwopsclose;
    return rw;
}
#endif

long stream::size()
{
    long pos = tell(), endpos;
    if(pos < 0 || !seek(0, SEEK_END)) return -1;
    endpos = tell();
    return pos == endpos || seek(pos, SEEK_SET) ? endpos : -1;
}

bool stream::getline(char *str, int len)
{
    loopi(len-1)
    {
        if(read(&str[i], 1) != 1) { str[i] = '\0'; return i > 0; }
        else if(str[i] == '\n') { str[i+1] = '\0'; return true; }
    }
    if(len > 0) str[len-1] = '\0';
    return true;
}

#ifndef WIN32
#include <sys/statvfs.h>
const int64_t MINFSSIZE = 50000000;         // 50MB
#endif

struct filestream : stream
{
    FILE *file;

    filestream() : file(NULL) {}
    ~filestream() { close(); }

    bool open(const char *name, const char *mode)
    {
        if(file) return false;
        file = fopen(name, mode);
#ifndef WIN32
        struct statvfs buf;
        if(file && strchr(mode,'w'))
        {
            int fail = fstatvfs(fileno(file), &buf);
            if (fail || (int64_t)buf.f_frsize * (int64_t)buf.f_bavail < MINFSSIZE)
            {
                close();
                return false;
            }
        }
#endif
        return file!=NULL;
    }

    bool opentemp(const char *name, const char *mode)
    {
        if(file) return false;
#ifdef WIN32
        file = fopen(name, mode);
#else
        file = tmpfile();
#endif
        return file!=NULL;
    }

    void close()
    {
        if(file) { fclose(file); file = NULL; }
    }

    bool end() { return feof(file)!=0; }
    long tell() { return ftell(file); }
    bool seek(long offset, int whence) { return fseek(file, offset, whence) >= 0; }
    int read(void *buf, int len) { return (int)fread(buf, 1, len, file); }
    int write(const void *buf, int len) { return (int)fwrite(buf, 1, len, file); }
    int getchar() { return fgetc(file); }
    bool putchar(int c) { return fputc(c, file)!=EOF; }
    bool getline(char *str, int len) { return fgets(str, len, file)!=NULL; }
    bool putstring(const char *str) { return fputs(str, file)!=EOF; }

    int printf(const char *fmt, ...)
    {
        va_list v;
        va_start(v, fmt);
        int result = vfprintf(file, fmt, v);
        va_end(v);
        return result;
    }
};

#ifndef STANDALONE
//VAR(dbggz, 0, 0, 1);
const int dbggz = 0;
#endif

struct gzstream : stream
{
    enum
    {
        MAGIC1   = 0x1F,
        MAGIC2   = 0x8B,
        BUFSIZE  = 16384,
        OS_UNIX  = 0x03
    };

    enum
    {
        F_ASCII    = 0x01,
        F_CRC      = 0x02,
        F_EXTRA    = 0x04,
        F_NAME     = 0x08,
        F_COMMENT  = 0x10,
        F_RESERVED = 0xE0
    };

    stream *file;
    z_stream zfile;
    uchar *buf;
    bool reading, writing, autoclose;
    uint crc;
    int headersize;

    gzstream() : file(NULL), buf(NULL), reading(false), writing(false), autoclose(false), crc(0), headersize(0)
    {
        zfile.zalloc = NULL;
        zfile.zfree = NULL;
        zfile.opaque = NULL;
        zfile.next_in = zfile.next_out = NULL;
        zfile.avail_in = zfile.avail_out = 0;
    }

    ~gzstream()
    {
        close();
    }

    void writeheader()
    {
        uchar header[] = { MAGIC1, MAGIC2, Z_DEFLATED, 0, 0, 0, 0, 0, 0, OS_UNIX };
        file->write(header, sizeof(header));
    }

    void readbuf(int size = BUFSIZE)
    {
        if(!zfile.avail_in) zfile.next_in = (Bytef *)buf;
        size = min(size, int(&buf[BUFSIZE] - &zfile.next_in[zfile.avail_in]));
        int n = file->read(zfile.next_in + zfile.avail_in, size);
        if(n > 0) zfile.avail_in += n;
    }

    int readbyte(int size = BUFSIZE)
    {
        if(!zfile.avail_in) readbuf(size);
        if(!zfile.avail_in) return 0;
        zfile.avail_in--;
        return *(uchar *)zfile.next_in++;
    }

    void skipbytes(int n)
    {
        while(n > 0 && zfile.avail_in > 0)
        {
            int skipped = min(n, (int)zfile.avail_in);
            zfile.avail_in -= skipped;
            zfile.next_in += skipped;
            n -= skipped;
        }
        if(n <= 0) return;
        file->seek(n, SEEK_CUR);
    }

    bool checkheader()
    {
        readbuf(10);
        if(readbyte() != MAGIC1 || readbyte() != MAGIC2 || readbyte() != Z_DEFLATED) return false;
        int flags = readbyte();
        if(flags & F_RESERVED) return false;
        skipbytes(6);
        if(flags & F_EXTRA)
        {
            int len = readbyte(512);
            len |= readbyte(512)<<8;
            skipbytes(len);
        }
        if(flags & F_NAME) while(readbyte(512));
        if(flags & F_COMMENT) while(readbyte(512));
        if(flags & F_CRC) skipbytes(2);
        headersize = file->tell() - zfile.avail_in;
        return zfile.avail_in > 0 || !file->end();
    }

    bool open(stream *f, const char *mode, bool needclose, int level)
    {
        if(file) return false;
        for(; *mode; mode++)
        {
            if(*mode=='r') { reading = true; break; }
            else if(*mode=='w') { writing = true; break; }
        }
        if(reading)
        {
            if(inflateInit2(&zfile, -MAX_WBITS) != Z_OK) reading = false;
        }
        else if(writing && deflateInit2(&zfile, level, Z_DEFLATED, -MAX_WBITS, min(MAX_MEM_LEVEL, 8), Z_DEFAULT_STRATEGY) != Z_OK) writing = false;
        if(!reading && !writing) return false;

        autoclose = needclose;
        file = f;
        crc = crc32(0, NULL, 0);
        buf = new uchar[BUFSIZE];

        if(reading)
        {
            if(!checkheader()) { stopreading(); return false; }
        }
        else if(writing) writeheader();
        return true;
    }

    uint getcrc() { return crc; }

    void finishreading()
    {
        if(!reading) return;
#ifndef STANDALONE
        if(dbggz)
        {
            uint checkcrc = 0, checksize = 0;
            loopi(4) checkcrc |= uint(readbyte()) << (i*8);
            loopi(4) checksize |= uint(readbyte()) << (i*8);
            if(checkcrc != crc)
                conoutf("gzip crc check failed: read %X, calculated %X", checkcrc, crc);
            if(checksize != zfile.total_out)
                conoutf("gzip size check failed: read %d, calculated %d", checksize, zfile.total_out);
        }
#endif
    }

    void stopreading()
    {
        if(!reading) return;
        inflateEnd(&zfile);
        reading = false;
    }

    void finishwriting()
    {
        if(!writing) return;
        for(;;)
        {
            int err = zfile.avail_out > 0 ? deflate(&zfile, Z_FINISH) : Z_OK;
            if(err != Z_OK && err != Z_STREAM_END) break;
            flush();
            if(err == Z_STREAM_END) break;
        }
        uchar trailer[8] =
        {
            uchar(crc&0xFF), uchar((crc>>8)&0xFF), uchar((crc>>16)&0xFF), uchar((crc>>24)&0xFF),
            uchar(zfile.total_in&0xFF), uchar((zfile.total_in>>8)&0xFF), uchar((zfile.total_in>>16)&0xFF), uchar((zfile.total_in>>24)&0xFF)
        };
        file->write(trailer, sizeof(trailer));
    }

    void stopwriting()
    {
        if(!writing) return;
        deflateEnd(&zfile);
        writing = false;
    }

    void close()
    {
        if(reading) finishreading();
        stopreading();
        if(writing) finishwriting();
        stopwriting();
        DELETEA(buf);
        if(autoclose) DELETEP(file);
    }

    bool end() { return !reading && !writing; }
    long tell() { return reading ? zfile.total_out : (writing ? zfile.total_in : -1); }

    bool seek(long offset, int whence)
    {
        if(writing || !reading) return false;

        if(whence == SEEK_END)
        {
            uchar skip[512];
            while(read(skip, sizeof(skip)) == sizeof(skip));
            return !offset;
        }
        else if(whence == SEEK_CUR) offset += zfile.total_out;

        if(offset >= (int)zfile.total_out) offset -= zfile.total_out;
        else if(offset < 0 || !file->seek(headersize, SEEK_SET)) return false;
        else
        {
            if(zfile.next_in && zfile.total_in <= uint(zfile.next_in - buf))
            {
                zfile.avail_in += zfile.total_in;
                zfile.next_in -= zfile.total_in;
            }
            else
            {
                zfile.avail_in = 0;
                zfile.next_in = NULL;
            }
            inflateReset(&zfile);
            crc = crc32(0, NULL, 0);
        }

        uchar skip[512];
        while(offset > 0)
        {
            int skipped = min(offset, (long)sizeof(skip));
            if(read(skip, skipped) != skipped) { stopreading(); return false; }
            offset -= skipped;
        }

        return true;
    }

    int read(void *buf, int len)
    {
        if(!reading || !buf || !len) return 0;
        zfile.next_out = (Bytef *)buf;
        zfile.avail_out = len;
        while(zfile.avail_out > 0)
        {
            if(!zfile.avail_in)
            {
                readbuf(BUFSIZE);
                if(!zfile.avail_in) { stopreading(); break; }
            }
            int err = inflate(&zfile, Z_NO_FLUSH);
            if(err == Z_STREAM_END) { crc = crc32(crc, (Bytef *)buf, len - zfile.avail_out); finishreading(); stopreading(); return len - zfile.avail_out; }
            else if(err != Z_OK) { stopreading(); break; }
        }
        crc = crc32(crc, (Bytef *)buf, len - zfile.avail_out);
        return len - zfile.avail_out;
    }

    bool flush()
    {
        if(zfile.next_out && zfile.avail_out < BUFSIZE)
        {
            if(file->write(buf, BUFSIZE - zfile.avail_out) != int(BUFSIZE - zfile.avail_out))
                return false;
        }
        zfile.next_out = buf;
        zfile.avail_out = BUFSIZE;
        return true;
    }

    int write(const void *buf, int len)
    {
        if(!writing || !buf || !len) return 0;
        zfile.next_in = (Bytef *)buf;
        zfile.avail_in = len;
        while(zfile.avail_in > 0)
        {
            if(!zfile.avail_out && !flush()) { stopwriting(); break; }
            int err = deflate(&zfile, Z_NO_FLUSH);
            if(err != Z_OK) { stopwriting(); break; }
        }
        crc = crc32(crc, (Bytef *)buf, len - zfile.avail_in);
        return len - zfile.avail_in;
    }
};


stream *openrawfile(const char *filename, const char *mode)
{
    const char *found = findfile(filename, mode);
#ifndef STANDALONE
    if(mode && (mode[0]=='w' || mode[0]=='a')) conoutf("writing to file: %s", found);
#endif
    if(!found) return NULL;
    filestream *file = new filestream;
    if(!file->open(found, mode))
    {
#ifndef STANDALONE
//         conoutf("file failure! %s",filename);
#endif
        delete file; return NULL;
    }
    return file;
}

stream *openfile(const char *filename, const char *mode)
{
#ifndef STANDALONE
    stream *s = openzipfile(filename, mode);
    if(s) return s;
#endif
    return openrawfile(filename, mode);
}

int getfilesize(const char *filename)
{
    stream *f = openfile(filename, "rb");
    if(!f) return -1;
    int len = f->size();
    delete f;
    return len;
}

stream *opentempfile(const char *name, const char *mode)
{
    const char *found = findfile(name, mode);
    filestream *file = new filestream;
    if(!file->opentemp(found ? found : name, mode)) { delete file; return NULL; }
    return file;
}

stream *opengzfile(const char *filename, const char *mode, stream *file, int level)
{
    stream *source = file ? file : openfile(filename, mode);
    if(!source) return NULL;
    gzstream *gz = new gzstream;
    if(!gz->open(source, mode, !file, level)) { if(!file) delete source; return NULL; }
    return gz;
}

char *loadfile(const char *fn, int *size, const char *mode)
{
    stream *f = openfile(fn, mode ? mode : "rb");
    if(!f) return NULL;
    int len = f->size();
    if(len<=0) { delete f; return NULL; }
    char *buf = new char[len+1];
    if(!buf) { delete f; return NULL; }
    buf[len] = 0;
    int rlen = f->read(buf, len);
    delete f;
    if(len!=rlen && (!mode || strchr(mode, 'b')))
    {
        delete[] buf;
        return NULL;
    }
    if(size!=NULL) *size = len;
    return buf;
}

