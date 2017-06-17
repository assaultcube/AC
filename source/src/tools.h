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
#define ASSERT(c) if(!(c)) { fflush(NULL); asm("int $3"); }
#else
#define ASSERT(c) if(!(c)) { fflush(NULL); { __asm int 3 } }
#endif

#include <iostream>
#define DEBUG(v) if(DEBUGCOND) { std::cout << behindpath(__FILE__) << ":" << __LINE__ << " " << __FUNCTION__ << "(..) " << v << std::endl; }
#define DEBUGVAR(v) if(DEBUGCOND) { std::cout << behindpath(__FILE__) << ":" << __LINE__ << " " << __FUNCTION__ << "(..) " << #v << " = " << v << std::endl; }
#define DEBUGCODE(v) v
#else
#define DEBUG(v) {}
#define DEBUGVAR(v) {}
#define DEBUGCODE(v)
#define ASSERT(c) if(c) {}
#endif

#ifdef NO_POSIX_R
extern char *strtok_r(char *s, const char *delim, char **b); // homebrew
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
template <typename T> inline T pow2(T x) { return x*x; }
inline int iabs(int n) { return labs(n); }

#define clamp(x,minval,maxval) (max(minval, min(x, maxval)))
#define rnd(x) ((int)(randomMT()&0xFFFFFF)%(x))
#define rndscale(x) (float((randomMT()&0xFFFFFF)*double(x)/double(0xFFFFFF)))
#define detrnd(s, x) ((int)(((((uint)(s))*1103515245+12345)>>16)%(x)))

#define loop(v,m) for(int v = 0; v<int(m); v++)
#define loopi(m) loop(i,m)
#define loopj(m) loop(j,m)
#define loopk(m) loop(k,m)
#define loopl(m) loop(l,m)
#define loopirev(m) for(int i = int(m) - 1; i >= 0; i--)


#define DELETEP(p) if(p) { delete   p; p = 0; }
#define DELETEA(p) if(p) { delete[] p; p = 0; }

#define PI    (3.141592654f)
#define PI2   (6.283185307f)   /* 2 * PI */
#define SQRT2 (1.414213562f)
#define SQRT3 (1.732050808f)
#define RAD   (0.01745329252f) /* PI / 180 */
#define NEARZERO (0.000245f)   /* epsilon for 1<<LARGEST_FACTOR, use to check if "zero enough" */

#ifdef WIN32
#ifndef __GNUC__
#pragma warning (3: 4189)       // local variable is initialized but not referenced
#pragma warning (disable: 4244) // conversion from 'int' to 'float', possible loss of data
#pragma warning (disable: 4355) // 'this' : used in base member initializer list
#pragma warning (disable: 4996) // 'strncpy' was declared deprecated
#endif

#define strcasecmp(a,b) _stricmp(a,b)
#define strncasecmp(a,b,n) _strnicmp(a,b,n)
#define PATHDIV '\\'
#define PATHDIVS "\\"
#else
#define __cdecl
#define _vsnprintf vsnprintf
#define PATHDIV '/'
#define PATHDIVS "/"
#endif

#ifdef __GNUC__
#define PRINTFARGS(fmt, args) __attribute__((format(printf, fmt, args)))
//#pragma GCC diagnostic ignored "-Wformat-zero-length" // apparently doesn't work from precompiled header -> moved to makefile
#else
#define PRINTFARGS(fmt, args)
#endif

// easy safe strings

#define MAXSTRLEN 260
typedef char string[MAXSTRLEN];

inline void vformatstring(char *d, const char *fmt, va_list v, int len = MAXSTRLEN) { _vsnprintf(d, len, fmt, v); d[len-1] = 0; }
inline char *copystring(char *d, const char *s, size_t len = MAXSTRLEN) { strncpy(d, s, len); d[len-1] = 0; return d; }
inline char *concatstring(char *d, const char *s, size_t len = MAXSTRLEN) { size_t used = strlen(d); return used < len ? copystring(d+used, s, len-used) : d; }
extern char *concatformatstring(char *d, const char *s, ...) PRINTFARGS(2, 3);

struct stringformatter
{
    char *buf;
    stringformatter(char *buf): buf((char *)buf) {}
    void operator()(const char *fmt, ...) PRINTFARGS(2, 3)
    {
        va_list v;
        va_start(v, fmt);
        vformatstring(buf, fmt, v);
        va_end(v);
    }
};

#define formatstring(d) stringformatter((char *)d)
#define defformatstring(d) string d; formatstring(d)
#define defvformatstring(d,last,fmt) string d; { va_list ap; va_start(ap, last); vformatstring(d, fmt, ap); va_end(ap); }

#define MAXMAPNAMELEN 64
inline bool validmapname(const char *s)
{
    int len = strlen(s);
    if(len > MAXMAPNAMELEN) return false;
    if(len == 3 || len == 4)
    {
        char uc[4];
        loopi(3) uc[i] = toupper(s[i]);
        uc[3] = '\0';
        const char *resd = "COMLPTCONPRNAUXNUL", *fnd = strstr(resd, uc);
        if(fnd)
        {
            int pos = (int) (fnd - resd);
            if(pos == 0 || pos == 3)
            {
                if(isdigit(s[3])) return false; // COMx, LPTx
            }
            else if(pos % 3 == 0) return false; // CON, PRN, AUX, NUL
        }
    }
    while(*s != '\0')
    {
        if(!isalnum(*s) && *s != '_' && *s != '-' && *s != '.') return false;
        ++s;
    }
    return true;
}

#define loopv(v)    for(int i = 0; i<(v).length(); i++)
#define loopvj(v)   for(int j = 0; j<(v).length(); j++)
#define loopvk(v)   for(int k = 0; k<(v).length(); k++)
#define loopvrev(v) for(int i = (v).length()-1; i>=0; i--)
#define loopvjrev(v) for(int j = (v).length()-1; j>=0; j--)

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

    databuf() : buf(NULL), len(0), maxlen(0), flags(0) {}

    template <class U>
    databuf(T *buf, U maxlen) : buf(buf), len(0), maxlen((int)maxlen), flags(0) {}

    const T &get()
    {
        static T overreadval = 0;
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

    int get(T *vals, int numvals)
    {
        int read = min(maxlen-len, numvals);
        if(read<numvals) flags |= OVERREAD;
        memcpy(vals, &buf[len], read*sizeof(T));
        len += read;
        return read;
    }

    int length() const { return len; }
    int remaining() const { return maxlen-len; }
    bool overread() const { return (flags&OVERREAD)!=0; }
    bool overwrote() const { return (flags&OVERWROTE)!=0; }

    void forceoverread()
    {
        len = maxlen;
        flags |= OVERREAD;
    }
};

typedef databuf<char> charbuf;
typedef databuf<uchar> ucharbuf;

struct packetbuf : ucharbuf
{
    ENetPacket *packet;
    int growth;

    packetbuf(ENetPacket *packet) : ucharbuf(packet->data, packet->dataLength), packet(packet), growth(0) {}
    packetbuf(int growth, int pflags = 0) : growth(growth)
    {
        packet = enet_packet_create(NULL, growth, pflags);
        buf = (uchar *)packet->data;
        maxlen = packet->dataLength;
    }
    ~packetbuf() { cleanup(); }

    void reliable() { packet->flags |= ENET_PACKET_FLAG_RELIABLE; }

    void resize(int n)
    {
        enet_packet_resize(packet, n);
        buf = (uchar *)packet->data;
        maxlen = packet->dataLength;
    }

    void checkspace(int n)
    {
        if(len + n > maxlen && packet && growth > 0) resize(max(len + n, maxlen + growth));
    }

    ucharbuf subbuf(int sz)
    {
        checkspace(sz);
        return ucharbuf::subbuf(sz);
    }

    void put(const uchar &val)
    {
        checkspace(1);
        ucharbuf::put(val);
    }

    void put(const uchar *vals, int numvals)
    {
        checkspace(numvals);
        ucharbuf::put(vals, numvals);
    }

    bool overwrote() const { return false; }

    ENetPacket *finalize()
    {
        resize(len);
        return packet;
    }

    void cleanup()
    {
        if(growth > 0 && packet && !packet->referenceCount) { enet_packet_destroy(packet); packet = NULL; buf = NULL; len = maxlen = 0; }
    }
};

template<class T>
struct bitbuf
{
    T &q;
    int blen;

    bitbuf(T &oq) : q(oq), blen(0) {}

    int getbits(int n)
    {
        int res = 0, p = 0;
        while(n > 0)
        {
            if(!blen) q.get();
            int r = 8 - blen;
            if(r > n) r = n;
            res |= ((q.buf[q.len - 1] >> blen) & ((1 << r) - 1)) << p;
            n -= r; p += r;
            blen = (blen + r) % 8;
        }
        return res;
    }

    void putbits(int n, int v)
    {
        while(n > 0)
        {
            if(!blen) { q.put(0); if(q.overwrote()) return; }
            int r = 8 - blen;
            if(r > n) r = n;
            q.buf[q.len - 1] |= (v & ((1 << r) - 1)) << blen;
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

    ~vector() { shrink(0); if(buf) delete[] (uchar *)buf; }

    vector<T> &operator=(const vector<T> &v)
    {
        shrink(0);
        if(v.length() > alen) growbuf(v.length());
        loopv(v) add(v[i]);
        return *this;
    }

    T &add(const T &x)
    {
        if(ulen==alen) growbuf(ulen+1);
        new (&buf[ulen]) T(x);
        return buf[ulen++];
    }

    T &add()
    {
        if(ulen==alen) growbuf(ulen+1);
        new (&buf[ulen]) T;
        return buf[ulen++];
    }

    T &dup()
    {
        if(ulen==alen) growbuf(ulen+1);
        new (&buf[ulen]) T(buf[ulen-1]);
        return buf[ulen++];
    }

    bool inrange(size_t i) const { return i<size_t(ulen); }
    bool inrange(int i) const { return i>=0 && i<ulen; }

    T &pop() { ASSERT(ulen > 0); return buf[--ulen]; }
    T &last() { ASSERT(ulen > 0); return buf[ulen-1]; }
    void drop() { ASSERT(ulen > 0); buf[--ulen].~T(); }
    bool empty() const { return ulen==0; }

    int capacity() const { return alen; }
    int length() const { return ulen; }
    T &operator[](int i) { ASSERT(i>=0 && i<ulen); return buf[i]; }
    const T &operator[](int i) const { ASSERT(i >= 0 && i<ulen); return buf[i]; }

    void shrink(int i)         { ASSERT(i>=0 && i<=ulen); while(ulen>i) drop(); }
    void setsize(int i) { ASSERT(i>=0 && i<=ulen); ulen = i; }

    void deletecontents() { while(!empty()) delete   pop(); }
    void deletearrays() { while(!empty()) delete[] pop(); }

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

    void growbuf(int sz)
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
        if(ulen+sz > alen) growbuf(ulen+sz);
        return databuf<T>(&buf[ulen], sz);
    }

    void advance(int sz)
    {
        ulen += sz;
    }

    void addbuf(const databuf<T> &p)
    {
        advance(p.length());
    }

    T *pad(int n)
    {
        T *buf = reserve(n).buf;
        advance(n);
        return buf;
    }

    void put(const T &v) { add(v); }

    void put(const T *v, int n)
    {
        databuf<T> buf = reserve(n);
        buf.put(v, n);
        addbuf(buf);
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

    T *insert(int i, const T *e, int n)
    {
        if(ulen+n>alen) growbuf(ulen+n);
        loopj(n) add(T());
        for(int p = ulen-1; p>=i+n; p--) buf[p] = buf[p-n];
        loopj(n) buf[i+j] = e[j];
        return &buf[i];
    }
};

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

static inline uint hthash(const uchar *key)
{
    return *((uint *)key);
}

static inline bool htcmp(const uchar *x, const uchar *y)  // assume "uchar *" points to 32byte public keys
{
    return !memcmp(x, y, 32);
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
    volatile int in, out;
    T data[SIZE];

    ringbuf() { clear(); }

    void clear()
    {
        in = out = 0;
    }

    bool empty() const { return in == out; }
    bool full() const { return length() >= SIZE - 1; }

    int maxsize() const { return SIZE - 1; } // yes, only SIZE-1, sry
    int length() const { return (SIZE + in - out) % SIZE; }

    T &remove() // get one (crashes, if empty)
    {
        ASSERT(!empty());
        T &res = data[out];
        out = (out + 1) % SIZE;
        return res;
    }

    T *remove(int *n) // get n (reduces n, if the data wraps around or isn't available)
    {
        if(*n > length()) *n = length();
        T *res = data + out;
        if(out + *n >= SIZE) *n = SIZE - out;
        out = (out + *n) % SIZE;
        return res;
    }

    T *peek(int *n) // look at n without removing (reduces n, if the data wraps around or isn't available)
    {
        if(*n > length()) *n = length();
        if(out + *n >= SIZE) *n = SIZE - out;
        return data + out;
    }

    void add(const T &e) // add one (crashes, if full) -- between threads, better use stage & commit
    {
        ASSERT(!full());
        volatile int i;
        data[(i = in)] = e;
        in = (i + 1) % SIZE;
    }

    T &stage(const T &e) // add one without acknowledging it
    {
        return (data[in] = e);
    }

    T *stage() // add one without acknowledging it
    {
        return data + in;
    }

    void stage(const T *e, int n) // add n without acknowledging it
    {
        int n1 = in + n >= SIZE ? SIZE - in : n, n2 = n - n1;
        memcpy(data + in, e, n1 * sizeof(T));
        if(n2) memcpy(data, e + n1, n2 * sizeof(T));
    }

    void commit() // add staged one (crashes, if full)
    {
        ASSERT(!full());
        in = (in + 1) % SIZE;
    }

    void commit(int n) // add staged n (crashes, if full)
    {
        ASSERT(length() + n < SIZE);
        in = (in + n) % SIZE;
    }

    void add(const T *e, int n) // add n
    {
        stage(e, n);
        commit(n);
    }

    T &operator[](int i) // [0] is the one, that remove() will get next (in other words, [0] is the oldest entry, [length()-1] was the last added one, loopv goes from old to new)
    {
        ASSERT(i >= 0 && i < length());
        return data[(out + i) % SIZE];
    }

    const T &operator[](int i) const
    {
        ASSERT(i >= 0 && i < length());
        return data[(out + i) % SIZE];
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
    uint millis;                                // SDL_GetTicks() returns an uint value

    stopwatch() : millis(0) {}

    void start()
    {
        millis = SDL_GetTicks();
    }

    // returns elapsed time
    int elapsed()
    {
        uint time = SDL_GetTicks() - millis;    // subtraction also works in case of timer wraparounds
        return time;
    }
};
#endif

inline char *newstring(size_t l)                { return new char[l+1]; }
inline char *newstring(const char *s, size_t l) { return copystring(newstring(l), s, l+1); }
inline char *newstring(const char *s)           { return newstring(s, strlen(s)); }
inline char *newstringbuf()                     { return newstring(MAXSTRLEN-1); }
inline char *newstringbuf(const char *s)        { return newstring(s, MAXSTRLEN-1); }
inline void delstring(const char *s)            { delete[] (char *)s; }
#define DELSTRING(s) if(s) { delstring(s); s = NULL; }

#ifndef INT_LEAST64_MIN
typedef unsigned long long int uint64_t;
#endif
const int islittleendian = 1;
#ifdef SDL_BYTEORDER
    #define endianswap16 SDL_Swap16
    #define endianswap32 SDL_Swap32
    #define endianswap64 SDL_Swap64
#else
    #if defined(__GNUC__) && /*!defined(__clang__) && */!defined(__ICL) && __GNUC__ >= 4 && !defined(WIN32) && !defined(__APPLE__)
        inline ushort endianswap16(ushort n) { return __builtin_bswap16(n); }
        inline uint endianswap32(uint n) { return __builtin_bswap32(n); }
        inline uint64_t endianswap64(uint64_t n) { return __builtin_bswap64(n); }
    #else
        inline ushort endianswap16(ushort n) { return (n<<8) | (n>>8); }
        inline uint endianswap32(uint n) { return (n<<24) | (n>>24) | ((n>>8)&0xFF00) | ((n<<8)&0xFF0000); }
        inline uint64_t endianswap64(uint64_t n) { return ((uint64_t)endianswap32((uint)n) << 32) | ((uint64_t)endianswap32((uint)(n >> 32))); }
    #endif
#endif
template<class T> inline T endianswap(T n) { union { T t; uchar u[sizeof(T)]; } s, d; s.t = n; loopi(sizeof(T)) d.u[i] = s.u[sizeof(T) - 1 - i]; return d.t; }
template<> inline ushort endianswap<ushort>(ushort n) { return endianswap16(n); }
template<> inline short endianswap<short>(short n) { return endianswap16(n); }
template<> inline uint endianswap<uint>(uint n) { return endianswap32(n); }
template<> inline int endianswap<int>(int n) { return endianswap32(n); }
template<> inline uint64_t endianswap<uint64_t>(uint64_t n) { return endianswap64(n); }
template<class T> inline void endianswap(T *buf, int len) { for(T *end = &buf[len]; buf < end; buf++) *buf = endianswap(*buf); }
template<class T> inline T endiansame(T n) { return n; }
template<class T> inline void endiansame(T *buf, int len) {}
#ifdef SDL_BYTEORDER
    #if SDL_BYTEORDER == SDL_LIL_ENDIAN
        #define lilswap endiansame
        #define bigswap endianswap
    #else
        #define lilswap endianswap
        #define bigswap endiansame
    #endif
#else
    template<class T> inline T lilswap(T n) { return *(const uchar *)&islittleendian ? n : endianswap(n); }
    template<class T> inline void lilswap(T *buf, int len) { if(!*(const uchar *)&islittleendian) endianswap(buf, len); }
    template<class T> inline T bigswap(T n) { return *(const uchar *)&islittleendian ? endianswap(n) : n; }
    template<class T> inline void bigswap(T *buf, int len) { if(*(const uchar *)&islittleendian) endianswap(buf, len); }
#endif

#define uint2ip(address, ip) uchar ip[4]; \
if(isbigendian())\
{ \
    enet_uint32 big = endianswap(address);\
    memcpy(&ip, &big, 4);\
}\
else memcpy(&ip, &address, 4);\

/* workaround for some C platforms that have these two functions as macros - not used anywhere */
#ifdef getchar
#undef getchar
#endif
#ifdef putchar
#undef putchar
#endif

struct stream
{
    virtual ~stream() {}
    virtual void close() = 0;
    virtual bool end() = 0;
    virtual long tell() { return -1; }
    virtual bool seek(long offset, int whence = SEEK_SET) { return false; }
    virtual long size();
    virtual void fflush() {}
    virtual int read(void *buf, int len) { return 0; }
    virtual int write(const void *buf, int len) { return 0; }
    virtual int getchar() { uchar c; return read(&c, 1) == 1 ? c : -1; }
    virtual bool putchar(int n) { uchar c = n; return write(&c, 1) == 1; }
    virtual bool getline(char *str, int len);
    virtual bool putstring(const char *str) { int len = (int)strlen(str); return write(str, len) == len; }
    virtual bool putline(const char *str) { return putstring(str) && putchar('\n'); }
    virtual int printf(const char *fmt, ...) PRINTFARGS(2, 3) { return -1; }
    virtual uint getcrc() { return 0; }

    template<class T> bool put(T n) { return write(&n, sizeof(n)) == sizeof(n); }
    template<class T> bool putlil(T n) { return put<T>(lilswap(n)); }
    template<class T> bool putbig(T n) { return put<T>(bigswap(n)); }

    template<class T> T get() { T n; return read(&n, sizeof(n)) == sizeof(n) ? n : 0; }
    template<class T> T getlil() { return lilswap(get<T>()); }
    template<class T> T getbig() { return bigswap(get<T>()); }

#ifndef STANDALONE
    SDL_RWops *rwops();
#endif
};

extern string _timestringbuffer;
extern const char *timestring(time_t t, bool local = false, const char *fmt = NULL, char *buf = _timestringbuffer);
inline const char *timestring(bool local = false, const char *fmt = NULL, char *buf = _timestringbuffer) { return timestring(time(NULL), local, fmt, buf); }
extern const char *asctimestr();
extern const char *numtime();
extern void transformoldentitytypes(int mapversion, uchar &enttype);
extern int fixmapheadersize(int version, int headersize);
extern char *path(char *s);
extern char *path(const char *s, bool copy);
extern char *unixpath(char *s);
extern const char *behindpath(const char *s);
extern const char *parentdir(const char *directory);
extern bool fileexists(const char *path, const char *mode);
extern bool createdir(const char *path);
extern size_t fixpackagedir(char *dir);
extern void sethomedir(const char *dir);
extern bool havehomedir();
extern void addpackagedir(const char *dir);
extern int findfilelocation;
enum { FFL_WORKDIR = -2, FFL_HOME = -1, FFL_ZIP = 0 };
extern const char *findfile(const char *filename, const char *mode);
extern int getfilesize(const char *filename);
extern stream *openvecfile(vector<uchar> *s = NULL, bool autodelete = true);
extern stream *openmemfile(const uchar *buf, int size, int *refcnt);
extern bool findzipfile(const char *name);
extern stream *openzipfile(const char *filename, const char *mode);
extern stream *openfile(const char *filename, const char *mode);
extern stream *opentempfile(const char *filename, const char *mode);
extern stream *opengzfile(const char *filename, const char *mode, stream *file = NULL, int level = Z_BEST_COMPRESSION);
extern char *loadfile(const char *fn, int *size, const char *mode = NULL);
extern int streamcopy(stream *dest, stream *source, int maxlen = INT_MAX);
extern void filerotate(const char *basename, const char *ext, int keepold, const char *oldformat = NULL);
extern const char *stream_capabilities();
extern void listsubdirs(const char *dir, vector<char *> &subdirs, int (__cdecl *sf)(const char **, const char **));
extern bool listdir(const char *dir, const char *ext, vector<char *> &files);
extern void listfiles(const char *dir, const char *ext, vector<char *> &files, int (__cdecl *sf)(const char **, const char **) = NULL);
extern void listfilesrecursive(const char *dir, vector<char *> &files, int level = 0);
extern void listdirsrecursive(const char *dir, vector<char *> &subdirs, int level = 0);
extern void listzipfiles(const char *dir, const char *ext, vector<char *> &files);
extern void listzipdirs(const char *dir, vector<char *> &subdirs);
extern bool delfile(const char *path);
extern void backup(char *name, char *backupname);
extern bool validzipmodname(char *name);
extern void *zipmanualopen(stream *f, vector<const char *> &files);
extern stream *zipmanualstream(void *a, int n);
extern int zipmanualread(void *a, int n, stream *f, int maxlen = INT_MAX);
extern void zipmanualclose(void *a);
extern struct mapstats *loadmapstats(const char *filename, bool getlayout);
extern bool cmpb(void *b, int n, enet_uint32 c);
extern bool cmpf(char *fn, enet_uint32 c);
extern enet_uint32 adler(unsigned char *data, size_t len);
extern void endianswap(void *, int, int);
extern bool isbigendian();
extern void strtoupper(char *t, const char *s = NULL);
extern void seedMT(uint seed);
extern uint randomMT(void);
extern void popMT(void);
extern void entropy_init(uint seed);
extern void entropy_save();
extern void entropy_add_byte(uchar b);
extern void entropy_add_block(const uchar *s, int len);
extern void entropy_get(uchar *buf, int len);

struct iprange { enet_uint32 lr, ur; };
extern const char *atoip(const char *s, enet_uint32 *ip);
extern const char *atoipr(const char *s, iprange *ir);
extern const char *iptoa(const enet_uint32 ip);
extern const char *iprtoa(const struct iprange &ipr);
extern int cmpiprange(const struct iprange *a, const struct iprange *b);
extern int cmpipmatch(const struct iprange *a, const struct iprange *b);
extern int cvecprintf(vector<char> &v, const char *s, ...) PRINTFARGS(2, 3);
extern const char *hiddenpwd(const char *pwd, int showchars = 0);
extern int getlistindex(const char *key, const char *list[], bool acceptnumeric = true, int deflt = -1);
extern void parseupdatelist(hashtable<const char *, int> &ht, char *buf, const char *prefix = NULL, const char *suffix = NULL);

struct twoint { int key, val; };
struct threeint { int key, val1, val2; };
extern int cmpintasc(const int *a, const int *b);  // leads to ascending sort order
extern int cmpintdesc(const int *a, const int *b); // leads to descending sort order
extern int stringsort(const char **a, const char **b);
extern int stringsortrev(const char **a, const char **b);
extern int stringsortignorecase(const char **a, const char **b);
extern int stringsortignorecaserev(const char **a, const char **b);

#if defined(WIN32) && !defined(_DEBUG) && !defined(__GNUC__)
extern void stackdumper(unsigned int type, EXCEPTION_POINTERS *ep);
#endif

struct sl_semaphore
{
    void *data;
    int *errorcount;

    sl_semaphore(int init, int *errorcount);  // init: initial semaphore value; errorcount: pointer to error counter for semaphore-related errors or NULL
    ~sl_semaphore();
    void wait();     // blocks, until semaphore gets available
    int trywait();   // returns 0, if semaphore was locked (like wait(), but returns !=0 instead of blocking)
    int timedwait(int howlongmillis); // like trywait(), but waits for a litte before returning failure
    int getvalue();  // returns current semaphore value
    void post();     // increments (unlocks) semaphore
};

extern void *sl_createthread(int (*fn)(void *), void *data);
extern int sl_waitthread(void *ti);
extern bool sl_pollthread(void *ti);
extern void sl_detachthread(void *ti);
extern void sl_sleep(int duration);
extern bool ismainthread();

#endif

