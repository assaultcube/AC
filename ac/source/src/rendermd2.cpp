// rendermd2.cpp: loader code adapted from a nehe tutorial

#include "cube.h"

struct md2_header
{
    int magic;
    int version;
    int skinWidth, skinHeight;
    int frameSize;
    int numSkins, numVertices, numTexcoords;
    int numTriangles, numGlCommands, numFrames;
    int offsetSkins, offsetTexcoords, offsetTriangles;
    int offsetFrames, offsetGlCommands, offsetEnd;
};

struct md2_vertex
{
    uchar vertex[3], lightNormalIndex;
};

struct md2_frame
{
    float      scale[3];
    float      translate[3];
    char       name[16];
    md2_vertex vertices[1];
};

struct md2
{
    int numGlCommands;
    int* glCommands;
    int numTriangles;
    int frameSize;
    int numFrames;
    int numVerts;
    char* frames;
    vec **mverts;
    int displaylist;
    int displaylistverts;
    
    mapmodelinfo mmi;
    char *loadname;
    int mdlnum;
    bool loaded;

    bool load(char* filename);
    void render(vec &light, int numFrame, int range, float x, float y, float z, float yaw, float pitch, float scale, float speed, int snap, int basetime);
    void scale(int frame, float scale, int sn);

    md2() : numGlCommands(0), frameSize(0), numFrames(0), displaylist(0), loaded(false) {};

    ~md2()
    {
        if(glCommands)
            delete [] glCommands;
        if(frames)
            delete [] frames;
    }
};


bool md2::load(char* filename)
{
    FILE* file;
    md2_header header;

    if((file= fopen(filename, "rb"))==NULL) return false;

    fread(&header, sizeof(md2_header), 1, file);
    endianswap(&header, sizeof(int), sizeof(md2_header)/sizeof(int));

    if(header.magic!= 844121161 || header.version!=8) return false;

    frames = new char[header.frameSize*header.numFrames];
    if(frames==NULL) return false;

    fseek(file, header.offsetFrames, SEEK_SET);
    fread(frames, header.frameSize*header.numFrames, 1, file);

    for(int i = 0; i < header.numFrames; ++i)
    {
        endianswap(frames + i * header.frameSize, sizeof(float), 6);
    }

    glCommands = new int[header.numGlCommands];
    if(glCommands==NULL) return false;

    fseek(file,       header.offsetGlCommands, SEEK_SET);
    fread(glCommands, header.numGlCommands*sizeof(int), 1, file);

    endianswap(glCommands, sizeof(int), header.numGlCommands);

    numFrames    = header.numFrames;
    numGlCommands= header.numGlCommands;
    frameSize    = header.frameSize;
    numTriangles = header.numTriangles;
    numVerts     = header.numVertices;

    fclose(file);
    
    mverts = new vec*[numFrames];
    loopj(numFrames) mverts[j] = NULL;

    return true;
};

float snap(int sn, float f) { return sn ? (float)(((int)(f+sn*0.5f))&(~(sn-1))) : f; };

void md2::scale(int frame, float scale, int sn)
{
    mverts[frame] = new vec[numVerts];
    md2_frame *cf = (md2_frame *) ((char*)frames+frameSize*frame);
    float sc = 16.0f/scale;
    loop(vi, numVerts)
    {
        uchar *cv = (uchar *)&cf->vertices[vi].vertex;
        vec *v = &(mverts[frame])[vi];
        v->x =  (snap(sn, cv[0]*cf->scale[0])+cf->translate[0])/sc;
        v->y = -(snap(sn, cv[1]*cf->scale[1])+cf->translate[1])/sc;
        v->z =  (snap(sn, cv[2]*cf->scale[2])+cf->translate[2])/sc;
    };
};

void md2::render(vec &light, int frame, int range, float x, float y, float z, float yaw, float pitch, float sc, float speed, int snap, int basetime)
{
    loopi(range) if(!mverts[frame+i]) scale(frame+i, sc, snap);
    
    glPushMatrix ();
    glTranslatef(x, y, z);
    glRotatef(yaw+180, 0, -1, 0);
    glRotatef(pitch, 0, 0, 1);
    
	glColor3fv((float *)&light);

    if(displaylist && frame==0 && range==1)
    {
		glCallList(displaylist);
		xtraverts += displaylistverts;
    }
    else
    {
		if(frame==0 && range==1)
		{
			static int displaylistn = 10;
			glNewList(displaylist = displaylistn++, GL_COMPILE);
			displaylistverts = xtraverts;
		};
		
		int time = lastmillis-basetime;
		int fr1 = (int)(time/speed);
		float frac1 = (time-fr1*speed)/speed;
		float frac2 = 1-frac1;
		fr1 = fr1%range+frame;
		int fr2 = fr1+1;
		if(fr2>=frame+range) fr2 = frame;
		vec *verts1 = mverts[fr1];
		vec *verts2 = mverts[fr2];

		for(int *command = glCommands; (*command)!=0;)
		{
			int numVertex = *command++;
			if(numVertex>0) { glBegin(GL_TRIANGLE_STRIP); }
			else            { glBegin(GL_TRIANGLE_FAN); numVertex = -numVertex; };

			loopi(numVertex)
			{
				float tu = *((float*)command++);
				float tv = *((float*)command++);
				glTexCoord2f(tu, tv);
				int vn = *command++;
				vec &v1 = verts1[vn];
				vec &v2 = verts2[vn];
				#define ip(c) v1.c*frac2+v2.c*frac1
				glVertex3f(ip(x), ip(z), ip(y));
			};

			xtraverts += numVertex;

			glEnd();
		};
		
		if(displaylist)
		{
			glEndList();
			displaylistverts = xtraverts-displaylistverts;
		};
	};

    glPopMatrix();
}

hashtable<md2 *> *mdllookup = NULL;
vector<md2 *> mapmodels;
const int FIRSTMDL = 40;

void delayedload(md2 *m)
{ 
    if(!m->loaded)
    {
        sprintf_sd(name1)("packages/models/%s/tris.md2", m->loadname);
        if(!m->load(path(name1))) fatal("loadmodel: ", name1);
        sprintf_sd(name2)("packages/models/%s/skin.jpg", m->loadname);
        int xs, ys;
        installtex(FIRSTMDL+m->mdlnum, path(name2), xs, ys);
        m->loaded = true;
    };
};

int modelnum = 0;

md2 *loadmodel(char *name)
{

    if(!mdllookup) mdllookup = new hashtable<md2 *>;
    md2 **mm = mdllookup->access(name);
    if(mm) return *mm;
    md2 *m = new md2();
    m->mdlnum = modelnum++;
    mapmodelinfo mmi = { 2, 2, 0, 0, "" }; 
    m->mmi = mmi;
    m->loadname = newstring(name);
    mdllookup->access(m->loadname, &m);
   
   return m;
};

void mapmodel(char *rad, char *h, char *zoff, char *snap, char *name)
{
    char *mpdir = "mapmodels/";
    char *lname = new char[ strlen(mpdir) + strlen(name) + 1 ];
    strcpy(lname, mpdir);
    strcat(lname, name);
    md2 *m = loadmodel(lname);
    mapmodelinfo mmi = { atoi(rad), atoi(h), atoi(zoff), atoi(snap), m->loadname };
    m->mmi = mmi;
    mapmodels.add(m);
};

void mapmodelreset() { mapmodels.setsize(0); };

mapmodelinfo &getmminfo(int i) { return i<mapmodels.length() ? mapmodels[i]->mmi : *(mapmodelinfo *)0; };

COMMAND(mapmodel, ARG_5STR);
COMMAND(mapmodelreset, ARG_NONE);

void rendermodel(char *mdl, int frame, int range, int tex, float rad, float x, float y, float z, float yaw, float pitch, bool teammate, float scale, float speed, int snap, int basetime)
{
    md2 *m = loadmodel(mdl); 
    
    //if(isoccluded(player1->o.x, player1->o.y, x-rad, z-rad, rad*2)) return;

    delayedload(m);
    
    int xs, ys;
    glBindTexture(GL_TEXTURE_2D, tex ? lookuptexture(tex, xs, ys) : FIRSTMDL+m->mdlnum);
    
    int ix = (int)x;
    int iy = (int)z;
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
    
/*
    if(teammate)
    {
        light.x *= 0.6f;
        light.y *= 0.7f;
        light.z *= 1.2f;
    };
*/

    m->render(light, frame, range, x, y, z, yaw, pitch, scale, speed, snap, basetime);
};
