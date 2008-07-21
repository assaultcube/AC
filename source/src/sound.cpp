// sound.cpp: uses OpenAL, some code chunks are from devmaster.net and gamedev.net

#include "pch.h"
#include "cube.h"

#ifdef __APPLE__
#include "OpenAL/al.h" 
#include "OpenAL/alc.h" 
#include "Vorbis/vorbisfile.h"
#else
#include "AL/al.h" 
#include "AL/alc.h" 
#include "vorbis/vorbisfile.h"
#endif

VARF(audio, 0, 1, 1, initwarning("audio"));

static bool nosound = true;
static bool sourcesavail = true;
ALCdevice *device = NULL;
ALCcontext *context = NULL;

void alclearerr()
{
    alGetError();
}

bool alerr(bool msg = true)
{
	ALenum er = alGetError();
    if(er && msg) conoutf("\f3OpenAL Error (%X)", er);
    return er > 0;
}

// provides an interface for callbacks.

struct sourceowner
{
    virtual ~sourceowner(){};
    virtual void onsourcereassign(struct source *s) = 0;
};

// represents an OpenAL source, an audio emitter in the 3D world
// available sources are provided by the sourcescheduler

struct source
{
    ALuint id;

    sourceowner *owner;
    bool locked, valid;
    int priority;

    source() : id(0), owner(NULL), locked(false), valid(false), priority(SP_NORMAL)
    {
        valid = generate();
    };

    ~source()
    {
        if(valid) delete_();
    };

    void lock() { locked = true; }

    void unlock() 
    { 
        locked = false; 
        stop();
        buffer(NULL);
    }

    void reset()
    {
        owner = NULL;
        locked = false;
        priority = SP_NORMAL;

        // restore default settings
        stop();
        buffer(NULL);
        gain(1.0f);
        pitch(1.0f);
        position(0.0, 0.0, 0.0);
        sourcerelative(false);
    }

    void init(sourceowner *o)
    {
        ASSERT(o);
        owner = o;
    }

    void onreassign()
    {
        if(owner)
        {
            owner->onsourcereassign(this);
            owner = NULL;
        }
    }

    // OpenAL wrapping methods

    bool generate()
    {
        alclearerr();
        alGenSources(1, &id);
        //ASSERT(id<1000);
        alSourcef(id, AL_REFERENCE_DISTANCE, 1.0f);
        return !alerr(false);
    }

    bool delete_()
    {
        alclearerr();
        alDeleteSources(1, &id);
        return !alerr();
    }

    bool buffer(ALuint buf_id)
    {        
        alclearerr();
        alSourcei(id, AL_BUFFER, buf_id);
        return !alerr();
    }

    bool looping(bool enable)
    {
        alclearerr();
        alSourcei(id, AL_LOOPING, enable ? 1 : 0);
        return !alerr();
    }
        
    bool queuebuffers(ALsizei n, const ALuint *buffer_ids)
    {
        alclearerr();
        alSourceQueueBuffers(id, n, buffer_ids);
        return !alerr();
    }

    bool unqueueallbuffers()
    {
        alclearerr();
        ALint queued;
        alGetSourcei(id, AL_BUFFERS_QUEUED, &queued);
        alerr();
        loopi(queued)
        {
            ALuint buffer;
            alSourceUnqueueBuffers(id, 1, &buffer);
        }
        return !alerr();
    }

    bool gain(float g)
    {
        alclearerr();
        alSourcef(id, AL_GAIN, g);
        return !alerr();
    }

    bool pitch(float p)
    {
        alclearerr();
        alSourcef(id, AL_PITCH, p);
        return !alerr();
    }

    bool position(const vec &pos)
    {
        alclearerr();
        alSourcefv(id, AL_POSITION, (ALfloat *) &pos);
        return !alerr();
    }

    bool position(float x, float y, float z)
    {
        alclearerr();
        alSource3f(id, AL_POSITION, x, y, z);
        return !alerr();
    }

    vec position()
    {
        alclearerr();
        vec p;
        alGetSourcefv(id, AL_POSITION, (ALfloat*) &p);
        if(alerr()) return vec(0,0,0);
        else return p;
    }

    bool sourcerelative(bool enable)
    {
        alclearerr();
        alSourcei(id, AL_SOURCE_RELATIVE, enable ? AL_TRUE : AL_FALSE);
        //alSourcef(id, AL_ROLLOFF_FACTOR, enable ? 1.0f : 0.0f); 
        return !alerr();
    }

    int state()
    {
        ALint s; 
        alGetSourcei(id, AL_SOURCE_STATE, &s);
        return s;
    }    
    
    bool playing()
    {
        return (state() == AL_PLAYING);
    }

    bool play()
    {              
        alclearerr();
        alSourcePlay(id);
        return !alerr();
    }

    bool stop()
    {
        alclearerr();
        alSourceStop(id);
        return !alerr();
    }

    bool rewind()
    {
        alclearerr();
        alSourceRewind(id);
        return !alerr();
    }

    void printposition()
    {
        alclearerr();
        float x, y, z;
        alGetSource3f(id, AL_POSITION, &x, &y, &z);
        ALint s;
        alGetSourcei(id, AL_SOURCE_TYPE, &s);
        conoutf("sound %d: %f\t%f\t%f\t\t%d", id, x, y, z, s);
        alerr();
    }
};


// AC sound scheduler, manages available sound sources
// under load it uses priority and distance information to reassign its resources

struct sourcescheduler
{
    bool sourcesavail;
    vector<source *> sources;

    sourcescheduler()
    {
        sources.reserve(32);
    }

    ~sourcescheduler()
    {
        sources.deletecontentsp();
    }

    void init()
    {
        sourcesavail = true;

        loopi(31)
        {
            source *src = new source();
            if(src->valid) sources.add(src);
            else
            {
                DELETEP(src);
                sourcesavail = false;
                break;
            }
        }
    }

    // returns a free sound source
    // consuming code must call sourcescheduler::releasesource() after use

    source *newsource(int priority, const vec &o)
    {
        source *src = NULL;
        
        if(sources.length())
        {
            // get free item
            loopv(sources) if(!sources[i]->locked)
            {
                src = sources[i];
                break;
            }
        }

        if(!src) // no channels left :(
        {
            src = NULL;
            if(SP_LOW == priority) return NULL;
            loopv(sources) // replace stopped or lower prio sound
            {
                source *s = sources[i];
                if(SP_LOW == s->priority) 
                {
                    src = s;
                    conoutf("ac sound sched: replaced low prio sound"); // FIXME
                    break;
                }
            }
            if(!src)
            {
                float dist = camera1->o.dist(o);
                float score = dist - priority*10.0f;

                source *farthest = NULL;
                float farthestscore = 0.0f;

                loopv(sources) // still no channel, replace far away sounds of same priority
                {
                    source *l = sources[i];
                    if(l->priority <= priority)
                    { 
                        float ldist = camera1->o.dist(l->position());
                        float lscore = ldist - l->priority*10.0f;
                        if(!farthest || lscore > farthestscore)
                        {
                            farthest = l;
                            farthestscore = lscore;
                        }
                    }
                }
                if(farthestscore >= score+5.0f)
                {
                    src = farthest;
                    conoutf("ac sound sched: replaced sound of same prio"); // FIXME
                }
            }

            if(src) src->onreassign(); // inform previous owner about the take-over

        }
        if(!src) 
        {
            conoutf("ac sound sched: sound aborted, no channel takeover possible"); // FIXME
            return NULL;
        }        

        src->reset();       // default settings
        src->lock();        // exclusive lock
        return src;
    }

    void releasesource(source *src)
    {
        ASSERT(src);
        if(!src) return;
        src->unlock();
    }
};

// scheduler instance
sourcescheduler scheduler;


struct oggstream
{
    OggVorbis_File oggfile;
    bool isopen;
    vorbis_info *info;
    ALuint bufferids[2];
    source *src;
    ALenum format;
    ALint laststate;

    string name;
    double totalseconds;
    static const int BUFSIZE = (1024 * 16);
    int startmillis, endmillis, startfademillis, endfademillis;
    float volume, gain;
    bool looping;
 
    oggstream() : isopen(false), src(NULL)
    { 
        reset(); 

        src = scheduler.newsource(SP_HIGH, camera1->o);
        if(src) src->sourcerelative(true);

        alclearerr();
        alGenBuffers(2, bufferids);
        alerr();
    }

    ~oggstream() 
    { 
        reset();

        if(src) scheduler.releasesource(src);
        
        alclearerr();
        alDeleteBuffers(2, bufferids);
        alerr();
    }

    bool open(const char *f)
    {
        if(!f || playing()) return false;
        if(playing() || isopen) reset();

        const char *exts[] = { "", ".wav", ".ogg" };
        string filepath;

        loopi(sizeof(exts)/sizeof(exts[0]))
        {
            s_sprintf(filepath)("packages/audio/songs/%s%s", f, exts[i]);
            FILE *file = fopen(findfile(path(filepath), "rb"), "rb");
            if(!file) continue;

            isopen = !ov_open_callbacks(file, &oggfile, NULL, 0, OV_CALLBACKS_DEFAULT);
            if(!isopen)
            {
                fclose(file);
                continue;
            }

            info = ov_info(&oggfile, -1);
            format = info->channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
            totalseconds = ov_time_total(&oggfile, -1);
            s_strcpy(name, f);

            return true;
        }
        return false;
    }

    bool stream(ALuint bufid)
    {
        loopi(2)
        {
            char pcm[BUFSIZE];
            ALsizei size = 0;
            int bitstream;
            while(size < BUFSIZE)
            {
                long bytes = ov_read(&oggfile, pcm + size, BUFSIZE - size, isbigendian(), 2, 1, &bitstream);
                if(bytes > 0) size += bytes;
                else if (bytes < 0) return false;
                else break; // done
            }
            
            if(size==0)
            {
                if(looping && !ov_pcm_seek(&oggfile, 0)) continue; // try again to replay
                else return false;
            }

            alclearerr();
            alBufferData(bufid, format, pcm, size, info->rate);
            return !alerr();
        }
        return false;
    }

    bool update()
    {
        if(!isopen || !src || !playing()) return false;
        
        // update buffer queue
        ALint processed;
        bool active = true;
        alGetSourcei(src->id, AL_BUFFERS_PROCESSED, &processed);
        loopi(processed)
        {
            ALuint buffer;
            alSourceUnqueueBuffers(src->id, 1, &buffer);
            active = stream(buffer);
            if(active) alSourceQueueBuffers(src->id, 1, &buffer);
        }

        if(active)
        {
            // fade in
            if(startmillis > 0)
            {
                const float start = (lastmillis-startmillis)/(float)startfademillis;
                if(start>=0.00f && start<=1.00001f) setgain(start);
            }

            // fade out
            if(endmillis > 0)
            {
                if(lastmillis>endmillis) // stop
                {
                    active = false;
                }
                else // set gain
                {
                    const float end = (endmillis-lastmillis)/(float)endfademillis;
                    if(end>=-0.00001f && end<=1.00f) setgain(end);
                }
            }
        }

        if(!active) reset();
        return active;
    }


    void reset()
    {
        if(isopen) 
        {
            isopen = !ov_clear(&oggfile);
            totalseconds = 0.0f;

            if(src)
            {
                src->stop();
                src->unqueueallbuffers();
            }
        }
        
        startmillis = endmillis = startfademillis = endfademillis = 0;
        gain = volume = 1.0f;
        looping = false;
        name[0] = '\0';
    }

    bool playing() 
    { 
        if(!src) return false;
        return src->playing();
    }

    void updategain() 
    { 
        ASSERT(src);
        src->gain(gain*volume);
    }

    void setgain(float g)
    { 
        gain = g;
        updategain();
    }

    void setvolume(float v)
    {
        volume = v;
        updategain();
    }

    void fadein(int startmillis, int fademillis)
    {
        setgain(0.01f);
        this->startmillis = startmillis;
        this->startfademillis = fademillis;
    }

    void fadeout(int endmillis, int fademillis)
    {
        this->endmillis = (endmillis || totalseconds > 0.0f) ? endmillis : lastmillis+(int)totalseconds;
        this->endfademillis = fademillis;
    }

    bool playback(bool looping = false)
    {
        if(playing()) return true;
        if(!src || !stream(bufferids[0]) || !stream(bufferids[1])) return false;
        if(startmillis == endmillis == startfademillis == endfademillis == 0) setgain(1.0f);
       
        updategain();
        src->queuebuffers(2, bufferids);
        src->play();

        this->looping = looping;
        return true;
    }

    void seek(double offset)
    {
        if(!totalseconds) return;
        ov_time_seek_page(&oggfile, fmod(totalseconds-5.0f, totalseconds));
    }
};

struct sbuffer
{
    ALuint id;
    string name;

    sbuffer() 
    { 
        name[0] = '\0';
    }

    ~sbuffer()
    {
        unload();
    }

    bool load(char *sound)
    {
        alclearerr();
        alGenBuffers(1, &id);
        if(!alerr())
        {
            const char *exts[] = { "", ".wav", ".ogg" };
            string filepath;
            loopi(sizeof(exts)/sizeof(exts[0]))
            {
                s_sprintf(filepath)("packages/audio/sounds/%s%s", sound, exts[i]);
                const char *file = findfile(path(filepath), "rb");
                size_t len = strlen(filepath);

                if(len >= 4 && !strcasecmp(filepath + len - 4, ".ogg"))
                {
                    FILE *f = fopen(file, "rb");
                    if(!f) continue;

                    OggVorbis_File oggfile;
                    if(!ov_open_callbacks(f, &oggfile, NULL, 0, OV_CALLBACKS_DEFAULT))
                    {
                        vorbis_info *info = ov_info(&oggfile, -1);

                        const size_t BUFSIZE = 32*1024;
                        vector<char> buf;
                        int bitstream;
                        long bytes;

                        do
                        {
                            char buffer[BUFSIZE];
                            bytes = ov_read(&oggfile, buffer, BUFSIZE, 0, 2, 1, &bitstream); // fix endian
                            loopi(bytes) buf.add(buffer[i]);
                        } while(bytes > 0);

                        alBufferData(id, info->channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16, buf.getbuf(), buf.length(), info->rate);
                        ov_clear(&oggfile);
                    }
                    else 
                    {
                        fclose(f);
                        continue;
                    }
                }
                else
                {
                    SDL_AudioSpec wavspec;
                    uint32_t wavlen;
                    uint8_t *wavbuf;

                    if(!SDL_LoadWAV(file, &wavspec, &wavbuf, &wavlen)) continue;
                    
                    ALenum format;
                    switch(wavspec.format) // map wav header to openal format
                    {
                        case AUDIO_U8:
                        case AUDIO_S8:
                            format = wavspec.channels==2 ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8;
                            break;
                        case AUDIO_U16:
                        case AUDIO_S16:
                            format = wavspec.channels==2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
                            break;
                        default:
                            SDL_FreeWAV(wavbuf);
                            return false;
                    }

                    alBufferData(id, format, wavbuf, wavlen, wavspec.freq);
                    SDL_FreeWAV(wavbuf);

                    if(alerr()) break;
                }

                s_strcpy(name, sound);
                return true;
            }
            unload(); // loading failed
        }
        return false;
    }

    void unload()
    {
        alclearerr();
        if(alIsBuffer(id)) alDeleteBuffers(1, &id);
        alerr();
    }
};

oggstream *gamemusic = NULL;


struct soundconfig
{
    sbuffer *buf;
    int vol, uses, maxuses;
    bool loop;

    soundconfig(sbuffer *b, int vol, int maxuses, bool loop)
    {
        buf = b;
        this->vol = vol;
        this->maxuses = maxuses;
        this->loop = loop;
        uses = 0;
    }

    void onattach() { uses++; }
    void ondetach() { uses--; }
};

vector<soundconfig> gamesounds, mapsounds;


// short living sound occurrence, dies once the sound stops
struct location : sourceowner
{
    soundconfig *cfg;
    source *src;

    vec pos;
    physent *p;
    entity *e;

    bool stale;

    location() : cfg(NULL), src(NULL), p(NULL), e(NULL), stale(false)
    {
    }

    ~location() 
    {
        if(e) e->soundinuse = false;
        if(src) scheduler.releasesource(src);
        if(cfg) cfg->ondetach(); 
    };

    bool init(int sound, physent *p = NULL, entity *ent = NULL, const vec *v = NULL, int priority = SP_NORMAL) 
    {
        vector<soundconfig> &sounds = ent ? mapsounds : gamesounds;
        if(!sounds.inrange(sound)) 
        { 
            conoutf("unregistered sound: %d", sound); 
            stale = true;
            return false; 
        }
        
        // get sound config
        cfg = &sounds[sound];
        cfg->onattach();
        if(ent && cfg->maxuses && cfg->uses >= cfg->maxuses) // check max-use limits
        {
            stale = true;
            return false; 
        }

        // assign buffer
        sbuffer *buf = cfg->buf;
        if(!buf) 
        {
            stale = true;
            return false;
        }

        // obtain source
        src = scheduler.newsource(priority, p ? p->o : ent ? vec(ent->x, ent->y, ent->z) : v ? *v : camera1->o);
        // apply configuration
        if(!src || !src->valid || !src->buffer(cfg->buf->id) || !src->looping(cfg->loop) || !src->gain(cfg->vol/100.0f))
        {
            stale = true;
            return false;
        }
        src->init(this);
        
        // set position
        if(p) attachtoworldobj(p);
        else if(ent) attachtoworldobj(ent);
        else if(v) attachtoworldobj(v);
        else attachtoworldobj(camera1);

        return true;
    }

    void attachtoworldobj(physent *d)
    {
        ASSERT(!stale);
        if(stale) return;

        p = d;
        e = NULL;
        if(p==camera1) src->sourcerelative(true);
    }

    void attachtoworldobj(const vec *v)
    {
        ASSERT(!stale);
        if(stale) return;

        p = NULL;
        e = NULL;
        pos.x = v->x;
        pos.y = v->y;
        pos.z = v->z;
        src->sourcerelative(false);
    }

    void attachtoworldobj(entity *ent)
    {
        ASSERT(!stale);
        if(stale) return;

        e = ent;
        e->soundinuse = true;
        p = NULL;
        if(e->attr2) // radius set
        {
            src->sourcerelative(true);
        }
    }

    void play()
    {
        if(stale) return;

        updatepos();
        src->play();
    }

    void update()
    {
        if(stale) return;

        int s = src->state();

        switch(s)
        {
            case AL_PLAYING:
                updatepos();
                break;
            case AL_STOPPED:
            case AL_PAUSED:
            case AL_INITIAL:
                stale = true;                
                break;
        }
    }

    void updatepos()
    {
        ASSERT(!stale);
        if(stale) return;

        if(p) // players
        {
            if(p!=camera1) src->position(p->o);
        }
        else if(e) // entities
        {
            if(e->attr2)
            {
                // own distance model for entities/mapsounds: linear & clamping
                float dist = camera1->o.dist(vec(e->x, e->y, e->z));
                if(dist <= e->attr3) src->gain(1.0f);
                else if(dist <= e->attr2) src->gain(1.0f - dist/(float)e->attr2);
                else src->gain(0.0f);
            }
            else src->position(e->x, e->y, e->z);
        }
        else src->position(pos); // static stuff
    }

    void onsourcereassign(source *s)
    {
        if(s==src) stale = true; // mark for deletion if source got lost
    }
};

struct locvector : vector<location *>
{
    virtual ~locvector(){};

    int find(int sound, physent *p)
    { 
        loopi(ulen) if(buf[i] && buf[i]->p == p && buf[i]->cfg == &gamesounds[sound]) return i;
        return -1;
    }

    void delete_(int i)
    {
        location *loc = buf[i];
        remove(i);
        delete loc;
    }

    // give owner's locations a static position
    void detachfromplayer(playerent *owner)
    {
        if(!owner) return;
        loopv(*this)
        {
            location *l = buf[i];
            if(!l || l->p != owner) continue;
            l->attachtoworldobj(&l->p->o);
        }
    }

    // update stuff, remove stale data
    void updatelocations()
    {
        loopv(*this)
        {
            location *l = buf[i];
            if(!l) continue;

            l->update();
            if(l->stale) delete_(i--);
        }
    }

    // force pitch across all locations
    void forcepitch(float pitch)
    {
        loopv(*this) 
        {
            location *l = buf[i];
            if(!l) continue;

            if(l->src && l->src->locked) l->src->pitch(pitch);
        }
    }
};

locvector locations;


struct bufferhashtable : hashtable<char *, sbuffer>
{
    virtual ~bufferhashtable() {};

    // find or load data
    virtual sbuffer *find(char *name)
    {
        sbuffer *b = access(name);
        if(!b)
        {
            char *n = newstring(name);
            b = &(*this)[n];
            if(!b->load(name))
            {
                remove(name);
                DELETEA(n);
                b = NULL;
            }
        }
        return b;
    }
};

bufferhashtable bufferpool;


VARFP(soundvol, 0, 128, 255,
{
    alListenerf(AL_GAIN, soundvol/255.0f);
});

void setmusicvol() 
{ 
    extern int musicvol; 
    if(gamemusic) gamemusic->setvolume(musicvol > 0 ? musicvol/255.0f : 0);
}
VARFP(musicvol, 0, 64, 255, setmusicvol());

char *musicdonecmd = NULL;

void stopsound()
{
    if(nosound) return;
    DELETEA(musicdonecmd);
    if(gamemusic) gamemusic->reset();   
}

void initsound()
{
    if(!audio)
    {
        conoutf("audio is disabled");
        return;
    }

    nosound = true;
    device = NULL;
    context = NULL;

    // list available devices                    
    if(alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT"))
    {
        const ALCchar *devices = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
        if(devices)
        {
            string d;
            s_strcpy(d, "Audio devices:");

            // null separated device string
            for(const ALchar *c = devices; c[strlen(c)+1]; c += strlen(c)+1)
            {
                s_sprintf(d)("%s%c%s", d, c==devices ? ' ' : ',' , c);
            }
            conoutf(d);
        }
    }

    // open device
    const char *devicename = getalias("openaldevice");
    device = alcOpenDevice(devicename && devicename[0] ? devicename : NULL);

    if(device)
    {
        context = alcCreateContext(device, NULL);
        if(context)
        {
            alcMakeContextCurrent(context);
            alDistanceModel(AL_EXPONENT_DISTANCE_CLAMPED);
            
            conoutf("Sound: %s (%s)", alGetString(AL_RENDERER), alGetString(AL_VENDOR));
            conoutf("Driver: %s", alGetString(AL_VERSION));

            scheduler.init();
            gamemusic = new oggstream();

            nosound = false;
        }
    }

    if(nosound)
    {
        alerr();
        if(context) alcDestroyContext(context);
        if(device) alcCloseDevice(device);
        conoutf("sound initialization failed!");
    }
}

cvector musics;

void music(char *name, char *millis, char *cmd)
{
    if(nosound) return;
    stopsound();
    if(musicvol && *name)
    {
        if(cmd[0]) musicdonecmd = newstring(cmd);

        if(gamemusic->open(name))
        {
            // fade
            if(atoi(millis) > 0)
            {
                const int fadetime = 1000;
                gamemusic->fadein(lastmillis, fadetime);
                gamemusic->fadeout(lastmillis+atoi(millis), fadetime);
            }

            // play
            bool loop = cmd && cmd[0];
            if(!gamemusic->playback(loop))
            {
                conoutf("could not play music: %s", name);
            }
            setmusicvol();
        }
        else conoutf("could not open music: %s", name);
    }
}

void musicsuggest(int id, int millis, bool rndofs) // play bg music if nothing else is playing
{
    if(nosound || !gamemusic) return;
    if(gamemusic->playing()) return;

    if(!musics.inrange(id))
    {
        conoutf("\f3music %d not registered", id);
        return;
    }
    char *name = musics[id];
    if(gamemusic->open(name))
    {
        gamemusic->fadein(lastmillis, 1000);
        gamemusic->fadeout(millis ? lastmillis+millis : 0, 1000);
        if(rndofs) gamemusic->seek(millis ? (double)rnd(millis)/2.0f : (double)lastmillis);
        if(!gamemusic->playback(rndofs)) conoutf("could not play music: %s", name);
    }
    else conoutf("could not open music: %s", name);
}

void musicfadeout(int id)
{
    if(!gamemusic->playing() || !musics.inrange(id)) return;
    if(!strcmp(musics[id], gamemusic->name)) gamemusic->fadeout(lastmillis+1000, 1000);
}

void registermusic(char *name)
{
    if(!name || !name[0]) return;
    musics.add(newstring(name));
}

COMMAND(registermusic, ARG_1STR);
COMMAND(music, ARG_3STR);

int findsound(char *name, int vol, vector<soundconfig> &sounds)
{
    loopv(sounds)
    {
        if(!strcmp(sounds[i].buf->name, name) && (!vol || sounds[i].vol==vol)) return i;
    }
    return -1;
}

int addsound(char *name, int vol, int maxuses, bool loop, vector<soundconfig> &sounds)
{
    sbuffer *b = bufferpool.find(name);
    if(!b)
    {
        conoutf("\f3failed to load sample %s", name);
        return -1;
    }

    soundconfig s(b, vol > 0 ? vol : 100, maxuses, loop);
    sounds.add(s);
    return sounds.length()-1;
}

void registersound(char *name, char *vol, char *loop) { addsound(name, atoi(vol), 0, atoi(loop) != 0, gamesounds); }
COMMAND(registersound, ARG_4STR);

void mapsound(char *name, char *vol, char *maxuses, char *loop) { addsound(name, atoi(vol), atoi(maxuses), atoi(loop) != 0, mapsounds); }
COMMAND(mapsound, ARG_4STR);

void soundcleanup()
{
    if(nosound) return;

    stopsound();
    //gamesounds.setsizenodelete(0);
    gamesounds.setsize(0);

    clearsounds();
    
    alcMakeContextCurrent(NULL);
    if(context) alcDestroyContext(context);
    if(device) alcCloseDevice(device);
}

void clearsounds()
{
    if(gamemusic) gamemusic->reset();
    //mapsounds.setsizenodelete(0);
    //locations.setsize(0);
    mapsounds.setsize(0);
    locations.deletecontentsp();
}

VAR(footsteps, 0, 1, 1);

int lastpitch = 1.0f;

void updateplayerfootsteps(playerent *p, int sound)
{
    if(!p) return;

    const int footstepradius = 16;
    int locid = locations.find(sound, p);
    location *loc = locid < 0 ? NULL : locations[locid];
    bool local = (p == camera1);

    bool inrange = (local || (camera1->o.dist(p->o) < footstepradius && footsteps));
    bool nosteps = (!footsteps || p->state != CS_ALIVE || lastmillis-p->lastpain < 300 || (!p->onfloor && p->timeinair>50) || (!p->move && !p->strafe) || (sound==S_FOOTSTEPS && p->crouching) || (sound==S_FOOTSTEPSCROUCH && !p->crouching) || p->inwater);

    if(inrange && !nosteps)
    {
        if(!loc) playsound(sound, p, NULL, NULL, local ? SP_HIGH : SP_LOW);
    }
    else if(loc) 
    {
        locations.delete_(locid);
    }
}

void updateloopsound(int sound, bool active, float vol = 1.0f)
{
    if(camera1->type != ENT_PLAYER) return;
    int locid = locations.find(sound, camera1);
    location *l = locid < 0 ? NULL : locations[locid];
    if(!l && active) playsound(sound, NULL, NULL, NULL, SP_HIGH);
    else if(l && !active)
    {
        locations.delete_(locid);
    }
    if(l && vol != 1.0f) l->src->gain(vol);
}

void updateaudio()
{
    alcSuspendContext(context); // don't process sounds while we mess around
    
    bool alive = player1->state!=CS_DEAD; 
    bool firstperson = camera1->type==ENT_PLAYER;


    // footsteps
    updateplayerfootsteps(player1, S_FOOTSTEPS); 
    updateplayerfootsteps(player1, S_FOOTSTEPSCROUCH);
    loopv(players)
    {
        playerent *p = players[i];
        if(!p) continue;
        updateplayerfootsteps(p, S_FOOTSTEPS);
        updateplayerfootsteps(p, S_FOOTSTEPSCROUCH);
    }

    // water
    updateloopsound(S_UNDERWATER, alive && player1->inwater);
    
    // tinnitus
    bool tinnitus = alive && player1->eardamagemillis>0 && lastmillis<=player1->eardamagemillis;
    float tinnitusvol = tinnitus && player1->eardamagemillis-lastmillis<=1000 ? (player1->eardamagemillis-lastmillis)/1000.0f : 1.0f;
    updateloopsound(S_TINNITUS, tinnitus, tinnitusvol);

    if(alive && firstperson)
    {
         // set lower pitch if "player's ear got damaged"
        float pitch = tinnitus ? 0.65 : 1.0f;
        if(pitch!=lastpitch)
        {
            locations.forcepitch(pitch);
            lastpitch = pitch;
        }
    }

    // update map sounds
    loopv(ents)
    {
        entity &e = ents[i];
        vec o(e.x, e.y, e.z);
        if(e.type!=SOUND || e.soundinuse || camera1->o.dist(o)>=e.attr2) continue;
        playsound(e.attr1, NULL, &e);
    }

    // update all sound locations
    locations.updatelocations();

    // update background music
    if(gamemusic)
    {
        if(!gamemusic->update())
        {
            // music ended, exec command
            if(musicdonecmd)
            {
                char *cmd = musicdonecmd;
                musicdonecmd = NULL;
                execute(cmd);
                delete[] cmd;
            }
        }
    }

    // listener
    vec o[2];
    o[0].x = (float)(cosf(RAD*(camera1->yaw-90)));
    o[0].y = (float)(sinf(RAD*(camera1->yaw-90)));
    o[0].z = 0.0f;
    o[1].x = o[1].y = 0.0f;
    o[1].z = -1.0f;
    alListenerfv(AL_ORIENTATION, (ALfloat *) &o);
    alListenerfv(AL_POSITION, (ALfloat *) &camera1->o);

    alcProcessContext(context);
}

VARP(maxsoundsatonce, 0, 40, 100);

void playsound(int n, physent *p, entity *ent, const vec *v, int priority)
{
    if(nosound || !soundvol) return;

    // avoid bursts of sounds with heavy packetloss and in sp
    if(!ent)
    {
        static int soundsatonce = 0, lastsoundmillis = 0;
        if(totalmillis==lastsoundmillis) soundsatonce++; else soundsatonce = 1;
        lastsoundmillis = totalmillis;
        if(maxsoundsatonce && soundsatonce>maxsoundsatonce) return;
    }

    location *loc = new location();
    loc->init(n, p, ent, v, priority);
    loc->play();
    locations.add(loc);
}

void playsound(int n, int priority) { playsound(n, NULL, NULL, NULL, priority); };
void playsound(int n, physent *p, int priority) { playsound(n, p, NULL, NULL, priority); };
void playsound(int n, entity *e, int priority) { playsound(n, NULL, e, NULL, priority); };
void playsound(int n, const vec *v, int priority) { playsound(n, NULL, NULL, v, priority); };

void playsoundname(char *s, const vec *loc, int vol) 
{ 
    if(vol <= 0) vol = 100;
    int id = findsound(s, vol, gamesounds);
    if(id < 0) id = addsound(s, vol, 0, false, gamesounds);
    playsound(id, loc);
}

void sound(int n) { playsound(n); }
COMMAND(sound, ARG_1INT);

void playsoundc(int n, physent *p)
{ 
    if(p && p!=player1) playsound(n, p);
    else
    {
        addmsg(SV_SOUND, "i", n);
        playsound(n);
    }
}

void voicecom(char *sound, char *text)
{
    if(!sound || !sound[0]) return;
    static int last = 0;
    if(!last || lastmillis-last > 2000)
    {
        s_sprintfd(soundpath)("voicecom/%s", sound);
        int s = findsound(soundpath, 0, gamesounds);
        if(s < 0 || s < S_AFFIRMATIVE || s > S_NICESHOT) return;
        playsound(s, SP_HIGH);
        if(s == S_NICESHOT) // public
        {
            addmsg(SV_VOICECOM, "ri", s);
            toserver(text);
        }
        else // team
        {
            addmsg(SV_VOICECOMTEAM, "ri", s);
            s_sprintfd(teamtext)("%c%s", '%', text);
            toserver(teamtext);
        }
        last = lastmillis;
    }
}

COMMAND(voicecom, ARG_2STR);

void detachsounds(playerent *owner)
{
    locations.detachfromplayer(owner);
}

void soundtest()
{
    loopi(S_NULL) playsound(i, rnd(SP_HIGH+1));
}

COMMAND(soundtest, ARG_NONE);
