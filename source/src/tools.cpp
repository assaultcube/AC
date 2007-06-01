// implementation of generic tools

#include "cube.h"

//////////////////////////// pool ///////////////////////////

///////////////////////// misc tools ///////////////////////

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

char *parentdir(char *directory)
{
    char *p = strrchr(directory, '/');
    if(!p) p = directory;
    size_t len = p-directory+1;
    char *parent = new char[len];
    s_strncpy(parent, directory, len);
    return parent;
}

char *loadfile(char *fn, int *size)
{
    FILE *f = fopen(fn, "rb");
    if(!f) return NULL;
    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    if(len<=0) { fclose(f); return NULL; };
    fseek(f, 0, SEEK_SET);
    char *buf = new char[len+1];
    if(!buf) { fclose(f); return NULL; };
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
