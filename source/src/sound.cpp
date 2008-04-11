// sound.cpp: uses OpenAL, some code chunks are from devmaster.net and gamedev.net

#include "pch.h"
#include "cube.h"

bool nosound = true;
VARP(musicvol, 0, 128, 255);
VARP(soundvol, 0, 255, 255);

#include "AL/al.h" 
#include "AL/alc.h" 
#include "AL/alut.h"
#include "vorbis/vorbisfile.h"

void alerr(ALenum error, const char *note)
{
    conoutf("OpenAL %s (%s) %X",  alutGetErrorString(error), note, error);
}

bool alerr()
{
    ALenum er = alutGetError();
    if(er) alerr(er, NULL);
    return er!=0;
}

struct oggstream
{
    FILE *file;
    OggVorbis_File oggfile;
    vorbis_info *info;
    ALuint bufferids[2];
    ALuint sourceid;
    ALenum format;
    ALint laststate;
    static const int BUFSIZE = (1024 * 16);
 
    oggstream() : sourceid(0) {}

    bool open(const char *f)
    {
        if(playing()) release();

        file = fopen(path(f, true), "rb");
        if(!file) return false;

        if(ov_open_callbacks(file, &oggfile, NULL, 0, OV_CALLBACKS_DEFAULT) < 0)
        {
            fclose(file);
            return false;
        }
        info = ov_info(&oggfile, -1);
        format = info->channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
        alGenBuffers(2, bufferids);
        alerr();
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

    bool playing() 
    { 
        if(!sourceid) return false;
        alGetSourcei(sourceid, AL_SOURCE_STATE, &laststate); 
        return laststate == AL_PLAYING; 
    }

    bool playback()
    {
        if(playing()) return true;
        if(!sourceid || !stream(bufferids[0])) return false;
        stream(bufferids[1]);
        alGetError();
        alSourceQueueBuffers(sourceid, 2, bufferids);
        alSourcePlay(sourceid);
        ASSERT(!alGetError());
        return true;
    }

    bool replay()
    {
        release();
        return playback();
    }

    bool stream(ALuint bufid)
    {
        char pcm[BUFSIZE];
        ALsizei size = 0;
        int bitstream;
        while(size < BUFSIZE)
        {
            long bytes = ov_read(&oggfile, pcm + size, BUFSIZE - size, 0, 2, 1, &bitstream);
            if(bytes > 0) size += bytes;
            else if (bytes < 0) return false;
            else break; // done
        }
        if(size==0) return false;
        alGetError();
        alBufferData(bufid, format, pcm, size, info->rate);
        ASSERT(!alGetError());
        return true;
    }

    bool update()
    {
        if(!sourceid) return false;
        // update buffer queue
        ALint processed;
        bool active = true;
        alGetSourcei(sourceid, AL_BUFFERS_PROCESSED, &processed);
        loopi(processed)
        {
            alSourceUnqueueBuffers(sourceid, 1, &bufferids[i]);
            active = stream(bufferids[i]);
            alSourceQueueBuffers(sourceid, 1, &bufferids[i]);
        }
        
        // update volume
        alSourcef(sourceid, AL_GAIN, musicvol/255.0f);
        return active;
    }

    void empty()
    {
        loopi(2) 
        {
            alSourceUnqueueBuffers(sourceid, 1, &bufferids[i]);
            alerr();
        }
    }

    void release()
    {
        if(sourceid)
        {
            alSourceStop(sourceid);
            empty();
            alDeleteSources(1, &sourceid);
            alerr();
            alDeleteBuffers(2, bufferids);
            alerr();
            sourceid = 0;
        }
        ov_clear(&oggfile);
    }
};

struct sbuffer
{
    ALuint dat;
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
        alGenBuffers(1, &dat);
        if(!alerr())
        {
            const char *exts[] = { "", ".wav", ".ogg" };
            string filepath;
            loopi(sizeof(exts)/sizeof(exts[0]))
            {
                s_sprintf(filepath)("packages/audio/sounds/%s%s", sound, exts[i]);
                const char *file = findfile(path(filepath), "rb");
                int len = strlen(filepath);
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

                        alBufferData(dat, info->channels == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16, buf.getbuf(), buf.length(), info->rate);
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
                    ALenum format;
                    ALsizei size, freq;
                    ALboolean loop;
                    ALvoid *data;
                    alutLoadWAVFile((ALbyte *)file, &format, &data, &size, &freq, &loop);
                    if(alutGetError()) continue; // try another file extension

                    alBufferData(dat, format, data, size, freq);
                    if(alerr()) break;

                    alutUnloadWAV(format, data, size, freq);
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
        alDeleteBuffers(1, &dat);
        ALenum er;
        if((er = alGetError()))
        {
            alerr(er, "unloading sound");
        }
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

// FIXME
VAR(al_reference_distance, 0, 100, 10000);
VAR(al_rolloff_factor, 0, 100, 10000);

// short living sound occurrence, dies once the sound stops
struct location
{
    slot *s;
    ALuint dat;
    bool inuse;

    vec pos;
    physent *p;
    entity *e;

    location() : inuse(false)
    { 
        alGenSources(1, &dat);
    }

    ~location()
    {
        alDeleteSources(1, &dat);
        ASSERT(!alGetError());
    }

    void assignslot(slot *sl)
    {
        if(inuse) return;
        s = sl;
        alSourcei(dat, AL_BUFFER, s->buf->dat);
        alSourcei(dat, AL_LOOPING, s->loop);
        alSourcef(dat, AL_GAIN, s->vol/100.0f);
        alSourcef(dat, AL_REFERENCE_DISTANCE, al_reference_distance/100.0f);
        alSourcef(dat, AL_ROLLOFF_FACTOR, al_rolloff_factor/100.0f);
    }

    void play()
    {
        alSourcePlay(dat);
        inuse = true;
        s->uses++;
        updatepos();
    }

    void attachtoworldobj(physent *d)
    {
        if(inuse) return;
        p = d;
        e = NULL;
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
        p = NULL;
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
        e = NULL;
        inuse = false;
        alSourceStop(dat);
    }

    void gain(float g)
    {
        alSourcef(dat, AL_GAIN, g);
    }

    int seconds()
    {
        ALint secs;
        alGetSourcei(dat, AL_SEC_OFFSET, &secs);
        return secs;
    }

    void update()
    {
        if(!inuse) return;
        int s; alGetSourcei(dat, AL_SOURCE_STATE, &s);
        if(AL_PLAYING == s) updatepos();
        else reset();
    }

    void updatepos()
    {
        if(p) // players
        {
            if(p==player1) // play sound without distance calculations
            {
                alSourcef(dat, AL_ROLLOFF_FACTOR,  0.0);
                alSourcei(dat, AL_SOURCE_RELATIVE, AL_TRUE);
                alSource3f(dat, AL_POSITION, 0.0, 0.0, 0.0);
                alSource3f(dat, AL_VELOCITY, 0.0, 0.0, 0.0);
                gain(0.2f); // dampen local sounds
            }
            else
            {
                alSourcefv(dat, AL_POSITION, (ALfloat *) &p->o);
                alSourcefv(dat, AL_VELOCITY, (ALfloat *) &p->vel);
            }
        }
        else if(e) // entities
        {
            alSource3f(dat, AL_POSITION, (float)e->x, (float)e->y, (float)e->z);
        }
        else alSourcefv(dat, AL_POSITION, (ALfloat *) &pos); // static
    }
};


hashtable<char *, sbuffer> buffers;
vector<slot> gamesounds, mapsounds;
vector<location> locations;
oggstream gamemusic;

char *musicdonecmd = NULL;

void stopsound()
{
    if(nosound) return;
    DELETEA(musicdonecmd);
    gamemusic.release();   
}

VARF(soundchans, 0, 64, 128, initwarning());
/*VARF(soundfreq, 0, MIX_DEFAULT_FREQUENCY, 44100, initwarning());
VARF(soundbufferlen, 128, 1024, 4096, initwarning());
*/

void initsound()
{
    alutInit(0, NULL);
    ALenum er;
    if((er = alGetError()))
    {
        alerr(er, "initialization failed");
    }
    else
    {
        conoutf("Sound: %s (%s)", alGetString(AL_RENDERER), alGetString(AL_VENDOR));
        conoutf("Driver: %s", alGetString(AL_VERSION));
        nosound = false;
    }
}

void music(char *name, char *cmd)
{
    if(nosound) return;
    stopsound();
    if(musicvol && *name)
    {
        if(cmd[0]) musicdonecmd = newstring(cmd);

        const char *exts[] = { "", ".wav", ".ogg" };
        string filepath;
        loopi(sizeof(exts)/sizeof(exts[0]))
        {
            s_sprintf(filepath)("packages/audio/songs/%s%s", name, exts[i]);
            const char *file = findfile(path(filepath), "rb");
            if(gamemusic.open(file)) break;
        }
        if(!gamemusic.playback())
        {
            conoutf("could not play music: %s", name);
        }
    }
}

int findsound(char *name, int vol, vector<slot> &sounds)
{
    loopv(sounds)
    {
        if(!strcmp(sounds[i].buf->name, name) /*&& (!vol || sounds[i].vol==vol)*/) return i;
    }
    return -1;
}


COMMAND(music, ARG_2STR);

int addsound(char *name, int vol, int maxuses, bool loop, vector<slot> &sounds)
{
    sbuffer *b = buffers.access(name);
    if(!b)
    {
        char *n = newstring(name);
        b = &buffers[n];
        b->load(name);
    }
    sounds.add(slot(b, vol > 0 ? vol : 100, maxuses, loop));
    return sounds.length()-1;
}

void registersound(char *name, char *vol, char *loop) { addsound(name, atoi(vol), 0, atoi(loop) != 0, gamesounds); }
COMMAND(registersound, ARG_3STR);

void mapsound(char *name, char *vol, char *maxuses, char *loop) { addsound(name, atoi(vol), atoi(maxuses), atoi(loop) != 0, mapsounds); }
COMMAND(mapsound, ARG_4STR);

void cleansound()
{
    if(nosound) return;
    stopsound();
    gamesounds.setsizenodelete(0);
    mapsounds.setsizenodelete(0);
    locations.setsizenodelete(0);
    gamemusic.release();
    alutExit();
}

void clearmapsounds()
{
    loopv(locations) if(locations[i].e && locations[i].inuse)
    {
        locations[i].reset();
    }
    mapsounds.setsizenodelete(0);
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

void updateplayerfootsteps(playerent *p, int sound)
{
    const int footstepradius = 16, footstepalign = 15;
    location *loc = findsoundloc(sound, p);
    bool silence = !footsteps || p->state != CS_ALIVE || lastmillis-p->lastpain < 300 || (!p->onfloor && p->timeinair>50) || (!p->move && !p->strafe) || (sound==S_FOOTSTEPS && p->crouching) || (sound==S_FOOTSTEPSCROUCH && !p->crouching);
    bool local = (p == player1);

    if(!silence && (local || (camera1->o.dist(p->o) < footstepradius && footsteps))) // is in range
    {
        if(!loc) // not yet playing, start it
        {
            if(local) playsound(sound, p);
            else
            {
                // sync to model animation
                int basetime = -((int)(size_t)p&0xFFF); 
                int time = lastmillis-basetime;
                int speed = int(1860/p->maxspeed);
                // TODO: share with model code
                if(time%speed < footstepalign) playsound(sound, p);
            }
        }
    }
    else if(loc) loc->reset(); // out of range, stop it
}

void checkplayerloopsounds()
{
    // local player
    updateplayerfootsteps(player1, S_FOOTSTEPS); 
    updateplayerfootsteps(player1, S_FOOTSTEPSCROUCH);

    // others
    loopv(players)
    {
        playerent *p = players[i];
        if(!p) continue;
        updateplayerfootsteps(p, S_FOOTSTEPS);
        updateplayerfootsteps(p, S_FOOTSTEPSCROUCH);
    }
}

int soundrad(int sound)
{
    const int rads[] =
    {
        S_FOOTSTEPS, 16,
        -1
    };
    for(const int *r = rads; *r >= 0; r += 2) if(*r==sound) return r[1];
    return -1;
}

/*
soundloc *newsoundloc(int sound, int chan, soundslot *slot, physent *p = NULL, entity *ent = NULL, const vec *loc = NULL)
{
    
    if(!p && !ent && !loc) return NULL;
    while(chan >= soundlocs.length()) soundlocs.add().inuse = false;
    soundloc *sl = &soundlocs[chan];
    sl->slot = slot;
    sl->pse = p;
    sl->ent = ent && !p ? ent : NULL;
    sl->o = loc && !ent ? *loc : vec(0,0,0);
    sl->radius = ent ? ent->attr2 : soundrad(sound);
    sl->size = ent ? ent->attr3 : -1;
    sl->awake();
    return &soundlocs[chan];
    
    return NULL;
}
*/

void updatevol()
{
    vec pos(player1->o.x, player1->o.y, player1->o.z+player1->eyeheight);
    alListenerfv(AL_POSITION, (ALfloat *) &pos);
    alListenerfv(AL_VELOCITY, (ALfloat *) &player1->vel);
    alListenerf(AL_GAIN, soundvol/255.0f);

    // orientation
    vec o[2];
    o[0].x = (float)(cosf(RAD*(camera1->yaw-90)));
    o[0].y = (float)(sinf(RAD*(camera1->yaw-90)));
    o[0].z = 0.0f;
    o[1].x = o[1].y = 0.0f;
    o[1].z = -1.0f;
    alListenerfv(AL_ORIENTATION, (ALfloat *) &o);

    // update all sound locations
    loopv(locations) if(locations[i].inuse) locations[i].update();

    // background music
    if(!gamemusic.update())
    {
        // music ended
        if(musicdonecmd)
        {
            gamemusic.release();
            char *cmd = musicdonecmd;
            musicdonecmd = NULL;
            execute(cmd);
            delete[] cmd;
        }
        else
        {
            gamemusic.replay();
        }
    }
}

VARP(maxsoundsatonce, 0, 40, 100);

void playsound(int n, physent *p, entity *ent, const vec *v)
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
    if(ent && s.maxuses && s.uses >= s.maxuses) return;

    // get free location item
    location *loc = NULL;
    if(locations.length() < soundchans) loc = &locations.add();
    else loopv(locations) if(!locations[i].inuse) 
    {
        loc = &locations[i];
        break;
    }
    if(!loc) return;

    // attach to world obj
    if(p) loc->attachtoworldobj(p);
    else if(ent) loc->attachtoworldobj(ent);
    else if(v) loc->attachtoworldobj(v);
    else loc->attachtoworldobj(player1);

    loc->assignslot(&s); // assign sound slot
    loc->play();
}

void playsoundname(char *s, const vec *loc, int vol) 
{ 
    if(!vol) vol = 100;
    int id = findsound(s, vol, gamesounds);
    if(id < 0) id = addsound(s, vol, 0, false, gamesounds);
    playsound(id, NULL, NULL, loc);
}
COMMAND(playsoundname, ARG_3STR);

void sound(int *n) { playsound(*n); }
COMMAND(sound, ARG_1INT);

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
        playsound(s);
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

