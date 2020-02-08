// weapon.cpp: all shooting and effects code

#include "cube.h"
#include "bot/bot.h"
#include "hudgun.h"

VARP(autoreload, 0, 1, 1);
VARP(akimboautoswitch, 0, 1, 1);
VARP(akimboendaction, 0, 3, 3); // 0: switch to knife, 1: stay with pistol (if has ammo), 2: switch to grenade (if possible), 3: switch to primary (if has ammo) - all fallback to previous one w/o ammo for target

sgray sgr[SGRAYS*3];
sgray pat[SGRAYS*3]; // DEBUG 2011may27

int burstshotssettings[NUMGUNS] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, };

void updatelastaction(playerent *d, int millis = lastmillis)
{
    loopi(NUMGUNS) d->weapons[i]->updatetimers(millis);
    d->lastaction = millis;
}

void checkweaponswitch()
{
    if(!player1->weaponchanging) return;
    int timeprogress = lastmillis-player1->weaponchanging;
    if(timeprogress>weapon::weaponchangetime)
    {
        addmsg(SV_WEAPCHANGE, "ri", player1->weaponsel->type);
        player1->weaponchanging = 0;
    }
    else if(timeprogress>(weapon::weaponchangetime>>1) && player1->weaponsel != player1->nextweaponsel)
    {
        player1->prevweaponsel = player1->weaponsel;
        player1->weaponsel = player1->nextweaponsel;
    }
}

void selectweapon(weapon *w)
{
    if(!w || !player1->weaponsel->deselectable()) return;
    if(w->selectable())
    {
        int i = w->type;
        // substitute akimbo
        weapon *akimbo = player1->weapons[GUN_AKIMBO];
        if(w->type==GUN_PISTOL && akimbo->selectable()) w = akimbo;

        player1->weaponswitch(w);
        if(identexists("onWeaponSwitch"))
        {
            string o;
            formatstring(o)("onWeaponSwitch %d", i);
            execute(o);
        }
    }
}

void requestweapon(int *w)
{
    if(keypressed && player1->state == CS_ALIVE && *w >= 0 && *w < NUMGUNS )
    {
        if (player1->akimbo && *w==GUN_PISTOL) *w = GUN_AKIMBO;
        selectweapon(player1->weapons[*w]);
    }
}

void shiftweapon(int *s)
{
    if(keypressed && player1->state == CS_ALIVE)
    {
        if(!player1->weaponsel->deselectable()) return;

        weapon *curweapon = player1->weaponsel;
        weapon *akimbo = player1->weapons[GUN_AKIMBO];

        // collect available weapons
        vector<weapon *> availweapons;
        loopi(NUMGUNS)
        {
            weapon *w = player1->weapons[i];
            if(!w) continue;
            if(w->selectable() || w==curweapon || (w->type==GUN_PISTOL && player1->akimbo))
            {
                availweapons.add(w);
            }
        }

        // replace pistol by akimbo
        if(player1->akimbo)
        {
            availweapons.removeobj(akimbo); // and remove initial akimbo
            int pistolidx = availweapons.find(player1->weapons[GUN_PISTOL]);
            if(pistolidx>=0) availweapons[pistolidx] = akimbo; // insert at pistols position
            if(curweapon->type==GUN_PISTOL) curweapon = akimbo; // fix selection
        }

        // detect the next weapon
        int num = availweapons.length();
        int curidx = availweapons.find(curweapon);
        if(!num || curidx<0) return;
        int idx = (curidx + *s) % num;
        if(idx<0) idx += num;
        weapon *next = availweapons[idx];
        if(next->type!=player1->weaponsel->type) // different weapon
        {
            selectweapon(next);
        }
    }
    else if(player1->isspectating()) updatefollowplayer(*s);
}

bool quicknade = false, nadeattack = false;
VARP(quicknade_hold, 0, 0, 1);

void quicknadethrow(bool on)
{
    if(player1->state != CS_ALIVE) return;
    if(on)
    {
        if(player1->weapons[GUN_GRENADE]->mag > 0)
        {
            if(player1->weaponsel->type != GUN_GRENADE) selectweapon(player1->weapons[GUN_GRENADE]);
            if(player1->weaponsel->type == GUN_GRENADE || quicknade_hold) { player1->attacking = true; nadeattack = true; }
        }
    }
    else if (nadeattack)
    {
        nadeattack = player1->attacking = false;
        if(player1->weaponsel->type == GUN_GRENADE) quicknade = true;
    }
}

void currentprimary() { intret(player1->primweap->type); }
void prevweapon() { intret(player1->prevweaponsel->type); }
void curweapon() { intret(player1->weaponsel->type); }

void magcontent(int *w) { if(*w >= 0 && *w < NUMGUNS) intret(player1->weapons[*w]->mag); else intret(-1); }
void magreserve(int *w) { if(*w >= 0 && *w < NUMGUNS) intret(player1->weapons[*w]->ammo); else intret(-1); }

COMMANDN(weapon, requestweapon, "i");
COMMAND(shiftweapon, "i");
COMMAND(quicknadethrow, "d");
COMMAND(currentprimary, "");
COMMAND(prevweapon, "");
COMMAND(curweapon, "");
COMMAND(magcontent, "i");
COMMAND(magreserve, "i");

void tryreload(playerent *p)
{
    if(!p || p->state!=CS_ALIVE || p->weaponsel->reloading || p->weaponchanging) return;
    p->weaponsel->reload(false);
}

void selfreload() { tryreload(player1); }
COMMANDN(reload, selfreload, "");

int lastsgs_hits = 0;
int lastsgs_dmgt = 0;

void createrays(vec &from, vec &to) // create random spread of rays for the shotgun
{
    // 2011jan17:ft: once this basic approach has been agreed upon this function should be optimized
    // 2011may27:ft: indeed
    vec dir = vec(from).sub(to);
    float f = dir.magnitude() / 10.0f;
    dir.normalize();
    vec spoke;
    spoke.orthogonal(dir);
    spoke.normalize();
    spoke.mul(f);
    loopk(3)
    {
        float rnddir = rndscale(2*M_PI);
        loopi(SGRAYS)
        {
            int j = k * SGRAYS;
            sgr[j+i].ds = k; //2011jun18 - damage-independent-section-indicator .. not: k==0 ? SGSEGDMG_O : (k==1? SGSEGDMG_M: SGSEGDMG_C);
            vec p(spoke);
            float base = 0.0f;
            int wrange = 50;
            switch(sgr[j+i].ds)
            {
                case 0:/* SGSEGDMG_O:*/  base = SGCObase/100.0f; wrange = SGCOrange; break;
                case 1:/* SGSEGDMG_M:*/  base = SGCMbase/100.0f; wrange = SGCMrange; break;
                case 2:/* SGSEGDMG_C:*/
                default:          base = SGCCbase/100.0f; wrange = SGCCrange; break;
            }
            int rndmul = rnd(wrange);
            float veclen = base + rndmul/100.0f;
            p.mul( veclen );
        
            p.rotate( 2*M_PI/SGRAYS * i + rnddir , dir );
            vec rray = vec(to);
            float nvl = veclen / ( ( SGCObase / 100.0f ) + ( SGCOrange / 100.0f ) );
            vec pbv = vec( nvl, 0, 0);
            pbv.rotate( 2*M_PI/SGRAYS * i + rnddir, vec( 0, 0, 1) );
            pat[j+i].ds = int(veclen*100);
            pat[j+i].rv = pbv;
            rray.add(p);
            sgr[j+i].rv = rray;
        }
    }
}

static inline bool intersectbox(const vec &o, const vec &rad, const vec &from, const vec &to, vec *end) // if lineseg hits entity bounding box
{
    const vec *p;
    vec v = to, w = o;
    v.sub(from);
    w.sub(from);
    float c1 = w.dot(v);

    if(c1<=0) p = &from;
    else
    {
        float c2 = v.squaredlen();
        if(c2<=c1) p = &to;
        else
        {
            float f = c1/c2;
            v.mul(f).add(from);
            p = &v;
        }
    }

    if(p->x <= o.x+rad.x
       && p->x >= o.x-rad.x
       && p->y <= o.y+rad.y
       && p->y >= o.y-rad.y
       && p->z <= o.z+rad.z
       && p->z >= o.z-rad.z)
    {
        if(end) *end = *p;
        return true;
    }
    return false;
}

static inline bool intersectsphere(const vec &from, const vec &to, vec center, float radius, float &dist)
{
    vec ray(to);
    ray.sub(from);
    center.sub(from);
    float v = center.dot(ray),
          inside = radius*radius - center.squaredlen();
    if(inside < 0 && v < 0) return false;
    float raysq = ray.squaredlen(), d = inside*raysq + v*v;
    if(d < 0) return false;
    dist = (v - sqrtf(d)) / raysq;
    return dist >= 0 && dist <= 1;
}

static inline bool intersectcylinder(const vec &from, const vec &to, const vec &start, const vec &end, float radius, float &dist)
{
    vec d(end), m(from), n(to);
    d.sub(start);
    m.sub(start);
    n.sub(from);
    float md = m.dot(d),
          nd = n.dot(d),
          dd = d.squaredlen();
    if(md < 0 && md + nd < 0) return false;
    if(md > dd && md + nd > dd) return false;
    float nn = n.squaredlen(),
          mn = m.dot(n),
          a = dd*nn - nd*nd,
          k = m.squaredlen() - radius*radius,
          c = dd*k - md*md;
    if(fabs(a) < 0.005f)
    {
        if(c > 0) return false;
        if(md < 0) dist = -mn / nn;
        else if(md > dd) dist = (nd - mn) / nn;
        else dist = 0;
        return true;
    }
    else if(c > 0)
    {
        float b = dd*mn - nd*md,
              discrim = b*b - a*c;
        if(discrim < 0) return false;
        dist = (-b - sqrtf(discrim)) / a;
    }
    else dist = 0;
    float offset = md + dist*nd;
    if(offset < 0)
    {
        if(nd <= 0) return false;
        dist = -md / nd;
        if(k + dist*(2*mn + dist*nn) > 0) return false;
    }
    else if(offset > dd)
    {
        if(nd >= 0) return false;
        dist = (dd - md) / nd;
        if(k + dd - 2*md + dist*(2*(mn-nd) + dist*nn) > 0) return false;
    }
    return dist >= 0 && dist <= 1;
}


int intersect(playerent *d, const vec &from, const vec &to, vec *end)
{
    float dist;
    if(d->head.x >= 0)
    {
        if(intersectsphere(from, to, d->head, HEADSIZE, dist))
        {
            if(end) (*end = to).sub(from).mul(dist).add(from);
            return 2;
        }
    }
    float y = d->yaw*RAD, p = (d->pitch/4+90)*RAD, c = cosf(p);
    vec bottom(d->o), top(sinf(y)*c, -cosf(y)*c, sinf(p))/*, mid(top)*/;
    bottom.z -= d->eyeheight;
    float h = d->eyeheight /*+ d->aboveeye*/; // this mod makes the shots pass over the shoulders
//     mid.mul(h*0.5).add(bottom);            // this mod divides the hitbox in 2
    top.mul(h).add(bottom);
    if( intersectcylinder(from, to, bottom, top, d->radius, dist) ) // FIXME if using 2 hitboxes
    {
        if(end) (*end = to).sub(from).mul(dist).add(from);
        return 1;
    }
    return 0;

#if 0
    const float eyeheight = d->eyeheight;
    vec o(d->o);
    o.z += (d->aboveeye - eyeheight)/2;
    return intersectbox(o, vec(d->radius, d->radius, (d->aboveeye + eyeheight)/2), from, to, end) ? 1 : 0;
#endif
}

bool intersect(entity *e, const vec &from, const vec &to, vec *end)
{
    mapmodelinfo *mmi = getmminfo(e->attr2);
    if(!mmi || !mmi->h) return false;

    float lo = float(S(e->x, e->y)->floor+mmi->zoff+e->attr3);
    return intersectbox(vec(e->x, e->y, lo+mmi->h/2.0f), vec(mmi->rad, mmi->rad, mmi->h/2.0f), from, to, end);
}

playerent *intersectclosest(const vec &from, const vec &to, playerent *at, float &bestdist, int &hitzone, bool aiming = true)
{
    playerent *best = NULL;
    bestdist = 1e16f;
    int zone;
    if(at!=player1 && player1->state==CS_ALIVE && (zone = intersect(player1, from, to)))
    {
        best = player1;
        bestdist = at->o.dist(player1->o);
        hitzone = zone;
    }
    loopv(players)
    {
        playerent *o = players[i];
        if(!o || o==at || (o->state!=CS_ALIVE && (aiming || (o->state!=CS_EDITING && o->state!=CS_LAGGED)))) continue;
        float dist = at->o.dist(o->o);
        if(dist < bestdist && (zone = intersect(o, from, to)))
        {
            best = o;
            bestdist = dist;
            hitzone = zone;
        }
    }
    return best;
}

playerent *playerincrosshair()
{
    if(camera1->type == ENT_PLAYER || (camera1->type == ENT_CAMERA && player1->spectatemode == SM_DEATHCAM))
    {
        float dist;
        int hitzone;
        return intersectclosest(camera1->o, worldpos, (playerent *)camera1, dist, hitzone, false);
    }
    else return NULL;
}

void damageeffect(int damage, playerent *d)
{
    particle_splash(PART_BLOOD, damage/10, 1000, d->o);
}

struct hitweap
{
    float hits;
    int shots;
    hitweap() {hits=shots=0;}
};
hitweap accuracym[NUMGUNS];

inline void attackevent(playerent *owner, int weapon)
{
    if(owner == player1 && identexists("onAttack"))
    {
        defformatstring(onattackevent)("onAttack %d", weapon);
        execute(onattackevent);
    }
}

vector<hitmsg> hits;

void hit(int damage, playerent *d, playerent *at, const vec &vel, int gun, bool gib, int info)
{
    if(d==player1 || d->type==ENT_BOT || !m_mp(gamemode)) d->hitpush(damage, vel, at, gun);

    if(at == player1 && d != player1)
    {
        extern int hitsound;
        extern int lasthit;
        if(hitsound == 2 && lasthit != lastmillis)
        {
            defformatstring(hitsnd)("sound %d %d;", S_HITSOUND, SP_HIGHEST);
            addsleep(60, hitsnd);
        }
        lasthit = lastmillis;
    }

    if(!m_mp(gamemode)) dodamage(damage, d, at, gun, gib);
    else
    {
        hitmsg &h = hits.add();
        h.target = d->clientnum;
        h.lifesequence = d->lifesequence;
        h.info = info;
        if(d==player1)
        {
            h.dir = ivec(0, 0, 0);
            d->damageroll(damage);
            updatedmgindicator(at->o);
            damageblend(damage);
            damageeffect(damage, d);
            audiomgr.playsound(S_PAIN6, SP_HIGH);
        }
        else
        {
            h.dir = ivec(int(vel.x*DNF), int(vel.y*DNF), int(vel.z*DNF));
//             damageeffect(damage, d);
//             audiomgr.playsound(S_PAIN1+rnd(5), d);
        }
    }
}

void hitpush(int damage, playerent *d, playerent *at, vec &from, vec &to, int gun, bool gib, int info)
{
    vec v(to);
    v.sub(from);
    v.normalize();
    hit(damage, d, at, v, gun, gib, info);
}

float expdist(playerent *o, vec &dir, const vec &v)
{
    vec middle = o->o;
    middle.z += (o->aboveeye-o->eyeheight)/2;
    float dist = middle.dist(v, dir);
    dir.div(dist);
    if(dist<0) dist = 0;
    return dist;
}

void radialeffect(playerent *o, vec &v, int qdam, playerent *at, int gun)
{
    if(o->state!=CS_ALIVE) return;
    vec dir;
    float dist = expdist(o, dir, v);
    if(dist<EXPDAMRAD)
    {
        if(at == player1) accuracym[gun].hits += 1.0f-(float)dist/EXPDAMRAD;
        int damage = (int)(qdam*(1-dist/EXPDAMRAD));
        hit(damage, o, at, dir, gun, true, int(dist*DMF));
    }
}

vector<bounceent *> bounceents;

void removebounceents(playerent *owner)
{
    loopv(bounceents) if(bounceents[i]->owner==owner) { delete bounceents[i]; bounceents.remove(i--); }
}

void movebounceents()
{
    loopv(bounceents) if(bounceents[i])
    {
        bounceent *p = bounceents[i];
        if((p->bouncetype==BT_NADE || p->bouncetype==BT_GIB) && p->applyphysics()) movebounceent(p, 1, false);
        if(!p->isalive(lastmillis))
        {
            p->destroy();
            delete p;
            bounceents.remove(i--);
        }
    }
}

void clearbounceents()
{
    if(gamespeed==100);
    else if(multiplayer(false)) bounceents.add((bounceent *)player1);
    loopv(bounceents) if(bounceents[i]) { delete bounceents[i]; bounceents.remove(i--); }
}

void renderbounceents()
{
    loopv(bounceents)
    {
        bounceent *p = bounceents[i];
        if(!p) continue;
        string model;
        vec o(p->o);

        int anim = ANIM_MAPMODEL, basetime = 0;
        switch(p->bouncetype)
        {
            case BT_NADE:
            if (identexists("modmdlbounce3"))
                copystring(model, getalias("modmdlbounce3"));
                else
                copystring(model, "weapons/grenade/static");
                break;
            case BT_GIB:
            default:
            {
                uint n = (((4*(uint)(size_t)p)+(uint)p->timetolive)%3)+1;

                defformatstring(widn)("modmdlbounce%d", n-1);

                if (identexists(widn))
                copystring(model, getalias(widn));
                else
                formatstring(model)("misc/gib0%u", n);

                int t = lastmillis-p->millis;
                if(t>p->timetolive-2000)
                {
                    anim = ANIM_DECAY;
                    basetime = p->millis+p->timetolive-2000;
                    t -= p->timetolive-2000;
                    o.z -= t*t/4000000000.0f*t;
                }
                break;
            }
        }
        path(model);
        rendermodel(model, anim|ANIM_LOOP|ANIM_DYNALLOC, 0, 1.1f, o, p->yaw+90, p->pitch, 0, basetime);
    }
}

VARP(gib, 0, 1, 1);
VARP(gibnum, 0, 6, 1000);
VARP(gibttl, 0, 7000, 60000);
VARP(gibspeed, 1, 30, 100);

void addgib(playerent *d)
{
    if(!d || !gib || !gibttl) return;
    audiomgr.playsound(S_GIB, d);

    loopi(gibnum)
    {
        bounceent *p = bounceents.add(new bounceent);
        p->owner = d;
        p->millis = lastmillis;
        p->timetolive = gibttl+rnd(10)*100;
        p->bouncetype = BT_GIB;

        p->o = d->o;
        p->o.z -= d->aboveeye;
        p->inwater = hdr.waterlevel>p->o.z;

        p->yaw = (float)rnd(360);
        p->pitch = (float)rnd(360);

        p->maxspeed = 30.0f;
        p->rotspeed = 3.0f;

        const float angle = (float)rnd(360);
        const float speed = (float)gibspeed;

        p->vel.x = sinf(RAD*angle)*rnd(1000)/1000.0f;
        p->vel.y = cosf(RAD*angle)*rnd(1000)/1000.0f;
        p->vel.z = rnd(1000)/1000.0f;
        p->vel.mul(speed/100.0f);

        p->resetinterp();
    }
}

void shorten(vec &from, vec &target, float dist)
{
    target.sub(from).mul(min(1.0f, dist)).add(from);
}

void raydamage(vec &from, vec &to, playerent *d)
{
    int dam = d->weaponsel->info.damage;
    int hitzone = -1;
    playerent *o = NULL;
    float dist;
    bool hitted=false;
    int rayscount = 0, hitscount = 0;
    if(d->weaponsel->type==GUN_SHOTGUN)
    {
        playerent *hits[3*SGRAYS];
        loopk(3)
        loopi(SGRAYS)
        {
            rayscount++;
            int h = k*SGRAYS + i;
            if((hits[h] = intersectclosest(from, sgr[h].rv, d, dist, hitzone)))
                shorten(from, sgr[h].rv, dist);
        }
        loopk(3)
        loopi(SGRAYS)
        {
            int h = k*SGRAYS + i;
            if(hits[h])
            {
                o = hits[h];
                hits[h] = NULL;
                int numhits_o, numhits_m, numhits_c;
                numhits_o = numhits_m = numhits_c = 0;
                switch(sgr[h].ds)
                {
                    case 0:/* SGSEGDMG_O:*/ numhits_o++; break;
                    case 1:/* SGSEGDMG_M:*/ numhits_m++; break;
                    case 2:/* SGSEGDMG_C:*/ numhits_c++; break;
                    default: break;
                }
                for(int j = i+1; j < 3*SGRAYS; j++) if(hits[j] == o)
                {
                    hits[j] = NULL;
                    switch(sgr[j].ds)
                    {
                        case 0:/* SGSEGDMG_O:*/ numhits_o++; break;
                        case 1:/* SGSEGDMG_M:*/ numhits_m++; break;
                        case 2:/* SGSEGDMG_C:*/ numhits_c++; break;
                        default: break;
                    }
                }
                int numhits = numhits_o + numhits_m + numhits_c;
                int dmgreal = 0;
                float dmg4r = 0.0f;
                bool withBONUS = false;//'';
                if(SGDMGBONUS)
                {
                    float d2o = SGDMGDISTB;
                    if(o) d2o = vec(from).sub(o->o).magnitude();
                    if(d2o <= (SGDMGDISTB/10.0f) && numhits)
                    {
                        /*
                         float soc = ( SGCCdmg/10.0f * SGDMGTOTAL/100.0f ) + ( SGCMdmg/10.0f * SGDMGTOTAL/100.0f ) + ( SGCOdmg/10.0f * SGDMGTOTAL/100.0f );
                         float d2t = SGDMGTOTAL - soc;
                         */
                        dmg4r += SGDMGBONUS;//d2t; //was: "d2t" == if percentage-points are dangling and we have hits, we have a base-damage value
                        withBONUS = true;
                        //printf("established SOC: %.2f | d2t: %.2f\n", soc, d2t);
                        //if(numhits) dmgreal += int(d2t);
                    }
                }
                dmg4r += /*(int)*/( ( SGCOdmg/10.0f * SGDMGTOTAL/100.0f ) * numhits_o/21.0f );
                dmg4r += /*(int)*/( ( SGCMdmg/10.0f * SGDMGTOTAL/100.0f ) * numhits_m/21.0f );
                dmg4r += /*(int)*/( ( SGCCdmg/10.0f * SGDMGTOTAL/100.0f ) * numhits_c/21.0f );
                //              int dmgreal = (SGSEGDMG_O * numhits_o + SGSEGDMG_M * numhits_m + SGSEGDMG_C * numhits_c)/3;
                /*
                 float d2o = SGBONUSDIST;
                 if(o) d2o = vec(from).sub(o->o).magnitude();
                 if(d2o <= (SGBONUSDIST/10.0f) && numhits)
                 {
                 dmgreal += (SGMAXDMGABS-SGMAXDMGLOC);
                 dmgreal = min(dmgreal, SGMAXDMGABS);
                 }
                 */
                dmgreal = (int) ceil(dmg4r);
                lastsgs_hits = numhits;
                lastsgs_dmgt = dmgreal;
//                conoutf("%d [%.3f%s] DMG with %d hits", lastsgs_dmgt, dmg4r, withBONUS ? "\fs\f3+\fr" : "", lastsgs_hits);
                int info = (withBONUS ? SGDMGBONUS : 0) | (numhits_c << 8) | (numhits_m << 16) | (numhits_o << 24);
                if(numhits) hitpush( dmgreal, o, d, from, to, d->weaponsel->type, lastsgs_hits == SGRAYS*3, isbigendian() ? endianswap(info) : info);
//              if(numhits) hitpush( dmgreal, o, d, from, to, d->weaponsel->type, dmgreal == SGMAXDMGABS, dmgreal);

                if(d==player1) hitted = true;
                hitscount+=numhits;
            }
        }
    }
    else if((o = intersectclosest(from, to, d, dist, hitzone)))
    {
        bool gib = false;
        switch(d->weaponsel->type)
        {
            case GUN_KNIFE: gib = true; break;
            case GUN_SNIPER: if(d==player1 && hitzone==2) { dam *= 3; gib = true; }; break;
            default: break;
        }
        bool info = gib;
        hitpush(dam, o, d, from, to, d->weaponsel->type, gib, info ? 1 : 0);
        if(d==player1) hitted=true;
        shorten(from, to, dist);
        hitscount++;
    }

    if(d==player1)
    {
        if(!rayscount) rayscount = 1;
        if(hitted) accuracym[d->weaponsel->type].hits+=(float)hitscount/rayscount;
        accuracym[d->weaponsel->type].shots++;
    }
}

const char *weapstr(unsigned int i)
{
    switch (i)
    {
    case GUN_AKIMBO:
        return "Akimbo";
    case GUN_PISTOL:
        return "Pistol";
    case GUN_ASSAULT:
        return "MTP-57";
    case GUN_SUBGUN:
        return "A-ARD/10";
    case GUN_SNIPER:
        return "AD-81 SR";
    case GUN_SHOTGUN:
        return "V-19 SG";
    case GUN_KNIFE:
        return "Knife";
    case GUN_CARBINE:
        return "TMP-M&A CB";
    case GUN_GRENADE:
        return "Grenades";
    }
    return "x";
}

VARP(accuracy,0,0,1);

void r_accuracy(int h)
{
    if(!accuracy) return;
    vector <char*>lines;
    int rows = 0, cols = 0;
    float spacing = curfont->defaultw*2, x_offset = curfont->defaultw, y_offset = float(2*h) - 2*spacing;

    loopi(NUMGUNS) if(i != GUN_CPISTOL && accuracym[i].shots)
    {
        float acc = 100.0f*accuracym[i].hits/(float)accuracym[i].shots;
        string line;
        rows++;
        if(i == GUN_GRENADE || i == GUN_SHOTGUN)
        {
            formatstring(line)("\f5%5.1f%s (%.1f/%d) :\f0%s", acc, "%", accuracym[i].hits, (int)accuracym[i].shots, weapstr(i));
        }
        else
        {
            formatstring(line)("\f5%5.1f%s (%d/%d) :\f0%s", acc, "%", (int)accuracym[i].hits, (int)accuracym[i].shots, weapstr(i));
        }
        cols=max(cols,(int)strlen(line));
        lines.add(newstring(line));
    }
    if(rows<1) return;
    cols++;
    blendbox(x_offset, spacing+y_offset, spacing+x_offset+curfont->defaultw*cols, y_offset-curfont->defaulth*rows, true, -1);
    int x=0;
    loopv(lines)
    {
        char *line = lines[i];
        draw_textf(line,spacing*0.5+x_offset,y_offset-x*curfont->defaulth-0.5*spacing);
        x++;
    }
}

void accuracyreset()
{
    loopi(NUMGUNS)
    {
        accuracym[i].hits=accuracym[i].shots=0;
    }
    conoutf(_("Your accuracy has been reset."));
}
COMMAND(accuracyreset, "");
// weapon

weapon::weapon(class playerent *owner, int type) : type(type), owner(owner), info(guns[type]),
    ammo(owner->ammo[type]), mag(owner->mag[type]), gunwait(owner->gunwait[type]), reloading(0)
{
}

const int weapon::weaponchangetime = 400;
const float weapon::weaponbeloweye = 0.2f;

int weapon::flashtime() const { return max((int)info.attackdelay, 120)/4; }

void weapon::sendshoot(vec &from, vec &to, int millis)
{
    if(owner!=player1) return;
    owner->shoot = true;
    addmsg(SV_SHOOT, "ri2i3iv", millis, owner->weaponsel->type,
           (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF),
           hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
    player1->pstatshots[player1->weaponsel->type]++; //NEW
}

bool weapon::modelattacking()
{
    int animtime = min(owner->gunwait[owner->weaponsel->type], (int)owner->weaponsel->info.attackdelay);
    if(lastmillis - owner->lastaction < animtime) return true;
    else return false;
}

void weapon::attacksound()
{
    if(info.sound == S_NULL) return;
    bool local = (owner == player1);
    audiomgr.playsound(info.sound, owner, local ? SP_HIGH : SP_NORMAL);
}

bool weapon::reload(bool autoreloaded)
{
    if(mag>=info.magsize || ammo<=0) return false;
    updatelastaction(owner);
    reloading = lastmillis;
    gunwait += info.reloadtime;

    int numbullets = min(info.magsize - mag, ammo);
    mag += numbullets;
    ammo -= numbullets;

    bool local = (player1 == owner);
    audiomgr.playsound(info.reload, owner, local ? SP_HIGH : SP_NORMAL);
    if(local)
    {
        addmsg(SV_RELOAD, "ri2", lastmillis, owner->weaponsel->type);
        if(identexists("onReload"))
        {
            defformatstring(str)("onReload %d", (int)autoreloaded);
            execute(str);
        }
    }
    return true;
}

VARP(oldfashionedgunstats, 0, 0, 1);

void weapon::renderstats()
{
    char gunstats[64];
    if(oldfashionedgunstats) sprintf(gunstats, "%i/%i", mag, ammo); else sprintf(gunstats, "%i", mag);
    draw_text(gunstats, 590, 823);
    if(!oldfashionedgunstats)
    {
        int offset = text_width(gunstats);
        glScalef(0.5f, 0.5f, 1.0f);
        sprintf(gunstats, "%i", ammo);
        draw_text(gunstats, (590 + offset)*2, 826*2);
        glLoadIdentity();
    }
}


static int recoiltest = 0;//VAR(recoiltest, 0, 0, 1); // DISABLE ON RELEASE
static int recoilincrease = 2; //VAR(recoilincrease, 1, 2, 10);
static int recoilbase = 40;//VAR(recoilbase, 0, 40, 1000);
static int maxrecoil = 1000;//VAR(maxrecoil, 0, 1000, 1000);

void weapon::attackphysics(vec &from, vec &to) // physical fx to the owner
{
    const guninfo &g = info;
    vec unitv;
    float dist = to.dist(from, unitv);
    float f = dist/1000;
    int spread = dynspread();
    float recoil = dynrecoil()*-0.01f;

    // spread
    if(spread>1)
    {
        #define RNDD (rnd(spread)-spread/2)*f
        vec r(RNDD, RNDD, RNDD);
        to.add(r);
        #undef RNDD
    }
    // kickback & recoil
    if(recoiltest)
    {
        owner->vel.add(vec(unitv).mul(recoil/dist).mul(owner->crouching ? 0.75 : 1.0f));
        owner->pitchvel = min(powf(shots/(float)(recoilincrease), 2.0f)+(float)(recoilbase)/10.0f, (float)(maxrecoil)/10.0f);
    }
    else
    {
        owner->vel.add(vec(unitv).mul(recoil/dist).mul(owner->crouching ? 0.75 : 1.0f));
        owner->pitchvel = min(powf(shots/(float)(g.recoilincrease), 2.0f)+(float)(g.recoilbase)/10.0f, (float)(g.maxrecoil)/10.0f);
    }
}

VARP(righthanded, 0, 1, 1); // flowtron 20090727

void weapon::renderhudmodel(int lastaction, int index)
{
    playerent *p = owner;
    vec unitv;
    float dist = worldpos.dist(p->o, unitv);
    unitv.div(dist);

    weaponmove wm;
    if(!intermission) wm.calcmove(unitv, lastaction, p);
//    if(!intermission) wm.calcmove(unitv, p->lastaction, p);
    defformatstring(widn)("modmdlweap%d", type);
    defformatstring(path)("weapons/%s", identexists(widn)?getalias(widn):info.modelname);
    bool emit = (wm.anim&ANIM_INDEX)==ANIM_GUN_SHOOT && (lastmillis - lastaction) < flashtime();
//    bool emit = (wm.anim&ANIM_INDEX)==ANIM_GUN_SHOOT && (lastmillis - p->lastaction) < flashtime();
    rendermodel(path, wm.anim|ANIM_DYNALLOC|(righthanded==index ? ANIM_MIRROR : 0)|(emit ? ANIM_PARTICLE : 0), 0, -1, wm.pos, p->yaw+90, p->pitch+wm.k_rot, 40.0f, wm.basetime, NULL, NULL, 1.28f);
}

void weapon::updatetimers(int millis)
{
    if(gunwait) gunwait = max(gunwait - (millis-owner->lastaction), 0);
}

void weapon::onselecting()
{
    updatelastaction(owner);
    audiomgr.playsound(S_GUNCHANGE, owner == player1? SP_HIGH : SP_NORMAL);
}

void weapon::renderhudmodel() { renderhudmodel(owner->lastaction); }
void weapon::renderaimhelp(bool teamwarning) { drawcrosshair(owner, teamwarning ? CROSSHAIR_TEAMMATE : owner->weaponsel->type + 3); }
int weapon::dynspread() { return info.spread; }
float weapon::dynrecoil() { return info.recoil; }
bool weapon::selectable() { return this != owner->weaponsel && owner->state == CS_ALIVE && !owner->weaponchanging; }
bool weapon::deselectable() { return !reloading; }

void weapon::equipplayer(playerent *pl)
{
    if(!pl) return;
    pl->weapons[GUN_ASSAULT] = new assaultrifle(pl);
    pl->weapons[GUN_GRENADE] = new grenades(pl);
    pl->weapons[GUN_KNIFE] = new knife(pl);
    pl->weapons[GUN_PISTOL] = new pistol(pl);
    pl->weapons[GUN_CPISTOL] = new cpistol(pl);
    pl->weapons[GUN_CARBINE] = new carbine(pl);
    pl->weapons[GUN_SHOTGUN] = new shotgun(pl);
    pl->weapons[GUN_SNIPER] = new sniperrifle(pl);
    pl->weapons[GUN_SUBGUN] = new subgun(pl);
    pl->weapons[GUN_AKIMBO] = new akimbo(pl);
    pl->selectweapon(GUN_ASSAULT);
    pl->setprimary(GUN_ASSAULT);
    pl->setnextprimary(GUN_ASSAULT);
}

bool weapon::valid(int id) { return id>=0 && id<NUMGUNS; }

// grenadeent

enum { NS_NONE, NS_ACTIVATED = 0, NS_THROWN, NS_EXPLODED };

grenadeent::grenadeent (playerent *owner, int millis)
{
    ASSERT(owner);
    nadestate = NS_NONE;
    local = owner==player1;
    bounceent::owner = owner;
    bounceent::millis = lastmillis;
    timetolive = 2000-millis;
    bouncetype = BT_NADE;
    maxspeed = 30.0f;
    rotspeed = 6.0f;
    distsincebounce = 0.0f;
}

grenadeent::~grenadeent()
{
    if(owner && owner->weapons[GUN_GRENADE]) owner->weapons[GUN_GRENADE]->removebounceent(this);
}

void grenadeent::explode()
{
    if(nadestate!=NS_ACTIVATED && nadestate!=NS_THROWN ) return;
    nadestate = NS_EXPLODED;
    static vec n(0,0,0);
    hits.setsize(0);
    splash();
    if(local)
        addmsg(SV_EXPLODE, "ri3iv", lastmillis, GUN_GRENADE, millis, hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
    audiomgr.playsound(S_FEXPLODE, &o);
}

void grenadeent::splash()
{
    particle_splash(PART_SPARK, 50, 300, o);
    particle_fireball(PART_FIREBALL, o);
    addscorchmark(o);
    adddynlight(NULL, o, 16, 200, 100, 255, 255, 224);
    adddynlight(NULL, o, 16, 600, 600, 192, 160, 128);
    if(owner == player1)
    {
        accuracym[GUN_GRENADE].shots++;
    }
    else if(!m_botmode)
    {
        return;
    }
    int damage = guns[GUN_GRENADE].damage;

    radialeffect(owner->type == ENT_BOT ? player1 : owner, o, damage, owner, GUN_GRENADE);
    loopv(players)
    {
        playerent *p = players[i];
        if(!p) continue;
        radialeffect(p, o, damage, owner, GUN_GRENADE);
    }
}

void grenadeent::activate(const vec &from, const vec &to)
{
    if(nadestate!=NS_NONE) return;
    nadestate = NS_ACTIVATED;

    if(local)
    {
        addmsg(SV_SHOOT, "ri2i3i", millis, owner->weaponsel->type,
//                (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF),
               (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF),
               0);
        audiomgr.playsound(S_GRENADEPULL, SP_HIGH);
        player1->pstatshots[GUN_GRENADE]++; //NEW
    }
}

void grenadeent::_throw(const vec &from, const vec &vel)
{
    if(nadestate!=NS_ACTIVATED) return;
    nadestate = NS_THROWN;
    this->vel = vel;
    this->o = from;
    this->resetinterp();
    inwater = hdr.waterlevel>o.z;
    if(local)
    {
        addmsg(SV_THROWNADE, "ri7", int(o.x*DMF), int(o.y*DMF), int(o.z*DMF), int(vel.x*DMF), int(vel.y*DMF), int(vel.z*DMF), lastmillis-millis);
        audiomgr.playsound(S_GRENADETHROW, SP_HIGH);
    }
    else audiomgr.playsound(S_GRENADETHROW, owner);
}

void grenadeent::moveoutsidebbox(const vec &direction, playerent *boundingbox)
{
    vel = direction;
    o = boundingbox->o;
    inwater = hdr.waterlevel>o.z;

    boundingbox->cancollide = false;
    loopi(10) moveplayer(this, 10, true, 10);
    boundingbox->cancollide = true;
}

void grenadeent::destroy() { explode(); }
bool grenadeent::applyphysics() { return nadestate==NS_THROWN; }

void grenadeent::oncollision()
{
    if(distsincebounce>=1.5f) audiomgr.playsound(S_GRENADEBOUNCE1+rnd(2), &o);
    distsincebounce = 0.0f;
}

void grenadeent::onmoved(const vec &dist)
{
    distsincebounce += dist.magnitude();
}

// grenades

grenades::grenades(playerent *owner) : weapon(owner, GUN_GRENADE), inhandnade(NULL), throwwait((13*1000)/40), throwmillis(0), state(GST_NONE) {}

int grenades::flashtime() const { return 0; }

bool grenades::busy() { return state!=GST_NONE; }

bool grenades::attack(vec &targ)
{
    int attackmillis = lastmillis-owner->lastaction;
    vec &to = targ;

    bool quickwait = attackmillis*3>=gunwait && !(m_arena && m_teammode && arenaintermission);
    bool waitdone = attackmillis>=gunwait && quickwait;
    if(waitdone) gunwait = reloading = 0;

    switch(state)
    {
        case GST_NONE:
            if(waitdone && owner->attacking && this==owner->weaponsel)
            {
                attackevent(owner, type);
                activatenade(to); // activate
            }
        break;

        case GST_INHAND:
            if(waitdone || ( quicknade && quickwait ) )
            {
                if(!owner->attacking || this!=owner->weaponsel) thrownade(); // throw
                else if(!inhandnade->isalive(lastmillis)) dropnade(); // drop & have fun
            }
            break;

        case GST_THROWING:
            if(attackmillis >= throwwait) // throw done
            {
                reset();
                if(!mag && this==owner->weaponsel) // switch to primary immediately
                {
                    owner->weaponchanging = lastmillis-1-(weaponchangetime/2);
                    owner->nextweaponsel = owner->weaponsel = owner->primweap;
                }
                return false;
            }
            break;
    }
    return true;
}

void grenades::attackfx(const vec &from, const vec &to, int millis) // other player's grenades
{
    throwmillis = lastmillis-millis;
    if(millis == 0) audiomgr.playsound(S_GRENADEPULL, owner); // activate
    else if(millis > 0) // throw
    {
        grenadeent *g = new grenadeent(owner, millis);
        bounceents.add(g);
        g->_throw(from, to);
    }
}

int grenades::modelanim()
{
    if(state == GST_THROWING) return ANIM_GUN_THROW;
    else
    {
        int animtime = min(gunwait, (int)info.attackdelay);
        if(state == GST_INHAND || lastmillis - owner->lastaction < animtime) return ANIM_GUN_SHOOT;
    }
    return ANIM_GUN_IDLE;
}

void grenades::activatenade(const vec &to)
{
    if(!mag) return;
    throwmillis = 0;

    inhandnade = new grenadeent(owner);
    bounceents.add(inhandnade);

    updatelastaction(owner);
    mag--;
    gunwait = info.attackdelay;
    owner->lastattackweapon = this;
    state = GST_INHAND;
    inhandnade->activate(owner->o, to);
}

void grenades::thrownade()
{
    if (quicknade && owner->weaponsel->type == GUN_GRENADE) selectweapon(owner->prevweaponsel);
    quicknade = false;
    if(!inhandnade) return;
    const float speed = cosf(RAD*owner->pitch);
    vec vel(sinf(RAD*owner->yaw)*speed, -cosf(RAD*owner->yaw)*speed, sinf(RAD*owner->pitch));
    vel.mul(1.5f);
    thrownade(vel);
}

void grenades::thrownade(const vec &vel)
{
    inhandnade->moveoutsidebbox(vel, owner);
    inhandnade->_throw(inhandnade->o, vel);
    inhandnade = NULL;

    throwmillis = lastmillis;
    updatelastaction(owner);
    state = GST_THROWING;
    if(this==owner->weaponsel) owner->attacking = false;
}

void grenades::dropnade()
{
    vec n(0,0,0);
    thrownade(n);
}

void grenades::renderstats()
{
    char gunstats[64];
    sprintf(gunstats, "%i", mag);
    draw_text(gunstats, 830, 823);
}

bool grenades::selectable() { return weapon::selectable() && state != GST_INHAND && mag; }
void grenades::reset() { throwmillis = 0; state = GST_NONE; }

void grenades::onselecting() { reset(); audiomgr.playsound(S_GUNCHANGE); }
void grenades::onownerdies()
{
    reset();
    if(owner==player1 && inhandnade) dropnade();
}

void grenades::removebounceent(bounceent *b)
{
    if(b == inhandnade) { inhandnade = NULL; reset(); }
}

// gun base class

gun::gun(playerent *owner, int type) : weapon(owner, type) {}

bool gun::attack(vec &targ)
{
    int attackmillis = lastmillis-owner->lastaction - gunwait;
    if(attackmillis<0) return false;
    gunwait = reloading = 0;

    if(!owner->attacking)
    {
        shots = 0;
        checkautoreload();
        return false;
    }

    attackmillis = lastmillis - min(attackmillis, curtime);
    updatelastaction(owner, attackmillis);
    if(!mag)
    {
        audiomgr.playsoundc(S_NOAMMO);
        gunwait += 250;
        owner->lastattackweapon = NULL;
        shots = 0;
        checkautoreload();
        return false;
    }

    owner->lastattackweapon = this;
    shots++;

    if(!info.isauto) owner->attacking = false;

    if(burstshotssettings[this->type] > 0 && shots >= burstshotssettings[this->type]) owner->attacking = false;

    vec from = owner->o;
    vec to = targ;
    from.z -= weaponbeloweye;

    attackphysics(from, to);

    attackevent(owner, type);

    hits.setsize(0);
    raydamage(from, to, owner);
    attackfx(from, to, 0);

    gunwait = info.attackdelay;
    mag--;

    sendshoot(from, to, attackmillis);

    return true;
}

void gun::attackfx(const vec &from, const vec &to, int millis)
{
    traceresult_s tr;
    TraceLine(from, to, owner, true, &tr);
    addbullethole(owner, from, tr.end);
    addshotline(owner, from, tr.end);
    particle_splash(PART_SPARK, 5, 250, tr.end);
    adddynlight(owner, from, 4, 100, 50, 96, 80, 64);
    attacksound();
}

int gun::modelanim() { return modelattacking() ? ANIM_GUN_SHOOT|ANIM_LOOP : ANIM_GUN_IDLE; }
void gun::checkautoreload() { if(autoreload && owner==player1 && !mag) reload(true); }


// shotgun

shotgun::shotgun(playerent *owner) : gun(owner, GUN_SHOTGUN) {}

void shotgun::attackphysics(vec &from, vec &to)
{
    createrays(from, to);
    gun::attackphysics(from, to);
}

bool shotgun::attack(vec &targ)
{
    return gun::attack(targ);
}

void shotgun::attackfx(const vec &from, const vec &to, int millis)
{
    loopi(SGRAYS) particle_splash(PART_SPARK, 5, 200, sgr[i].rv);

    if(addbullethole(owner, from, to))
        loopk(3) loopi(3) addbullethole(owner, from, sgr[k*SGRAYS+i*SGRAYS/3].rv, 0, false);
    adddynlight(owner, from, 4, 100, 50, 96, 80, 64);
    attacksound();
}

bool shotgun::selectable() { return weapon::selectable() && !m_noprimary && this == owner->primweap; }


// subgun

subgun::subgun(playerent *owner) : gun(owner, GUN_SUBGUN) {}
bool subgun::selectable() { return weapon::selectable() && !m_noprimary && this == owner->primweap; }
int subgun::dynspread() { return shots > 2 ? 70 : ( info.spread + ( shots > 0 ? ( shots == 1 ? 5 : 10 ) : 0 ) ); } // CHANGED: 2010nov19 was: min(info.spread + 10 * shots, 80)


// sniperrifle

sniperrifle::sniperrifle(playerent *owner) : gun(owner, GUN_SNIPER), scoped(false) {}

void sniperrifle::attackfx(const vec &from, const vec &to, int millis)
{
    traceresult_s tr;
    TraceLine(from, to, owner, true, &tr);
    addbullethole(owner, from, tr.end);
    addshotline(owner, from, tr.end);
    particle_splash(PART_SPARK, 50, 200, tr.end);
    particle_trail(PART_SMOKE, 500, from, tr.end);
    adddynlight(owner, from, 4, 100, 50, 96, 80, 64);
    attacksound();
}

bool sniperrifle::reload(bool autoreloaded)
{
    bool r = weapon::reload(autoreloaded);
    if(owner==player1 && r) { scoped = false; player1->scoping = false; }
    return r;
}

#define SCOPESETTLETIME 180
int sniperrifle::dynspread()
{
    if(scoped)
    {
        int scopetime = lastmillis - scoped_since;
        if(scopetime > SCOPESETTLETIME)
            return 1;
        else
            return max((info.spread * (SCOPESETTLETIME - scopetime)) / SCOPESETTLETIME, 1);
    }
    return info.spread;
}
float sniperrifle::dynrecoil() { return scoped && lastmillis - scoped_since > SCOPESETTLETIME ? info.recoil / 3 : info.recoil; }
bool sniperrifle::selectable() { return weapon::selectable() && !m_noprimary && this == owner->primweap; }
void sniperrifle::onselecting() { weapon::onselecting(); scoped = false; player1->scoping = false; }
void sniperrifle::ondeselecting() { scoped = false; player1->scoping = false; }
void sniperrifle::onownerdies() { scoped = false; player1->scoping = false; }
void sniperrifle::renderhudmodel() { if(!scoped) weapon::renderhudmodel(); }

void sniperrifle::renderaimhelp(bool teamwarning)
{
    if(scoped) drawscope();
    if(scoped || teamwarning) drawcrosshair(owner, teamwarning ? CROSSHAIR_TEAMMATE : CROSSHAIR_SCOPE, NULL, 24.0f);
}

void sniperrifle::setscope(bool enable)
{
    if(this == owner->weaponsel && !reloading && owner->state == CS_ALIVE)
    {
        if(scoped == false && enable == true) scoped_since = lastmillis;
        if(enable != scoped) owner->scoping = enable;
        scoped = enable;
    }
}

// carbine

carbine::carbine(playerent *owner) : gun(owner, GUN_CARBINE) {}

bool carbine::selectable() { return weapon::selectable() && !m_noprimary && this == owner->primweap; }


// assaultrifle

assaultrifle::assaultrifle(playerent *owner) : gun(owner, GUN_ASSAULT) {}

int assaultrifle::dynspread() { return shots > 2 ? 55 : ( info.spread + ( shots > 0 ? ( shots == 1 ? 5 : 15 ) : 0 ) ); }
float assaultrifle::dynrecoil() { return info.recoil + (rnd(8)*-0.01f); }
bool assaultrifle::selectable() { return weapon::selectable() && !m_noprimary && this == owner->primweap; }

// combat pistol

cpistol::cpistol(playerent *owner) : gun(owner, GUN_CPISTOL), bursting(false) {}
bool cpistol::selectable() { return false; /*return weapon::selectable() && !m_noprimary && this == owner->primweap;*/ }
void cpistol::onselecting() { weapon::onselecting(); bursting = false; }
void cpistol::ondeselecting() { bursting = false; }
bool cpistol::reload(bool autoreloaded)
{
    bool r = weapon::reload(autoreloaded);
    if(owner==player1 && r) { bursting = false; }
    return r;
}

bool burst = false;
int burstshots = 0;

bool cpistol::attack(vec &targ) // modded from gun::attack // FIXME
{
    int attackmillis = lastmillis-owner->lastaction - gunwait;
    if(attackmillis<0) return false;
    gunwait = reloading = 0;

    if (bursting) burst = true;

    if(!owner->attacking)
    {
        shots = 0;
        checkautoreload();
        return false;
    }

    attackmillis = lastmillis - min(attackmillis, curtime);
    updatelastaction(owner, attackmillis);
    if(!mag)
    {
        audiomgr.playsoundc(S_NOAMMO);
        gunwait += 250;
        owner->lastattackweapon = NULL;
        shots = 0;
        checkautoreload();
        return false;
    }

    owner->lastattackweapon = this;
    shots++;
    if (burst) burstshots++;

    if(!burst) owner->attacking = false;

    vec from = owner->o;
    vec to = targ;
    from.z -= weaponbeloweye;

    attackphysics(from, to);

    hits.setsize(0);
    raydamage(from, to, owner);
    attackfx(from, to, 0);

    if ( burst && burstshots > 2 )
    {
        gunwait = 500;
        burstshots = 0;
        burst = owner->attacking = false;
    }
    else if ( burst )
    {
        gunwait = 80;
    }
    else
    {
        gunwait = info.attackdelay;
    }
    mag--;

    sendshoot(from, to, attackmillis);
    return true;
}

void cpistol::setburst(bool enable)
{
    if(this == owner->weaponsel && !reloading && owner->state == CS_ALIVE)
    {
        bursting = enable;
    }
}

void setburst(bool enable)
{
    if(player1->weaponsel->type != GUN_CPISTOL) return;
    if(intermission) return;
    cpistol *cp = (cpistol *)player1->weaponsel;
    cp->setburst(enable);
    if (!burst)
    {
        if ( enable && burstshots == 0 ) attack(true);
    }
    else
    {
        if ( burstshots == 0 ) burst = player1->attacking = enable;
    }
}

COMMAND(setburst, "d");

// pistol

pistol::pistol(playerent *owner) : gun(owner, GUN_PISTOL) {}
bool pistol::selectable() { return weapon::selectable() && !m_nopistol; }


// akimbo

akimbo::akimbo(playerent *owner) : gun(owner, GUN_AKIMBO), akimboside(0), akimbomillis(0)
{
    akimbolastaction[0] = akimbolastaction[1] = 0;
}

void akimbo::attackfx(const vec &from, const vec &to, int millis)
{
    akimbolastaction[akimboside] = owner->lastaction;
    akimboside = (akimboside+1)%2;
    gun::attackfx(from, to, millis);
}

void akimbo::onammopicked()
{
    akimbomillis = lastmillis + 30000;
    if(owner==player1)
    {
        // if(owner->weaponsel->type!=GUN_SNIPER && owner->weaponsel->type!=GUN_GRENADE) owner->weaponswitch(this);
        if(akimboautoswitch || owner->weaponsel->type==GUN_PISTOL) owner->weaponswitch(this); // Give the client full control over akimbo auto-switching // Bukz 2011apr23
        addmsg(SV_AKIMBO, "ri", lastmillis);
    }
}

void akimbo::onselecting()
{
    gun::onselecting();
    akimbolastaction[0] = akimbolastaction[1] = lastmillis;
}

bool akimbo::selectable() { return weapon::selectable() && !m_nopistol && owner->akimbo; }
void akimbo::updatetimers(int millis) { weapon::updatetimers(millis); /*loopi(2) akimbolastaction[i] = millis;*/ }
void akimbo::reset() { akimbolastaction[0] = akimbolastaction[1] = akimbomillis = akimboside = 0; }

void akimbo::renderhudmodel()
{
    weapon::renderhudmodel(akimbolastaction[0], 0);
    weapon::renderhudmodel(akimbolastaction[1], 1);
}

bool akimbo::timerout() { return akimbomillis && akimbomillis <= lastmillis; }


// knife

knife::knife(playerent *owner) : weapon(owner, GUN_KNIFE) {}

int knife::flashtime() const { return 0; }

bool knife::attack(vec &targ)
{
    int attackmillis = lastmillis-owner->lastaction - gunwait;
    if(attackmillis<0) return false;
    gunwait = reloading = 0;

    if(!owner->attacking) return false;

    attackmillis = lastmillis - min(attackmillis, curtime);
    updatelastaction(owner, attackmillis);

    owner->lastattackweapon = this;
    owner->attacking = false;

    vec from = owner->o;
    vec to = targ;
    from.z -= weaponbeloweye;

    vec unitv;
    float dist = to.dist(from, unitv);
    unitv.div(dist);
    unitv.mul(3); // punch range
    to = from;
    to.add(unitv);
    if ( owner->pitch < 0 ) to.z += 2.5 * sin( owner->pitch * 0.01745329 );

    attackevent(owner, type);

    hits.setsize(0);
    raydamage(from, to, owner);
    attackfx(from, to, 0);
    sendshoot(from, to, attackmillis);
    gunwait = info.attackdelay;

    return true;
}

int knife::modelanim() { return modelattacking() ? ANIM_GUN_SHOOT : ANIM_GUN_IDLE; }

void knife::drawstats() {}
void knife::attackfx(const vec &from, const vec &to, int millis) { attacksound(); }
void knife::renderstats() { }


void setscope(bool enable)
{
    if(player1->weaponsel->type != GUN_SNIPER) return;
    if(intermission) return;
    sniperrifle *sr = (sniperrifle *)player1->weaponsel;
    sr->setscope(enable);
}

COMMANDF(setscope, "i", (int *on) { setscope(*on != 0); });


void shoot(playerent *p, vec &targ)
{
    if(p->state!=CS_ALIVE || p->weaponchanging) return;
    weapon *weap = p->weaponsel;
    if(weap)
    {
        weap->attack(targ);
        loopi(NUMGUNS)
        {
            weapon *bweap = p->weapons[i];
            if(bweap != weap && bweap->busy()) bweap->attack(targ);
        }
    }
}

void checkakimbo()
{
    if(player1->akimbo)
    {
        akimbo &a = *((akimbo *)player1->weapons[GUN_AKIMBO]);
        if(a.timerout() || player1->state == CS_DEAD)
        {
            weapon &p = *player1->weapons[GUN_PISTOL];
            player1->akimbo = false;
            a.reset();
            // transfer ammo to pistol
            p.mag = min((int)p.info.magsize, max(a.mag, p.mag));
            p.ammo = max(p.ammo, p.ammo);
            // fix akimbo magcontent
            a.mag = 0;
            a.ammo = 0;
            if(player1->weaponsel->type==GUN_AKIMBO)
            {

                switch(akimboendaction)
                {
                    case 0: player1->weaponswitch(player1->weapons[GUN_KNIFE]); break;
                    case 1:
                    {
                        if(player1->weapons[GUN_PISTOL]->ammo) player1->weaponswitch(&p);
                        else player1->weaponswitch(player1->weapons[GUN_KNIFE]);
                        break;
                    }
                    case 2:
                    {
                        if(player1->mag[GUN_GRENADE]) player1->weaponswitch(player1->weapons[GUN_GRENADE]);
                        else {
                            if(player1->weapons[GUN_PISTOL]->ammo) player1->weaponswitch(&p);
                            else player1->weaponswitch(player1->weapons[GUN_KNIFE]);
                        }
                        break;
                    }
                    case 3:
                    {
                        if(player1->ammo[player1->primary]) player1->weaponswitch(player1->weapons[player1->primary]);
                        else {
                            if(player1->mag[GUN_GRENADE]) player1->weaponswitch(player1->weapons[GUN_GRENADE]);
                            else {
                                if(player1->weapons[GUN_PISTOL]->ammo) player1->weaponswitch(&p);
                                else player1->weaponswitch(player1->weapons[GUN_KNIFE]);
                            }
                        }
                        break;
                    }
                    default: break;
                /*
                    case 0: player1->weaponswitch(&p); break;
                    case 1:
                    {
                        if( player1->ammo[player1->primary] ) player1->weaponswitch(player1->weapons[player1->primary]);
                        else player1->weaponswitch(&p);
                        break;
                    }
                    case 2:
                    {
                        if( player1->mag[GUN_GRENADE] ) player1->weaponswitch(player1->weapons[GUN_GRENADE]);
                        else
                        {
                            if( player1->ammo[player1->primary] ) player1->weaponswitch(player1->weapons[player1->primary]);
                            else player1->weaponswitch(&p);
                        }
                        break;
                    }
                    default: break;
                */
                }
            }
            if(player1->state != CS_DEAD) audiomgr.playsoundc(S_AKIMBOOUT);
        }
    }
}

void checkweaponstate()
{
    checkweaponswitch();
    checkakimbo();
}
