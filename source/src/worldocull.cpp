// worldocull.cpp: occlusion map and occlusion test

#include "cube.h"

#define NUMRAYS 512

float rdist[NUMRAYS];
bool ocull = true;
float odist = 256;

void toggleocull() { ocull = !ocull; }

COMMAND(toggleocull, "");

// constructs occlusion map: cast rays in all directions on the 2d plane and record distance.
// done exactly once per frame.

void disableraytable()
{
    odist = 1e16f;
    loopi(NUMRAYS) rdist[i] = 1e16f;
}

void computeraytable(float vx, float vy, float fov)
{
    if(!ocull) { disableraytable(); return; }

    odist = getvar("fog")*1.5f;

    float apitch = (float)fabs(camera1->pitch);
    float af = fov/2+apitch/1.5f+3;
    float byaw = (camera1->yaw-90+af)/360*PI2;
    float syaw = (camera1->yaw-90-af)/360*PI2;

    loopi(NUMRAYS)
    {
        float angle = i*PI2/NUMRAYS;
        if((apitch>45 // must be bigger if fov>120
        || (angle<byaw && angle>syaw)
        || (angle<byaw-PI2 && angle>syaw-PI2)
        || (angle<byaw+PI2 && angle>syaw+PI2))
        && !OUTBORD(vx, vy)
        && !SOLID(S(int(vx), int(vy))))       // try to avoid tracing ray if outside of frustrum
        {
            float ray = i*8/(float)NUMRAYS;
            float dx, dy;
            if(ray>1 && ray<3) { dx = -(ray-2); dy = 1; }
            else if(ray>=3 && ray<5) { dx = -1; dy = -(ray-4); }
            else if(ray>=5 && ray<7) { dx = ray-6; dy = -1; }
            else { dx = 1; dy = ray>4 ? ray-8 : ray; }
            float sx = vx;
            float sy = vy;
            for(;;)
            {
                sx += dx;
                sy += dy;
                if(SOLID(S(int(sx), int(sy))))    // 90% of time spend in this function is on this line
                {
                    rdist[i] = (float)(fabs(sx-vx)+fabs(sy-vy));
                    break;
                }
            }
        }
        else
        {
            rdist[i] = 2;
        }
    }
}

// test occlusion for a cube... one of the most computationally expensive functions in the engine
// as its done for every cube and entity, but its effect is more than worth it!

#ifdef __GNUC__
// GCC seems to have trouble inlining these
#define ca(xv, yv) ({ float x = (xv), y = (yv); x>y ? y/x : 2-x/y; })
#define ma(xv, yv) ({ float x = (xv), y = (yv); x==0 ? (y>0 ? 2 : -2) : y/x; })
#else
static inline float ca(float x, float y) { return x>y ? y/x : 2-x/y; }
static inline float ma(float x, float y) { return x==0 ? (y>0 ? 2 : -2) : y/x; }
#endif

int isoccluded(float vx, float vy, float cx, float cy, float csize)     // v = viewer, c = cube to test 
{
    // ABC
    // D E
    // FGH

    // - check middle cube? BG

    // find highest and lowest angle in the occlusion map that this cube spans, based on its most left and right
    // points on the border from the viewer pov... I see no easier way to do this than this silly code below

    float xdist = 0, ydist = 0, h = 0, l = 0;
    if(cx<=vx)              // ABDFG
    {
        if(cx+csize<vx)     // ADF
        {
            if((xdist = vx-(cx+csize)) > odist) return 2;
            if(cy<=vy)      // AD
            {
                if(cy+csize<vy) { if((ydist = vy-(cy+csize)) > odist) return 2; h = ca(-(cx-vx), ydist)+4; l = ca(xdist, -(cy-vy))+4; }        // A
                else            {                                               h = ma(xdist, -(cy+csize-vy))+4; l = ma(xdist, -(cy-vy))+4; }  // D
            }
            else                { if((ydist = cy-vy) > odist) return 2;         h = ca(cy+csize-vy, xdist)+2; l = ca(ydist, -(cx-vx))+2; }     // F
        }
        else                // BG
        {
            if(cy<=vy)
            {
                if(cy+csize<vy) { if((ydist = vy-(cy+csize)) > odist) return 2; h = ma(ydist, cx-vx)+6; l = ma(ydist, cx+csize-vx)+6; }        // B
                else return 0;
            }
            else                { if((ydist = cy-vy) > odist) return 2;         h = ma(ydist, -(cx+csize-vx))+2; l = ma(ydist, -(cx-vx))+2; }  // G
        }
    }
    else                    // CEH
    {
        if((xdist = cx-vx) > odist) return 2;
        if(cy<=vy)          // CE
        {
            if(cy+csize<vy) { if((ydist = vy-(cy+csize)) > odist) return 2;     h = ca(-(cy-vy), xdist)+6; l = ca(ydist, cx+csize-vx)+6; }     // C
            else            {                                                   h = ma(xdist, cy-vy); l = ma(xdist, cy+csize-vy); }            // E
        }
        else                { if((ydist = cy-vy) > odist) return 2;             h = ca(cx+csize-vx, ydist); l = ca(xdist, cy+csize-vy); }      // H
    }

    float dist = xdist+ydist-1; // 1 needed?
    int si = int(h*(NUMRAYS/8))+NUMRAYS;     // get indexes into occlusion map from angles
    int ei = int(l*(NUMRAYS/8))+NUMRAYS+1; 
    if(ei<=si) ei += NUMRAYS;

    for(int i = si; i<=ei; i++)
    {
        if(dist<rdist[i&(NUMRAYS-1)]) return 0;     // if any value in this segment of the occlusion map is further away then cube is not occluded
    }

    return 1;                                       // cube is entirely occluded
}

