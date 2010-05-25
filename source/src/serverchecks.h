
#define MINELINE 50

//FIXME
/* There are smarter ways to implement this function */
int getmaxarea(bool inversed_x, bool inversed_y, bool transposed, int maplayout_factor, char *maplayout)
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
        if ( xf - xi > MINELINE ) {                                     // if the line has the minimun threshold of emptiness
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

        sav_x = false;                                                  // reset x
        xi = xf = 0;
    }
    return maxarea;
}

int checkarea(int maplayout_factor, char *maplayout) {
    int area = 0, maxarea = 0;
    for (int i=0; i < 8; i++) {
        area = getmaxarea((i & 1),(i & 2),(i & 4), maplayout_factor, maplayout);
        if ( area > maxarea ) maxarea = area;
    }
    return maxarea;
}

/**
This part is related to medals system. WIP
 */

inline int minhits2combo(int gun)
{
    switch (gun)
    {
        case GUN_SUBGUN:
        case GUN_AKIMBO:
        case GUN_GRENADE:
            return 3;
        default:
            return 2;
    }
}

void checkcombo (client *target, client *actor, int damage, int gun)
{
    int diffhittime = servmillis - actor->lasthit;
    actor->lasthit = servmillis;
    if ((gun == GUN_SHOTGUN && gun == GUN_GRENADE) && damage < 20) {
        actor->lastgun = gun;
        return;
    }

    if ( diffhittime < 750 ) {
        if ( gun == actor->lastgun ) {
            if ( diffhittime * 2 < guns[gun].attackdelay * 3 ) {
                actor->combohits++;
                actor->combotime+=diffhittime;
                actor->combodamage+=damage;
                int mh2c = minhits2combo(gun);
                if ( actor->combohits > mh2c && actor->combo < 3 && actor->combohits % mh2c == 1 ) {
                    actor->combo++;
                    actor->points += 10;
                    actor->ncombos++;
                }
            }
        } else {
            switch (gun) {
                case GUN_KNIFE:
                case GUN_PISTOL:
                    if ( guns[actor->lastgun].isauto ) break;
                case GUN_GRENADE:
                    actor->combohits++;
                    actor->combotime+=diffhittime;
                    actor->combodamage+=damage;
                    actor->combo++;
                    actor->points += 10;
                    actor->ncombos++;
                    break;
            }
        }
    } else {
        actor->combo=0;
        actor->combotime=0;
        actor->combodamage=0;
        actor->combohits=0;
    }

    actor->lastgun = gun;
}

/** This function is partially temporary... it needs to be re-made */
void checkcover (client *target, client *actor) // FIXME
{
    int team = actor->team;
    int oteam = team_opposite(team);
    sflaginfo &f = sflaginfos[team];
    sflaginfo &of = sflaginfos[oteam];
    float flagflag = sqrt(float((f.x-of.x)*(f.x-of.x) + (f.y-of.y)*(f.y-of.y)));
    if ( flagflag < 50 ) return;
    if ( flagflag > 200 ) flagflag = 200;

    if (m_ctf) {
        float range = flagflag / 4;
        if (f.state == CTFF_INBASE) {
            float dx = actor->state.o.x-f.x, dy = actor->state.o.y-f.y;
            float dist = sqrt (dx*dx+dy*dy);
            dx = target->state.o.x-f.x, dy = target->state.o.y-f.y;
            dist += sqrt (dx*dx+dy*dy);
            if ( dist < range ) {
                sendf(actor->clientnum, 1, "ri2", SV_HUDEXTRAS, 1); //FIXME
                actor->points += 3 * clientnumber / 2;
                actor->ncovers++;
            }
        }
        if (of.state == CTFF_STOLEN && actor->clientnum != of.actor_cn) {
            float dx = actor->state.o.x-clients[of.actor_cn]->state.o.x, dy = actor->state.o.y-clients[of.actor_cn]->state.o.y;
            float dist = sqrt (dx*dx+dy*dy);
            dx = target->state.o.x-clients[of.actor_cn]->state.o.x, dy = target->state.o.y-clients[of.actor_cn]->state.o.y;
            dist += sqrt(dx*dx+dy*dy);
            if ( dist < range ) {
                sendf(actor->clientnum, 1, "ri2", SV_HUDEXTRAS, 1);  //FIXME
                actor->points += 5 * clientnumber / 2;
                actor->ncovers++;
            }
        }
    } else if (m_htf) {
        float range = flagflag / 5;
        if (f.state == CTFF_DROPPED) {
            float dx = target->state.o.x-f.pos[0], dy = target->state.o.y-f.pos[1];
            float dist = sqrt (dx*dx+dy*dy);
            if ( dist < range ) {
                sendf(actor->clientnum, 1, "ri2", SV_HUDEXTRAS, 1);  //FIXME
                actor->points += 3 * clientnumber / 2;
                actor->ncovers++;
            }
        }
        if (f.state == CTFF_STOLEN && actor->clientnum != f.actor_cn) {
            float dx = actor->state.o.x-clients[f.actor_cn]->state.o.x, dy = actor->state.o.y-clients[f.actor_cn]->state.o.y;
            float dist = sqrt (dx*dx+dy*dy);
            dx = target->state.o.x-clients[f.actor_cn]->state.o.x, dy = target->state.o.y-clients[f.actor_cn]->state.o.y;
            dist += sqrt (dx*dx+dy*dy);
            if ( dist < range ) {
                sendf(actor->clientnum, 1, "ri2", SV_HUDEXTRAS, 1);  //FIXME
                actor->points += 6 * clientnumber / 2;
                actor->ncovers++;
            }
        }
    }
}

/** This function is completely temporary, and it is meanless for now */
void checkfrag (client *target, client *actor, int gun, bool gib)
{
    int targethasflag = clienthasflag(target->clientnum);
    int actorhasflag = clienthasflag(actor->clientnum);
    target->points -= 5;
    if(target!=actor) {
        if(!isteam(target->team, actor->team)) {

            if (m_teammode) {
                if(!m_flags) actor->points += 5 * target->points / 100;
                else actor->points += 4 * target->points / 100;
            }
            else actor->points += 3 * target->points / 100;

            if (gib) {
                if ( gun == GUN_GRENADE ) actor->points += 12;
                else if ( gun == GUN_SNIPER ) {
                    actor->points += 16;
                    actor->nhs++;
                }
                else if ( gun == GUN_KNIFE ) actor->points += 20;
            }
            else actor->points += 10;

            if ( targethasflag >= 0 ) {
                actor->points += 3 * clientnumber;
                if ( m_htf ) target->points -= clientnumber;
            }

            if ( m_htf && actorhasflag >= 0 ) actor->points += clientnumber;

            if ( m_flags ) checkcover (target, actor);

            if ( actor->combo ) sendf(actor->clientnum, 1, "ri2", SV_HUDEXTRAS, 0);

        } else {

            if ( targethasflag >= 0 ) {
                actor->points -= 2 * clientnumber;
            }
            else actor->points -= 10;

        }
    } 
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

