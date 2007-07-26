// sound.cpp: uses fmod on windows and sdl_mixer on unix (both had problems on the other platform)

#include "cube.h"
#include "bot/bot.h"

//#ifndef WIN32    // NOTE: fmod not being supported for the moment as it does not allow stereo pan/vol updating during playback
#define USE_MIXER
//#endif

bool nosound = true;

#define MAXCHAN 32
#define SOUNDFREQ 22050

#ifdef USE_MIXER
    #include "SDL_mixer.h"
    #define MAXVOL MIX_MAX_VOLUME
    Mix_Music *mod = NULL;
    void *stream = NULL;
#else
    #include "fmod.h"
    #define MAXVOL 255
    FMUSIC_MODULE *mod = NULL;
    FSOUND_STREAM *stream = NULL;
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

    sample(const char *name) : name(newstring(name)), sound(NULL) {}
    ~sample() { DELETEA(name); }
};

struct soundloc { vec loc; bool inuse; };
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

VARP(soundvol, 0, 150, 255);
VARFP(musicvol, 0, 128, 255, setmusicvol(musicvol));

void stopsound()
{
    if(nosound) return;
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

void music(char *name)
{
    if(nosound) return;
    stopsound();
    if(soundvol && musicvol)
    {
        s_sprintfd(sn)("packages/audio/songs/%s", name);
        const char *file = findfile(path(sn), "rb");
        #ifdef USE_MIXER
            if((mod = Mix_LoadMUS(file)))
            {
                Mix_PlayMusic(mod, -1);
                Mix_VolumeMusic((musicvol*MAXVOL)/255);
            }
        #else
            if((mod = FMUSIC_LoadSong(file)))
            {
                FMUSIC_PlaySong(mod);
                FMUSIC_SetMasterVolume(mod, musicvol);
            }
            else if((stream = FSOUND_Stream_Open(file, FSOUND_LOOP_NORMAL, 0, 0)))
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

COMMAND(music, ARG_1STR);

vector<sample *> samples;

int registersound(char *name)
{
    loopv(samples) if(!strcmp(samples[i]->name, name)) return i;
    samples.add(new sample(name));
    return samples.length()-1;
}

COMMAND(registersound, ARG_1EST);

void cleansound()
{
    if(nosound) return;
    stopsound();
    #ifdef USE_MIXER
        Mix_CloseAudio();
    #else
        FSOUND_Close();
    #endif
}

VAR(stereo, 0, 1, 1);

void updatechanvol(int chan, vec *loc)
{
    int vol = soundvol, pan = 255/2;
    if(loc)
    {
        vec v;
        float dist = camera1->o.dist(*loc, v);
        vol -= (int)(dist*3*soundvol/255); // simple mono distance attenuation
        if(vol<0) vol = 0;
        if(stereo && (v.x != 0 || v.y != 0))
        {
            float yaw = -atan2f(v.x, v.y) - camera1->yaw*RAD; // relative angle of sound along X-Y axis
            pan = int(255.9f*(0.5f*sinf(yaw)+0.5f)); // range is from 0 (left) to 255 (right)
        }
    }
    vol = (vol*MAXVOL)/255;
    #ifdef USE_MIXER
        Mix_Volume(chan, vol);
        Mix_SetPanning(chan, 255-pan, pan);
    #else
        FSOUND_SetVolume(chan, vol);
        FSOUND_SetPan(chan, pan);
    #endif
}  

void newsoundloc(int chan, vec *loc)
{
    while(chan >= soundlocs.length()) soundlocs.add().inuse = false;
    soundlocs[chan].loc = *loc;
    soundlocs[chan].inuse = true;
}

void updatevol()
{
    if(nosound) return;
    loopv(soundlocs) if(soundlocs[i].inuse)
    {
        #ifdef USE_MIXER
            if(Mix_Playing(i))
        #else
            if(FSOUND_IsPlaying(i))
        #endif
                updatechanvol(i, &soundlocs[i].loc);
            else soundlocs[i].inuse = false;
    }
}

void playsoundc(int n)
{ 
    if(demoplayback) return;
    addmsg(SV_SOUND, "i", n);
    playsound(n);
}

void playsound(int n, vec *loc)
{
    if(nosound) return;
    if(!soundvol) return;
	if(n==S_NULL) return;
	if(m_botmode) BotManager.LetBotsHear(n, loc);

    static int soundsatonce = 0, lastsoundmillis = 0;
    if(lastmillis==lastsoundmillis) soundsatonce++; else soundsatonce = 1;
    lastsoundmillis = lastmillis;
    if(soundsatonce>5) return;  // avoid bursts of sounds with heavy packetloss and in sp

    if(!samples.inrange(n)) { conoutf("unregistered sound: %d", n); return; }

    sample &s = *samples[n];
    if(!s.sound)
    {
        s_sprintfd(buf)("packages/audio/sounds/%s", s.name);

        loopi(2)
        {
            if(i) s_strcat(buf, ".wav");
            const char *file = findfile(path(buf), "rb");
            #ifdef USE_MIXER
                s.sound = Mix_LoadWAV(file);
            #else
                s.sound = FSOUND_Sample_Load(n, file, FSOUND_LOOP_OFF, 0, 0);
            #endif
            if(s.sound) break;
        }

        if(!s.sound) { conoutf("failed to load sample: %s", buf); return; }
    }
    
    #ifdef USE_MIXER
        int chan = Mix_PlayChannel(-1, s.sound, 0);
    #else
        int chan = FSOUND_PlaySoundEx(FSOUND_FREE, s.sound, NULL, true);
    #endif
    if(chan<0) return;
    if(loc) newsoundloc(chan, loc);
    updatechanvol(chan, loc);
    #ifndef USE_MIXER
        FSOUND_SetPaused(chan, false);
    #endif
}

void sound(int n) { playsound(n, NULL); }
COMMAND(sound, ARG_1INT);
