// weapon.cpp: all shooting and effects code

#include "cube.h"
#include "bot/bot.h"
#include "hudgun.h"

VARP(autoreload, 0, 1, 1);
VARP(akimboautoswitch, 0, 1, 1);
VARP(akimboendaction, 0, 3, 3); // 0: switch to knife, 1: stay with pistol (if has ammo), 2: switch to grenade (if possible), 3: switch to primary (if has ammo) - all fallback to previous one w/o ammo for target

struct sgray {
    int ds; // damage flag: 0:outer, 1:medium, 2:center
    vec rv; // ray vector
};

sgray sgr[SGRAYS*3];

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
        if(player1->attacking && player1->state == CS_ALIVE) attack(false);
        int i = w->type;
        // substitute akimbo
        weapon *akimbo = player1->weapons[GUN_AKIMBO];
        if(w->type==GUN_PISTOL && akimbo->selectable()) w = akimbo;

        player1->weaponswitch(w);
        exechook(HOOK_SP, "onWeaponSwitch", "%d", i);
    }
}

void requestweapon(char *ws)
{
    int w = getlistindex(ws, gunnames, true, 0);
    if(keypressed && player1->state == CS_ALIVE && w >= 0)
    {
        if (player1->akimbo && w == GUN_PISTOL) w = GUN_AKIMBO;
        selectweapon(player1->weapons[w]);
    }
}
COMMANDN(weapon, requestweapon, "s");

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
COMMAND(shiftweapon, "i");

bool quicknade = false;

void quicknadethrow(bool on)
{
    if(player1->state != CS_ALIVE) return;
    if(on)
    {
        if(player1->weapons[GUN_GRENADE]->mag > 0)
        {
            if(player1->weaponsel->type != GUN_GRENADE) selectweapon(player1->weapons[GUN_GRENADE]);
            if(player1->weaponsel->type == GUN_GRENADE || player1->nextweaponsel->type == GUN_GRENADE)
            {
                if(!player1->weapons[GUN_GRENADE]->busy()) attack(true);
            }
        }
    }
    else
    {
        attack(false);
        if(player1->weaponsel->type == GUN_GRENADE) quicknade = true;
    }
}
COMMAND(quicknadethrow, "d");

COMMANDF(currentprimary, "", () { intret(player1->primweap->type); });
COMMANDF(prevweapon, "", () { intret(player1->prevweaponsel->type); });
COMMANDF(curweapon, "", () { intret(player1->weaponsel->type); });

COMMANDF(magcontent, "s", (char *ws)
{
    int w = getlistindex(ws, gunnames, true, -1);
    if(w >= 0) intret(player1->weapons[w]->mag);
    else intret(-1);
});

COMMANDF(magreserve, "s", (char *ws)
{
    int w = getlistindex(ws, gunnames, true, -1);
    if(w >= 0) intret(player1->weapons[w]->ammo);
    else intret(-1);
});

void tryreload(playerent *p)
{
    if(!p || p->state!=CS_ALIVE || p->weaponsel->reloading || p->weaponchanging) return;
    p->weaponsel->reload(false);
}

COMMANDF(reload, "", () { tryreload(player1); });

void createrays(const vec &from, const vec &to) // create random spread of rays for the shotgun
{
    vec dir = vec(from).sub(to);
    float f = dir.magnitude() / 10.0f;
    dir.normalize();
    vec spoke;
    spoke.orthogonal(dir);
    spoke.normalize();
    spoke.mul(f);
    loopk(3)
    {
        float base;
        int wrange;
        switch(k)
        {
            case 0:  base = SGCObase / 100.0f; wrange = SGCOrange; break;
            case 1:  base = SGCMbase / 100.0f; wrange = SGCMrange; break;
            case 2:
            default: base = SGCCbase / 100.0f; wrange = SGCCrange; break;
        }
        float rnddir = rndscale(PI2);
        loopi(SGRAYS)
        {
            int j = k * SGRAYS + i;
            sgr[j].ds = k;
            vec p(spoke);
            int rndmul = rnd(wrange);
            float veclen = base + rndmul/100.0f;
            p.mul(veclen);

            p.rotate(PI2 / SGRAYS * i + rnddir, dir);
            vec rray = vec(to);
            rray.add(p);
            sgr[j].rv = rray;
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
}

bool intersect(entity *e, const vec &from, const vec &to, vec *end)
{
    mapmodelinfo *mmi = getmminfo(e->attr2);
    if(!mmi || !mmi->h) return false;

    float lo = float(S(e->x, e->y)->floor + mmi->zoff + e->attr3);
    return intersectbox(vec(e->x, e->y, lo + mmi->h / 2.0f), vec(mmi->rad, mmi->rad, mmi->h / 2.0f), from, to, end);
}

playerent *intersectclosest(const vec &from, const vec &to, const playerent *at, float &bestdistsquared, int &hitzone, bool aiming = true)
{
    playerent *best = NULL;
    bestdistsquared = 1e16f;
    int zone;
    if(at!=player1 && player1->state==CS_ALIVE && (zone = intersect(player1, from, to)))
    {
        best = player1;
        bestdistsquared = at->o.squareddist(player1->o);
        hitzone = zone;
    }
    loopv(players)
    {
        playerent *o = players[i];
        if(!o || o==at || (o->state!=CS_ALIVE && (aiming || (o->state!=CS_EDITING && o->state!=CS_LAGGED)))) continue;
        float distsquared = at->o.squareddist(o->o);
        if(distsquared < bestdistsquared && (zone = intersect(o, from, to)))
        {
            best = o;
            bestdistsquared = distsquared;
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

inline bool intersecttriangle(const vec &from, const vec &dir, const vec &v0, const vec &v1, const vec &v2, vec *end, float *_t) // precise but rather expensive, based on Moeller–Trumbore intersection algorithm
{
    const float EPSILON = 0.00001f;
    vec edge1 = v1; edge1.sub(v0);      // edge1 = v1 - v0
    vec edge2 = v2; edge2.sub(v0);      // edge2 = v2 - v0
    vec pvec; pvec.cross(dir, edge2);   // pvec = dir x edge2
    float det = edge1.dot(pvec);        // det = edge1 * pvec
    if(fabs(det) < EPSILON) return false;
    float invdet = 1.0f / det;
    vec tvec = from; tvec.sub(v0);      // tvec = from - v0
    float u = invdet * tvec.dot(pvec);  // u = tvec * pvec / det
    if(u < 0.0f || u > 1.0f) return false;
    vec qvec; qvec.cross(tvec, edge1);  // qvec = tvec x edge1
    float v = invdet * dir.dot(qvec);   // v = dir * qvec / det
    if(v < 0.0f || u + v > 1.0f) return false;
    float t = invdet * edge2.dot(qvec); // t = edge2 * qvec / det
    if(t < EPSILON) return false;
    if(_t) *_t = t;                     // 0..1 if intersection is between from and to
    if(end) *end = dir, end->mul(t).add(from); // calculate point of intersection
    return true;
}

inline bool intersecttriangle2(const vec &from, const vec &to, const vec &v0, const vec &v1, const vec &v2, vec *end, float *_t)
{
    vec dir = to; dir.sub(from);        // dir = to - from
    return intersecttriangle(from, dir, v0, v1, v2, end, _t);
}

inline bool intersectcorner(const vec &from, const vec &dir, int x, int y, int size, bool cdir, vec *end, float *_t)
{
    float fsx, fsy, dsx, dsy;
    if(cdir)
    {
        dsx = dir.x + dir.y; fsx = from.x - x + from.y - y - size;
        dsy = dir.y - dir.x; fsy = from.y - y - from.x + x + size;
    }
    else
    {
        dsx = dir.x - dir.y; fsx = from.x - x - from.y + y;
        dsy = dir.y + dir.x; fsy = from.y - y + from.x - x;
    }
    if(!dsx) return false;
    float t = -fsx / dsx;
    float dy = dsy * t + fsy;
    if(fabs(t * dsx + fsx) < NEARZERO && dy >= 0 && dy <= 2 * size)
    {
        if(_t) *_t = t;
        if(end) *end = dir, end->mul(t).add(from);
        return true;
    }
    return false;
}

void intersectgeometry(const vec &from, vec &to) // check line for contact with map geometry, shorten if necessary
{
    int x = from.x, y = from.y, hfnb[4] = { 0, 1, ssize, ssize +1 };
    if(OUTBORD(x, y) || from.z < -127.0f || from.z > 127.0f) return;
    vec d = to;                                     // d: direction
    d.sub(from);

    float distmin = 1.0f, vdelta[4];
    sqr *r[2], *s, *nb[4];
    if(fabs(d.x) + fabs(d.y) < 2.0f) d.mul((distmin = 32.0f));
    loop(xy, 2) // first check x == const planes and then y == const planes
    {
        float dxy = xy ? d.y : d.x, fromxy = xy ? from.y : from.x;
        bool dxynz = fabs(dxy) < NEARZERO;
        if(dxy == 0.0f) dxy = 1e-20;        // hack

        int ixy = fromxy, step = dxy < 0 ? -1 : 1, steps = fabs(dxy) + 1, sqrdir = xy ? -ssize : -1;
        while(steps-- > 0)
        {
            // intersect d with plane
            float t = dxynz ? distmin : (ixy - fromxy) / dxy;
            vec p = d;
            p.mul(t).add(from);

            // check position
            if(xy) x = p.x, y = ixy;
            else x = ixy, y = p.y;
            if(t > distmin || OUTBORD(x, y)) break;

            // always check cubes on both sides of the plane
            r[0] = S(x, y);
            r[1] = r[0] + sqrdir;
            if(t > 0) loopk(2)
            {
                s = r[k];
                if(SOLID(s) || s->floor > p.z || s->ceil < p.z || s->type == CORNER)
                { // cube s is a probable hit, examine further
                    if(k)
                    { // match x|y back to s
                        if(xy) y--;
                        else x--;
                    }

                    if(s->type == CORNER)
                    {
                        sqr *ns, *h = NULL, *n;
                        int bx, by, bs;
                        int q = cornertest(x, y, bx, by, bs, ns, h);
                        vec newto; float newdist;
                        if(intersectcorner(from, d, bx, by, bs, !(q & 1), &newto, &newdist) && newdist < distmin && (!h || newto.z < ns->floor || newto.z > ns->ceil)) to = newto, distmin = newdist;
                        if(d.z)
                        { // intersect with z == const plane where the corner ends
                            bool downwards = d.z < 0.0f;
                            n = h ? h : ns;
                            float endplate = downwards ? n->floor : n->ceil;
                            float tz = (endplate - from.z) / d.z;
                            vec pz = d;
                            pz.mul(tz).add(from);
                            if(pz.x >= bx && pz.x <= bx + bs && pz.y >= by && pz.y <= by + bs && tz < distmin) to = pz, distmin = tz;
                            if(h)
                            { // intersect with the triangles where socket corners end
                                float sockplate = downwards ? ns->floor : ns->ceil;
                                tz = (sockplate - from.z) / d.z;
                                pz = d; pz.mul(tz).add(from);
                                float cx = pz.x - bx, cy = pz.y - by;
                                bool hit = false;
                                switch(q)                                   //  0XX3
                                {                                           //  XXXX
                                    case 0: hit = cx + cy >= bs; break;     //  1XX2
                                    case 1: hit = cx >= cy;      break;
                                    case 2: hit = cx + cy <= bs; break;
                                    case 3: hit = cx <= cy;      break;
                                    default: hit = true;         break;     // annoy bad mappers
                                }
                                if(hit && cx >= 0 && cy >= 0 && cx <= bs && cy <= bs && tz < distmin) to = pz, distmin = tz;
                            }
                        }
                    }
                    else
                    {
                        // finish checking walls
                        if(SOLID(s) || s->type == SPACE || (s->type == CHF && s->floor > p.z) || (s->type == FHF && s->ceil < p.z))
                        {
                            if(t < distmin) to = p, distmin = t;
                        }
                        else if(s->type == CHF || s->type == FHF)
                        {
                            loopi(4) vdelta[i] = ((nb[i] = s + hfnb[i]))->vdelta;   // 23
                            int nb2 = "\002\001\013\023"[xy + 2 * k], nb1 = nb2 >> 3; nb2 &= 3;
                            if(s->type == FHF)
                            { // FHF side surfaces
                                float a = s->floor - vdelta[nb1] / 4.0f, b = (vdelta[nb1] - vdelta[nb2]) / 4.0f, dummy, i = modff(xy ? p.x : p.y, &dummy);
                                if(a + i * b > p.z && t < distmin) to = p, distmin = t;
                            }
                            else
                            { // CHF side surfaces
                                float a = s->ceil + vdelta[nb1] / 4.0f, b = (vdelta[nb2] - vdelta[nb1]) / 4.0f, dummy, i = modff(xy ? p.x : p.y, &dummy);
                                if(a + i * b < p.z && t < distmin) to = p, distmin = t;
                            }
                        }

                        // check floor and ceiling
                        bool isflat = true, downwards = d.z < 0.0f;
                        if(s->type == CHF || s->type == FHF) loopi(3) if(vdelta[3] != vdelta[i]) isflat = false;
                        float h = downwards ? s->floor - (isflat && s->type == FHF ? vdelta[0] / 4.0f : 0) : s->ceil + (isflat && s->type == CHF ? vdelta[0] / 4.0f : 0);
                        if(isflat || (downwards && s->type == CHF) || (!downwards && s->type == FHF))
                        { // intersect with z == const plane on either SPACE or flat ends of FHF and CHF
                            float tz = d.z != 0.0f ? (h - from.z) / d.z : distmin;
                            vec pz = d;
                            pz.mul(tz).add(from);
                            if(int(pz.x) == x && int(pz.y) == y && tz < distmin) to = pz, distmin = tz;
                        }
                        if(!isflat)
                        { // sloped CHF or FHF: check two triangles, regardless of "downwards" or not (FHF slopes can be seen looking upwards)
                            if(s->type == FHF) loopi(4) vdelta[i] *= -1.0f;
                            float hh = s->type == FHF ? s->floor : s->ceil;
                            vec v0(x, y, hh + vdelta[0] / 4.0f), v1(x + 1, y, hh + vdelta[1] / 4.0f), v2(x, y + 1, hh + vdelta[2] / 4.0f), v3(x + 1, y + 1, hh + vdelta[3] / 4.0f);
                            float newdist; vec newto;
                            if(s->type == FHF)
                            {
                                if(intersecttriangle(from, d, v0, v1, v2, &newto, &newdist) && newdist < distmin) to = newto, distmin = newdist;
                                if(intersecttriangle(from, d, v3, v1, v2, &newto, &newdist) && newdist < distmin) to = newto, distmin = newdist;
                            }
                            else
                            {
                                if(intersecttriangle(from, d, v0, v1, v3, &newto, &newdist) && newdist < distmin) to = newto, distmin = newdist;
                                if(intersecttriangle(from, d, v2, v0, v3, &newto, &newdist) && newdist < distmin) to = newto, distmin = newdist;
                            }
                        }
                    }
                }
            }
            ixy += step;
        }
    }
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
    if(owner == player1) exechook(HOOK_SP, "onAttack", "%d", weapon);
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
            lasthit = lastmillis;
        }
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
            if(d != at) updatedmgindicator(player1, at->o);
            damageblend(damage, d);
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
    else if(multiplayer(NULL)) bounceents.add((bounceent *)player1);
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
                copystring(model, "weapons/grenade/static");
                break;
            case BT_GIB:
            default:
            {
                uint n = (((4*(uint)(size_t)p)+(uint)p->timetolive)%3)+1;
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
        rendermodel(model, anim|ANIM_LOOP|ANIM_DYNALLOC, 0, 1.1f, o, 0, p->yaw+90, p->pitch, 0, basetime);
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
    d->nocorpse = true; // don't render regular corpse: it was gibbed

    loopi(gibnum)
    {
        bounceent *p = bounceents.add(new bounceent);
        p->owner = d;
        p->millis = lastmillis;
        p->timetolive = gibttl+rnd(10)*100;
        p->bouncetype = BT_GIB;

        p->o = d->o;
        p->o.z -= d->aboveeye;
        p->inwater = waterlevel > p->o.z;

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

void shorten(const vec &from, vec &target, float distsquared)
{
    target.sub(from);
    float m = target.squaredlen();
    if(m < 0.07f) target.add(vec(0.1f, 0.1f, 0.1f));   // if "from == target" just fake a target to avoid zero-length fx vectors
    else target.mul(max(0.07f, sqrtf(distsquared / m)));
    target.add(from);
}

void raydamage(vec &from, vec &to, playerent *d)
{
    int dam = d->weaponsel->info.damage;
    int hitzone = -1;
    playerent *o = NULL;
    float distsquared, hitdistsquared = 0.0f;
    bool hit = false;
    int rayscount = 0, hitscount = 0;
    if(d->weaponsel->type==GUN_SHOTGUN)
    {
        playerent *hits[3*SGRAYS];
        loopk(3)
        loopi(SGRAYS)
        {
            rayscount++;
            int h = k*SGRAYS + i;
            if((hits[h] = intersectclosest(from, sgr[h].rv, d, distsquared, hitzone)))
                shorten(from, sgr[h].rv, (hitdistsquared = distsquared));
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
                    case 0: numhits_o++; break;
                    case 1: numhits_m++; break;
                    case 2: numhits_c++; break;
                    default: break;
                }
                for(int j = i+1; j < 3*SGRAYS; j++) if(hits[j] == o)
                {
                    hits[j] = NULL;
                    switch(sgr[j].ds)
                    {
                        case 0: numhits_o++; break;
                        case 1: numhits_m++; break;
                        case 2: numhits_c++; break;
                        default: break;
                    }
                }
                int numhits = numhits_o + numhits_m + numhits_c;
                int dmgreal = 0;
                float dmg4r = 0.0f;
                bool withBONUS = false;
                if(SGDMGBONUS)
                {
                    float d2o = SGDMGDISTB;
                    if(o) d2o = vec(from).sub(o->o).magnitude();
                    if(d2o <= (SGDMGDISTB/10.0f) && numhits)
                    {
                        dmg4r += SGDMGBONUS;
                        withBONUS = true;
                    }
                }
                dmg4r += (SGCOdmg / 10.0f * SGDMGTOTAL / 100.0f) * numhits_o / 21.0f;
                dmg4r += (SGCMdmg / 10.0f * SGDMGTOTAL / 100.0f) * numhits_m / 21.0f;
                dmg4r += (SGCCdmg / 10.0f * SGDMGTOTAL / 100.0f) * numhits_c / 21.0f;
                dmgreal = (int) ceil(dmg4r);
                int info = (withBONUS ? SGDMGBONUS : 0) | (numhits_c << 8) | (numhits_m << 16) | (numhits_o << 24);
                if(numhits) hitpush(dmgreal, o, d, from, to, d->weaponsel->type, numhits == SGRAYS * 3, info);

                if(d == player1) hit = true;
                hitscount+=numhits;
            }
        }
        if(hitscount) shorten(from, to, hitdistsquared);
    }
    else if((o = intersectclosest(from, to, d, distsquared, hitzone)))
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
        if(d == player1) hit = true;
        shorten(from, to, distsquared);
        hitscount++;
    }

    if(d==player1)
    {
        if(!rayscount) rayscount = 1;
        if(hit) accuracym[d->weaponsel->type].hits += (float)hitscount / rayscount;
        accuracym[d->weaponsel->type].shots++;
    }
}

const char *weapstr(int i) { return valid_weapon(i) ? guns[i].title : "x"; }

VARP(accuracy,0,0,1);

void r_accuracy(int h)
{
    int i = player1->weaponsel->type;
    if(accuracy && valid_weapon(i) && i != GUN_CPISTOL)
    {
        int x_offset = 2 * HUDPOS_X_BOTTOMLEFT, y_offset = 2 * (h - 1.75 * FONTH);
        string line;
        float acc = accuracym[i].shots ? 100.0 * accuracym[i].hits / (float)accuracym[i].shots : 0;
        if(i == GUN_GRENADE || i == GUN_SHOTGUN)
        {
            formatstring(line)("\f5%5.1f%% (%.1f/%d): \f0%s", acc, accuracym[i].hits, accuracym[i].shots, weapstr(i));
        }
        else
        {
            formatstring(line)("\f5%5.1f%% (%d/%d): \f0%s", acc, (int)accuracym[i].hits, accuracym[i].shots, weapstr(i));
        }
        blendbox(x_offset, y_offset + FONTH, x_offset + text_width(line) + 2 * FONTH, y_offset - FONTH, false, -1);
        draw_textf("%s", x_offset + FONTH, y_offset - 0.5 * FONTH, line);
    }
}

void accuracyinfo()
{
    vector <char*>lines;
    loopi(NUMGUNS) if(i != GUN_CPISTOL && accuracym[i].shots)
    {
        float acc = 100.0 * accuracym[i].hits / (float)accuracym[i].shots;
        string line;
        if(i == GUN_GRENADE || i == GUN_SHOTGUN)
        {
            formatstring(line)("\f0%-10s\t\f5 %.1f%% (%.1f/%d)", weapstr(i), acc, accuracym[i].hits, accuracym[i].shots);
        }
        else
        {
            formatstring(line)("\f0%-10s\t\f5 %.1f%% (%d/%d)", weapstr(i), acc, (int)accuracym[i].hits, accuracym[i].shots);
        }
        lines.add(newstring(line));
    }
    loopv(lines) conoutf("%s", lines[i]);
    lines.deletearrays();
}

COMMAND(accuracyinfo, "");

void accuracyreset()
{
    loopi(NUMGUNS)
    {
        accuracym[i].hits=accuracym[i].shots=0;
    }
    conoutf("Your accuracy has been reset");
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
    if(info.reload != S_NULL) audiomgr.playsound(info.reload, owner, local ? SP_HIGH : SP_NORMAL);
    if(local)
    {
        addmsg(SV_RELOAD, "ri2", lastmillis, owner->weaponsel->type);
        exechook(HOOK_SP, "onReload", "%d", (int)autoreloaded);
    }
    return true;
}

VARP(oldfashionedgunstats, 0, 0, 1);

void weapon::renderstats()
{
    string gunstats;
    if(oldfashionedgunstats) formatstring(gunstats)("%d/%d", mag, ammo); else formatstring(gunstats)("%d", mag);
    draw_text(gunstats, HUDPOS_WEAPON + HUDPOS_NUMBERSPACING, 823);
    if(!oldfashionedgunstats)
    {
        int offset = text_width(gunstats);
        glScalef(0.5f, 0.5f, 1.0f);
        formatstring(gunstats)("%d", ammo);
        draw_text(gunstats, (HUDPOS_WEAPON + HUDPOS_NUMBERSPACING + offset)*2, 826*2);
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
    defformatstring(path)("weapons/%s", info.modelname);
    bool emit = (wm.anim&ANIM_INDEX)==ANIM_GUN_SHOOT && (lastmillis - lastaction) < flashtime();
//    bool emit = (wm.anim&ANIM_INDEX)==ANIM_GUN_SHOOT && (lastmillis - p->lastaction) < flashtime();
    rendermodel(path, wm.anim|ANIM_DYNALLOC|(righthanded==index ? ANIM_MIRROR : 0)|(emit ? ANIM_PARTICLE : 0), 0, -1, wm.pos, 0, p->yaw+90, p->pitch+wm.k_rot, 40.0f, wm.basetime, NULL, NULL, 1.28f);
}

void weapon::updatetimers(int millis)
{
    if(gunwait) gunwait = max(gunwait - (millis-owner->lastaction), 0);
}

void weapon::onselecting()
{
    updatelastaction(owner);
    bool local = (owner == player1);
    audiomgr.playsound(S_GUNCHANGE, owner, local ? SP_HIGH : SP_NORMAL);
}

void weapon::renderhudmodel() { renderhudmodel(owner->lastaction); }
void weapon::renderaimhelp(bool teamwarning)
{
    if(!editmode) drawcrosshair(owner, teamwarning ? CROSSHAIR_TEAMMATE : owner->weaponsel->type);
    else drawcrosshair(owner, CROSSHAIR_EDIT);
}
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
    if(((grenades *)owner->weapons[GUN_GRENADE])->state == GST_NONE) owner->weapons[GUN_GRENADE]->reset();
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
    inwater = waterlevel > o.z;
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
    inwater = waterlevel > o.z;

    boundingbox->cancollide = false;
    loopi(10) moveplayer(this, 10, true, 10);
    boundingbox->cancollide = true;
}

void grenadeent::destroy() { explode(); }
bool grenadeent::applyphysics() { return nadestate==NS_THROWN; }

void grenadeent::oncollision()
{
    if(distsincebounce>=1.5f) audiomgr.playsound(rnd(2) ? S_GRENADEBOUNCE1 : S_GRENADEBOUNCE2, &o);
    distsincebounce = 0.0f;
}

void grenadeent::onmoved(const vec &dist)
{
    distsincebounce += dist.magnitude();
}

// grenades

grenades::grenades(playerent *owner) : weapon(owner, GUN_GRENADE), inhandnade(NULL), throwwait((13*1000)/40), throwmillis(0), cookingmillis(0), state(GST_NONE) {}

int grenades::flashtime() const { return 0; }

bool grenades::busy() { return state!=GST_NONE; }

bool grenades::attack(vec &targ)
{
    int attackmillis = lastmillis-owner->lastaction;
    vec &to = targ;

    bool waitdone = attackmillis>=gunwait && !(m_arena && m_teammode && arenaintermission);
    if(waitdone) gunwait = reloading = 0;

    switch(state)
    {
        case GST_NONE:
            if(waitdone && owner->attacking && this==owner->weaponsel)
            {
                attackevent(owner, type);
                activatenade(to); // activate
            }
            else quicknade = false;
        break;

        case GST_INHAND:
            if(waitdone)
            {
                if(!owner->attacking || this!=owner->weaponsel) thrownade(); // throw
                else if(!inhandnade->isalive(lastmillis)) dropnade(); // drop & have fun
            }
            break;

        case GST_THROWING:
            if(attackmillis >= throwwait) // throw done
            {
                if(this == owner->weaponsel)
                {
                    if(quicknade || !mag)
                    {
                        owner->weaponchanging = lastmillis - 1 - (weaponchangetime / 2);
                        if(quicknade) owner->nextweaponsel = owner->weaponsel = owner->prevweaponsel; // switch to previous weapon immediately
                        else owner->nextweaponsel = owner->weaponsel = owner->primweap; // switch to primary immediately
                    }
                }
                reset();
                return false;
            }
            break;
    }
    return true;
}

void grenades::attackfx(const vec &from, const vec &to, int millis) // other player's grenades
{
    throwmillis = lastmillis-millis;
    cookingmillis = millis;
    if(millis == 0 || millis == -1)
    {
        state = GST_INHAND;
        audiomgr.playsound(S_GRENADEPULL, owner); // activate
    }
    else if(millis > 0) // throw
    {
        grenadeent *g = new grenadeent(owner, millis);
        state = GST_THROWING;
        bounceents.add(g);
        g->_throw(from, to);
    }
}

int grenades::modelanim()
{
    if(state == GST_THROWING)
    {
        if(lastmillis - owner->lastaction >= throwwait) state = GST_NONE;
        return ANIM_GUN_THROW;
    }
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
    sprintf(gunstats, "%d", mag);
    draw_text(gunstats, oldfashionedgunstats ? HUDPOS_GRENADE + HUDPOS_NUMBERSPACING + 25 : HUDPOS_GRENADE + HUDPOS_NUMBERSPACING, 823);
}

bool grenades::selectable() { return weapon::selectable() && state != GST_INHAND && mag; }
void grenades::reset() { throwmillis = 0; cookingmillis = 0; if(owner == player1) quicknade = false; state = GST_NONE; }

void grenades::onselecting()
{
    reset();
    bool local = (owner == player1);
    audiomgr.playsound(S_GUNCHANGE, owner, local ? SP_HIGH : SP_NORMAL);
}

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
        bool local = (owner == player1);
        audiomgr.playsound(S_NOAMMO, owner, local ? SP_HIGH : SP_NORMAL);
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
    if(from.squareddist(to) > 0.07f)
    {
        addbullethole(owner, from, to);
        addshotline(owner, from, to);
    }
    particle_splash(PART_SPARK, 5, 250, to);
    adddynlight(owner, from, 4, 100, 50, 96, 80, 64);
    attacksound();
}

int gun::modelanim() { return modelattacking() ? ANIM_GUN_SHOOT|ANIM_LOOP : ANIM_GUN_IDLE; }
void gun::checkautoreload() { if(autoreload && owner==player1 && !mag) reload(true); }
void gun::onownerdies() { shots = 0; }


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
    if(from.squareddist(to) > 0.07f)
    {
        addbullethole(owner, from, to);
        addshotline(owner, from, to);
        particle_trail(PART_SMOKE, 500, from, to);
    }
    particle_splash(PART_SPARK, 50, 200, to);
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
void sniperrifle::onownerdies() { scoped = false; player1->scoping = false; shots = 0; }
void sniperrifle::renderhudmodel() { if(!scoped) weapon::renderhudmodel(); }

void sniperrifle::renderaimhelp(bool teamwarning)
{
    if(scoped) drawscope();
    if(!editmode)
    {
        if(scoped || teamwarning) drawcrosshair(owner, teamwarning ? CROSSHAIR_TEAMMATE : CROSSHAIR_SCOPE, NULL, 24.0f);
    }
    else drawcrosshair(owner, CROSSHAIR_EDIT);
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
        if(akimboautoswitch || owner->weaponsel->type==GUN_PISTOL)
        {
            if(player1->weapons[GUN_GRENADE]->busy()) player1->attacking = false;
            owner->weaponswitch(this);
        }
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
    sniperrifle *sr = (sniperrifle *)player1->weaponsel;
    sr->setscope(enable);
}

COMMANDF(setscope, "i", (int *on) { setscope(*on != 0); });

void shoot(playerent *p, vec &targ)
{
    if(p->state!=CS_ALIVE) return;
    weapon *weap = p->weaponsel, *bweap = p->weapons[GUN_GRENADE];
    if(bweap->busy()) bweap->attack(targ); // continue ongoing nade action
    else bweap = NULL;
    if(weap && !p->weaponchanging && weap != bweap) weap->attack(targ);
}

void checkakimbo()
{
    if(player1->akimbo)
    {
        akimbo &a = *((akimbo *)player1->weapons[GUN_AKIMBO]);
        if(a.timerout() || player1->state == CS_DEAD || player1->state == CS_SPECTATE)
        {
            weapon &p = *player1->weapons[GUN_PISTOL];
            player1->akimbo = false;
            a.reset();

            if(player1->weaponsel->type==GUN_AKIMBO || (player1->weaponchanging && player1->nextweaponsel->type==GUN_AKIMBO))
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
            if(player1->state != CS_DEAD && player1->state != CS_SPECTATE) audiomgr.playsoundc(S_AKIMBOOUT);
        }
    }
}

void checkweaponstate()
{
    checkweaponswitch();
    checkakimbo();
}
