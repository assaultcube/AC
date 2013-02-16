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

            stream *f = openfile(path, "rb");
            if(!f) return false;
            md3header header;
            f->read(&header, sizeof(md3header));
            lilswap(&header.version, 1);
            lilswap(&header.flags, 9);
            if(strncmp(header.id, "IDP3", 4) != 0 || header.version != 15) // header check
            {
                delete f;
                conoutf("md3: corrupted header");
                return false;
            }

            numframes = header.numframes;
            numtags = header.numtags;
            if(numtags)
            {
                tags = new tag[numframes*numtags];
                f->seek(header.ofs_tags, SEEK_SET);
                md3tag tag;

                loopi(header.numframes*header.numtags)
                {
                    f->read(&tag, sizeof(md3tag));
                    lilswap((float *)&tag.pos, 12);
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
                f->seek(mesh_offset, SEEK_SET);
                f->read(&mheader, sizeof(md3meshheader));
                lilswap(&mheader.flags, 10);

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
                f->seek(mesh_offset + mheader.ofs_triangles, SEEK_SET);
                loopj(mheader.numtriangles)
                {
                    md3triangle tri;
                    f->read(&tri, sizeof(md3triangle)); // read the triangles
                    lilswap((int *)&tri, 3);
                    loopk(3) m.tris[j].vert[k] = (ushort)tri.vertexindices[k];
                }

                m.numverts = mheader.numvertices;
                m.tcverts = new tcvert[m.numverts];
                f->seek(mesh_offset + mheader.ofs_uv , SEEK_SET);
                f->read(m.tcverts, 2*sizeof(float)*m.numverts); // read the UV data
                lilswap(&m.tcverts[0].u, 2*m.numverts);

                m.verts = new vec[numframes*m.numverts + 1];
                f->seek(mesh_offset + mheader.ofs_vertices, SEEK_SET);
                loopj(numframes*mheader.numvertices)
                {
                    md3vertex v;
                    f->read(&v, sizeof(md3vertex)); // read the vertices
                    lilswap((short *)&v, 4);

                    m.verts[j].x = v.vertex[1]/64.0f;
                    m.verts[j].y = v.vertex[0]/64.0f;
                    m.verts[j].z = v.vertex[2]/64.0f;
                }

                mesh_offset += mheader.meshsize;
            }
            delete f;

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
        formatstring(md3dir)("packages/models/%s", loadname);

        const char *pname = parentdir(loadname);
        defformatstring(cfgname)("packages/models/%s/md3.cfg", loadname);

        loadingmd3 = this;
        per_idents = false;
        neverpersist = true;
        if(execfile(cfgname) && parts.length()) // configured md3, will call the md3* commands below
        {
            neverpersist = false;
            per_idents = true;
            loadingmd3 = NULL;
            if(parts.empty()) return false;
            loopv(parts) if(!parts[i]->filename) return false;
        }
        else // md3 without configuration, try default tris and skin
        {
            per_idents = false;
            loadingmd3 = NULL;
            md3part &mdl = *new md3part;
            parts.add(&mdl);
            mdl.model = this;
            mdl.index = 0;
            defformatstring(name1)("packages/models/%s/tris.md3", loadname);
            if(!mdl.load(path(name1)))
            {
                formatstring(name1)("packages/models/%s/tris.md3", pname);    // try md3 in parent folder (vert sharing)
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
    defformatstring(filename)("%s/%s", md3dir, model);
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
            defformatstring(spath)("%s/%s", md3dir, skin);
            m.skin = textureload(spath);
        }
    }
}

void md3anim(char *anim, int *startframe, int *range, float *speed)
{
    if(!loadingmd3 || loadingmd3->parts.empty()) { conoutf("not loading an md3"); return; };
    int num = findanim(anim);
    if(num<0) { conoutf("could not find animation %s", anim); return; };
    loadingmd3->parts.last()->setanim(num, *startframe, *range, *speed);
}

void md3link(int *parent, int *child, char *tagname)
{
    if(!loadingmd3) { conoutf("not loading an md3"); return; };
    if(!loadingmd3->parts.inrange(*parent) || !loadingmd3->parts.inrange(*child)) { conoutf("no models loaded to link"); return; }
    if(!loadingmd3->parts[*parent]->link(loadingmd3->parts[*child], tagname)) conoutf("could not link model %s", loadingmd3->loadname);
}

void md3emit(char *tag, int *type, int *arg1, int *arg2)
{
    if(!loadingmd3 || loadingmd3->parts.empty()) { conoutf("not loading an md3"); return; };
    md3::part &mdl = *loadingmd3->parts.last();
    if(!mdl.addemitter(tag, *type, *arg1, *arg2)) { conoutf("could not find tag %s", tag); return; }
}

COMMAND(md3load, "s");
COMMAND(md3skin, "ss");
COMMAND(md3anim, "siif");
COMMAND(md3link, "iis");
COMMAND(md3emit, "siii");
