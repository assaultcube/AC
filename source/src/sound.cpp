// sound.cpp: uses OpenAL, some code chunks are from devmaster.net and gamedev.net

#include "pch.h"
#include "cube.h"

#include "AL/al.h" 
#include "AL/alc.h" 
#include "AL/alut.h"
#include "vorbis/vorbisfile.h"

bool nosound = true;

bool alerr()
{
    ALenum er = alutGetError(); // alut
    if(er) 
    {
        conoutf("OpenAL %s %X",  alutGetErrorString(er), er);
        return true;
    }
    else
    {
        er = alGetError(); // al
        if(er)
        {
            conoutf("OpenAL Error %X", er);
            return true;
        }
    }
    return false;
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

    void gain(float g) { alSourcef(sourceid, AL_GAIN, g); }

    bool playback()
    {
        if(playing()) return true;
        if(!sourceid || !stream(bufferids[0])) return false;
        stream(bufferids[1]);
        alGetError();
        alSourceQueueBuffers(sourceid, 2, bufferids);
        alSourcePlay(sourceid);
        extern void setmusicvol(); setmusicvol();
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
                    ALenum format;
                    ALsizei size, freq;
                    ALboolean loop;
                    ALvoid *data;
                    alutLoadWAVFile((ALbyte *)file, &format, &data, &size, &freq, &loop);
                    if(alutGetError()) continue; // try another file extension

                    alBufferData(id, format, data, size, freq);
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
        alDeleteBuffers(1, &id);
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
        alGenSources(1, &id);
        alSourcef(id, AL_MAX_GAIN, maxgain);
        alSourcef(id, AL_ROLLOFF_FACTOR, 2.0f);
        return !alerr();
    }

    void delsource()
    {
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
        alSourcePlay(id);
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
        if(p == camera1) sourcerelative(true);
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
        alSource3f(id, AL_POSITION, 0.0, 0.0, 0.0);
        alSource3f(id, AL_VELOCITY, 0.0, 0.0, 0.0);
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
            case AL_INITIAL:
                reset();
                break;
        }
    }

    void updatepos()
    {
        if(p) // players
        {
            if(p != camera1)
            {
                //gain(s->vol/1000.0f);
                alSourcefv(id, AL_POSITION, (ALfloat *) &p->o);
            }
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
};

const float location::maxgain = 0.1f;


hashtable<char *, sbuffer> buffers;
vector<slot> gamesounds, mapsounds;
vector<location> locations;
oggstream gamemusic;

VARFP(soundvol, 0, 128, 255,
{
    alListenerf(AL_GAIN, soundvol/255.0f*10.0f);
});

void setmusicvol() { extern int musicvol; if(gamemusic.playing()) gamemusic.gain(musicvol/255.0f); }
VARFP(musicvol, 0, 128, 255, setmusicvol());

char *musicdonecmd = NULL;

void stopsound()
{
    if(nosound) return;
    DELETEA(musicdonecmd);
    gamemusic.release();   
}

void initsound()
{
    alutInit(0, NULL);
    if(alerr())
    {
        conoutf("sound initialization failed!");
    }
    else
    {
        conoutf("Sound: %s (%s)", alGetString(AL_RENDERER), alGetString(AL_VENDOR));
        conoutf("Driver: %s", alGetString(AL_VERSION));
        alDistanceModel(AL_EXPONENT_DISTANCE_CLAMPED);
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
        if(!strcmp(sounds[i].buf->name, name) && (!vol || sounds[i].vol==vol)) return i;
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
    mapsounds.setsizenodelete(0);
    locations.setsizenodelete(0);

    gamemusic.release();
    alutExit();
}

void clearsounds()
{
    mapsounds.setsizenodelete(0);
    locations.setsizenodelete(0);
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
    bool local = (p == camera1);

    if((local || (camera1->o.dist(p->o) < footstepradius && footsteps))) // is in range
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
                if(time%speed < footstepalign) playsound(sound, p, NULL, NULL, local ? SP_HIGH : SP_LOW);
            }
        }
        
        if(loc) // check again, only pause sound if player isn't moving
        {
            bool playing = loc->playing();
            if(!footsteps || p->state != CS_ALIVE || lastmillis-p->lastpain < 300 || (!p->onfloor && p->timeinair>50) || (!p->move && !p->strafe) || (sound==S_FOOTSTEPS && p->crouching) || (sound==S_FOOTSTEPSCROUCH && !p->crouching) || p->inwater)
            {
                if(playing) loc->pause();
            }
            else
            {
                if(!playing) loc->play();
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
        else gamemusic.replay();
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
    if(ent && s.maxuses && s.uses >= s.maxuses) return;

    // AC sound scheduler
    static bool sourcesavail = true;
    location *loc = NULL;
    loopv(locations) if(!locations[i].inuse) loc = &locations[i]; // get free item
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
            if(farthestscore >= score+5.0f) // better don't play a new one than stopping a playing sound
            {
                conoutf("ac sound sched: replaced sound of same prio"); // FIXME
            }
        }
    }
    if(!loc) 
    {
        conoutf("ac sound sched: sound aborted, no channel takeover possible"); // FIXME
        return;
    }


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

