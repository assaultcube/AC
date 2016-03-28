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

enum { LOGTARGET_CONSOLE = 1, LOGTARGET_FILE = 2, LOGTARGET_SYSLOG = 4 };

static char loglevelenabled[ACLOG_NUM];

bool initlogging(const char *identity, int facility_, int consolethres, int filethres, int syslogthres, bool logtimestamp, const char *logfilepath)
{
    static void *logworkerthread_id = NULL;
    extern int logworkerthread(void *nop);
    if(!logworkerthread_id) logworkerthread_id = sl_createthread(logworkerthread, NULL); // always start, if dedicated server
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
    formatstring(filepath)("%sserverlog_%s_%s.txt", logfilepath, timestring(true), identity);
    path(filepath);
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
    loopi(ACLOG_NUM) loglevelenabled[i] = (consolethreshold <= i ? LOGTARGET_CONSOLE : 0) | (fp && filethreshold <= i ? LOGTARGET_FILE : 0) | (syslogthreshold <= i ? LOGTARGET_SYSLOG : 0);
    if(enabled) printf("%s\n", msg);
    return enabled;
}

void exitlogging()
{
    loopi(ACLOG_NUM) loglevelenabled[i] = 0;
    if(fp) { fclose(fp); fp = NULL; }
#ifdef AC_USE_SYSLOG
    if(enabled && syslogthreshold < ACLOG_NUM) closelog();
#endif
    syslogthreshold = ACLOG_NUM;
    enabled = false;
}

bool logcheck(int level)
{
    ASSERT(level >= 0 && level < ACLOG_NUM);
    return loglevelenabled[level] != 0;
}

struct logline_s { int level; time_t t; string msg; };

static ringbuf<logline_s *, 1024> mainlog; // increase this number, if log overflows occur
static ringbuf<logline_s *, 32> threadlog;
static int mainlogoverflow = 0;
static sl_semaphore threadlogsem(1, NULL), threadlogfullsem(0, NULL), mainlogsem(0, NULL);

extern int stat_mainlog_peaklevel;

int logworkerthread(void *nop)
{
    string tmp;
    for(;;)
    {
        while(mainlog.empty()) mainlogsem.wait();
        int mainloglevel = (100 * mainlog.length()) / mainlog.maxsize();
        if(mainloglevel > stat_mainlog_peaklevel) stat_mainlog_peaklevel = mainloglevel;
        logline_s *ll = mainlog.remove();
        int targets = loglevelenabled[ll->level];
        bool logtocon = (targets & LOGTARGET_CONSOLE) != 0, logtofile = fp && (targets & LOGTARGET_FILE) != 0, logtosyslog = (targets & LOGTARGET_SYSLOG) != 0;
        const char *ts = timestamp ? timestring(ll->t, true, "%b %d %H:%M:%S ", tmp) : "", *ld = levelprefix[ll->level];
        char *p, *l = ll->msg;
        do
        { // break into single lines first
            if((p = strchr(l, '\n'))) *p = '\0';
            if(logtocon) printf("%s%s%s\n", ts, ld, l);
            if(logtofile) fprintf(fp, "%s%s%s\n", ts, ld, l);
            if(logtosyslog)
#ifdef AC_USE_SYSLOG
                syslog(levels[ll->level], "%s", l);
#else
            {
                defformatstring(text)("<%d>%s: %s", (16 + facility) * 8 + levels[ll->level], ident, l); // no TIMESTAMP, no hostname: syslog will add this
                ENetBuffer buf;
                buf.data = text;
                buf.dataLength = strlen(text);
                enet_socket_send(logsock, &logdest, &buf, 1);
            }
#endif
            l = p + 1;
        }
        while(p);
#ifdef _DEBUG
        if(logtocon) fflush(stdout);
        if(logtofile) fflush(fp);
#endif
        delete ll;
    }
    return 0;
}

void mlog(int level, const char *msg, ...) // log line from main thread (never blocks)
{
    ASSERT(level >= 0 && level < ACLOG_NUM && ismainthread());
    if(!loglevelenabled[level]) return;
    if(mainlog.full()) { mainlogoverflow++; return; } // do. not. block.
    logline_s *ll = mainlog.stage(new logline_s);
    vformatstring(ll->msg, msg, msg);
    filtertext(ll->msg, ll->msg, FTXT__LOG);
    ll->t = time(NULL);
    ll->level = level;
    mainlog.commit();
    if(mainlogsem.getvalue() < 1) mainlogsem.post();
}

void tlog(int level, const char *msg, ...) // log line from other-than-main thread (may block)
{
    ASSERT(level >= 0 && level < ACLOG_NUM && !ismainthread());
    if(!loglevelenabled[level]) return;
    threadlogsem.wait(); // properly lock, so different threads can use this
    while(threadlog.full()) threadlogfullsem.wait(); // wait for buffer to clear
    logline_s *ll = threadlog.stage(new logline_s);
    vformatstring(ll->msg, msg, msg);
    filtertext(ll->msg, ll->msg, FTXT__LOG);
    ll->t = time(NULL);
    ll->level = level;
    threadlog.commit();
    threadlogsem.post();
}

void xlog(int level, const char *msg, ...) // log line from any thread (more expensive: only use for functions, that are called from main and other threads)
{
    ASSERT(level >= 0 && level < ACLOG_NUM);
    if(!loglevelenabled[level]) return;
    bool ismain = ismainthread();
    if(ismain)
    {
        if(mainlog.full()) { mainlogoverflow++; return; } // do. not. block.
    }
    else
    {
        threadlogsem.wait(); // properly lock, so different threads can use this
        while(threadlog.full()) threadlogfullsem.wait(); // wait for buffer to clear
    }
    logline_s *ll = ismain ? mainlog.stage(new logline_s) : threadlog.stage(new logline_s);
    vformatstring(ll->msg, msg, msg);
    filtertext(ll->msg, ll->msg, FTXT__LOG);
    ll->t = time(NULL);
    ll->level = level;
    if(ismain)
    {
        mainlog.commit();
        if(mainlogsem.getvalue() < 1) mainlogsem.post();
    }
    else
    {
        threadlog.commit();
        threadlogsem.post();
    }
}

extern int stat_theadlog_peaklevel;

void poll_logbuffers() // just copy threadlog to mainlog and restart blocked threads, if necessary
{
    int threadloglevel = (100 * threadlog.length()) / threadlog.maxsize();
    if(threadloglevel > stat_theadlog_peaklevel) stat_theadlog_peaklevel = threadloglevel;
    while(!mainlog.full() && !threadlog.empty())
    {
        mainlog.stage(threadlog.remove());
        mainlog.commit();
    }
    if(mainlogoverflow && !mainlog.full())
    {
        mlog(ACLOG_ERROR, "main log ringbuffer overflow, %d entries lost", mainlogoverflow);
        mainlogoverflow = 0;
    }
    if(!threadlog.full() && threadlogfullsem.getvalue() < 1) threadlogfullsem.post();
    if(!mainlog.empty() && mainlogsem.getvalue() < 1) mainlogsem.post();
}
