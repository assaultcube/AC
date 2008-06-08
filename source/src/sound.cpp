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

bool alerr()
{
	ALenum er = alGetError();
    if(er) conoutf("\f3OpenAL Error (%X)", er);
    return er > 0;
}

struct oggstream
{
    OggVorbis_File oggfile;
    vorbis_info *info;
    ALuint bufferids[2];
    ALuint sourceid;
    ALenum format;
    ALint laststate;

    string name;
    double totalseconds;
    static const int BUFSIZE = (1024 * 16);
    int startmillis, endmillis, startfademillis, endfademillis;
    float volume, gain;
    bool looping;
 
    oggstream() : sourceid(0), startmillis(0), endmillis(0), startfademillis(0), endfademillis(0), volume(1.0f), gain(1.0f), looping(false) {}
    ~oggstream() { reset(); }

    bool open(const char *f)
    {
        if(!f) return false;
        if(playing()) reset();

        const char *exts[] = { "", ".wav", ".ogg" };
        string filepath;

        loopi(sizeof(exts)/sizeof(exts[0]))
        {
            s_sprintf(filepath)("packages/audio/songs/%s%s", f, exts[i]);
            FILE *file = fopen(findfile(path(filepath), "rb"), "rb");
            if(!file) continue;

            if(ov_open_callbacks(file, &oggfile, NULL, 0, OV_CALLBACKS_DEFAULT) < 0)
            {
                fclose(file);
                continue;
            }
            info = ov_info(&oggfile, -1);
            format = info->channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
            totalseconds = ov_time_total(&oggfile, -1);
            s_strcpy(this->name, f);

            alclearerr();
            alGenBuffers(2, bufferids);
            alGenSources(1, &sourceid);
            alerr();

            alSource3f(sourceid, AL_POSITION, 0.0, 0.0, 0.0);
            alSource3f(sourceid, AL_VELOCITY, 0.0, 0.0, 0.0);
            alSource3f(sourceid, AL_DIRECTION, 0.0, 0.0, 0.0);
            // disable sound distance calculations
            alSourcef(sourceid, AL_ROLLOFF_FACTOR, 0.0f);
            alSourcei(sourceid, AL_SOURCE_RELATIVE, AL_TRUE);
            return true;
        }
        return false;
    }

    bool playing() 
    { 
        if(!sourceid) return false;
        alGetSourcei(sourceid, AL_SOURCE_STATE, &laststate); 
        return laststate == AL_PLAYING;
    }

    void updategain() 
    { 
        alSourcef(sourceid, AL_GAIN, gain*volume); 
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
        setgain(0.0f);
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
        if(!sourceid || !stream(bufferids[0]) || !stream(bufferids[1])) return false;
        alSourceQueueBuffers(sourceid, 2, bufferids);
        alSourcePlay(sourceid);
        this->looping = looping;
        return true;
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
        if(!sourceid || !playing()) return false;
        // update buffer queue
        ALint processed;
        bool active = true;
        alGetSourcei(sourceid, AL_BUFFERS_PROCESSED, &processed);
        loopi(processed)
        {
            ALuint buffer;
            alSourceUnqueueBuffers(sourceid, 1, &buffer);
            active = stream(buffer);
            alSourceQueueBuffers(sourceid, 1, &buffer);
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

    void empty()
    {
        ALint queued;
        alGetSourcei(sourceid, AL_BUFFERS_QUEUED, &queued);
        alclearerr();
        loopi(queued)
        {
            ALuint buffer;
            alSourceUnqueueBuffers(sourceid, 1, &buffer);
        }
        alerr();
    }

    void release()
    {
        if(sourceid)
        {
            alSourceStop(sourceid);
            empty();
            alclearerr();
            alDeleteSources(1, &sourceid);
            alDeleteBuffers(2, bufferids);
            alerr();
            sourceid = 0;
        }
        ov_clear(&oggfile);
        
        setgain(1.0f);
        startmillis = endmillis = startfademillis = endfademillis = 0;
        looping = false;
    }

    void reset()
    {
        release();
        setgain(1.0f);
        startmillis = endmillis = startfademillis = endfademillis = 0;
        looping = false;
        name[0] = '\0';
    }

    void seek(double offset)
    {
        if(!totalseconds) return;
        //ov_time_seek_page(&oggfile, fmod(offset, totalseconds));
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

struct slot
{
    sbuffer *buf;
    int vol, uses, maxuses;
    bool loop;

    slot(sbuffer *b, int vol, int maxuses, bool loop)
    {
        buf = b;
        this->vol = vol;
        this->maxuses = maxuses;
        this->loop = loop;
        uses = 0;
    }
};

// short living sound occurrence, dies once the sound stops
struct location
{
    slot *s;
    ALuint id;
    bool inuse;
    int priority;
    static const float maxgain;

    vec pos;
    physent *p;
    entity *e;

    location() : inuse(false), priority(SP_NORMAL) {};
    ~location() { delsource(); }

    bool gensource()
    {
        alclearerr();
        alGenSources(1, &id);
        alSourcef(id, AL_MAX_GAIN, maxgain);
        alSourcef(id, AL_ROLLOFF_FACTOR, 2.0f);
        return !alerr();
    }

    void delsource()
    {
        alclearerr();
        alDeleteSources(1, &id);
        alerr();
    }

    void assignslot(slot *sl)
    {
        if(inuse) return;
        ASSERT(sl && sl > 0);
        s = sl;
        alSourcei(id, AL_BUFFER, s->buf->id);
        alSourcei(id, AL_LOOPING, s->loop);
    }

    void play()
    {
        if(!inuse)
        {
            s->uses++;
            inuse = true;
        }
        updatepos();
        alclearerr();
        alSourcePlay(id);
        alerr();
    }

    void pause()
    {
        if(!inuse) return;
        alSourcePause(id);
    }

    void attachtoworldobj(physent *d)
    {
        if(inuse) return;
        p = d;
        e = NULL;
        if(p==camera1) sourcerelative(true);
    }

    void attachtoworldobj(const vec *v)
    {
        if(inuse) return;
        p = NULL;
        e = NULL;
        pos.x = v->x;
        pos.y = v->y;
        pos.z = v->z;
    }

    void attachtoworldobj(entity *ent)
    {
        if(inuse) return;
        e = ent;
        e->soundinuse = true;
        p = NULL;
        if(e->attr2) // radius set
        {
            sourcerelative(true);
        }
    }

    void sourcerelative(bool enable) // disable distance calculations
    {
        alSourcei(id, AL_SOURCE_RELATIVE, enable ? AL_TRUE : AL_FALSE);
        alSourcef(id, AL_ROLLOFF_FACTOR,  enable ? 0.0f : 1.0f); 
    }

    void reset()
    {
        if(!inuse) return;
        if(s)
        {
            s->uses--;
            s = NULL;
        }
        p = NULL;
        if(e) e->soundinuse = false;
        e = NULL;
        inuse = false;
        priority = SP_NORMAL;
        alSourceStop(id);
        // default settings
        sourcerelative(false);
        alSourcef(id, AL_REFERENCE_DISTANCE, 1.0f);
        alSource3f(id, AL_POSITION, 0.0f, 0.0f, 0.0f);
        alSource3f(id, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
        alSourcef(id, AL_PITCH, 1.0f);
    }

    void gain(float g)
    {
        alSourcef(id, AL_GAIN, g);
    }

    int seconds()
    {
        ALint secs;
        alGetSourcei(id, AL_SEC_OFFSET, &secs);
        return secs;
    }

    vec o() { return p ? p->o : e ? vec(e->x, e->y, e->z) : pos; }

    bool playing()
    {
        ALint p;
        alGetSourcei(id, AL_SOURCE_STATE, &p);
        return (p == AL_PLAYING);
    }

    void update()
    {
        if(!inuse) return;
        ALint s; alGetSourcei(id, AL_SOURCE_STATE, &s);

        switch(s)
        {
            case AL_PLAYING:
                updatepos();
                break;
            case AL_STOPPED:
            case AL_PAUSED:
            case AL_INITIAL:
                reset();
                break;
        }
    }

    void updatepos()
    {
        if(p) // players
        {
            if(p!=camera1) alSourcefv(id, AL_POSITION, (ALfloat *) &p->o);
        }
        else if(e) // entities
        {
            // own distance model for entities/mapsounds: linear & clamping
            if(e->attr2)
            {
                float dist = camera1->o.dist(vec(e->x, e->y, e->z));
                if(dist <= e->attr3) gain(1.0f);
                else if(dist <= e->attr2) gain(1.0f - dist/(float)e->attr2);
                else gain(0.0f);
            }
            else alSource3f(id, AL_POSITION, (float)e->x, (float)e->y, (float)e->z);
        }
        else alSourcefv(id, AL_POSITION, (ALfloat *) &pos); // static stuff
    }

    void pitch(float p)
    {
        alSourcef(id, AL_PITCH, p);
    }
};

const float location::maxgain = 0.1f;


hashtable<char *, sbuffer> buffers;
vector<slot> gamesounds, mapsounds;
vector<location> locations;
oggstream gamemusic;

VARFP(soundvol, 0, 128, 255,
{
    alListenerf(AL_GAIN, soundvol/255.0f);
});

void setmusicvol() 
{ 
    extern int musicvol; 
    gamemusic.setvolume(musicvol/255.0f);
}
VARFP(musicvol, 0, 64, 255, setmusicvol());

char *musicdonecmd = NULL;

void stopsound()
{
    if(nosound) return;
    DELETEA(musicdonecmd);
    gamemusic.reset();   
}

void initsound()
{
    if(!audio)
    {
        conoutf("audio is disabled");
        return;
    }

    alclearerr();
    device = alcOpenDevice(NULL);
    if(!alerr() && device)
    {
        context = alcCreateContext(device, NULL);
        if(!alerr() && context)
        {
            alcMakeContextCurrent(context);
            alDistanceModel(AL_EXPONENT_DISTANCE_CLAMPED);
            // TODO: display available extensions
            conoutf("Sound: %s (%s)", alGetString(AL_RENDERER), alGetString(AL_VENDOR));
            conoutf("Driver: %s", alGetString(AL_VERSION));
            nosound = false;
        }
    }

    if(nosound) conoutf("sound initialization failed!");
}

cvector musics;

void music(char *name, char *millis, char *cmd)
{
    if(nosound) return;
    stopsound();
    if(musicvol && *name)
    {
        if(cmd[0]) musicdonecmd = newstring(cmd);

        if(gamemusic.open(name))
        {
            // fade
            if(atoi(millis) > 0)
            {
                const int fadetime = 1000;
                gamemusic.fadein(lastmillis, fadetime);
                gamemusic.fadeout(lastmillis+atoi(millis), fadetime);
            }

            // play
            bool loop = cmd && cmd[0];
            if(!gamemusic.playback(loop))
            {
                conoutf("could not play music: %s", name);
            }
        }
        else conoutf("could not open music: %s", name);
    }
}

void musicsuggest(int id, int millis, bool rndofs) // play bg music if nothing else is playing
{
    if(nosound) return;
    if(gamemusic.playing()) return;

    if(!musics.inrange(id))
    {
        conoutf("\f3music %d not registered", id);
        return;
    }
    char *name = musics[id];
    if(gamemusic.open(name))
    {
        gamemusic.fadein(lastmillis, 1000);
        gamemusic.fadeout(millis ? lastmillis+millis : 0, 1000);
        if(rndofs) gamemusic.seek(millis ? (double)rnd(millis)/2.0f : (double)lastmillis);
        if(!gamemusic.playback(rndofs)) conoutf("could not play music: %s", name);
    }
    else conoutf("could not open music: %s", name);
}

void musicfadeout(int id)
{
    if(!gamemusic.playing() || !musics.inrange(id)) return;
    if(!strcmp(musics[id], gamemusic.name)) gamemusic.fadeout(lastmillis+1000, 1000);
}

void registermusic(char *name)
{
    if(!name || !name[0]) return;
    musics.add(newstring(name));
}

COMMAND(registermusic, ARG_1STR);
COMMAND(music, ARG_3STR);

int findsound(char *name, int vol, vector<slot> &sounds)
{
    loopv(sounds)
    {
        if(!strcmp(sounds[i].buf->name, name) && (!vol || sounds[i].vol==vol)) return i;
    }
    return -1;
}

int addsound(char *name, int vol, int maxuses, bool loop, vector<slot> &sounds)
{
    sbuffer *b = buffers.access(name);
    if(!b)
    {
        char *n = newstring(name);
        b = &buffers[n];
        if(!b->load(name))
        {
            buffers.remove(name);
            DELETEA(n);
            conoutf("\f3failed to load sample %s", name);
        }
    }
    slot s(b, vol > 0 ? vol : 500, maxuses, loop);
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
    gamesounds.setsizenodelete(0);

    clearsounds();
    
    alcMakeContextCurrent(NULL);
    alcDestroyContext(context);
    alcCloseDevice(device);
}

void clearsounds()
{
    mapsounds.setsizenodelete(0);
    loopv(locations) if(locations[i].inuse) locations[i].reset();
    locations.setsizenodelete(0);
    gamemusic.reset();
}

void checkmapsounds()
{
    loopv(ents)
    {
        entity &e = ents[i];
        vec o(e.x, e.y, e.z);
        if(e.type!=SOUND || e.soundinuse || camera1->o.dist(o)>=e.attr2) continue;
        playsound(e.attr1, NULL, &e);
    }
}

VAR(footsteps, 0, 1, 1);

location *findsoundloc(int sound, physent *p) 
{ 
    loopv(locations) if(locations[i].p == p && locations[i].s == &gamesounds[sound]) return &locations[i];
    return NULL;
}

int lastpitch = 1.0f;

void updatepitch() // set lower pitch if "player's ear got damaged"
{
    if(camera1->type!=ENT_PLAYER) return;
    playerent *p = (playerent *) camera1;
    float pitch = lastmillis<=p->eardamagemillis && p->state!=CS_DEAD ? 0.65 : 1.0f;
    if(pitch==lastpitch) return;
    loopv(locations) locations[i].pitch(pitch);
    lastpitch = pitch;
}

void updateplayerfootsteps(playerent *p, int sound)
{
    if(!p) return;

    const int footstepradius = 16;
    location *loc = findsoundloc(sound, p);
    bool local = (p == camera1);

    bool inrange = (local || (camera1->o.dist(p->o) < footstepradius && footsteps));
    bool nosteps = (!footsteps || p->state != CS_ALIVE || lastmillis-p->lastpain < 300 || (!p->onfloor && p->timeinair>50) || (!p->move && !p->strafe) || (sound==S_FOOTSTEPS && p->crouching) || (sound==S_FOOTSTEPSCROUCH && !p->crouching) || p->inwater);

    if(inrange && !nosteps)
    {
        if(!loc) playsound(sound, p, NULL, NULL, local ? SP_HIGH : SP_LOW);
    }
    else if(loc) loc->reset();
}

void updateloopsound(int sound, bool active, float vol = 1.0f)
{
    if(camera1->type != ENT_PLAYER) return;
    location *l = findsoundloc(sound, camera1);
    if(!l && active) playsound(sound, NULL, NULL, NULL, SP_HIGH);
    else if(l && !active) l->reset();
    if(l && vol != 1.0f) l->gain(vol);
}

void checkplayerloopsounds()
{
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

    bool alive = player1->state!=CS_DEAD;
    // water
    updateloopsound(S_UNDERWATER, alive && player1->inwater);
    // tinnitus
    bool tinnitus = alive && player1->eardamagemillis>0 && lastmillis<=player1->eardamagemillis;
    float tinnitusvol = tinnitus && player1->eardamagemillis-lastmillis<=1000 ? (player1->eardamagemillis-lastmillis)/1000.0f : 1.0f;
    updateloopsound(S_TINNITUS, tinnitus, tinnitusvol);
}

void updatevol()
{
    // orientation
    vec o[2];
    o[0].x = (float)(cosf(RAD*(camera1->yaw-90)));
    o[0].y = (float)(sinf(RAD*(camera1->yaw-90)));
    o[0].z = 0.0f;
    o[1].x = o[1].y = 0.0f;
    o[1].z = -1.0f;
    alListenerfv(AL_ORIENTATION, (ALfloat *) &o);
    alListener3f(AL_POSITION, camera1->o.x, camera1->o.y, camera1->o.z);

    updatepitch();

    // update all sound locations
    loopv(locations) if(locations[i].inuse) locations[i].update();

    // background music
    if(!gamemusic.update())
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

    vector<slot> &sounds = ent ? mapsounds : gamesounds;
    if(!sounds.inrange(n)) { conoutf("unregistered sound: %d", n); return; }
    slot &s = sounds[n];
    if(!s.buf) return;
    if(ent && s.maxuses && s.uses >= s.maxuses) return;

    // AC sound scheduler
    location *loc = NULL;
    if(locations.length())
    {
        // get free item
        loopv(locations) if(!locations[i].inuse)
        {
            loc = &locations[i]; 
            break;
        }
    }
    else sourcesavail = true; // empty collection, enable creation of new items
    
    if(!loc && sourcesavail) // create new and try generating a source
    {
        loc = &locations.add();
        if(!loc->gensource()) 
        {
            sourcesavail = false;
            locations.pop();
            loc = NULL;
        }
    }
    if(!loc) // no channels left :(
    {
        loc = NULL;
        if(SP_LOW == priority) return;
        loopv(locations) // replace stopped or lower prio sound
        {
            location *l = &locations[i];
            if(SP_LOW == l->priority) 
            {
                loc = l;
                conoutf("ac sound sched: replaced low prio sound"); // FIXME
                break;
            }
        }
        if(!loc)
        {
            float dist = camera1->o.dist(p ? p->o : ent ? vec(ent->x, ent->y, ent->z) : v ? *v : camera1->o);
            float score = dist - priority*10.0f;

            location *farthest = NULL;
            float farthestscore = 0.0f;

            loopv(locations) // still no channel, replace far away sounds of same priority
            {
                location *l = &locations[i];
                if(l->priority <= priority)
                { 
                    float ldist = camera1->o.dist(l->o());
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
                loc = farthest;
                conoutf("ac sound sched: replaced sound of same prio"); // FIXME
            }
        }
    }
    if(!loc) 
    {
        conoutf("ac sound sched: sound aborted, no channel takeover possible"); // FIXME
        return;
    }


    loc->reset();
    loc->priority = priority;
    // attach to world obj
    if(p) loc->attachtoworldobj(p);
    else if(ent) loc->attachtoworldobj(ent);
    else if(v) loc->attachtoworldobj(v);
    else loc->attachtoworldobj(camera1);

    loc->assignslot(&s); // assign sound slot
    loc->play();
}

void playsound(int n, int priority) { playsound(n, NULL, NULL, NULL, priority); };
void playsound(int n, physent *p, int priority) { playsound(n, p, NULL, NULL, priority); };
void playsound(int n, entity *e, int priority) { playsound(n, NULL, e, NULL, priority); };
void playsound(int n, const vec *v, int priority) { playsound(n, NULL, NULL, v, priority); };

void playsoundname(char *s, const vec *loc, int vol) 
{ 
    if(!vol) vol = 100;
    int id = findsound(s, vol, gamesounds);
    if(id < 0) id = addsound(s, vol, 0, false, gamesounds);
    playsound(id, loc);
}

void sound(int n) { playsound(n); }
COMMAND(sound, ARG_1INT);

void detachsounds(playerent *owner)
{
    loopv(locations)
    {
        location &l = locations[i];
        if(l.p != owner) continue;
        l.attachtoworldobj(&l.p->o);
    }
}

void playsoundc(int n)
{ 
    addmsg(SV_SOUND, "i", n);
    playsound(n);
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

void soundtest()
{
    loopi(S_NULL) playsound(i, SP_HIGH);
}

COMMAND(soundtest, ARG_NONE);
