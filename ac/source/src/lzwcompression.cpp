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
        loopi(256)
        {   
            lzwentry &e = add();
            e.data = new uchar(i);
            e.size = 1;
        }
    }

    void resettostaticentries()
    {
        // delete dynamic entries
        setsize(256);
    }
    
    virtual ~lzwdirectory()
    {
        resettostaticentries();
        //while(ulen) delete[] pop().data;
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
            for(int n = numbits-(8-bitoffset); n>0; n-=8)
            {
                put(0); // add new uchars if required
            }
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
                wc = w;
                wcsize = wsize+1;
            }
            else
            {
                wcsize = 1;
                wc = c;
            }

            lzwentry e = { wc, wcsize };
            int entry = dictionary.find(e);

            if(entry>=0) // found
            {
                // try further
                lzwentry &e = dictionary[entry];
                w = wc; //e.data;
                wsize = e.size;
            }
            else // does not exist yet
            {
                if(w) // add previous (known) entry to the output
                {
                    lzwentry e = { w, wsize };
                    int entry = dictionary.find(e);
                    ASSERT(entry>=0);
                    out.putuint(entry, fieldsize);
                    //conoutf("compressing value %d %c (size %d)", entry, c, fieldsize);
                }

                // add new entry to the dictionary
                lzwentry e = { wc, wcsize };
                dictionary.add(e);
                
                w = c;
                wsize = 1;

                // increase bitfieldsize
                int supporteddictsize = (1<<fieldsize);
                if(dictionary.length()>supporteddictsize) fieldsize++;
            }
        }

        // add remaining entry
        lzwentry e = { w, wsize };
        int entry = dictionary.find(e);
        ASSERT(entry>=0);
        out.putuint(entry, fieldsize);
        
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

            // new dictionary entry
            lzwentry newentry = { new uchar[wsize+1], wsize+1 };
            memcpy(newentry.data, w, wsize);
            newentry.data[wsize] = e.data[0];
            dictionary.add(newentry);

            w = e.data;
            wsize = e.size;

            // adjust bitfieldsize
            int supporteddictsize = (1<<fieldsize);
            if(dictionary.length()>=supporteddictsize) fieldsize++;
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
    const int NUMRUNS = 3;

    // input data
    uchar rndbuf[1024];
    loopi(1024) rndbuf[i] = (uchar)rand();
    s_sprintfd(txt)("TOBEORNOTTOBEORTOBEORNOT#");

    uchar *ibuf[] = { (uchar*)&txt, rndbuf };
    size_t len[] = { strlen(txt), 1024 };
    
    loopj(NUMRUNS)
    {
        conoutf("run %d", j);
        loopi(sizeof(ibuf)/sizeof(ibuf[0]))
        {
            int starttime, endtime;
            conoutf("starting phase %d", i);

            lzwbuffer inbuf(ibuf[i], len[i]);

            // compressed
            uchar cbuf[8*1024];
            memset(cbuf, 0, 8*1024);
            lzwbuffer compressed(cbuf, 8*1024);
            
            starttime = SDL_GetTicks();
            inbuf.compress(compressed);        
            endtime = SDL_GetTicks();
            conoutf("compressed %d bytes to %d bytes in %d milliseconds", len[i], compressed.len, endtime-starttime);

            // decompressed
            uchar dbuf[1024];
            memset(dbuf, 0, 1024);
            lzwbuffer decompressed(dbuf, 1024);

            starttime = SDL_GetTicks();
            compressed.decompress(decompressed);
            endtime = SDL_GetTicks();
            conoutf("decompressed %d bytes to %d bytes in %d milliseconds", compressed.len, decompressed.len, endtime-starttime);

            // verify
            ASSERT(!memcmp(ibuf[i], dbuf, len[i]));
        }
    }
}

COMMAND(testbitbuf, ARG_NONE);
COMMAND(testlzw, ARG_NONE);

#endif