/** 
 @file compress.c
 @brief An order-2 adaptive range coder
*/
#define ENET_BUILDING_LIB 1
#include <string.h>
#include "enet/enet.h"

typedef struct _ENetSymbol
{
    /* binary indexed tree of symbols */
    enet_uint8 value;
    enet_uint8 count;
    enet_uint16 under;
    enet_uint16 left, right;

    /* context defined by this symbol */
    enet_uint16 symbols;
    enet_uint16 escapes;
    enet_uint16 total;
    enet_uint16 parent; 
} ENetSymbol;

enum
{
    ENET_RANGE_CODER_TOP    = 1<<24,
    ENET_RANGE_CODER_BOTTOM = 1<<16,

    ENET_CONTEXT_SYMBOL_DELTA = 3,
    ENET_CONTEXT_SYMBOL_MINIMUM = 1,
    ENET_CONTEXT_ESCAPE_MINIMUM = 1,

    ENET_SUBCONTEXT_ORDER = 2,
    ENET_SUBCONTEXT_SYMBOL_DELTA = 2,
    ENET_SUBCONTEXT_ESCAPE_DELTA = 5
};

typedef struct _ENetRangeCoder
{
    /* only allocate enough symbols for reasonable MTUs */
    ENetSymbol symbols[4096];
} ENetRangeCoder;

void *
enet_range_coder_create (void)
{
    ENetRangeCoder * rangeCoder = (ENetRangeCoder *) enet_malloc (sizeof (ENetRangeCoder));
    if (rangeCoder == NULL)
      return NULL;

    return rangeCoder;
}

void
enet_range_coder_destroy (void * context)
{
    ENetRangeCoder * rangeCoder = (ENetRangeCoder *) context;
    if (rangeCoder == NULL)
      return;

    enet_free (rangeCoder);
}

#define ENET_SYMBOL_CREATE(symbol, value_, count_) \
{ \
    symbol = & rangeCoder -> symbols [nextSymbol ++]; \
    symbol -> value = value_; \
    symbol -> count = count_; \
    symbol -> under = count_; \
    symbol -> left = 0; \
    symbol -> right = 0; \
    symbol -> symbols = 0; \
    symbol -> escapes = 0; \
    symbol -> total = 0; \
    symbol -> parent = 0; \
}

#define ENET_CONTEXT_CREATE(context, escapes_, minimum) \
{ \
    ENET_SYMBOL_CREATE (context, 0, 0); \
    (context) -> escapes = escapes_; \
    (context) -> total = escapes_ + 256*minimum; \
    (context) -> symbols = 0; \
}

static enet_uint16
enet_symbol_rescale (ENetSymbol * symbol)
{
    enet_uint16 total = 0;
    for (;;)
    {
        symbol -> count -= symbol->count >> 1;
        symbol -> under = symbol -> count;
        if (symbol -> left)
          symbol -> under += enet_symbol_rescale (symbol + symbol -> left);
        total += symbol -> under;
        if (! symbol -> right) break;
        symbol += symbol -> right;
    } 
    return total;
}

#define ENET_CONTEXT_RESCALE(context, minimum) \
{ \
    (context) -> total = (context) -> symbols ? enet_symbol_rescale ((context) + (context) -> symbols) : 0; \
    (context) -> escapes -= (context) -> escapes >> 1; \
    (context) -> total += (context) -> escapes + 256*minimum; \
}

#define ENET_RANGE_CODER_OUTPUT(value) \
{ \
    if (outData >= outEnd) \
      return 0; \
    * outData ++ = value; \
}

#define ENET_RANGE_CODER_ENCODE(under, count, total) \
{ \
    encodeRange /= (total); \
    encodeLow += (under) * encodeRange; \
    encodeRange *= (count); \
    for (;;) \
    { \
        if((encodeLow ^ (encodeLow + encodeRange)) >= ENET_RANGE_CODER_TOP) \
        { \
            if(encodeRange >= ENET_RANGE_CODER_BOTTOM) break; \
            encodeRange = -encodeLow & (ENET_RANGE_CODER_BOTTOM - 1); \
        } \
        ENET_RANGE_CODER_OUTPUT (encodeLow >> 24); \
        encodeRange <<= 8; \
        encodeLow <<= 8; \
    } \
}

#define ENET_RANGE_CODER_FLUSH \
{ \
    while (encodeLow) \
    { \
        ENET_RANGE_CODER_OUTPUT (encodeLow >> 24); \
        encodeLow <<= 8; \
    } \
}

#define ENET_RANGE_CODER_FREE_SYMBOLS \
{ \
    if (nextSymbol >= sizeof (rangeCoder -> symbols) / sizeof (ENetSymbol) - ENET_SUBCONTEXT_ORDER) \
    { \
        nextSymbol = 0; \
        ENET_CONTEXT_CREATE (root, ENET_CONTEXT_ESCAPE_MINIMUM, ENET_CONTEXT_SYMBOL_MINIMUM); \
        predicted = 0; \
        order = 0; \
    } \
}

#define ENET_CONTEXT_ENCODE(context, symbol_, value_, under_, count_, update) \
{ \
    ENetSymbol * child; \
    under_ = 0; \
    count_ = 0; \
    if (! (context) -> symbols) \
    { \
        ENET_SYMBOL_CREATE (child, value_, update); \
        (context) -> symbols = child - (context); \
        symbol_ = child; \
    } \
    else \
    { \
        ENetSymbol * node = (context) + (context) -> symbols; \
        for (;;) \
        { \
            if (value_ < node -> value) \
            { \
                node -> under += update; \
                if (node -> left) { node += node -> left; continue; } \
                ENET_SYMBOL_CREATE (child, value_, update); \
                node -> left = child - node; \
                symbol_ = child; \
            } \
            else \
            if (value_ > node -> value) \
            { \
                under_ += node -> under; \
                if (node -> right) { node += node -> right; continue; } \
                ENET_SYMBOL_CREATE (child, value_, update); \
                node -> right = child - node; \
                symbol_ = child; \
            } \
            else \
            { \
                count_ = node -> count; \
                under_ += node -> under - count_; \
                node -> under += update; \
                node -> count += update; \
                symbol_ = node; \
            } \
            break; \
        } \
    } \
}

size_t
enet_range_coder_compress (void * context, const ENetBuffer * inBuffers, size_t inBufferCount, size_t inLimit, enet_uint8 * outData, size_t outLimit)
{
    ENetRangeCoder * rangeCoder = (ENetRangeCoder *) context;
    enet_uint8 * outStart = outData, * outEnd = & outData [outLimit];
    const enet_uint8 * inData, * inEnd;
    enet_uint32 encodeLow = 0, encodeRange = ~0;
    ENetSymbol * root;
    enet_uint16 predicted = 0;
    size_t order = 0, nextSymbol = 0;

    if (rangeCoder == NULL || inBufferCount <= 0 || inLimit <= 0)
      return 0;

    inData = (const enet_uint8 *) inBuffers -> data;
    inEnd = & inData [inBuffers -> dataLength];
    inBuffers ++;
    inBufferCount --;

    ENET_CONTEXT_CREATE (root, ENET_CONTEXT_ESCAPE_MINIMUM, ENET_CONTEXT_SYMBOL_MINIMUM);

    for (;;)
    {
        ENetSymbol * subcontext, * symbol;
        enet_uint8 value;
        enet_uint16 under, count, * parent = & predicted;
        size_t level = order;
        if (inData >= inEnd)
        {
            if (inBufferCount <= 0)
              break;
            inData = (const enet_uint8 *) inBuffers -> data;
            inEnd = & inData [inBuffers -> dataLength];
            inBuffers ++;
            inBufferCount --;
        }
        value = * inData ++;

        for (subcontext = & rangeCoder -> symbols [predicted]; 
             subcontext != root; 
             subcontext = & rangeCoder -> symbols [subcontext -> parent])
        {
            ENET_CONTEXT_ENCODE (subcontext, symbol, value, under, count, ENET_SUBCONTEXT_SYMBOL_DELTA);
            * parent = symbol - rangeCoder -> symbols;
            parent = & symbol -> parent;
            if (count > 0)
            {
                ENET_RANGE_CODER_ENCODE (subcontext -> escapes + under, count, subcontext -> total);
            }
            else
            {
                if (subcontext -> escapes > 0)
                    ENET_RANGE_CODER_ENCODE (0, subcontext -> escapes, subcontext -> total);
                subcontext -> escapes += ENET_SUBCONTEXT_ESCAPE_DELTA;
                subcontext -> total += ENET_SUBCONTEXT_ESCAPE_DELTA;
            }
            subcontext -> total += ENET_SUBCONTEXT_SYMBOL_DELTA;
            if (count > 0xFF - 2*ENET_SUBCONTEXT_SYMBOL_DELTA || subcontext -> total > ENET_RANGE_CODER_BOTTOM - 0x100)
              ENET_CONTEXT_RESCALE (subcontext, 0);
            if (count > 0) goto nextInput;
        }

        ENET_CONTEXT_ENCODE (root, symbol, value, under, count, ENET_CONTEXT_SYMBOL_DELTA);
        * parent = symbol - rangeCoder -> symbols;
        parent = & symbol -> parent;
        ENET_RANGE_CODER_ENCODE (root -> escapes + under + value*ENET_CONTEXT_SYMBOL_MINIMUM, count + ENET_CONTEXT_SYMBOL_MINIMUM, root -> total);
        root -> total += ENET_CONTEXT_SYMBOL_DELTA; 
        if (count > 0xFF - 2*ENET_CONTEXT_SYMBOL_DELTA || root -> total > ENET_RANGE_CODER_BOTTOM - 0x100)
          ENET_CONTEXT_RESCALE (root, ENET_CONTEXT_SYMBOL_MINIMUM);

    nextInput:
        if (order >= ENET_SUBCONTEXT_ORDER) 
          predicted = rangeCoder -> symbols [predicted].parent;
        else 
          order ++;
        ENET_RANGE_CODER_FREE_SYMBOLS;
    }

    ENET_RANGE_CODER_FLUSH;

    return (size_t) (outData - outStart);
}

#define ENET_RANGE_CODER_SEED \
{ \
    if (inData < inEnd) decodeCode |= * inData ++ << 24; \
    if (inData < inEnd) decodeCode |= * inData ++ << 16; \
    if (inData < inEnd) decodeCode |= * inData ++ << 8; \
    if (inData < inEnd) decodeCode |= * inData ++; \
}

#define ENET_RANGE_CODER_READ(total) ((decodeCode - decodeLow) / (decodeRange /= (total)))

#define ENET_RANGE_CODER_DECODE(under, count, total) \
{ \
    decodeLow += (under) * decodeRange; \
    decodeRange *= (count); \
    for (;;) \
    { \
        if((decodeLow ^ (decodeLow + decodeRange)) >= ENET_RANGE_CODER_TOP) \
        { \
            if(decodeRange >= ENET_RANGE_CODER_BOTTOM) break; \
            decodeRange = -decodeLow & (ENET_RANGE_CODER_BOTTOM - 1); \
        } \
        decodeCode <<= 8; \
        if (inData < inEnd) \
          decodeCode |= * inData ++; \
        decodeRange <<= 8; \
        decodeLow <<= 8; \
    } \
}

#define ENET_CONTEXT_TRY_DECODE(context, symbol_, code, value_, under_, count_, update) \
{ \
    under_ = 0; \
    count_ = 0; \
    if (! (context) -> symbols) \
      return 0; \
    else \
    { \
        ENetSymbol * node = (context) + (context) -> symbols; \
        for (;;) \
        { \
            if (code >= under_ + node -> under) \
            { \
                under_ += node -> under; \
                if (! node -> right) return 0; \
                node += node -> right; \
            } \
            else \
            if (code < under_ + node -> under - node -> count) \
            { \
                node -> under += update; \
                if (! node -> left) return 0; \
                node += node -> left; \
            } \
            else \
            { \
                value_ = node -> value; \
                count_ = node -> count; \
                under_ += node -> under - count_; \
                node -> under += update; \
                node -> count += update; \
                symbol_ = node; \
                break; \
            } \
        } \
    } \
}

#define ENET_CONTEXT_DECODE(context, symbol_, code, value_, under_, count_, update, minimum) \
{ \
    ENetSymbol * child; \
    int lowValue = -1; \
    under_ = 0; \
    count_ = 0; \
    if (! (context) -> symbols) \
    { \
        value_ = code / minimum; \
        ENET_SYMBOL_CREATE (child, value_, update); \
        (context) -> symbols = child - (context); \
        symbol_ = child; \
    } \
    else \
    { \
        ENetSymbol * node = (context) + (context) -> symbols; \
        for (;;) \
        { \
            enet_uint16 total = under_ + node -> under + (node -> value + 1)*minimum; \
            if (code >= total) \
            { \
                under_ += node -> under; \
                lowValue = node -> value; \
                if (node -> right) { node += node -> right; continue; } \
                value_ = lowValue + 1 + (code - under_ - (lowValue + 1)*minimum) / minimum; \
                ENET_SYMBOL_CREATE (child, value_, update); \
                node -> right = child - node; \
                symbol_ = child; \
            } \
            else \
            if (code < total - node -> count - minimum) \
            { \
                node -> under += update; \
                if (node -> left) { node += node -> left; continue; } \
                value_ = lowValue + 1 + (code - under_ - (lowValue + 1)*minimum) / minimum; \
                ENET_SYMBOL_CREATE (child, value_, update); \
                node -> left = child - node; \
                symbol_ = child; \
            } \
            else \
            { \
                value_ = node -> value; \
                count_ = node -> count; \
                under_ += node -> under - count_; \
                node -> under += update; \
                node -> count += update; \
                symbol_ = node; \
            } \
            break; \
        } \
    } \
}

size_t
enet_range_coder_decompress (void * context, const enet_uint8 * inData, size_t inLimit, enet_uint8 * outData, size_t outLimit)
{
    ENetRangeCoder * rangeCoder = (ENetRangeCoder *) context;
    enet_uint8 * outStart = outData, * outEnd = & outData [outLimit];
    const enet_uint8 * inEnd = & inData [inLimit];
    enet_uint32 decodeLow = 0, decodeCode = 0, decodeRange = ~0;
    ENetSymbol * root;
    enet_uint16 predicted = 0;
    size_t order = 0, nextSymbol = 0;

    if (rangeCoder == NULL || inLimit <= 0)
      return 0;

    ENET_CONTEXT_CREATE (root, ENET_CONTEXT_ESCAPE_MINIMUM, ENET_CONTEXT_SYMBOL_MINIMUM);

    ENET_RANGE_CODER_SEED;

    for (;;)
    {
        ENetSymbol * subcontext, * symbol, * patch;
        enet_uint8 value = 0;
        enet_uint16 code, under, count, bottom, * parent = & predicted;

        for (subcontext = & rangeCoder -> symbols [predicted];
             subcontext != root;
             subcontext = & rangeCoder -> symbols [subcontext -> parent])
        {
            if (subcontext -> escapes <= 0)
              continue;
            code = ENET_RANGE_CODER_READ (subcontext -> total);
            if (code < subcontext -> escapes) 
            {
                ENET_RANGE_CODER_DECODE (0, subcontext -> escapes, subcontext -> total);
                continue;
            }
            code -= subcontext -> escapes;
            ENET_CONTEXT_TRY_DECODE (subcontext, symbol, code, value, under, count, ENET_SUBCONTEXT_SYMBOL_DELTA);
            bottom = symbol - rangeCoder -> symbols;
            ENET_RANGE_CODER_DECODE (subcontext -> escapes + under, count, subcontext -> total);
            subcontext -> total += ENET_SUBCONTEXT_SYMBOL_DELTA;
            if (count > 0xFF - 2*ENET_SUBCONTEXT_SYMBOL_DELTA || subcontext -> total > ENET_RANGE_CODER_BOTTOM - 0x100)
              ENET_CONTEXT_RESCALE (subcontext, 0);
            goto patchContexts;
        }

        code = ENET_RANGE_CODER_READ (root -> total);
        if (code < root -> escapes)
        {
            ENET_RANGE_CODER_DECODE (0, root -> escapes, root -> total);
            break;
        }
        code -= root -> escapes;
        ENET_CONTEXT_DECODE (root, symbol, code, value, under, count, ENET_CONTEXT_SYMBOL_DELTA, ENET_CONTEXT_SYMBOL_MINIMUM);
        bottom = symbol - rangeCoder -> symbols;
        ENET_RANGE_CODER_DECODE (root -> escapes + under + value*ENET_CONTEXT_SYMBOL_MINIMUM, count + ENET_CONTEXT_SYMBOL_MINIMUM, root -> total);
        root -> total += ENET_CONTEXT_SYMBOL_DELTA;
        if (count > 0xFF - 2*ENET_CONTEXT_SYMBOL_DELTA || root -> total > ENET_RANGE_CODER_BOTTOM - 0x100)
          ENET_CONTEXT_RESCALE (root, ENET_CONTEXT_SYMBOL_MINIMUM);

    patchContexts:
        for (patch = & rangeCoder -> symbols [predicted];
             patch != subcontext;
             patch = & rangeCoder -> symbols [patch -> parent])
        {
            ENET_CONTEXT_ENCODE (patch, symbol, value, under, count, ENET_SUBCONTEXT_SYMBOL_DELTA);
            * parent = symbol - rangeCoder -> symbols;
            parent = & symbol -> parent;
            if (count <= 0)
            {
                patch -> escapes += ENET_SUBCONTEXT_ESCAPE_DELTA;
                patch -> total += ENET_SUBCONTEXT_ESCAPE_DELTA;
            }
            patch -> total += ENET_SUBCONTEXT_SYMBOL_DELTA; 
            if (count > 0xFF - 2*ENET_SUBCONTEXT_SYMBOL_DELTA || patch -> total > ENET_RANGE_CODER_BOTTOM - 0x100)
              ENET_CONTEXT_RESCALE (patch, 0);
        }
        * parent = bottom;

        ENET_RANGE_CODER_OUTPUT (value);

        if (order >= ENET_SUBCONTEXT_ORDER)
          predicted = rangeCoder -> symbols [predicted].parent;
        else
          order ++;
        ENET_RANGE_CODER_FREE_SYMBOLS;
    }
                        
    return (size_t) (outData - outStart);
}

/** Sets the packet compressor the host should use to the default range coder.
    @param host host to enable the range coder for
    @returns 0 on success, < 0 on failure
*/
int
enet_host_compress_with_range_coder (ENetHost * host)
{
    ENetCompressor compressor;
    memset (& compressor, 0, sizeof (compressor));
    compressor.context = enet_range_coder_create();
    if (compressor.context == NULL)
      return -1;
    compressor.compress = enet_range_coder_compress;
    compressor.decompress = enet_range_coder_decompress;
    compressor.destroy = enet_range_coder_destroy;
    enet_host_compress (host, & compressor);
    return 0;
}
    
    
     
