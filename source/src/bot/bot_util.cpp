//
// C++ Implementation: bot_util
//
// Description: Basic util functions, mostly bot related
//
//
// Author:  <rickhelmus@gmail.com>
//
// 
//
//

#include "cube.h"
#include "bot.h"

// Function code by PMB - Begin


// Random functions begin
// copied from
// http://forums.bots-united.com/showthread.php?t=244&page=1&pp=10&highlight=random
// So credits to PMB and the person(s) who actually made it :]

// maximum value returned by our number generator (2^31 - 1 = 0x7FFFFFFF)
#define LRAND_MAX 2147483647L
long lseed; // our random number generator's seed

long lrand (void)
{
     // this function is the equivalent of the rand() standard C library function,
     // except that whereas rand() works only with short integers
     // (i.e. not above 32767), this function is able to generate 32-bit random
     // numbers. Isn't that nice ?
     // credits go to Ray Gardner for his fast implementation of minimal random
     // number generators
     // http://c.snippets.org/snip_lister.php?fname=rg_rand.c
     
     // compose the two 16-bit parts of the long integer and assemble them
     static unsigned long lrand_lo, lrand_hi;
     lrand_lo = 16807 * (long) (lseed & 0xFFFF); // low part
     lrand_hi = 16807 * (long) ((unsigned long) lseed >> 16);
     // high part
     lrand_lo += (lrand_hi & 0x7FFF) << 16;
     // assemble both in lrand_lo
     // is the resulting number greater than LRAND_MAX (half the capacity) ?
     if (lrand_lo > LRAND_MAX)
     {
          lrand_lo &= LRAND_MAX; // then get rid of the disturbing bit
          lrand_lo++; // and increase it a bit (to avoid overflow problems, I suppose)
     }
     
     lrand_lo += lrand_hi >> 15;
     // now do twisted maths to generate the next seed
     // is the resulting number greater than LRAND_MAX (half the capacity) ?
     if (lrand_lo > LRAND_MAX)
     {
          lrand_lo &= LRAND_MAX; // then get rid of the disturbing bit
          lrand_lo++; // and increase it a bit (to avoid overflow problems, I suppose)
     }
     // now we've got our (pseudo-)random number.
     lseed = (long) lrand_lo; // put it in the seed for next time
     return (lseed); // and return it. Yeah, simple as that.
}

void lsrand (unsigned long initial_seed)
{
     // this function initializes the random seed based on the initial seed value
     // passed in the initial_seed parameter. Since random seeds are
     // usually initialized with the time of day, and time is a value that
     // changes slowly, we bump the seed twice to have it in a more
     // random state for future calls of the random number generator.
     
     // fill in the initial seed of the random number generator
     lseed = (long) initial_seed; 
     lseed = lrand (); // bump it once
     lseed = lrand (); // bump it twice
     return; // that's all folks
}

long RandomLong (long from, long to)
{
     // this function returns a random integer number between (and including)
     // the starting and ending values passed by parameters from and to.
     if (to <= from) 
          return (from);
          
     return (from + lrand () / (LRAND_MAX / (to - from + 1)));
}

float RandomFloat (float from, float to)
{
     // this function returns a random floating-point number between (and including)
     // the starting and ending values passed by parameters from and to.
     
     if (to <= from)
          return (from);
          
     return (from + (float) lrand () / (LRAND_MAX / (to - from)));
}

// End random functions

void AnglesToVectors(vec angles, vec &forward, vec &right, vec &up)
{
     static float degrees_to_radians = 2 * PI / 360;
     float angle;
     float sin_pitch;
     float sin_yaw;
     float sin_roll;
     float cos_pitch;
     float cos_yaw;
     float cos_roll;
         
     // For some reason this has to be done :)
     // Me = math n00b
     angles.x = -angles.x;
     angles.y -= 90;
     
     // compute the sin and cosine of the pitch component
     angle = angles.x * degrees_to_radians;
     sin_pitch = sinf (angle);
     cos_pitch = cosf (angle);
     
     // compute the sin and cosine of the yaw component
     angle = angles.y * degrees_to_radians;
     sin_yaw = sinf (angle);
     cos_yaw = cosf (angle);
     
     // compute the sin and cosine of the roll component
     angle = angles.z * degrees_to_radians;
     sin_roll = sinf (angle);
     cos_roll = cosf (angle);
     
     // build the FORWARD vector
     forward.x = cos_pitch * cos_yaw;
     forward.y = cos_pitch * sin_yaw;
     forward.z = -sin_pitch;
     
     // build the RIGHT vector
     right.x = -(-(sin_roll * sin_pitch * cos_yaw) - (cos_roll * -sin_yaw));
     right.y = -(-(sin_roll * sin_pitch * sin_yaw) - (cos_roll * cos_yaw));
     right.z = -(sin_roll * cos_pitch);
     
     // build the UPWARDS vector
     up.x = ((cos_roll * sin_pitch * cos_yaw) - (sin_roll * -sin_yaw));
     up.y = ((cos_roll * sin_pitch * sin_yaw) - (sin_roll * cos_yaw));
     up.z = cos_roll * cos_pitch;
     
     return;
}

// Function code by PMB - End

float WrapXAngle(float angle)
{
     if (angle > 90) angle = 90;
     else if (angle < -90) angle = -90;

     return angle;
}

float WrapYZAngle(float angle)
{
     while (angle >= 360.0f) angle -= 360.0f;
     while (angle < 0.0f) angle += 360.0f;

     return angle;
}

// Sees how far it can come from 'from' to 'to' and puts the end vector in 'end'
// (1337 description :-)
// UNDONE: Optimize this as much as possible
void TraceLine(vec from, vec to, dynent *pTracer, bool CheckPlayers, traceresult_s *tr,
               bool SkipTags)
{
     tr->end = from;
     tr->collided = false;
     
     static float flNearestDist, flDist;
     static vec v;
     static bool solid;
    
     flNearestDist = 9999.0f;
     
     if(OUTBORD((int)from.x, (int)from.y)) return;
   
     // Check if the 'line' collides with entities like mapmodels
     loopv(ents)
     {
          entity &e = ents[i];
          if(e.type!=MAPMODEL) continue; // Only check map models for now
               
          mapmodelinfo *mmi = getmminfo(e.attr2);
          if(!mmi || !mmi->h) continue;
    
          float lo = (float)(S(e.x, e.y)->floor+mmi->zoff+e.attr3);
          float hi = lo+mmi->h;
          float z = (fabs(from.z-lo) < fabs(from.z-hi)) ? lo : hi;           
          makevec(&v, e.x, e.y, z);          
          flDist = GetDistance(from, v); 
               
          if ((flDist < flNearestDist) && (intersect(&e, from, to, &tr->end)))
          {
               flNearestDist = GetDistance(from, tr->end);
               tr->collided = true;
          }
     }

     if (CheckPlayers)
     {
          // Check if the 'line' collides with players
          loopv(players)
          {
               playerent *d = players[i];
               if(!d || (d==pTracer) || (d->state != CS_ALIVE)) continue; // Only check valid players
                    
               flDist = GetDistance(from, d->o); 
               
               if ((flDist < flNearestDist) && (intersect(d, from, to, &tr->end)))
               {
                    flNearestDist = flDist;
                    tr->collided = true;
               }
          }

          // Check if the 'line' collides with the local player(player1)
          playerent *d = player1; // Shortcut
          if (d && (d!=pTracer) && !BotManager.m_pBotToView && (d->state == CS_ALIVE))
          {
               flDist = GetDistance(from, d->o); 
               
               if ((flDist < flNearestDist) && (intersect(d, from, to, &tr->end)))
               {
                    flNearestDist = flDist;
                    tr->collided = true;
               }
          }          
     }
    
    
     float dx = to.x-from.x;
     float dy = to.y-from.y; 
     int steps = (int)(sqrt(dx*dx+dy*dy)/0.9);
     if(!steps) // If from and to are on the same cube...
     {
          if (GetDistance(from, to) < flNearestDist)         
          {
               tr->end = to;
               sqr *s = S(int(from.x), int(from.y));
               float flr = GetCubeFloor(int(from.x), int(from.y));
               solid = ((!SkipTags || !s->tag) && SOLID(s));
               if (solid || (to.z < flr) || (to.z > s->ceil))
               {
                    tr->collided = true;
                    tr->end.z = (fabs(to.z-s->ceil) < fabs(to.z-flr)) ? s->ceil : flr;
               }
          }
          return;
     }
     
     float x = from.x;
     float y = from.y;
     int i = 0;
     vec endcube = from;
     
     // Now check if the 'line' is hit by a cube
     while(i<steps)
     {
          if(OUTBORD((int)x, (int)y)) break;
          if (GetDistance(from, endcube) >= flNearestDist) break;
          sqr *s = S(int(x), int(y));
          solid = ((!SkipTags || !s->tag) && SOLID(s));
          if(solid) break;
          float floor = s->floor;
          if(s->type==FHF) floor -= s->vdelta/4.0f;
          float ceil = s->ceil;
          if(s->type==CHF) ceil += s->vdelta/4.0f;
          float rz = from.z-((from.z-to.z)*(i/(float)steps));
          if(rz<floor || rz>ceil) break;
        
          endcube.x = x;
          endcube.y = y;
          endcube.z = rz;
          x += dx/(float)steps;
          y += dy/(float)steps;
          i++;
     }
    
     if ((i>=steps) && !tr->collided)
     {
          tr->end = to;
     }
     else
     {
          tr->collided = true;
          if (GetDistance(from, endcube) < flNearestDist)
               tr->end = endcube;
     }

     return;
}

float GetDistance(vec v1, vec v2)
{
     if ((v1.x == 0) && (v2.x == 0) && (v1.y == 0) && (v2.y == 0) && (v1.z == 0) &&
         (v2.z == 0))
          return 0.0f;
          
     return v1.dist(v2);
}

// As GetDistance(), but doesn take height in account.
float Get2DDistance(vec v1, vec v2)
{
     if ((v1.x == 0) && (v2.x == 0) && (v1.y == 0) && (v2.y == 0))
          return 0.0f;
     
     v1.z = v2.z = 0.0f;
     return v1.dist(v2);
}

bool IsVisible(vec v1, vec v2, dynent *tracer, bool SkipTags)
{
     traceresult_s tr;
     TraceLine(v1, v2, tracer, (tracer!=NULL), &tr, SkipTags);
     return !tr.collided;
}

// Prediction:
// - pos: Current position
// - vel: Current velocity
// - Time: In seconds, predict how far it is in Time seconds
vec PredictPos(vec pos, vec vel, float Time)
{
     float flVelLength = vel.magnitude();
    
     if (flVelLength <= 1.0)
          return pos; // don't bother with low velocities...

     vec v_end = vel;
     v_end.mul(Time);
     v_end.add(pos);
     return v_end;
}

// returns true if the given file is valid
bool IsValidFile(const char *szFileName)
{
     FILE *fp = fopen(szFileName, "r");

     if (fp)
     {
          fclose(fp);
          return true;
     }

     return false;
}

bool FileIsOlder(const char *szFileName1, const char *szFileName2)
{
/*     int file1, file2;
     struct stat stat1, stat2;

     file1 = open(szFileName1, O_RDONLY);
     file2 = open(szFileName2, O_RDONLY);

     fstat(file1, &stat1);
     fstat(file2, &stat2);

     close(file1);
     close(file2);

     return(stat1.st_mtime < stat2.st_mtime);*/ return false; // FIXME
}

vec Normalize(vec v)
{
     float flLen = sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
     static vec v_norm = g_vecZero;
     if (flLen == 0) 
     {
          v_norm.x=0;v_norm.y=0;v_norm.z=1;
     }
     else
     {
          flLen = 1 / flLen;
          v_norm.x = v.x * flLen; v_norm.y = v.y * flLen; v_norm.z = v.z * flLen;
     }
     return v_norm;
}

float GetYawDiff(float curyaw, vec v1, vec v2)
{
     float yaw = -(float)atan2(v2.x - v1.x, v2.y - v1.y)/PI*180+180;    
     return yaw-curyaw;
}

vec CrossProduct(const vec &a, const vec &b)
{
     vec cross(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
     return cross;
}

int GetDirection(const vec &angles, const vec &v1, const vec &v2)
{
     vec tmp, forward, right, up, cross;
     float flDot;

     AnglesToVectors(angles, forward, right, up);

     tmp = v2;
     tmp.sub(v1);
     tmp.z = 0.0f; // Make 2D
     tmp = Normalize(tmp);
     forward.z = 0; // Make 2D
          
     flDot = tmp.dot(forward);
     cross = CrossProduct(tmp, forward);
     
     int Dir = DIR_NONE;
               
     if (cross.z > 0.1f) Dir |= LEFT;
     else if (cross.z < -0.1f) Dir |= RIGHT;
     if (flDot < -0.1f) Dir |= BACKWARD;
     else if (flDot > 0.1f) Dir |= FORWARD;
     
     if (v1.z > v2.z) Dir |= DOWN;
     if (v1.z < v2.z) Dir |= UP;
     
     return Dir;
}

float GetCubeFloor(int x, int y)
{
     sqr *s = S(x, y);
     float floor = s->floor;
     if (s->type == FHF)
          floor -= (s->vdelta+S(x+1,y)->vdelta+S(x,y+1)->vdelta+S(x+1,y+1)->vdelta)/16.0f;
     return floor;
}

float GetCubeHeight(int x, int y)
{
     sqr *s = S(x, y);
     float ceil = s->ceil;
     if(s->type == CHF)
          ceil += (s->vdelta+S(x+1,y)->vdelta+S(x,y+1)->vdelta+S(x+1,y+1)->vdelta)/16.0f;
     return ceil;
}

const char *SkillNrToSkillName(short skillnr)
{
     if (skillnr == 0) return "best";
     else if (skillnr == 1) return "good";
     else if (skillnr == 2) return "medium";
     else if (skillnr == 3) return "worse";
     else if (skillnr == 4) return "bad";
     
     return NULL;
}

bool IsInGame(dynent *d)
{
     if (!d) return false;
     
     if (d->type==ENT_BOT)
     {
          loopv(bots)
          {
               if (bots[i] == d)
                    return true;
          }
     }
     else
     {
          if (d == player1)
               return true;
          else
          {
               loopv(players)
               {
#ifdef VANILLA_CUBE               
                    if (!players[i] || (players[i]->state == CS_DEDHOST)) continue;
#elif defined(AC_CUBE)
                    if (!players[i]) continue;
#endif                    
                    if (players[i] == d)
                         return true;
               }
          }
     }
     return false;
}
