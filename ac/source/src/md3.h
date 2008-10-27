struct md3;

md3 *loadingmd3 = NULL;

string md3dir;

struct md3tag
{
    char name[64];
    vec pos;
    float rotation[3][3];
};

struct md3vertex
{
    short vertex[3];
    short normal;
};

struct md3triangle
{
    int vertexindices[3];
};

struct md3header
{
    char id[4];
    int version;
    char name[64];
    int flags;
    int numframes, numtags, nummeshes, numskins;
    int ofs_frames, ofs_tags, ofs_meshes, ofs_eof; // offsets
};

struct md3meshheader
{
    char id[4];
    char name[64];
    int flags;
    int numframes, numshaders, numvertices, numtriangles;
    int ofs_triangles, ofs_shaders, ofs_uv, ofs_vertices, meshsize; // offsets
};

struct md3 : vertmodel
{
    md3(const char *name) : vertmodel(name) {}

    int type() { return MDL_MD3; }

    struct md3part : part
    {
        bool load(char *path)
        {
            if(filename) return true;

            FILE *f = openfile(path, "rb");
            if(!f) return false;
            md3header header;
            fread(&header, sizeof(md3header), 1, f);
            endianswap(&header.version, sizeof(int), 1);
            endianswap(&header.flags, sizeof(int), 9);
            if(strncmp(header.id, "IDP3", 4) != 0 || header.version != 15) // header check
            { 
                fclose(f);
                conoutf("md3: corrupted header"); 
                return false; 
            }

            numframes = header.numframes;
            numtags = header.numtags;        
            if(numtags)
            {
                tags = new tag[numframes*numtags];
                fseek(f, header.ofs_tags, SEEK_SET);
                md3tag tag;
                
                loopi(header.numframes*header.numtags) 
                {
                    fread(&tag, sizeof(md3tag), 1, f);
                    endianswap(&tag.pos, sizeof(float), 12);
                    if(tag.name[0] && i<header.numtags) tags[i].name = newstring(tag.name);
                    tags[i].pos = vec(tag.pos.y, tag.pos.x, tag.pos.z);
                    memcpy(tags[i].transform, tag.rotation, sizeof(tag.rotation));
                    // undo the x/y swap
                    loopj(3) swap(tags[i].transform[0][j], tags[i].transform[1][j]);
                    // then restore it
                    loopj(3) swap(tags[i].transform[j][0], tags[i].transform[j][1]);
                }
                links = new linkedpart[numtags];
            }

            int mesh_offset = header.ofs_meshes;
            loopi(header.nummeshes)
            {
                md3meshheader mheader;
                fseek(f, mesh_offset, SEEK_SET);
                fread(&mheader, sizeof(md3meshheader), 1, f);
                endianswap(&mheader.flags, sizeof(int), 10);

                if(mheader.numtriangles <= 0)
                {
                    mesh_offset += mheader.meshsize;
                    continue;
                }
                
                mesh &m = *meshes.add(new mesh);
                m.owner = this;
                m.name = newstring(mheader.name);
               
                m.numtris = mheader.numtriangles; 
                m.tris = new tri[m.numtris];
                fseek(f, mesh_offset + mheader.ofs_triangles, SEEK_SET);
                loopj(mheader.numtriangles)
                {
                    md3triangle tri;
                    fread(&tri, sizeof(md3triangle), 1, f); // read the triangles
                    endianswap(&tri, sizeof(int), 3);
                    loopk(3) m.tris[j].vert[k] = (ushort)tri.vertexindices[k];
                }

                m.numverts = mheader.numvertices;
                m.tcverts = new tcvert[m.numverts];
                fseek(f, mesh_offset + mheader.ofs_uv , SEEK_SET); 
                fread(m.tcverts, 2*sizeof(float), m.numverts, f); // read the UV data
                endianswap(m.tcverts, sizeof(float), 2*m.numverts);
                
                m.verts = new vec[numframes*m.numverts + 1];
                fseek(f, mesh_offset + mheader.ofs_vertices, SEEK_SET); 
                loopj(numframes*mheader.numvertices)
                {
                    md3vertex v;
                    fread(&v, sizeof(md3vertex), 1, f); // read the vertices
                    endianswap(&v, sizeof(short), 4);

                    m.verts[j].x = v.vertex[1]/64.0f;
                    m.verts[j].y = v.vertex[0]/64.0f;
                    m.verts[j].z = v.vertex[2]/64.0f;
                }

                mesh_offset += mheader.meshsize;
            }
            fclose(f);

            filename = newstring(path); 
            return true;
        }

        void begingenshadow()
        {
            matrixpos = 0;
            matrixstack[0].identity();
            matrixstack[0].rotate_around_z(180*RAD);
        }
    };
   
    void render(int anim, int varseed, float speed, int basetime, const vec &o, float yaw, float pitch, dynent *d, modelattach *a, float scale)
    {
        if(!loaded) return;

        if(a) for(int i = 0; a[i].tag; i++)
        {
            vertmodel *m = (vertmodel *)a[i].m;
            if(!m)
            {
                if(a[i].pos) link(NULL, a[i].tag);
                continue;
            }
            part *p = m->parts[0];
            if(link(p, a[i].tag, a[i].pos)) p->index = parts.length()+i;
        }

        if(!cullface) glDisable(GL_CULL_FACE);
        else if(anim&ANIM_MIRROR) glCullFace(GL_BACK);

        if(stenciling)
        {
            shadowdir = vec(0, 1/SQRT2, -1/SQRT2);
            shadowdir.rotate_around_z((-shadowyaw-yaw-180.0f)*RAD);
            shadowdir.rotate_around_y(-pitch*RAD);
            (shadowpos = shadowdir).mul(shadowdist);
        }

        modelpos = o;
        modelyaw = yaw;
        modelpitch = pitch;

        matrixpos = 0;
        matrixstack[0].identity();
        matrixstack[0].translate(o);
        matrixstack[0].rotate_around_z((yaw+180)*RAD);
        matrixstack[0].rotate_around_y(-pitch*RAD);
        if(anim&ANIM_MIRROR || scale!=1) matrixstack[0].scale(scale, anim&ANIM_MIRROR ? -scale : scale, scale);
        parts[0]->render(anim, varseed, speed, basetime, d);

        if(!cullface) glEnable(GL_CULL_FACE);
        else if(anim&ANIM_MIRROR) glCullFace(GL_FRONT);

        if(a) for(int i = 0; a[i].tag; i++) link(NULL, a[i].tag);

        if(d) d->lastrendered = lastmillis;
    }

    void rendershadow(int anim, int varseed, float speed, int basetime, const vec &o, float yaw, modelattach *a)
    {
        if(parts.length()>1) return;
        parts[0]->rendershadow(anim, varseed, speed, basetime, o, yaw);
    }

    bool load()
    {
        if(loaded) return true;
        s_sprintf(md3dir)("packages/models/%s", loadname);

        const char *pname = parentdir(loadname);
        s_sprintfd(cfgname)("packages/models/%s/md3.cfg", loadname);

        loadingmd3 = this;
        persistidents = false;
        if(execfile(cfgname) && parts.length()) // configured md3, will call the md3* commands below
        {
            persistidents = true;
            loadingmd3 = NULL;
            if(parts.empty()) return false;
            loopv(parts) if(!parts[i]->filename) return false;
        }
        else // md3 without configuration, try default tris and skin
        {
            persistidents = false;
            loadingmd3 = NULL;
            md3part &mdl = *new md3part;
            parts.add(&mdl);
            mdl.model = this;
            mdl.index = 0; 
            s_sprintfd(name1)("packages/models/%s/tris.md3", loadname);
            if(!mdl.load(path(name1)))
            {
                s_sprintf(name1)("packages/models/%s/tris.md3", pname);    // try md3 in parent folder (vert sharing)
                if(!mdl.load(path(name1))) return false;
            };
            Texture *skin;
            loadskin(loadname, pname, skin);
            loopv(mdl.meshes) mdl.meshes[i]->skin  = skin;
            if(skin==notexture) conoutf("could not load model skin for %s", name1);
        }
        loopv(parts) parts[i]->scaleverts(scale/16.0f, vec(translate.x, -translate.y, translate.z));
        radius = calcradius();
        if(shadowdist) calcneighbors();
        calcbbs();
        return loaded = true;
    }
};

void md3load(char *model)
{   
    if(!loadingmd3) { conoutf("not loading an md3"); return; };
    s_sprintfd(filename)("%s/%s", md3dir, model);
    md3::md3part &mdl = *new md3::md3part;
    loadingmd3->parts.add(&mdl);
    mdl.model = loadingmd3;
    mdl.index = loadingmd3->parts.length()-1;
    if(!mdl.load(path(filename))) conoutf("could not load %s", filename); // ignore failure
}  
    
void md3skin(char *objname, char *skin)
{   
    if(!objname || !skin) return;
    if(!loadingmd3 || loadingmd3->parts.empty()) { conoutf("not loading an md3"); return; };
    md3::part &mdl = *loadingmd3->parts.last();
    loopv(mdl.meshes)
    {   
        md3::mesh &m = *mdl.meshes[i];
        if(!strcmp(objname, "*") || !strcmp(m.name, objname))
        {
            s_sprintfd(spath)("%s/%s", md3dir, skin);
            m.skin = textureload(spath);
        }
    }
}

void md3anim(char *anim, char *startframe, char *range, char *speed)
{
    if(!loadingmd3 || loadingmd3->parts.empty()) { conoutf("not loading an md3"); return; };
    int num = findanim(anim);
    if(num<0) { conoutf("could not find animation %s", anim); return; };
    loadingmd3->parts.last()->setanim(num, atoi(startframe), atoi(range), atof(speed));
}

void md3link(char *parentno, char *childno, char *tagname)
{
    if(!loadingmd3) { conoutf("not loading an md3"); return; };
    int parent = ATOI(parentno), child = ATOI(childno);
    if(!loadingmd3->parts.inrange(parent) || !loadingmd3->parts.inrange(child)) { conoutf("no models loaded to link"); return; }
    if(!loadingmd3->parts[parent]->link(loadingmd3->parts[child], tagname)) conoutf("could not link model %s", loadingmd3->loadname);
}

void md3emit(char *tag, char *type, char *arg1, char *arg2)
{
    if(!loadingmd3 || loadingmd3->parts.empty()) { conoutf("not loading an md3"); return; };
    md3::part &mdl = *loadingmd3->parts.last();
    if(!mdl.addemitter(tag, ATOI(type), ATOI(arg1), ATOI(arg2))) { conoutf("could not find tag %s", tag); return; }
}
 
COMMAND(md3load, ARG_1STR);
COMMAND(md3skin, ARG_3STR);
COMMAND(md3anim, ARG_4STR);
COMMAND(md3link, ARG_3STR);
COMMAND(md3emit, ARG_4STR);
            
