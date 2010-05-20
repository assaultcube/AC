/** 
 @file compress.c
 @brief An order-1 adaptive range coder
*/
#define ENET_BUILDING_LIB 1
#include <string.h>
#include "enet/enet.h"

typedef struct _ENetSymbol
{
    enet_uint8 value;
    enet_uint8 count;
    enet_uint16 total;
    enet_uint16 left, right;
} ENetSymbol;

typedef struct _ENetPredictor
{
    enet_uint16 escapes;
    enet_uint16 total;
    ENetSymbol *symbols;
} ENetPredictor;

enum
{
    ENET_RANGE_CODER_TOP    = 1<<24,
    ENET_RANGE_CODER_BOTTOM = 1<<16,

    ENET_CONTEXT_SYMBOL_DELTA = 3,
    ENET_CONTEXT_SYMBOL_MINIMUM = 1,
    ENET_CONTEXT_ESCAPE_MINIMUM = 1,

    ENET_SUBCONTEXT_SYMBOL_DELTA = 2,
    ENET_SUBCONTEXT_ESCAPE_DELTA = 3
};

typedef struct _ENetRangeCoder
{
    ENetPredictor context, subcontexts[256];
    ENetSymbol symbols[256 + ENET_PROTOCOL_MAXIMUM_MTU + 1];
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

#define ENET_PREDICTOR_CREATE(predictor, escapes_, minimum) \
{ \
    (predictor).escapes = escapes_; \
    (predictor).total = escapes_ + 256*minimum; \
    (predictor).symbols = NULL; \
}

#define ENET_SYMBOL_CREATE(symbol, value_, count_) \
{ \
    symbol = & rangeCoder -> symbols [nextSymbol ++]; \
    symbol -> value = value_; \
    symbol -> count = count_; \
    symbol -> total = count_; \
    symbol -> left = 0; \
    symbol -> right = 0; \
}

static enet_uint16
enet_symbol_rescale (ENetSymbol * symbol)
{
    enet_uint16 total = 0;
    for (;;)
    {
        symbol -> count -= symbol->count >> 1;
        symbol -> total = symbol -> count;
        if (symbol -> left)
          symbol -> total += enet_symbol_rescale (symbol + symbol -> left);
        total += symbol -> total;
        if (! symbol -> right) break;
        symbol += symbol -> right;
    } 
    return total;
}

#define ENET_PREDICTOR_RESCALE(predictor, minimum) \
{ \
    (predictor).total = (predictor).symbols != NULL ? enet_symbol_rescale ((predictor).symbols) : 0; \
    (predictor).escapes -= (predictor).escapes >> 1; \
    (predictor).total += (predictor).escapes + 256*minimum; \
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
    if (nextSymbol + 1 >= sizeof (rangeCoder -> symbols) / sizeof (ENetSymbol)) \
    { \
        ENET_PREDICTOR_CREATE (rangeCoder -> context, ENET_CONTEXT_ESCAPE_MINIMUM, ENET_CONTEXT_SYMBOL_MINIMUM); \
        subcontext = NULL; \
        nextSymbol = 0; \
    } \
}

#define ENET_PREDICTOR_ENCODE(predictor, value_, under_, count_, update) \
{ \
    under_ = 0; \
    count_ = 0; \
    if ((predictor).symbols == NULL) \
    { \
        ENET_SYMBOL_CREATE ((predictor).symbols, value_, update); \
    } \
    else \
    { \
        ENetSymbol * symbol = (predictor).symbols, * child; \
        for (;;) \
        { \
            if (value_ < symbol -> value) \
            { \
                symbol -> total += update; \
                if (symbol -> left) { symbol += symbol -> left; continue; } \
                ENET_SYMBOL_CREATE (child, value_, update); \
                symbol -> left = child - symbol; \
            } \
            else \
            if (value_ > symbol -> value) \
            { \
                under_ += symbol -> total; \
                if (symbol -> right) { symbol += symbol -> right; continue; } \
                ENET_SYMBOL_CREATE (child, value_, update); \
                symbol -> right = child - symbol; \
            } \
            else \
            { \
                count_ = symbol -> count; \
                under_ += symbol -> total - count_; \
                symbol -> total += update; \
                symbol -> count += update; \
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
    ENetPredictor * subcontext = NULL;
    size_t nextSymbol = 0;

    if (rangeCoder == NULL || inBufferCount <= 0 || inLimit <= 0)
      return 0;

    inData = (const enet_uint8 *) inBuffers -> data;
    inEnd = & inData [inBuffers -> dataLength];
    inBuffers ++;
    inBufferCount --;

    ENET_PREDICTOR_CREATE (rangeCoder -> context, ENET_CONTEXT_ESCAPE_MINIMUM, ENET_CONTEXT_SYMBOL_MINIMUM);

    for (;;)
    {
        enet_uint8 value;
        enet_uint16 under, count;
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

        if (subcontext != NULL)
        {
            ENET_PREDICTOR_ENCODE (* subcontext, value, under, count, ENET_SUBCONTEXT_SYMBOL_DELTA);
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
              ENET_PREDICTOR_RESCALE (* subcontext, 0);
            if (count <= 0)
              goto parentContext;
        }
        else
        {
        parentContext:
            ENET_PREDICTOR_ENCODE (rangeCoder -> context, value, under, count, ENET_CONTEXT_SYMBOL_DELTA);
            if (count <= 0)
              ENET_PREDICTOR_CREATE (rangeCoder -> subcontexts [value], 0, 0);
            ENET_RANGE_CODER_ENCODE (rangeCoder -> context.escapes + under + value*ENET_CONTEXT_SYMBOL_MINIMUM, count + ENET_CONTEXT_SYMBOL_MINIMUM, rangeCoder -> context.total);
            rangeCoder -> context.total += ENET_CONTEXT_SYMBOL_DELTA; 
            if (count > 0xFF - 2*ENET_CONTEXT_SYMBOL_DELTA || rangeCoder -> context.total > ENET_RANGE_CODER_BOTTOM - 0x100)
              ENET_PREDICTOR_RESCALE (rangeCoder -> context, ENET_CONTEXT_SYMBOL_MINIMUM);
        }

        subcontext = & rangeCoder -> subcontexts [value];
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

#define ENET_PREDICTOR_TRY_DECODE(predictor, code, value_, under_, count_, update) \
{ \
    under_ = 0; \
    count_ = 0; \
    if ((predictor).symbols == NULL) \
      return 0; \
    else \
    { \
        ENetSymbol * symbol = (predictor).symbols, * child; \
        for (;;) \
        { \
            if (code >= under_ + symbol -> total) \
            { \
                under_ += symbol -> total; \
                if (! symbol -> right) return 0; \
                symbol += symbol -> right; \
            } \
            else \
            if (code < under_ + symbol -> total - symbol -> count) \
            { \
                symbol -> total += update; \
                if (! symbol -> left) return 0; \
                symbol += symbol -> left; \
            } \
            else \
            { \
                value_ = symbol -> value; \
                count_ = symbol -> count; \
                under_ += symbol -> total - count_; \
                symbol -> total += update; \
                symbol -> count += update; \
                break; \
            } \
        } \
    } \
}

#define ENET_PREDICTOR_DECODE(predictor, code, value_, under_, count_, update, minimum) \
{ \
    int lowValue = -1; \
    under_ = 0; \
    count_ = 0; \
    if ((predictor).symbols == NULL) \
    { \
        value_ = code / minimum; \
        ENET_SYMBOL_CREATE ((predictor).symbols, value_, update); \
    } \
    else \
    { \
        ENetSymbol * symbol = (predictor).symbols, * child; \
        for (;;) \
        { \
            enet_uint16 total = under_ + symbol -> total + (symbol -> value + 1)*minimum; \
            if (code >= total) \
            { \
                under_ += symbol -> total; \
                lowValue = symbol -> value; \
                if (symbol -> right) { symbol += symbol -> right; continue; } \
                value_ = lowValue + 1 + (code - under_ - (lowValue + 1)*minimum) / minimum; \
                ENET_SYMBOL_CREATE (child, value_, update); \
                symbol -> right = child - symbol; \
            } \
            else \
            if (code < total - symbol -> count - minimum) \
            { \
                symbol -> total += update; \
                if (symbol -> left) { symbol += symbol -> left; continue; } \
                value_ = lowValue + 1 + (code - under_ - (lowValue + 1)*minimum) / minimum; \
                ENET_SYMBOL_CREATE (child, value_, update); \
                symbol -> left = child - symbol; \
            } \
            else \
            { \
                value_ = symbol -> value; \
                count_ = symbol -> count; \
                under_ += symbol -> total - count_; \
                symbol -> total += update; \
                symbol -> count += update; \
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
    ENetPredictor * subcontext = NULL;
    size_t nextSymbol = 0;

    if (rangeCoder == NULL || inLimit <= 0)
      return 0;

    ENET_PREDICTOR_CREATE (rangeCoder -> context, ENET_CONTEXT_ESCAPE_MINIMUM, ENET_CONTEXT_SYMBOL_MINIMUM);

    ENET_RANGE_CODER_SEED;

    for (;;)
    {
        enet_uint8 value = 0;
        enet_uint16 code, under, count;
        if (subcontext != NULL && subcontext -> escapes > 0)
        {
            code = ENET_RANGE_CODER_READ (subcontext -> total);
            if (code < subcontext -> escapes)
            {
                ENET_RANGE_CODER_DECODE (0, subcontext -> escapes, subcontext -> total);
                goto parentContext;
            }
            code -= subcontext -> escapes;
            ENET_PREDICTOR_TRY_DECODE (* subcontext, code, value, under, count, ENET_SUBCONTEXT_SYMBOL_DELTA);
            ENET_RANGE_CODER_DECODE (subcontext -> escapes + under, count, subcontext -> total);
            subcontext -> total += ENET_SUBCONTEXT_SYMBOL_DELTA;
            if (count > 0xFF - 2*ENET_SUBCONTEXT_SYMBOL_DELTA || subcontext -> total > ENET_RANGE_CODER_BOTTOM - 0x100)
              ENET_PREDICTOR_RESCALE (* subcontext, 0);
        }
        else
        {
        parentContext:
            code = ENET_RANGE_CODER_READ (rangeCoder -> context.total);
            if (code < rangeCoder -> context.escapes)
            {
                ENET_RANGE_CODER_DECODE (0, rangeCoder -> context.escapes, rangeCoder -> context.total);
                break;
            }
            code -= rangeCoder -> context.escapes;
            ENET_PREDICTOR_DECODE (rangeCoder -> context, code, value, under, count, ENET_CONTEXT_SYMBOL_DELTA, ENET_CONTEXT_SYMBOL_MINIMUM);
            if (count <= 0)
              ENET_PREDICTOR_CREATE (rangeCoder -> subcontexts [value], 0, 0);
            ENET_RANGE_CODER_DECODE (rangeCoder -> context.escapes + under + value*ENET_CONTEXT_SYMBOL_MINIMUM, count + ENET_CONTEXT_SYMBOL_MINIMUM, rangeCoder -> context.total);
            rangeCoder -> context.total += ENET_CONTEXT_SYMBOL_DELTA;
            if (count > 0xFF - 2*ENET_CONTEXT_SYMBOL_DELTA || rangeCoder -> context.total > ENET_RANGE_CODER_BOTTOM - 0x100)
              ENET_PREDICTOR_RESCALE (rangeCoder -> context, ENET_CONTEXT_SYMBOL_MINIMUM);
            if (subcontext != NULL)
            {
                enet_uint16 under, count;
                ENET_PREDICTOR_ENCODE (* subcontext, value, under, count, ENET_SUBCONTEXT_SYMBOL_DELTA);
                if (count <= 0)
                {
                    subcontext -> escapes += ENET_SUBCONTEXT_ESCAPE_DELTA;
                    subcontext -> total += ENET_SUBCONTEXT_ESCAPE_DELTA;
                }
                subcontext -> total += ENET_SUBCONTEXT_SYMBOL_DELTA; 
                if (count > 0xFF - 2*ENET_SUBCONTEXT_SYMBOL_DELTA || subcontext -> total > ENET_RANGE_CODER_BOTTOM - 0x100)
                  ENET_PREDICTOR_RESCALE (* subcontext, 0);
            }
        }

        ENET_RANGE_CODER_OUTPUT (value);

        subcontext = & rangeCoder -> subcontexts [value];
        ENET_RANGE_CODER_FREE_SYMBOLS;
    }
                        
    return (size_t) (outData - outStart);
}

/** Sets the packet compressor the host should use to the default range compressor.
    @param host host to enable or disable compression for
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
    
    
     
