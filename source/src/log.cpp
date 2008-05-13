// server-side logging of events

#include "pch.h"
#include "cube.h"

struct filelog : log
{
    FILE *file;
    
    filelog(const char *filepath) 
    {
        file = fopen(filepath, "w");
    }

    ~filelog()
    {
        if(file) fclose(file);
    }

    virtual void writeline(int level, const char *msg, ...)
    {
        if(!enabled) return;

        s_sprintfdv(sf, msg);
        s_sprintfd(out)("%s\n", sf);
        filtertext(out, out);
        
        if(console)
        {
            puts(out);
            fflush(stdout);
        }
        if(file) 
        {
            fwrite(out, sizeof(char), strlen(out), file);
            fflush(file);
        }
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
	
	openlog(this->ident, LOG_NDELAY, this->facility);
    }

    ~posixsyslog()
    {
	closelog();
    }

    virtual void writeline(int level, const char *msg, ...)
    {
	if(!enabled) return;
	s_sprintfdv(sf, msg);
	s_sprintfd(out)("%s\n", sf);
	filtertext(out, out);
 	int l = (level==log::info ? LOG_INFO : ( level==log::warning ? LOG_WARNING : LOG_ERR));
	syslog(l, out);
	if(console) puts(out);
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
