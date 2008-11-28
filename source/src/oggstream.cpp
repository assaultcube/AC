// audio streaming

#include "pch.h"
#include "cube.h"

#define DEBUGCOND (audiodebug==1)

// ogg compat

static int oggseek(FILE *f, ogg_int64_t off, int whence)
{
    return f ? fseek(f, (long)off, whence) : -1;
}

ov_callbacks oggcallbacks = 
{
    (size_t (*)(void *, size_t, size_t, void *))  fread,
    (int (*)(void *, ogg_int64_t, int))           oggseek,
    (int (*)(void *))                             fclose,
    (long (*)(void *))                            ftell
};

// ogg audio streaming

oggstream::oggstream() : valid(false), isopen(false), src(NULL)
{ 
    reset();

    // grab a source and keep it during the whole lifetime
    src = sourcescheduler::default().newsource(SP_HIGHEST, camera1->o);
    if(src)
    {
        if(src->valid)
        {
            src->init(this);
            src->sourcerelative(true);
        }
        else
        {
            sourcescheduler::default().releasesource(src);
            src = NULL;
        }
    }
    
    if(!src) return;

    alclearerr();
    alGenBuffers(2, bufferids);
    valid = !ALERR;
}

oggstream::~oggstream() 
{ 
    reset();

    if(src) sourcescheduler::default().releasesource(src);
    
    if(alIsBuffer(bufferids[0]) || alIsBuffer(bufferids[1]))
    {
        alclearerr();
        alDeleteBuffers(2, bufferids);
        ALERR;
    }
}

void oggstream::reset()
{
    name[0] = '\0';

    // stop playing
    if(src)
    {
        src->stop();
        src->unqueueallbuffers();
        src->buffer(0);
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

bool oggstream::open(const char *f)
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

void oggstream::onsourcereassign(source *s)
{
    // should NEVER happen because streams do have the highest priority, see constructor
    ASSERT(0);
    if(src && src==s)
    {
        reset();
        src = NULL;
    }
}

bool oggstream::stream(ALuint bufid)
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

bool oggstream::update()
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

bool oggstream::playing() 
{ 
    ASSERT(valid);
    return src->playing();
}

void oggstream::updategain() 
{ 
    ASSERT(valid);
    src->gain(gain*volume);
}

void oggstream::setgain(float g)
{ 
    ASSERT(valid);
    gain = g;
    updategain();
}

void oggstream::setvolume(float v)
{
    ASSERT(valid);
    volume = v;
    updategain();
}

void oggstream::fadein(int startmillis, int fademillis)
{
    ASSERT(valid);
    setgain(0.01f);
    this->startmillis = startmillis;
    this->startfademillis = fademillis;
}

void oggstream::fadeout(int endmillis, int fademillis)
{
    ASSERT(valid);
    this->endmillis = (endmillis || totalseconds > 0.0f) ? endmillis : lastmillis+(int)totalseconds;
    this->endfademillis = fademillis;
}

bool oggstream::playback(bool looping)
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

void oggstream::seek(double offset)
{
    ASSERT(valid);
    if(!totalseconds) return;
    ov_time_seek_page(&oggfile, fmod(totalseconds-5.0f, totalseconds));
}

