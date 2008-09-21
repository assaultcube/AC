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
    void fillstaticentries()
    {
        // build static dictionary
        // FIXME: optimize access
        loopi(255)
        {   
            lzwentry &e = add();
            e.data = new uchar(i);
            e.size = 1;
        }
    }

    void resettostaticentries()
    {
        // delete dynamic entries
        while(length()>255) delete[] pop().data;
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

    lzwbuffer(uchar *buf, size_t maxlen) : bitbuf(buf, maxlen)
    {
        dictionary.fillstaticentries();
    }

    virtual ~lzwbuffer()
    {
    }

    void compress(lzwbuffer &out)
    {   
        lzwbuffer b(buf, maxlen);
        b.len = maxlen;

        uchar *w = NULL;
        size_t wsize = 0;
        size_t fieldsize = 9;

        for(uchar *c = &b.buf[0]; c<&b.buf[b.len]; c++)
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

            lzwentry e = { wc, wcsize };
            int entry = dictionary.find(e);

            if(entry>=0) // found
            {
                // try further
                lzwentry &e = dictionary[entry];
                w = e.data;
                wsize = e.size;
                DELETEA(wc);
            }
            else // does not exist yet
            {
                if(w) // add previous (known) entry to the output
                {
                    lzwentry e = { w, wsize };
                    int entry = dictionary.find(e);
                    ASSERT(entry>=0);
                    out.putuint((uint)entry, fieldsize);

                    char c = ' ';
                    if(wsize<=128) c = w[0];
                    conoutf("compressing value %d %c (size %d)", entry, c, fieldsize);
                }

                // add new entry to the dictionary
                lzwentry e = dictionary.add();
                e.data = wc;
                e.size = wcsize;
                
                w = c;
                wsize = 1;
            }

            // increase bitfieldsize
            int supporteddictsize = (1<<fieldsize)-1;
            if(dictionary.length()>=supporteddictsize) fieldsize++;
        }

        // add remaining entry
        lzwentry e = { w, wsize };
        int entry = dictionary.find(e);
        out.putuint(entry, fieldsize);

        char c = ' ';
        if(wsize<=128) c = w[0];
        conoutf("compressing value %d %c (size %d)", entry, c, fieldsize);
        
        dictionary.resettostaticentries();
    }

    void decompress(lzwbuffer &out)
    {
        lzwbuffer b(buf, maxlen);
        b.maxlen = len;

        size_t fieldsize = 9;
        uchar tmp = b.getuint(fieldsize);
        uchar *w = &tmp;
        size_t wsize = 1;

        out.put(*w);
        conoutf("decompressing value %d %c (size %d)", *w, *w, fieldsize);

        for(uint k = b.getuint(fieldsize); !b.overread(); k = b.getuint(fieldsize))
        {
            lzwentry e;
            if(dictionary.inrange(k))
            {
                e = dictionary[k];
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
            out.put(e.data, e.size);
            conoutf("decompressing value %d %c (size %d)", k, *e.data, fieldsize);

            // new dictionary entry
            lzwentry newentry = { new uchar[wsize+1], wsize+1 };
            memcpy(newentry.data, w, wsize);
            newentry.data[wsize] = e.data[0];
            dictionary.add(newentry);

            // adjust bitfieldsize
            int supporteddictsize = (1<<fieldsize)-1;
            if(dictionary.length()>=supporteddictsize) fieldsize++;

            w = e.data;
            wsize = e.size;
        }

        dictionary.resettostaticentries();
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
    lzwbuffer inbuf((uchar*)&txt, strlen(txt));
    
    conoutf("compressing data: %s", txt);

    uchar cbuf[1024];
    memset(cbuf, 0, 1024);
    lzwbuffer compressed(cbuf, 1024);
    inbuf.compress(compressed);
    uchar *r = compressed.buf;

    uchar dbuf[1024];
    memset(dbuf, 0, 1024);
    lzwbuffer decompress(dbuf, 1024);
    compressed.decompress(decompress);
    uchar *r2 = decompress.buf;
    conoutf("uncompressed data: %s", (char*)r2);

    ASSERT(!strcmp((char*)r2, txt));
}

COMMAND(testbitbuf, ARG_NONE);
COMMAND(testlzw, ARG_NONE);

#endif