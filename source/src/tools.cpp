// implementation of generic tools

#include "cube.h"

//////////////////////////// pool ///////////////////////////

///////////////////////// misc tools ///////////////////////

char *path(char *s)
{
    for(char *t = s; t = strpbrk(t, "/\\"); *t++ = PATHDIV);
    return s;
};

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
    };
    if(size!=NULL) *size = len;
    return buf;
};

void endianswap(void *memory, int stride, int length)   // little endian as storage format
{
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    loop(w, length) loop(i, stride/2)
    {
        uchar *p = (uchar *)memory+w*stride;
        uchar t = p[i];
        p[i] = p[stride-i-1];
        p[stride-i-1] = t;
    };
#endif
}
