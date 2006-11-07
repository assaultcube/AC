// code for loading, linking and rendering md3 models
// See http://www.icculus.org/homepages/phaethon/q3a/formats/md3format.html for informations about the md3 format

#include "cube.h"

const int FIRSTMD3 = 20;
enum { MDL_LOWER = 0, MDL_UPPER, MDL_HEAD };

enum { MDL_GUN_IDLE = 0, MDL_GUN_ATTACK, MDL_GUN_RELOAD, MDL_GUN_ATTACK2}; // attack2 is for grenade only

#define MD3_DEFAULT_SCALE 0.08f

struct vec2
{
    float x, y;
};

struct md3tag
{
    char name[64];
    float pos[3];
    float rotation[3][3];
};

struct md3vertex
{
    signed short vertex[3];
    uchar normal[2];
};

struct md3triangle
{
    int vertexindices[3];
};

struct md3animinfo
{
    int start;
    int num;
    bool loop;
    int fps;
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

struct md3mesh
{
    char name[64];
    md3triangle *triangles;
    vec *vertices;
    vec2 *uv;
    int numtriangles, numvertices; // numvertices == numUV
    Texture *skin;
};

struct md3model
{
    vector<md3mesh> meshes;
    vector<md3animinfo> animations;
    md3state *animstate;
    md3model **links;
    md3tag *tags;
    int numframes, numtags;
    bool loaded;
    vec scale;
    bool mirrored;
    void setanim(int anim);
    void setanimstate(md3state &as);
    bool link(md3model *link, char *tag);
    bool load(char *path, bool _mirrored);
    void render();
    void draw(float x, float y, float z, float yaw, float pitch, float rad, vec &light);
    md3model();
    ~md3model();
};

md3model::md3model()
{
    mirrored = false;
    loaded = false;
    scale.x = scale.y = scale.z = MD3_DEFAULT_SCALE;
//    animstate = NULL;
    animstate = new md3state();
    animstate->lastTime = lastmillis;
};

md3model::~md3model()
{
    loopv(meshes){
        if(meshes[i].vertices) delete [] meshes[i].vertices;
        if(meshes[i].triangles) delete [] meshes[i].triangles;
        if(meshes[i].uv) delete [] meshes[i].uv;
    };
    if(links) delete[] links;
    if(tags) delete [] tags;
};

void md3model::setanimstate(md3state &as)
{
    animstate->anim = as.anim;
    animstate->frm = as.frm;
    animstate->lastTime = as.lastTime;
};

void md3model::setanim(int anim)
{
    if(animstate->anim != anim) animstate->lastTime = lastmillis;
    animstate->anim = anim;
};

bool md3model::link(md3model *link, char *tag)
{
    loopi(numtags)
        if(strcmp(tags[i].name, tag) == 0)
            {
                links[i] = link;
                return true;
            };
    return false;
};

bool md3model::load(char *path, bool _mirrored=false)
{
    mirrored = _mirrored;
    if(!path) return false;
    FILE *f = fopen(path, "rb");
    if(!f) return false;
    md3header header;
    fread(&header, sizeof(md3header), 1, f);
	
	endianswap(&header.version, sizeof(int), 1);
	endianswap(&header.flags, sizeof(int), 9);
	if(strncmp(header.id, "IDP3", 4) != 0 || header.version != 15) // header check

    if(header.id[0] != 'I' || header.id[1] != 'D' || header.id[2] != 'P' || header.id[3] != '3' || header.version != 15) // header check
    { printf("corruped header in md3 model: %s", path); return false; };
    
    tags = new md3tag[header.numframes * header.numtags];
    fseek(f, header.ofs_tags, SEEK_SET);
    fread(tags, sizeof(md3tag), header.numframes * header.numtags, f);
	loopi(header.numframes*header.numtags) endianswap(&tags[i].pos, sizeof(float), 12);
    numframes = header.numframes;
    numtags = header.numtags;
    
    links = new md3model *[header.numtags];
    loopi(header.numtags) links[i] = NULL;

    int mesh_offset = ftell(f);
    
    loopi(header.nummeshes)
    {   
        md3mesh mesh;
        md3meshheader mheader;
        fseek(f, mesh_offset, SEEK_SET);
        fread(&mheader, sizeof(md3meshheader), 1, f);
		endianswap(&mheader.flags, sizeof(int), 10); 
        s_strncpy(mesh.name, mheader.name, 64);
         
        mesh.triangles = new md3triangle[mheader.numtriangles];
        fseek(f, mesh_offset + mheader.ofs_triangles, SEEK_SET);       
        fread(mesh.triangles, sizeof(md3triangle), mheader.numtriangles, f); // read the triangles
		endianswap(mesh.triangles, sizeof(int), 3*mheader.numtriangles);
        mesh.numtriangles = mheader.numtriangles;
      
        mesh.uv = new vec2[mheader.numvertices];
        fseek(f, mesh_offset + mheader.ofs_uv , SEEK_SET); 
        fread(mesh.uv, sizeof(vec2), mheader.numvertices, f); // read the UV data
		endianswap(mesh.uv, sizeof(float), 2*mheader.numvertices);
        
        md3vertex *vertices = new md3vertex[mheader.numframes * mheader.numvertices];
        fseek(f, mesh_offset + mheader.ofs_vertices, SEEK_SET); 
        fread(vertices, sizeof(md3vertex), mheader.numframes * mheader.numvertices, f); // read the vertices
		endianswap(vertices, sizeof(short), 4*mheader.numframes*mheader.numvertices);

        mesh.vertices = new vec[mheader.numframes * mheader.numvertices]; // transform to our own structure
        loopj(mheader.numframes * mheader.numvertices)
        {
            mesh.vertices[j].x = vertices[j].vertex[0] / 64.0f * (mirrored ? -1 : 1);
            mesh.vertices[j].y = vertices[j].vertex[1] / 64.0f;
            mesh.vertices[j].z = vertices[j].vertex[2] / 64.0f;
        };   
        mesh.numvertices = mheader.numvertices;
        
        meshes.add(mesh);
        mesh_offset += mheader.meshsize;
        delete [] vertices;
    };
    
    loaded = true;
    return true;
};


void md3model::render()
{
    if(!loaded || meshes.length() <= 0) return;
    int nextfrm;
	float t = 0.0f;
    md3state *a = animstate; 
    
    if(a && animations.length() > a->anim)
    {
        md3animinfo *info = &animations[a->anim];
        
        int time = lastmillis - a->lastTime;
        float speed = 1000.0f / (float)info->fps;
        a->frm = (int) (time / speed);

        if(!info->loop && a->frm >= info->num-1)
        {
            a->frm = info->start + info->num-1;
            nextfrm = a->frm;
        }
        else
        {
            t = (time-a->frm*speed)/speed;    
            a->frm = info->start+(a->frm % info->num);
            nextfrm = a->frm + 1;
            if(nextfrm>=info->start+info->num) nextfrm=a->frm;
        };
    }
    else
    {   
        if(!a) a = new md3state();
        a->frm = 0;
        nextfrm = a->frm + 1;
    }
    
    #define interpolate(p1,p2) ((p1) + t * ((p2) - (p1)))
    
    loopv(meshes)
    {
        md3mesh *mesh = &meshes[i];

        glBindTexture(GL_TEXTURE_2D, mesh->skin->id);
        
        glBegin(GL_TRIANGLES);
            loopj(mesh->numtriangles) // triangles
            {
                loopk(3) // vertices
                {
                    int index = mesh->triangles[j].vertexindices[k];
                    vec *point1 = &mesh->vertices[index + a->frm * mesh->numvertices];
                    vec *point2 = &mesh->vertices[index + nextfrm * mesh->numvertices];
                    
                    if(mesh->uv) 
                        glTexCoord2f(mesh->uv[index].x, mesh->uv[index].y);
                    
                    glVertex3f( interpolate(point1->x, point2->x),
                                interpolate(point1->y, point2->y),
                                interpolate(point1->z, point2->z));
                };
            };
        glEnd();
    };

    
    loopi(numtags) // render the linked models - interpolate rotation and position of the 'link-tags'
    {
        md3model *link = links[i];
        if(!link) continue;
        GLfloat matrix[16] = {0}; // fixme: un-obfuscate it!
        loopj(3) matrix[j] = interpolate(tags[a->frm * numtags + i].rotation[0][j], tags[nextfrm * numtags + i].rotation[0][j]); // rotation
        loopj(3) matrix[4 + j] = interpolate(tags[a->frm * numtags + i].rotation[1][j], tags[nextfrm * numtags + i].rotation[1][j]);
        loopj(3) matrix[8 + j] = interpolate(tags[a->frm * numtags + i].rotation[2][j], tags[nextfrm * numtags + i].rotation[2][j]);
        loopj(3) matrix[12 + j] = interpolate(tags[a->frm * numtags + i].pos[j] , tags[nextfrm * numtags + i].pos[j]); // position
        matrix[15] = 1.0f;
        glPushMatrix();
            glMultMatrixf(matrix);
            link->render();
        glPopMatrix();
    };
};

void md3model::draw(float x, float y, float z, float yaw, float pitch, float rad, vec &light)
{
    glPushMatrix();
    glColor3fv((float *)&light);
    glTranslatef(x, y, z);

    glRotatef(yaw+180, 0, -1, 0);
    glRotatef(pitch, 0, 0, 1);
    glRotatef(-90, 0, 1, 0);
    glRotatef(-90, 1, 0, 0);
    
    glScalef(scale.x, scale.y, scale.z);
    
    if(mirrored) glCullFace(GL_BACK);
    render();
    if(mirrored) glCullFace(GL_FRONT);
    
    glPopMatrix();
};

void md3setanim(dynent *d, int anim)
{
    if(anim <= BOTH_DEAD3) // assign to both, legs and torso
    {
        d->animstate[MDL_LOWER].anim = anim;
        d->animstate[MDL_UPPER].anim = anim;
    }
    else if(anim <= TORSO_STAND2)
        d->animstate[MDL_UPPER].anim = anim;
    else
        d->animstate[MDL_LOWER].anim = anim - 7;
};

vector<md3model *> models;
vector<md3animinfo> tmp_animations;
int firstweapon = -1;
string basedir; // necessary for relative path's

void md3skin(char *objname, char *skin) // called by the {lower|upper|head}.cfg (originally .skin in Q3A)
{
    md3model *mdl = models.last();
    loopv(mdl->meshes)
    {   
        md3mesh *mesh = &mdl->meshes[i];
        if(strcmp(mesh->name, objname) == 0)
        {
            s_sprintfd(path)("%s/%s", basedir, skin); // 'skin' is a relative url
            bool highqual = strstr(basedir, "weapons") != NULL;
            mesh->skin = textureload(path, true, highqual); 
        };
    };
};

COMMAND(md3skin, ARG_2STR);

void md3animation(char *first, char *nums, char *loopings, char *fps) /* configurable animations */
{
    md3animinfo &a = tmp_animations.add();
    a.start = atoi(first);
    int n = atoi(nums);
    if(n<=0) n = 1;
    a.num = n;
    (atoi(loopings) > 0) ? a.loop = true : a.loop = false;
    a.fps = atoi(fps);
};

COMMAND(md3animation, ARG_5STR);

bool loadweapon(char *name, bool isakimbo=false)
{
    if(!name) return false;
    sprintf(basedir, "packages/models/weapons/%s", name);
    
    md3model *mdl = new md3model();
    s_sprintfd(mdl_path)("%s/tris_high.md3", basedir);
    s_sprintfd(cfg_path)("%s/default.skin", basedir);
    s_sprintfd(modelcfg_path) ("%s/animations.cfg", basedir);
    
    if(!mdl->load(mdl_path, isakimbo))
    {
        printf("could not load: %s\n", mdl_path);
        delete mdl;
        return false;
    }

    models.add(mdl);
    exec(cfg_path);
    tmp_animations.setsize(0);
    exec(modelcfg_path); // load animations
    loopj(tmp_animations.length()) mdl->animations.add(tmp_animations[j]);
    return true;
}

void loadweapons()
{ 
    firstweapon = models.length();
    loopi(NUMGUNS) loadweapon(hudgunnames[i]);
    loadweapon(hudgunnames[GUN_PISTOL], true); // akimbo
};

VAR(swayspeeddiv, 1, 125, 1000);
VAR(swaymovediv, 1, 200, 1000); 

VAR(swayupspeeddiv, 1, 125, 1000);
VAR(swayupmovediv, 1, 200, 1000); 

VAR(x, 0, 0, 1000);
VAR(y, 0, 0, 1000);
VAR(z, 0, 0, 1000);

struct weaponmove
{
    float k_rot, kick;
    vec pos;
    int anim;
    
	weaponmove() : k_rot(0), kick(0), anim(0) { pos.x = pos.y = pos.z = 0.0f; };

    void calcmove(vec base, int basetime)
    {
        int timediff = NADE_THROWING ? (lastmillis-player1->thrownademillis) : lastmillis-basetime;
        int animtime = attackdelay(player1->gunselect);
        int rtime = reloadtime(player1->gunselect);
        
        kick = k_rot = 0.0f;
        pos = player1->o;
        anim = MDL_GUN_IDLE;
        
        if(player1->weaponchanging)
        {
            anim = MDL_GUN_RELOAD;
            float percent_done = (float)(timediff)*100.0f/(float) WEAPONCHANGE_TIME;
            if(percent_done>=100 || percent_done<0) percent_done = 100;
            k_rot = -(sin((float)(percent_done*2/100.0f*90.0f)*PI/180.0f)*90);
        }
        else if(player1->reloading)
        {
            anim = MDL_GUN_RELOAD;
            float percent_done = (float)(timediff)*100.0f/(float)rtime;
            if(percent_done>=100 || percent_done<0) percent_done = 100;
            k_rot = -(sin((float)(percent_done*2/100.0f*90.0f)*PI/180.0f)*90);
        }
        else
        {
            vec sway = base;
            float percent_done = 0.0f;
            float k_back = 0.0f;
            
            if(player1->gunselect==player1->lastattackgun)
            {
                percent_done = timediff*100.0f/(float)animtime;
                if(percent_done > 100.0f) percent_done = 100.0f;
                // f(x) = -sin(x-1.5)^3
                kick = -sin(pow((1.5f/100.0f*percent_done)-1.5f,3));
            };
            
			if(player1->lastaction && player1->lastattackgun==player1->gunselect)
            {
				if(NADE_THROWING && timediff<animtime) anim = MDL_GUN_ATTACK2;
				else if(lastmillis-player1->lastaction<animtime || NADE_IN_HAND) 
					anim = MDL_GUN_ATTACK;
			};
            
            if(player1->gunselect!=GUN_GRENADE && player1->gunselect!=GUN_KNIFE)
            {
                k_rot = kick_rot(player1->gunselect)*kick;
                k_back = kick_back(player1->gunselect)*kick/10;
            };
    
            float swayspeed = (float) (sin((float)lastmillis/swayspeeddiv))/(swaymovediv/10.0f);
            float swayupspeed = (float) (sin((float)lastmillis/swayupspeeddiv-90))/(swayupmovediv/10.0f);

            #define g0(x) ((x) < 0.0f ? -(x) : (x))
            float plspeed = min(1.0f, sqrt(g0(player1->vel.x*player1->vel.x) + g0(player1->vel.y*player1->vel.y)));
            
            swayspeed *= plspeed/2;
            swayupspeed *= plspeed/2;
            
            float tmp = sway.x;
            sway.x = sway.y;
            sway.y = -tmp;
            
            if(swayupspeed<0.0f)swayupspeed = -swayupspeed; // sway a semicirle only
            sway.z = 1.0f;
            
            sway.x *= swayspeed;
            sway.y *= swayspeed;
            sway.z *= swayupspeed;
            
            pos.x = player1->o.x-base.x*k_back+sway.x;
            pos.y = player1->o.y-base.y*k_back+sway.y;
            pos.z = player1->o.z-base.z*k_back+sway.z;
        };
    };
};


void rendergun(md3model *weapon, int lastaction)
{
    int ix = (int)player1->o.x;
    int iy = (int)player1->o.y;
    vec light = { 1.0f, 1.0f, 1.0f }; 

    if(!OUTBORD(ix, iy))
    {
            sqr *s = S(ix,iy);  
            float ll = 256.0f; // 0.96f;
            float of = 0.0f; // 0.1f;      
            light.x = s->r/ll+of;
            light.y = s->g/ll+of;
            light.z = s->b/ll+of;
    };
        
    vec unitv;
    float dist = worldpos.dist(player1->o, unitv);
    unitv.div(dist);
    
	weaponmove wm;
	if(!intermission) wm.calcmove(unitv, lastaction);
    weapon->setanim(wm.anim);
    weapon->draw(wm.pos.x, wm.pos.z, wm.pos.y, player1->yaw + 90, player1->pitch+wm.k_rot, 1.0f, light);  
};

void rendermd3gun()
{
    if(firstweapon >= 0)
    {
        if(player1->akimbo && player1->gunselect==GUN_PISTOL) // akimbo
        {
            rendergun(models[firstweapon+GUN_PISTOL], player1->akimbolastaction[0]);
            rendergun(models[firstweapon+NUMGUNS], player1->akimbolastaction[1]);
        }
        else
        {
            md3model *weapon = models[firstweapon + player1->gunselect];    
            rendergun(weapon, player1->lastaction);
        };
    };
};


