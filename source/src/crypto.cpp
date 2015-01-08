#include "cube.h"

#ifdef STANDALONE
#define DEBUGCOND (true)
#else
VARP(cryptodebug, 0, 0, 1);
#define DEBUGCOND (cryptodebug==1)
#endif

static const char *hexdigits = "0123456789abcdef";


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
#ifdef _DEBUG
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
#endif


/////////////////////////////////////////////////////////  FNV-1a  ///////////////////////////////////////////////////////////////////
// Fowler–Noll–Vo is a non-cryptographic hash function created by Glenn Fowler, Landon Curt Noll, and Phong Vo. It is public domain.
// We use the 32-bit-version here.

inline void fnv1a_init(uint32_t &hash)
{
    hash = 2166136261UL;
}

inline void fnv1a_add(uint32_t &hash, uchar byte)
{
    hash ^= byte;
    hash *= 16777619;
}


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
        xor_block(entpool + rnd(ENTPOOLSIZE / 8 - 16) * 8, s, bl);      // use pseudorandom to mix entropy into the pool in a hard to predict fashion
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
///////////////////////////////////////////////  key derivation function  ////////////////////////////////////////////////////////////////////////
// * basically a hash function that delivers keylen bytes calculated from pass and salt
// * but uses a lot of memory and a quite complex algorithm, to make brute-force offline-attacks on short passwords as hard as possible (even if someone uses fpgas)
// * last line of defense against "evil little brothers"

void passphrase2key(const char *pass, const uchar *salt, int saltlen, uchar *key, int keylen, int *iterations, int maxtime, int memusage)
{
    memset(key, 0, keylen);
    if(!*pass) return;                       // password "" ->
    memusage = clamp(memusage, 2, 1024);     // keep memory usage between 2MB and 1GB
    int memsize = (1<<20) * memusage, passlen = strlen(pass), tmpbuflen = 2 * SHA512SIZE, pplen = 2 * (saltlen + passlen);

    // prepare the passphrase (including salt)
    uchar *pp = new uchar[pplen];
    memcpy(pp, salt, saltlen);
    memcpy(pp + saltlen, pass, passlen);
    memcpy(pp + saltlen + passlen, pass, passlen);
    memcpy(pp + saltlen + 2 * passlen, salt, saltlen);
    if((memusage & 1) && passlen + saltlen >= 8) identhash((uint64_t *)(pp + saltlen + passlen));

    // mix up the passphrase a bit
    uchar *tmpbuf = new uchar[tmpbuflen + sizeof(uint)];
    memset(tmpbuf, 0, tmpbuflen + sizeof(uint));
    sha512(tmpbuf, pp, pplen);
    xor_block(pp, tmpbuf, min(pplen, tmpbuflen));
    sha512(tmpbuf + SHA512SIZE, pp, pplen);
    tiger::hash(pp, pplen, *((tiger::hashval *)tmpbuf));

    // initialise the huge buffer
    uchar *hugebuf = new uchar[memsize];
    memset(hugebuf, 0, memsize);
    int memchunk = memsize / tmpbuflen;
    loopi(tmpbuflen) seedMT_do(*((uint*)(tmpbuf + i)), (uint*)(hugebuf + memchunk * i), memchunk / sizeof(uint));   // spread the passphrase all over the huge buffer

    // start mixing
    #define mixit(x, y) (((((uint*)tmpbuf)[(roundsdone ^ (x)) % (tmpbuflen / sizeof(uint))]) % ((y) / 8 - (x) % 11)) * 8)
    stopwatch watch;
    watch.start();
    int roundsdone = 0;
    uint shortseed = 0;
    do
    {
        loopirev(memsize / 127) shortseed = ((uint*)hugebuf)[shortseed % (memsize / sizeof(uint))];
        seedMT_do(shortseed, (uint*)tmpbuf, tmpbuflen / sizeof(uint));
        loopi(2999) sha512_compress((uint64_t*)(hugebuf + mixit(i * 5, memsize - SHA512SIZE)), hugebuf + mixit(i * 17, memsize - 128));
        roundsdone++;
    }
    while((!*iterations && watch.elapsed() < maxtime) || (*iterations && *iterations > roundsdone));     // *iterations == 0: time-limited, otherwise rounds-limited

    // extract the key
    if(keylen * 8 > 192) sha512(tmpbuf, hugebuf, memsize);
    else tiger::hash(hugebuf, memsize, *((tiger::hashval *)tmpbuf));
    loopi(SHA512SIZE) sha512_compress(((uint64_t*)tmpbuf) + (i / 8), hugebuf + (((tmpbuf[i] * 68111) % (memsize - 128)) & ~7));
    memcpy(key, tmpbuf, min(keylen, tmpbuflen));                     // max keylen is 2 * SHA512SIZE
    DEBUG("passphrase2key: " << watch.elapsed() << " ms, " << roundsdone << " rounds, " << memsize << " bytes used");
    *iterations = roundsdone;
    delete[] hugebuf;
    delete[] tmpbuf;
    delete[] pp;
    #undef mixit
}
#endif

/////////////////////////////////////////////// hex converters /////////////////////////////////////////////////////////////////////////////////////

const char *bin2hex(char *d, const uchar *s, int len)
{ // d needs to be at least of size 2 * len + 1
    len *= 2;
    loopi(len) d[i] = hexdigits[(s[i / 2] >> ((i & 1) ? 0 : 4)) & 0xf];
    d[len] = '\0';
    return d;
}

int hex2bin(uchar *d, const char *s, int maxlen)
{
    memset(d, 0, maxlen);
    int len;
    for(len = 0; s[len] && len < maxlen * 2; len++)
    {
        const char *hd = strchr(hexdigits, tolower(s[len]));
        if(!hd) break; // -> not a hex digit
        d[len / 2] |= (hd - hexdigits) << ((len & 1) ? 0 : 4);
    }
    return len / 2;
}

///////////////////////////////////////////////  Ed25519: high-speed high-security signatures  //////////////////////////////////////////////////////

#include "crypto_tools.h"

void ed25519_pubkey_from_private(uchar *pubkey, const uchar *privkey)
{
    unsigned char az[64];
    sc25519 scsk;
    ge25519 gepk;

    sha512(az, privkey, 32);
    az[0] &= 248;
    az[31] &= 127;
    az[31] |= 64;

    sc25519_from32bytes(&scsk,az);

    ge25519_scalarmult_base(&gepk, &scsk);
    ge25519_pack(pubkey, &gepk);
}

void privkey_from_prepriv(unsigned char *privkey, const unsigned char *prepriv, int preprivlen, unsigned char *privpriv = NULL) // derive private key from even more secret bulk of entropy
{
    uchar temp[SHA512SIZE], pub[32], testpriv[32];
    sha512(temp, prepriv, preprivlen);
    int esc, minesc = 32, minescpos = 0;
    loopi(33)
    { // find privte key with the public key with the least getint() escape codes (yes, that decreases the keyrange. sue me.)
        ed25519_pubkey_from_private(testpriv, temp + i);
        ed25519_pubkey_from_private(pub, testpriv);
        esc = 0;
        loopj(32) if((pub[j] & 0xfe) == 0x80) esc++;
        if(esc < minesc) { minesc = esc; minescpos = i; }
        if(esc == 0) break;
    }
    ed25519_pubkey_from_private(privkey, temp + minescpos);  // (the resulting private key is actually itself a public key of a private derived from prepriv)
    if(privpriv) memcpy(privpriv, temp + minescpos, 32);
}

void ed25519_sign(uchar *sm, int *smlen, const uchar *m, int mlen, const uchar *sk)
{                                           // sk: 32-byte private key + 32-byte public key
    uchar pk[32], az[64], nonce[64], hram[64];
    sc25519 sck, scs, scsk;
    ge25519 ger;

    memmove(pk, sk + 32, 32);               // pk: 32-byte public key A

    sha512(az, sk, 32);
    az[0] &= 248;
    az[31] &= 127;
    az[31] |= 64;                           // az: 32-byte scalar a, 32-byte randomizer z

    if(smlen) *smlen = mlen + 64;
    memmove(sm + 64, m, mlen);              // need memmove here: m and sm may overlap
    memmove(sm + 32, az + 32, 32);          // sm: 32-byte uninit, 32-byte z, mlen-byte m
    sha512(nonce, sm + 32, mlen + 32);      // nonce: 64-byte H(z,m)

    sc25519_from64bytes(&sck, nonce);
    ge25519_scalarmult_base(&ger, &sck);
    ge25519_pack(sm, &ger);                 // sm: 32-byte R, 32-byte z, mlen-byte m

    memmove(sm + 32, pk, 32);               // sm: 32-byte R, 32-byte A, mlen-byte m
    sha512(hram, sm, mlen + 64);            // hram: 64-byte H(R,A,m)

    sc25519_from64bytes(&scs, hram);
    sc25519_from32bytes(&scsk, az);
    sc25519_mul(&scs, &scs, &scsk);
    sc25519_add(&scs, &scs, &sck);          // scs: S = nonce + H(R,A,m)a

    sc25519_to32bytes(sm + 32, &scs);       // sm: 32-byte R, 32-byte S, mlen-byte m
}

uchar *ed25519_sign_check(uchar *sm, int smlen, const uchar *pk)
{
    uchar scopy[32], hram[64], rcheck[32];
    ge25519 get1, get2;
    sc25519 schram, scs;

    if(smlen < 64 || (sm[63] & 224) || ge25519_unpackneg_vartime(&get1, pk)) return NULL;  // frame error

    memmove(scopy, sm + 32, 32);
    sc25519_from32bytes(&scs, sm + 32);

    memmove(sm + 32, pk, 32);
    sha512(hram, sm, smlen);

    sc25519_from64bytes(&schram, hram);

    ge25519_double_scalarmult_vartime(&get2, &get1, &schram, &ge25519_base, &scs);
    ge25519_pack(rcheck, &get2);

    memmove(sm + 32, scopy, 32);        // restore sm

    return memcmp(rcheck, sm, 32) ? NULL : sm + 64;
}


#ifndef STANDALONE
#ifdef _DEBUG
void ed25519test(char *vectorfilename)  // check ed25519 functions with test vectors from http://ed25519.cr.yp.to/python/sign.input
{
    path(vectorfilename);
    stopwatch watch;
    int vectorfilesize = 0;
    char *vectorfile = loadfile(vectorfilename, &vectorfilesize);
    if(!vectorfile) { conoutf("could not read test vector file %s", vectorfilename); return; }
    conoutf("loaded %d bytes from %s", vectorfilesize, vectorfilename);
    watch.start();
    int lines = 0, inputerrors = 0, pubfail = 0, signfail = 0, verifyfail = 0, verifyfail2 = 0;
    for(char *l = strtok(vectorfile, "\n"); l; l = strtok(NULL, "\n"))
    { // a line consists of 32 bytes private key, 32 byte public key, ":", 32 byte public key (again), ":", message, ":", signed message, ":"
        char *vprivpub = l, *vpub = strchr(l, ':'), *vmsg, *vsmsg, hextemp[65];
        int linelen = strlen(l), msglen = 0;
        uchar privpub[64], pub[32], msg[linelen], smsg[linelen], temp[64];
        lines++;
        if(!vpub || !(vmsg = strchr(vpub + 1, ':')) || !(vsmsg = strchr(vmsg + 1, ':')) ||
           hex2bin(privpub, vprivpub, 64) != 64 || hex2bin(pub, ++vpub, 32) != 32 || (msglen = hex2bin(msg, ++vmsg, linelen)) < 0 || hex2bin(smsg, ++vsmsg, linelen) != (msglen + 64) ||
           memcmp(msg, smsg + 64, msglen) || memcmp(privpub + 32, pub, 32)) { inputerrors++; continue; } // line does not match template
        ed25519_pubkey_from_private(temp, privpub);
        if(strncmp(bin2hex(hextemp, temp, 32), vpub, 64)) { pubfail++; continue; }   // private and public key don't match
        ed25519_sign(msg, &linelen, msg, msglen, privpub);
        if(linelen != msglen + 64 || memcmp(msg, smsg, linelen)) { signfail++; continue; }   // newly signed message doesn't match vector data
        if(!ed25519_sign_check(smsg, msglen + 64, pub)) { verifyfail++; continue; }     // verification of signed message failed
        smsg[msglen % 64]++;
        if(ed25519_sign_check(smsg, msglen + 64, pub)) { verifyfail2++; continue; }     // verification of altered signed message didn't fail (...but should've)
        smsg[msglen % 64]--;
        if(msglen)
        {
            smsg[42 % msglen]++;
            if(ed25519_sign_check(smsg, msglen + 64, pub)) { verifyfail2++; continue; }     // verification of altered signed message didn't fail (...but should've)
            smsg[42 % msglen]--;
        }
        pub[msglen % 32]++;
        if(ed25519_sign_check(smsg, msglen + 64, pub)) { verifyfail2++; continue; }     // verification of altered signed message didn't fail (...but should've)
    }
    DELETEA(vectorfile);
    conoutf("processed %d lines in %d milliseconds", lines, watch.elapsed());
    conoutf("%d vector file errors, %d private-pubkey mismatches, %d sign fails, %d verify fails, %d missed verify fails", inputerrors, pubfail, signfail, verifyfail, verifyfail2);
}
COMMAND(ed25519test, "s");

void ed25519speedtest()  // check speed of ed25519 functions
{
    uchar priv[32], pub[32], msg[64], smsg[128], sk[64];
    entropy_get(priv, 32);
    entropy_get(msg, 32);
    stopwatch watch;
    watch.start();
    loopi(200) ed25519_pubkey_from_private(pub, priv);
    conoutf("create public key from private key: %.2f ms", watch.elapsed() / 200.0f);
    memcpy(sk, priv, 32);
    memcpy(sk + 32, pub, 32);
    watch.start();
    loopi(200) ed25519_sign(smsg, NULL, msg, 64, sk);
    conoutf("signing 64-byte message: %.2f ms", watch.elapsed() / 200.0f);
    watch.start();
    loopi(200) ed25519_sign_check(smsg, 128, pub);
    conoutf("verify signature of 64-byte message: %.2f ms", watch.elapsed() / 200.0f);
}
COMMAND(ed25519speedtest, "");
#endif
#endif


#ifndef STANDALONE

//////////////////////////////////// game key management ////////////////////////////////////////////////////////////////////////
// * the game key is the primary player key that is associated with his player account
// * it is used during playing to authenticate the player to the server
// * needs special protection...

#define AUTHPREPRIVATECFGFILE  "config" PATHDIVS "authpreprivate.cfg"
#define AUTHPRIVATECFGFILE     "config" PATHDIVS "authprivate.cfg"

VARP(authmemusage, 2, 24, (1<<10) - 1);     // megabytes of RAM to use for password hash (when using a new password)
VARP(authrounds, 0, 0, INT_MAX - 1);        // create new password hashes with a fixed number of rounds (if authrounds > 0)
VARP(authmaxtime, 1<<9, 1<<12, 1<<16);      // create new password hashes with a fixed amount of time (in ms)

static uchar *sk = NULL;                    // game key
static struct { uchar *salt, *priv; uint pwdcfg; } passdargs;

static int passdeferred(void *pass)   // decrypt the private key in the background
{
    uchar keyhash[32];
    int iterations = passdargs.pwdcfg >> 10;
    passphrase2key((char*)pass, passdargs.salt, 16, keyhash, 32, &iterations, 0, passdargs.pwdcfg & 0x3ff);
    xor_block(passdargs.priv, keyhash, 32);
    delstring((char *)pass);
    extern void authsetup(char **args, int numargs);
    authsetup(NULL, -1);  // tell authsetup, that the passwd is done
    return 0;
}

void authsetup(char **args, int numargs)  // set up private and public keys
{
    const int preprivminlen = 32, preprivmaxlen = 128;
    static uchar *buf = NULL, pub[32], keyhash[128], psalt[16], salt[16], preprivlen = 0;
    static uint preprivpwdcfg = 0, privpwdcfg = 0;
    static void *passdrunning = NULL, *passdcleanup = NULL;
    static char *sleepcmd = NULL;
    if(!buf) entropy_get((buf = new uchar[1536]), 1536);
    uint32_t offs = 7337;
    loopi(16) fnv1a_add(offs, buf[i]);
    uchar *priv = buf + 24 + (offs & 0xfc), *prepriv = priv + 112 + ((offs >> 9) & 0xfc), res = 0;
    char hextemp[2 * preprivmaxlen + 1];

    if(passdrunning)
    {
        if(numargs < 0)
        {   // finish password decryption (background mode)
            privpwdcfg = 0;
            passdcleanup = passdrunning;
            passdrunning = NULL;
            if(sleepcmd) addsleep(0, sleepcmd);
            DELSTRING(sleepcmd);
        }
        else return;  // ignore further "authsetup" commands while a password decryption is running in the background
    }
    else
    {
        if(numargs < 0) return;  // should not happen
        else if(passdcleanup)
        {
            sl_waitthread(passdcleanup);
            passdcleanup = NULL;
        }
    }

    if(numargs > 0)
    {
        if(!strcasecmp(args[0], "PRE"))
        {
            // authsetup pre preprivhex [psalthex pwdcfg]
            preprivlen = numargs > 1 ? clamp(int(strlen(args[1])) / 2, preprivminlen, preprivmaxlen) : 32;
            if(numargs > 1) hex2bin(prepriv, args[1], preprivlen);
            if(numargs > 2) hex2bin(psalt, args[2], 16);
            preprivpwdcfg = numargs > 3 ? atoi(args[3]) : 0;
        }
        else if(!strcasecmp(args[0], "PRIV"))
        {
            // authsetup priv privhex [salthex pwdcfg]
            if(numargs > 1) hex2bin(priv, args[1], 32);
            if(numargs > 2) hex2bin(salt, args[2], 16);
            privpwdcfg = numargs > 3 ? atoi(args[3]) : 0;
            sk = NULL;
        }
        else if(!strcasecmp(args[0], "PUB"))
        {
            // authsetup pub pubhex
            if(numargs > 1) hex2bin(pub, args[1], 32);
            sk = NULL;
        }
        else if(!strcasecmp(args[0], "PPASS"))
        {
            // authsetup ppass preprivpass
            int iterations = preprivpwdcfg >> 10;
            if(iterations) passphrase2key(numargs > 1 ? args[1] : "", psalt, 16, keyhash, preprivlen, &iterations, 0, preprivpwdcfg & 0x3fe);
            xor_block(prepriv, keyhash, preprivlen);
            preprivpwdcfg = 0;
        }
        else if(!strcasecmp(args[0], "PASS"))
        {
            // authsetup pass privpass
            int iterations = privpwdcfg >> 10;
            if(iterations) passphrase2key(numargs > 1 ? args[1] : "", salt, 16, keyhash, 32, &iterations, 0, privpwdcfg & 0x3ff);
            xor_block(priv, keyhash, 32);
            privpwdcfg = 0;
        }
        else if(!strcasecmp(args[0], "PASSD"))
        {
            // authsetup passd privpass [commandwhendone]
            if(!passdrunning && privpwdcfg)
            {
                passdargs.salt = salt;
                passdargs.priv = priv;
                passdargs.pwdcfg = privpwdcfg;
                passdrunning = sl_createthread(passdeferred, newstring(numargs > 1 ? args[1] : ""));
                sleepcmd = numargs > 2 ? newstring(args[2]) : NULL;
            }
        }
        else if(!strcasecmp(args[0], "NEEDPASS"))
        {
            // authsetup needpass
            if(privpwdcfg) res = 1;
        }
        else if(!strcasecmp(args[0], "GENPRE"))
        {
            // authsetup genpre prelen
            preprivlen = clamp((numargs > 1) ? atoi(args[1]) : 42, preprivminlen, preprivmaxlen);
            entropy_get(prepriv, preprivlen);
            preprivpwdcfg = 0;
        }
        else if(!strcasecmp(args[0], "GENPRIV"))
        {
            // authsetup genpriv
            if(!preprivpwdcfg) privkey_from_prepriv(priv, prepriv, preprivlen);
            privpwdcfg = 0;
        }
        else if(!strcasecmp(args[0], "GENPUB"))
        {
            // authsetup genpub
            ed25519_pubkey_from_private(pub, priv);
            privpwdcfg = 0;
        }
        else if(!strcasecmp(args[0], "NEWPPASS"))
        {
            // authsetup newppass preprivpass [preprivfilename]
            if(preprivlen && !preprivpwdcfg)
            {
                if(numargs > 1 && args[1][0])  // new password
                {
                    entropy_get(psalt, 16);
                    int iterations = authrounds;
                    passphrase2key(args[1], psalt, 16, keyhash, preprivlen, &iterations, authmaxtime, authmemusage & 0x3fe);
                    preprivpwdcfg = (iterations << 10) | authmemusage;
                    xor_block(prepriv, keyhash, preprivlen);
                }
                const char *fn = numargs > 2 && args[2][0] ? path(args[2]) : AUTHPREPRIVATECFGFILE;
                char *oldfile = loadfile(fn, NULL);
                stream *f = openfile(fn, "wb");
                if(f)
                {
                    f->printf("\n"  "// remove this file from your computer immediately!\n"
                                    "// either print it out and delete it or move it to a thumbdrive.\n"
                                    "// YOU DO NOT NEED THIS FILE TO PLAY AC!\n" "\n");
                    if(oldfile) for(char *l = strtok(oldfile, "\n\r"); l; l = strtok(NULL, "\n\r")) if(*l) f->printf("// %s\n", l);
                    f->printf("authsetup pre %s", bin2hex(hextemp, prepriv, preprivlen));
                    if(preprivpwdcfg) f->printf(" %s %u", bin2hex(hextemp, psalt, 16), preprivpwdcfg & ~1);
                    f->printf("\n\n");
                    delete f;
                }
                DELETEA(oldfile);
            }
        }
        else if(!strcasecmp(args[0], "NEWPASS"))
        {
            // authsetup newpass privpass [privatefilename]
            if(!privpwdcfg)
            {
                ed25519_pubkey_from_private(keyhash + 32, priv);
                if(numargs > 1 && args[1][0])  // new password
                {
                    entropy_get(salt, 16);
                    int iterations = authrounds;
                    passphrase2key(args[1], salt, 16, keyhash, 32, &iterations, authmaxtime, authmemusage);
                    privpwdcfg = (iterations << 10) | authmemusage;
                    xor_block(priv, keyhash, 32);
                }
                const char *fn = numargs > 2 && args[2][0] ? path(args[2]) : AUTHPRIVATECFGFILE;
                char *oldfile = loadfile(fn, NULL);
                stream *f = openfile(fn, "wb");
                if(f)
                {
                    if(oldfile) for(char *l = strtok(oldfile, "\n\r"); l; l = strtok(NULL, "\n\r")) if(*l) f->printf("// %s\n", l);
                    f->printf("\nauthsetup priv %s", bin2hex(hextemp, priv, 32));
                    if(privpwdcfg) f->printf(" %s %u", bin2hex(hextemp, salt, 16), privpwdcfg);
                    f->printf("\nauthsetup pub %s\n\n", bin2hex(hextemp, keyhash + 32, 32));
                    delete f;
                }
                DELETEA(oldfile);
            }
        }
        else if(!strcasecmp(args[0], "UNARMED"))  // FIXME: this is for testing purposes only - to test simultaneous logins from the same account - delete before release!
        {
            // authsetup unarmed
            // pub aa55aab6d746c2000f3f6dc133c49c5d0485e0e44cbf036757e7945bca20106f  priv aa55aa55aa55aa55aa55aa55aa55aae5a973aa55aa55aa55aa55aa55aa55aa55
            hex2bin(priv, "aa55aa55aa55aa55aa55aa55aa55aae5a973aa55aa55aa55aa55aa55aa55aa55", 32);
            ed25519_pubkey_from_private(pub, priv);
            privpwdcfg = 0;
        }
    }
    ed25519_pubkey_from_private(keyhash, priv);
    if(!privpwdcfg && !memcmp(pub, keyhash, 32))
    {
        // priv matches pub: yay
        memcpy(priv + 32, pub, 32);
        sk = priv;
        if(!numargs) res = 1;  // "authsetup" with no arguments just checks the status
        DEBUG("successfully found keypair for pub " << bin2hex(hextemp, pub, 32));
    }
    else sk = NULL;
    intret(res);
}
COMMAND(authsetup, "v");
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




