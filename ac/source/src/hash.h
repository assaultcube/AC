/* Based off the reference implementation of Tiger, a cryptographically
 * secure 192 bit hash function by Ross Anderson and Eli Biham. More info at:
 * http://www.cs.technion.ac.il/~biham/Reports/Tiger/
 */

#define TIGER_PASSES 3

struct tiger
{
    static const int littleendian;

    typedef unsigned long long int chunk;

    union hashval
    {
        uchar bytes[3*8];
        chunk chunks[3];
    };

    static chunk sboxes[4*256];

    static void gensboxes()
    {
        const char *str = "Tiger - A Fast New Hash Function, by Ross Anderson and Eli Biham";
        chunk state[3] = { 0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL, 0xF096A5B4C3B2E187ULL };
        uchar temp[64];

        if(!*(const char *)&littleendian) loopj(64) temp[j^7] = str[j];
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

    static void compress(const chunk *str, chunk state[3])
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

#define round(a, b, c, x) \
      c ^= x; \
      a -= sb1[((c)>>(0*8))&0xFF] ^ sb2[((c)>>(2*8))&0xFF] ^ \
       sb3[((c)>>(4*8))&0xFF] ^ sb4[((c)>>(6*8))&0xFF] ; \
      b += sb4[((c)>>(1*8))&0xFF] ^ sb3[((c)>>(3*8))&0xFF] ^ \
       sb2[((c)>>(5*8))&0xFF] ^ sb1[((c)>>(7*8))&0xFF] ; \
      b *= mul;

            uint mul = !pass_no ? 5 : (pass_no==1 ? 7 : 9);
            round(a, b, c, x0) round(b, c, a, x1) round(c, a, b, x2) round(a, b, c, x3)
            round(b, c, a, x4) round(c, a, b, x5) round(a, b, c, x6) round(b, c, a, x7)

            chunk tmp = a; a = c; c = b; b = tmp;
        }

        a ^= aa;
        b -= bb;
        c += cc;

        state[0] = a;
        state[1] = b;
        state[2] = c;
    }

    static void hash(const uchar *str, int length, hashval &val)
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
            if(!*(const char *)&littleendian)
            {
                loopj(64) temp[j^7] = str[j];
                compress((chunk *)temp, val.chunks);
            }
            else compress((chunk *)str, val.chunks);
        }

        int j;
        if(!*(const char *)&littleendian)
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
    }
};

const int tiger::littleendian = 1;
tiger::chunk tiger::sboxes[4*256];

