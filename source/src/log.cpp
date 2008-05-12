// server-side logging of events

#include "pch.h"
#include "cube.h"

struct filelog : log
{
    FILE *file;
    
    filelog(char *filepath) 
    {
        file = fopen(filepath, "w");
    }

    ~filelog()
    {
        if(file) fclose(file);
    }

    virtual void writeline(int level, char *msg, ...)
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


struct syslog : log
{
    virtual void writeline(int level, char *msg, ...)
    {
        
    }
};


struct log *newlogger()
{
    return new filelog("serverlog.txt");
}