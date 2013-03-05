inline bool is_lagging(client *cl)
{
    return ( cl->spj > 50 || cl->ping > 500 || cl->ldt > 80 ); // do not change this except if you really know what are you doing
}

inline bool outside_border(vec &po)
{
    return (po.x < 0 || po.y < 0 || po.x >= maplayoutssize || po.y >= maplayoutssize);
}

inline void checkclientpos(client *cl)
{
    vec &po = cl->state.o;
    if( outside_border(po) || maplayout[((int) po.x) + (((int) po.y) << maplayout_factor)] > po.z + 3)
    {
        if(gamemillis > 10000 && (servmillis - cl->connectmillis) > 10000) cl->mapcollisions++;
        if(cl->mapcollisions && !(cl->mapcollisions % 25))
        {
            logline(ACLOG_INFO, "[%s] %s is colliding with the map", cl->hostname, cl->name);
        }
    }
}

#define POW2XY(A,B) (pow2(A.x-B.x)+pow2(A.y-B.y))

extern inline void addban(client *cl, int reason, int type = BAN_AUTO);

#ifdef ACAC
#include "anticheat.h"
#endif

#define MINELINE 50

//FIXME
/* There are smarter ways to implement this function, but most probably they will be very complex */
int getmaxarea(int inversed_x, int inversed_y, int transposed, int ml_factor, char *ml)
{
    int ls = (1 << ml_factor);
    int xi = 0, oxi = 0, xf = 0, oxf = 0, fx = 0, fy = 0;
    int area = 0, maxarea = 0;
    bool sav_x = false, sav_y = false;

    if (transposed) fx = ml_factor;
    else fy = ml_factor;

    // walk on x for each y
    for ( int y = (inversed_y ? ls-1 : 0); (inversed_y ? y >= 0 : y < ls); (inversed_y ? y-- : y++) ) {

    /* Analyzing each cube of the line */
        for ( int x = (inversed_x ? ls-1 : 0); (inversed_x ? x >= 0 : x < ls); (inversed_x ? x-- : x++) ) {
            if ( ml[ ( x << fx ) + ( y << fy ) ] != 127 ) {      // if it is not solid
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
                    area += xf - xi;
                } else {
                    oxi = xi;                                           // new area vertices
                    oxf = xf;
                }
            }
            else {
                oxi = xi;
                oxf = xf;
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

int checkarea(int maplayout_factor, char *maplayout)
{
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

const char * medal_messages[] = { "defended the flag", "covered the flag stealer", "defended the dropped flag", "covered the flag keeper", "covered teammate" };
enum { CTFLDEF, CTFLCOV, HTFLDEF, HTFLCOV, COVER, MEDALMESSAGENUM };
inline void print_medal_messages(client *c, int n)
{
    if (n<0 || n>=MEDALMESSAGENUM) return;
    logline(ACLOG_VERBOSE, "[%s] %s %s", c->hostname, c->name, medal_messages[n]);
}

inline void addpt(client *c, int points, int n = -1) {
    c->state.points += points;
    c->md.dpt += points;
    c->md.updated = true;
    c->md.upmillis = gamemillis + 240; // about 2 AR shots
    print_medal_messages(c,n);
}

/** cnumber is the number of players in the game, at a max value of 12 */
#define CTFPICKPT     cnumber                      // player picked the flag (ctf)
#define CTFDROPPT    -cnumber                      // player dropped the flag to other player (probably)
#define HTFLOSTPT  -2*cnumber                      // penalty
#define CTFLOSTPT     cnumber*distance/100         // bonus: 1/4 of the flag score bonus
#define CTFRETURNPT   cnumber                      // flag return
#define CTFSCOREPT   (cnumber*distance/25+10)      // flag score
#define HTFSCOREPT   (cnumber*4+10)
#define KTFSCOREPT   (cnumber*2+10)
#define COMBOPT       5                            // player frags with combo
#define REPLYPT       2                            // reply success
#define TWDONEPT      5                            // team work done
#define CTFLDEFPT     cnumber                      // player defended the flag in the base (ctf)
#define CTFLCOVPT     cnumber*2                    // player covered the flag stealer (ctf)
#define HTFLDEFPT     cnumber                      // player defended a droped flag (htf)
#define HTFLCOVPT     cnumber*3                    // player covered the flag keeper (htf)
#define COVERPT       cnumber*2                    // player covered teammate
#define DEATHPT      -4                            // player died
#define BONUSPT       target->state.points/400     // bonus (for killing high level enemies :: beware with exponential behavior!)
#define FLBONUSPT     target->state.points/300     // bonus if flag team mode
#define TMBONUSPT     target->state.points/200     // bonus if team mode (to give some extra reward for playing tdm modes)
#define HTFFRAGPT     cnumber/2                    // player frags while carrying the flag
#define CTFFRAGPT   2*cnumber                      // player frags the flag stealer
#define FRAGPT        10                           // player frags (normal)
#define HEADSHOTPT    15                           // player gibs with head shot
#define KNIFEPT       20                           // player gibs with the knife
#define SHOTGPT       12                           // player gibs with the shotgun
#define TKPT         -20                           // player tks
#define FLAGTKPT     -2*(10+cnumber)               // player tks the flag keeper/stealer

void flagpoints(client *c, int message)
{
    float distance = 0;
    int cnumber = totalclients < 13 ? totalclients : 12;
    switch (message)
    {
        case FM_PICKUP:
            c->md.flagpos.x = c->state.o.x;
            c->md.flagpos.y = c->state.o.y;
            c->md.flagmillis = servmillis;
            if (m_ctf) addpt(c, CTFPICKPT);
            break;
        case FM_DROP:
            if (m_ctf) addpt(c, CTFDROPPT);
            break;
        case FM_LOST:
            if (m_htf) addpt(c, HTFLOSTPT);
            else if (m_ctf) {
                distance = sqrt(POW2XY(c->state.o,c->md.flagpos));
                if (distance > 200) distance = 200;                   // ~200 is the distance between the flags in ac_depot
                addpt(c, CTFLOSTPT);
            }
            break;
        case FM_RETURN:
            addpt(c, CTFRETURNPT);
            break;
        case FM_SCORE:
            if (m_ctf) {
                distance = sqrt(POW2XY(c->state.o,c->md.flagpos));
                if (distance > 200) distance = 200;
#ifdef ACAC
                if ( validflagscore(distance,c) )
#endif
                    addpt(c, CTFSCOREPT);
            } else addpt(c, HTFSCOREPT);
            break;
        case FM_KTFSCORE:
            addpt(c, KTFSCOREPT);
            break;
        default:
            break;
    }
}


inline int minhits2combo(int gun)
{
    switch (gun)
    {
        case GUN_SUBGUN:
        case GUN_AKIMBO:
            return 4;
        case GUN_GRENADE:
        case GUN_ASSAULT:
            return 3;
        default:
            return 2;
    }
}

void checkcombo(client *target, client *actor, int damage, int gun)
{
    int diffhittime = servmillis - actor->md.lasthit;
    actor->md.lasthit = servmillis;
    if ((gun == GUN_SHOTGUN || gun == GUN_GRENADE) && damage < 20)
    {
        actor->md.lastgun = gun;
        return;
    }

    if ( diffhittime < 750 ) {
        if ( gun == actor->md.lastgun )
        {
            if ( diffhittime * 2 < guns[gun].attackdelay * 3 )
            {
                actor->md.combohits++;
                actor->md.combotime+=diffhittime;
                actor->md.combodamage+=damage;
                int mh2c = minhits2combo(gun);
                if ( actor->md.combohits > mh2c && actor->md.combohits % mh2c == 1 )
                {
                    if (actor->md.combo < 5) actor->md.combo++;
                    actor->md.ncombos++;
                }
            }
        }
        else
        {
            switch (gun)
            {
                case GUN_KNIFE:
                case GUN_PISTOL:
                    if ( guns[actor->md.lastgun].isauto ) break;
                case GUN_SNIPER:
                    if ( actor->md.lastgun > GUN_PISTOL ) break;
                default:
                    actor->md.combohits++;
                    actor->md.combotime+=diffhittime;
                    actor->md.combodamage+=damage;
                    if (actor->md.combo < 5) actor->md.combo++;
                    actor->md.ncombos++;
                    break;
            }
        }
    }
    else
    {
        actor->md.combo = 0;
        actor->md.combofrags = 0;
        actor->md.combotime = 0;
        actor->md.combodamage = 0;
        actor->md.combohits = 0;
    }
    actor->md.lastgun = gun;
}

#define COVERDIST 2000 // about 45 cubes
#define REPLYDIST 8000 // about 90 cubes
float coverdist = COVERDIST;

int checkteamrequests(int sender)
{
    int dtime, besttime = -1;
    int bestid = -1;
    client *ant = clients[sender];
    loopv(clients) {
        client *prot = clients[i];
        if ( i!=sender && prot->type != ST_EMPTY && prot->team == ant->team &&
             prot->state.state == CS_ALIVE && prot->md.askmillis > gamemillis && prot->md.ask >= 0 ) {
            float dist = POW2XY(ant->state.o,prot->state.o);
            if ( dist < REPLYDIST && (dtime=prot->md.askmillis-gamemillis) > besttime) {
                bestid = i;
                besttime = dtime;
            }
        }
    }
    if ( besttime >= 0 ) return bestid;
    return -1;
}

/** WIP */
void checkteamplay(int s, int sender)
{
    client *actor = clients[sender];

    if ( actor->state.state != CS_ALIVE ) return;
    switch(s){
        case S_IMONDEFENSE: // informs
            actor->md.linkmillis = gamemillis + 20000;
            actor->md.linkreason = s;
            break;
        case S_COVERME: // demands
        case S_STAYTOGETHER:
        case S_STAYHERE:
            actor->md.ask = s;
            actor->md.askmillis = gamemillis + 5000;
            break;
        case S_AFFIRMATIVE: // replies
        case S_ALLRIGHTSIR:
        case S_YES:
        {
            int id = checkteamrequests(sender);
            if ( id >= 0 ) {
                client *sgt = clients[id];
                actor->md.linked = id;
                if ( actor->md.linkmillis < gamemillis ) addpt(actor,REPLYPT);
                actor->md.linkmillis = gamemillis + 30000;
                actor->md.linkreason = sgt->md.ask;
                sendf(actor->clientnum, 1, "ri2", SV_HUDEXTRAS, HE_NUM+id);
                switch( actor->md.linkreason ) { // check demands
                    case S_STAYHERE:
                        actor->md.pos = sgt->state.o;
                        addpt(sgt,REPLYPT);
                        break;
                }
            }
            break;
        }
    }
}

void computeteamwork(int team, int exclude) // testing
{
    loopv(clients)
    {
        client *actor = clients[i];
        if ( i == exclude || actor->type == ST_EMPTY || actor->team != team || actor->state.state != CS_ALIVE || actor->md.linkmillis < gamemillis ) continue;
        vec position;
        bool teamworkdone = false;
        switch( actor->md.linkreason )
        {
            case S_IMONDEFENSE:
                position = actor->spawnp;
                teamworkdone = true;
                break;
            case S_STAYTOGETHER:
                if ( valid_client(actor->md.linked) ) position = clients[actor->md.linked]->state.o;
                teamworkdone = true;
                break;
            case S_STAYHERE:
                position = actor->md.pos;
                teamworkdone = true;
                break;
        }
        if ( teamworkdone )
        {
            float dist = POW2XY(actor->state.o,position);
            if (dist < COVERDIST)
            {
                addpt(actor,TWDONEPT);
                sendf(actor->clientnum, 1, "ri2", SV_HUDEXTRAS, HE_TEAMWORK);
            }
        }
    }
}

float a2c = 0, c2t = 0, a2t = 0; // distances: actor to covered, covered to target and actor to target

inline bool testcover(int msg, int factor, client *actor)
{
    if ( a2c < coverdist && c2t < coverdist && a2t < coverdist )
    {
        sendf(actor->clientnum, 1, "ri2", SV_HUDEXTRAS, msg);
        addpt(actor, factor);
        actor->md.ncovers++;
        return true;
    }
    return false;
}

#define CALCCOVER(C) \
    a2c = POW2XY(actor->state.o,C);\
    c2t = POW2XY(C,target->state.o);\
    a2t = POW2XY(actor->state.o,target->state.o)

bool validlink(client *actor, int cn)
{
    return actor->md.linked >= 0 && actor->md.linked == cn && gamemillis < actor->md.linkmillis && valid_client(actor->md.linked);
}

/** WIP */
void checkcover(client *target, client *actor)
{
    int team = actor->team;
    int oteam = team_opposite(team);

    bool covered = false;
    int coverid = -1;

    int cnumber = totalclients < 13 ? totalclients : 12;

    if ( m_flags ) {
        sflaginfo &f = sflaginfos[team];
        sflaginfo &of = sflaginfos[oteam];

        if ( m_ctf )
        {
            if ( f.state == CTFF_INBASE )
            {
                CALCCOVER(f);
                if ( testcover(HE_FLAGDEFENDED, CTFLDEFPT, actor) ) print_medal_messages(actor,CTFLDEF);
            }
            if ( of.state == CTFF_STOLEN && actor->clientnum != of.actor_cn )
            {
                covered = true; coverid = of.actor_cn;
                CALCCOVER(clients[of.actor_cn]->state.o);
                if ( testcover(HE_FLAGCOVERED, CTFLCOVPT, actor) ) print_medal_messages(actor,CTFLCOV);
            }
        }
        else if ( m_htf )
        {
            if ( actor->clientnum != f.actor_cn )
            {
                if ( f.state == CTFF_DROPPED )
                {
                    struct { short x, y; } nf;
                    nf.x = f.pos[0]; nf.y = f.pos[1];
                    CALCCOVER(nf);
                    if ( testcover(HE_FLAGDEFENDED, HTFLDEFPT, actor) ) print_medal_messages(actor,HTFLDEF);
                }
                if ( f.state == CTFF_STOLEN )
                {
                    covered = true; coverid = f.actor_cn;
                    CALCCOVER(clients[f.actor_cn]->state.o);
                    if ( testcover(HE_FLAGCOVERED, HTFLCOVPT, actor) ) print_medal_messages(actor,HTFLCOV);
                }
            }
        }
    }

    if ( !(covered && actor->md.linked==coverid) && validlink(actor,actor->md.linked) )
    {
        CALCCOVER(clients[actor->md.linked]->state.o);
        if ( testcover(HE_COVER, COVERPT, actor) ) print_medal_messages(actor,COVER);
    }

}

#undef CALCCOVER

/** WiP */
void checkfrag(client *target, client *actor, int gun, bool gib)
{
    int targethasflag = clienthasflag(target->clientnum);
    int actorhasflag = clienthasflag(actor->clientnum);
    int cnumber = totalclients < 13 ? totalclients : 12;
    addpt(target,DEATHPT);
    if(target!=actor) {
        if(!isteam(target->team, actor->team)) {

            if (m_teammode) {
                if(!m_flags) addpt(actor, TMBONUSPT);
                else addpt(actor, FLBONUSPT);

                checkcover (target, actor);
                if ( m_htf && actorhasflag >= 0 ) addpt(actor, HTFFRAGPT);

                if ( m_ctf && targethasflag >= 0 ) {
                    addpt(actor, CTFFRAGPT);
                }
            }
            else addpt(actor, BONUSPT);

            if (gib && gun != GUN_GRENADE) {
                if ( gun == GUN_SNIPER ) {
                    addpt(actor, HEADSHOTPT);
                    actor->md.nhs++;
                }
                else if ( gun == GUN_KNIFE ) addpt(actor, KNIFEPT);
                else if ( gun == GUN_SHOTGUN ) addpt(actor, SHOTGPT);
            }
            else addpt(actor, FRAGPT);

            if ( actor->md.combo ) {
                actor->md.combofrags++;
                addpt(actor,COMBOPT);
                actor->md.combosend = true;
            }

        } else {

            if ( targethasflag >= 0 ) addpt(actor, FLAGTKPT);
            else addpt(actor, TKPT);

        }
    }
}

int next_afk_check = 200;

/* this function is managed to the PUBS, id est, many people playing in an open server */
void check_afk()
{
    next_afk_check = servmillis + 7 * 1000;
    /* if we have few people (like 2x2), or it is not a teammode with the server not full: do nothing! */
    if ( totalclients < 5 || ( totalclients < scl.maxclients && !m_teammode)  ) return;
    loopv(clients)
    {
        client &c = *clients[i];
        if ( c.type != ST_TCPIP || c.connectmillis + 60 * 1000 > servmillis ||
             c.inputmillis + scl.afk_limit > servmillis || clienthasflag(c.clientnum) != -1 ) continue;
        if ( ( c.state.state == CS_DEAD && !m_arena && c.state.lastdeath + 45 * 1000 < gamemillis) ||
             ( c.state.state == CS_ALIVE && c.upspawnp ) /*||
             ( c.state.state == CS_SPECTATE && totalclients >= scl.maxclients )  // only kick spectator if server is full - 2011oct16:flowtron: mmh, that seems reasonable enough .. still, kicking spectators for inactivity seems harsh! disabled ATM, kick them manually if you must.
             */
            )
        {
            logline(ACLOG_INFO, "[%s] %s %s", c.hostname, c.name, "is afk");
            defformatstring(msg)("%s is afk", c.name);
            sendservmsg(msg);
            disconnect_client(c.clientnum, DISC_AFK);
        }
    }
}

/** This function counts how much non-killing-damage the player does to any teammates
    The damage limit is 100 hp per minute, which is about 2 tks per minute in a normal game
    In normal games, the players go over 6 tks only in the worst cases */
void check_ffire(client *target, client *actor, int damage)
{
    if ( mastermode != MM_OPEN ) return;
    actor->ffire += damage;
    if ( actor->ffire > 300 && actor->ffire * 600 > gamemillis) {
        logline(ACLOG_INFO, "[%s] %s %s", actor->hostname, actor->name, "kicked for excessive friendly fire");
        defformatstring(msg)("%s %s", actor->name, "kicked for excessive friendly fire");
        sendservmsg(msg);
        disconnect_client(actor->clientnum, DISC_FFIRE);
    }
}

inline int check_pdist(client *c, float & dist) // pick up distance
{
    // ping 1000ms at max velocity can produce an error of 20 cubes
    float delay = 9.0f + (float)c->ping * 0.02f + (float)c->spj * 0.025f; // at pj/ping 40/100, delay = 12
    if ( dist > delay )
    {
        if ( dist < 1.5f * delay ) return 1;
#ifdef ACAC
        pickup_checks(c,dist-delay);
#endif
        return 2;
    }
    return 0;
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

inline void checkmove(client *cl)
{
    cl->ldt = gamemillis - cl->lmillis;
    cl->lmillis = gamemillis;
    if ( cl->ldt < 40 ) cl->ldt = 40;
    cl->t += cl->ldt;
    cl->spj = (( 7 * cl->spj + cl->ldt ) >> 3);

    if ( cl->input != cl->f )
    {
        cl->input = cl->f;
        cl->inputmillis = servmillis;
    }

    if(maplayout) checkclientpos(cl);

#ifdef ACAC
    m_engine(cl);
#endif

    if ( !cl->upspawnp )
    {
        cl->spawnp = cl->state.o;
        cl->upspawnp = true;
        cl->spj = cl->ldt = 40;
    }

    return;
}

inline void checkshoot(int & cn, gameevent & shot, int & hits, int & tcn)
{
#ifdef ACAC
    s_engine(cn, shot, hits, tcn);
#endif
    return;
}

inline void checkweapon(int & type, int & var)
{
#ifdef ACAC
    w_engine(type,var);
#endif
    return;
}

bool validdamage(client *&target, client *&actor, int &damage, int &gun, bool &gib)
{
#ifdef ACAC
    if (!d_engine(target, actor, damage, gun, gib)) return false;
#endif
    return true;
}

inline int checkmessage(client *c, int type)
{
#ifdef ACAC
    type = p_engine(c,type);
#endif
    return type;
}

