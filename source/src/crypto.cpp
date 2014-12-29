#include "cube.h"

///////////////////////// cryptography /////////////////////////////////

/* Based off the reference implementation of Tiger, a cryptographically
 * secure 192 bit hash function by Ross Anderson and Eli Biham. More info at:
 * http://www.cs.technion.ac.il/~biham/Reports/Tiger/
 */

#define TIGER_PASSES 3

namespace tiger
{
    typedef unsigned long long int chunk;

    union hashval
    {
        uchar bytes[3*8];
        chunk chunks[3];
    };

    chunk sboxes[4*256];

    void compress(const chunk *str, chunk state[3])
    {
        chunk a, b, c;
        chunk aa, bb, cc;
        chunk x0, x1, x2, x3, x4, x5, x6, x7;

        a = state[0];
        b = state[1];
        c = state[2];

        x0=str[0]; x1=str[1]; x2=str[2]; x3=str[3];
        x4=str[4]; x5=str[5]; x6=str[6]; x7=str[7];

        aa = a;
        bb = b;
        cc = c;

        loop(pass_no, TIGER_PASSES)
        {
            if(pass_no)
            {
                x0 -= x7 ^ 0xA5A5A5A5A5A5A5A5ULL; x1 ^= x0; x2 += x1; x3 -= x2 ^ ((~x1)<<19);
                x4 ^= x3; x5 += x4; x6 -= x5 ^ ((~x4)>>23); x7 ^= x6;
                x0 += x7; x1 -= x0 ^ ((~x7)<<19); x2 ^= x1; x3 += x2;
                x4 -= x3 ^ ((~x2)>>23); x5 ^= x4; x6 += x5; x7 -= x6 ^ 0x0123456789ABCDEFULL;
            }

#define sb1 (sboxes)
#define sb2 (sboxes+256)
#define sb3 (sboxes+256*2)
#define sb4 (sboxes+256*3)

#define tround(a, b, c, x) \
      c ^= x; \
      a -= sb1[((c)>>(0*8))&0xFF] ^ sb2[((c)>>(2*8))&0xFF] ^ \
       sb3[((c)>>(4*8))&0xFF] ^ sb4[((c)>>(6*8))&0xFF] ; \
      b += sb4[((c)>>(1*8))&0xFF] ^ sb3[((c)>>(3*8))&0xFF] ^ \
       sb2[((c)>>(5*8))&0xFF] ^ sb1[((c)>>(7*8))&0xFF] ; \
      b *= mul;

            uint mul = !pass_no ? 5 : (pass_no==1 ? 7 : 9);
            tround(a, b, c, x0) tround(b, c, a, x1) tround(c, a, b, x2) tround(a, b, c, x3)
            tround(b, c, a, x4) tround(c, a, b, x5) tround(a, b, c, x6) tround(b, c, a, x7)

            chunk tmp = a; a = c; c = b; b = tmp;
        }

        a ^= aa;
        b -= bb;
        c += cc;

        state[0] = a;
        state[1] = b;
        state[2] = c;
    }

    void gensboxes()
    {
        const char *str = "Tiger - A Fast New Hash Function, by Ross Anderson and Eli Biham";
        chunk state[3] = { 0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL, 0xF096A5B4C3B2E187ULL };
        uchar temp[64];

        if(!*(const uchar *)&islittleendian) loopj(64) temp[j^7] = str[j];
        else loopj(64) temp[j] = str[j];
        loopi(1024) loop(col, 8) ((uchar *)&sboxes[i])[col] = i&0xFF;

        int abc = 2;
        loop(pass, 5) loopi(256) for(int sb = 0; sb < 1024; sb += 256)
        {
            abc++;
            if(abc >= 3) { abc = 0; compress((chunk *)temp, state); }
            loop(col, 8)
            {
                uchar val = ((uchar *)&sboxes[sb+i])[col];
                ((uchar *)&sboxes[sb+i])[col] = ((uchar *)&sboxes[sb + ((uchar *)&state[abc])[col]])[col];
                ((uchar *)&sboxes[sb + ((uchar *)&state[abc])[col]])[col] = val;
            }
        }
    }

    void hash(const uchar *str, int length, hashval &val)
    {
        static bool init = false;
        if(!init) { gensboxes(); init = true; }

        uchar temp[64];

        val.chunks[0] = 0x0123456789ABCDEFULL;
        val.chunks[1] = 0xFEDCBA9876543210ULL;
        val.chunks[2] = 0xF096A5B4C3B2E187ULL;

        int i = length;
        for(; i >= 64; i -= 64, str += 64)
        {
            if(!*(const uchar *)&islittleendian)
            {
                loopj(64) temp[j^7] = str[j];
                compress((chunk *)temp, val.chunks);
            }
            else compress((chunk *)str, val.chunks);
        }

        int j;
        if(!*(const uchar *)&islittleendian)
        {
            for(j = 0; j < i; j++) temp[j^7] = str[j];
            temp[j^7] = 0x01;
            while(++j&7) temp[j^7] = 0;
        }
        else
        {
            for(j = 0; j < i; j++) temp[j] = str[j];
            temp[j] = 0x01;
            while(++j&7) temp[j] = 0;
        }

        if(j > 56)
        {
            while(j < 64) temp[j++] = 0;
            compress((chunk *)temp, val.chunks);
            j = 0;
        }
        while(j < 56) temp[j++] = 0;
        *(chunk *)(temp+56) = (chunk)length<<3;
        compress((chunk *)temp, val.chunks);
        lilswap(val.chunks, 3);
    }
}
#undef sb1
#undef sb2
#undef sb3
#undef sb4
#undef tround

///////////////////////////////////////////  SHA512  ////////////////////////////////////////////////////////////////////
// adapted from pseudocode and various public domain sources (probably all originating in LibTomCrypt)
// (consider this code public domain, too)

#define RR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define SHA512ROUND(a, b, c, d, e, f, g, h, i)         \
     d += h = h + (RR64(e,14) ^ RR64(e,18) ^ RR64(e,41)) + (g ^ (e & (f ^ g))) + K[i] + W[i];   \
     h += (RR64(a,28) ^ RR64(a,34) ^ RR64(a,39)) + (((a | b) & c) | (a & b));
#define SHA512SIZE (512 / 8)

void sha512_compress(uint64_t *state, const uchar *inbuf)  // sha512 main function: compress one chunk of 128 bytes
{
    static const uint64_t K[80] =
    {
        0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
        0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL, 0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
        0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL, 0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
        0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
        0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL, 0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
        0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
        0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
        0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL, 0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
        0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
        0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
    };

    uint64_t S[8], W[80];

    loopi(8) S[i] = state[i];
    loopi(16) W[i] = bigswap(((uint64_t*)inbuf)[i]);
    loopi(64) W[i + 16] = W[i] + W[i + 9] + (RR64(W[i + 14],19) ^ RR64(W[i + 14],61) ^ (W[i + 14] >> 6)) + (RR64(W[i + 1],1) ^ RR64(W[i + 1],  8) ^ (W[i + 1] >> 7));

    for(int i = 0; i < 80; i += 8)
    {
        SHA512ROUND(S[0],S[1],S[2],S[3],S[4],S[5],S[6],S[7],i+0);
        SHA512ROUND(S[7],S[0],S[1],S[2],S[3],S[4],S[5],S[6],i+1);
        SHA512ROUND(S[6],S[7],S[0],S[1],S[2],S[3],S[4],S[5],i+2);
        SHA512ROUND(S[5],S[6],S[7],S[0],S[1],S[2],S[3],S[4],i+3);
        SHA512ROUND(S[4],S[5],S[6],S[7],S[0],S[1],S[2],S[3],i+4);
        SHA512ROUND(S[3],S[4],S[5],S[6],S[7],S[0],S[1],S[2],i+5);
        SHA512ROUND(S[2],S[3],S[4],S[5],S[6],S[7],S[0],S[1],i+6);
        SHA512ROUND(S[1],S[2],S[3],S[4],S[5],S[6],S[7],S[0],i+7);
    }
    loopi(8) state[i] += S[i];
}

void sha512(uchar *hash, const uchar *msg, int msglen)
{
    uint64_t state[8] = { 0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
                          0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL };
    uint64_t total = msglen * 8ULL;
    for(; msglen >= 128; msglen -= 128)
    {
        sha512_compress(state, msg);
        msg += 128;
    }
    uchar endmsg[128] = { 0 };
    memcpy(endmsg, msg, msglen);
    endmsg[msglen] = 0x80;
    if(msglen > 111)
    {
        sha512_compress(state, endmsg);
        memset(endmsg, 0, 128);
    }
    *((uint64_t*)(endmsg + 120)) = bigswap(total);
    sha512_compress(state, endmsg);
    loopi(8) ((uint64_t*)hash)[i] = bigswap(state[i]);
}
#undef RR64
#undef SHA512ROUND

#ifndef STANDALONE
COMMANDF(sha512, "s", (char *msg)   // SHA512 debug command
{
    uchar hash[SHA512SIZE];
    sha512(hash, (uchar*)msg, strlen(msg));
    defformatstring(erg)("SHA512: ");
    loopi(SHA512SIZE) concatformatstring(erg, "%02x", hash[i]);
    conoutf("%s", erg);
});

COMMANDF(tiger, "s", (char *msg)    // tiger debug command
{
    tiger::hashval hash;
    tiger::hash((uchar *)msg, (int)strlen(msg), hash);
    defformatstring(erg)("TIGER: ");
    loopi(24) concatformatstring(erg, "%02x", hash.bytes[i]);
    conoutf("%s", erg);
});
#endif


////////////////////////// crypto rand (Mersenne twister) ////////////////////////////////////////

#define MT_N (624)
#define MT_M (397)
#define MT_K (0x9908B0DFU)

static uint mt_state[2 * MT_N];
static int mt_next = MT_N;

void seedMT_do(uint seed, uint *statearray, uint arraysize)
{
    statearray[0] = seed;
    for(uint i = 1; i < arraysize; i++)
        statearray[i] = seed = 1812433253U * (seed ^ (seed >> 30)) + i;
}

void seedMT(uint seed)
{
    loopi(MT_N) mt_state[i + MT_N] = mt_state[i];
    mt_state[0] = seed;
    seedMT_do(seed, mt_state, MT_N);
    mt_next = 0;
}

uint randomMT()
{
    int cur = mt_next;
    if(++mt_next >= MT_N)
    {
        if(mt_next > MT_N) { seedMT(5489U + time(NULL)); cur = mt_next++; }
        else mt_next = 0;
    }
    uint y = (mt_state[cur] & 0x80000000U) | (mt_state[mt_next] & 0x7FFFFFFFU);
    mt_state[cur] = y = mt_state[cur < MT_N - MT_M ? cur + MT_M : cur + MT_M - MT_N] ^ (y >> 1) ^ (-int(y & 1U) & MT_K);
    y ^= (y >> 11);
    y ^= (y <<  7) & 0x9D2C5680U;
    y ^= (y << 15) & 0xEFC60000U;
    y ^= (y >> 18);
    return y;
}

void popMT()  // undo last seedMT()
{
    loopi(MT_N) mt_state[i] = mt_state[i + MT_N];
}

///////////////////////////////////////////////  Entropy collector  ////////////////////////////////////////////////////////////////////////////////
// * collects entropy (true random data) through mouse moves and keyboard action (and from other sources)
// * used to seed the pseudorandom generator
// * used to generate very good private keys

#define ENTCHUNKS 32                        // pool size (in 128-bytes chunks)
#define ENTPOOLSIZE (ENTCHUNKS * 128)
#define ENTROPYSAVEFILE "config" PATHDIVS "entropy.dat"

static uchar *entpool = NULL;

static void xor_block(uchar *d, const uchar *s, int len)
{
    uint64_t *dd = (uint64_t *)d, *ss = (uint64_t *)s;
    for( ; len > 7; len -= 8) *dd++ ^= *ss++;
    d = (uchar*)dd, s = (uchar*)ss;
    while(len-- > 0) *d++ ^= *s++;
}

static void mix_block(uchar *d, const uchar *s, int len)
{
    static uchar sum = 0;
    int t;
    while(len-- > 0)
    {
        t = *d * *s++ + sum;
        *d++ += t >> 4;
        sum += t >> 7;
    }
}

void entropy_init(uint seed)
{
    DELETEP(entpool);
    entpool = new uchar[ENTPOOLSIZE + 128];
    seedMT_do(seed, (uint*)entpool, ENTPOOLSIZE / sizeof(uint));
    int len = 0;
    char *buf = loadfile(ENTROPYSAVEFILE, &len);
    if(buf && len) entropy_add_block((uchar*)buf, len);
    DELETEA(buf);
    entropy_get((uchar*)mt_state, MT_N * sizeof(uint));
}

void entropy_save()
{
    stream *f = openfile(ENTROPYSAVEFILE, "w");
    if(f) f->write(entpool, ENTPOOLSIZE);
    DELETEP(f);
}

void entropy_add_byte(uchar b)
{
    static int next = 0;

    int r = entpool[next] & 7;
    entpool[next++] ^= b << r | b >> (8 - r);
    if(next >= ENTPOOLSIZE) next = 0;
}

void entropy_add_block(const uchar *s, int len)
{
    for( ; len > 0; s += 128, len -= 128)
    {
        int bl = min(len, 128);
        xor_block(entpool + rnd(ENTPOOLSIZE / 8 - 16) * 8, s, bl);
        mix_block(entpool + rnd(ENTPOOLSIZE - bl), s, bl);
    }
}

void entropy_get(uchar *buf, int len)
{
    ASSERT(len <= ENTPOOLSIZE);
    uchar *tempbuf = new uchar[len + SHA512SIZE];
    memcpy(tempbuf, entpool, len);
    memcpy(entpool + ENTPOOLSIZE, entpool, 128);
    sha512(tempbuf, entpool, ENTPOOLSIZE);
    loopi(len) sha512_compress((uint64_t*)(tempbuf + (i & ~7)), entpool + rnd(ENTPOOLSIZE / 8) * 8);
    memcpy(buf, tempbuf, len);
    delete[] tempbuf;
}

#ifndef STANDALONE
COMMANDF(entropy, "", ()
{
    uchar buf[32];
    entropy_get(buf, 32);
    defformatstring(erg)("entropy: ");
    loopi(32) concatformatstring(erg, "%02x", buf[i]);
    conoutf("%s", erg);
});
#endif


/////////////////// misc helper functions ////////////////////////////////////////////////////////////////

static const char *hashchunktoa(tiger::chunk h)   // portable solution instead of printf("%llx")
{                                                 // use next protocol bump to switch to hashstring() above!
    static string buf;
    static int bufidx;
    bufidx = (bufidx + 1) & 0x3;
    char *s = buf + (bufidx * 33) + 33;
    *s-- = '\0';
    while(h)
    {
        *s-- = "0123456789abcdef"[h & 0xf];
        h >>= 4;
    }
    return s + 1;
}

const char *genpwdhash(const char *name, const char *pwd, int salt)
{
    static string temp;
    formatstring(temp)("%s %d %s %s %d", pwd, salt, name, pwd, iabs(PROTOCOL_VERSION));
    tiger::hashval hash;
    tiger::hash((uchar *)temp, (int)strlen(temp), hash);
    formatstring(temp)("%s %s %s", hashchunktoa(hash.chunks[0]), hashchunktoa(hash.chunks[1]), hashchunktoa(hash.chunks[2]));
    return temp;
}





