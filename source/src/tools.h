// generic useful stuff for any C++ program

#ifndef _TOOLS_H
#define _TOOLS_H

#ifdef NULL
#undef NULL
#endif
#define NULL 0

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;

#ifdef _DEBUG
#ifdef __GNUC__
#define ASSERT(c) if(!(c)) { asm("int $3"); };
#else
#define ASSERT(c) if(!(c)) { __asm int 3 };
#endif
#else
#define ASSERT(c) if(c) {};
#endif

#define swap(t,a,b) { t m=a; a=b; b=m; }
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#define rnd(max) (rand()%(max))
#define rndreset() (srand(1))
#define rndtime() { loopi(lastmillis&0xF) rnd(i+1); }

#define loop(v,m) for(int v = 0; v<int(m); v++)
#define loopi(m) loop(i,m)
#define loopj(m) loop(j,m)
#define loopk(m) loop(k,m)
#define loopl(m) loop(l,m)


#define DELETEP(p) if(p) { delete   p; p = 0; };
#define DELETEA(p) if(p) { delete[] p; p = 0; };

#define PI  (3.1415927f)
#define PI2 (2*PI)
#define SQRT3 (1.7320508f)
#define RAD (PI / 180.0f)

#ifdef WIN32
#ifdef M_PI
#undef M_PI
#endif
#define M_PI 3.14159265

#ifndef __GNUC__
#pragma warning (3: 4189)       // local variable is initialized but not referenced
#pragma warning (disable: 4244) // conversion from 'int' to 'float', possible loss of data
#pragma warning (disable: 4355) // 'this' : used in base member initializer list
#pragma warning (disable: 4996) // 'strncpy' was declared deprecated
#endif

#define PATHDIV '\\'
#else
#define __cdecl
#define _vsnprintf vsnprintf
#define PATHDIV '/'
#endif

// easy safe strings

#define _MAXDEFSTR 260
typedef char string[_MAXDEFSTR];

inline void formatstring(char *d, const char *fmt, va_list v) { _vsnprintf(d, _MAXDEFSTR, fmt, v); d[_MAXDEFSTR-1] = 0; };
inline char *s_strncpy(char *d, const char *s, size_t m) { strncpy(d,s,m); d[m-1] = 0; return d; };
inline char *s_strcpy(char *d, const char *s) { return s_strncpy(d,s,_MAXDEFSTR); };
inline char *s_strcat(char *d, const char *s) { size_t n = strlen(d); return s_strncpy(d+n,s,_MAXDEFSTR-n); };


struct s_sprintf_f
{
    char *d;
    s_sprintf_f(char *str): d(str) {};
    void operator()(const char* fmt, ...)
    {
        va_list v;
        va_start(v, fmt);
        formatstring(d, fmt, v);
        va_end(v);
    };
};

#define s_sprintf(d) s_sprintf_f((char *)d)
#define s_sprintfd(d) string d; s_sprintf(d)
#define s_sprintfdlv(d,last,fmt) string d; { va_list ap; va_start(ap, last); formatstring(d, fmt, ap); va_end(ap); }
#define s_sprintfdv(d,fmt) s_sprintfdlv(d,fmt,fmt)


template <class T> void _swap(T &a, T &b) { T t = a; a = b; b = t; };



extern char *path(char *s);
extern char *parentdir(char *directory);
extern char *loadfile(char *fn, int *size);
extern void endianswap(void *, int, int);
extern void seedMT(uint seed);
extern uint randomMT(void);

#define loopv(v)    if(false) {} else for(int i = 0; i<(v).length(); i++)
#define loopvj(v)   if(false) {} else for(int j = 0; j<(v).length(); j++)
#define loopvk(v)   if(false) {} else for(int k = 0; k<(v).length(); k++)
#define loopvrev(v) if(false) {} else for(int i = (v).length()-1; i>=0; i--)

template <class T> struct vector
{
    T *buf;
    int alen;
    int ulen;

    vector()
    {
        alen = 8;
        buf = (T *)new uchar[alen*sizeof(T)];
        ulen = 0;
    };
    vector(const vector<T> &v)
    {
        alen = v.length();
        buf = (T *)new uchar[alen*sizeof(T)];
        ulen = 0;
        *this = v;
    };

    ~vector() { setsize(0); delete[] (uchar *)buf; };

    vector<T> &operator=(const vector<T> &v)
    {
        setsize(0);
        loopv(v) add(v[i]);
        return *this;
    };

    T &add(const T &x)
    {
        if(ulen==alen) vrealloc();
        new (&buf[ulen]) T(x);
        return buf[ulen++];
    };

    T &add()
    {
        if(ulen==alen) vrealloc();
        new (&buf[ulen]) T;
        return buf[ulen++];
    };

    bool inrange(size_t i) const { return i<size_t(ulen); };
    bool inrange(int i) const { return i>=0 && i<ulen; };

    T &pop() { return buf[--ulen]; };
    T &last() { return buf[ulen-1]; };
    void drop() { buf[--ulen].~T(); };
    bool empty() const { return ulen==0; };

    int length() const { return ulen; };
    T &operator[](int i) { ASSERT(i>=0 && i<ulen); return buf[i]; };
    const T &operator[](int i) const { ASSERT(i >= 0 && i<ulen); return buf[i]; };
    
    void setsize(int i)         { ASSERT(i<=ulen); while(ulen>i) drop(); };
    void setsizenodelete(int i) { ASSERT(i<=ulen); ulen = i; };
    
    void deletecontentsp() { while(!empty()) delete   pop(); };
    void deletecontentsa() { while(!empty()) delete[] pop(); };
    
    T *getbuf() { return buf; };
    const T *getbuf() const { return buf; };

    template<class ST>
    void sort(int (__cdecl *cf)(ST *, ST *), int i = 0, int n = -1) { qsort(&buf[i], n<0?ulen:n, sizeof(T), (int (__cdecl *)(const void *,const void *))cf); };

    void *_realloc(void *p, int oldsize, int newsize)
    {
        void *np = new uchar[newsize];
        if(!oldsize) return np;
        memcpy(np, p, newsize>oldsize ? oldsize : newsize);
        delete[] (char *)p;
        return np;
    };
    
    void vrealloc()
    {
        int olen = alen;
        buf = (T *)_realloc(buf, olen*sizeof(T), (alen *= 2)*sizeof(T));
    };

    void remove(int i, int n)
    {
        for(int p = i+n; p<ulen; p++) buf[p-n] = buf[p];
        ulen -= n;
    };

    T remove(int i)
    {
        T e = buf[i];
        for(int p = i+1; p<ulen; p++) buf[p-1] = buf[p];
        ulen--;
        return e;
    };

    int find(const T &o)
    {
        loopi(ulen) if(buf[i]==o) return i;
        return -1;
    }
    
    void removeobj(const T &o)
    {
        loopi(ulen) if(buf[i]==o) remove(i--);
    };

    T &insert(int i, const T &e)
    {
        add(T());
        for(int p = ulen-1; p>i; p--) buf[p] = buf[p-1];
        buf[i] = e;
        return buf[i];
    };
};

typedef vector<char *> cvector;
typedef vector<int> ivector;
typedef vector<ushort> usvector;

static inline uint hthash(const char *key)
{
    uint h = 5381;
    for(int i = 0, k; (k = key[i]); i++) h = ((h<<5)+h)^k;    // bernstein k=33 xor
    return h;
};

static inline bool htcmp(const char *x, const char *y)
{
    return !strcmp(x, y);
};

static inline uint hthash(int key)
{   
    return key;
};

static inline bool htcmp(int x, int y)
{
    return x==y;
};

static inline uint hthash(uint key)
{
    return key;
};

static inline bool htcmp(uint x, uint y)
{
    return x==y;
};

template <class K, class T> struct hashtable
{
    typedef K key;
    typedef const K const_key;
    typedef T value;
    typedef const T const_value;

    enum { CHUNKSIZE = 16 };

    struct chain      { T data; K key;    chain      *next; };
    struct chainchunk { chain chunks[CHUNKSIZE]; chainchunk *next; };

    int size;
    int numelems;
    chain **table;
    chain *enumc;

    int chunkremain;
    chainchunk *lastchunk;

    hashtable(int size = 1<<10)
      : size(size)
    {
        numelems = 0;
        chunkremain = 0;
        lastchunk = NULL;
        table = new chain *[size];
        loopi(size) table[i] = NULL;
    };

    ~hashtable()
    {
        DELETEA(table);
    };

    chain *insert(const K &key, uint h)
    {
        if(!chunkremain)
        {
            chainchunk *chunk = new chainchunk;
            chunk->next = lastchunk;
            lastchunk = chunk;
            chunkremain = CHUNKSIZE;
        }
        chain *c = &lastchunk->chunks[--chunkremain];
        c->key = key;
        c->next = table[h]; 
        table[h] = c;
        numelems++;
        return c;
    };

    chain *find(const K &key, bool doinsert)
    {
        uint h = hthash(key)&(size-1);
        for(chain *c = table[h]; c; c = c->next)
        {
            if(htcmp(key, c->key)) return c;
        }
        if(doinsert) return insert(key, h);
        return NULL;
    };

    T *access(const K &key, const T *data = NULL)
    {
        chain *c = find(key, data != NULL);
        if(data) c->data = *data;
        if(c) return &c->data;
        return NULL;
    };

    T &operator[](const K &key)
    {
        return find(key, true)->data;
    };

    void clear()
    {
        loopi(size)
        {
            /*
            for(chain *c = table[i], *next; c; c = next) 
            { 
                next = c->next; 
                delete c; 
            };*/
            table[i] = NULL;
        };
        numelems = 0;
        chunkremain = 0;
        for(chainchunk *chunk; lastchunk; lastchunk = chunk)
        {
            chunk = lastchunk->next;
            delete lastchunk;
        }
    };
};

#define enumeratekt(ht,k,e,t,f,b) loopi((ht).size)  for(hashtable<k,t>::chain *enumc = (ht).table[i]; enumc; enumc = enumc->next) { hashtable<k,t>::const_key &e = enumc->key; t &f = enumc->data; b; }
#define enumerate(ht,t,e,b)       loopi((ht).size) for((ht).enumc = (ht).table[i]; (ht).enumc; (ht).enumc = (ht).enumc->next) { t &e = (ht).enumc->data; b; }

inline char *newstring(size_t l)                { return new char[l+1]; };
inline char *newstring(const char *s, size_t l) { return s_strncpy(newstring(l), s, l+1); };
inline char *newstring(const char *s)           { return newstring(s, strlen(s));          };
inline char *newstringbuf(const char *s)        { return newstring(s, _MAXDEFSTR-1);       };

template <class T>
struct databuf
{
    enum
    {
        OVERREAD  = 1<<0,
        OVERWROTE = 1<<1
    };

    T *buf;
    int len, maxlen;
    uchar flags;
	
	template <class U>
    databuf(T *buf, U maxlen) : buf(buf), len(0), maxlen((int)maxlen), flags(0) {};

    const T &get()
    {
        static T overreadval;
        if(len<maxlen) return buf[len++];
        flags |= OVERREAD;
        return overreadval;
    };

    void put(const T &val)
    {
        if(len<maxlen) buf[len++] = val;
        else flags |= OVERWROTE;
    };

    void put(const T *vals, int numvals)
    {
        if(maxlen-len<numvals) flags |= OVERWROTE;
        memcpy(&buf[len], vals, min(maxlen-len, numvals)*sizeof(T));
        len += min(maxlen-len, numvals);
    };

    int length() const { return len; };
    int remaining() const { return maxlen-len; };
    bool overread() const { return flags&OVERREAD; };
    bool overwrote() const { return flags&OVERWROTE; };

    void forceoverread()
    {
        len = maxlen;
        flags |= OVERREAD;
    };
};

typedef databuf<char> charbuf;
typedef databuf<uchar> ucharbuf;

#ifndef __GNUC__
#ifdef _DEBUG
//#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
inline void *__cdecl operator new(size_t n, const char *fn, int l) { return ::operator new(n, 1, fn, l); };
inline void __cdecl operator delete(void *p, const char *fn, int l) { ::operator delete(p, 1, fn, l); };
#define new new(__FILE__,__LINE__)
#endif 
#endif


#endif

