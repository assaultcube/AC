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
            };
            if(as.anim&ANIM_REVERSE)
            {
                fr1 = (as.frame+as.range-1)-(fr1-as.frame);
                fr2 = (as.frame+as.range-1)-(fr2-as.frame);
            };
        };
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
        int dynframe, dynlen;
        GLuint statlist;
        int statlen;

        mesh() : name(0), verts(0), tcverts(0), tris(0), skin(crosshair), tex(0), dynbuf(0), dynidx(0), dynframe(-1), statlist(0) {};

        ~mesh()
        {
            DELETEA(name);
            DELETEA(verts);
            DELETEA(tcverts);
            DELETEA(tris);
            if(statlist) glDeleteLists(statlist, 1);
            DELETEA(dynidx);
            DELETEA(dynbuf);
        };

        void gendyn()
        {
            tristrip ts;
            ts.addtriangles(tris->vert, numtris);
            vector<ushort> idxs;
            ts.buildstrips(idxs);
            dynbuf = new vec[numverts];
            dynidx = new ushort[idxs.length()];
            memcpy(dynidx, idxs.getbuf(), idxs.length()*sizeof(ushort));
            dynlen = idxs.length();
        };

        void gendynverts(anpos &cur, anpos *prev, float ai_t)
        {
            vec *vert1 = &verts[cur.fr1 * numverts],
                *vert2 = &verts[cur.fr2 * numverts],
                *pvert1 = NULL, *pvert2 = NULL;
            if(prev)
            {
                pvert1 = &verts[prev->fr1 * numverts];
                pvert2 = &verts[prev->fr2 * numverts];
                dynframe = -1;
            }
            else if(cur.fr1==cur.fr2)
            {
                if(cur.fr1==dynframe) return;
                dynframe = cur.fr1;
            }
            else dynframe = -1;
            loopi(numverts) // vertices
            {
                vec &v = dynbuf[i];
                #define ip(p1, p2, t) (p1+t*(p2-p1))
                #define ip_v(p, c, t) ip(p##1[i].c, p##2[i].c, t)
                if(prev)
                {
                    #define ip_v_ai(c) ip( ip_v(pvert, c, prev->t), ip_v(vert, c, cur.t), ai_t)
                    v = vec(ip_v_ai(x), ip_v_ai(y), ip_v_ai(z));
                    #undef ip_v_ai
                }
                else
                {
                    v = vec(ip_v(vert, x, cur.t), ip_v(vert, y, cur.t), ip_v(vert, z, cur.t));
                };
                #undef ip
                #undef ip_v
            };
        };

        void render(animstate &as, anpos &cur, anpos *prev, float ai_t)
        {
            if(!dynbuf) return;

            int id = skin->id;
            if(tex)
            {
                int xs, ys;
                id = lookuptexture(tex, xs, ys);
            };
            glBindTexture(GL_TEXTURE_2D, id);
          
            if(as.anim&ANIM_MIRROR)
            {
                glPushMatrix();
                glScalef(-1, 1, 1);
            };
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
                    };
                    tcvert &tc = tcverts[index];
                    glTexCoord2f(tc.u, tc.v);
                    glVertex3fv(&dynbuf[tc.index].x);
                };
                glEnd();
                if(isstat)
                {
                    glEndList();
                    glCallList(statlist);
                    statlen = dynlen;
                };
                xtraverts += dynlen;
            };
            if(as.anim&ANIM_MIRROR) glPopMatrix();
        };                     
    };

    struct animinfo
    {
        int frame, range;
        float speed;
    };

    struct tag
    {
        char *name;
        vec pos;
        float transform[3][3];
        
        tag() : name(NULL) {};
        ~tag() { DELETEA(name); };
    };

    struct part
    {
        bool loaded;
        vertmodel *model;
        int index, numframes;
        vector<mesh *> meshes;
        vector<animinfo> *anims;
        part **links;
        tag *tags;
        int numtags;

        part() : loaded(false), anims(NULL), links(NULL), tags(NULL), numtags(0) {};
        virtual ~part()
        {
            meshes.deletecontentsp();
            DELETEA(anims);
            DELETEA(links);
            DELETEA(tags);
        };

        bool link(part *link, const char *tag)
        {
            loopi(numtags) if(!strcmp(tags[i].name, tag))
            {
                links[i] = link;
                return true;
            };
            return false;
        };

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
               };
           };
           loopi(numframes*numtags)
           {
               vec &v = tags[i].pos;
               if(!index) v.add(translate);
               v.mul(scale);
           };
        };

        void gendyn()
        {
            loopv(meshes) meshes[i]->gendyn();
        };
            
        virtual void getdefaultanim(animstate &as, int anim, int varseed, float speed)
        {
            as.frame = 0;
            as.range = 1;
            as.speed = speed;
        };

        bool calcanimstate(int anim, int varseed, float speed, int basetime, dynent *d, animstate &as)
        {
            as.anim = anim;
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
                    animinfo &ai = ais[varseed%ais.length()];
                    as.frame = ai.frame;
                    as.range = ai.range;
                    as.speed = speed*100.0f/ai.speed;
                }
                else
                {
                    as.frame = 0;
                    as.range = 1;
                    as.speed = speed;
                };
            }
            else getdefaultanim(as, anim&ANIM_INDEX, varseed, speed);
            if(anim&(ANIM_START|ANIM_END))
            {
                if(anim&ANIM_END) as.frame += as.range-1;
                as.range = 1; 
            };

            if(as.frame+as.range>numframes)
            {
                if(as.frame>=numframes) return false;
                as.range = numframes-as.frame;
            };

            if(d && index<2)
            {
                if(d->lastmodel[index]!=this || d->lastanimswitchtime[index]==-1)
                {
                    d->current[index] = as;
                    d->lastanimswitchtime[index] = lastmillis-animationinterpolationtime*2;
                }
                else if(d->current[index] != as)
                {
                    if(lastmillis-d->lastanimswitchtime[index]>animationinterpolationtime/2) d->prev[index] = d->current[index];
                    d->current[index] = as;
                    d->lastanimswitchtime[index] = lastmillis;
                };
                d->lastmodel[index] = this;
            };
            return true;
        };
        
        void render(int anim, int varseed, float speed, int basetime, dynent *d)
        {
            if(meshes.length() <= 0) return;
            animstate as;
            if(!calcanimstate(anim, varseed, speed, basetime, d, as)) return;
    
            if(!meshes[0]->dynbuf) gendyn();
    
            anpos prev, cur;
            cur.setframes(d && index<2 ? d->current[index] : as);
    
            float ai_t = 0;
            bool doai = !(anim&ANIM_NOINTERP) && d && index<2 && lastmillis-d->lastanimswitchtime[index]<animationinterpolationtime;
            if(doai)
            {
                prev.setframes(d->prev[index]);
                ai_t = (lastmillis-d->lastanimswitchtime[index])/(float)animationinterpolationtime;
            };
            
            loopv(meshes) meshes[i]->render(as, cur, doai ? &prev : NULL, ai_t);

            loopi(numtags) if(links[i]) // render the linked models - interpolate rotation and position of the 'link-tags'
            {
                part *link = links[i];

                GLfloat matrix[16];
                tag *tag1 = &tags[cur.fr1*numtags+i];
                tag *tag2 = &tags[cur.fr2*numtags+i];
                #define ip(p1, p2, t) (p1+t*(p2-p1))
                #define ip_ai_tag(c) ip( ip( tag1p->c, tag2p->c, prev.t), ip( tag1->c, tag2->c, cur.t), ai_t)
                if(doai)
                {
                    tag *tag1p = &tags[prev.fr1 * numtags + i];
                    tag *tag2p = &tags[prev.fr2 * numtags + i];
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
                };
                #undef ip_ai_tag
                #undef ip 
                matrix[3] = matrix[7] = matrix[11] = 0.0f;
                matrix[15] = 1.0f;
                glPushMatrix();
                    glMultMatrixf(matrix);
                    link->render(anim, varseed, speed, basetime, d);
                glPopMatrix();
            };
        };

        void setanim(int num, int frame, int range, float speed)
        {
            if(frame<0 || frame>=numframes || range<=0 || frame+range>numframes) 
            { 
                conoutf("invalid frame %d, range %d in model %s", frame, range, model->loadname); 
                return; 
            };
            if(!anims) anims = new vector<animinfo>[NUMANIMS];
            animinfo &ai = anims[num].add();
            ai.frame = frame;
            ai.range = range;
            ai.speed = speed;
        };
    };

    bool loaded;
    char *loadname;
    vector<part *> parts;

    vertmodel(const char *name) : loaded(false)
    {
        loadname = newstring(name);
    };

    ~vertmodel()
    {
        delete[] loadname;
        parts.deletecontentsp();
    };

    char *name() { return loadname; };

    bool link(part *link, const char *tag)
    {
        loopv(parts) if(parts[i]->link(link, tag)) return true;
        return false;
    };

    void setskin(int tex)
    {
        if(parts.length()!=1 || parts[0]->meshes.length()!=1) return;
        mesh &m = *parts[0]->meshes[0]; 
        m.tex = tex;
    };
};

