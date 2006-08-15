// rendermd2.cpp: loader code adapted from a nehe tutorial

#include "cube.h"

VARP(animationinterpolationtime, 0, 150, 1000);

struct md2_anpos
{
    int fr1, fr2;
    float frac1, frac2;
            
    void setframes(md2state &a)
    {
		int time = lastmillis-a.basetime;
		fr1 = (int)(time/a.speed);
		frac1 = (time-fr1*a.speed)/a.speed;
		frac2 = 1-frac1;
        //if(a.anim&ANIM_LOOP) // FIXME
        {
		    fr1 = fr1%a.range+a.frame;
		    fr2 = fr1+1;
		    if(fr2>=a.frame+a.range) fr2 = a.frame;
        }
/*        else
        {
            fr1 = min(fr1, a.range-1)+a.frame;
            fr2 = min(fr1+1, a.frame+a.range-1);
        };
        if(a.anim&ANIM_REVERSE)
        {
            fr1 = (a.frame+a.range-1)-(fr1-a.frame);
            fr2 = (a.frame+a.range-1)-(fr2-a.frame);
        };*/
	};
};

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
    void render(vec &light, int numFrame, int range, float x, float y, float z, float yaw, float pitch, float scale, float speed, int snap, int basetime, dynent *d=NULL);
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

void md2::render(vec &light, int frame, int range, float x, float y, float z, float yaw, float pitch, float sc, float speed, int snap, int basetime, dynent *d)
{
	if(range==0 || frame+range-1>numFrames)
	{
		conoutf("invalid md2 frame or range!");
		return;
	}

	md2state ai;
	ai.frame = frame;
	ai.basetime = basetime;
	ai.range = range;
	ai.speed = speed;

    loopi(ai.range) if(!mverts[ai.frame+i]) scale(ai.frame+i, sc, snap);

	if(d)
    {
        if(d->lastanimswitchtime==-1) { d->current = ai; d->lastanimswitchtime = lastmillis-animationinterpolationtime*2; }
        else if(d->current != ai)
        {
            if(lastmillis-d->lastanimswitchtime>animationinterpolationtime/2) d->prev = d->current;
            d->current = ai;
            d->lastanimswitchtime = lastmillis;
        };
    };
    
    glPushMatrix ();
    glTranslatef(x, y, z);
    glRotatef(yaw+180, 0, -1, 0);
    glRotatef(pitch, 0, 0, 1);
    
	glColor3fv((float *)&light);

    if(displaylist && ai.frame==0 && ai.range==1)
    {
		glCallList(displaylist);
		xtraverts += displaylistverts;
    }
    else
    {
		if(ai.frame==0 && ai.range==1)
		{
			static int displaylistn = 10;
			glNewList(displaylist = displaylistn++, GL_COMPILE);
			displaylistverts = xtraverts;
		};
		
		md2_anpos prev, current;
        current.setframes(d ? d->current : ai);
#ifdef _DEBUG
		prev.setframes(ai);
#endif
		vec *verts1 = mverts[current.fr1], *verts2 = mverts[current.fr2], *verts1p, *verts2p;
		float aifrac1, aifrac2;
		bool doai = d && lastmillis-d->lastanimswitchtime<animationinterpolationtime;
		if(doai)
		{
		    prev.setframes(d->prev);
		    verts1p = mverts[prev.fr1];
		    verts2p = mverts[prev.fr2];
		    aifrac1 = (lastmillis-d->lastanimswitchtime)/(float)animationinterpolationtime;
		    aifrac2 = 1-aifrac1;
		};

		if(doai) printf("(%i %f %i) %f (%i %f %i)\n", prev.fr1, prev.frac1, prev.fr2, aifrac1, current.fr1, current.frac1, current.fr2);

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
				#define ip(v1, v2, c)  (v1.c*current.frac2+v2.c*current.frac1)
				#define ipv(v1, v2, c) (v1 ## p.c*prev.frac2+v2 ## p.c*prev.frac1)
				#define ipa(v1, v2, c) (ip(v1, v2, c)*aifrac1+ipv(v1, v2, c)*aifrac2)
				if(doai)
				{
				    vec &v1p = verts1p[vn], &v2p = verts2p[vn];
				    glVertex3f(ipa(v1, v2, x), ipa(v1, v2, z), ipa(v1, v2, y));
				}
				else glVertex3f(ip(v1, v2, x), ip(v1, v2, z), ip(v1, v2, y));
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

void rendermodel(char *mdl, int frame, int range, int tex, float rad, float x, float y, float z, float yaw, float pitch, bool teammate, float scale, float speed, int snap, int basetime, bool oculling, dynent *d)
{
    md2 *m = loadmodel(mdl); 
    
    if(oculling && isoccluded(player1->o.x, player1->o.y, x-rad, z-rad, rad*2)) return;

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

    m->render(light, frame, range, x, y, z, yaw, pitch, scale, speed, snap, basetime, d);
};
