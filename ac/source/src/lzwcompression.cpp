// Lempel-Ziv-Welch compression
// using dynamic fieldsize

#include "pch.h"
#include "cube.h"

struct lzwentry
{
    uchar *data;
    size_t size;
    
    bool operator==(const lzwentry &other)
    {
        return size==other.size && !memcmp(data, other.data, size);
    }
};

struct lzwdirectory : vector<lzwentry>
{
    int find(uchar *v, size_t size) // find by value
    {
        if(size==1) // predefined dictionary up to value '255'
        {
            return v[0];
        }
        else // own dynamic values
        {
            lzwentry e;
            e.data = v;
            e.size = size;
            loopv(*this)
            {
                lzwentry *entry = &buf[i];
                if(entry && *entry==e) return i+256;
            }
            return -1;
        }
    }

    bool lzwentryexists(int eidx)
    {
        if(eidx<=255) return true;
        else return inrange(eidx-256);
    }
};

// allows bit-wise access
struct bitbuf : databuf<uchar>
{
    size_t bitoffset;
    bitbuf(uchar *buf, size_t maxlen) : databuf(buf, maxlen), bitoffset(0) {};
    virtual ~bitbuf() {};

    void putuchar(uint value, int numbits)
    {
        ASSERT(numbits && numbits<=8);
        
        if(bitoffset) // append to last uchar
        {
            ushort *b = (ushort *)(&buf[len-1]); // spans 2 uchars
            for(int n = numbits-(8-bitoffset); n>0; n-=8) put(0); // add new uchars if required
            *b |= (value & ((1<<numbits)-1))<<bitoffset; // append to bitstream
            bitoffset = (bitoffset+numbits) % 8; // update offset
        }
        else // add new uchar
        {
            put(value & ((1<<numbits)-1));
            if(numbits<8) bitoffset = numbits;
        }
    }

    void putuint(uint value, int numbits)
    {
        putuchar(value&0xFF, min(8, numbits));
        if(numbits>8) putuchar((value>>8)&0xFF, min(8, numbits-8));
        if(numbits>16) putuchar((value>>16)&0xFF, min(8, numbits-16));
        if(numbits>24) putuchar((value>>24)&0xFF, min(8, numbits-24));
    }

    uchar getuchar(int numbits)
    {
        ASSERT(numbits && numbits<=8);

        uchar out = 0;
        if(bitoffset)
        {
            ushort *b = (ushort *)(&buf[len-1]);
            for(int n = numbits-(8-bitoffset); n>0; n-=8) get();
            out = (*b>>bitoffset) & ((1<<numbits)-1);
            bitoffset = (bitoffset+numbits) % 8;
        }
        else
        {
            out = get() & ((1<<numbits)-1);
            if(numbits<8) bitoffset = numbits;
        }
        return out;
    }

    uint getuint(int numbits)
    {
        uint out = getuchar(min(8, numbits));
        if(numbits>8) out |= getuchar(min(8, numbits-8))<<8;
        if(numbits>16) out |= getuchar(min(8, numbits-16))<<16;
        if(numbits>24) out |= getuchar(min(8, numbits-24))<<24;
        return out;
    }
};

struct lzwbuffer : bitbuf
{
    lzwdirectory dictionary;

    lzwbuffer(uchar *buf, size_t maxlen) : bitbuf(buf, maxlen) {};
    virtual ~lzwbuffer()
    {
    }

    lzwbuffer compress()
    {
        uchar *obuf = new uchar[1024];
        memset(obuf, 0, 1024);
        lzwbuffer out(obuf, 1024);
        
        uchar *w = NULL;
        size_t wsize = 0;
        size_t fieldsize = 9;

        for(uchar *c = &buf[0]; c<&buf[len]; c++)
        {
            uchar *wc = NULL;
            size_t wcsize;
            if(wsize)
            {
                wcsize = wsize+1;
                wc = new uchar[wcsize];
                memcpy(wc, w, wsize);
                wc[wsize] = *c;
            }
            else
            {
                wcsize = 1;
                wc = new uchar[wcsize];
                wc[0] = *c;
            }

            // search in dict
            int entry = dictionary.find(wc, wcsize);

            if(entry>=0) // found
            {
                // try further
                w = wc;
                wsize = wcsize;
            }
            else // does not exist yet
            {
                if(w) // add previous (known) entry to the output
                {
                    int entry = dictionary.find(w, wsize);
                    ASSERT(entry>=0);
                    out.putuint((uint)entry, fieldsize);

                    char c = ' ';
                    if(wsize<=128) c = w[0];
                    conoutf("compressing value %d %c (size %d)", entry, c, fieldsize);
                }
                // add new entry to the dictionary
                lzwentry e = { wc, wcsize };
                dictionary.add(e);
                
                // reset
                w = c;
                wsize = 1;
            }

            // increase bitfieldsize
            int supporteddictsize = (1<<fieldsize)-1;
            if(dictionary.length()>=supporteddictsize) fieldsize++;
        }

        // add remaining entry
        int entry = dictionary.find(w, wsize);
        out.putuint(entry, fieldsize);

        char c = ' ';
        if(wsize<=128) c = w[0];
        conoutf("compressing value %d %c (size %d)", entry, c, fieldsize);

        out.len = out.bitoffset = 0;
        return out;
    }

    lzwbuffer decompress()
    {
        uchar *obuf = new uchar[1024];
        memset(obuf, 0, 1024);
        lzwbuffer out(obuf, 1024);

        size_t fieldsize = 9;
        uchar tmp = this->getuint(fieldsize);
        uchar *w = &tmp;
        size_t wsize = 1;

        out.put(*w);
        conoutf("decompressing value %d %c (size %d)", *w, *w, fieldsize);

        for(uint k = this->getuint(fieldsize); !overread(); k = this->getuint(fieldsize))
        {
            lzwentry e;
            if(dictionary.lzwentryexists(k))
            {
                // map index to our dictionary
                if(k<=255) // predefined values, not stored in our dictionary
                {
                    e = lzwentry();
                    e.data = new uchar(k);
                    e.size = 1;
                }
                else e = dictionary[k-256]; // map indices above 255
            }
            else if(k==dictionary.length())
            {
                e = lzwentry();
                e.size = wsize+1;
                e.data = new uchar[e.size];
                memcpy(e.data, w, wsize);
                e.data[wsize] = w[0];
            }
            else ASSERT(0);
            
            // add to output
            out.put(*e.data);
            conoutf("decompressing value %d %c (size %d)", k, *e.data, fieldsize);

            // new dictionary entry
            lzwentry newentry = { new uchar[wsize+1], wsize+1 };
            memcpy(newentry.data, w, wsize);
            newentry.data[wsize] = e.data[0];
            dictionary.add(newentry);

            // adjust bitfieldsize
            int supporteddictsize = (1<<fieldsize)-1;
            if(dictionary.length()>=supporteddictsize) 
            {
                fieldsize++;
            }

            w = e.data;
            wsize = e.size;
        }

        out.len = out.bitoffset = 0;
        return out;
    }
};

#ifdef _DEBUG

void testbitbuf()
{
    // test bitbuf struct using different fieldsizes and random data
    loopi(32)
    {
        uchar buf[1024] = { 0 };
        lzwbuffer p(buf, 1024);

        size_t fieldsize = i+1;

        // create random test data
        vector<uint> input;
        loopj(256)
        {
            uint v = rand();
            if(fieldsize!=32) v = v % (1<<fieldsize);
            input.add(v);
        }

        // store data
        loopvk(input) p.putuint(input[k], fieldsize);

        // retrieve from storage
        lzwbuffer b(buf, p.length());
        vector<uint> output;
        while(true)
        {
            uint o = b.getuint(fieldsize);
            if(b.overread()) break;
            output.add(o);
        }

        // verify result
        ASSERT(input.length()==output.length());
        loopvk(input)
        {
            ASSERT(input[k] == output[k]);
            conoutf("success: storing value %d (size %d)", input[k], fieldsize);
        }
    }
    conoutf("done");
}

// test LZW compression
void testlzw()
{
    s_sprintfd(txt)("TOBEORNOTTOBEORTOBEORNOT#");
    lzwbuffer p((uchar*)&txt, strlen(txt));
    p.len = strlen(txt);

    lzwbuffer compressed = p.compress();
    uchar *r = compressed.buf;

    lzwbuffer decompressed = compressed.decompress();
    uchar *r2 = decompressed.buf;
}

COMMAND(testbitbuf, ARG_NONE);
COMMAND(testlzw, ARG_NONE);


#endif