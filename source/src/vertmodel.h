VARP(dynshadowsize, 4, 5, 8);
VARP(aadynshadow, 0, 2, 3);
VARP(saveshadows, 0, 1, 1);

VAR(shadowyaw, 0, 45, 360);
vec shadowdir(0, 0, -1), shadowpos(0, 0, 0);

VAR(dbgstenc, 0, 0, 1);

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

    struct tcvert { float u, v; };
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

    struct mesh
    {
        char *name;
        part *owner;
        vec *verts;
        tcvert *tcverts;
        ushort *shareverts;
        tri *tris;
        int numverts, numtris;

        Texture *skin;
        int tex;

        modelcachelist<dyncacheentry> dyncache;
        modelcachelist<shadowcacheentry> shadowcache;

        ushort *dynidx;
        int dynlen;
        drawcall *dyndraws;
        int numdyndraws;
        GLuint statlist;
        int statlen;

        mesh() : name(0), owner(0), verts(0), tcverts(0), shareverts(0), tris(0), skin(notexture), tex(0), dynidx(0), dyndraws(0), statlist(0) 
        {
        }

        ~mesh()
        {
            DELETEA(name);
            DELETEA(verts);
            DELETEA(tcverts);
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
                    uint e1 = shareverts[t.vert[j]], e2 = shareverts[t.vert[(j+1)%3]];
                    uint &edge = edges.access(min(e1, e2) | (max(e1, e2)<<16), ~0U);
                    if((edge&0xFFFF)==0xFFFF)
                    {
                        edge &= ~0xFFFF;
                        edge |= i;
                    }
                    else if((edge&~0xFFFFU)==~0xFFFFU)
                    {
                        edge &= 0xFFFF;
                        edge |= i<<16;
                    }
                    else edge = 0;
                }
            }
            loopi(numtris)
            {
                tri &t = tris[i];
                loopj(3)
                {
                    uint e1 = shareverts[t.vert[j]], e2 = shareverts[t.vert[(j+1)%3]];
                    uint edge = edges[min(e1, e2) | (max(e1, e2)<<16)];
                    if(!edge) t.neighbor[j] = 0xFFFF;
                    else t.neighbor[j] = int(edge&0xFFFF)==i ? edge>>16 : edge&0xFFFF;
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
            side.setsizenodelete(0);

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
                        if(!side[i]) swap(e1, e2);
                        *idx++ = e2;
                        *idx++ = e1;
                        *idx++ = numverts;
                    }
                }
            }

            if(dbgstenc && stenciling==1) conoutf("%s: %d tris\n", owner->filename, (idx - d->idxs())/3);

            d->size = (uchar *)idx - (uchar *)d;
            return d;
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

            bool isstat = as.frame==0 && as.range==1;
            if(isstat && statlist && !stenciling)
            {
                glCallList(statlist);
                xtraverts += statlen;
            }
            else
            {
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
                if(as.anim&ANIM_NOSKIN && (!isstat || stenciling))
                {
                    if(lasttexcoordarray)
                    {
                        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                        lasttexcoordarray = NULL;
                    }
                }
                else if(lasttexcoordarray != tcverts)
                {
                    if(!lasttexcoordarray) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                    glTexCoordPointer(2, GL_FLOAT, sizeof(tcvert), tcverts);
                    lasttexcoordarray = tcverts;
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

                if(isstat) glNewList(statlist = glGenLists(1), GL_COMPILE);
                loopi(numdyndraws)
                {
                    const drawcall &d = dyndraws[i];
                    if(hasDRE) glDrawRangeElements_(d.type, d.minvert, d.maxvert, d.count, GL_UNSIGNED_SHORT, &dynidx[d.start]);
                    else glDrawElements(d.type, d.count, GL_UNSIGNED_SHORT, &dynidx[d.start]);
                }
                if(isstat)
                {
                    glEndList();
                    glCallList(statlist);
                    statlen = dynlen;
                }
                xtraverts += dynlen;
                return;
            }
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

    struct part
    {
        char *filename;
        vertmodel *model;
        int index, numframes;
        vector<mesh *> meshes;
        vector<animinfo> *anims;
        part **links;
        tag *tags;
        int numtags;
        particleemitter *emitters;
        GLuint *shadows;
        float shadowrad;

        part() : filename(NULL), anims(NULL), links(NULL), tags(NULL), numtags(0), emitters(NULL), shadows(NULL), shadowrad(0) {}
        virtual ~part()
        {
            DELETEA(filename);
            meshes.deletecontentsp();
            DELETEA(anims);
            DELETEA(links);
            DELETEA(tags);
            DELETEA(emitters);
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

        bool link(part *link, const char *tag)
        {
            loopi(numtags) if(!strcmp(tags[i].name, tag))
            {
                links[i] = link;
                return true;
            }
            return false;
        }

        bool gentag(const char *name, int vert, mesh *m = NULL)
        {
            if(!m)
            {
                if(meshes.empty()) return false;
                m = meshes[0];
            }
            if(vert < 0 || vert > m->numverts) return false;

            tag *ntags = new tag[(numtags + 1)*numframes];
            ntags[numtags].name = newstring(name); 
            loopi(numframes)
            {
                memcpy(&ntags[(numtags + 1)*i], &tags[numtags*i], numtags*sizeof(tag));

                tag *t = &ntags[(numtags + 1)*i + numtags];
                t->pos = m->verts[m->numverts*i + vert];
                t->identity();
            }
            loopi(numtags) tags[i].name = NULL;

            DELETEA(tags);
            tags = ntags;
            numtags++;

            DELETEA(links);
            links = new part *[numtags];
            loopi(numtags) links[i] = NULL;
            return true;
        }

        bool addemitter(const char *tag, int type, int arg1 = 0, int arg2 = 0)
        {
            loopi(numtags) if(!strcmp(tags[i].name, tag))
            {
                if(!emitters) emitters = new particleemitter[numtags];
                particleemitter &p = emitters[i];
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
       
        void calcnormal(GLfloat *m, vec &dir)
        {
            vec n(dir);
            dir.x = n.x*m[0] + n.y*m[1] + n.z*m[2];
            dir.y = n.x*m[4] + n.y*m[5] + n.z*m[6];
            dir.z = n.x*m[8] + n.y*m[9] + n.z*m[10];
        }

        void calcvertex(GLfloat *m, vec &pos)
        {
            vec p(pos);

            p.x -= m[12];
            p.y -= m[13];
            p.z -= m[14];

            pos.x = p.x*m[0] + p.y*m[1] + p.z*m[2];
            pos.y = p.x*m[4] + p.y*m[5] + p.z*m[6];
            pos.z = p.x*m[8] + p.y*m[9] + p.z*m[10];
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
            
            loopv(meshes) meshes[i]->render(as, cur, doai ? &prev : NULL, ai_t);

            loopi(numtags) if(links[i] || (anim&ANIM_PARTICLE && emitters && emitters[i].type>=0)) // render the linked models - interpolate rotation and position of the 'link-tags'
            {
                GLfloat matrix[16];
                gentagmatrix(cur, doai ? &prev : NULL, ai_t, i, matrix);

                part *link = links[i];
                if(link)
                {
                    vec oldshadowdir, oldshadowpos;
                    
                    if(stenciling)
                    {
                        oldshadowdir = shadowdir;
                        oldshadowpos = shadowpos;
                        calcnormal(matrix, shadowdir);
                        calcvertex(matrix, shadowpos);
                    }

                    glPushMatrix();
                        glMultMatrixf(matrix);
                        link->render(anim, varseed, speed, basetime, d);
                    glPopMatrix();

                    if(stenciling)
                    {
                        shadowdir = oldshadowdir;
                        shadowpos = oldshadowpos;
                    }
                }

                if(anim&ANIM_PARTICLE && emitters && emitters[i].type>=0)
                {
                    if(emitters[i].lastemit!=basetime)
                    {
                        emitters[i].seed = rnd(0x1000000);
                        emitters[i].lastemit = basetime;
                    }

                    vec tagpos(matrix[12], matrix[13], matrix[14]);
                    GLdouble mm[16];
                    glGetDoublev(GL_MODELVIEW_MATRIX, mm);
                    vec eyepos;
                    loopk(3) eyepos[k] = tagpos.x*mm[0+k] + tagpos.y*mm[4+k] + tagpos.z*mm[8+k] + mm[12+k];
                    vec worldpos;
                    loopk(3) worldpos[k] = eyepos.x*invmvmatrix[0+k] + eyepos.y*invmvmatrix[4+k] + eyepos.z*invmvmatrix[8+k] + invmvmatrix[12+k];
                    particle_emit(emitters[i].type, emitters[i].args, basetime, emitters[i].seed, worldpos);
                }
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

        void genshadow(int aasize, int frame, gzFile f)
        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            model->startrender();
            render(ANIM_ALL|ANIM_NOINTERP|ANIM_NOSKIN, 0, 1, lastmillis-frame, NULL);
            model->endrender();

            uchar *pixels = new uchar[aasize*aasize];
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, aasize, aasize, GL_RED, GL_UNSIGNED_BYTE, pixels);
#if 0
            SDL_Surface *img = SDL_CreateRGBSurface(SDL_SWSURFACE, aasize, aasize, 24, 0x0000FF, 0x00FF00, 0xFF0000, 0);
            loopi(aasize*aasize) memset((uchar *)img->pixels + 3*i, pixels[i], 3);
            s_sprintfd(imgname)("%s_%d.bmp", model->loadname, frame);
            for(char *s; (s = strchr(imgname, '/'));) *s = '_';
            SDL_SaveBMP(img, imgname);
            SDL_FreeSurface(img);
#endif
            if(aasize > 1<<dynshadowsize) 
                gluScaleImage(GL_ALPHA, aasize, aasize, GL_UNSIGNED_BYTE, pixels, 1<<dynshadowsize, 1<<dynshadowsize, GL_UNSIGNED_BYTE, pixels);

            int texsize = min(aasize, 1<<dynshadowsize);
            if(f) gzwrite(f, pixels, texsize*texsize);
            createtexture(shadows[frame], texsize, texsize, pixels, 3, true, GL_ALPHA);
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

            gzFile f = filename ? opengzfile(filename, "wb9") : NULL;
            if(f)
            {
                shadowheader hdr;
                hdr.size = min(aasize, 1<<dynshadowsize);
                hdr.frames = numframes;
                hdr.height = height;
                hdr.rad = rad;
                endianswap(&hdr.size, sizeof(ushort), 1);
                endianswap(&hdr.frames, sizeof(ushort), 1);
                endianswap(&hdr.height, sizeof(float), 1);
                endianswap(&hdr.rad, sizeof(float), 1);
                gzwrite(f, &hdr, sizeof(shadowheader));
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

            if(f) gzclose(f);
        }

        bool loadshadows(const char *filename)
        {
            gzFile f = opengzfile(filename, "rb9");
            if(!f) return false;
            shadowheader hdr;
            if(gzread(f, &hdr, sizeof(shadowheader))!=sizeof(shadowheader)) { gzclose(f); return false; }
            endianswap(&hdr.size, sizeof(ushort), 1);
            endianswap(&hdr.frames, sizeof(ushort), 1);
            if(hdr.size!=(1<<dynshadowsize) || hdr.frames!=numframes) { gzclose(f); return false; }
            endianswap(&hdr.height, sizeof(float), 1);
            endianswap(&hdr.rad, sizeof(float), 1);

            uchar *buf = new uchar[hdr.size*hdr.size*hdr.frames];
            if(gzread(f, buf, hdr.size*hdr.size*hdr.frames)!=hdr.size*hdr.size*hdr.frames) { gzclose(f); return false; }

            shadowrad = hdr.rad;
            shadows = new GLuint[hdr.frames];
            glGenTextures(hdr.frames, shadows);

            loopi(hdr.frames) createtexture(shadows[i], hdr.size, hdr.size, &buf[i*hdr.size*hdr.size], 3, true, GL_ALPHA);
            
            delete[] buf;

            gzclose(f);

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

            glBegin(GL_POLYGON);
            glTexCoord2f(0, 1); glVertex3f(x1*c - y1*s + o.x, y1*c + x1*s + o.y, o.z);
            glTexCoord2f(1, 1); glVertex3f(x2*c - y1*s + o.x, y1*c + x2*s + o.y, o.z);
            glTexCoord2f(1, 0); glVertex3f(x2*c - y2*s + o.x, y2*c + x2*s + o.y, o.z);
            glTexCoord2f(0, 0); glVertex3f(x1*c - y2*s + o.x, y2*c + x1*s + o.y, o.z);
            glEnd();
        }           

        char *shadowfile()
        {
            if(!saveshadows || !filename) return NULL;

            static string s;
            char *dir = strrchr(filename, PATHDIV);
            if(!dir) s[0] = '\0';
            else s_strncpy(s, filename, dir-filename+2); 
            s_strcat(s, "shadows.dat");
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
        parts.deletecontentsp();
    }

    char *name() { return loadname; }

    void cleanup()
    {
        loopv(parts) parts[i]->cleanup();
    }

    bool link(part *link, const char *tag)
    {
        loopv(parts) if(parts[i]->link(link, tag)) return true;
        return false;
    }

    void setskin(int tex = 0)
    {
        if(parts.length()!=1 || parts[0]->meshes.length()!=1) return;
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

    static bool enablealphablend, enablealphatest, enabledepthmask;
    static GLuint lasttex;
    static float lastalphatest;
    static void *lastvertexarray, *lasttexcoordarray;

    void startrender()
    {
        enablealphablend = enablealphatest = false;
        enabledepthmask = true;
        lasttex = 0;
        lastalphatest = -1;
        lastvertexarray = lasttexcoordarray = NULL;
    }

    void endrender()
    {
        if(enablealphablend) glDisable(GL_BLEND);
        if(enablealphatest) glDisable(GL_ALPHA_TEST);
        if(!enabledepthmask) glDepthMask(GL_TRUE);
        if(lastvertexarray) glDisableClientState(GL_VERTEX_ARRAY);
        if(lasttexcoordarray) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    static modelcache dynalloc, statalloc;
};

bool vertmodel::enablealphablend = false, vertmodel::enablealphatest = false, vertmodel::enabledepthmask = true;
GLuint vertmodel::lasttex = 0;
float vertmodel::lastalphatest = -1;
void *vertmodel::lastvertexarray = NULL, *vertmodel::lasttexcoordarray = NULL;

VARF(mdldyncache, 1, 2, 32, vertmodel::dynalloc.resize(mdldyncache<<20));
VARF(mdlstatcache, 1, 1, 32, vertmodel::statalloc.resize(mdlstatcache<<20));

modelcache vertmodel::dynalloc(mdldyncache<<20), vertmodel::statalloc(mdlstatcache<<20);

