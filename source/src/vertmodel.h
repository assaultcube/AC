VARP(dynshadowsize, 4, 5, 8);
VARP(aadynshadow, 0, 2, 3);
VARP(saveshadows, 0, 1, 1);

VARP(dynshadowquad, 0, 0, 1);

VAR(shadowyaw, 0, 45, 360);
vec shadowdir(0, 0, -1), shadowpos(0, 0, 0);

const int dbgstenc = 0;
const int dbgvlight = 0;
//VAR(dbgstenc, 0, 0, 2);
//VAR(dbgvlight, 0, 0, 1);

VARP(mdldlist, 0, 1, 1);

vec modelpos;
float modelyaw, modelpitch;

struct vertmodel : model
{
    struct anpos
    {
        int fr1, fr2;
        float t;

        void setframes(const animstate &as)
        {
            int time = lastmillis-as.basetime;
            fr1 = (int)(time/as.speed); // round to full frames
            t = (time-fr1*as.speed)/as.speed; // progress of the frame, value from 0.0f to 1.0f
            ASSERT(t >= 0.0f);
            if(as.anim&ANIM_LOOP)
            {
                fr1 = fr1%as.range+as.frame;
                fr2 = fr1+1;
                if(fr2>=as.frame+as.range) fr2 = as.frame;
            }
            else
            {
                fr1 = min(fr1, as.range-1)+as.frame;
                fr2 = min(fr1+1, as.frame+as.range-1);
            }
            if(as.anim&ANIM_REVERSE)
            {
                fr1 = (as.frame+as.range-1)-(fr1-as.frame);
                fr2 = (as.frame+as.range-1)-(fr2-as.frame);
            }
        }

        bool operator==(const anpos &a) const { return fr1==a.fr1 && fr2==a.fr2 && (fr1==fr2 || t==a.t); }
        bool operator!=(const anpos &a) const { return fr1!=a.fr1 || fr2!=a.fr2 || (fr1!=fr2 && t!=a.t); }
    };

    struct bb
    {
        vec low, high;

        void addlow(const vec &v)
        {
            low.x = min(low.x, v.x);
            low.y = min(low.y, v.y);
            low.z = min(low.z, v.z);
        }

        void addhigh(const vec &v)
        {
            high.x = max(high.x, v.x);
            high.y = max(high.y, v.y);
            high.z = max(high.z, v.z);
        }

        void add(const vec &v)
        {
            addlow(v);
            addhigh(v);
        }

        void add(const bb &b)
        {
            addlow(b.low);
            addhigh(b.high);
        }
    };

    struct tcvert { float u, v; };
    struct lightvert { uchar r, g, b, a; };
    struct tri { ushort vert[3]; ushort neighbor[3]; };

    struct part;

    typedef tristrip::drawcall drawcall;

    struct dyncacheentry : modelcacheentry<dyncacheentry>
    {
        anpos cur, prev;
        float t;

        vec *verts() { return (vec *)getdata(); }
        int numverts() { return int((size - sizeof(dyncacheentry)) / sizeof(vec)); }
    };

    struct shadowcacheentry : modelcacheentry<shadowcacheentry>
    {
        anpos cur, prev;
        float t;
        vec dir;

        ushort *idxs() { return (ushort *)getdata(); }
        int numidxs() { return int((size - sizeof(shadowcacheentry)) / sizeof(ushort)); }
    };

    struct lightcacheentry : modelcacheentry<lightcacheentry>
    {
        anpos cur, prev;
        float t;
        int lastcalclight;
        vec pos;
        float yaw, pitch;

        lightvert *verts() { return (lightvert *)getdata(); }
        int numverts() { return int((size - sizeof(lightcacheentry)) / sizeof(lightvert)); }
    };

    struct mesh
    {
        char *name;
        part *owner;
        vec *verts;
        tcvert *tcverts;
        bb *bbs;
        ushort *shareverts;
        tri *tris;
        int numverts, numtris;

        Texture *skin;
        int tex;

        modelcachelist<dyncacheentry> dyncache;
        modelcachelist<shadowcacheentry> shadowcache;
        modelcachelist<lightcacheentry> lightcache;

        ushort *dynidx;
        int dynlen;
        drawcall *dyndraws;
        int numdyndraws;
        GLuint statlist;
        int statlen;

        mesh() : name(0), owner(0), verts(0), tcverts(0), bbs(0), shareverts(0), tris(0), skin(notexture), tex(0), dynidx(0), dyndraws(0), statlist(0)
        {
        }

        ~mesh()
        {
            DELETEA(name);
            DELETEA(verts);
            DELETEA(tcverts);
            DELETEA(bbs);
            DELETEA(shareverts);
            DELETEA(tris);
            if(statlist) glDeleteLists(statlist, 1);
            DELETEA(dynidx);
            DELETEA(dyndraws);
        }

        void genstrips()
        {
            tristrip ts;
            ts.addtriangles(tris, numtris);
            vector<ushort> idxs;
            vector<drawcall> draws;
            ts.buildstrips(idxs, draws);
            dynidx = new ushort[idxs.length()];
            memcpy(dynidx, idxs.getbuf(), idxs.length()*sizeof(ushort));
            dynlen = idxs.length();
            dyndraws = new drawcall[draws.length()];
            memcpy(dyndraws, draws.getbuf(), draws.length()*sizeof(drawcall));
            numdyndraws = draws.length();
        }

        dyncacheentry *gendynverts(animstate &as, anpos &cur, anpos *prev, float ai_t)
        {
            dyncacheentry *d = dyncache.start();
            int cachelen = 0;
            for(; d != dyncache.end(); d = d->next, cachelen++)
            {
                if(d->cur != cur) continue;
                if(prev)
                {
                    if(d->prev == *prev && d->t == ai_t) return d;
                }
                else if(d->prev.fr1 < 0) return d;
            }

            d = dynalloc.allocate<dyncacheentry>((numverts + 1)*sizeof(vec));
            if(!d) return NULL;

            if(cachelen >= owner->model->cachelimit) dyncache.removelast();
            dyncache.addfirst(d);
            vec *buf = d->verts(),
                *vert1 = &verts[cur.fr1 * numverts],
                *vert2 = &verts[cur.fr2 * numverts],
                *pvert1 = NULL, *pvert2 = NULL;
            d->cur = cur;
            d->t = ai_t;
            if(prev)
            {
                d->prev = *prev;
                pvert1 = &verts[prev->fr1 * numverts];
                pvert2 = &verts[prev->fr2 * numverts];
            }
            else d->prev.fr1 = -1;

            #define iploop(body) \
                loopi(numverts) \
                { \
                    vec &v = buf[i]; \
                    body; \
                }
            #define ip(p1, p2, t) (p1+t*(p2-p1))
            #define ip_v(p, c, t) ip(p##1[i].c, p##2[i].c, t)
            #define ip_v_ai(c) ip(ip_v(pvert, c, prev->t), ip_v(vert, c, cur.t), ai_t)
            if(prev) iploop(v = vec(ip_v_ai(x), ip_v_ai(y), ip_v_ai(z)))
            else iploop(v = vec(ip_v(vert, x, cur.t), ip_v(vert, y, cur.t), ip_v(vert, z, cur.t)))
            #undef ip
            #undef ip_v
            #undef ip_v_ai

            if(d->verts() == lastvertexarray) lastvertexarray = (void *)-1;

            return d;
        }

        void weldverts()
        {
            hashtable<vec, int> idxs;
            shareverts = new ushort[numverts];
            loopi(numverts) shareverts[i] = (ushort)idxs.access(verts[i], i);
            for(int i = 1; i < owner->numframes; i++)
            {
                const vec *frame = &verts[i*numverts];
                loopj(numverts)
                {
                    if(frame[j] != frame[shareverts[j]]) shareverts[j] = (ushort)j;
                }
            }
        }

        void findneighbors()
        {
            hashtable<uint, uint> edges;
            loopi(numtris)
            {
                const tri &t = tris[i];
                loopj(3)
                {
                    uint e1 = shareverts[t.vert[j]], e2 = shareverts[t.vert[(j+1)%3]], shift = 0;
                    if(e1 > e2) { swap(e1, e2); shift = 16; }
                    uint &edge = edges.access(e1 | (e2<<16), ~0U);
                    if(((edge>>shift)&0xFFFF) != 0xFFFF) edge = 0;
                    else
                    {
                        edge &= 0xFFFF<<(16-shift);
                        edge |= i<<shift;
                    }
                }
            }
            loopi(numtris)
            {
                tri &t = tris[i];
                loopj(3)
                {
                    uint e1 = shareverts[t.vert[j]], e2 = shareverts[t.vert[(j+1)%3]], shift = 0;
                    if(e1 > e2) { swap(e1, e2); shift = 16; }
                    uint edge = edges[e1 | (e2<<16)];
                    if(!edge || int((edge>>shift)&0xFFFF)!=i) t.neighbor[j] = 0xFFFF;
                    else t.neighbor[j] = (edge>>(16-shift))&0xFFFF;
                }
            }
        }


        void cleanup()
        {
            if(statlist)
            {
                glDeleteLists(1, statlist);
                statlist = 0;
            }
        }

        shadowcacheentry *genshadowvolume(animstate &as, anpos &cur, anpos *prev, float ai_t, vec *buf)
        {
            if(!shareverts) return NULL;

            shadowcacheentry *d = shadowcache.start();
            int cachelen = 0;
            for(; d != shadowcache.end(); d = d->next, cachelen++)
            {
                if(d->dir != shadowpos || d->cur != cur) continue;
                if(prev)
                {
                    if(d->prev == *prev && d->t == ai_t) return d;
                }
                else if(d->prev.fr1 < 0) return d;
            }

            d = (owner->numframes > 1 || as.anim&ANIM_DYNALLOC ? dynalloc : statalloc).allocate<shadowcacheentry>(9*numtris*sizeof(ushort));
            if(!d) return NULL;

            if(cachelen >= owner->model->cachelimit) shadowcache.removelast();
            shadowcache.addfirst(d);
            d->dir = shadowpos;
            d->cur = cur;
            d->t = ai_t;
            if(prev) d->prev = *prev;
            else d->prev.fr1 = -1;

            static vector<uchar> side;
            side.setsize(0);

            loopi(numtris)
            {
                const tri &t = tris[i];
                const vec &a = buf[t.vert[0]], &b = buf[t.vert[1]], &c = buf[t.vert[2]];
                side.add(vec().cross(vec(b).sub(a), vec(c).sub(a)).dot(shadowdir) <= 0 ? 1 : 0);
            }

            ushort *idx = d->idxs();
            loopi(numtris) if(side[i])
            {
                const tri &t = tris[i];
                loopj(3)
                {
                    ushort n = t.neighbor[j];
                    if(n==0xFFFF || !side[n])
                    {
                        ushort e1 = shareverts[t.vert[j]], e2 = shareverts[t.vert[(j+1)%3]];
                        *idx++ = e2;
                        *idx++ = e1;
                        *idx++ = numverts;
                    }
                }
            }

            if(dbgstenc >= (owner->numframes > 1 || as.anim&ANIM_DYNALLOC ? 2 : 1)) conoutf("%s: %d tris", owner->filename, (idx - d->idxs())/3);

            d->size = (uchar *)idx - (uchar *)d;
            return d;
        }

        lightcacheentry *lightvertexes(animstate &as, anpos &cur, anpos *prev, float ai_t, vec *buf)
        {
            if(dbgvlight) return NULL;

            lightcacheentry *d = lightcache.start();
            int cachelen = 0;
            for(; d != lightcache.end(); d = d->next, cachelen++)
            {
                if(d->lastcalclight != lastcalclight || d->pos != modelpos || d->yaw != modelyaw || d->pitch != modelpitch || d->cur != cur) continue;
                if(prev)
                {
                    if(d->prev == *prev && d->t == ai_t) return d;
                }
                else if(d->prev.fr1 < 0) return d;
            }

            bb curbb;
            getcurbb(curbb, as, cur, prev, ai_t);
            float dist = max(curbb.low.magnitude(), curbb.high.magnitude());
            if(OUTBORDRAD(modelpos.x, modelpos.y, dist)) return NULL;

            d = (owner->numframes > 1 || as.anim&ANIM_DYNALLOC ? dynalloc : statalloc).allocate<lightcacheentry>(numverts*sizeof(lightvert));
            if(!d) return NULL;

            if(cachelen >= owner->model->cachelimit) lightcache.removelast();
            lightcache.addfirst(d);
            d->lastcalclight = lastcalclight;
            d->pos = modelpos;
            d->yaw = modelyaw;
            d->pitch = modelpitch;
            d->cur = cur;
            d->t = ai_t;
            if(prev) d->prev = *prev;
            else d->prev.fr1 = -1;

            const glmatrixf &m = matrixstack[matrixpos];
            lightvert *v = d->verts();
            loopi(numverts)
            {
                int x = (int)m.transformx(*buf), y = (int)m.transformy(*buf);
                const sqr *s = S(x, y);
                v->r = s->r;
                v->g = s->g;
                v->b = s->b;
                v->a = 255;
                v++;
                buf++;
            }
            if(d->verts() == lastcolorarray) lastcolorarray = (void *)-1;
            return d;
        }

        void getcurbb(bb &b, animstate &as, anpos &cur, anpos *prev, float ai_t)
        {
            b = bbs[cur.fr1];
            b.add(bbs[cur.fr2]);

            if(prev)
            {
                b.add(bbs[prev->fr1]);
                b.add(bbs[prev->fr2]);
            }
        }

        void render(animstate &as, anpos &cur, anpos *prev, float ai_t)
        {
            if(!(as.anim&ANIM_NOSKIN))
            {
                GLuint id = tex < 0 ? -tex : skin->id;
                if(tex > 0) id = lookuptexture(tex)->id;
                if(id != lasttex)
                {
                    glBindTexture(GL_TEXTURE_2D, id);
                    lasttex = id;
                }
                if(enablealphablend) { glDisable(GL_BLEND); enablealphablend = false; }
                if(skin->bpp == 32 && owner->model->alphatest > 0)
                {
                    if(!enablealphatest) { glEnable(GL_ALPHA_TEST); enablealphatest = true; }
                    if(lastalphatest != owner->model->alphatest)
                    {
                        glAlphaFunc(GL_GREATER, owner->model->alphatest);
                        lastalphatest = owner->model->alphatest;
                    }
                }
                else if(enablealphatest) { glDisable(GL_ALPHA_TEST); enablealphatest = false; }
                if(!enabledepthmask) { glDepthMask(GL_TRUE); enabledepthmask = true; }
            }

            if(enableoffset)
            {
                disablepolygonoffset(GL_POLYGON_OFFSET_FILL);
                enableoffset = false;
            }

            bool isstat = as.frame==0 && as.range==1;
            if(isstat && statlist && !stenciling)
            {
                glCallList(statlist);
                xtraverts += statlen;
                return;
            }
            else if(stenciling==1)
            {
                bb curbb;
                getcurbb(curbb, as, cur, prev, ai_t);
                glmatrixf mat;
                mat.mul(mvpmatrix, matrixstack[matrixpos]);
                if(!addshadowbox(curbb.low, curbb.high, shadowpos, mat)) return;
            }

            vec *buf = verts;
            dyncacheentry *d = NULL;
            if(!isstat)
            {
                d = gendynverts(as, cur, prev, ai_t);
                if(!d) return;
                buf = d->verts();
            }
            if(lastvertexarray != buf)
            {
                if(!lastvertexarray) glEnableClientState(GL_VERTEX_ARRAY);
                glVertexPointer(3, GL_FLOAT, sizeof(vec), buf);
                lastvertexarray = buf;
            }
            lightvert *vlight = NULL;
            if(as.anim&ANIM_NOSKIN && (!isstat || stenciling))
            {
                if(lasttexcoordarray)
                {
                    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                    lasttexcoordarray = NULL;
                }
            }
            else
            {
                if(lasttexcoordarray != tcverts)
                {
                    if(!lasttexcoordarray) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                    glTexCoordPointer(2, GL_FLOAT, sizeof(tcvert), tcverts);
                    lasttexcoordarray = tcverts;
                }
                if(owner->model->vertexlight)
                {
                    if(d) d->locked = true;
                    lightcacheentry *l = lightvertexes(as, cur, isstat ? NULL : prev, ai_t, buf);
                    if(d) d->locked = false;
                    if(l) vlight = l->verts();
                }
            }
            if(lastcolorarray != vlight)
            {
                if(vlight)
                {
                    if(!lastcolorarray) glEnableClientState(GL_COLOR_ARRAY);
                    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(lightvert), vlight);
                }
                else glDisableClientState(GL_COLOR_ARRAY);
                lastcolorarray = vlight;
            }
            if(stenciling)
            {
                if(d) d->locked = true;
                shadowcacheentry *s = genshadowvolume(as, cur, isstat ? NULL : prev, ai_t, buf);
                if(d) d->locked = false;
                if(!s) return;
                buf[numverts] = s->dir;
                glDrawElements(GL_TRIANGLES, s->numidxs(), GL_UNSIGNED_SHORT, s->idxs());
                xtraverts += s->numidxs();
                return;
            }

            bool builddlist = isstat && !owner->model->vertexlight && mdldlist;
            if(builddlist) glNewList(statlist = glGenLists(1), GL_COMPILE);
            loopi(numdyndraws)
            {
                const drawcall &d = dyndraws[i];
                if(hasDRE && !builddlist) glDrawRangeElements_(d.type, d.minvert, d.maxvert, d.count, GL_UNSIGNED_SHORT, &dynidx[d.start]);
                else glDrawElements(d.type, d.count, GL_UNSIGNED_SHORT, &dynidx[d.start]);
            }
            if(builddlist)
            {
                glEndList();
                glCallList(statlist);
                statlen = dynlen;
            }
            xtraverts += dynlen;
        }

        int findvert(int axis, int dir)
        {
            if(axis<0 || axis>2) return -1;
            int vert = -1;
            float bestval = -1e16f;
            loopi(numverts)
            {
                float val = verts[i][axis]*dir;
                if(val > bestval)
                {
                    vert = i;
                    bestval = val;
                }
            }
            return vert;
        }

        float calcradius()
        {
            float rad = 0;
            loopi(numverts) rad = max(rad, verts[i].magnitudexy());
            return rad;
        }

        void calcneighbors()
        {
            if(!shareverts)
            {
                weldverts();
                findneighbors();
            }
        }

        void calcbbs()
        {
            if(bbs) return;
            bbs = new bb[owner->numframes];
            loopi(owner->numframes)
            {
                bb &b = bbs[i];
                b.low = vec(1e16f, 1e16f, 1e16f);
                b.high = vec(-1e16f, -1e16f, -1e16f);
                const vec *frame = &verts[numverts*i];
                loopj(numverts) b.add(frame[j]);
            }
        }
    };

    struct animinfo
    {
        int frame, range;
        float speed;
    };

    struct particleemitter
    {
        int type, args[2], seed, lastemit;

        particleemitter() : type(-1), seed(-1), lastemit(-1)
        {
            memset(args, 0, sizeof(args));
        }
    };

    struct tag
    {
        char *name;
        vec pos;
        float transform[3][3];

        tag() : name(NULL) {}
        ~tag() { DELETEA(name); }

        void identity()
        {
            transform[0][0] = 1;
            transform[0][1] = 0;
            transform[0][2] = 0;

            transform[1][0] = 0;
            transform[1][1] = 1;
            transform[1][2] = 0;

            transform[2][0] = 0;
            transform[2][1] = 0;
            transform[2][2] = 1;
        }
    };

    struct linkedpart
    {
        part *p;
        particleemitter *emitter;
        vec *pos;

        linkedpart() : p(NULL), emitter(NULL), pos(NULL) {}
        ~linkedpart() { DELETEP(emitter); }
    };

    struct part
    {
        char *filename;
        vertmodel *model;
        int index, numframes;
        vector<mesh *> meshes;
        vector<animinfo> *anims;
        linkedpart *links;
        tag *tags;
        int numtags;
        GLuint *shadows;
        float shadowrad;

        part() : filename(NULL), anims(NULL), links(NULL), tags(NULL), numtags(0), shadows(NULL), shadowrad(0) {}
        virtual ~part()
        {
            DELETEA(filename);
            meshes.deletecontents();
            DELETEA(anims);
            DELETEA(links);
            DELETEA(tags);
            if(shadows) glDeleteTextures(numframes, shadows);
            DELETEA(shadows);
        }

        void cleanup()
        {
            loopv(meshes) meshes[i]->cleanup();
            if(shadows)
            {
                glDeleteTextures(numframes, shadows);
                DELETEA(shadows);
            }
        }

        bool link(part *link, const char *tag, vec *pos = NULL)
        {
            loopi(numtags) if(!strcmp(tags[i].name, tag))
            {
                links[i].p = link;
                links[i].pos = pos;
                return true;
            }
            return false;
        }

        bool gentag(const char *name, int *verts, int numverts, mesh *m = NULL)
        {
            if(!m)
            {
                if(meshes.empty()) return false;
                m = meshes[0];
            }
            if(numverts < 1) return false;
            loopi(numverts) if(verts[i] < 0 || verts[i] > m->numverts) return false;

            tag *ntags = new tag[(numtags + 1)*numframes];
            ntags[numtags].name = newstring(name);
            loopi(numframes)
            {
                memcpy(&ntags[(numtags + 1)*i], &tags[numtags*i], numtags*sizeof(tag));

                tag *t = &ntags[(numtags + 1)*i + numtags];
                t->pos = m->verts[m->numverts*i + verts[0]];
                if(numverts > 1)
                {
                    for(int j = 1; j < numverts; j++) t->pos.add(m->verts[m->numverts*i + verts[j]]);
                    t->pos.div(numverts);
                }
                t->identity();
            }
            loopi(numtags) tags[i].name = NULL;

            DELETEA(tags);
            tags = ntags;
            numtags++;

            linkedpart *nlinks = new linkedpart[numtags];
            loopi(numtags-1) swap(links[i].emitter, nlinks[i].emitter);
            DELETEA(links);
            links = nlinks;
            return true;
        }

        bool addemitter(const char *tag, int type, int arg1 = 0, int arg2 = 0)
        {
            loopi(numtags) if(!strcmp(tags[i].name, tag))
            {
                if(!links[i].emitter) links[i].emitter = new particleemitter;
                particleemitter &p = *links[i].emitter;
                p.type = type;
                p.args[0] = arg1;
                p.args[1] = arg2;
                return true;
            }
            return false;
        }

        void scaleverts(const float scale, const vec &translate)
        {
           loopv(meshes)
           {
               mesh &m = *meshes[i];
               loopj(numframes*m.numverts)
               {
                   vec &v = m.verts[j];
                   if(!index) v.add(translate);
                   v.mul(scale);
               }
           }
           loopi(numframes*numtags)
           {
               vec &v = tags[i].pos;
               if(!index) v.add(translate);
               v.mul(scale);
           }
        }

        void genstrips()
        {
            loopv(meshes) meshes[i]->genstrips();
        }

        virtual void getdefaultanim(animstate &as, int anim, int varseed)
        {
            as.frame = 0;
            as.range = 1;
        }

        void gentagmatrix(anpos &cur, anpos *prev, float ai_t, int i, GLfloat *matrix)
        {
            tag *tag1 = &tags[cur.fr1*numtags+i];
            tag *tag2 = &tags[cur.fr2*numtags+i];
            #define ip(p1, p2, t) (p1+t*(p2-p1))
            #define ip_ai_tag(c) ip( ip( tag1p->c, tag2p->c, prev->t), ip( tag1->c, tag2->c, cur.t), ai_t)
            if(prev)
            {
                tag *tag1p = &tags[prev->fr1 * numtags + i];
                tag *tag2p = &tags[prev->fr2 * numtags + i];
                loopj(3) matrix[j] = ip_ai_tag(transform[0][j]); // transform
                loopj(3) matrix[4 + j] = ip_ai_tag(transform[1][j]);
                loopj(3) matrix[8 + j] = ip_ai_tag(transform[2][j]);
                loopj(3) matrix[12 + j] = ip_ai_tag(pos[j]); // position
            }
            else
            {
                loopj(3) matrix[j] = ip(tag1->transform[0][j], tag2->transform[0][j], cur.t); // transform
                loopj(3) matrix[4 + j] = ip(tag1->transform[1][j], tag2->transform[1][j], cur.t);
                loopj(3) matrix[8 + j] = ip(tag1->transform[2][j], tag2->transform[2][j], cur.t);
                loopj(3) matrix[12 + j] = ip(tag1->pos[j], tag2->pos[j], cur.t); // position
            }
            #undef ip_ai_tag
            #undef ip
            matrix[3] = matrix[7] = matrix[11] = 0.0f;
            matrix[15] = 1.0f;
        }

        bool calcanimstate(int anim, int varseed, float speed, int basetime, dynent *d, animstate &as)
        {
            as.anim = anim;
            as.speed = speed<=0 ? 100.0f : speed;
            as.basetime = basetime;
            if((anim&ANIM_INDEX)==ANIM_ALL)
            {
                as.frame = 0;
                as.range = numframes;
            }
            else if(anims)
            {
                vector<animinfo> &ais = anims[anim&ANIM_INDEX];
                if(ais.length())
                {
                    animinfo &ai = ais[uint(varseed)%ais.length()];
                    as.frame = ai.frame;
                    as.range = ai.range;
                    if(ai.speed>0) as.speed = 1000.0f/ai.speed;
                }
                else getdefaultanim(as, anim&ANIM_INDEX, varseed);
            }
            else getdefaultanim(as, anim&ANIM_INDEX, varseed);
            if(anim&(ANIM_START|ANIM_END))
            {
                if(anim&ANIM_END) as.frame += as.range-1;
                as.range = 1;
            }

            if(as.frame+as.range>numframes)
            {
                if(as.frame>=numframes) return false;
                as.range = numframes-as.frame;
            }

            if(d && index<2)
            {
                if(d->lastmodel[index]!=this || d->lastanimswitchtime[index]==-1 || lastmillis-d->lastrendered>animationinterpolationtime)
                {
                    d->current[index] = as;
                    d->lastanimswitchtime[index] = lastmillis-animationinterpolationtime*2;
                }
                else if(d->current[index] != as)
                {
                    if(lastmillis-d->lastanimswitchtime[index]>animationinterpolationtime/2) d->prev[index] = d->current[index];
                    d->current[index] = as;
                    d->lastanimswitchtime[index] = lastmillis;
                }
                d->lastmodel[index] = this;
            }
            return true;
        }

        void render(int anim, int varseed, float speed, int basetime, dynent *d)
        {
            if(meshes.empty()) return;
            animstate as;
            if(!calcanimstate(anim, varseed, speed, basetime, d, as)) return;

            if(!meshes[0]->dynidx) genstrips();

            anpos prev, cur;
            cur.setframes(d && index<2 ? d->current[index] : as);

            float ai_t = 0;
            bool doai = !(anim&ANIM_NOINTERP) && d && index<2 && lastmillis-d->lastanimswitchtime[index]<animationinterpolationtime && d->prev[index].range>0;
            if(doai)
            {
                prev.setframes(d->prev[index]);
                ai_t = (lastmillis-d->lastanimswitchtime[index])/(float)animationinterpolationtime;
            }

            glPushMatrix();
            glMultMatrixf(matrixstack[matrixpos].v);
            loopv(meshes) meshes[i]->render(as, cur, doai ? &prev : NULL, ai_t);
            glPopMatrix();

            loopi(numtags)
            {
                linkedpart &link = links[i];
                if(!(link.p || link.pos || (anim&ANIM_PARTICLE && link.emitter))) continue;

                // render the linked models - interpolate rotation and position of the 'link-tags'
                glmatrixf linkmat;
                gentagmatrix(cur, doai ? &prev : NULL, ai_t, i, linkmat.v);

                matrixpos++;
                matrixstack[matrixpos].mul(matrixstack[matrixpos-1], linkmat);

                if(link.pos) *link.pos = matrixstack[matrixpos].gettranslation();

                if(link.p)
                {
                    vec oldshadowdir, oldshadowpos;

                    if(stenciling)
                    {
                        oldshadowdir = shadowdir;
                        oldshadowpos = shadowpos;
                        linkmat.invertnormal(shadowdir);
                        linkmat.invertvertex(shadowpos);
                    }

                    link.p->render(anim, varseed, speed, basetime, d);

                    if(stenciling)
                    {
                        shadowdir = oldshadowdir;
                        shadowpos = oldshadowpos;
                    }
                }

                if(anim&ANIM_PARTICLE && link.emitter)
                {
                    particleemitter &p = *link.emitter;

                    if(p.lastemit!=basetime)
                    {
                        p.seed = rnd(0x1000000);
                        p.lastemit = basetime;
                    }

                    particle_emit(p.type, p.args, basetime, p.seed, matrixstack[matrixpos].gettranslation());
                }

                matrixpos--;
            }
        }

        void setanim(int num, int frame, int range, float speed)
        {
            if(frame<0 || frame>=numframes || range<=0 || frame+range>numframes)
            {
                conoutf("invalid frame %d, range %d in model %s", frame, range, model->loadname);
                return;
            }
            if(!anims) anims = new vector<animinfo>[NUMANIMS];
            animinfo &ai = anims[num].add();
            ai.frame = frame;
            ai.range = range;
            ai.speed = speed;
        }

        virtual void begingenshadow()
        {
        }

        virtual void endgenshadow()
        {
        }

        void blurshadow(const uchar *in, uchar *out, uint size)
        {
            static const uint filter3x3[9] =
            {
                1, 2, 1,
                2, 4, 2,
                1, 2, 1
            };
            static const uint filter3x3sum = 16;
            const uchar *src = in, *prev = in - size, *next = in + size;
            uchar *dst = out;

            #define FILTER(c0, c1, c2, c3, c4, c5, c6, c7, c8) \
            { \
                uint c = *src, \
                     val = (filter3x3[0]*(c0) + filter3x3[1]*(c1) + filter3x3[2]*(c2) + \
                            filter3x3[3]*(c3) + filter3x3[4]*(c4) + filter3x3[5]*(c5) + \
                            filter3x3[6]*(c6) + filter3x3[7]*(c7) + filter3x3[8]*(c8)); \
                *dst++ = val/filter3x3sum; \
                src++; \
                prev++; \
                next++; \
            }

            FILTER(c, c, c, c, c, src[1], c, next[0], next[1]);
            for(uint x = 1; x < size-1; x++) FILTER(c, c, c, src[-1], c, src[1], next[-1], next[0], next[1]);
            FILTER(c, c, c, src[-1], c, c, next[-1], next[0], c);

            for(uint y = 1; y < size-1; y++)
            {
                FILTER(c, prev[0], prev[1], c, c, src[1], c, next[0], next[1]);
                for(uint x = 1; x < size-1; x++) FILTER(prev[-1], prev[0], prev[1], src[-1], c, src[1], next[-1], next[0], next[1]);
                FILTER(prev[-1], prev[0], c, src[-1], c, c, next[-1], next[0], c);
            }

            FILTER(c, prev[0], prev[1], c, c, src[1], c, c, c);
            for(uint x = 1; x < size-1; x++) FILTER(prev[-1], prev[0], prev[1], src[-1], c, src[1], c, c, c);
            FILTER(prev[-1], prev[0], c, src[-1], c, c, c, c, c);

            #undef FILTER
        }

        void genshadow(int aasize, int frame, stream *f)
        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            model->startrender();
            render(ANIM_ALL|ANIM_NOINTERP|ANIM_NOSKIN, 0, 1, lastmillis-frame, NULL);
            model->endrender();

            uchar *pixels = new uchar[2*aasize*aasize];
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, aasize, aasize, GL_RED, GL_UNSIGNED_BYTE, pixels);
#if 0
            SDL_Surface *img = SDL_CreateRGBSurface(SDL_SWSURFACE, aasize, aasize, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
            loopi(aasize*aasize) memset((uchar *)img->pixels + 3*i, pixels[i], 3);
            defformatstring(imgname)("%s_%d.bmp", model->loadname, frame);
            for(char *s; (s = strchr(imgname, '/'));) *s = '_';
            SDL_SaveBMP(img, imgname);
            SDL_FreeSurface(img);
#endif
            if(aasize > 1<<dynshadowsize)
                scaletexture(pixels, aasize, aasize, 1, pixels, 1<<dynshadowsize, 1<<dynshadowsize);

            int texsize = min(aasize, 1<<dynshadowsize);
            blurshadow(pixels, &pixels[texsize*texsize], texsize);
            if(f) f->write(&pixels[texsize*texsize], texsize*texsize);
            createtexture(shadows[frame], texsize, texsize, &pixels[texsize*texsize], 3, true, false, GL_ALPHA);

            delete[] pixels;
        }

        struct shadowheader
        {
            ushort size, frames;
            float height, rad;
        };

        void genshadows(float height, float rad)
        {
            if(shadows) return;

            char *filename = shadowfile();
            if(filename && loadshadows(filename)) return;

            shadowrad = rad;
            shadows = new GLuint[numframes];
            glGenTextures(numframes, shadows);

            extern SDL_Surface *screen;
            int aasize = 1<<(dynshadowsize + aadynshadow);
            while(aasize > screen->w || aasize > screen->h) aasize /= 2;

            stream *f = filename ? opengzfile(filename, "wb") : NULL;
            if(f)
            {
                shadowheader hdr;
                hdr.size = min(aasize, 1<<dynshadowsize);
                hdr.frames = numframes;
                hdr.height = height;
                hdr.rad = rad;
                f->putlil(hdr.size);
                f->putlil(hdr.frames);
                f->putlil(hdr.height);
                f->putlil(hdr.rad);
            }

            glViewport(0, 0, aasize, aasize);
            glClearColor(0, 0, 0, 1);
            glDisable(GL_FOG);
            glColor3f(1, 1, 1);

            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(-rad, rad, -rad, rad, 0.15f, height);

            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glScalef(1, -1, 1);

            glTranslatef(0, 0, -height);
            begingenshadow();
            loopi(numframes) genshadow(aasize, i, f);
            endgenshadow();

            glEnable(GL_FOG);
            glViewport(0, 0, screen->w, screen->h);

            if(f) delete f;
        }

        bool loadshadows(const char *filename)
        {
            stream *f = opengzfile(filename, "rb");
            if(!f) return false;
            shadowheader hdr;
            if(f->read(&hdr, sizeof(shadowheader))!=sizeof(shadowheader)) { delete f; return false; }
            lilswap(&hdr.size, 1);
            lilswap(&hdr.frames, 1);
            if(hdr.size!=(1<<dynshadowsize) || hdr.frames!=numframes) { delete f; return false; }
            lilswap(&hdr.height, 1);
            lilswap(&hdr.rad, 1);

            uchar *buf = new uchar[hdr.size*hdr.size*hdr.frames];
            if(f->read(buf, hdr.size*hdr.size*hdr.frames)!=hdr.size*hdr.size*hdr.frames) { delete f; delete[] buf; return false; }

            shadowrad = hdr.rad;
            shadows = new GLuint[hdr.frames];
            glGenTextures(hdr.frames, shadows);

            loopi(hdr.frames) createtexture(shadows[i], hdr.size, hdr.size, &buf[i*hdr.size*hdr.size], 3, true, false, GL_ALPHA);

            delete[] buf;
            delete f;

            return true;
        }

        void rendershadow(int anim, int varseed, float speed, int basetime, const vec &o, float yaw)
        {
            if(!shadows) return;
            animstate as;
            if(!calcanimstate(anim, varseed, speed, basetime, NULL, as)) return;
            anpos cur;
            cur.setframes(as);

            GLuint id = shadows[cur.fr1];
            if(id!=lasttex)
            {
                glBindTexture(GL_TEXTURE_2D, id);
                lasttex = id;
            }

            if(enabledepthmask) { glDepthMask(GL_FALSE); enabledepthmask = false; }
            if(enablealphatest) { glDisable(GL_ALPHA_TEST); enablealphatest = false; }
            if(!enablealphablend)
            {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                enablealphablend = true;
            }

            yaw *= RAD;
            float c = cosf(yaw), s = sinf(yaw);
            float x1 = -shadowrad, x2 = shadowrad;
            float y1 = -shadowrad, y2 = shadowrad;

            if(dynshadowquad)
            {
                glBegin(GL_QUADS);
                glTexCoord2f(0, 1); glVertex3f(x1*c - y1*s + o.x, y1*c + x1*s + o.y, o.z);
                glTexCoord2f(1, 1); glVertex3f(x2*c - y1*s + o.x, y1*c + x2*s + o.y, o.z);
                glTexCoord2f(1, 0); glVertex3f(x2*c - y2*s + o.x, y2*c + x2*s + o.y, o.z);
                glTexCoord2f(0, 0); glVertex3f(x1*c - y2*s + o.x, y2*c + x1*s + o.y, o.z);
                glEnd();
                xtraverts += 4;
                return;
            }

            if(!enableoffset)
            {
                enablepolygonoffset(GL_POLYGON_OFFSET_FILL);
                enableoffset = true;
            }
            if(lastvertexarray)
            {
                glDisableClientState(GL_VERTEX_ARRAY);
                lastvertexarray = NULL;
            }
            if(lasttexcoordarray)
            {
                glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                lasttexcoordarray = NULL;
            }

            vec texgenS, texgenT;
            texgenS.x = c / (2*shadowrad);
            texgenS.y = s / (2*shadowrad);
            texgenS.z = -(x1*c - y2*s + o.x)*texgenS.x - (y2*c + x1*s + o.y)*texgenS.y;

            texgenT.x = s / (2*shadowrad);
            texgenT.y = -c / (2*shadowrad);
            texgenT.z = -(x1*c - y2*s + o.x)*texgenT.x - (y2*c + x1*s + o.y)*texgenT.y;

            ::rendershadow(int(floor(o.x-shadowrad)), int(floor(o.y-shadowrad)), int(ceil(o.x+shadowrad)), int(ceil(o.y+shadowrad)), texgenS, texgenT);
        }

        char *shadowfile()
        {
            if(!saveshadows || !filename) return NULL;

            static string s;
            char *dir = strrchr(filename, PATHDIV);
            if(!dir) s[0] = '\0';
            else copystring(s, filename, dir-filename+2);
            concatstring(s, "shadows.dat");
            return s;
        }

        float calcradius()
        {
            float rad = 0;
            loopv(meshes) rad = max(rad, meshes[i]->calcradius());
            return rad;
        }

        void calcneighbors()
        {
            loopv(meshes) meshes[i]->calcneighbors();
        }

        void calcbbs()
        {
            loopv(meshes) meshes[i]->calcbbs();
        }
    };

    bool loaded;
    char *loadname;
    vector<part *> parts;

    vertmodel(const char *name) : loaded(false)
    {
        loadname = newstring(name);
    }

    ~vertmodel()
    {
        delete[] loadname;
        parts.deletecontents();
    }

    char *name() { return loadname; }

    void cleanup()
    {
        loopv(parts) parts[i]->cleanup();
    }

    bool link(part *link, const char *tag, vec *pos = NULL)
    {
        loopv(parts) if(parts[i]->link(link, tag, pos)) return true;
        return false;
    }

    void setskin(int tex = 0)
    {
        //if(parts.length()!=1 || parts[0]->meshes.length()!=1) return;
        if(parts.length() < 1 || parts[0]->meshes.length() < 1) return;
        mesh &m = *parts[0]->meshes[0];
        m.tex = tex;
    }

    void genshadows(float height, float rad)
    {
        if(parts.length()>1) return;
        parts[0]->genshadows(height, rad);
    }

    bool hasshadows()
    {
        return parts.length()==1 && parts[0]->shadows;
    }

    float calcradius()
    {
        return parts.empty() ? 0.0f : parts[0]->calcradius();
    }

    void calcneighbors()
    {
        loopv(parts) parts[i]->calcneighbors();
    }

    void calcbbs()
    {
        loopv(parts) parts[i]->calcbbs();
    }

    static bool enablealphablend, enablealphatest, enabledepthmask, enableoffset;
    static GLuint lasttex;
    static float lastalphatest;
    static void *lastvertexarray, *lasttexcoordarray, *lastcolorarray;
    static glmatrixf matrixstack[32];
    static int matrixpos;

    void startrender()
    {
        enablealphablend = enablealphatest = enableoffset = false;
        enabledepthmask = true;
        lasttex = 0;
        lastalphatest = -1;
        lastvertexarray = lasttexcoordarray = lastcolorarray = NULL;
    }

    void endrender()
    {
        if(enablealphablend) glDisable(GL_BLEND);
        if(enablealphatest) glDisable(GL_ALPHA_TEST);
        if(!enabledepthmask) glDepthMask(GL_TRUE);
        if(lastvertexarray) glDisableClientState(GL_VERTEX_ARRAY);
        if(lasttexcoordarray) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        if(lastcolorarray) glDisableClientState(GL_COLOR_ARRAY);
        if(enableoffset) disablepolygonoffset(GL_POLYGON_OFFSET_FILL);
    }

    static modelcache dynalloc, statalloc;
};

bool vertmodel::enablealphablend = false, vertmodel::enablealphatest = false, vertmodel::enabledepthmask = true, vertmodel::enableoffset = false;
GLuint vertmodel::lasttex = 0;
float vertmodel::lastalphatest = -1;
void *vertmodel::lastvertexarray = NULL, *vertmodel::lasttexcoordarray = NULL, *vertmodel::lastcolorarray = NULL;
glmatrixf vertmodel::matrixstack[32];
int vertmodel::matrixpos = 0;

VARF(mdldyncache, 1, 2, 32, vertmodel::dynalloc.resize(mdldyncache<<20));
VARF(mdlstatcache, 1, 1, 32, vertmodel::statalloc.resize(mdlstatcache<<20));

modelcache vertmodel::dynalloc(mdldyncache<<20), vertmodel::statalloc(mdlstatcache<<20);

