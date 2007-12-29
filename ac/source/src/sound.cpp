// sound.cpp: uses fmod on windows and sdl_mixer on unix (both had problems on the other platform)

#include "pch.h"
#include "cube.h"

//#ifndef WIN32    // NOTE: fmod not being supported for the moment as it does not allow stereo pan/vol updating during playback
#define USE_MIXER
//#endif

bool nosound = true;

#ifdef USE_MIXER
    #include "SDL_mixer.h"
    #define MAXVOL MIX_MAX_VOLUME
    Mix_Music *mod = NULL;
    void *stream = NULL;    // TODO
#else
    #include "fmod.h"
    FMUSIC_MODULE *mod = NULL;
    FSOUND_STREAM *stream = NULL;

    #define MAXVOL 255
    int musicchan;
#endif

struct sample
{
    char *name;
    #ifdef USE_MIXER
        Mix_Chunk *sound;
    #else
        FSOUND_SAMPLE *sound;
    #endif

    sample() : name(NULL) {}
    ~sample() { DELETEA(name); }
};

struct soundslot
{
    sample *s;
    int vol;
    int uses, maxuses;
    bool loop;
};

struct soundloc
{
    soundslot *slot;
    bool inuse;
    int radius, size;

    physent *pse;   // players/phys
    entity *ent;    // map ents
    vec o;          // static

    vec loc() { return pse ? pse->o : (ent ? vec(ent->x, ent->y, ent->z) : o); }

    void sleep()
    {
        if(ent) { ent->soundinuse = false; slot->uses--; }
        inuse = false;
    }

    void awake()
    {
        if(ent) { ent->soundinuse = true; slot->uses++; }
        inuse = true;
    }
};

vector<soundloc> soundlocs;

void setmusicvol(int musicvol)
{
    if(nosound) return;
    #ifdef USE_MIXER
        if(mod) Mix_VolumeMusic((musicvol*MAXVOL)/255);
    #else
        if(mod) FMUSIC_SetMasterVolume(mod, musicvol);
        else if(stream && musicchan>=0) FSOUND_SetVolume(musicchan, (musicvol*MAXVOL)/255);
    #endif
}

VARP(soundvol, 0, 255, 255);
VARFP(musicvol, 0, 128, 255, setmusicvol(musicvol));

char *musicdonecmd = NULL;

void stopsound()
{
    if(nosound) return;
    DELETEA(musicdonecmd);
    if(mod)
    {
        #ifdef USE_MIXER
            Mix_HaltMusic();
            Mix_FreeMusic(mod);
        #else
            FMUSIC_FreeSong(mod);
        #endif
        mod = NULL;
    }
    if(stream)
    {
        #ifndef USE_MIXER
            FSOUND_Stream_Close(stream);
        #endif
        stream = NULL;
    }
}

VARF(soundchans, 0, 32, 128, initwarning());
VARF(soundfreq, 0, MIX_DEFAULT_FREQUENCY, 44100, initwarning());
VARF(soundbufferlen, 128, 1024, 4096, initwarning());

void initsound()
{
    #ifdef USE_MIXER
        if(Mix_OpenAudio(soundfreq, MIX_DEFAULT_FORMAT, 2, soundbufferlen)<0)
        {
            conoutf("sound init failed (SDL_mixer): %s", (size_t)Mix_GetError());
            return;
        }
	    Mix_AllocateChannels(soundchans);	
    #else
        if(FSOUND_GetVersion()<FMOD_VERSION) fatal("old FMOD dll");
        if(!FSOUND_Init(soundfreq, soundchans, FSOUND_INIT_GLOBALFOCUS))
        {
            conoutf("sound init failed (FMOD): %d", FSOUND_GetError());
            return;
        }
    #endif
    nosound = false;
}

void musicdone()
{
    if(!musicdonecmd) return;
#ifdef USE_MIXER
    if(mod) Mix_FreeMusic(mod);
#else
    if(mod) FMUSIC_FreeSong(mod);
    if(stream) FSOUND_Stream_Close(stream);
#endif
    mod = NULL;
    stream = NULL;
    char *cmd = musicdonecmd;
    musicdonecmd = NULL;
    execute(cmd);
    delete[] cmd;
}

void music(char *name, char *cmd)
{
    if(nosound) return;
    stopsound();
    if(soundvol && *name)
    {
        if(cmd[0]) musicdonecmd = newstring(cmd);
        s_sprintfd(sn)("packages/audio/songs/%s", name);
        const char *file = findfile(path(sn), "rb");
        #ifdef USE_MIXER
            if((mod = Mix_LoadMUS(file)))
            {
                Mix_PlayMusic(mod, cmd[0] ? 0 : -1);
                Mix_VolumeMusic((musicvol*MAXVOL)/255);
            }
        #else
            if((mod = FMUSIC_LoadSong(file)))
            {
                FMUSIC_PlaySong(mod);
                FMUSIC_SetMasterVolume(mod, musicvol);
                FMUSIC_SetLooping(mod, cmd[0] ? FALSE : TRUE);
            }
            else if((stream = FSOUND_Stream_Open(file, cmd[0] ? FSOUND_LOOP_OFF : FSOUND_LOOP_NORMAL, 0, 0)))
            {
                musicchan = FSOUND_Stream_Play(FSOUND_FREE, stream);
                if(musicchan>=0) { FSOUND_SetVolume(musicchan, (musicvol*MAXVOL)/255); FSOUND_SetPaused(musicchan, false); }
            }
        #endif
            else
            {
                conoutf("could not play music: %s", sn);
            }
    }
}

COMMAND(music, ARG_2STR);

hashtable<char *, sample> samples;
vector<soundslot> gamesounds, mapsounds;

int findsound(char *name, int vol, vector<soundslot> &sounds)
{
    loopv(sounds)
    {
        if(!strcmp(sounds[i].s->name, name) && (!vol || sounds[i].vol==vol)) return i;
    }
    return -1;
}

int addsound(char *name, int vol, int maxuses, bool loop, vector<soundslot> &sounds)
{
    sample *s = samples.access(name);
    if(!s)
    {
        char *n = newstring(name);
        s = &samples[n];
        s->name = n;
        s->sound = NULL;
    }
    soundslot &slot = sounds.add();
    slot.s = s;
    slot.vol = vol ? vol : 100;
    slot.uses = 0;
    slot.maxuses = maxuses;
    slot.loop = loop;
    return sounds.length()-1;
}

void registersound(char *name, char *vol, char *loop) { addsound(name, atoi(vol), 0, atoi(loop) != 0, gamesounds); }
COMMAND(registersound, ARG_3STR);

void mapsound(char *name, char *vol, char *maxuses, char *loop) { addsound(name, atoi(vol), atoi(maxuses) < 0 ? 0 : max(1, *maxuses), atoi(loop) != 0, mapsounds); }
COMMAND(mapsound, ARG_4STR);

void cleansound()
{
    if(nosound) return;
    stopsound();
    gamesounds.setsizenodelete(0);
    mapsounds.setsizenodelete(0);
    samples.clear();
    #ifdef USE_MIXER
        Mix_CloseAudio();
    #else
        FSOUND_Close();
    #endif
}

void clearmapsounds()
{
    loopv(soundlocs) if(soundlocs[i].inuse && soundlocs[i].ent)
    {
        #ifdef USE_MIXER
            if(Mix_Playing(i)) Mix_HaltChannel(i);
        #else
            if(FSOUND_IsPlaying(i)) FSOUND_StopSound();
        #endif
        soundlocs[i].sleep();
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
VAR(footstepradius, 0, 16, 25);
VAR(footstepalign, 5, 15, 4000);

int findsoundloc(int sound, physent *p) 
{ 
    loopv(soundlocs) if(soundlocs[i].pse == p && soundlocs[i].slot == &gamesounds[sound]) return i;
    return -1;
}

void checkplayerloopsounds()
{
    if(findsoundloc(S_FOOTSTEPS, player1) == -1) playsound(S_FOOTSTEPS, player1);
    loopv(players)
    {
        playerent *p = players[i];
        if(!p) continue;

        int idx = findsoundloc(S_FOOTSTEPS, p);
        bool inuse = (idx >= 0 && soundlocs[idx].inuse);

        if(camera1->o.dist(p->o) < footstepradius && footsteps)
        {
            if(!inuse) // start sound
            {
                // sync to model animation
                int basetime = -((int)(size_t)p&0xFFF); 
                int time = lastmillis-basetime;
                int speed = int(1860/p->maxspeed);
                // TODO: share with model code
                if(time%speed < footstepalign) playsound(S_FOOTSTEPS, p);
            }
        }
        else if(inuse) // stop sound
        {
            Mix_HaltChannel(idx);
            soundlocs[idx].sleep(); 
        }
    }
}

VAR(stereo, 0, 1, 1);

void updatechanvol(int chan, int svol, soundloc *sl)
{
    int vol = soundvol, pan = 255/2;
    if(sl)
    {
        vec v;
        float dist = camera1->o.dist(sl->loc(), v);
        
        int rad = sl->radius;
        if(rad >= 0)
        {
            int size = sl->size;
            if(size > 0)
            {
                rad -= size;
                dist -= size;
            }
            vol -= (int)(min(max(dist/rad, 0), 1)*soundvol);
        }
        else
        {
            vol -= (int)(dist*3*soundvol/255); // simple mono distance attenuation
            if(vol<0) vol = 0;
        }
        if(sl->pse) // control looping physent sound volume
        {
            if(sl->slot == &gamesounds[S_FOOTSTEPS]) // control footsteps
            {
                if(sl->pse->type == ENT_PLAYER || sl->pse->type == ENT_BOT)
                {
                    playerent *p = (playerent *)sl->pse;
                    ASSERT(p);
                    bool nofootsteps = !footsteps || p->state != CS_ALIVE || lastmillis-p->lastpain < 300 || (!p->onfloor && p->timeinair>50) || (!p->move && !p->strafe);
                    if(nofootsteps) vol = 0; // fade out instead?
                }
            }
        }
        if(stereo && (v.x != 0 || v.y != 0) && dist>0)
        {
            float yaw = -atan2f(v.x, v.y) - camera1->yaw*RAD; // relative angle of sound along X-Y axis
            pan = int(255.9f*(0.5f*sinf(yaw)+0.5f)); // range is from 0 (left) to 255 (right)
        }
    }
    vol = (vol*MAXVOL*svol)/255/255;
    vol = min(vol, MAXVOL);
    #ifdef USE_MIXER
        Mix_Volume(chan, vol);
        Mix_SetPanning(chan, 255-pan, pan);
    #else
        FSOUND_SetVolume(chan, vol);
        FSOUND_SetPan(chan, pan);
    #endif
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
}

void updatevol()
{
    if(nosound) return;
    loopv(soundlocs) if(soundlocs[i].inuse)
    {
        soundloc &sl = soundlocs[i];
        #ifdef USE_MIXER
            if(Mix_Playing(i))
        #else
            if(FSOUND_IsPlaying(i))
        #endif
                updatechanvol(i, soundlocs[i].slot->vol, &sl);
            else sl.sleep();

    }
#ifndef USE_MIXER
    if(mod && FMUSIC_IsFinished(mod)) musicdone();
    else if(stream && !FSOUND_IsPlaying(musicchan)) musicdone();
#else
    if(mod && !Mix_PlayingMusic()) musicdone();
#endif
}

VARP(maxsoundsatonce, 0, 5, 100);

void playsound(int n, physent *p, entity *ent, const vec *loc)
{
    if(nosound) return;
    if(!soundvol) return;

    if(!ent) // fixme
    {
        static int soundsatonce = 0, lastsoundmillis = 0;
        if(totalmillis==lastsoundmillis) soundsatonce++; else soundsatonce = 1;
        lastsoundmillis = totalmillis;
        if(maxsoundsatonce && soundsatonce>maxsoundsatonce) return;  // avoid bursts of sounds with heavy packetloss and in sp
    }

    vector<soundslot> &sounds = ent ? mapsounds : gamesounds;
    if(!sounds.inrange(n)) { conoutf("unregistered sound: %d", n); return; }
    soundslot &slot = sounds[n];
    if(ent && slot.maxuses && slot.uses>=slot.maxuses) return;

    if(!slot.s->sound)
    {
        const char *exts[] = { "", ".wav", ".ogg" };
        string buf;
        loopi(sizeof(exts)/sizeof(exts[0]))
        {
            s_sprintf(buf)("packages/audio/sounds/%s%s", slot.s->name, exts[i]);
            const char *file = findfile(path(buf), "rb");
            #ifdef USE_MIXER
                slot.s->sound = Mix_LoadWAV(file);
            #else
                slot.s->sound = FSOUND_Sample_Load(ent ? n+gamesounds.length() : n, file, slot.loop ? FSOUND_LOOP_NORMAL : FSOUND_LOOP_OFF, 0, 0);
            #endif
            if(slot.s->sound) break;
        }

        if(!slot.s->sound) { conoutf("failed to load sample: %s", buf); return; }
    }

    #ifdef USE_MIXER
        int chan = Mix_PlayChannel(-1, slot.s->sound, slot.loop ? -1 : 0);
    #else
        int chan = FSOUND_PlaySoundEx(FSOUND_FREE, slot.s->sound, NULL, true);
    #endif
    if(chan<0) return;

    soundloc *sl = p || ent || loc ? newsoundloc(n, chan, &slot, p, ent, loc) : NULL;
    updatechanvol(chan, slot.vol, sl);
    #ifndef USE_MIXER
        FSOUND_SetPaused(chan, false);
    #endif
}

void playsoundname(char *s, const vec *loc, int vol) 
{ 
    if(!vol) vol = 100;
    int id = findsound(s, vol, gamesounds);
    if(id < 0) id = addsound(s, vol, 0, false, gamesounds);
    playsound(id, NULL, NULL, loc);
}

void sound(int *n) { playsound(*n); }
COMMAND(sound, ARG_1INT);

void playsoundc(int n)
{ 
    addmsg(SV_SOUND, "i", n);
    playsound(n);
}
