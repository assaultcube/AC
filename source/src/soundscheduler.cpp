// AC sound scheduler, manages available sound sources.
// It keeps a set of reserved sources for each priority level to avoid running out of sources by floods of low-priority sounds.
// Under load it uses priority and distance information to reassign its resources.

#include "cube.h"

#define DEBUGCOND (audiodebug==1)

VARP(soundschedpriorityscore, 0, 100, 1000);
VARP(soundscheddistancescore, 0, 5, 1000);
VARP(soundschedoldbonus, 0, 100, 1000);
VARP(soundschedreserve, 0, 2, 100);

sourcescheduler *sourcescheduler::inst;

sourcescheduler::sourcescheduler()
{
}

sourcescheduler &sourcescheduler::instance()
{
    if(inst==NULL) inst = new sourcescheduler();
    return *inst;
}

void sourcescheduler::init(int numsoundchannels)
{
    this->numsoundchannels = numsoundchannels;
    int newchannels = numsoundchannels - sources.length();
    if(newchannels < 0)
    {
        loopv(sources)
        {
            source *src = sources[i];
            if(src->locked) continue;
            sources.remove(i--);
            delete src;
            if(sources.length() <= numsoundchannels) break;
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

void sourcescheduler::reset()
{
    loopv(sources) sources[i]->reset();
    sources.deletecontents();
}

// returns a free sound source (channel)
// consuming code must call sourcescheduler::releasesource() after use

source *sourcescheduler::newsource(int priority, const vec &o)
{
    if(!sources.length()) 
    {
        DEBUG("empty source collection");
        return NULL;
    }

    source *src = NULL;

    // reserve some sources for sounds of higher priority
    int reserved = (SP_HIGHEST-priority)*soundschedreserve;
    DEBUGVAR(reserved);
    
    // search unused source
    loopv(sources) 
    {
        if(!sources[i]->locked && reserved--<=0)
        {
            src = sources[i];
            DEBUGVAR(src);
            break;
        }
    }

    if(!src) 
    {
        DEBUG("no empty source found");

        // low priority sounds can't replace others
        if(SP_LOW==priority)
        {
            DEBUG("low prio sound aborted");
            return NULL;
        }

        // try replacing a used source
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
            
            const vec & otherpos = s->position();
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
            src->onreassign(); // inform previous owner about the take-over
            DEBUG("replaced sound of same prio");
        }
    }

    if(!src) 
    {
        DEBUG("sound aborted, no channel takeover possible");
        return NULL;
    }

    src->reset();       // default settings
    src->lock();        // exclusive lock
    src->priority = priority;
    return src;
}

// give source back to the pool
void sourcescheduler::releasesource(source *src)
{
    ASSERT(src);
    ASSERT(src->locked); // detect double release

    if(!src) return;
    
    DEBUG("unlocking source");
    
    src->unlock();

    if(sources.length() > numsoundchannels)
    {
        sources.removeobj(src);
        delete src;
        DEBUG("source deleted");
    }
}


