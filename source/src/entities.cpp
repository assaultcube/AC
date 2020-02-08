// entities.cpp: map entity related functions (pickup etc.)

#include "cube.h"

VAR(showclips, 0, 1, 1);
VAR(showmodelclipping, 0, 0, 1);

vector<entity> ents;
vector<int> eh_ents; // edithide entities
const char *entmdlnames[] =
 {
     "pistolclips", "ammobox", "nade", "health", "helmet", "kevlar", "akimbo", "nades", //FIXME
 };

 void renderent(entity &e)
 {
     /* FIXME: if the item list change, this hack will be messed */

     defformatstring(widn)("modmdlpickup%d", e.type-3);
     defformatstring(mdlname)("pickups/%s", identexists(widn)?getalias(widn):

     entmdlnames[e.type-I_CLIPS+(m_lss && e.type==I_GRENADE ? 5:0)]);

     float z = (float)(1+sinf(lastmillis/100.0f+e.x+e.y)/20), yaw = lastmillis/10.0f;
     rendermodel(mdlname, ANIM_MAPMODEL|ANIM_LOOP|ANIM_DYNALLOC, 0, 0, vec(e.x, e.y, z+S(e.x, e.y)->floor+e.attr1), yaw, 0);
 }

void renderclip(entity &e)
{
    float xradius = max(float(e.attr2), 0.1f), yradius = max(float(e.attr3), 0.1f);
    vec bbmin(e.x - xradius, e.y - yradius, float(S(e.x, e.y)->floor+e.attr1)),
        bbmax(e.x + xradius, e.y + yradius, bbmin.z + max(float(e.attr4), 0.1f));

    glDisable(GL_TEXTURE_2D);
    switch(e.type)
    {
        case CLIP:     linestyle(1, 0xFF, 0xFF, 0); break;  // yellow
        case MAPMODEL: linestyle(1, 0, 0xFF, 0);    break;  // green
        case PLCLIP:   linestyle(1, 0xFF, 0, 0xFF); break;  // magenta
    }
    glBegin(GL_LINES);

    glVertex3f(bbmin.x, bbmin.y, bbmin.z);
    loopi(2) glVertex3f(bbmax.x, bbmin.y, bbmin.z);
    loopi(2) glVertex3f(bbmax.x, bbmax.y, bbmin.z);
    loopi(2) glVertex3f(bbmin.x, bbmax.y, bbmin.z);
    glVertex3f(bbmin.x, bbmin.y, bbmin.z);

    glVertex3f(bbmin.x, bbmin.y, bbmax.z);
    loopi(2) glVertex3f(bbmax.x, bbmin.y, bbmax.z);
    loopi(2) glVertex3f(bbmax.x, bbmax.y, bbmax.z);
    loopi(2) glVertex3f(bbmin.x, bbmax.y, bbmax.z);
    glVertex3f(bbmin.x, bbmin.y, bbmax.z);

    loopi(8) glVertex3f(i&2 ? bbmax.x : bbmin.x, i&4 ? bbmax.y : bbmin.y, i&1 ? bbmax.z : bbmin.z);

    glEnd();
    glEnable(GL_TEXTURE_2D);
}

void rendermapmodels()
{
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==MAPMODEL)
        {
            mapmodelinfo *mmi = getmminfo(e.attr2);
            if(!mmi) continue;
            rendermodel(mmi->name, ANIM_MAPMODEL|ANIM_LOOP, e.attr4, 0, vec(e.x, e.y, (float)S(e.x, e.y)->floor+mmi->zoff+e.attr3), (float)((e.attr1+7)-(e.attr1+7)%15), 0, 10.0f);
        }
    }
}

void showedithide()
{
    loopv(eh_ents)
    {
        if(eh_ents[i]>0 && eh_ents[i]<MAXENTTYPES) { conoutf("#%02d: %d : %s", i, eh_ents[i], entnames[eh_ents[i]]); }
        else { conoutf("#%02d: %d : -n/a-", i, eh_ents[i]);  }
    }
}
COMMAND(showedithide, "");

void setedithide(char *text) // FIXME: human indexing inside
{
    eh_ents.setsize(0);
    if(text && text[0] != '\0')
    {
        const char *s = strtok(text, " ");
        do
        {
            bool k = false;
            int sn = -1;
            int tn = atoi(s);
            loopi(MAXENTTYPES) if(!strcmp(entnames[i], s)) sn = i;
            if(sn!=-1) { loopv(eh_ents) { if(eh_ents[i]==sn) { k = true; } } }
            else sn = tn;
            if(!k) { if(sn>0 && sn<MAXENTTYPES) eh_ents.add(sn); }
            s = strtok(NULL, " ");
        }
        while(s);
    }
}
COMMAND(setedithide, "c");

void seteditshow(char *just)
{
    eh_ents.setsize(0);
    if(just && just[0] != '\0')
    {
        const char *s = strtok(just, " ");
        int sn = -1;
        int tn = atoi(s);
        loopi(MAXENTTYPES) if(!strcmp(entnames[i], s)) sn = i;
        if(sn==-1) sn = tn;
        loopi(MAXENTTYPES-1)
        {
            int j = i+1;
            if(j!=sn) eh_ents.add(j);
        }
    }
}
COMMAND(seteditshow, "s");

void renderentarrow(const entity &e, const vec &dir, float radius)
{
    if(radius <= 0) return;
    float arrowsize = min(radius/8, 0.5f);
    vec epos(e.x, e.y, e.z);
    vec target = vec(dir).mul(radius).add(epos), arrowbase = vec(dir).mul(radius - arrowsize).add(epos), spoke;
    spoke.orthogonal(dir);
    spoke.normalize();
    spoke.mul(arrowsize);
    glDisable(GL_TEXTURE_2D); // this disables reaction to light, but also emphasizes shadows .. a nice effect, but should be independent
    glDisable(GL_CULL_FACE);
    glLineWidth(3);
    glBegin(GL_LINES);
    glVertex3fv(epos.v);
    glVertex3fv(target.v);
    glEnd();
    glBegin(GL_TRIANGLE_FAN);
    glVertex3fv(target.v);
    loopi(5)
    {
        vec p(spoke);
        p.rotate(2*M_PI*i/4.0f, dir);
        p.add(arrowbase);
        glVertex3fv(p.v);
    }
    glEnd();
    glLineWidth(1);
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
}

void renderentities()
{
    int closest = editmode ? closestent() : -1;
    if(editmode && !reflecting && !refracting && !stenciling)
    {
        static int lastsparkle = 0;
        if(lastmillis - lastsparkle >= 20)
        {
            lastsparkle = lastmillis - (lastmillis%20);
            // closest see above
            loopv(ents)
            {
                entity &e = ents[i];
                if(e.type==NOTUSED) continue;
                bool ice = false;
                loopk(eh_ents.length()) if(eh_ents[k]==e.type) ice = true;
                if(ice) continue;
                vec v(e.x, e.y, e.z);
                if(vec(v).sub(camera1->o).dot(camdir) < 0) continue;
                //particle_splash(i == closest ? PART_ELIGHT : PART_ECLOSEST, 2, 40, v);
                int sc = PART_ECARROT; // "carrot" (orange) - entity slot currently unused, possibly "reserved"
                if(i==closest)
                {
                    sc = PART_ECLOSEST; // blue
                }
                else switch(e.type)
                {
                    case LIGHT : sc = PART_ELIGHT; break; // white
                    case PLAYERSTART: sc = PART_ESPAWN; break; // green
                    case I_CLIPS:
                    case I_AMMO:
                    case I_GRENADE: sc = PART_EAMMO; break; // red
                    case I_HEALTH:
                    case I_HELMET:
                    case I_ARMOUR:
                    case I_AKIMBO: sc = PART_EPICKUP; break; // yellow
                    case MAPMODEL:
                    case SOUND: sc = PART_EMODEL; break; // magenta
                    case LADDER:
                    case CLIP:
                    case PLCLIP: sc = PART_ELADDER; break; // grey
                    case CTF_FLAG: sc = PART_EFLAG; break; // turquoise
                    default: break;
                }
                //particle_splash(sc, i==closest?6:2, i==closest?120:40, v);
                particle_splash(sc, 2, 40, v);
            }
        }
    }
    loopv(ents)
    {
        entity &e = ents[i];
        if(isitem(e.type))
        {
            if((!OUTBORD(e.x, e.y) && e.spawned) || editmode)
            {
                renderent(e);
            }
        }
        else if(editmode)
        {
            if(e.type==CTF_FLAG)
            {
                defformatstring(path)("pickups/flags/%s", team_basestring(e.attr2));
                rendermodel(path, ANIM_FLAG|ANIM_LOOP, 0, 0, vec(e.x, e.y, (float)S(e.x, e.y)->floor), (float)((e.attr1+7)-(e.attr1+7)%15), 0, 120.0f);
            }
            else if((e.type == CLIP || e.type == PLCLIP) && showclips && !stenciling) renderclip(e);
            else if(e.type == MAPMODEL && showclips && showmodelclipping && !stenciling)
            {
                mapmodelinfo *mmi = getmminfo(e.attr2);
                if(mmi && mmi->h)
                {
                    entity ce = e;
                    ce.type = MAPMODEL;
                    ce.attr1 = mmi->zoff+e.attr3;
                    ce.attr2 = ce.attr3 = mmi->rad;
                    ce.attr4 = mmi->h;
                    renderclip(ce);
                }
            }
        }
        if(editmode && i==closest && !stenciling)//closest see above
        {
            switch(e.type)
            {
                case PLAYERSTART:
                {
                    glColor3f(0, 1, 1);
                    vec dir;
                    vecfromyawpitch(e.attr1, 0, -1, 0, dir);
                    renderentarrow(e, dir, 4);
                    glColor3f(1, 1, 1);
                }
                default: break;
            }
        }
    }
    if(m_flags && !editmode) loopi(2)
    {
        flaginfo &f = flaginfos[i];
        switch(f.state)
        {
            case CTFF_STOLEN:
                if(f.actor && f.actor != player1)
                {
                    if(OUTBORD(f.actor->o.x, f.actor->o.y)) break;
                    defformatstring(path)("pickups/flags/small_%s%s", m_ktf ? "" : team_basestring(i), m_htf ? "_htf" : m_ktf ? "ktf" : "");
                    rendermodel(path, ANIM_FLAG|ANIM_START|ANIM_DYNALLOC, 0, 0, vec(f.actor->o).add(vec(0, 0, 0.3f+(sinf(lastmillis/100.0f)+1)/10)), lastmillis/2.5f, 0, 120.0f);
                }
                break;
            case CTFF_INBASE:
                if(!numflagspawn[i]) break;
            case CTFF_DROPPED:
            {
                if(OUTBORD(f.pos.x, f.pos.y)) break;
                entity &e = *f.flagent;
                defformatstring(path)("pickups/flags/%s%s", m_ktf ? "" : team_basestring(i),  m_htf ? "_htf" : m_ktf ? "ktf" : "");
                if(f.flagent->spawned) rendermodel(path, ANIM_FLAG|ANIM_LOOP, 0, 0, vec(f.pos.x, f.pos.y, f.state==CTFF_INBASE ? (float)S(int(f.pos.x), int(f.pos.y))->floor : f.pos.z), (float)((e.attr1+7)-(e.attr1+7)%15), 0, 120.0f);
                break;
            }
            case CTFF_IDLE:
                break;
        }
    }
}

// these two functions are called when the server acknowledges that you really
// picked up the item (in multiplayer someone may grab it before you).

void pickupeffects(int n, playerent *d)
{
    if(!ents.inrange(n)) return;
    entity &e = ents[n];
    e.spawned = false;
    if(!d) return;
    d->pickup(e.type);
    if (m_lss && e.type == I_GRENADE) d->pickup(e.type); // get 2
    itemstat *is = d->itemstats(e.type);
    if(d!=player1 && d->type!=ENT_BOT) return;
    if(is)
    {
        if(d==player1)
        {
            audiomgr.playsoundc(is->sound);

            /*
                onPickup arg1 legend:
                  0 = pistol clips
                  1 = ammo box
                  2 = grenade
                  3 = health pack
                  4 = helmet
                  5 = armour
                  6 = akimbo
            */
            if(identexists("onPickup"))
            {
                string o;
                itemstat *tmp = NULL;
                switch(e.type)
                {
                    case I_CLIPS:   tmp = &ammostats[GUN_PISTOL]; break;
                    case I_AMMO:    tmp = &ammostats[player1->primary]; break;
                    case I_GRENADE: tmp = &ammostats[GUN_GRENADE]; break;
                    case I_AKIMBO:  tmp = &ammostats[GUN_AKIMBO]; break;
                    case I_HEALTH:
                    case I_HELMET:
                    case I_ARMOUR:  tmp = &powerupstats[e.type-I_HEALTH]; break;
                    default: break;
                }
                if(tmp)
                {
                    formatstring(o)("onPickup %d %d", e.type - 3, m_lss && e.type == I_GRENADE ? 2 : tmp->add);
                    execute(o);
                }
            }
        }
        else audiomgr.playsound(is->sound, d);
    }

    weapon *w = NULL;
    switch(e.type)
    {
        case I_AKIMBO: w = d->weapons[GUN_AKIMBO]; break;
        case I_CLIPS: w = d->weapons[GUN_PISTOL]; break;
        case I_AMMO: w = d->primweap; break;
        case I_GRENADE: w = d->weapons[GUN_GRENADE]; break;
    }
    if(w) w->onammopicked();
}

// these functions are called when the client touches the item

extern int lastspawn;

void trypickup(int n, playerent *d)
{
    entity &e = ents[n];
    switch(e.type)
    {
        default:
            if( d->canpickup(e.type) && lastmillis > e.lastmillis + 250 && lastmillis > lastspawn + 500 )
            {
                if(d->type==ENT_PLAYER) addmsg(SV_ITEMPICKUP, "ri", n);
                else if(d->type==ENT_BOT && serverpickup(n, -1)) pickupeffects(n, d);
                e.lastmillis = lastmillis;
            }
            break;

        case LADDER:
            if(!d->crouching) d->onladder = true;
            break;
    }
}

void trypickupflag(int flag, playerent *d)
{
    if(d==player1)
    {
        flaginfo &f = flaginfos[flag];
        flaginfo &of = flaginfos[team_opposite(flag)];
        if(f.state == CTFF_STOLEN) return;
        bool own = flag == team_base(d->team);

        if(m_ctf)
        {
            if(own) // it's the own flag
            {
                if(f.state == CTFF_DROPPED) flagreturn(flag);
                else if(f.state == CTFF_INBASE && of.state == CTFF_STOLEN && of.actor == d && of.ack) flagscore(of.team);
            }
            else flagpickup(flag);
        }
        else if(m_htf)
        {
            if(own)
            {
                flagpickup(flag);
            }
            else
            {
                if(f.state == CTFF_DROPPED) flagscore(f.team); // may not count!
            }
        }
        else if(m_ktf)
        {
            if(f.state != CTFF_INBASE) return;
            flagpickup(flag);
        }
    }
}

void checkitems(playerent *d)
{
    if(editmode || d->state!=CS_ALIVE) return;
    d->onladder = false;
    float eyeheight = d->eyeheight;
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type==NOTUSED) continue;
        if(e.type==LADDER)
        {
            if(OUTBORD(e.x, e.y)) continue;
            vec v(e.x, e.y, d->o.z);
            float dist1 = d->o.dist(v);
            float dist2 = d->o.z - (S(e.x, e.y)->floor+eyeheight);
            if(dist1<1.5f && dist2<e.attr1) trypickup(i, d);
            continue;
        }

        if(!e.spawned) continue;
        if(OUTBORD(e.x, e.y)) continue;

        if(e.type==CTF_FLAG) continue;
        // simple 2d collision
        vec v(e.x, e.y, S(e.x, e.y)->floor+eyeheight);
        if(isitem(e.type)) v.z += e.attr1;
        if(d->o.dist(v)<2.5f) trypickup(i, d);
    }
    if(m_flags) loopi(2)
    {
        flaginfo &f = flaginfos[i];
        entity &e = *f.flagent;
        if(!e.spawned || !f.ack || (f.state == CTFF_INBASE && !numflagspawn[i])) continue;
        if(OUTBORD(f.pos.x, f.pos.y)) continue;
        if(f.state==CTFF_DROPPED) // 3d collision for dropped ctf flags
        {
            if(objcollide(d, f.pos, 2.5f, 8.0f)) trypickupflag(i, d);
        }
        else // simple 2d collision
        {
            vec v = f.pos;
            v.z = S(int(v.x), int(v.y))->floor + eyeheight;
            if(d->o.dist(v)<2.5f) trypickupflag(i, d);
        }
    }
}

void spawnallitems()            // spawns items locally
{
    loopv(ents) if(ents[i].fitsmode(gamemode) || (multiplayer(false) && gamespeed!=100 && (i=-1)))
    {
        ents[i].spawned = true;
        ents[i].lastmillis = lastmillis;
    }
}

void resetspawns(int type)
{
    loopv(ents) if(type < 0 || type == ents[i].type) ents[i].spawned = false;
    if(m_noitemsnade || m_pistol)
    {
        loopv(ents) ents[i].transformtype(gamemode);
    }
}

void setspawn(int i, bool on)
{
    if(ents.inrange(i))
    {
        ents[i].spawned = on;
        if (on) ents[i].lastmillis = lastmillis; // to control trypickup spam
    }
}

bool selectnextprimary(int num)
{
    switch(num)
    {
//         case GUN_CPISTOL:
        case GUN_CARBINE:
        case GUN_SHOTGUN:
        case GUN_SUBGUN:
        case GUN_SNIPER:
        case GUN_ASSAULT:
            player1->setnextprimary(num);
            addmsg(SV_PRIMARYWEAP, "ri", player1->nextprimweap->type);
            return true;

        default:
            conoutf("this is not a valid primary weapon");
            return false;
    }
}

VARFP(nextprimary, 0, GUN_ASSAULT, NUMGUNS,
{
    if(!selectnextprimary(nextprimary)) selectnextprimary((nextprimary = GUN_ASSAULT));
});

// flag ent actions done by the local player

int flagdropmillis = 0;

void flagpickup(int fln)
{
    if(flagdropmillis && flagdropmillis>lastmillis) return;
    flaginfo &f = flaginfos[fln];
    int action = f.state == CTFF_INBASE ? FA_STEAL : FA_PICKUP;
    f.flagent->spawned = false;
    f.state = CTFF_STOLEN;
    f.actor = player1; // do this although we don't know if we picked the flag to avoid getting it after a possible respawn
    f.actor_cn = getclientnum();
    f.ack = false;
    addmsg(SV_FLAGACTION, "rii", action, f.team);
}

void tryflagdrop(bool manual)
{
    loopi(2)
    {
        flaginfo &f = flaginfos[i];
        if(f.state==CTFF_STOLEN && f.actor==player1)
        {
            f.flagent->spawned = false;
            f.state = CTFF_DROPPED;
            f.ack = false;
            flagdropmillis = lastmillis+3000;
            addmsg(SV_FLAGACTION, "rii", manual ? FA_DROP : FA_LOST, f.team);
        }
    }
}

void flagreturn(int fln)
{
    flaginfo &f = flaginfos[fln];
    f.flagent->spawned = false;
    f.ack = false;
    addmsg(SV_FLAGACTION, "rii", FA_RETURN, f.team);
}

void flagscore(int fln)
{
    flaginfo &f = flaginfos[fln];
    f.ack = false;
    addmsg(SV_FLAGACTION, "rii", FA_SCORE, f.team);
}

// flag ent actions from the net

void flagstolen(int flag, int act)
{
    playerent *actor = getclient(act);
    flaginfo &f = flaginfos[flag];
    f.actor = actor; // could be NULL if we just connected
    f.actor_cn = act;
    f.flagent->spawned = false;
    f.ack = true;
}

void flagdropped(int flag, float x, float y, float z)
{
    flaginfo &f = flaginfos[flag];
    if(OUTBORD(x, y)) return; // valid pos
    bounceent p;
    p.plclipped = true;
    p.rotspeed = 0.0f;
    p.o.x = x;
    p.o.y = y;
    p.o.z = z;
    p.vel.z = -0.8f;
    p.aboveeye = p.eyeheight = p.maxeyeheight = 0.4f;
    p.radius = 0.1f;

    bool oldcancollide = false;
    if(f.actor)
    {
        oldcancollide = f.actor->cancollide;
        f.actor->cancollide = false; // avoid collision with owner
    }
    loopi(100) // perform physics steps
    {
        moveplayer(&p, 10, true, 50);
        if(p.stuck) break;
    }
    if(f.actor) f.actor->cancollide = oldcancollide; // restore settings

    f.pos.x = round(p.o.x);
    f.pos.y = round(p.o.y);
    f.pos.z = round(p.o.z);
    if(f.pos.z < hdr.waterlevel) f.pos.z = (short) hdr.waterlevel;
    f.flagent->spawned = true;
    f.ack = true;
}

void flaginbase(int flag)
{
    flaginfo &f = flaginfos[flag];
    f.actor = NULL; f.actor_cn = -1;
    f.pos = vec(f.flagent->x, f.flagent->y, f.flagent->z);
    f.flagent->spawned = true;
    f.ack = true;
}

void flagidle(int flag)
{
    flaginbase(flag);
    flaginfos[flag].flagent->spawned = false;
}

void entstats(void)
{
    int entcnt[MAXENTTYPES] = {0}, clipents = 0, spawncnt[5] = {0};
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type >= MAXENTTYPES) continue;
        entcnt[e.type]++;
        switch(e.type)
        {
            case MAPMODEL:
            {
                mapmodelinfo *mmi = getmminfo(e.attr2);
                if(mmi && mmi->h) clipents++;
                break;
            }
            case PLAYERSTART:
                if(e.attr2 < 2) spawncnt[e.attr2]++;
                if(e.attr2 == 100) spawncnt[2]++;
                break;
            case CTF_FLAG:
                if(e.attr2 < 2) spawncnt[e.attr2 + 3]++;
                break;
        }
    }
    loopi(MAXENTTYPES)
    {
        if(entcnt[i]) switch(i)
        {
            case MAPMODEL:      conoutf(" %d %s, %d clipped", entcnt[i], entnames[i], clipents); break;
            case PLAYERSTART:   conoutf(" %d %s, %d CLA, %d RVSF, %d FFA", entcnt[i], entnames[i], spawncnt[0], spawncnt[1], spawncnt[2]); break;
            case CTF_FLAG:      conoutf(" %d %s, %d CLA, %d RVSF", entcnt[i], entnames[i], spawncnt[3], spawncnt[4]); break;
            default:            conoutf(" %d %s", entcnt[i], entnames[i]); break;
        }
    }
    conoutf("total entities: %d", ents.length());
}

COMMAND(entstats, "");

vector<int> changedents;
int lastentsync = 0;

void syncentchanges(bool force)
{
    if(lastmillis - lastentsync < 1000 && !force) return;
    loopv(changedents) if(ents.inrange(changedents[i]))
    {
        entity &e = ents[changedents[i]];
        addmsg(SV_EDITENT, "ri9", changedents[i], e.type, e.x, e.y, e.z, e.attr1, e.attr2, e.attr3, e.attr4);
    }
    changedents.setsize(0);
    lastentsync = lastmillis;
}
