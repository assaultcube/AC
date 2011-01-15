// rndmap.cpp: perlin noise landscape generation and some experimental random map stuff, currently not used

#include "cube.h"

float noise(int x, int y, int seed)
{
    int n = x+y*57;
    n = (n<<13)^n;
    return 1.0f-((n*(n*n*15731+789221)+1376312589)&0x7fffffff)/1073741824.0f;
}

float smoothednoise(int x, int y, int seed)
{
    float corners = (noise(x-1, y-1, seed)+noise(x+1, y-1, seed)+noise(x-1, y+1, seed)+noise(x+1, y+1, seed))/16;
    float sides = (noise(x-1, y, seed)+noise(x+1, y, seed)+noise(x, y-1, seed)+noise(x, y+1, seed))/8;
    float center = noise(x, y, seed)/4;
    return corners+sides+center;
}

float interpolate(float a, float b, float x)
{
    float ft = x*3.1415927f;
    float f = (1.0f-cosf(ft))*0.5f;
    return a*(1-f)+b*f;
}

float interpolatednoise(float x, float y, int seed)
{
    int ix = (int)x;
    float fx = x-ix;
    int iy = (int)y;
    float fy = y-iy;
    float v1 = smoothednoise(ix,   iy,   seed);
    float v2 = smoothednoise(ix+1, iy,   seed);
    float v3 = smoothednoise(ix,   iy+1, seed);
    float v4 = smoothednoise(ix+1, iy+1, seed);
    float i1 = interpolate(v1, v2, fx);
    float i2 = interpolate(v3, v4, fy);
    return interpolate(i1, i2, fy);
}

float perlinnoise_2D(float x, float y, int seedstep, float pers)
{
    float total = 0;
    int seed = 0;
    for(int i = 0; i<7; i++)
    {
        float frequency = (float)(2^i);
        float amplitude = (float)pow(pers, i);
        total += interpolatednoise(x*frequency, y*frequency, seed)*amplitude;
        seed += seedstep;
    }
    return total;
}

void perlinarea(block &b, int scale, int seed, int psize)
{
    seed = rnd(10000);
    if(!scale) scale = 10;
    for(int y = b.y; y<=b.y+b.ys; y++) for(int x = b.x; x<=b.x+b.xs; x++)
    {
        sqr *s = S(x,y);
        if(!SOLID(s) && x!=b.x+b.xs && y!=b.y+b.ys) s->type = FHF; 
        s->vdelta = (int)(perlinnoise_2D(x/((float)scale)+seed, y/((float)scale)+seed, 1000, 0.01f)*50+25);
        if(s->vdelta>128) s->vdelta = 0;
    }
}


