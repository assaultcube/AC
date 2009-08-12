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
#define ASSERT(c) if(!(c)) { asm("int $3"); }
#else
#define ASSERT(c) if(!(c)) { __asm int 3 }
#endif

#include <iostream>
#define DEBUG(v) if(DEBUGCOND) { std::cout << behindpath(__FILE__) << ":" << __LINE__ << " " << __FUNCTION__ << "(..) " << v << std::endl; }
#define DEBUGVAR(v) if(DEBUGCOND) { std::cout << behindpath(__FILE__) << ":" << __LINE__ << " " << __FUNCTION__ << "(..) " << #v << " = " << v << std::endl; }
#else
#define DEBUG(v) {}
#define DEBUGVAR(v) {}
#define ASSERT(c) if(c) {}
#endif

#ifdef swap
#undef swap
#endif
template<class T>
static inline void swap(T &a, T &b)
{
    T t = a;
    a = b;
    b = t;
}
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
template<class T>
static inline T max(T a, T b)
{
    return a > b ? a : b;
}
template<class T>
static inline T min(T a, T b)
{
    return a < b ? a : b;
}

static inline float round(float x) { return floor(x + 0.5f); }

#define clamp(a,b,c) (max(b, min(a, c)))
#define rnd(max) (rand()%(max))
#define detrnd(s, x) ((int)(((((uint)(s))*1103515245+12345)>>16)%(x)))

#define loop(v,m) for(int v = 0; v<int(m); v++)
#define loopi(m) loop(i,m)
#define loopj(m) loop(j,m)
#define loopk(m) loop(k,m)
#define loopl(m) loop(l,m)


#define DELETEP(p) if(p) { delete   p; p = 0; }
#define DELETEA(p) if(p) { delete[] p; p = 0; }

#define PI  (3.1415927f)
#define PI2 (2*PI)
#define SQRT2 (1.4142136f)
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

#define strcasecmp(a,b) _stricmp(a,b)
#define strncasecmp(a,b,n) _strnicmp(a,b,n)
#define PATHDIV '\\'
#else
#define __cdecl
#define _vsnprintf vsnprintf
#define PATHDIV '/'
#endif

// easy safe strings

#define _MAXDEFSTR 260
typedef char string[_MAXDEFSTR];

inline void formatstring(char *d, const char *fmt, va_list v) { _vsnprintf(d, _MAXDEFSTR, fmt, v); d[_MAXDEFSTR-1] = 0; }
inline char *s_strncpy(char *d, const char *s, size_t m) { strncpy(d,s,m); d[m-1] = 0; return d; }
inline char *s_strcpy(char *d, const char *s) { return s_strncpy(d,s,_MAXDEFSTR); }
inline char *s_strcat(char *d, const char *s) { size_t n = strlen(d); return s_strncpy(d+n,s,_MAXDEFSTR-n); }
extern char *s_strcatf(char *d, const char *s, ...);

struct s_sprintf_f
{
    char *d;
    s_sprintf_f(char *str) : d(str) {}
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

#define loopv(v)    if(false) {} else for(int i = 0; i<(v).length(); i++)
#define loopvj(v)   if(false) {} else for(int j = 0; j<(v).length(); j++)
#define loopvk(v)   if(false) {} else for(int k = 0; k<(v).length(); k++)
#define loopvrev(v) if(false) {} else for(int i = (v).length()-1; i>=0; i--)
#define loopvjrev(v) if(false) {} else for(int j = (v).length()-1; i>=0; i--)

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
    databuf(T *buf, U maxlen) : buf(buf), len(0), maxlen((int)maxlen), flags(0) {}

    const T &get()
    {
        static T overreadval;
        if(len<maxlen) return buf[len++];
        flags |= OVERREAD;
        return overreadval;
    }

    databuf subbuf(int sz)
    {
        sz = min(sz, maxlen-len);
        len += sz;
        return databuf(&buf[len-sz], sz);
    }

    void put(const T &val)
    {
        if(len<maxlen) buf[len++] = val;
        else flags |= OVERWROTE;
    }

    void put(const T *vals, int numvals)
    {
        if(maxlen-len<numvals) flags |= OVERWROTE;
        memcpy(&buf[len], vals, min(maxlen-len, numvals)*sizeof(T));
        len += min(maxlen-len, numvals);
    }

    int length() const { return len; }
    int remaining() const { return maxlen-len; }
    bool overread() const { return flags&OVERREAD; }
    bool overwrote() const { return flags&OVERWROTE; }

    void forceoverread()
    {
        len = maxlen;
        flags |= OVERREAD;
    }
};

typedef databuf<char> charbuf;
typedef databuf<uchar> ucharbuf;

struct bitbuf
{
    ucharbuf *q;
    int blen;

    bitbuf(ucharbuf *oq) : q(oq), blen(0) {}

    int getbits(int n)
    {
        int res = 0, p = 0;
        while(n > 0)
        {
            if(!blen) q->get();
            int r = 8 - blen;
            if(r > n) r = n;
            res |= ((q->buf[q->len - 1] >> blen) & ((1 << r) - 1)) << p;
            n -= r; p += r;
            blen = (blen + r) % 8;
        }
        return res;
    }

    void putbits(int n, int v)
    {
        while(n > 0)
        {
            if(!blen) { q->put(0); if(q->overwrote()) return; }
            int r = 8 - blen;
            if(r > n) r = n;
            q->buf[q->len - 1] |= (v & ((1 << r) - 1)) << blen;
            n -= r; v >>= r;
            blen = (blen + r) % 8;
        }
    }

    int rembits() { return (8 - blen) % 8; }   // 0..7 bits remaining in current byte
};

template <class T> struct vector
{
   static const int MINSIZE = 8;

    T *buf;
    int alen, ulen;

    vector() : buf(NULL), alen(0), ulen(0)
    {
    }

    vector(const vector &v) : buf(NULL), alen(0), ulen(0)
    {
        *this = v;
    }

    ~vector() { setsize(0); if(buf) delete[] (uchar *)buf; }

    vector<T> &operator=(const vector<T> &v)
    {
        setsize(0);
        if(v.length() > alen) vrealloc(v.length());
        loopv(v) add(v[i]);
        return *this;
    }

    T &add(const T &x)
    {
        if(ulen==alen) vrealloc(ulen+1);
        new (&buf[ulen]) T(x);
        return buf[ulen++];
    }

    T &add()
    {
        if(ulen==alen) vrealloc(ulen+1);
        new (&buf[ulen]) T;
        return buf[ulen++];
    }

    T &dup()
    {
        if(ulen==alen) vrealloc(ulen+1);
        new (&buf[ulen]) T(buf[ulen-1]);
        return buf[ulen++];
    }

    bool inrange(size_t i) const { return i<size_t(ulen); }
    bool inrange(int i) const { return i>=0 && i<ulen; }

    T &pop() { return buf[--ulen]; }
    T &last() { return buf[ulen-1]; }
    void drop() { buf[--ulen].~T(); }
    bool empty() const { return ulen==0; }

    int length() const { return ulen; }
    T &operator[](int i) { ASSERT(i>=0 && i<ulen); return buf[i]; }
    const T &operator[](int i) const { ASSERT(i >= 0 && i<ulen); return buf[i]; }

    void setsize(int i)         { ASSERT(i<=ulen); while(ulen>i) drop(); }
    void setsizenodelete(int i) { ASSERT(i<=ulen); ulen = i; }

    void deletecontentsp() { while(!empty()) delete   pop(); }
    void deletecontentsa() { while(!empty()) delete[] pop(); }

    T *getbuf() { return buf; }
    const T *getbuf() const { return buf; }
    bool inbuf(const T *e) const { return e >= buf && e < &buf[ulen]; }

    template<class ST>
    void sort(int (__cdecl *cf)(ST *, ST *), int i = 0, int n = -1)
    {
        qsort(&buf[i], n<0 ? ulen : n, sizeof(T), (int (__cdecl *)(const void *,const void *))cf);
    }

    template<class ST>
    T *search(T *key, int (__cdecl *cf)(ST *, ST *), int i = 0, int n = -1)
    {
        return (T *) bsearch(key, &buf[i], n<0 ? ulen : n, sizeof(T), (int (__cdecl *)(const void *,const void *))cf);
    }

    void vrealloc(int sz)
    {
        int olen = alen;
        if(!alen) alen = max(MINSIZE, sz);
        else while(alen < sz) alen *= 2;
        if(alen <= olen) return;
        uchar *newbuf = new uchar[alen*sizeof(T)];
        if(olen > 0)
        {
            memcpy(newbuf, buf, olen*sizeof(T));
            delete[] (uchar *)buf;
        }
        buf = (T *)newbuf;
    }

    databuf<T> reserve(int sz)
    {
        if(ulen+sz > alen) vrealloc(ulen+sz);
        return databuf<T>(&buf[ulen], sz);
    }

    void addbuf(const databuf<T> &p)
    {
        ulen += p.length();
    }

    void remove(int i, int n)
    {
        for(int p = i+n; p<ulen; p++) buf[p-n] = buf[p];
        ulen -= n;
    }

    T remove(int i)
    {
        T e = buf[i];
        for(int p = i+1; p<ulen; p++) buf[p-1] = buf[p];
        ulen--;
        return e;
    }

    int find(const T &o)
    {
        loopi(ulen) if(buf[i]==o) return i;
        return -1;
    }

    void removeobj(const T &o)
    {
        loopi(ulen) if(buf[i]==o) remove(i--);
    }

    void replacewithlast(const T &o)
    {
        if(!ulen) return;
        loopi(ulen-1) if(buf[i]==o)
        {
            buf[i] = buf[ulen-1];
        }
        ulen--;
    }

    T &insert(int i, const T &e)
    {
        add(T());
        for(int p = ulen-1; p>i; p--) buf[p] = buf[p-1];
        buf[i] = e;
        return buf[i];
    }
};

typedef vector<char *> cvector;
typedef vector<int> ivector;
typedef vector<ushort> usvector;

static inline uint hthash(const char *key)
{
    uint h = 5381;
    for(int i = 0, k; (k = key[i]); i++) h = ((h<<5)+h)^k;    // bernstein k=33 xor
    return h;
}

static inline bool htcmp(const char *x, const char *y)
{
    return !strcmp(x, y);
}

static inline uint hthash(int key)
{
    return key;
}

static inline bool htcmp(int x, int y)
{
    return x==y;
}

static inline uint hthash(uint key)
{
    return key;
}

static inline bool htcmp(uint x, uint y)
{
    return x==y;
}

template <class K, class T> struct hashtable
{
    typedef K key;
    typedef const K const_key;
    typedef T value;
    typedef const T const_value;

    enum { CHUNKSIZE = 16 };

    struct chain      { T data; K key; chain *next; };
    struct chainchunk { chain chains[CHUNKSIZE]; chainchunk *next; };

    int size;
    int numelems;
    chain **table;
    chain *enumc;

    chainchunk *chunks;
    chain *unused;

    hashtable(int size = 1<<10)
      : size(size)
    {
        numelems = 0;
        chunks = NULL;
        unused = NULL;
        table = new chain *[size];
        loopi(size) table[i] = NULL;
    }

    ~hashtable()
    {
        DELETEA(table);
        deletechunks();
    }

    void insertchains(chainchunk *emptychunk)
    {
        loopi(CHUNKSIZE-1) emptychunk->chains[i].next = &emptychunk->chains[i+1];
        emptychunk->chains[CHUNKSIZE-1].next = unused;
        unused = emptychunk->chains;
    }

    chain *insert(const K &key, uint h)
    {
        if(!unused)
        {
            chainchunk *chunk = new chainchunk;
            chunk->next = chunks;
            chunks = chunk;
            insertchains(chunk);
        }
        chain *c = unused;
        unused = unused->next;
        c->key = key;
        c->next = table[h];
        table[h] = c;
        numelems++;
        return c;
    }

    #define HTFIND(success, fail) \
        uint h = hthash(key)&(size-1); \
        for(chain *c = table[h]; c; c = c->next) \
        { \
            if(htcmp(key, c->key)) return (success); \
        } \
        return (fail);

    T *access(const K &key)
    {
        HTFIND(&c->data, NULL);
    }

    T &access(const K &key, const T &data)
    {
        HTFIND(c->data, insert(key, h)->data = data);
    }

    T &operator[](const K &key)
    {
        HTFIND(c->data, insert(key, h)->data);
    }

    #undef HTFIND

    bool remove(const K &key)
    {
        uint h = hthash(key)&(size-1);
        for(chain **p = &table[h], *c = table[h]; c; p = &c->next, c = c->next)
        {
            if(htcmp(key, c->key))
            {
                *p = c->next;
                c->data.~T();
                c->key.~K();
                new (&c->data) T;
                new (&c->key) K;
                c->next = unused;
                unused = c;
                numelems--;
                return true;
            }
        }
        return false;
    }

    void deletechunks()
    {
        for(chainchunk *nextchunk; chunks; chunks = nextchunk)
        {
            nextchunk = chunks->next;
            delete chunks;
        }
    }

    void clear(bool del = true)
    {
        loopi(size) table[i] = NULL;
        numelems = 0;
        unused = NULL;
        if(del) deletechunks();
        else
        {
            for(chainchunk *chunk = chunks; chunk; chunk = chunk->next) insertchains(chunk);
        }
    }
};

template <class T, int SIZE> struct ringbuf
{
    int index, len;
    T data[SIZE];

    ringbuf() { clear(); }

    void clear()
    {
        index = len = 0;
    }

    bool empty() const { return !len; }

    int maxsize() const { return SIZE; }
    int length() const { return len; }

    T &remove()
    {
        int start = index - len;
        if(start < 0) start += SIZE;
        len--;
        return data[start];
    }

    T &add(const T &e)
    {
        T &t = data[index];
        t = e;
        index++;
        if(index>=SIZE) index = 0;
        if(len<SIZE) len++;
        return t;
    }

    T &add() { return add(T()); }

    T &operator[](int i)
    {
        int start = index - len;
        if(start < 0) start += SIZE;
        i += start;
        if(i >= SIZE) i -= SIZE;
        return data[i];
    }

    const T &operator[](int i) const
    {
        int start = index - len;
        if(start < 0) start += SIZE;
        i += start;
        if(i >= SIZE) i -= SIZE;
        return data[i];
    }
};

#define itoa(s, i) sprintf(s, "%d", i)
#define ftoa(s, f) sprintf(s, (f) == int(f) ? "%.1f" : "%.7g", f)

#define enumeratekt(ht,k,e,t,f,b) loopi((ht).size) for(hashtable<k,t>::chain *enumc = (ht).table[i]; enumc;) { hashtable<k,t>::const_key &e = enumc->key; t &f = enumc->data; enumc = enumc->next; b; }
#define enumeratek(ht,k,e,b)      loopi((ht).size) for((ht).enumc = (ht).table[i]; (ht).enumc;) { k &e = (ht).enumc->key; (ht).enumc = (ht).enumc->next; b; }
#define enumerate(ht,t,e,b)       loopi((ht).size) for((ht).enumc = (ht).table[i]; (ht).enumc;) { t &e = (ht).enumc->data; (ht).enumc = (ht).enumc->next; b; }

#ifndef STANDALONE
// ease time measurement
struct stopwatch
{
	int millis;

	stopwatch() : millis(-1) {}

	void start()
	{
		millis = SDL_GetTicks();
	}

	// returns elapsed time
	int stop()
	{
		ASSERT(millis >= 0);
		int time = SDL_GetTicks() - millis;
		millis = -1;
		return time;
	}
};
#endif

inline char *newstring(size_t l)                { return new char[l+1]; }
inline char *newstring(const char *s, size_t l) { return s_strncpy(newstring(l), s, l+1); }
inline char *newstring(const char *s)           { return newstring(s, strlen(s)); }
inline char *newstringbuf()                     { return newstring(_MAXDEFSTR-1); }
inline char *newstringbuf(const char *s)        { return newstring(s, _MAXDEFSTR-1); }

inline const char *_gettext(const char *msgid)
{
	if(msgid && msgid[0] != '\0')
		return gettext(msgid);
	else
		return "";
}

#define _(s) _gettext(s)

extern const char *timestring(bool local = false, const char *fmt = NULL);
extern const char *asctime();
extern const char *numtime();
extern char *path(char *s);
extern char *path(const char *s, bool copy);
extern const char *behindpath(const char *s);
extern const char *parentdir(const char *directory);
extern bool fileexists(const char *path, const char *mode);
extern bool createdir(const char *path);
extern void sethomedir(const char *dir);
extern void addpackagedir(const char *dir);
extern const char *findfile(const char *filename, const char *mode);
extern int getfilesize(const char *filename);
extern FILE *openfile(const char *filename, const char *mode);
extern gzFile opengzfile(const char *filename, const char *mode);
extern char *loadfile(const char *fn, int *size, const char *mode = NULL);
extern bool listdir(const char *dir, const char *ext, vector<char *> &files);
extern int listfiles(const char *dir, const char *ext, vector<char *> &files);
extern bool delfile(const char *path);
extern struct mapstats *loadmapstats(const char *filename, bool getlayout);
extern bool cmpb(void *b, int n, enet_uint32 c);
extern bool cmpf(char *fn, enet_uint32 c);
extern enet_uint32 adler(unsigned char *data, size_t len);
extern void endianswap(void *, int, int);
extern bool isbigendian();
extern void strtoupper(char *t, const char *s = NULL);

struct iprange { enet_uint32 lr, ur; };
extern const char *atoip(const char *s, enet_uint32 *ip);
extern const char *atoipr(const char *s, iprange *ir);
extern const char *iptoa(const enet_uint32 ip);
extern const char *iprtoa(const struct iprange &ipr);
extern int cmpiprange(const struct iprange *a, const struct iprange *b);
extern int cmpipmatch(const struct iprange *a, const struct iprange *b);
extern const char *hiddenpwd(const char *pwd, int showchars = 0);

#if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
extern void stackdumper(unsigned int type, EXCEPTION_POINTERS *ep);
#endif

#endif

