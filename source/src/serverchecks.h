
float pow2(float x)
{
    return x*x;
}

#define POW2XY(A,B) pow2(A.x-B.x)+pow2(A.y-B.y)

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

#define COVERDIST 2000 // about 45 cubes

int checkteamrequests(int sender)
{
    float bestdist = COVERDIST * 1000;
    int bestid = -1;
    client *ant = clients[sender];
    loopv(clients) {
        client *prot = clients[i];
        if ( i!=sender && prot->type!=ST_EMPTY && prot->team==ant->team && prot->ask>=0 && prot->askmillis + 5000 > gamemillis ) {
            float dist = POW2XY(ant->state.o,prot->state.o);
            if (dist < bestdist) {
                bestid = i;
                bestdist = dist;
            }
        }
    }
    if ( bestdist < MINFF ) return bestid;
    return -1;
}

/** WIP */
void checkteamplay(int s, int sender)
{
    client *actor = clients[sender];
    switch(s){
        case S_COVERME:
        case S_STAYTOGETHER:
            actor->ask = s;
            actor->askmillis = gamemillis;
            break;
        case S_AFFIRMATIVE:
        case S_ALLRIGHTSIR:
        case S_YES:
        {
            int id = checkteamrequests(sender);
            if (id >= 0) {
                actor->linked = id;
                actor->linkmillis = gamemillis;
                actor->linkreason = s;
                sendf(actor->clientnum, 1, "ri2", SV_HUDEXTRAS, 100+id);
            }
            break;
        }
/*        case S_NEGATIVE:
        case S_NOCANDO:
        case S_THERESNOWAYSIR:

        case S_COMEONMOVE:
        case S_COMINGINWITHTHEFLAG:
        case S_DEFENDTHEFLAG:
        case S_ENEMYDOWN:
        case S_GOGETEMBOYS:
        case S_GOODJOBTEAM:
        case S_IGOTONE:
        case S_IMADECONTACT:
        case S_IMATTACKING:
        case S_IMONDEFENSE:
        case S_IMONYOURTEAMMAN:
        case S_RECOVERTHEFLAG:
        case S_SORRY:
        case S_SPREADOUT:
        case S_STAYHERE:
        case S_WEDIDIT:
        case S_NICESHOT:*/
    }
}

int FlagFlag = MINFF * 1000;

float a2c = 0, c2t = 0, a2t = 0; // distances: actor to covered, covered to target and actor to target

inline void testcover(int msg, int factor, client *actor)
{
    if ( a2c < COVERDIST && c2t < COVERDIST && a2t < COVERDIST ) {
        sendf(actor->clientnum, 1, "ri2", SV_HUDEXTRAS, msg);
        actor->points += factor * clientnumber;
        actor->ncovers++;
    }
}

#define CALCCOVER(C) \
    a2c = POW2XY(actor->state.o,C);\
    c2t = POW2XY(C,target->state.o);\
    a2t = POW2XY(actor->state.o,target->state.o)

bool validlink (client *actor, int cn)
{
    return actor->linked >= 0 && actor->linked == cn && gamemillis < actor->linkmillis + 20000 && valid_client(actor->linked);
}

/** WIP */
void checkcover (client *target, client *actor)
{
    int team = actor->team;
    int oteam = team_opposite(team);

    bool covered = false;
    int coverid = -1;

    if ( m_flags ) {
        sflaginfo &f = sflaginfos[team];
        sflaginfo &of = sflaginfos[oteam];

        if ( m_ctf ) {
            if ( f.state == CTFF_INBASE ) {
                CALCCOVER(f);
                testcover(HE_FLAGDEFENDED, 1, actor); /* Flag defended */
            }
            if ( of.state == CTFF_STOLEN && actor->clientnum != of.actor_cn ) {
                covered = true; coverid = of.actor_cn;
                CALCCOVER(clients[of.actor_cn]->state.o);
                testcover(HE_FLAGCOVERED, 2, actor); /* Flag covered */
            }
        } else if ( m_htf ) {
            if ( f.state == CTFF_DROPPED ) {
                struct { short x, y; } nf;
                nf.x = f.pos[0]; nf.y = f.pos[1];
                CALCCOVER(nf);
                testcover(HE_FLAGDEFENDED, 1, actor); /* Flag defended */
            }
            if ( f.state == CTFF_STOLEN && actor->clientnum != f.actor_cn ) {
                covered = true; coverid = f.actor_cn;
                CALCCOVER(clients[f.actor_cn]->state.o);
                testcover(HE_FLAGCOVERED, 3, actor); /* Flag covered */
            }
        }
    }

    if ( !(covered && actor->linked==coverid) && validlink(actor,actor->linked) )
    {
        CALCCOVER(clients[actor->linked]->state.o);
        testcover(HE_COVER, 2, actor); /* covered */
    }

}

#undef CALCCOVER

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

                checkcover (target, actor);
                if ( m_htf && actorhasflag >= 0 ) actor->points += clientnumber;

                if ( targethasflag >= 0 ) {
                    actor->points += 3 * clientnumber;
                    if ( m_htf ) target->points -= clientnumber;
                }
            }
            else actor->points += 3 * target->points / 100;

            if (gib) {
                if ( gun == GUN_GRENADE ) actor->points += 10;
                else if ( gun == GUN_SNIPER ) {
                    actor->points += 15;
                    actor->nhs++;
                }
                else if ( gun == GUN_KNIFE ) actor->points += 20;
            }
            else actor->points += 10;

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

