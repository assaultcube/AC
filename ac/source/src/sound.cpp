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

VARF(audio, 0, 1, 1, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARP(audiodebug, 0, 0, 1);
VARP(gainscale, 1, 100, 100);

VAR(al_referencedistance, 0, 400, 1000000);
VAR(al_rollofffactor, 0, 70, 1000000);

static bool nosound = true;
ALCdevice *device = NULL;
ALCcontext *context = NULL;

struct location *playsound(int n, const struct worldobjreference &r, int priority, float offset = 0.0f);

void alclearerr()
{
    alGetError();
}

bool alerr(bool msg = true, int line = 0)
{
	ALenum er = alGetError();
	if(er && msg) 
	{
		const char *desc = "unknown";
		switch(er)
		{
			case AL_INVALID_NAME: desc = "invalid name"; break;
			case AL_INVALID_ENUM: desc = "invalid enum"; break;
			case AL_INVALID_VALUE: desc = "invalid value"; break;
			case AL_INVALID_OPERATION: desc = "invalid operation"; break;
			case AL_OUT_OF_MEMORY: desc = "out of memory"; break;
		}
		if(line) conoutf("\f3OpenAL Error (%X): %s, line %d", er, desc, line);
		else conoutf("\f3OpenAL Error (%X): %s", er, desc);
	}	
    return er > 0;
}

#define ALERR alerr(true, __LINE__)

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
        ASSERT(!valid || alIsSource(id));
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
        ASSERT(alIsSource(id));

        owner = NULL;
        locked = false;
        priority = SP_NORMAL;

        // restore default settings
        
        stop();
        buffer(NULL);

        pitch(1.0f);
        gain(1.0f);
        position(0.0f, 0.0f, 0.0f);
        velocity(0.0f, 0.0f, 0.0f);
        
        looping(false);
        sourcerelative(false);        

        // fit into distance model
        alSourcef(id, AL_REFERENCE_DISTANCE, al_referencedistance/100.0f);
        alSourcef(id, AL_ROLLOFF_FACTOR, al_rollofffactor/100.0f);
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

        return !alerr(false);
    }

    bool delete_()
    {
        alclearerr();
        alDeleteSources(1, &id);
        return !ALERR;
    }

    bool buffer(ALuint buf_id)
    {        
        alclearerr();
        alSourcei(id, AL_BUFFER, buf_id);
        return !ALERR;
    }

    bool looping(bool enable)
    {
        alclearerr();
        alSourcei(id, AL_LOOPING, enable ? 1 : 0);
        return !ALERR;
    }
        
    bool queuebuffers(ALsizei n, const ALuint *buffer_ids)
    {
        alclearerr();
        alSourceQueueBuffers(id, n, buffer_ids);
        return !ALERR;
    }

    bool unqueueallbuffers()
    {
        alclearerr();
        ALint queued;
        alGetSourcei(id, AL_BUFFERS_QUEUED, &queued);
        ALERR;
        loopi(queued)
        {
            ALuint buffer;
            alSourceUnqueueBuffers(id, 1, &buffer);
        }
        return !ALERR;
    }

    bool gain(float g)
    {
        alclearerr();
        alSourcef(id, AL_GAIN, g);
        return !ALERR;
    }

    bool pitch(float p)
    {
        alclearerr();
        alSourcef(id, AL_PITCH, p);
        return !ALERR;
    }

    bool position(const vec &pos)
    {
        alclearerr();
        alSourcefv(id, AL_POSITION, (ALfloat *) &pos);
        return !ALERR;
    }

    bool position(float x, float y, float z)
    {
        alclearerr();
        alSource3f(id, AL_POSITION, x, y, z);
        return !ALERR;
    }

    bool velocity(float x, float y, float z)
    {
        alclearerr();
        alSource3f(id, AL_VELOCITY, x, y, z);
        return !ALERR;
    }

    vec position()
    {
        alclearerr();
        vec p;
        alGetSourcefv(id, AL_POSITION, (ALfloat*) &p);
        if(ALERR) return vec(0,0,0);
        else return p;
    }

    bool sourcerelative(bool enable)
    {
        alclearerr();
        alSourcei(id, AL_SOURCE_RELATIVE, enable ? AL_TRUE : AL_FALSE);
        return !ALERR;
    }

    int state()
    {
        ALint s; 
        alGetSourcei(id, AL_SOURCE_STATE, &s);
        return s;
    }    

    bool secoffset(float secs)
    {
        alclearerr();
        alSourcef(id, AL_SEC_OFFSET, secs);
        return !ALERR;
    }

    float secoffset()
    {
        alclearerr();
        ALfloat s;
        alGetSourcef(id, AL_SEC_OFFSET, &s);
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
        return !ALERR;
    }

    bool stop()
    {
        alclearerr();
        alSourceStop(id);
        return !ALERR;
    }

    bool rewind()
    {
        alclearerr();
        alSourceRewind(id);
        return !ALERR;
    }

    void printposition()
    {
        alclearerr();
        vec v = position();
        ALint s;
        alGetSourcei(id, AL_SOURCE_TYPE, &s);
        conoutf("sound %d: pos(%f,%f,%f) t(%d) ", id, v.x, v.y, v.z, s);
        ALERR;
    }
};

VARP(soundschedpriorityscore, 0, 100, 1000);
VARP(soundscheddistancescore, 0, 5, 1000);
VARP(soundschedoldbonus, 0, 100, 1000);

// AC sound scheduler, manages available sound sources
// under load it uses priority and distance information to reassign its resources

extern int soundchannels;

struct sourcescheduler
{
    vector<source *> sources;

    sourcescheduler() {}

    void init()
    {
        int newchannels = soundchannels - sources.length();
        if(newchannels < 0)
        {
            loopv(sources)
            {
                source *src = sources[i];
                if(src->locked) continue;
                sources.remove(i--);
                delete src;
                if(sources.length() <= soundchannels) break;
            }
        }
        else loopi(newchannels)
        {
            source *src = new source();
            if(src->valid) sources.add(src);
            else
            {
                DELETEP(src);
                break;
            }
        }

    }

    void reset()
    {
        loopv(sources) sources[i]->reset();
        sources.deletecontentsp();
    }

    // returns a free sound source (channel)
    // consuming code must call sourcescheduler::releasesource() after use

    source *newsource(int priority, const vec &o)
    {
        source *src = NULL;

        if(sources.length())
        {
            // search unused source
            loopv(sources) if(!sources[i]->locked)
            {
                src = sources[i];
                break;
            }
        }

        if(SP_LOW==priority) return NULL; // low priority sounds can't replace others

        if(!src) // try replacing a used source
        {
            // score our sound
            const float dist = o.iszero() ? 0.0f : camera1->o.dist(o);
            const float score = (priority*soundschedpriorityscore) - (dist*soundscheddistancescore);

            // score other sounds
            float worstscore = 0.0f;
            source *worstsource = NULL;

            loopv(sources)
            {
                source *s = sources[i];
                if(s->priority==SP_HIGHEST) continue; // highest priority sounds can't be replaced
                
                vec otherpos = s->position();
                float otherdist = otherpos.iszero() ? 0.0f : camera1->o.dist(otherpos);
                float otherscore = (s->priority*soundschedpriorityscore) - (otherdist*soundscheddistancescore) + soundschedoldbonus;
                if(!worstsource || otherscore < worstscore)
                {
                    worstsource = s;
                    worstscore = otherscore;
                }
            }

            // pick worst source and replace it
            if(worstsource && score>worstscore)
            {
                src = worstsource;
                if(audiodebug) conoutf("ac sound sched: replaced sound of same prio");
                src->onreassign(); // inform previous owner about the take-over
            }
        }

        if(!src) 
        {
            if(audiodebug) conoutf("ac sound sched: sound aborted, no channel takeover possible");
            return NULL;
        }

        src->reset();       // default settings
        src->lock();        // exclusive lock
        src->priority = priority;
        return src;
    }

    // give source back to the pool
    void releasesource(source *src)
    {
        ASSERT(src);
        ASSERT(src->locked); // detect double release

        if(!src) return;
        src->unlock();

        if(sources.length() > soundchannels)
        {
            sources.removeobj(src);
            delete src;
        }
    }
};

// scheduler instance
sourcescheduler scheduler;

VARF(soundchannels, 4, 32, 1024, { if(!nosound) scheduler.init(); });

// binding of sounds to the 3D world

struct worldobjreference
{
    enum worldobjtype { WR_CAMERA, WR_PHYSENT, WR_ENTITY, WR_STATICPOS };
    int type;

    worldobjreference(int t) : type(t) {};
    virtual ~worldobjreference() {};
    virtual worldobjreference *clone() const = 0;
    virtual const vec &currentposition() const = 0;
    virtual bool nodistance() = 0;
    virtual bool operator==(const worldobjreference &other) = 0;
    virtual bool operator!=(const worldobjreference &other) { return !(*this==other); }
    virtual void attach() {}
    virtual void detach() {}
};

struct camerareference : worldobjreference
{
    camerareference() : worldobjreference(WR_CAMERA) {};
    worldobjreference *clone() const { return new camerareference(*this); }
    const vec &currentposition() const { return camera1->o; }
    bool nodistance() { return true; }
    bool operator==(const worldobjreference &other) { return type==other.type; }
};

struct physentreference : worldobjreference
{
    physent *phys;

    physentreference(physent *ref) : worldobjreference(WR_PHYSENT)
    {
        ASSERT(ref);
        phys = ref;
    }
    
    worldobjreference *clone() const { return new physentreference(*this); }
    const vec &currentposition() const { return phys->o; }
    bool nodistance() { return phys==camera1; }
    bool operator==(const worldobjreference &other) { return type==other.type && phys==((physentreference &)other).phys; }
};

struct entityreference : worldobjreference
{
    entity *ent;

    entityreference(entity *ref) : worldobjreference(WR_ENTITY)
    {
        ASSERT(ref);
        ent = ref;
    }

    worldobjreference *clone() const { return new entityreference(*this); }

    const vec &currentposition() const
    {
        static vec tmp = vec(ent->x, ent->y, ent->z);
        return tmp;
    }

    bool nodistance() { return ent->attr2>0; }
    bool operator==(const worldobjreference &other) { return type==other.type && ent==((entityreference &)other).ent; }
};

struct staticreference : worldobjreference
{
    vec pos;

    staticreference(const vec &ref) : worldobjreference(WR_STATICPOS)
    {
        pos = ref;
    }

    worldobjreference *clone() const { return new staticreference(*this); }
    const vec &currentposition() const { return pos; }
    bool nodistance() { return false; }
    bool operator==(const worldobjreference &other) { return type==other.type && pos==((staticreference &)other).pos; }
};


static int oggseek(FILE *f, ogg_int64_t off, int whence)
{
    return f ? fseek(f, off, whence) : -1;
}

static ov_callbacks oggcallbacks = 
{
    (size_t (*)(void *, size_t, size_t, void *))  fread,
    (int (*)(void *, ogg_int64_t, int))           oggseek,
    (int (*)(void *))                             fclose,
    (long (*)(void *))                            ftell
};

// audio streaming for background music

struct oggstream : sourceowner
{
    string name;
    bool valid;

    // file stream
    OggVorbis_File oggfile;
    bool isopen;
    vorbis_info *info;
    double totalseconds;
    static const int BUFSIZE = 1024 * 512; // 512kb buffer

    // OpenAL resources
    ALuint bufferids[2];
    source *src;
    ALenum format;

    // settings
    float volume, gain;
    int startmillis, endmillis, startfademillis, endfademillis;
    bool looping;
 
    oggstream() : valid(false), isopen(false), src(NULL)
    { 
        reset();

        // grab a source and keep it during the whole lifetime
        src = scheduler.newsource(SP_HIGHEST, camera1->o);
        if(src)
        {
            if(src->valid)
            {
                src->init(this);
                src->sourcerelative(true);
            }
            else
            {
                scheduler.releasesource(src);
                src = NULL;
            }
        }
        
        if(!src) return;

        alclearerr();
        alGenBuffers(2, bufferids);
        valid = !ALERR;
    }

    ~oggstream() 
    { 
        reset();

        if(src) scheduler.releasesource(src);
        
        if(alIsBuffer(bufferids[0]) || alIsBuffer(bufferids[1]))
        {
            alclearerr();
            alDeleteBuffers(2, bufferids);
            ALERR;
        }
    }

    void reset()
    {
        name[0] = '\0';

        // stop playing
        if(src)
        {
            src->stop();
            src->unqueueallbuffers();
            src->buffer(NULL);
        }
        format = AL_NONE;

        // reset file handler
        if(isopen) 
        {
            isopen = !ov_clear(&oggfile);
        }
        info = NULL;
        totalseconds = 0.0f;
        
        // default settings
        startmillis = endmillis = startfademillis = endfademillis = 0;
        gain = 1.0f; // reset gain but not volume setting
        looping = false;
    }

    bool open(const char *f)
    {
        ASSERT(valid);
        if(!f) return false;
        if(playing() || isopen) reset(); 

        const char *exts[] = { "", ".wav", ".ogg" };
        string filepath;

        loopi(sizeof(exts)/sizeof(exts[0]))
        {
            s_sprintf(filepath)("packages/audio/songs/%s%s", f, exts[i]);
            FILE *file = fopen(findfile(path(filepath), "rb"), "rb");
            if(!file) continue;

            isopen = !ov_open_callbacks(file, &oggfile, NULL, 0, oggcallbacks);
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

    void onsourcereassign(source *s)
    {
        // should NEVER happen because streams do have the highest priority, see constructor
        ASSERT(0);
        if(src && src==s)
        {
            reset();
            src = NULL;
        }
    }

    bool stream(ALuint bufid)
    {
        ASSERT(valid);

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
            return !ALERR;
        }
        
        return false;
    }

    bool update()
    {
        ASSERT(valid);
        if(!isopen || !playing()) return false;
        
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
                if(start>=0.00f && start<=1.00001f)
                {
                    setgain(start);
                    return true;
                }
            }

            // fade out
            if(endmillis > 0)
            {
                if(lastmillis<=endmillis) // set gain
                {
                    const float end = (endmillis-lastmillis)/(float)endfademillis;
                    if(end>=-0.00001f && end<=1.00f)
                    {
                        setgain(end);
                        return true;
                    }
                }
                else  // stop
                {
                    active = false;
                }
            }
        }

        if(!active) reset(); // reset stream if the end is reached
        return active;
    }

    bool playing() 
    { 
        ASSERT(valid);
        return src->playing();
    }

    void updategain() 
    { 
        ASSERT(valid);
        src->gain(gain*volume);
    }

    void setgain(float g)
    { 
        ASSERT(valid);
        gain = g;
        updategain();
    }

    void setvolume(float v)
    {
        ASSERT(valid);
        volume = v;
        updategain();
    }

    void fadein(int startmillis, int fademillis)
    {
        ASSERT(valid);
        setgain(0.01f);
        this->startmillis = startmillis;
        this->startfademillis = fademillis;
    }

    void fadeout(int endmillis, int fademillis)
    {
        ASSERT(valid);
        this->endmillis = (endmillis || totalseconds > 0.0f) ? endmillis : lastmillis+(int)totalseconds;
        this->endfademillis = fademillis;
    }

    bool playback(bool looping = false)
    {
        ASSERT(valid);
        if(playing()) return true;
        this->looping = looping;
        if(!stream(bufferids[0]) || !stream(bufferids[1])) return false;
        if(!startmillis && !endmillis && !startfademillis && !endfademillis) setgain(1.0f);
       
        updategain();
        src->queuebuffers(2, bufferids);
        src->play();

        return true;
    }

    void seek(double offset)
    {
        ASSERT(valid);
        if(!totalseconds) return;
        ov_time_seek_page(&oggfile, fmod(totalseconds-5.0f, totalseconds));
    }
};

struct sbuffer
{
    ALuint id;
    string name;

    sbuffer() : id(0) 
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
        if(!ALERR)
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
                    if(!ov_open_callbacks(f, &oggfile, NULL, 0, oggcallbacks))
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

                    if(ALERR) break;
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
        if(!id) return;
        alclearerr();
        if(alIsBuffer(id)) alDeleteBuffers(1, &id);
        id = 0;
        ALERR;
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
    worldobjreference *ref;

    bool stale;

    location(int sound, const worldobjreference &r, int priority = SP_NORMAL) : cfg(NULL), src(NULL), ref(NULL), stale(false)
    {
        vector<soundconfig> &sounds = (r.type==worldobjreference::WR_ENTITY ? mapsounds : gamesounds);
        if(!sounds.inrange(sound)) 
        { 
            conoutf("unregistered sound: %d", sound);
            stale = true;
            return;
        }
        
        // get sound config
        cfg = &sounds[sound];
        cfg->onattach();
        if(r.type==worldobjreference::WR_ENTITY && cfg->maxuses >= 0 && cfg->uses >= cfg->maxuses) // check max-use limits
        {
            stale = true;
            return; 
        }

        // assign buffer
        sbuffer *buf = cfg->buf;
        if(!buf) 
        {
            stale = true;
            return;
        }

        // obtain source
        src = scheduler.newsource(priority, r.currentposition());
        // apply configuration
        if(!src || !src->valid || !src->buffer(cfg->buf->id) || !src->looping(cfg->loop) || !src->gain(cfg->vol/100.0f*((float)gainscale)/100.0f))
        {
            stale = true;
            return;
        }
        src->init(this);

        // set position
        attachworldobjreference(r);
    }

    ~location() 
    {
        if(src) scheduler.releasesource(src);
        if(cfg) cfg->ondetach(); 
        if(ref)
        {
            ref->detach();
            DELETEP(ref);
        }
    };

    // attach a reference to a world object to get the 3D position from

    void attachworldobjreference(const worldobjreference &r)
    {
        ASSERT(!stale && src && src->valid);
        if(stale) return;

        if(ref)
        {
            ref->detach();
            DELETEP(ref);
        }
        ref = r.clone(); 
        evaluateworldobjref();
        ref->attach();
    }

    // enable/disable distance calculations
    void evaluateworldobjref()
    {
        src->sourcerelative(ref->nodistance());
    }

    // marks itself for deletion if source got lost
    void onsourcereassign(source *s)
    {
        if(s==src)
        {
            stale = true; 
            src = NULL;
        }
    }

    void updatepos()
    {
        ASSERT(!stale && ref);
        if(stale) return;

        const vec &pos = ref->currentposition();

        switch(ref->type)
        {
            case worldobjreference::WR_CAMERA: break;
            case worldobjreference::WR_PHYSENT:
            {
                if(!ref->nodistance()) src->position(pos);
                break;
            }
            case worldobjreference::WR_ENTITY:
            {
                if(ref->nodistance())
                {
                    // own distance model for entities/mapsounds: linear & clamping
                    entityreference &eref = *(entityreference *)ref;
                    float dist = camera1->o.dist(pos);
                    if(dist <= eref.ent->attr3) src->gain(1.0f);
                    else if(dist <= eref.ent->attr2) src->gain(1.0f - dist/(float)eref.ent->attr2);
                    else src->gain(0.0f);
                }
                else src->position(pos);
                break;
            }
            case worldobjreference::WR_STATICPOS:
            {
                src->position(pos);
                break;
            }
        }
    }

    void update()
    {
        if(stale) return;

        switch(src->state())
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

    void play(bool loop = false)
    {
        if(stale) return;

        updatepos();
        if(loop) src->looping(loop);
        src->play();
    }

    void offset(float secs)
    {
        ASSERT(!stale);
        if(stale) return;
        src->secoffset(secs);
    }

    float offset()
    {
        ASSERT(!stale);
        if(stale) return 0.0f;
        return src->secoffset();
    }

    void drop() 
    {
        src->stop();
        stale = true; // drop from collection on next update cycle
    }
};

struct locvector : vector<location *>
{
    virtual ~locvector(){};

    location *find(int sound, worldobjreference *ref = NULL, const vector<soundconfig> &soundcollection = gamesounds)
    { 
        loopi(ulen) if(buf[i] && !buf[i]->stale)
        {
            if(buf[i]->cfg != &soundcollection[sound]) continue; // check if its the same sound
            if(ref && *(buf[i]->ref)!=*ref) continue; // optionally check if its the same reference
            return buf[i]; // found
        }
        return NULL;
    }

    void delete_(int i)
    {
        location *loc = buf[i];
        remove(i);
        delete loc;
    }

    void replaceworldobjreference(const worldobjreference &oldr, const worldobjreference &newr)
    {
        loopv(*this)
        {
            location *l = buf[i];
            if(!l) continue;
            if(*(l->ref)==oldr) l->attachworldobjreference(newr);
        }
    }

    // update stuff, remove stale data
    void updatelocations()
    {
        // check if camera carrier changed
        bool camchanged = false;
        static physent *lastcamera = NULL;
        if(lastcamera!=camera1)
        {
            if(lastcamera!=NULL) camchanged = true;
            lastcamera = camera1;
        }

        // update all locations
        loopv(*this)
        {
            location *l = buf[i];
            if(!l) continue;

            l->update();
            if(l->stale) delete_(i--);
            else if(camchanged) l->evaluateworldobjref(); // cam changed, evaluate world reference again
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

    // delete all sounds except world-neutral sounds like GUI/notifacation
    void deleteworldobjsounds()
    {
        loopv(*this)
        {
            location *l = buf[i];
            if(!l) continue;
            // world-neutral sounds
            if(l->cfg == &gamesounds[S_MENUENTER] ||
                l->cfg == &gamesounds[S_MENUSELECT] ||
                l->cfg == &gamesounds[S_CALLVOTE] ||
                l->cfg == &gamesounds[S_VOTEPASS] ||
                l->cfg == &gamesounds[S_VOTEFAIL]) continue;

            delete_(i--);
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
    if(!nosound) alListenerf(AL_GAIN, soundvol/255.0f);
});

void setmusicvol() 
{ 
    extern int musicvol; 
    if(gamemusic) gamemusic->setvolume(musicvol > 0 ? musicvol/255.0f : 0);
}
VARFP(musicvol, 0, 128, 255, setmusicvol());

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

            alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
            
            // backend infos
            conoutf("Sound: %s / %s (%s)", alcGetString(device, ALC_DEVICE_SPECIFIER), alGetString(AL_RENDERER), alGetString(AL_VENDOR));
            conoutf("Driver: %s", alGetString(AL_VERSION));

            // allocate OpenAL resources
            scheduler.init();
            
            // let the stream get the first source from the scheduler
            gamemusic = new oggstream();
            if(!gamemusic->valid) DELETEP(gamemusic);

            nosound = false;
        }
    }

    if(nosound)
    {
        ALERR;
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
    if(nosound || !gamemusic) return;
    if(!gamemusic->playing() || !musics.inrange(id)) return;
    if(!strcmp(musics[id], gamemusic->name)) gamemusic->fadeout(lastmillis+1000, 1000);
}

void registermusic(char *name)
{
    if(nosound) return;
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
    if(nosound) return -1;

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

void registersound(char *name, char *vol, char *loop) { addsound(name, atoi(vol), -1, atoi(loop) != 0, gamesounds); }
COMMAND(registersound, ARG_4STR);

void mapsound(char *name, char *vol, char *maxuses, char *loop) { addsound(name, atoi(vol), atoi(maxuses), atoi(loop) != 0, mapsounds); }
COMMAND(mapsound, ARG_4STR);

// called at game exit
void soundcleanup()
{
    if(nosound) return;

    // destroy consuming code
    stopsound();
    DELETEP(gamemusic);
    mapsounds.setsize(0);
    locations.deletecontentsp();
    gamesounds.setsize(0);
    bufferpool.clear();
    
    // kill scheduler
    scheduler.reset();
    
    // shutdown openal
    alcMakeContextCurrent(NULL);
    if(context) alcDestroyContext(context);
    if(device) alcCloseDevice(device);
}

// clear world-related sounds, called on mapchange
void clearworldsounds()
{
    stopsound();
    mapsounds.setsize(0);
    locations.deleteworldobjsounds();
}

VAR(footsteps, 0, 1, 1);

float lastpitch = 1.0f;

void updateplayerfootsteps(playerent *p)
{
    if(!p) return;

    const int footstepradius = 16;
    static int lastfootsteps = 0;
    static float lastoffset = 0;

    // find existing footstep sounds
    physentreference ref(p);
    location *locs[] = {
        locations.find(S_FOOTSTEPS, &ref),
        locations.find(S_FOOTSTEPSCROUCH, &ref),
        locations.find(S_WATERFOOTSTEPS, &ref),
        locations.find(S_WATERFOOTSTEPSCROUCH, &ref)
    };
    
    bool local = (p == camera1);
    bool inrange = (local || (camera1->o.dist(p->o) < footstepradius && footsteps));

    if(!footsteps || !inrange || p->state != CS_ALIVE || lastmillis-p->lastpain < 300 || (!p->onfloor && p->timeinair>50) || (!p->move && !p->strafe) || p->inwater)
    {
        // no footsteps
        if(lastfootsteps>0 && lastmillis-lastfootsteps>100) // stop sound after a short delay
        {
            loopi(sizeof(locs)/sizeof(locs[0]))
            {
                location *l = locs[i];
                if(!l) continue;
                lastoffset = l->offset(); // save last offset
                l->drop();
            }
        }
    }
    else 
    {
        // play footsteps
        bool water = p->o.z-p->dyneyeheight()<hdr.waterlevel;
        int stepsound;
        if(p->crouching) stepsound = water ? S_WATERFOOTSTEPSCROUCH : S_FOOTSTEPSCROUCH; // crouch
        else stepsound = water ? S_WATERFOOTSTEPS : S_FOOTSTEPS; // normal


        // proc existing sounds
        bool isplaying = false;
        loopi(sizeof(locs)/sizeof(locs[0]))
        {
            location *l = locs[i];
            if(!l) continue;    
            if(i+S_FOOTSTEPS==stepsound) isplaying = true; // already playing
            else l->drop(); // different footstep sound, drop it
        }

        if(!isplaying)
        {
            // play using existing offset, if available
            playsound(stepsound, ref, local ? SP_HIGH : SP_LOW, lastoffset>0.01f ? lastoffset : 0.0f);
        }

        lastfootsteps = lastmillis;
    }
}

void updateloopsound(int sound, bool active, float vol = 1.0f)
{
    if(camera1->type != ENT_PLAYER) return;
    location *l = locations.find(sound);
    if(!l && active) playsound(sound, SP_HIGH);
    else if(l && !active) l->drop();
    if(l && vol != 1.0f) l->src->gain(vol);
}

void updateaudio()
{
    if(nosound) return;

    alcSuspendContext(context); // don't process sounds while we mess around
    
    bool alive = player1->state!=CS_DEAD; 
    bool firstperson = camera1->type==ENT_PLAYER;

    // footsteps
    updateplayerfootsteps(player1); 
    loopv(players)
    {
        playerent *p = players[i];
        if(!p) continue;
        updateplayerfootsteps(p);
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
        if(e.type!=SOUND) continue;

        int sound = e.attr1;
        bool hearable = camera1->o.dist(o)<e.attr2;
        entityreference entref(&e);

        // search existing sound loc
        location *loc = locations.find(sound, &entref, mapsounds);

        if(hearable && !loc) // play
        {
            playsound(sound, entref, SP_HIGH);
        }
        else if(!hearable && loc) // stop
        {
            loc->drop();
        }
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

VARP(maxsoundsatonce, 0, 10, 100);


location *playsound(int n, const worldobjreference &r, int priority, float offset)
{
    if(nosound || !soundvol) return NULL;

    bool loop = false;

    if(r.type==worldobjreference::WR_ENTITY) loop = true; // loop map sounds
    else 
    {
        // avoid bursts of sounds with heavy packetloss and in sp
        static int soundsatonce = 0, lastsoundmillis = 0;
        if(totalmillis==lastsoundmillis) soundsatonce++; else soundsatonce = 1;
        lastsoundmillis = totalmillis;
        if(maxsoundsatonce && soundsatonce>maxsoundsatonce) 
        {
            if(audiodebug) conoutf("sound %d filtered by maxsoundsatonce (soundsatonce %d)", n, soundsatonce);
            return NULL;
        }
    }

    location *loc = new location(n, r, priority);
    locations.add(loc);
    if(offset>0) loc->offset(offset);
    loc->play(loop);

    if(audiodebug) conoutf("played sound no %d with prio %d", n, priority);
    return loc;
}

void playsound(int n, int priority) { playsound(n, camerareference(), priority); };
void playsound(int n, physent *p, int priority) { playsound(n, physentreference(p), priority); };
void playsound(int n, entity *e, int priority) { playsound(n, entityreference(e), priority); };
void playsound(int n, const vec *v, int priority) { playsound(n, staticreference(*v), priority); };

void playsoundname(char *s, const vec *loc, int vol) 
{ 
    if(!nosound) return;

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
    if(nosound) return;
    // make all dependent locations static
    locations.replaceworldobjreference(physentreference(owner), staticreference(owner->o));
}

void soundtest()
{
    loopi(S_NULL) playsound(i, rnd(SP_HIGH+1));
}

COMMAND(soundtest, ARG_NONE);

