inline bool is_lagging(client *cl)
{
    return ( cl->spj > 50 || cl->ping > 500 || cl->ldt > 80 ); // do not change this except if you really know what are you doing
}

inline bool outside_border(vec &po)
{
    return sg->curmap && (po.x < sg->curmap->x1 || po.y < sg->curmap->y1 || po.x > sg->curmap->x2 || po.y > sg->curmap->y2 || po.z < sg->curmap->zmin || po.z > sg->curmap->zmax);
}

inline void checkclientpos(client *cl)
{
    vec &po = cl->state.o;
    if(outside_border(po) || (sg->curmap && sg->layout && sg->layout[((int) po.x) + (((int) po.y) << sg->curmap->sfactor)] > po.z + 3))
    {
        if(sg->gamemillis > 10000 && (servmillis - cl->connectmillis) > 10000) cl->mapcollisions++;
        if(cl->mapcollisions && !(cl->mapcollisions % 25))
        {
            xlog(ACLOG_INFO, "[%s] %s is colliding with the map", cl->hostname, cl->name);
        }
    }
}

extern inline void addban(client *cl, int reason, int type = BAN_AUTO);

#ifdef ACAC
#include "anticheat.h"
#endif

// AC1.2 code::
// we kept the server logging but dropped all SV_HUDEXTRA messages

#define POW2XY(A,B) (pow2(A.x-B.x)+pow2(A.y-B.y))

const char * effort_messages[] = { "defended the flag", "covered the flag stealer", "defended the dropped flag", "covered the flag keeper", "covered teammate" };
enum { CTFLDEF, CTFLCOV, HTFLDEF, HTFLCOV, COVER, EFFORTMESSAGENUM };
inline void print_effort_messages(client *c, int n)
{
    if(n<0 || n>=EFFORTMESSAGENUM) return;
    mlog(ACLOG_VERBOSE, "[%s] AT3-ACHIEVEMENT: %s %s", c->hostname, c->name, effort_messages[n]);
}

inline void achievement(client *c, int points, int n = -1) {
    c->state.at3_points += points;
    c->effort.deltapoints += points;
    c->effort.updatemillis = sg->gamemillis + 240; // about 2 AR shots
    print_effort_messages(c,n);
}

// clcount is the number of players in the game (<=12)
#define CTFPICKPT     clcount                      // player picked the flag (ctf)
#define CTFDROPPT    -clcount                      // player dropped the flag to other player (probably)
#define HTFLOSTPT  -2*clcount                      // penalty
#define CTFLOSTPT     clcount*distance/100         // bonus: 1/4 of the flag score bonus
#define CTFRETURNPT   clcount                      // flag return
#define CTFSCOREPT   (clcount*distance/25+10)      // flag score
#define HTFSCOREPT   (clcount*4+10)
#define KTFSCOREPT   (clcount*2+10)
#define COMBOPT       5                            // player frags with combo
#define REPLYPT       2                            // reply success
#define TWDONEPT      5                            // team work done
#define CTFLDEFPT     clcount                      // player defended the flag in the base (ctf)
#define CTFLCOVPT     clcount*2                    // player covered the flag stealer (ctf)
#define HTFLDEFPT     clcount                      // player defended a droped flag (htf)
#define HTFLCOVPT     clcount*3                    // player covered the flag keeper (htf)
#define COVERPT       clcount*2                    // player covered teammate
#define DEATHPT      -4                            // player died
#define SLAYBONUSPT   target->state.at3_points/400 // bonus for killing high level enemies – CAUTION: this establishes a feedback system (https://en.wikipedia.org/wiki/Feedback)
#define FLAGBONUSPT   target->state.at3_points/300 // bonus if flag team mode
#define TEAMBONUSPT   target->state.at3_points/200 // bonus if team mode (to give some extra reward for playing tdm modes)
#define HTFFRAGPT     clcount/2                    // player frags while carrying the flag
#define CTFFRAGPT   2*clcount                      // player frags the flag stealer
#define FRAGPT        10                           // player frags (normal)
#define HEADSHOTPT    15                           // player gibs with head shot
#define KNIFEPT       20                           // player gibs with the knife
#define SHOTGPT       12                           // player gibs with the shotgun
#define TKPT         -20                           // player tks
#define FLAGTKPT     -2*(10+clcount)               // player tks the flag keeper/stealer

void flagpoints(client *c, int message)
{
    float distance = 0;
    int clcount = min(12, totalclients);
    // flag distance is capped at 200 (distance between the flags in ac_depot)
    switch (message)
    {
        case FM_PICKUP:
            c->effort.flagpos.x = c->state.o.x;
            c->effort.flagpos.y = c->state.o.y;
            if(m_ctf) achievement(c, CTFPICKPT);
            break;
        case FM_DROP:
            if(m_ctf) achievement(c, CTFDROPPT);
            break;
        case FM_LOST:
            if(m_htf) achievement(c, HTFLOSTPT);
            else if(m_ctf)
            {
                distance = min(200, (int)sqrt(POW2XY(c->state.o, c->effort.flagpos)));
                achievement(c, CTFLOSTPT);
            }
            break;
        case FM_RETURN:
            achievement(c, CTFRETURNPT);
            break;
        case FM_SCORE:
            if(m_ctf)
            {
                distance = min(200, (int)sqrt(POW2XY(c->state.o,c->effort.flagpos)));
#ifdef ACAC
                if(validflagscore(distance,c))
#endif
                    achievement(c, CTFSCOREPT);
            }else{
                achievement(c, HTFSCOREPT);
            }
            break;
        case FM_KTFSCORE:
            achievement(c, KTFSCOREPT);
            break;
        default:
            break;
    }
}

inline int hitreq4combo(int gun)
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
    int diffhittime = servmillis - actor->effort.lasthit;
    actor->effort.lasthit = servmillis;
    if((gun == GUN_SHOTGUN || gun == GUN_GRENADE) && damage < 20)
    {
        actor->effort.lastgun = gun;
        return;
    }
    if(diffhittime < 750)
    {
        if(gun == actor->effort.lastgun)
        {
            if(diffhittime * 2 < guns[gun].attackdelay * 3)
            {
                actor->effort.combohits++;
                int hr4c = hitreq4combo(gun);
                if(actor->effort.combohits > hr4c && actor->effort.combohits % hr4c == 1)
                {
                    if(actor->effort.combocount < 5) actor->effort.combocount++;
                }
            }
        }
        else
        {
            switch(gun)
            {
                case GUN_KNIFE:
                case GUN_PISTOL:
                    if(guns[actor->effort.lastgun].isauto) break;
                case GUN_SNIPER:
                    if(actor->effort.lastgun > GUN_PISTOL) break;
                default:
                    actor->effort.combohits++;
                    if(actor->effort.combocount < 5) actor->effort.combocount++;
                    break;
            }
        }
    }
    else
    {
        actor->effort.combohits = 0;
        actor->effort.combocount = 0;
    }
    actor->effort.lastgun = gun;
}

#define COVERDIST 2000 // about 45 cubes
#define REPLYDIST 8000 // about 90 cubes
float coverdist = COVERDIST;

bool callercheck(client *c)
{
    return c->type != ST_EMPTY
        && c->state.state == CS_ALIVE
        && c->effort.askmillis > sg->gamemillis
        && c->effort.asktask >= 0;
}

int checkteamrequests(int sender)
{
    int dtime, besttime = -1;
    int bestid = -1;
    client *radio_out = clients[sender];
    loopv(clients)
    {
        client *radio_in = clients[i];
        if(i != sender && radio_in->team == radio_out->team && callercheck(radio_in))
        {
            float distance = POW2XY(radio_out->state.o, radio_in->state.o);
            if( distance < REPLYDIST && (dtime = radio_in->effort.askmillis - sg->gamemillis) > besttime)
            {
                bestid = i;
                besttime = dtime;
            }
        }
    }
    if(besttime >= 0) return bestid;
    return -1;
}

void checkteamplay(int radiosignal, int sender)
{
    client *actor = clients[sender];
    if(actor->state.state != CS_ALIVE) return;
    switch(radiosignal){
        case S_IMONDEFENSE: // informs
            actor->effort.linkmillis = sg->gamemillis + 20000;
            actor->effort.linkreason = radiosignal;
            break;
        case S_COVERME: // demands
        case S_STAYTOGETHER:
        case S_STAYHERE:
            actor->effort.asktask = radiosignal;
            actor->effort.askmillis = sg->gamemillis + 5000;
            break;
        case S_AFFIRMATIVE: // replies
        case S_ALLRIGHTSIR:
        case S_YES:
        {
            int id = checkteamrequests(sender);
            if(id >= 0)
            {
                client *taskmaster = clients[id];
                actor->effort.linked = id;
                if(actor->effort.linkmillis < sg->gamemillis) achievement(actor, REPLYPT);
                actor->effort.linkmillis = sg->gamemillis + 30000;
                actor->effort.linkreason = taskmaster->effort.asktask;
                switch(actor->effort.linkreason) // check demands
                {
                    case S_STAYHERE:
                        actor->effort.taskpos = taskmaster->state.o;
                        achievement(taskmaster, REPLYPT);
                        break;
                }
            }
            break;
        }
    }
}

void computeteamwork(int team, int exclude)
{
    loopv(clients)
    {
        client *actor = clients[i];
        if(i == exclude || actor->type == ST_EMPTY || actor->team != team || actor->state.state != CS_ALIVE || actor->effort.linkmillis < sg->gamemillis ) continue;
        vec position;
        bool teamworkdone = false;
        switch(actor->effort.linkreason)
        {
            case S_IMONDEFENSE:
                position = actor->spawnpos;
                teamworkdone = true;
                break;
            case S_STAYTOGETHER:
                if(valid_client(actor->effort.linked)) position = clients[actor->effort.linked]->state.o;
                teamworkdone = true;
                break;
            case S_STAYHERE:
                position = actor->effort.taskpos;
                teamworkdone = true;
                break;
        }
        if(teamworkdone)
        {
            float distance = POW2XY(actor->state.o,position);
            if(distance < COVERDIST)
            {
                achievement(actor, TWDONEPT);
            }
        }
    }
}

float dist_a2c = 0, dist_c2t = 0, dist_a2t = 0; // distances: actor to covered, covered to target and actor to target

inline bool testcover(int factor, client *actor)
{
    if(dist_a2c < coverdist && dist_c2t < coverdist && dist_a2t < coverdist)
    {
        achievement(actor, factor);
        return true;
    }
    return false;
}

#define CALCCOVER(C) \
    dist_a2c = POW2XY(actor->state.o,C);\
    dist_c2t = POW2XY(C,target->state.o);\
    dist_a2t = POW2XY(actor->state.o,target->state.o)

bool validlink(client *actor, int cn)
{
    return actor->effort.linked >= 0 && actor->effort.linked == cn && sg->gamemillis < actor->effort.linkmillis && valid_client(actor->effort.linked);
}

void checkcover(client *target, client *actor)
{
    int myteam = actor->team;
    int opteam = team_opposite(myteam);
    bool covered = false;
    int coverid = -1;
    int clcount = min(12, totalclients);
    if(m_flags_)
    {
        sflaginfo &myflag = sg->sflaginfos[myteam];
        sflaginfo &opflag = sg->sflaginfos[opteam];
        if(m_ctf)
        {
            if(myflag.state == CTFF_INBASE)
            {
                CALCCOVER(myflag);
                if(testcover(CTFLDEFPT, actor)) print_effort_messages(actor, CTFLDEF);
            }
            if(opflag.state == CTFF_STOLEN && actor->clientnum != opflag.actor_cn)
            {
                covered = true;
                coverid = opflag.actor_cn;
                CALCCOVER(clients[opflag.actor_cn]->state.o);
                if(testcover(CTFLCOVPT, actor)) print_effort_messages(actor, CTFLCOV);
            }
        }
        else if(m_htf)
        {
            if(actor->clientnum != myflag.actor_cn)
            {
                if(myflag.state == CTFF_DROPPED)
                {
                    struct { short x, y; } nf;
                    nf.x = myflag.pos[0];
                    nf.y = myflag.pos[1];
                    CALCCOVER(nf);
                    if(testcover(HTFLDEFPT, actor)) print_effort_messages(actor, HTFLDEF);
                }
                if(myflag.state == CTFF_STOLEN)
                {
                    covered = true;
                    coverid = myflag.actor_cn;
                    CALCCOVER(clients[myflag.actor_cn]->state.o);
                    if(testcover(HTFLCOVPT, actor)) print_effort_messages(actor, HTFLCOV);
                }
            }
        }
    }

    if(!(covered && actor->effort.linked==coverid) && validlink(actor,actor->effort.linked))
    {
        CALCCOVER(clients[actor->effort.linked]->state.o);
        if(testcover(COVERPT, actor)) print_effort_messages(actor, COVER);
    }

}

#undef CALCCOVER

void checkfrag(client *target, client *actor, int gun, bool gib)
{
    int targethasflag = clienthasflag(target->clientnum);
    int actorhasflag = clienthasflag(actor->clientnum);
    int clcount = min(12, totalclients);
    achievement(target, DEATHPT);
    if(target != actor) {
        if(!isteam(target->team, actor->team))
        {
            if(m_teammode)
            {
                achievement(actor, m_flags_ ? FLAGBONUSPT : TEAMBONUSPT);
                checkcover(target, actor);
                if(m_htf && actorhasflag >= 0) achievement(actor, HTFFRAGPT);
                if(m_ctf && targethasflag >= 0) achievement(actor, CTFFRAGPT);
            }
            else achievement(actor, SLAYBONUSPT);

            if(gib && gun != GUN_GRENADE)
            {
                int typept = -1;
                switch(gun)
                {
                    case GUN_SNIPER: typept = HEADSHOTPT; break;
                    case GUN_KNIFE: typept = KNIFEPT; break;
                    case GUN_SHOTGUN: typept = SHOTGPT; break;
                    default: break;
                }
                if(typept > -1) achievement(actor, typept);
            }
            else achievement(actor, FRAGPT);

            if(actor->effort.combocount) achievement(actor, COMBOPT);
        }else{
            achievement(actor, (targethasflag >= 0) ? FLAGTKPT : TKPT);
        }
    }
}
// ::AC1.2 code

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
        if ( c.type != ST_TCPIP || c.role > CR_DEFAULT || c.connectmillis + 60 * 1000 > servmillis ||
             clienthasflag(c.clientnum) != -1 ) continue;
        if ( ( c.state.state == CS_DEAD && !m_arena && (c.state.lastdeath + scl.afk_limit) < sg->gamemillis) ||
             ( c.state.state == CS_ALIVE && c.state.lastclaction && (c.state.lastclaction + scl.afk_limit) < sg->gamemillis) /*||
             ( c.state.state == CS_SPECTATE && totalclients >= scl.maxclients )  // only kick spectator if server is full - 2011oct16:flowtron: mmh, that seems reasonable enough .. still, kicking spectators for inactivity seems harsh! disabled ATM, kick them manually if you must.
             */
            )
        {
            xlog(ACLOG_INFO, "[%s] %s %s", c.hostname, c.name, "is afk");
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
    if ( sg->mastermode != MM_OPEN ) return;
    actor->ffire += damage;
    if ( actor->ffire > 300 && actor->ffire * 600 > sg->gamemillis) {
        xlog(ACLOG_INFO, "[%s] %s %s", actor->hostname, actor->name, "kicked for excessive friendly fire");
        defformatstring(msg)("%s %s", actor->name, "kicked for excessive friendly fire");
        sendservmsg(msg);
        disconnect_client(actor->clientnum, DISC_FFKICK);
    }
}

inline int check_pdist(client *c, float & dist, float bubble = 1.5f) // pick up distance
{
    // ping 1000ms at max velocity can produce an error of 20 cubes
    float delay = 9.0f + (float)c->ping * 0.02f + (float)c->spj * 0.025f; // at pj/ping 40/100, delay = 12
    if(dist > delay)
    {
        if(dist < bubble * delay ) return 1;
#ifdef ACAC
        pickup_checks(c,dist-delay);
#endif
        return 2;
    }
    return 0;
}

inline void keepspawnpos(client *cl)
{
    if(!cl->keptspawnpos)
    {
        cl->spawnpos = cl->state.o;
        cl->keptspawnpos = true;
    }
}
