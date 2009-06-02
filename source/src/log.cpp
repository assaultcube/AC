// server-side logging of events

#include "pch.h"
#include "cube.h"

#if !defined(WIN32) && !defined(__APPLE__)

    #include <syslog.h>
    #include <signal.h>

    #define AC_USE_SYSLOG

    static const int facilities[] = { LOG_LOCAL0, LOG_LOCAL1, LOG_LOCAL2, LOG_LOCAL3, LOG_LOCAL4, LOG_LOCAL5, LOG_LOCAL6, LOG_LOCAL7 };
    static const int levels[] = { LOG_DEBUG, LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERR };
#endif

static const char *leveldesc[] = { "", "", "", "WARNING: ", "ERROR: " };
static FILE *fp = NULL;
static string filepath, ident;
static int facility = -1,
#ifdef AC_USE_SYSLOG
        filethreshold = ACLOG_NUM,
        syslogthreshold = ACLOG_INFO,
#else
        filethreshold = ACLOG_INFO,
        syslogthreshold = ACLOG_NUM,
#endif
    consolethreshold = ACLOG_INFO;
static bool timestamp = false, enabled = false;

bool initlogging(const char *identity, int facility_, int consolethres, int filethres, int syslogthres, bool logtimestamp)
{
    facility = facility_;
    timestamp = logtimestamp;
    if(consolethres >= 0) consolethreshold = min(consolethres, (int)ACLOG_NUM);
    if(filethres >= 0) filethreshold = min(filethres, (int)ACLOG_NUM);
#ifdef AC_USE_SYSLOG
    if(syslogthres >= 0) syslogthreshold = min(syslogthres, (int)ACLOG_NUM);
    if(syslogthreshold < ACLOG_NUM)
    {
        facility &= 7;
        s_sprintf(ident)("AssaultCube%s", identity);
        openlog(ident, LOG_NDELAY, facilities[facility]);
    }
#endif
    s_sprintf(filepath)("serverlog_%s_%s.txt", timestring(true), identity);
    if(fp) { fclose(fp); fp = NULL; }
    if(filethreshold < ACLOG_NUM)
    {
        fp = fopen(filepath, "w");
        if(!fp) printf("failed to open \"%s\" for writing\n", filepath);
    }
    enabled = consolethreshold < ACLOG_NUM || fp || syslogthreshold < ACLOG_NUM;
    return enabled;
}

void exitlogging()
{
    if(fp) { fclose(fp); fp = NULL; }
#ifdef AC_USE_SYSLOG
    if(syslogthreshold < ACLOG_NUM) closelog();
#endif
    syslogthreshold = ACLOG_NUM;
    enabled = false;
}

bool logline(int level, const char *msg, ...)
{
    if(!enabled) return false;
    if(level < 0 || level >= ACLOG_NUM) return false;
    s_sprintfdv(sf, msg);
    filtertext(sf, sf, 2);
    const char *ts = timestamp ? timestring(true, "%b %d %H:%M:%S ") : "", *ld = leveldesc[level];
    char *p, *l = sf;
    do
    { // break into single lines first
        if((p = strchr(l, '\n'))) *p = '\0';
        if(consolethreshold <= level) printf("%s%s%s\n", ts, ld, l);
        if(fp && filethreshold <= level) fprintf(fp, "%s%s%s\n", ts, ld, l);
#ifdef AC_USE_SYSLOG
        if(syslogthreshold <= level) syslog(levels[level], "%s", l);
#endif
        l = p + 1;
    }
    while(p);
    if(consolethreshold <= level) fflush(stdout);
    if(fp && filethreshold <= level) fflush(fp);
    return consolethreshold <= level;
}
