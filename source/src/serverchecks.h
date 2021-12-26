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

