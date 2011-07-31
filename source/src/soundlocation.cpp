// short living sound occurrence, dies once the sound stops

#include "cube.h"

#define DEBUGCOND (audiodebug==1)

VARP(gainscale, 1, 90, 100);
int warn_about_unregistered_sound = 0;
location::location(int sound, const worldobjreference &r, int priority) : cfg(NULL), src(NULL), ref(NULL), stale(false), playmillis(0)
{
    vector<soundconfig> &sounds = (r.type==worldobjreference::WR_ENTITY ? mapsounds : gamesounds);
    if(!sounds.inrange(sound)) 
    { 
        if (lastmillis - warn_about_unregistered_sound > 30 * 1000) // delay message to every 30 secs so console is not spammed.
        {
            // occurs when a map contains an ambient sound entity, but sound entity is not found in map cfg file.
            conoutf("\f3ERROR: this map contains at least one unregistered ambient sound (sound entity# %d)", sound);
            warn_about_unregistered_sound = lastmillis;
        }
        stale = true;
        return;
    }

    // get sound config
    cfg = &sounds[sound];
    cfg->onattach();
    const float dist = camera1->o.dist(r.currentposition());
    if((r.type==worldobjreference::WR_ENTITY && cfg->maxuses >= 0 && cfg->uses >= cfg->maxuses) || cfg->muted || (cfg->audibleradius && dist>cfg->audibleradius)) // check max-use limits and audible radius
    {
        stale = true;
        return; 
    }

    // assign buffer
    sbuffer *buf = cfg->buf;
    if(!buf || !buf->id) 
    {
        stale = true;
        return;
    }

    // obtain source
    src = sourcescheduler::instance().newsource(priority, r.currentposition());
    // apply configuration
    if(!src || !src->valid || !src->buffer(cfg->buf->id) || !src->looping(cfg->loop) || !setvolume(1.0f))
    {
        stale = true;
        return;
    }
    src->init(this);

    // set position
    attachworldobjreference(r);
}

location::~location() 
{
    if(src) sourcescheduler::instance().releasesource(src);
    if(cfg) cfg->ondetach(); 
    if(ref)
    {
        ref->detach();
        DELETEP(ref);
    }
}

// attach a reference to a world object to get the 3D position from

void location::attachworldobjreference(const worldobjreference &r)
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
void location::evaluateworldobjref()
{
    src->sourcerelative(ref->nodistance());
}

// marks itself for deletion if source got lost
void location::onsourcereassign(source *s)
{
    if(s==src)
    {
        stale = true; 
        src = NULL;
    }
}

void location::updatepos()
{
    ASSERT(!stale && ref);
    if(stale) return;

    const vec &pos = ref->currentposition();

    // forced fadeout radius
    bool volumeadjust = (cfg->model==soundconfig::DM_LINEAR);
    float forcedvol = 1.0f;
    if(volumeadjust)
    {
        float dist = camera1->o.dist(pos);
        if(dist>cfg->audibleradius) forcedvol = 0.0f;
        else if(dist<0) forcedvol = 1.0f;
        else forcedvol = 1.0f-(dist/cfg->audibleradius);
    }

    // reference determines the used model
    switch(ref->type)
    {
        case worldobjreference::WR_CAMERA: break;
        case worldobjreference::WR_PHYSENT:
        {
            if(!ref->nodistance()) src->position(pos);
            if(volumeadjust) setvolume(forcedvol);
            break;
        }
        case worldobjreference::WR_ENTITY:
        {
            entityreference &eref = *(entityreference *)ref;
            const float vol = eref.ent->attr4<=0.0f ? 1.0f : eref.ent->attr4/255.0f;
            float dist = camera1->o.dist(pos);

            if(ref->nodistance())
            {
                // own distance model for entities/mapsounds: linear & clamping
                
                const float innerradius = float(eref.ent->attr3); // full gain area / size property
                const float outerradius = float(eref.ent->attr2); // fading gain area / radius property

                if(dist <= innerradius) src->gain(1.0f*vol); // inside full gain area
                else if(dist <= outerradius) // inside fading gain area
                {
                    const float fadeoutdistance = outerradius-innerradius;
                    const float fadeout = dist-innerradius;
                    src->gain((1.0f - fadeout/fadeoutdistance)*vol);
                }
                else src->gain(0.0f); // outside entity
            }
            else
            {
                // use openal distance model to make the sound appear from a certain direction (non-ambient)
                src->position(pos);
                src->gain(vol);
            }
            break;
        }
        case worldobjreference::WR_STATICPOS:
        {
            if(!ref->nodistance()) src->position(pos);
            if(volumeadjust) setvolume(forcedvol);
            break;
        }
    }
}

void location::update()
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
            DEBUG("location is stale");
            break;
    }
}

void location::play(bool loop)
{
    if(stale) return;

    updatepos();
    if(loop) src->looping(loop);
    if(src->play()) playmillis = totalmillis;
}

void location::pitch(float p)
{
    if(stale) return;
    src->pitch(p);
}

bool location::setvolume(float v)
{
    if(stale) return false;
    return src->gain(cfg->vol/100.0f*((float)gainscale)/100.0f*v);
}

void location::offset(float secs)
{
    ASSERT(!stale);
    if(stale) return;
    src->secoffset(secs);
}

float location::offset()
{
    ASSERT(!stale);
    if(stale) return 0.0f;
    return src->secoffset();
}

void location::drop() 
{
    src->stop();
    stale = true; // drop from collection on next update cycle
}


// location collection

location *locvector::find(int sound, worldobjreference *ref, const vector<soundconfig> &soundcollection /* = gamesounds*/)
{ 
    if(sound<0 || sound>=soundcollection.length()) return NULL;
    loopi(ulen) if(buf[i] && !buf[i]->stale)
    {
        if(buf[i]->cfg != &soundcollection[sound]) continue; // check if its the same sound
        if(ref && *buf[i]->ref!=*ref) continue; // optionally check if its the same reference
        return buf[i]; // found
    }
    return NULL;
}

void locvector::delete_(int i)
{
    delete remove(i);
}

void locvector::replaceworldobjreference(const worldobjreference &oldr, const worldobjreference &newr)
{
    loopv(*this)
    {
        location *l = buf[i];
        if(!l || !l->ref) continue;
        if(*l->ref==oldr) l->attachworldobjreference(newr);
    }
}

// update stuff, remove stale data
void locvector::updatelocations()
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
void locvector::forcepitch(float pitch)
{
    loopv(*this) 
    {
        location *l = buf[i];
        if(!l) continue;
        if(l->src && l->src->locked) l->src->pitch(pitch);
    }
}

// delete all sounds except world-neutral sounds like GUI/notification
void locvector::deleteworldobjsounds()
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
};

