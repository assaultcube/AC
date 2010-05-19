
#define MINELINE 50

int getmaxarea(bool inversed_x, bool inversed_y, bool transposed)
{
    int ls = (1 << maplayout_factor);
    int xi = 0, oxi = 0, xf = 0, oxf = 0, yi = 0, yf = 0, fx = 0, fy = 0;
    int area = 0, maxarea = 0;
    bool sav_x = false, sav_y = false;

    if (transposed) fx = maplayout_factor;
    else fy = maplayout_factor;

    // walk on x for each y
    for ( int y = (inversed_y ? ls-1 : 0); (inversed_y ? y >= 0 : y < ls); (inversed_y ? y-- : y++) ) {

    /* Analyzing each cube of the line */
        for ( int x = (inversed_x ? ls-1 : 0); (inversed_x ? x >= 0 : x < ls); (inversed_x ? x-- : x++) ) {
            if ( maplayout[ ( x << fx ) + ( y << fy ) ] != 127 ) {      // if it is not solid
                if ( sav_x ) {                                          // if the last cube was saved
                    xf = x;                                             // new end for this line
                }
                else {
                    xi = x;                                             // new begin of the line
                    sav_x = true;                                       // accumulating cubes from now
                }
            } else {                                    // solid
                if ( xf - xi > MINELINE ) break;                        // if the empty line is greater than a minimum, get out
                sav_x = false;                                          // stop the accumulation of cubes
            }
        }

    /* Analyzing this line with the previous one */
        if ( xf - xi > MINELINE ) {                                           // if the line has the minimun threshold of emptiness
            if ( sav_y ) {                                              // if the last line was saved
                if ( 2*oxi + MINELINE < 2*xf &&
                     2*xi + MINELINE < 2*oxf ) {                        // if the last line intersect this one
                    yf = y;                                             // new end of area
                    area += xf - xi;
                } else {
                    oxi = xi;                                           // new area vertices
                    oxf = xf;
                }
            }
            else {
                oxi = xi;
                oxf = xf;
                yi = y;                                                 // new begin of area
                sav_y = true;                                           // accumulating lines from now
            }
        } else {
            sav_y = false;                                              // stop the accumulation of lines
            if (area > maxarea) maxarea = area;                         // new max area
            area=0;
        }

        sav_x = false;                                                  // reset
        xi = xf = 0;
    }
    return maxarea;
}

/* Please... someone more skilled do a better job here... this is ridiculous */
int checkarea() {
    int area = 0, maxarea = 0;
    area = getmaxarea(false,false,false);
    if ( area > maxarea ) maxarea = area;
    area = getmaxarea(true,false,false);
    if ( area > maxarea ) maxarea = area;
    area = getmaxarea(false,true,false);
    if ( area > maxarea ) maxarea = area;
    area = getmaxarea(true,true,false);
    if ( area > maxarea ) maxarea = area;
    area = getmaxarea(false,false,true);
    if ( area > maxarea ) maxarea = area;
    area = getmaxarea(true,false,true);
    if ( area > maxarea ) maxarea = area;
    area = getmaxarea(false,true,true);
    if ( area > maxarea ) maxarea = area;
    area = getmaxarea(true,true,true);
    if ( area > maxarea ) maxarea = area;

    return maxarea;
}

/**
If you read README.txt you must know that AC does not have cheat protection implemented.
However this file is the sketch to a very special kind of cheat detection tools in server side.

This is not based in program tricks, i.e., encryption, secret bytes, nor monitoring/scanning tools.

The idea behind these cheat detections is to check (or reproduce) the client data, and verify if
this data is expected or possible. Also, there is no need to check all clients all time, and
one coding this kind of check must pay a special attention to the lag effect and how it can
affect the data observed. This is not a trivial task, and probably it is the main reason why
such tools were never implemented.

This part is here for compatibility purposes.
If you know nothing about these detections, please, just ignore it.
*/

inline void checkmove (int cn, int *v)
{
    return;
}

inline void checkshoot (int cn, gameevent *shot)
{
    return;
}

bool validdamage (client *target, client *actor, int gun, bool gib)
{
    return true;
}

