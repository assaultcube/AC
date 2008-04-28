VARP(dynshadowsize, 4, 5, 8);
VARP(aadynshadow, 0, 2, 3);
VARP(saveshadows, 0, 1, 1);

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
    };

    struct tcvert { float u, v; ushort index; };
    struct tri { ushort vert[3]; };

    struct mesh
    {
        char *name;
        vec *verts;
        tcvert *tcverts;
        tri *tris;
        int numverts, numtcverts, numtris;

        Texture *skin;
        int tex;

        vec *dynbuf;
        ushort *dynidx;
        int dynlen;
        anpos dyncur, dynprev;
        float dynt;
        GLuint statlist;
        int statlen;

        mesh() : name(0), verts(0), tcverts(0), tris(0), skin(notexture), tex(0), dynbuf(0), dynidx(0), statlist(0) 
        {
            dyncur.fr1 = dynprev.fr1 = -1;
        }

        ~mesh()
        {
            DELETEA(name);
            DELETEA(verts);
            DELETEA(tcverts);
            DELETEA(tris);
            if(statlist) glDeleteLists(statlist, 1);
            DELETEA(dynidx);
            DELETEA(dynbuf);
        }

        void genstrips()
        {
            tristrip ts;
            ts.addtriangles(tris->vert, numtris);
            vector<ushort> idxs;
            ts.buildstrips(idxs);
            dynbuf = new vec[numverts];
            dynidx = new ushort[idxs.length()];
            memcpy(dynidx, idxs.getbuf(), idxs.length()*sizeof(ushort));
            dynlen = idxs.length();
        }

        void gendynverts(anpos &cur, anpos *prev, float ai_t)
        {
            vec *vert1 = &verts[cur.fr1 * numverts],
                *vert2 = &verts[cur.fr2 * numverts],
                *pvert1 = NULL, *pvert2 = NULL;
            if(prev)
            {
                if(dynprev==*prev && dyncur==cur && dynt==ai_t) return;
                dynprev = *prev;
                dynt = ai_t;
                pvert1 = &verts[prev->fr1 * numverts];
                pvert2 = &verts[prev->fr2 * numverts];
            }
            else
            {
                if(dynprev.fr1<0 && dyncur==cur) return;
                dynprev.fr1 = -1;
            }
            dyncur = cur;
            #define iploop(body) \
                loopi(numverts) \
                { \
                    vec &v = dynbuf[i]; \
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
        }

        void cleanup()
        {
            if(statlist)
            {
                glDeleteLists(1, statlist);
                statlist = 0;
            }
        }

        void render(animstate &as, anpos &cur, anpos *prev, float ai_t)
        {
            if(!dynbuf) return;

            if(!(as.anim&ANIM_NOSKIN))
            {
                GLuint id = tex < 0 ? -tex : skin->id;
                if(tex > 0) id = lookuptexture(tex)->id;
                if(id!=lasttex)
                {
                    glBindTexture(GL_TEXTURE_2D, id);
                    lasttex = id;
                }
                if(enablealphablend) { glDisable(GL_BLEND); enablealphablend = false; }
                if(!enabledepthmask) { glDepthMask(GL_TRUE); enabledepthmask = true; }
            }

            bool isstat = as.frame==0 && as.range==1;
            if(isstat && statlist)
            {
                glCallList(statlist);
                xtraverts += statlen;
            }
            else
            {
                if(isstat) glNewList(statlist = glGenLists(1), GL_COMPILE);
                gendynverts(cur, prev, ai_t);
                loopj(dynlen)
                {
                    ushort index = dynidx[j];
                    if(index>=tristrip::RESTART || !j)
                    {
                        if(j) glEnd();
                        glBegin(index==tristrip::LIST ? GL_TRIANGLES : GL_TRIANGLE_STRIP);
                        if(index>=tristrip::RESTART) continue;
                    }
                    tcvert &tc = tcverts[index];
                    if(isstat || !(as.anim&ANIM_NOSKIN)) glTexCoord2f(tc.u, tc.v);
                    glVertex3fv(&dynbuf[tc.index].x);
                }
                glEnd();
                if(isstat)
                {
                    glEndList();
                    glCallList(statlist);
                    statlen = dynlen;
                }
                xtraverts += dynlen;
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
                    glPushMatrix();
                        glMultMatrixf(matrix);
                        link->render(anim, varseed, speed, basetime, d);
                    glPopMatrix();
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
                    extern GLdouble invmvmatrix[16];
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

    static bool enablealphablend, enabledepthmask;
    static GLuint lasttex;

    void startrender()
    {
        enablealphablend = false;
        enabledepthmask = true;
        lasttex = 0;
    }

    void endrender()
    {
        if(enablealphablend) glDisable(GL_BLEND);
        if(!enabledepthmask) glDepthMask(GL_TRUE);
    }
};

bool vertmodel::enablealphablend = false, vertmodel::enabledepthmask = true;
GLuint vertmodel::lasttex = 0;

