// server-side logging of events

#include "cube.h"

#if !defined(WIN32) && !defined(__APPLE__)

    #include <syslog.h>
    #include <signal.h>

    #define AC_USE_SYSLOG

    static const int facilities[] = { LOG_LOCAL0, LOG_LOCAL1, LOG_LOCAL2, LOG_LOCAL3, LOG_LOCAL4, LOG_LOCAL5, LOG_LOCAL6, LOG_LOCAL7 };
    static const int levels[] = { LOG_DEBUG, LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERR };
#else
    static const int levels[] = { 7, 7, 6, 4, 3 };
    ENetSocket logsock = ENET_SOCKET_NULL;
    ENetAddress logdest = { ENET_HOST_ANY, 514 };
#endif

static const char *levelprefix[] = { "", "", "", "WARNING: ", "ERROR: " };
static const char *levelname[] = { "DEBUG", "VERBOSE", "INFO", "WARNING", "ERROR", "DISABLED" };
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
    if(syslogthres >= 0) syslogthreshold = min(syslogthres, (int)ACLOG_NUM);
    facility &= 7;
    formatstring(ident)("AssaultCube[%s]", identity);
    if(syslogthreshold < ACLOG_NUM)
    {
#ifdef AC_USE_SYSLOG
        openlog(ident, LOG_NDELAY, facilities[facility]);
#else
        if((logsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM)) == ENET_SOCKET_NULL || enet_address_set_host(&logdest, "localhost") < 0) syslogthreshold = ACLOG_NUM;
#endif
    }
    formatstring(filepath)("serverlog_%s_%s.txt", timestring(true), identity);
    if(fp) { fclose(fp); fp = NULL; }
    if(filethreshold < ACLOG_NUM)
    {
        fp = fopen(filepath, "w");
        if(!fp) printf("failed to open \"%s\" for writing\n", filepath);
    }
    defformatstring(msg)("logging started: console(%s), file(%s", levelname[consolethreshold], levelname[fp ? filethreshold : ACLOG_NUM]);
    if(fp) concatformatstring(msg, ", \"%s\"", filepath);
    concatformatstring(msg, "), syslog(%s", levelname[syslogthreshold]);
    if(syslogthreshold < ACLOG_NUM) concatformatstring(msg, ", \"%s\", local%d", ident, facility);
    concatformatstring(msg, "), timestamp(%s)", timestamp ? "ENABLED" : "DISABLED");
    enabled = consolethreshold < ACLOG_NUM || fp || syslogthreshold < ACLOG_NUM;
    if(enabled) printf("%s\n", msg);
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
    defvformatstring(sf, msg, msg);
    filtertext(sf, sf, 2);
    const char *ts = timestamp ? timestring(true, "%b %d %H:%M:%S ") : "", *ld = levelprefix[level];
    char *p, *l = sf;
    do
    { // break into single lines first
        if((p = strchr(l, '\n'))) *p = '\0';
        if(consolethreshold <= level) printf("%s%s%s\n", ts, ld, l);
        if(fp && filethreshold <= level) fprintf(fp, "%s%s%s\n", ts, ld, l);
        if(syslogthreshold <= level)
#ifdef AC_USE_SYSLOG
            syslog(levels[level], "%s", l);
#else
        {
            defformatstring(text)("<%d>%s: %s", (16 + facility) * 8 + levels[level], ident, l); // no TIMESTAMP, no hostname: syslog will add this
            ENetBuffer buf;
            buf.data = text;
            buf.dataLength = strlen(text);
            enet_socket_send(logsock, &logdest, &buf, 1);
        }
#endif
        l = p + 1;
    }
    while(p);
    if(consolethreshold <= level) fflush(stdout);
    if(fp && filethreshold <= level) fflush(fp);
    return consolethreshold <= level;
}
