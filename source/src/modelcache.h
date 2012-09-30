template<class T> struct modelcacheentry
{
    typedef modelcacheentry<T> entry;

    T *prev, *next, *nextalloc;
    size_t size;
    bool locked;

    modelcacheentry(T *prev = NULL, T *next = NULL) : prev(prev), next(next), nextalloc(NULL), size(0), locked(false) {}

    bool empty() const
    {
        return prev==next;
    }

    void linkbefore(T *pos)
    {
        next = pos;
        prev = pos->entry::prev;
        prev->entry::next = (T *)this;
        next->entry::prev = (T *)this;
    }

    void linkafter(T *pos)
    {
        next = pos->entry::next;
        prev = pos;
        prev->entry::next = (T *)this;
        next->entry::prev = (T *)this;
    }

    void unlink()
    {
        prev->modelcacheentry<T>::next = next;
        next->modelcacheentry<T>::prev = prev;
        prev = next = (T *)this;
    }

    void *getdata()
    {
        return (T *)this + 1;
    }
};

template<class T> struct modelcachelist : modelcacheentry<T>
{
    typedef modelcacheentry<T> entry;

    modelcachelist() : entry((T *)this, (T *)this) {}

    T *start() { return entry::next; }
    T *end() { return (T *)this; }

    void addfirst(entry *e)
    {
        e->linkafter((T *)this);
    }

    void addlast(entry *e)
    {
        e->linkbefore((T *)this);
    }

    void removefirst()
    {
        if(!entry::empty()) entry::next->unlink();
    }

    void removelast()
    {
        if(!entry::empty()) entry::prev->unlink();
    }
};

struct modelcache
{
    struct entry : modelcacheentry<entry> {};

    uchar *buf;
    size_t size;
    entry *curalloc;

    modelcache(size_t size = 0) : buf(size ? new uchar[size] : NULL), size(size), curalloc(NULL) {}
    ~modelcache() { DELETEA(buf); }

    void resize(size_t nsize)
    {
        if(curalloc)
        {
            for(curalloc = (entry *)buf; curalloc; curalloc = curalloc->nextalloc) curalloc->unlink();
        }
        DELETEA(buf);
        buf = nsize ? new uchar[nsize] : 0;
        size = nsize;
    }

    template<class T>
    T *allocate(size_t reqsize)
    {
        reqsize += sizeof(T);
        if(reqsize > size) return NULL;

        if(!curalloc)
        {
            curalloc = (entry *)buf;
            curalloc->size = reqsize;
            curalloc->locked = false;
            curalloc->nextalloc = NULL;
            return (T *)curalloc;
        }

        if(curalloc) for(bool failed = false;;)
        {
            uchar *nextfree = curalloc ? (uchar *)curalloc + curalloc->size : (uchar *)buf;
            entry *nextused = curalloc ? curalloc->nextalloc : (entry *)buf;
            for(;;)
            {
                if(!nextused)
                {
                    if(size_t(&buf[size] - nextfree) >= reqsize) goto success;
                    break;
                }
                else if(size_t((uchar *)nextused - nextfree) >= reqsize) goto success;
                else if(nextused->locked) break;
                nextused->unlink();
                nextused = nextused->nextalloc;
            }
            if(curalloc) curalloc->nextalloc = nextused;
            else if(failed) { curalloc = nextused; break; }
            else failed = true;
            curalloc = nextused;
            continue;

        success:
            entry *result = (entry *)nextfree;
            result->size = reqsize;
            result->locked = false;
            result->nextalloc = nextused;
            if(curalloc) curalloc->nextalloc = result;
            curalloc = result;
            return (T *)curalloc;
        }

        curalloc = (entry *)buf;
        return NULL;
    }

    template<class T>
    void release(modelcacheentry<T> *e)
    {
        e->unlink();
    }
};

