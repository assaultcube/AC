struct md2;

md2 *loadingmd2 = 0;

struct md2 : vertmodel
{
    struct md2_header
    {
        int magic;
        int version;
        int skinwidth, skinheight;
        int framesize;
        int numskins, numvertices, numtexcoords;
        int numtriangles, numglcommands, numframes;
        int offsetskins, offsettexcoords, offsettriangles;
        int offsetframes, offsetglcommands, offsetend;
    };

    struct md2_vertex
    {
        uchar vertex[3], normalindex;
    };

    struct md2_frame
    {
        float      scale[3];
        float      translate[3];
        char       name[16];
    };
    
    md2(const char *name) : vertmodel(name) {}

    int type() { return MDL_MD2; }

    struct md2part : part
    {
        void gentcverts(int *glcommands, vector<tcvert> &tcverts, vector<ushort> &vindexes, vector<tri> &tris)
        {
            hashtable<ivec, int> tchash;
            vector<ushort> idxs;
            for(int *command = glcommands; (*command)!=0;)
            {
                int numvertex = *command++;
                bool isfan = numvertex<0;
                if(isfan) numvertex = -numvertex;
                idxs.setsizenodelete(0);
                loopi(numvertex)
                {
                    union { int i; float f; } u, v;
                    u.i = *command++;
                    v.i = *command++;
                    int vindex = *command++;
                    ivec tckey(u.i, v.i, vindex);
                    int *idx = tchash.access(tckey);
                    if(!idx)
                    {
                        idx = &tchash[tckey];
                        *idx = tcverts.length();
                        tcvert &tc = tcverts.add();
                        tc.u = u.f;
                        tc.v = v.f;
                        vindexes.add((ushort)vindex);
                    }        
                    idxs.add(*idx);
                }
                loopi(numvertex-2) 
                { 
                    tri &t = tris.add();
                    if(isfan)
                    {
                        t.vert[0] = idxs[0];
                        t.vert[1] = idxs[i+1];
                        t.vert[2] = idxs[i+2];
                    }
                    else loopk(3) t.vert[k] = idxs[i&1 && k ? i+(1-(k-1))+1 : i+k];
                }
            }
        }
        
        bool load(char *path)
        {
            if(filename) return true;

            FILE *file = openfile(path, "rb");
            if(!file) return false;

            md2_header header;
            fread(&header, sizeof(md2_header), 1, file);
            endianswap(&header, sizeof(int), sizeof(md2_header)/sizeof(int));

            if(header.magic!=844121161 || header.version!=8) 
            {
                fclose(file);
                return false;
            }
           
            numframes = header.numframes;

            mesh &m = *new mesh;
            meshes.add(&m);
            m.owner = this;

            int *glcommands = new int[header.numglcommands];
            fseek(file, header.offsetglcommands, SEEK_SET);
            int numglcommands = (int)fread(glcommands, sizeof(int), header.numglcommands, file);
            endianswap(glcommands, sizeof(int), numglcommands);
            if(numglcommands < header.numglcommands) memset(&glcommands[numglcommands], 0, (header.numglcommands-numglcommands)*sizeof(int));

            vector<tcvert> tcgen;
            vector<ushort> vgen;
            vector<tri> trigen;
            gentcverts(glcommands, tcgen, vgen, trigen);
            delete[] glcommands;

            m.numverts = tcgen.length();
            m.tcverts = new tcvert[m.numverts];
            memcpy(m.tcverts, tcgen.getbuf(), m.numverts*sizeof(tcvert));
            m.numtris = trigen.length();
            m.tris = new tri[m.numtris];
            memcpy(m.tris, trigen.getbuf(), m.numtris*sizeof(tri));

            m.verts = new vec[m.numverts*numframes+1];

            md2_vertex *tmpverts = new md2_vertex[header.numvertices];
            int frame_offset = header.offsetframes;
            vec *curvert = m.verts;
            loopi(header.numframes)
            {
                md2_frame frame;
                fseek(file, frame_offset, SEEK_SET);
                fread(&frame, sizeof(md2_frame), 1, file);
                endianswap(&frame, sizeof(float), 6);

                fread(tmpverts, sizeof(md2_vertex), header.numvertices, file);
                loopj(m.numverts)
                {
                    const md2_vertex &v = tmpverts[vgen[j]];
                    *curvert++ = vec(v.vertex[0]*frame.scale[0]+frame.translate[0],
                                   -(v.vertex[1]*frame.scale[1]+frame.translate[1]),
                                     v.vertex[2]*frame.scale[2]+frame.translate[2]);
                }
                frame_offset += header.framesize;
            }
            delete[] tmpverts;

            fclose(file);
          
            filename = newstring(path);
            return true;
        }

        void getdefaultanim(animstate &as, int anim, int varseed)
        {
            //                      0   1   2   3   4   5   6   7   8   9  10  11  12   13  14  15  16  17 18  19  20   21  21  23  24     
            //                      I   R   A   P   P   P   J   L   F   S   T   W   P   CI  CW  CA  CP  CD  D   D   D   LD  LD  LD   F
            static int frame[] =  { 0,  40, 46, 54, 58, 62, 66, 69, 72, 84, 95, 112,123,135,154,160,169,173,178,184,190,183,189,197, 0 };
            static int range[] =  { 40, 6,  8,  4,  4,  4,  3,  3,  12, 11, 17, 11, 12, 19, 6,  9,  4,  5,  6,  6,  8,  1,  1,  1,   7 };
            static int animfr[] = { 0,  1,  2,  3,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 21, 24 };

            if((size_t)anim >= sizeof(animfr)/sizeof(animfr[0]))
            {
                as.frame = 0;
                as.range = 1;
                return;
            }
            int n = animfr[anim];
            if(anim==ANIM_PAIN || anim==ANIM_DEATH || anim==ANIM_LYING_DEAD) n += uint(varseed)%3;
            as.frame = frame[n];
            as.range = range[n];
        }

        void begingenshadow()
        {
            glRotatef(180, 0, 0, 1);
        }
    };

    void render(int anim, int varseed, float speed, int basetime, const vec &o, float yaw, float pitch, dynent *d, modelattach *a, float scale)
    {
        if(!loaded) return;

        if(!cullface) glDisable(GL_CULL_FACE);
        else if(anim&ANIM_MIRROR) glCullFace(GL_BACK);

        if(stenciling && !parts[0]->index)
        {
            shadowdir = vec(0, 1/SQRT2, -1/SQRT2);
            shadowdir.rotate_around_z((-shadowyaw-yaw-180.0f)*RAD);
            (shadowpos = shadowdir).mul(shadowdist);
        }

        glPushMatrix();
        glTranslatef(o.x, o.y, o.z);
        glRotatef(yaw+180, 0, 0, 1);
        glRotatef(pitch, 0, -1, 0);
        if(anim&ANIM_MIRROR || scale!=1) glScalef(scale, anim&ANIM_MIRROR ? -scale : scale, scale);
        parts[0]->render(anim, varseed, speed, basetime, d);
        glPopMatrix();

        if(!cullface) glEnable(GL_CULL_FACE);
        else if(anim&ANIM_MIRROR) glCullFace(GL_FRONT);

        if(a) for(int i = 0; a[i].name; i++)
        {
            vertmodel *m = (vertmodel *)a[i].m;
            if(!m) continue;
            m->parts[0]->index = parts.length()+i;
            m->setskin();
            m->render(anim, varseed, speed, basetime, o, yaw, pitch, d, NULL, scale);
        }

        if(d) d->lastrendered = lastmillis;
    }

    void rendershadow(int anim, int varseed, float speed, int basetime, const vec &o, float yaw, modelattach *a)
    {
        parts[0]->rendershadow(anim, varseed, speed, basetime, o, yaw);
        if(a) for(int i = 0; a[i].name; i++)
        {
            vertmodel *m = (vertmodel *)a[i].m;
            if(!m) continue;
            part *p = m->parts[0];
            p->rendershadow(anim, varseed, speed, basetime, o, yaw);
        }
    }

    bool load()
    { 
        if(loaded) return true;
        md2part &mdl = *new md2part;
        parts.add(&mdl);
        mdl.model = this;
        mdl.index = 0;
        const char *pname = parentdir(loadname);
        s_sprintfd(name1)("packages/models/%s/tris.md2", loadname);
        if(!mdl.load(path(name1)))
        {
            s_sprintf(name1)("packages/models/%s/tris.md2", pname);    // try md2 in parent folder (vert sharing)
            if(!mdl.load(path(name1))) return false;
        }
        Texture *skin;
        loadskin(loadname, pname, skin);
        loopv(mdl.meshes) mdl.meshes[i]->skin  = skin;
        if(skin==notexture) conoutf("could not load model skin for %s", name1);
        loadingmd2 = this;
        s_sprintfd(name2)("packages/models/%s/md2.cfg", loadname);
        if(!execfile(name2))
        {
            s_sprintf(name2)("packages/models/%s/md2.cfg", pname);
            execfile(name2);
        }
        loadingmd2 = 0;
        loopv(parts) parts[i]->scaleverts(scale/16.0f, vec(translate.x, -translate.y, translate.z));
        radius = calcradius();
        if(shadowdist) calcneighbors();
        return loaded = true;
    }
};

void md2anim(char *anim, char *frame, char *range, char *speed)
{
    if(!loadingmd2 || loadingmd2->parts.empty()) { conoutf("not loading an md2"); return; }
    int num = findanim(anim);
    if(num<0) { conoutf("could not find animation %s", anim); return; }
    loadingmd2->parts.last()->setanim(num, atoi(frame), atoi(range), atof(speed));
}

void md2tag(char *name, char *vert)
{
    if(!loadingmd2 || loadingmd2->parts.empty()) { conoutf("not loading an md2"); return; }
    md2::part &mdl = *loadingmd2->parts.last();
    int vnum = -1;
    if(isdigit(vert[0])) vnum = ATOI(vert);
    else
    {
        int axis = 0, dir = 1;
        for(char *s = vert; *s; s++) switch(*s)
        {
            case '+': dir = 1; break;
            case '-': dir = -1; break;
            case 'x':
            case 'X': axis = 0; break;
            case 'y':
            case 'Y': axis = 1; break;
            case 'z':
            case 'Z': axis = 2; break;
        }
        if(!mdl.meshes.empty()) vnum = mdl.meshes[0]->findvert(axis, dir);
        if(vnum < 0) { conoutf("could not find vertex %s", vert); return; }
    }
    if(!mdl.gentag(name, vnum)) { conoutf("could not find vertex %d", vnum); return; }
}

void md2emit(char *tag, char *type, char *arg1, char *arg2)
{
    if(!loadingmd2 || loadingmd2->parts.empty()) { conoutf("not loading an md2"); return; };
    md2::part &mdl = *loadingmd2->parts.last();
    if(!mdl.addemitter(tag, ATOI(type), ATOI(arg1), ATOI(arg2))) { conoutf("could not find tag %s", tag); return; }
}

COMMAND(md2anim, ARG_4STR);
COMMAND(md2tag, ARG_2STR);
COMMAND(md2emit, ARG_4STR);

