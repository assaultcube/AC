// simple OpenAL call wrappers

#include "cube.h"

#define DEBUGCOND (audiodebug==1)

VAR(al_referencedistance, 0, 400, 1000000);
VAR(al_rollofffactor, 0, 100, 1000000);

// represents an OpenAL source, an audio emitter in the 3D world

source::source() : id(0), owner(NULL), locked(false), valid(false), priority(SP_NORMAL)
{
    valid = generate();
    ASSERT(!valid || alIsSource(id));
}

source::~source()
{
    if(valid) delete_();
}

void source::lock()
{
    locked = true;
    DEBUG("source locked, " << lastmillis);
}

void source::unlock()
{
    locked = false;
    owner = NULL;
    stop();
    buffer(0);
    DEBUG("source unlocked, " << lastmillis);
}

void source::reset()
{
    ASSERT(alIsSource(id));
    owner = NULL;
    locked = false;
    priority = SP_NORMAL;

    // restore default settings

    stop();
    buffer(0);

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


void source::init(sourceowner *o)
{
    ASSERT(o);
    owner = o;
}

void source::onreassign()
{
    if(owner)
    {
        owner->onsourcereassign(this);
        owner = NULL;
    }
}

bool source::generate()
{
    alclearerr();
    alGenSources(1, &id);

    return !alerr(false);
}

bool source::delete_()
{
    alclearerr();
    alDeleteSources(1, &id);
    return !ALERR;
}

bool source::buffer(ALuint buf_id)
{
    alclearerr();
#ifdef __APPLE__    // weird bug
    if (buf_id)
#endif
        alSourcei(id, AL_BUFFER, buf_id);

    return !ALERR;
}

bool source::looping(bool enable)
{
    alclearerr();
    alSourcei(id, AL_LOOPING, enable ? 1 : 0);
    return !ALERR;
}

bool source::queuebuffers(ALsizei n, const ALuint *buffer_ids)
{
    alclearerr();
    alSourceQueueBuffers(id, n, buffer_ids);
    return !ALERR;
}

bool source::unqueueallbuffers()
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

bool source::gain(float g)
{
    alclearerr();
    alSourcef(id, AL_GAIN, g);
    return !ALERRF("gain: %f", g);
}

bool source::pitch(float p)
{
    alclearerr();
    alSourcef(id, AL_PITCH, p);
    return !ALERRF("pitch: %f", p);
}

bool source::position(const vec &pos)
{
    return position(pos.x, pos.y, pos.z);
}

bool source::position(float x, float y, float z)
{
    alclearerr();
    alSource3f(id, AL_POSITION, x, y, z);
    if(x < 0 || y < 0 || z < -128 || x > ssize || y > ssize || z > 128) clientlogf("warning: sound position out of range (%f,%f,%f)", x, y, z);
    return !ALERRF("id %u, %d, x: %f, y: %f, z: %f", id, alIsSource(id) == AL_TRUE ? 1 : 0,  x, y, z);
//    return !ALERRF("x: %f, y: %f, z: %f", x, y, z);   // when sound bug has been absent for a while, switch to this version
}

bool source::velocity(float x, float y, float z)
{
    alclearerr();
    alSource3f(id, AL_VELOCITY, x, y, z);
    return !ALERRF("dx: %f, dy: %f, dz: %f", x, y, z);
}

vec source::position()
{
    alclearerr();
    ALfloat v[3];
    alGetSourcefv(id, AL_POSITION, v);
    if(ALERR) return vec(0,0,0);
    else return vec(v[0], v[1], v[2]);
}

bool source::sourcerelative(bool enable)
{
    alclearerr();
    alSourcei(id, AL_SOURCE_RELATIVE, enable ? AL_TRUE : AL_FALSE);
    return !ALERR;
}

int source::state()
{
    ALint s;
    alGetSourcei(id, AL_SOURCE_STATE, &s);
    return s;
}

bool source::secoffset(float secs)
{
    alclearerr();
    alSourcef(id, AL_SEC_OFFSET, secs);
    // some openal implementations seem to spam invalid enum on this
    // return !ALERR;
    return !alerr(false);
}

float source::secoffset()
{
    alclearerr();
    ALfloat s;
    alGetSourcef(id, AL_SEC_OFFSET, &s);
    // some openal implementations seem to spam invalid enum on this
    // ALERR;
    alerr(false);
    return s;
}

bool source::playing()
{
    return (state() == AL_PLAYING);
}

bool source::play()
{
    alclearerr();
    alSourcePlay(id);
    return !ALERR;
}

bool source::stop()
{
    alclearerr();
    alSourceStop(id);
    return !ALERR;
}

bool source::rewind()
{
    alclearerr();
    alSourceRewind(id);
    return !ALERR;
}

void source::printposition()
{
    alclearerr();
    vec v = position();
    ALint s;
    alGetSourcei(id, AL_SOURCE_TYPE, &s);
    conoutf("sound %d: pos(%f,%f,%f) t(%d) ", id, v.x, v.y, v.z, s);
    ALERR;
}


// represents an OpenAL sound buffer

sbuffer::sbuffer() : id(0), name(NULL)
{
}

sbuffer::~sbuffer()
{
    unload();
}

bool sbuffer::load(bool trydl)
{
    if(!name) return false;
    if(id) return true;
    alclearerr();
    alGenBuffers(1, &id);
    if(!ALERR)
    {
        const char *exts[] = { "", ".wav", ".ogg" };
        string filepath;
        loopi(sizeof(exts)/sizeof(exts[0]))
        {
            formatstring(filepath)("packages/audio/%s%s", name, exts[i]);
            stream *f = openfile(path(filepath), "rb");
            if(!f) continue;

            size_t len = strlen(filepath);
            if(len >= 4 && !strcasecmp(filepath + len - 4, ".ogg"))
            {
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
                        bytes = ov_read(&oggfile, buffer, BUFSIZE, isbigendian(), 2, 1, &bitstream);
                        loopi(bytes) buf.add(buffer[i]);
                    } while(bytes > 0);

                    alBufferData(id, info->channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16, buf.getbuf(), buf.length(), info->rate);
                    ov_clear(&oggfile);
                }
                else
                {
                    delete f;
                    continue;
                }
            }
            else
            {
                SDL_AudioSpec wavspec;
                uint32_t wavlen;
                uint8_t *wavbuf;

                if(!SDL_LoadWAV_RW(f->rwops(), 1, &wavspec, &wavbuf, &wavlen))
                {
                    SDL_ClearError();
                    continue;
                }

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
                        delete f;
                        unload();
                        return false;
                }

                alBufferData(id, format, wavbuf, wavlen, wavspec.freq);
                SDL_FreeWAV(wavbuf);
                delete f;

                if(ALERR)
                {
                    unload();
                    return false;
                };
            }

            return true;
        }
        if(trydl) requirepackage(PCK_AUDIO, name); // only try donwloading after trying all extensions
    }
    unload(); // loading failed
    return false;
}

void sbuffer::unload()
{
    if(!id) return;
    alclearerr();
    if(alIsBuffer(id)) alDeleteBuffers(1, &id);
    id = 0;
    ALERR;
}

// buffer collection, find or load data

bufferhashtable::~bufferhashtable() {}

sbuffer *bufferhashtable::find(const char *name)
{
    sbuffer *b = access(name);
    if(!b)
    {
        name = newstring(name);
        b = &(*this)[name];
        b->name = name;
    }
    return b;
}


// OpenAL error handling

void alclearerr()
{
    alGetError();
}

bool alerr(bool msg, int line, const char *s, ...)
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
        if(s)
        {
            defvformatstring(p, s, s);
            conoutf("\f3OpenAL Error (%X): %s, line %d, (%s)", er, desc, line, p);
        }
        else if(line) conoutf("\f3OpenAL Error (%X): %s, line %d", er, desc, line);
        else conoutf("\f3OpenAL Error (%X): %s", er, desc);
    }
    return er > 0;
}
