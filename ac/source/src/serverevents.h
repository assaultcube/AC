// processing of server events

void processevent(client *c, explodeevent &e)
{
    clientstate &gs = c->state;
    switch(e.gun)
    {
        case GUN_GRENADE:
            if(!gs.grenades.remove(e.id)) return;
            break;

        default:
            return;
    }
    for(int i = 1; i<c->events.length() && c->events[i].type==GE_HIT; i++)
    {
        hitevent &h = c->events[i].hit;
        if(!clients.inrange(h.target)) continue;
        client *target = clients[h.target];
        if(target->type==ST_EMPTY || target->state.state!=CS_ALIVE || h.lifesequence!=target->state.lifesequence || h.dist<0 || h.dist>EXPDAMRAD) continue;

        int j = 1;
        for(j = 1; j<i; j++) if(c->events[j].hit.target==h.target) break;
        if(j<i) continue;

        int damage = int(guns[e.gun].damage*(1-h.dist/EXPDAMRAD));
        serverdamage(target, c, damage, e.gun, true, h.dir);
    }
}

void processevent(client *c, shotevent &e)
{
    clientstate &gs = c->state;
    int wait = e.millis - gs.lastshot;
    if(!gs.isalive(gamemillis) ||
       e.gun<GUN_KNIFE || e.gun>=NUMGUNS ||
       wait<gs.gunwait[e.gun] ||
       gs.mag[e.gun]<=0)
        return;
    if(e.gun!=GUN_KNIFE) gs.mag[e.gun]--;
    loopi(NUMGUNS) if(gs.gunwait[i]) gs.gunwait[i] = max(gs.gunwait[i] - (e.millis-gs.lastshot), 0);
    gs.lastshot = e.millis;
    gs.gunwait[e.gun] = attackdelay(e.gun);
    if(e.gun==GUN_PISTOL && gs.akimbomillis>gamemillis) gs.gunwait[e.gun] /= 2;
    sendf(-1, 1, "ri6x", SV_SHOTFX, c->clientnum, e.gun,
//         int(e.from[0]*DMF), int(e.from[1]*DMF), int(e.from[2]*DMF),
        int(e.to[0]*DMF), int(e.to[1]*DMF), int(e.to[2]*DMF),
        c->clientnum);
    gs.shotdamage += guns[e.gun].damage*(e.gun==GUN_SHOTGUN ? SGRAYS : 1);
    switch(e.gun)
    {
        case GUN_GRENADE: gs.grenades.add(e.id); break;
        default:
        {
            int totalrays = 0, maxrays = e.gun==GUN_SHOTGUN ? SGRAYS : 1;
            for(int i = 1; i<c->events.length() && c->events[i].type==GE_HIT; i++)
            {
                hitevent &h = c->events[i].hit;
                if(!clients.inrange(h.target)) continue;
                client *target = clients[h.target];
                if(target->type==ST_EMPTY || target->state.state!=CS_ALIVE || h.lifesequence!=target->state.lifesequence) continue;

                int rays = e.gun==GUN_SHOTGUN ? h.info : 1;
                if(rays<1) continue;
                totalrays += rays;
                if(totalrays>maxrays) continue;

                int damage = rays*guns[e.gun].damage;
                bool gib = false;
                if(e.gun==GUN_KNIFE || (e.gun==GUN_SHOTGUN && rays==maxrays)) gib = true;
                else if(e.gun==GUN_SNIPER) gib = h.info!=0;
                if(e.gun==GUN_SNIPER && gib) damage *= 3;
                serverdamage(target, c, damage, e.gun, gib, h.dir);
            }
            break;
        }
    }
}

void processevent(client *c, suicideevent &e)
{
    serverdamage(c, c, 1000, GUN_KNIFE, false);
}

void processevent(client *c, pickupevent &e)
{
    clientstate &gs = c->state;
    if(m_mp(gamemode) && !gs.isalive(gamemillis)) return;
    serverpickup(e.ent, c->clientnum);
}

void processevent(client *c, scopeevent &e) // FIXME remove in the next protocol change
{
    clientstate &gs = c->state;
    if(!gs.isalive(gamemillis)/* || e.gun!=GUN_SNIPER*/) return; // currently we check the gun on the client-side only
    gs.scoped = e.scoped;
    sendf(-1, 1, "ri3", SV_SCOPE, c->clientnum, gs.scoped);
}

void processevent(client *c, reloadevent &e)
{
    clientstate &gs = c->state;
    if(!gs.isalive(gamemillis) ||
       e.gun<GUN_KNIFE || e.gun>=NUMGUNS ||
       !reloadable_gun(e.gun) ||
       gs.ammo[e.gun]<=0)
        return;

    bool akimbo = e.gun==GUN_PISTOL && gs.akimbomillis>e.millis;
    int mag = (akimbo ? 2 : 1) * magsize(e.gun), numbullets = min(gs.ammo[e.gun], mag - gs.mag[e.gun]);
    if(numbullets<=0) return;

    gs.mag[e.gun] += numbullets;
    gs.ammo[e.gun] -= numbullets;

    int wait = e.millis - gs.lastshot;
    /*
    // actually the clients sends this by itself, but sanity checks should be made!!
    // does this state get reset on spawn etc?
    if(gs.scoped) // the state of this needs to be verified in a number of places! (flowtron:2010jul05)
    {
        gs.scoped = false;
        sendf(-1, 1, "ri3", SV_SCOPE, c->clientnum, gs.scoped);
    }
    */
    sendf(-1, 1, "ri3", SV_RELOAD, c->clientnum, e.gun);
    if(gs.gunwait[e.gun] && wait<gs.gunwait[e.gun]) gs.gunwait[e.gun] += reloadtime(e.gun);
    else
    {
        loopi(NUMGUNS) if(gs.gunwait[i]) gs.gunwait[i] = max(gs.gunwait[i] - (e.millis-gs.lastshot), 0);
        gs.lastshot = e.millis;
        gs.gunwait[e.gun] += reloadtime(e.gun);
    }
}

void processevent(client *c, akimboevent &e)
{
    clientstate &gs = c->state;
    if(!gs.isalive(gamemillis) || gs.akimbomillis) return;
    gs.akimbomillis = e.millis+30000;
}

void clearevent(client *c)
{
    int n = 1;
    while(n<c->events.length() && c->events[n].type==GE_HIT) n++;
    c->events.remove(0, n);
}

void processevents()
{
    loopv(clients)
    {
        client *c = clients[i];
        if(c->type==ST_EMPTY) continue;
        if(c->state.akimbomillis && c->state.akimbomillis < gamemillis) { c->state.akimbomillis = 0; c->state.akimbo = false; }
        while(c->events.length())
        {
            gameevent &e = c->events[0];
            if(e.type<GE_SUICIDE)
            {
                if(e.shot.millis>gamemillis) break;
                if(e.shot.millis<c->lastevent) { clearevent(c); continue; }
                c->lastevent = e.shot.millis;
            }
            switch(e.type)
            {
                case GE_SHOT: processevent(c, e.shot); break;
                case GE_EXPLODE: processevent(c, e.explode); break;
                case GE_AKIMBO: processevent(c, e.akimbo); break;
                case GE_RELOAD: processevent(c, e.reload); break;
                case GE_SCOPING: processevent(c, e.scoping); break;
                // untimed events
                case GE_SUICIDE: processevent(c, e.suicide); break;
                case GE_PICKUP: processevent(c, e.pickup); break;
            }
            clearevent(c);
        }
    }
}

