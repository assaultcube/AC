#include "cube.h"

// loading and rendering code for the MD3 model format
// parts of the code are from gamedev.net

const int FIRSTMD3 = 20;
int nummodels = 0;
int numskins = 0;

struct  vec2
{
    float x, y;
};

struct md3header
{
    char    fileID[4];           
    int     version;
    char    filename[68];
    int     numFrames, numTags, numMeshes;
    int     maxSkins;
    int     headersize;
    int     tagStart, tagEnd;
    int     filesize;
};

struct md3meshinfo
{
    char    meshId[4];
    char    name[68];
    int     numMeshFrames, numSkins, numVertices, numTriangles;
    int     triStart;
    int     headersize;
    int     uvStart;
    int     vertexStart;
    int     meshSize;
};

struct md3tag
{
    char    name[64];
    vec     pos;
    float   rotation[3][3];
};

struct md3bone
{
    float   mins[3], maxs[3];                    
    float   position[3];                
    float   scale;                      
    char    creator[16];
};

struct md3triangle
{
   signed short  vertex[3];             
   unsigned char normal[2];
};   

struct md3face
{
   int vertexindices[3];
};

struct md3texcoord
{
     float texturecoord[2];
};

struct md3skin
{
    char name[68];
};

struct md3tface
{
    int vertindex[3];
    int coordindex[3];
};

struct md3animinfo
{
    int startframe;
    int endframe;
    bool loop;
    int fps;
};

struct md3object
{
    int  numVerts, numFaces, numTexVertex;
    int  tex;
    char name[255];
    vec  *verts;
    vec  *normals;
    vec2  *texVerts;
    md3tface *faces;
    int numTags;//EDIT: AH
};

struct md3model 
{
    vector<md3object> objects;
    vector<md3animinfo> animations;
    md3model **links;               
    md3tag *tags;
    int numTags;
/*    int curAnim, curFrame, nextFrame;
    int lastTime;                    */
    md3state *animstate;
    bool loaded;
    int modelnum;
    float scale;
    bool load(char *mdlpath);
    void link(md3model *link, char *tag);
    void render();
    void draw(float x, float y, float z, float yaw, float pitch, float rad);
    md3model();
    ~md3model();
};

md3model::md3model()
{
    loaded = false;
    scale = 0.08f;
};

md3model::~md3model()
{
    loopv(objects){
        if(objects[i].verts) delete [] objects[i].verts;
        if(objects[i].normals) delete [] objects[i].normals;
        if(objects[i].texVerts) delete [] objects[i].texVerts;
        if(objects[i].faces) delete [] objects[i].faces;
    };
    if(links) free(links);
    if(tags) delete [] tags;
};

bool md3model::load(char *mdlpath)
{
    loaded = false;
    if(!mdlpath) return false;

    FILE *f = fopen(mdlpath, "rb");
    if(!f) { conoutf("could not load md3 model: %s", (int)mdlpath); return false; };
    md3header header;
    fread(&header, 1, sizeof(header), f);
    char *id = header.fileID;
    if(id[0] != 'I' || id[1] != 'D' || id[2] != 'P' || header.version != 15) { // check the header
        conoutf("wrong header in md3 model: %s", (int)mdlpath);
        return false;
    };
    
    md3bone *bones = new md3bone[header.numFrames]; // read bones
    fread(bones, sizeof(md3bone), header.numFrames, f);
    delete [] bones;
    
    tags = new md3tag[header.numFrames * header.numTags]; // read tags
    fread(tags, sizeof(md3tag), header.numFrames * header.numTags, f);
    numTags = header.numTags;
    
    links = (md3model **) malloc(sizeof(md3model) * header.numTags); // alloc mem for the links
    loopi(header.numTags) links[i] = NULL;
    
    long meshOffset = ftell(f);
    md3meshinfo meshHeader;

    loopj(header.numMeshes) // go through all sub objects
    {
        fseek(f, meshOffset, SEEK_SET); // read header
        fread(&meshHeader, sizeof(md3meshinfo), 1, f);

        md3skin *skins = new md3skin[meshHeader.numSkins];
        md3texcoord *texcoords = new md3texcoord[meshHeader.numVertices];
        md3face *triangles = new md3face[meshHeader.numTriangles];
        md3triangle *vertices = new md3triangle[meshHeader.numVertices * meshHeader.numMeshFrames];
        
        fread(skins, sizeof(md3skin), meshHeader.numSkins, f); // read skin information     
        
        fseek(f, meshOffset + meshHeader.triStart, SEEK_SET); // read triangles/face data
        fread(triangles, sizeof(md3face), meshHeader.numTriangles, f);
        
        fseek(f, meshOffset + meshHeader.uvStart, SEEK_SET); // read UV data
        fread(texcoords, sizeof(md3texcoord), meshHeader.numVertices, f);
        
        fseek(f, meshOffset + meshHeader.vertexStart, SEEK_SET); // read vertex/face index information
        fread(vertices, sizeof(md3triangle), meshHeader.numMeshFrames * meshHeader.numVertices, f);

        md3object currentMesh = {0};
        strcpy(currentMesh.name, meshHeader.name);
        
        currentMesh.numVerts   = meshHeader.numVertices;
        currentMesh.numTexVertex = meshHeader.numVertices;
        currentMesh.numFaces   = meshHeader.numTriangles;

        currentMesh.verts    = new vec[currentMesh.numVerts * meshHeader.numMeshFrames];
        currentMesh.texVerts = new vec2 [currentMesh.numVerts];
        currentMesh.faces    = new md3tface [currentMesh.numFaces];

        loopi(currentMesh.numVerts * meshHeader.numMeshFrames)
        {
            currentMesh.verts[i].x =  vertices[i].vertex[0] / 64.0f;
            currentMesh.verts[i].y =  vertices[i].vertex[1] / 64.0f;
            currentMesh.verts[i].z =  vertices[i].vertex[2] / 64.0f;
        }

        loopi(currentMesh.numTexVertex)
        {
            currentMesh.texVerts[i].x =  texcoords[i].texturecoord[0];
            currentMesh.texVerts[i].y = -texcoords[i].texturecoord[1];
        }

        loopi(currentMesh.numFaces)
        {
            currentMesh.faces[i].vertindex[0] = triangles[i].vertexindices[0];
            currentMesh.faces[i].vertindex[1] = triangles[i].vertexindices[1];
            currentMesh.faces[i].vertindex[2] = triangles[i].vertexindices[2];

            currentMesh.faces[i].coordindex[0] = triangles[i].vertexindices[0];
            currentMesh.faces[i].coordindex[1] = triangles[i].vertexindices[1];
            currentMesh.faces[i].coordindex[2] = triangles[i].vertexindices[2];
        }
        
        objects.add(currentMesh);    
        
        delete [] skins;
        delete [] texcoords;
        delete [] triangles;
        delete [] vertices;   
        meshOffset += meshHeader.meshSize;
    };
    
    loaded = true;
    return true;
};

void md3model::link(md3model *link, char *tag)
{
    loopi(numTags)
        if(strcmp(tags[i].name, tag) == 0)
            {
                links[i] = link;
                return;
            };
};

void modulo(int a, int b)
{
    conoutf("%i", a % b);
};
COMMAND(modulo, ARG_2INT);

void md3model::render()
{
    int startfrm = 0;
    int endfrm = 1;
    float t = 0.0f;
    
    md3state *a = animstate;
    md3animinfo *anim = NULL;
    
    if(animations.length()) 
    {
        anim = &animations[a->anim];
        if(a->frm < anim->startframe || a->frm > anim->endframe) 
        {
            a->frm = anim->startframe;
            a->lastTime = lastmillis;  
        };
        startfrm = anim->startframe;
        endfrm = anim->endframe;
    }
    
    int nextfrm = (a->frm + 1) % endfrm;
    if(nextfrm == 0) 
    {
        if(anim && anim->loop) nextfrm = startfrm;
        else 
            a->lastTime = lastmillis;
    };
    int elapsedtime = 0;
    
    if(animations.length())
    {
        elapsedtime = lastmillis - a->lastTime;
        t = (float) elapsedtime / (1000 / anim->fps);
        if(t >= 1.0f)
        {
            a->frm = nextfrm;
            a->lastTime = lastmillis;
        };
    };
    
    printf("anim:%d\tfrm:%d\tt:%f\n", a->anim, a->frm, t);
    
    loopv(objects)
    {
        md3object *obj = &objects[i];
        
        int currentIndex = a->frm * obj->numVerts; 
        int nextIndex = nextfrm * obj->numVerts; 
    
        glBindTexture(GL_TEXTURE_2D, obj->tex + FIRSTMD3);  
        
        glBegin(GL_TRIANGLES);
            loopj(obj->numFaces)
            {
                loop(whichVertex, 3)
                {
                    int index = obj->faces[j].vertindex[whichVertex];
    
                    if(obj->texVerts) 
                        glTexCoord2f(obj->texVerts[index].x, -obj->texVerts[index].y);

                    vec point1 = obj->verts[ currentIndex + index ];
                    vec point2 = obj->verts[ nextIndex + index];
                        
                    glVertex3f(point1.x + t * (point2.x - point1.x),
                            point1.y + t * (point2.y - point1.y),
                            point1.z + t * (point2.z - point1.z));                 
                }
            }
        glEnd();
    }
         
    #define ip(r, c,n) (r) = (c) + t * ((n) - (c))
    #define ip_vec(c,n) ip(c.x,c.x,n.x); ip(c.y,c.y,n.y); ip(c.z,c.z,n.z);
    loopi(numTags)
    {
        md3model *link = links[i];
        if(!link) continue;
        vec pos = tags[a->frm * numTags + i].pos;
        vec nextpos = tags[nextfrm * numTags + i].pos;
        ip_vec(pos, nextpos);
        float rot[3][3];
        loop(j, 3) loop(k, 3) ip(rot[j][k], tags[a->frm * numTags + i].rotation[j][k], tags[nextfrm * numTags + i].rotation[j][k]);
        GLfloat matrix[16];
        matrix[0] = rot[0][0];
        matrix[1] = rot[0][1];
        matrix[2] = rot[0][2];
        matrix[3] = 0;
        matrix[4] = rot[1][0];
        matrix[5] = rot[1][1];
        matrix[6] = rot[1][2];
        matrix[7] = 0;
        matrix[8] = rot[2][0];
        matrix[9] = rot[2][1];
        matrix[10]= rot[2][2];
        matrix[11]= 0;
        matrix[12] = pos.x;
        matrix[13] = pos.y;
        matrix[14] = pos.z;
        matrix[15] = 1;
        glPushMatrix();
            glMultMatrixf(matrix);
            link->render();
        glPopMatrix();
    };
};

void md3model::draw(float x, float y, float z, float yaw, float pitch, float rad)
{
    if(!loaded || objects.length() <= 0) return;
    //if(isoccluded(player1->o.x, player1->o.y, x-rad, z-rad, rad*2)) return;
    
    glPushMatrix();
    
    int ix = (int)x;
    int iy = (int)z;
    vec light = { 1.0f, 1.0f, 1.0f };
    if(!OUTBORD(ix, iy))
    {
        sqr *s = S(ix,iy);  
        float ll = 256.0f; // 0.96f;
        float of = 0.0f; // 0.1f;      
        light.x = 0.75f * s->r/ll+of;
        light.y = 0.75f * s->g/ll+of;
        light.z = 0.75f * s->b/ll+of;
    };    
    glColor3fv((float *)&light);
    
    glTranslatef(x, y, z); // avoid models above ground
    
    glRotatef(yaw+180, 0, -1, 0);
    glRotatef(pitch, 0, 0, 1);
    glRotatef(-90, 1, 0, 0);
    
    glScalef( scale, scale, scale);
    glTranslatef( 0.0f, 0.0f, 24.0f );
    
    render();
    
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

vector<md3model *> playermodels;
vector<md3model *> weaponmodels;
vector<md3animinfo> tmp_animations;
char basedir[_MAXDEFSTR]; // necessary for relative path's

void md3skin(char *objname, char *skin) // called by the {lower|upper|head}.cfg (originally .skin in Q3A)
{
    md3model *mdl = playermodels.last();
    loopv(mdl->objects)
    {   
        md3object *obj = &mdl->objects[i];
        if(strcmp(obj->name, objname) == 0)
        {
            int xs, ys;
            sprintf_sd(path)("%s/%s", basedir, skin); // 'skin' is a relative url
            installtex(FIRSTMD3 + numskins, path, xs, ys);
            obj->tex = numskins;
            numskins++;
        };
    };
};

COMMAND(md3skin, ARG_2STR);

void md3animation(char *firstframe, char *numframes, char *loopingframes, char *fps) /* configurable animations - use hardcoded instead ? */
{
    md3animinfo &a = tmp_animations.add();
    a.startframe = atoi(firstframe);
    a.endframe = a.startframe + atoi(numframes);
    (atoi(loopingframes) > 0) ? a.loop = true : a.loop = false;
    a.fps = atoi(fps);
};

COMMAND(md3animation, ARG_5STR);

/*
void rendermd3(char *mdl, float x, float y, float z, float yaw, float pitch, float rad)
{    
    if(playermodels.length()) playermodels[MDL_LOWER]->draw(x, y, z, yaw, pitch, rad);
};*/

VAR(anim, 0, 0, 1000);

void rendermd3player(dynent *d)
{
    if(playermodels.length() >= 3 && d->mdl[MDL_LOWER] == 0)
    {
        loopi(3) playermodels[d->mdl[i]]->animstate = &d->animstate[i]; // get d's current frame
        
        md3setanim(d, anim);
        
        int n;
        float mz = d->o.z-d->eyeheight+1.55f;
        /*if(d->state==CS_DEAD)
        {

            md3setanim(d, BOTH_DEATH1 + rnd(2) * 2);
            //mdl = (((int)d>>6)&1)+1;
            //mz = d->o.z-d->eyeheight+0.2f;
            //scale = 1.2f;
        }
        else if(d->state==CS_EDITING)                   { md3setanim(d, TORSO_STAND); }
        else if(d->state==CS_LAGGED)                    { md3setanim(d, LEGS_IDLE); md3setanim(d, TORSO_GESTURE); }
        else if(d->monsterstate==M_ATTACKING)           { n = 8;  }
        else if(d->monsterstate==M_PAIN)                { n = 10; } 
        else if((!d->move && !d->strafe) || !d->moving) { md3setanim(d, LEGS_IDLE); md3setanim(d, TORSO_STAND); } 
        else if(!d->onfloor && d->timeinair>100)        { md3setanim(d, LEGS_JUMP ); }
        else                                            { n = 14; }; */
        
        playermodels[d->mdl[MDL_LOWER]]->draw(d->o.x, mz, d->o.y, d->yaw+90, d->pitch, d->radius);
    };
};

void testrender(float x, float y, float z, float yaw, float pitch, float radius)
{
    if(playermodels.length())
    {
        playermodels[0]->draw(x, z, y, yaw+90, pitch, radius);
    };
};

md3model *loadmd3(char *path) // loads a single md3 model and the skins for its objects (cfg file)
{
    md3model *mdl = new md3model();
    playermodels.add(mdl);
    sprintf_sd(mdl_path)("%s.md3", path);
    sprintf_sd(cfg_path)("%s.cfg", path);
    playermodels.last()->load(mdl_path);
    exec(cfg_path);
    return playermodels.last();
};

char pl_objects[3][6] = {"lower", "upper", "head"};

void loadplayermdl(char *model)
{ 
    tmp_animations.setsize(0);
    sprintf(basedir, "packages/models/playermodels/%s", model);
    md3model *mdls[3];
    loopi(3)  // load lower,upper and head objects
    {
        sprintf_sd(path)("%s/%s", basedir, pl_objects[i]);
        mdls[i] = loadmd3(path);
    };
    mdls[MDL_LOWER]->link(mdls[MDL_UPPER], "tag_torso");
    mdls[MDL_UPPER]->link(mdls[MDL_HEAD], "tag_head");
    sprintf_sd(modelcfg)("%s/model.cfg", basedir);
    exec(modelcfg); // load animations to tmp_animation
    loopv(tmp_animations) // assign the loaded animations to the lower,upper or head model
    {
        md3animinfo &anim = tmp_animations[i];
        if(i <= BOTH_DEAD3)
        {
            mdls[MDL_UPPER]->animations.add(anim);
            mdls[MDL_LOWER]->animations.add(anim);
        }
        else if(i <= TORSO_STAND2)
            mdls[MDL_UPPER]->animations.add(anim);
        else if(i <= LEGS_TURN)    
            mdls[MDL_LOWER]->animations.add(anim);
    };
};

COMMAND(loadplayermdl, ARG_1STR);
