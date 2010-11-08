// audio streaming

#include "cube.h"

#define DEBUGCOND (audiodebug==1)

// ogg compat

static size_t oggcallbackread(void *ptr, size_t size, size_t nmemb, void *datasource)
{
    stream *s = (stream *)datasource;
    return s ? s->read(ptr, int(size*nmemb))/size : 0;
}

static int oggcallbackseek(void *datasource, ogg_int64_t offset, int whence)
{
    stream *s = (stream *)datasource;
    return s && s->seek(long(offset), whence) ? 0 : -1;
}

static int oggcallbackclose(void *datasource)
{
    stream *s = (stream *)datasource;
    if(!s) return -1;
    delete s;
    return 0;
}

static long oggcallbacktell(void *datasource)
{
    stream *s = (stream *)datasource;
    return s ? s->tell() : -1;
}

ov_callbacks oggcallbacks = { oggcallbackread, oggcallbackseek, oggcallbackclose, oggcallbacktell }; 

// ogg audio streaming

oggstream::oggstream() : valid(false), isopen(false), src(NULL)
{ 
    reset();

    // grab a source and keep it during the whole lifetime
    src = sourcescheduler::instance().newsource(SP_HIGHEST, camera1->o);
    if(src)
    {
        if(src->valid)
        {
            src->init(this);
            src->sourcerelative(true);
        }
        else
        {
            sourcescheduler::instance().releasesource(src);
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

    if(src) sourcescheduler::instance().releasesource(src);
    
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
        formatstring(filepath)("packages/audio/soundtracks/%s%s", f, exts[i]);
        ::stream *file = openfile(path(filepath), "rb");
        if(!file) continue;

        isopen = !ov_open_callbacks(file, &oggfile, NULL, 0, oggcallbacks);
        if(!isopen)
        {
            delete file;
            continue;
        }

        info = ov_info(&oggfile, -1);
        format = info->channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
        totalseconds = ov_time_total(&oggfile, -1);
        copystring(name, f);

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

