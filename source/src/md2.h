struct md2;

md2 *loadingmd2 = 0;

static inline bool htcmp(const vertmodel::tcvert &x, const vertmodel::tcvert &y)
{
    return x.u==y.u && x.v==y.v && x.index==y.index;
};

static inline uint hthash(const vertmodel::tcvert &x)
{
    return x.index;
};

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
    
    md2(const char *name) : vertmodel(name) {};

    int type() { return MDL_MD2; };

    struct md2part : part
    {
        void gentcverts(int *glcommands, vector<tcvert> &tcverts, vector<tri> &tris)
        {
            hashtable<tcvert, int> tchash;
            vector<ushort> idxs;
            for(int *command = glcommands; (*command)!=0;)
            {
                int numvertex = *command++;
                bool isfan;
                if(isfan = (numvertex<0)) numvertex = -numvertex;
                idxs.setsizenodelete(0);
                loopi(numvertex)
                {
                    union { int i; float f; } u, v;
                    u.i = *command++;
                    v.i = *command++;
                    tcvert tckey = { u.f, v.f, (ushort)*command++ };
                    int *idx = tchash.access(tckey);
                    if(!idx)
                    {
                        idx = &tchash[tckey];
                        *idx = tcverts.length();
                        tcverts.add(tckey);
                    };        
                    idxs.add(*idx);
                };
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
                };
            };
        };
        
        bool load(char *filename)
        {
            if(loaded) return true;
            FILE *file = fopen(filename, "rb");
            if(!file) return false;

            md2_header header;
            fread(&header, sizeof(md2_header), 1, file);
            endianswap(&header, sizeof(int), sizeof(md2_header)/sizeof(int));

            if(header.magic!=844121161 || header.version!=8) 
            {
                fclose(file);
                return false;
            };
           
            numframes = header.numframes;

            mesh &m = *new mesh;
            meshes.add(&m);

            int *glcommands = new int[header.numglcommands];
            fseek(file, header.offsetglcommands, SEEK_SET);
            fread(glcommands, header.numglcommands*sizeof(int), 1, file);
            endianswap(glcommands, sizeof(int), header.numglcommands);

            vector<tcvert> tcgen;
            vector<tri> trigen;
            gentcverts(glcommands, tcgen, trigen);
            delete[] glcommands;

            m.numtcverts = tcgen.length();
            m.tcverts = new tcvert[m.numtcverts];
            memcpy(m.tcverts, tcgen.getbuf(), m.numtcverts*sizeof(tcvert));
            m.numtris = trigen.length();
            m.tris = new tri[m.numtris];
            memcpy(m.tris, trigen.getbuf(), m.numtris*sizeof(tri));

            m.numverts = header.numvertices;
            m.verts = new vec[m.numverts*numframes];

            int frame_offset = header.offsetframes;
            vec *curvert = m.verts;
            loopi(header.numframes)
            {
                md2_frame frame;
                fseek(file, frame_offset, SEEK_SET);
                fread(&frame, sizeof(md2_frame), 1, file);
                endianswap(&frame, sizeof(float), 6);
                loopj(header.numvertices)
                {
                    md2_vertex v;
                    fread(&v, sizeof(md2_vertex), 1, file);
                    *curvert++ = vec(v.vertex[0]*frame.scale[0]+frame.translate[0],
                                     v.vertex[2]*frame.scale[2]+frame.translate[2],
                                     -(v.vertex[1]*frame.scale[1]+frame.translate[1]));
                };
                frame_offset += header.framesize;
            };
                 
            fclose(file);
           
            return loaded = true;
        };

        void getdefaultanim(animstate &as, int anim, int varseed, float speed)
        {
            //                      0   1   2   3   4   5   6   7   8   9  10  11  12   13  14  15  16  17 18  19  20   21  21  23  24     
            //                      I   R   A   P   P   P   J   L   F   S   T   W   P   CI  CW  CA  CP  CD  D   D   D   LD  LD  LD   F
            static int frame[] =  { 0,  40, 46, 54, 58, 62, 66, 69, 72, 84, 95, 112,123,135,154,160,169,173,178,184,190,183,189,197, 0 };
            static int range[] =  { 40, 5,  8,  4,  4,  4,  3,  3,  12, 11, 17, 11, 13, 19, 6,  9,  4,  5,  6,  6,  8,  1,  1,  1,   7 };
            static int animfr[] = { 0, 1, 2, 3, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21, 24 };

            as.speed = speed;
            if((size_t)anim >= sizeof(animfr)/sizeof(animfr[0]))
            {
                as.frame = 0;
                as.range = 1;
                return;
            };
            int n = animfr[anim];
            if(anim==ANIM_PAIN || anim==ANIM_DEATH || anim==ANIM_LYING_DEAD) n += varseed%3;
            as.frame = frame[n];
            as.range = range[n];
        };
    };

    void render(int anim, int varseed, float speed, int basetime, float x, float y, float z, float yaw, float pitch, dynent *d, model *vwepmdl, float scale)
    {
        if(!loaded) return;

        glPushMatrix();
        glTranslatef(x, y, z);
        glRotatef(yaw+180, 0, -1, 0);
        glRotatef(pitch, 0, 0, 1);
        if(scale!=1) glScalef(scale, scale, scale);
        parts[0]->render(anim, varseed, speed, basetime, d);
        glPopMatrix();

        if(vwepmdl)
        {
            ((md2 *)vwepmdl)->parts[0]->index = parts.length();
            vwepmdl->setskin();
            vwepmdl->render(anim, varseed, speed, basetime, x, y, z, yaw, pitch, d, NULL, scale);
        };
    };

    bool load()
    { 
        if(loaded) return true;
        md2part &mdl = *new md2part;
        parts.add(&mdl);
        mdl.model = this;
        mdl.index = 0;
        char *pname = parentdir(loadname);
        s_sprintfd(name1)("packages/models/%s/tris.md2", loadname);
        if(!mdl.load(path(name1)))
        {
            s_sprintf(name1)("packages/models/%s/tris.md2", pname);    // try md2 in parent folder (vert sharing)
            if(!mdl.load(path(name1))) { delete[] pname; return false; };
        };
        Texture *skin;
        loadskin(loadname, pname, skin, this);
        loopv(mdl.meshes) mdl.meshes[i]->skin  = skin;
        if(skin==crosshair) conoutf("could not load model skin for %s", name1);
        loadingmd2 = this;
        s_sprintfd(name3)("packages/models/%s/md2.cfg", loadname);
        if(!execfile(name3))
        {
            s_sprintf(name3)("packages/models/%s/md2.cfg", pname);
            execfile(name3);
        };
        delete[] pname;
        loadingmd2 = 0;
        loopv(parts) parts[i]->scaleverts(scale/16.0f, vec(translate.x, translate.z, -translate.y));
        return loaded = true;
    };
};

void md2anim(char *anim, char *frame, char *range, char *s)
{
    if(!loadingmd2 || loadingmd2->parts.empty()) { conoutf("not loading an md2"); return; };
    int num = findanim(anim);
    if(num<0) { conoutf("could not find animation %s", anim); return; };
    float speed = s[0] ? atof(s) : 100.0f;
    loadingmd2->parts.last()->setanim(num, atoi(frame), atoi(range), speed);
};

COMMAND(md2anim, ARG_4STR);

