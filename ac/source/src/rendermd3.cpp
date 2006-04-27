// code for loading, linking and rendering md3 models
// See http://www.draekko.org/documentation/md3format.html for informations about the md3 format

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
    int tex;
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
    bool load(char *path, bool mirror);
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
    if(links) free(links);
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

bool md3model::load(char *path, bool mirror=false)
{
    mirrored = mirror;
    if(!path) return false;
    FILE *f = fopen(path, "rb");
    if(!f) return false;
    md3header header;
    fread(&header, sizeof(md3header), 1, f);
    if(header.id[0] != 'I' || header.id[1] != 'D' || header.id[2] != 'P' || header.id[3] != '3' || header.version != 15) // header check
    { printf("corruped header in md3 model: %s", path); return false; };
    
    tags = new md3tag[header.numframes * header.numtags];
    fseek(f, header.ofs_tags, SEEK_SET);
    fread(tags, sizeof(md3tag), header.numframes * header.numtags, f);
    numframes = header.numframes;
    numtags = header.numtags;
    
    links = (md3model **) malloc(sizeof(md3model) * header.numtags);
    loopi(header.numtags) links[i] = NULL;

    int mesh_offset = ftell(f);
    
    loopi(header.nummeshes)
    {   
        md3mesh mesh;
        md3meshheader mheader;
        fseek(f, mesh_offset, SEEK_SET);
        fread(&mheader, sizeof(md3meshheader), 1, f);
        strn0cpy(mesh.name, mheader.name, 64);
         
        mesh.triangles = new md3triangle[mheader.numtriangles];
        fseek(f, mesh_offset + mheader.ofs_triangles, SEEK_SET);       
        fread(mesh.triangles, sizeof(md3triangle), mheader.numtriangles, f); // read the triangles
        mesh.numtriangles = mheader.numtriangles;
      
        mesh.uv = new vec2[mheader.numvertices];
        fseek(f, mesh_offset + mheader.ofs_uv , SEEK_SET); 
        fread(mesh.uv, sizeof(vec2), mheader.numvertices, f); // read the UV data
        
        md3vertex *vertices = new md3vertex[mheader.numframes * mheader.numvertices];
        fseek(f, mesh_offset + mheader.ofs_vertices, SEEK_SET); 
        fread(vertices, sizeof(md3vertex), mheader.numframes * mheader.numvertices, f); // read the vertices
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

VAR(animdebug, 0, 0, 1);

void md3model::render()
{
    if(!loaded || meshes.length() <= 0) return;
    float t = 0.0f;
    int nextfrm;
    md3state *a = animstate; 
    
    if(a && animations.length() > a->anim)
    {
        md3animinfo *info = &animations[a->anim];
        
        int time = lastmillis - a->lastTime;
        float speed = 1000.0f / (float)info->fps;
        a->frm = (int) (time / speed);

        //printf("no loop  %i > %i\n", a->frm, info->start + info->num-1);
        if(!info->loop && a->frm >= info->num -1)
        {
            if(animdebug) printf("no loop  %i > %i\n", a->frm, info->start + info->num-1);
            a->frm = info->start + info->num-1;
            nextfrm = a->frm;
        }
        else
        {
            t = (time-a->frm*speed)/speed;    
            a->frm = info->start+(a->frm % info->num);
            //if(a->frm>=info->start+info->num-1)
            nextfrm = a->frm + 1;
            //if(animdebug) printf("%i >= %i + %i", nextfrm, a->frm, info->num);
            if(nextfrm>=info->start+info->num) nextfrm=a->frm;
        };
        
        //if(a->frm < info->start || a->frm > info->end) // we switched to a new animation, jump to the start
        //    a->frm = info->start;
        //t = (float) (lastmillis - a->lastTime) / (1000.0f / (float) info->fps ); // t has a value from 0.0f to 1.0f - used for interpolation
        
        if(animdebug) printf("lm:%i lt:%i fps:%i\t(%i %i)\n", lastmillis, a->lastTime, info->fps, info->start, info->num);
        
        
        /*if(t >= 1.0f) // jump to the next keyframe
        {
            a->frm++;
            a->lastTime = lastmillis;
            //t = 0.0f;
            t = t % 1;
            //printf("next keyframe\n");
        };
        if(a->frm >= info->end)
        {
            //printf("end\n");
            if(info->loop)
                a->frm = info->start;
            else
            {
                a->frm = info->end;
                t = 0.0f; // stops the animation
            };
        };*/
    }
    else
    {
        if(animdebug) printf("overflow: curframe %i > maxframe %i\n", a->anim, animations.length());
        
        if(!a) a = new md3state();
        a->frm = 0;
        nextfrm = a->frm + 1;
    }
    
    if(animdebug) printf("interp. from frame %i to %i, %f percent done\n", a->frm, nextfrm, t*100.0f);
    
    #define interpolate(p1,p2) ((p1) + t * ((p2) - (p1)))
    
    loopv(meshes)
    {
        md3mesh *mesh = &meshes[i];

        glBindTexture(GL_TEXTURE_2D, FIRSTMD3 + mesh->tex);
        
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
    
    glScalef( scale.x, scale.y, scale.z);
    
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
int numskins = 0;
char basedir[_MAXDEFSTR]; // necessary for relative path's

void md3skin(char *objname, char *skin) // called by the {lower|upper|head}.cfg (originally .skin in Q3A)
{
    md3model *mdl = models.last();
    loopv(mdl->meshes)
    {   
        md3mesh *mesh = &mdl->meshes[i];
        if(strcmp(mesh->name, objname) == 0)
        {
            int xs, ys;
            sprintf_sd(path)("%s/%s", basedir, skin); // 'skin' is a relative url
            bool highqual = strstr(basedir, "weapons") != NULL;
            installtex(FIRSTMD3 + numskins, path, xs, ys, true, highqual);
            mesh->tex = numskins;
            numskins++;
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
    //a.end = a.start + n - 1;
    a.num = n;
    (atoi(loopings) > 0) ? a.loop = true : a.loop = false;
    a.fps = atoi(fps);
};

COMMAND(md3animation, ARG_5STR);

void loadweapons()
{ 
    firstweapon = models.length();
    loopi(NUMGUNS+1)
    {   
        bool akimbo = i==NUMGUNS;
        sprintf(basedir, "packages/models/weapons/%s", akimbo ? hudgunnames[GUN_PISTOL] : hudgunnames[i]);
        md3model *mdl = new md3model();
        models.add(mdl);
        sprintf_sd(mdl_path)("%s/tris_high.md3", basedir);
        sprintf_sd(cfg_path)("%s/default.skin", basedir);
        sprintf_sd(modelcfg_path) ("%s/animations.cfg", basedir);
        if(!mdl->load(mdl_path, akimbo)) printf("could not load: %s\n", mdl_path);
        exec(cfg_path);
        tmp_animations.setsize(0);
        exec(modelcfg_path);
        loopj(tmp_animations.length()) mdl->animations.add(tmp_animations[j]);
    }
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
      
    weaponmove(vec base, int basetime)
    {
        bool akimbo = player1->akimbo && player1->gunselect==GUN_PISTOL;
        bool throwingnade = player1->gunselect==GUN_GRENADE && player1->thrownademillis;
        int timediff = throwingnade ? (lastmillis-player1->thrownademillis) : lastmillis-basetime;
        kick = k_rot = 0.0f;
        pos = player1->o;
        anim = MDL_GUN_IDLE;
        
        if(akimbo && basetime>lastmillis) // akimbo gun queued for reloading
        {

        }
        else if(player1->reloading)
        {
            anim = MDL_GUN_RELOAD;
            int rtime = reloadtime(player1->gunselect);
            float percent_done = (float)(timediff)*100.0f/rtime;
            if(percent_done >= 100) percent_done = 100;
            k_rot = -(sin((float)(percent_done*2/100.0f*90.0f)*PI/180.0f)*90);
        }
        else
        {
            vec sway = base;
            float k_back = 0.0f;
            
            if(player1->gunselect==player1->lastattackgun)
            {
                int percent_done = timediff*100/attackdelay(player1->gunselect);
                if(percent_done > 100) percent_done = 100.0;
                // f(x) = -sin(x-1.5)^3A
                kick = -sin(pow((1.5f/100*percent_done)-1.5f,3));
            };
            
            if(kick>0.01f)
            { 
                if(throwingnade) {  anim = MDL_GUN_ATTACK2; return; }
                else anim = MDL_GUN_ATTACK;
            };
            
            if(player1->gunselect!=GUN_GRENADE && player1->gunselect!=GUN_KNIFE)
            {
                k_rot = kick_rot(player1->gunselect)*kick;
                k_back = kick_back(player1->gunselect)*kick/10;
            };
    
            int swayt = (lastmillis+1)/10 % 100;
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

void rendermd3gun()
{
    if(firstweapon >= 0)
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
      
        vdist(dist, unitv, player1->o, worldpos);
        vdiv(unitv, dist);
        
        md3model *weapon = models[firstweapon + player1->gunselect];
        md3model *akimbo = player1->akimbo && player1->gunselect==GUN_PISTOL ? models[firstweapon+NUMGUNS] : NULL;
        
        weaponmove *w1, *w2;
        
        if(akimbo)
        {
            w1 = new weaponmove(unitv, akimbolastaction[0]);
            w2 = new weaponmove(unitv, akimbolastaction[1]);
            akimbo->setanim(w2->anim);
        }
        else w1 = new weaponmove(unitv, player1->lastaction);                           
           
        if(!w1) return;
        weapon->setanim(w1->anim);
        weapon->draw( w1->pos.x, w1->pos.z, w1->pos.y, player1->yaw + 90, player1->pitch+w1->k_rot, 1.0f, light);        
        
        if(akimbo && w2) akimbo->draw( w2->pos.x, w2->pos.z, w2->pos.y, player1->yaw + 90, player1->pitch+w2->k_rot, 1.0f, light);
    };
};


