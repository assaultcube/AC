static void FUNCNAME(halvetexture)(uchar *src, uint sw, uint sh, uchar *dst)
{
    uint stride = sw*BPP;
    for(uchar *yend = &src[sh*stride]; src < yend;)
    {
        for(uchar *xend = &src[stride]; src < xend; src += 2*BPP, dst += BPP)
        {
            #define OP(c, n) dst[n] = (uint(src[n]) + uint(src[n+BPP]) + uint(src[stride+n]) + uint(src[stride+n+BPP]))>>2
            PIXELOP
            #undef OP
        }
        src += stride;
    }
}

static void FUNCNAME(shifttexture)(uchar *src, uint sw, uint sh, uchar *dst, uint dw, uint dh)
{
    uint stride = sw*BPP, wfrac = sw/dw, hfrac = sh/dh, wshift = 0, hshift = 0;
    while(dw<<wshift < sw) wshift++;
    while(dh<<hshift < sh) hshift++;
    uint tshift = wshift + hshift;
    for(uchar *yend = &src[sh*stride]; src < yend;)
    {
        for(uchar *xend = &src[stride]; src < xend; src += wfrac*BPP, dst += BPP)
        {        
            #define OP(c, n) c##t = 0
            DEFPIXEL
            #undef OP
            for(uchar *ycur = src, *xend = &ycur[wfrac*BPP], *yend = &src[hfrac*stride]; 
                ycur < yend; 
                ycur += stride, xend += stride)
            {
                for(uchar *xcur = ycur; xcur < xend; xcur += BPP)
                {
                    #define OP(c, n) c##t += xcur[n]    
                    PIXELOP
                    #undef OP
                }
            }
            #define OP(c, n) dst[n] = (c##t)>>tshift
            PIXELOP
            #undef OP
        }
        src += (hfrac-1)*stride; 
    }
}

static void FUNCNAME(scaletexture)(uchar *src, uint sw, uint sh, uchar *dst, uint dw, uint dh)
{
    uint stride = sw*BPP, wfrac = (sw<<12)/dw, hfrac = (sh<<12)/dh;
    for(uint dy = 0, y = 0, yi = 0; dy < dh; dy++)
    {
        uint y2 = y + hfrac, yi2 = y2>>12, 
             h = y2 - y, ih = yi2 - yi,
             ylow, yhigh;
        if(yi < yi2) { ylow = 0x1000U - (y&0xFFFU); yhigh = y2&0xFFFU; }
        else { ylow = y2 - y; yhigh = 0; }

        for(uint dx = 0, x = 0, xi = 0; dx < dw; dx++)
        {
            uint x2 = x + wfrac, xi2 = x2>>12, 
                 w = x2 - x, iw = xi2 - xi, iy = 0,
                 xlow, xhigh; 
            if(xi < xi2) { xlow = 0x1000U - (x&0xFFFU); xhigh = x2&0xFFFU; }
            else { xlow = x2 - x; xhigh = 0; }

            #define OP(c, n) c##t = 0
            DEFPIXEL
            #undef OP
            for(uchar *ycur = &src[xi*BPP], *xend = &ycur[max(iw, 1U)*BPP], *yend = &ycur[stride*(max(ih, 1U) + (yhigh ? 1 : 0))]; 
                ycur < yend; 
                ycur += stride, xend += stride, iy++)
            {
                #define OP(c, n) c = (ycur[n]*xlow)>>12
                DEFPIXEL
                #undef OP
                for(uchar *xcur = &ycur[BPP]; xcur < xend; xcur += BPP) 
                {
                    #define OP(c, n) c += xcur[n]    
                    PIXELOP
                    #undef OP
                }
                if(xhigh) 
                { 
                    #define OP(c, n) c += (xend[n]*xhigh)>>12
                    PIXELOP
                    #undef OP
                }
                #define OP(c, n) c = (c<<12)/w
                PIXELOP
                #undef OP
                if(!iy) 
                {
                    #define OP(c, n) c##t += (c*ylow)>>12
                    PIXELOP
                    #undef OP
                }
                else if(iy==ih)
                {
                    #define OP(c, n) c##t += (c*yhigh)>>12
                    PIXELOP
                    #undef OP
                }
                else 
                { 
                    #define OP(c, n) c##t += c
                    PIXELOP
                    #undef OP
                }
            }

            #define OP(c, n) dst[n] = ((c##t)<<12)/h
            PIXELOP
            #undef OP

            dst += BPP;
            x = x2;
            xi = xi2;
        }

        src += ih*stride; 
        y = y2;
        yi = yi2;
    }
}

#undef FUNCNAME
#undef DEFPIXEL
#undef PIXELOP
#undef BPP

