// worldocull.cpp: occlusion map and occlusion test

#include "cube.h"

#define NUMRAYS 512

float rdist[NUMRAYS];
bool ocull = true;
float odist = 256;

void toggleocull() { ocull = !ocull; };

COMMAND(toggleocull, ARG_NONE);

// constructs occlusion map: cast rays in all directions on the 2d plane and record distance.
// done exactly once per frame.

void computeraytable(float vx, float vy)
{
    if(!ocull) return;

    odist = getvar("fog")*1.5f;

    float apitch = (float)fabs(player1->pitch);
    float af = getvar("fov")/2+apitch/1.5f+3;
    float byaw = (player1->yaw-90+af)/360*PI2;
    float syaw = (player1->yaw-90-af)/360*PI2;

    loopi(NUMRAYS)
    {
        float angle = i*PI2/NUMRAYS;
        if((apitch>45 // must be bigger if fov>120
        || (angle<byaw && angle>syaw)
        || (angle<byaw-PI2 && angle>syaw-PI2)
        || (angle<byaw+PI2 && angle>syaw+PI2))
        && !OUTBORD(vx, vy)
        && !SOLID(S(fast_f2nat(vx), fast_f2nat(vy))))       // try to avoid tracing ray if outside of frustrum
        {
            float ray = i*8/(float)NUMRAYS;
            float dx, dy;
            if(ray>1 && ray<3) { dx = -(ray-2); dy = 1; }
            else if(ray>=3 && ray<5) { dx = -1; dy = -(ray-4); }
            else if(ray>=5 && ray<7) { dx = ray-6; dy = -1; }
            else { dx = 1; dy = ray>4 ? ray-8 : ray; };
            float sx = vx;
            float sy = vy;
            for(;;)
            {
                sx += dx;
                sy += dy;
                if(SOLID(S(fast_f2nat(sx), fast_f2nat(sy))))    // 90% of time spend in this function is on this line
                {
                    rdist[i] = (float)(fabs(sx-vx)+fabs(sy-vy));
                    break;
                };
            };
        }
        else
        {
            rdist[i] = 2;
        };
    };
};

// test occlusion for a cube... one of the most computationally expensive functions in the engine
// as its done for every cube and entity, but its effect is more than worth it!

inline float ca(float x, float y) { return x>y ? y/x : 2-x/y; }; 
inline float ma(float x, float y) { return x==0 ? (y>0 ? 2 : -2) : y/x; };

int isoccluded(float vx, float vy, float cx, float cy, float csize)     // v = viewer, c = cube to test 
{
    if(!ocull) return 0;

    float nx = vx, ny = vy;     // n = point on the border of the cube that is closest to v
    if(nx<cx) nx = cx;
    else if(nx>cx+csize) nx = cx+csize;
    if(ny<cy) ny = cy;
    else if(ny>cy+csize) ny = cy+csize;
    float xdist = (float)fabs(nx-vx);
    float ydist = (float)fabs(ny-vy);
    if(xdist>odist || ydist>odist) return 2;
    float dist = xdist+ydist-1; // 1 needed?

    // ABC
    // D E
    // FGH

    // - check middle cube? BG

    // find highest and lowest angle in the occlusion map that this cube spans, based on its most left and right
    // points on the border from the viewer pov... I see no easier way to do this than this silly code below

    float h, l;
    if(cx<=vx)              // ABDFG
    {
        if(cx+csize<vx)     // ADF
        {
            if(cy<=vy)      // AD
            {
                if(cy+csize<vy) { h = ca(-(cx-vx), -(cy+csize-vy))+4; l = ca(-(cx+csize-vx), -(cy-vy))+4; }         // A
                else            { h = ma(-(cx+csize-vx), -(cy+csize-vy))+4; l =  ma(-(cx+csize-vx), -(cy-vy))+4; }  // D
            }
            else                { h = ca(cy+csize-vy, -(cx+csize-vx))+2; l = ca(cy-vy, -(cx-vx))+2; };              // F
        }
        else                // BG
        {
            if(cy<=vy)
            {
                if(cy+csize<vy) { h = ma(-(cy+csize-vy), cx-vx)+6; l = ma(-(cy+csize-vy), cx+csize-vx)+6; }         // B
                else return 0;
            }
            else     { h = ma(cy-vy, -(cx+csize-vx))+2; l = ma(cy-vy, -(cx-vx))+2; };                               // G
        };
    }
    else                    // CEH
    {
        if(cy<=vy)          // CE
        {
            if(cy+csize<vy) { h = ca(-(cy-vy), cx-vx)+6; l = ca(-(cy+csize-vy), cx+csize-vx)+6; }                   // C
            else            { h = ma(cx-vx, cy-vy); l = ma(cx-vx, cy+csize-vy); };                                  // E
        }
        else                { h = ca(cx+csize-vx, cy-vy); l = ca(cx-vx, cy+csize-vy); };                            // H
    };
    int si = fast_f2nat(h*(NUMRAYS/8))+NUMRAYS;     // get indexes into occlusion map from angles
    int ei = fast_f2nat(l*(NUMRAYS/8))+NUMRAYS+1; 
    if(ei<=si) ei += NUMRAYS;

    for(int i = si; i<=ei; i++)
    {
        if(dist<rdist[i&(NUMRAYS-1)]) return 0;     // if any value in this segment of the occlusion map is further away then cube is not occluded
    };

    return 1;                                       // cube is entirely occluded
};

