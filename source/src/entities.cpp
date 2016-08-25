// entities.cpp: map entity related functions (pickup etc.)

#include "cube.h"

VAR(showclips, 0, 1, 1);
VARP(showmodelclipping, 0, 0, 1);
VARP(showladderentities, 0, 0, 1);
VARP(showplayerstarts, 0, 0, 1);
VAR(edithideentmask, 0, 0, INT_MAX);

vector<entity> ents;

const char *entmdlnames[] =
{
    "pickups/nades", "pickups/pistolclips", "pickups/ammobox", "pickups/nade", "pickups/health", "pickups/helmet", "pickups/kevlar", "pickups/akimbo", ""   // doublenades + I_CLIPS..I_AKIMBO
};

void renderent(entity &e)
{
    const char *mdlname = entmdlnames[isitem(e.type) && !(m_lss && e.type == I_GRENADE) ? e.type - I_CLIPS + 1 : 0];  // render double nades in lss
    float z = (float)(1+sinf(lastmillis/100.0f+e.x+e.y)/20), yaw = lastmillis/10.0f;
    rendermodel(mdlname, ANIM_MAPMODEL|ANIM_LOOP|ANIM_DYNALLOC, 0, 0, vec(e.x, e.y, z+S(e.x, e.y)->floor + float(e.attr1) / ENTSCALE10), 0, yaw, 0);
}

void renderclip(int type, int x, int y, float xs, float ys, float h, float elev, float tilt, int shape)
{
    if(xs < 0.05f) xs = 0.05f;
    if(ys < 0.05f) ys = 0.05f;
    if(h < 0.1f) h = 0.1f;
    vec bbmin(x - xs, y - ys, S(x, y)->floor + elev),
        bbmax(x + xs, y + ys, bbmin.z + h);
    vec bb[4];
    loopi(4) bb[i] = bbmin;
    bb[2].x = bb[1].x = bbmax.x;
    bb[2].y = bb[3].y = bbmax.y;
    vec o(x, y, bbmin.z);

    float tx = 0, ty = 0, angle = 0;
    switch(shape & 3)
    {
        case 1: tx = tilt; break; // tilt x
        case 2: ty = tilt; break; // tilt y
        case 3: angle = PI/4; break; // rotate 45°
    }
    loopi(4)
    {
        bb[i].sub(o).rotate_around_z(angle); // rotate
        bb[i].z += bb[i].x * tx + bb[i].y * ty; // tilt
        bb[i].add(o);
    }

    glDisable(GL_TEXTURE_2D);
    switch(type)
    {
        case CLIP:     linestyle(1, 0xFF, 0xFF, 0); break;  // yellow
        case MAPMODEL: linestyle(1, 0, 0xFF, 0);    break;  // green
        case PLCLIP:   linestyle(1, 0xFF, 0, 0xFF); break;  // magenta
        case LADDER:   linestyle(1, 0, 0, 0xFF);    break;  // blue
    }
    glBegin(GL_LINES);
    loopi(16)
    {
        int j = ((i + 1) % 8) / 2;
        glVertex3f(bb[j].x, bb[j].y, bb[j].z + (i > 7 ? h : 0));
    }
    loopi(8) glVertex3f(bb[i / 2].x, bb[i / 2].y, bb[i / 2].z + (i & 1 ? h : 0));
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
            rendermodel(mmi->name, ANIM_MAPMODEL|ANIM_LOOP, e.attr4, 0, vec(e.x, e.y, S(e.x, e.y)->floor + mmi->zoff + float(e.attr3) / ENTSCALE5), e.attr6, float(e.attr1) / ENTSCALE10, float(e.attr5) / ENTSCALE10, 10.0f, 0, NULL, NULL, mmi->scale);
        }
    }
}

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
        p.rotate(PI2 * i / 4.0f, dir);
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
            loopv(ents)
            {
                entity &e = ents[i];
                if(e.type==NOTUSED) continue;
                if(edithideentmask & (1 << (e.type - 1))) continue;
                vec v(e.x, e.y, e.z);
                if(vec(v).sub(camera1->o).dot(camdir) < 0) continue;
                int sc = PART_ECARROT; // use "carrot" for unknown types
                if(i == closest)
                {
                    sc = PART_ECLOSEST; // blue
                }
                else switch(e.type)
                {
                    case LIGHT:       sc = PART_ELIGHT;  break; // white
                    case PLAYERSTART: sc = PART_ESPAWN;  break; // green
                    case I_CLIPS:
                    case I_AMMO:
                    case I_GRENADE:   sc = PART_EAMMO;   break; // red
                    case I_HEALTH:
                    case I_HELMET:
                    case I_ARMOUR:
                    case I_AKIMBO:    sc = PART_EPICKUP; break; // yellow
                    case MAPMODEL:
                    case SOUND:       sc = PART_EMODEL;  break; // magenta
                    case LADDER:
                    case CLIP:
                    case PLCLIP:      sc = PART_ELADDER; break; // grey
                    case CTF_FLAG:    sc = PART_EFLAG;   break; // turquoise
                    default: break;
                }
                particle_splash(sc, i == closest ? 14 : 2, i == closest ? 50 : 40, v);
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
                rendermodel(path, ANIM_FLAG|ANIM_LOOP, 0, 0, vec(e.x, e.y, (float)S(e.x, e.y)->floor), 0, float(e.attr1) / ENTSCALE10, 0, 120.0f);
            }
            else if((e.type == CLIP || e.type == PLCLIP) && showclips && !stenciling)
            {
                renderclip(e.type, e.x, e.y, float(e.attr2) / ENTSCALE5, float(e.attr3) / ENTSCALE5, float(e.attr4) / ENTSCALE5, float(e.attr1) / ENTSCALE10, float(e.attr6) / (4 * ENTSCALE10), e.attr7);
            }
            else if(e.type == MAPMODEL && showclips && showmodelclipping && !stenciling)
            {
                mapmodelinfo *mmi = getmminfo(e.attr2);
                if(mmi && mmi->h)
                {
                    renderclip(e.type, e.x, e.y, mmi->rad, mmi->rad, mmi->h, mmi->zoff + float(e.attr3) / ENTSCALE5, 0, 0);
                }
            }
            else if(e.type == LADDER && showladderentities && !stenciling)
            {
                renderclip(e.type, e.x, e.y, 0, 0, e.attr1, 0, 0, 3);
            }
            else if(e.type == PLAYERSTART && showplayerstarts)
            {
                vec o(e.x, e.y, 0);
                if(!OUTBORD(e.x, e.y)) o.z += S(e.x, e.y)->floor;
                const char *skin;
                switch(e.attr2)
                {
                    case 0: skin = "packages/models/playermodels/CLA/red.jpg"; break;
                    case 1: skin = "packages/models/playermodels/RVSF/blue.jpg"; break;
                    case 100: skin = "packages/models/playermodels/ffaspawn.jpg"; break;
                    default: skin = "packages/models/playermodels/unknownspawn.jpg"; break;
                }
                rendermodel("playermodels", ANIM_IDLE, -(int)textureload(skin)->id, 1.5f, o, 0, e.attr1 / ENTSCALE10 + 90, 0);
            }
        }
        if(editmode && i == closest && !stenciling)
        {
            switch(e.type)
            {
                case PLAYERSTART:
                {
                    glColor3f(0, 1, 1);
                    vec dir(0, -1, 0);
                    dir.rotate_around_z(float(e.attr1) / ENTSCALE10 * RAD);
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
                    rendermodel(path, ANIM_FLAG|ANIM_START|ANIM_DYNALLOC, 0, 0, vec(f.actor->o).add(vec(0, 0, 0.3f+(sinf(lastmillis/100.0f)+1)/10)), 0, lastmillis/2.5f, 0, 120.0f);
                }
                break;
            case CTFF_INBASE:
                if(!clentstats.flags[i]) break;
            case CTFF_DROPPED:
            {
                if(OUTBORD(f.pos.x, f.pos.y)) break;
                entity &e = *f.flagent;
                defformatstring(path)("pickups/flags/%s%s", m_ktf ? "" : team_basestring(i),  m_htf ? "_htf" : m_ktf ? "ktf" : "");
                if(f.flagent->spawned) rendermodel(path, ANIM_FLAG|ANIM_LOOP, 0, 0, vec(f.pos.x, f.pos.y, f.state==CTFF_INBASE ? (float)S(int(f.pos.x), int(f.pos.y))->floor : f.pos.z), 0, float(e.attr1) / ENTSCALE10, 0, 120.0f);
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
                if(tmp) exechook(HOOK_SP, "onPickup", "%d %d", e.type - 3, m_lss && e.type == I_GRENADE ? 2 : tmp->add);
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
        if(isitem(e.type)) v.z += float(e.attr1) / ENTSCALE10;
        if(d->o.dist(v)<2.5f) trypickup(i, d);
    }
    if(m_flags) loopi(2)
    {
        flaginfo &f = flaginfos[i];
        entity &e = *f.flagent;
        if(!e.spawned || !f.ack || (f.state == CTFF_INBASE && !clentstats.flags[i])) continue;
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

void resetpickups(int type)
{
    loopv(ents) if(type < 0 || type == ents[i].type) ents[i].spawned = false;
    if(m_noitemsnade || m_pistol)
    {
        loopv(ents) ents[i].transformtype(gamemode);
    }
}

void setpickupspawn(int i, bool on)
{
    if(ents.inrange(i))
    {
        ents[i].spawned = on;
        if (on) ents[i].lastmillis = lastmillis; // to control trypickup spam
    }
}

SVARFP(nextprimary, guns[GUN_ASSAULT].modelname,
{
    int n = getlistindex(nextprimary, gunnames, true, -1);
    switch(n)
    {
        default:
            conoutf("\"%s\" is not a valid primary weapon", nextprimary);
            n = GUN_ASSAULT;
        case GUN_CARBINE:
        case GUN_SHOTGUN:
        case GUN_SUBGUN:
        case GUN_SNIPER:
        case GUN_ASSAULT:
            player1->setnextprimary(n);
            addmsg(SV_PRIMARYWEAP, "ri", player1->nextprimweap->type);
            nextprimary = exchangestr(nextprimary, gunnames[player1->nextprimweap->type]);
            break;
    }
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
            f.pos.x = floor(player1->o.x + 0.5f);
            f.pos.y = floor(player1->o.y + 0.5f);
            f.pos.z = floor(player1->o.z + 0.5f);
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
    p.aboveeye = 1.0f;
    p.eyeheight = p.maxeyeheight = 0.1f;
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
    if(p.o.z < waterlevel)
    {
        p.aboveeye = 6.7f;
        loopirev((waterlevel - p.o.z) / 0.2f + 1)
        {
            p.o.z += 0.2f;
            if(p.o.z > z) p.radius = 1.0f; // don't float through small holes in the ceiling
            if(collide(&p)) break;
        }
        if(p.o.z > waterlevel) p.o.z = waterlevel;
    }
    if(f.actor) f.actor->cancollide = oldcancollide; // restore settings

    f.pos.x = floor(p.o.x + 0.5f);
    f.pos.y = floor(p.o.y + 0.5f);
    f.pos.z = floor(p.o.z * 5 + 0.5f) / 5;
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

void entstats_(void)
{
    entitystats_s es;
    calcentitystats(es, NULL, 0);
    int clipents = 0, xmodels = 0, xsounds = 0;
    loopv(ents)
    {
        entity &e = ents[i];
        if(e.type == MAPMODEL)
        {
            mapmodelinfo *mmi = getmminfo(e.attr2);
            if(!mmi) xmodels++;
            if(mmi && mmi->h) clipents++;
        }
        else if(e.type == SOUND)
        {
            if(!mapconfigdata.mapsoundlines.inrange(e.attr1)) xsounds++;
        }
    }
    string txt = "", clips = "";
    loopi(MAXENTTYPES) if(es.entcnt[i]) switch(i)
    {
        case MAPMODEL:      conoutf(" %d %s, %d clipped, %s%d unconfigured", es.entcnt[i], entnames[i], clipents, xmodels ? "\f3" : "", xmodels); break;
        case SOUND:         conoutf(" %d %s, %s%d unconfigured", es.entcnt[i], entnames[i], xsounds ? "\f3" : "", xsounds); break;
        case PLAYERSTART:   conoutf(" %d %s, %d CLA, %d RVSF, %d FFA%c \f3unknown %d", es.entcnt[i], entnames[i], es.spawns[0], es.spawns[1], es.spawns[2], es.unknownspawns ? ',' : '\0', es.unknownspawns); break;
        case CTF_FLAG:      conoutf(" %d %s, %d CLA, %d RVSF%c \f3unknown %d", es.entcnt[i], entnames[i], es.flags[0], es.flags[1], es.unknownflags ? ',' : '\0', es.unknownflags); break;
        case CLIP:
        case PLCLIP:        concatformatstring(clips, ", %d %s", es.entcnt[i], entnames[i]); break;
        case NOTUSED:       conoutf(" %d deleted", es.entcnt[i]); break;
        default:            if(isitem(i)) concatformatstring(txt, ", %d %s", es.entcnt[i], entnames[i]);
                            else conoutf(" %d %s", es.entcnt[i], entnames[i]);
                            break;
    }
    if(*clips) conoutf(" %s", clips + 2);
    if(es.pickups)
    {
        conoutf(" %d pickups:%s", es.pickups, txt + 1);
        *txt = '\0';
        loopi(LARGEST_FACTOR + 1) concatformatstring(txt, " %d", es.pickupdistance[i]);
        conoutf(" pickupdistance:%s", txt);
    }
    if(es.entcnt[CTF_FLAG]) conoutf(" flag distance: %d", es.flagentdistance);
    conoutf(" map capabilities: has ffa spawns %d, has team spawns %d, has flags %d", es.hasffaspawns ? 1 : 0, es.hasteamspawns ? 1 : 0, es.hasflags ? 1 : 0);
    if(es.modes_possible & GMMASK__MPNOCOOP) conoutf(" possible multiplayer modes: %s", gmode_enum(es.modes_possible & GMMASK__MPNOCOOP, txt));
    conoutf("total entities: %d", ents.length());
    intret(xmodels + xsounds);
}

COMMANDN(entstats, entstats_, "");

vector<int> changedents;
int lastentsync = 0;

void syncentchanges(bool force)
{
    if(lastmillis - lastentsync < 1000 && !force) return;
    loopv(changedents) if(ents.inrange(changedents[i]))
    {
        entity &e = ents[changedents[i]];
        addmsg(SV_EDITENT, "ri9i3", changedents[i], e.type, e.x, e.y, e.z, e.attr1, e.attr2, e.attr3, e.attr4, e.attr5, e.attr6, e.attr7);
    }
    changedents.setsize(0);
    lastentsync = lastmillis;
}

void clampentityattributes(persistent_entity &e)
{
    if(e.type < MAXENTTYPES)
    {
        int c;
        #define CLAMPATTR(x) \
            c = entwraparound[e.type][x - 1]; \
            if(c > 0) e.attr##x = (e.attr##x % c + c) % c;  /* fold value into range 0..c */ \
            else if(c < 0) e.attr##x = e.attr##x % (-c)     /* fold value into range -c..c */
        CLAMPATTR(1);
        CLAMPATTR(2);
        CLAMPATTR(3);
        CLAMPATTR(4);
        CLAMPATTR(5);
        CLAMPATTR(6);
        CLAMPATTR(7);
    }
}

const char *formatentityattributes(const persistent_entity &e, bool withcomma)
{
    static string res;
    int t = e.type < MAXENTTYPES ? e.type : 0;
    const char *c = withcomma ? "," : "";
    #define AA(x) floatstr(float(e.attr##x) / entscale[t][x - 1], true)
    formatstring(res)("%s%s %s%s %s%s %s%s %s%s %s%s %s", AA(1), c, AA(2), c, AA(3), c, AA(4), c, AA(5), c, AA(6), c, AA(7));
    #undef AA
    return res;
}
