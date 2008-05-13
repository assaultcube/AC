// server-side logging of events

#include "pch.h"
#include "cube.h"

struct filelog : log
{
    FILE *file;
    string filepath;
    
    filelog(const char *filepath) 
    {
        s_strcpy(this->filepath, filepath);
    }

    ~filelog()
    {
        close();
    }

    virtual void writeline(int level, const char *msg, ...)
    {
        if(!enabled) return;
        s_sprintfdv(sf, msg);
        filtertext(sf, sf);
        
        if(console)
        {
            puts(sf);
            fflush(stdout);
        }
        if(file) 
        {
            fprintf(file, "%s\n", sf);
            fflush(file);
        }
    }

    virtual void open() 
    { 
        if(file) return;
        file = fopen(filepath, "w");
        if(file) enabled = true;
    }

    virtual void close()
    {
        if(!file) return;
        fclose(file);
        enabled = false;
    }
};


#if !defined(WIN32) && !defined(__APPLE__)

#include <syslog.h>
#include <signal.h>

struct posixsyslog : log
{
    int facility;
    string ident;

    posixsyslog(int facility, const char *ident)
    {
        this->facility = facility;
        s_strcpy(this->ident, ident);
    }

    ~posixsyslog()
    {
        close();
    }

    virtual void writeline(int level, const char *msg, ...)
    {
        if(!enabled) return;
        s_sprintfdv(sf, msg);
        filtertext(sf, sf);
        int l = (level==log::info ? LOG_INFO : ( level==log::warning ? LOG_WARNING : LOG_ERR));
        syslog(l, sf);
        if(console)
        {
            puts(sf);
            fflush(stdout);
        }
    }

    virtual void open() 
    { 
        if(enabled) return;
        openlog(ident, LOG_NDELAY, facility);
        enabled = true;
    }

    virtual void close()
    {
        if(!enabled) return;
        closelog();
        enabled = false;
    }
};

struct log *newlogger(const char *identity)
{
    s_sprintfd(id)("AssaultCube %s", identity);
    return new posixsyslog(LOG_LOCAL6, id);
}

#else

struct log *newlogger(const char *identity)
{
    s_sprintfd(file)("serverlog_%s.txt", identity);
    return new filelog(file);
}

#endif
