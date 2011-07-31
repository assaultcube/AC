// code for running the server as a background process/daemon/service

struct servercontroller
{
    virtual void start() = 0;
    virtual void keepalive() = 0;
    virtual void stop() = 0;
    virtual ~servercontroller() {}
    int argc;
    char **argv;
};

#ifdef WIN32

struct winservice : servercontroller
{
    SERVICE_STATUS_HANDLE statushandle;
    SERVICE_STATUS status;
    HANDLE stopevent;
    const char *name;

    winservice(const char *name) : name(name) 
    { 
        callbacks::svc = this; 
        statushandle = 0;
    };

    ~winservice() 
    { 
        if(status.dwCurrentState != SERVICE_STOPPED) stop(); 
        callbacks::svc = NULL;
    }

    void start() // starts the server again on a new thread and returns once the windows service has stopped
    {
        SERVICE_TABLE_ENTRY dispatchtable[] = { { (LPSTR)name, (LPSERVICE_MAIN_FUNCTION)callbacks::main }, { NULL, NULL } };
        if(StartServiceCtrlDispatcher(dispatchtable)) exit(EXIT_SUCCESS);
        else fatal("an error occurred running the AC server as windows service. make sure you start the server from the service control manager and not from the command line.");
    }

    void keepalive()
    { 
        if(statushandle)
        {
            report(SERVICE_RUNNING, 0); 
            handleevents();
        }
    };

    void stop()
    {
        if(statushandle)
        {
            report(SERVICE_STOP_PENDING, 0);
            if(stopevent) CloseHandle(stopevent);
            status.dwControlsAccepted &= ~(SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
            report(SERVICE_STOPPED, 0);    
        }
    }

    void handleevents()
    {
        if(WaitForSingleObject(stopevent, 0) == WAIT_OBJECT_0) stop();
    }

    void WINAPI requesthandler(DWORD ctrl)
    {
        switch(ctrl)
        {
            case SERVICE_CONTROL_STOP:
                report(SERVICE_STOP_PENDING, 0);
                SetEvent(stopevent);
                return;
            default: break;
        }
        report(status.dwCurrentState, 0);
    }

    void report(DWORD state, DWORD wait)
    {
        status.dwCurrentState = state;
        status.dwWaitHint = wait;
        status.dwWin32ExitCode = NO_ERROR;
        status.dwControlsAccepted = SERVICE_START_PENDING == state || SERVICE_STOP_PENDING == state ? 0 : SERVICE_ACCEPT_STOP;
        if(state == SERVICE_RUNNING || state == SERVICE_STOPPED) status.dwCheckPoint = 0;
        else status.dwCheckPoint++;
        SetServiceStatus(statushandle, &status);
    }

    int WINAPI svcmain() // new server thread's entry point
    {
        // fix working directory to make relative paths work
        if(argv && argv[0])
        {
            string procpath;
            copystring(procpath, parentdir(argv[0]));
            copystring(procpath, parentdir(procpath));
            SetCurrentDirectory((LPSTR)procpath);
        }

        statushandle = RegisterServiceCtrlHandler(name, (LPHANDLER_FUNCTION)callbacks::requesthandler);
        if(!statushandle) return EXIT_FAILURE;
        status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        status.dwServiceSpecificExitCode = 0;
        report(SERVICE_START_PENDING, 3000);
        stopevent = CreateEvent(NULL, true, false, NULL);
        if(!stopevent) { stop(); return EXIT_FAILURE; }
        extern int main(int argc, char **argv);
        return main(argc, argv);
    }
   

    struct callbacks
    {
        static winservice *svc;
        static int WINAPI main() { return svc->svcmain(); };
        static void WINAPI requesthandler(DWORD request) { svc->requesthandler(request); };
    };

    /*void log(const char *msg, bool error)
    {
        HANDLE eventsrc = RegisterEventSource(NULL, "AC Server");
        if(eventsrc)
        {          
            int eventid = ((error ? 0x11 : 0x1) << 10) & (0x1 << 9) & (FACILITY_NULL << 6) & 0x1; // TODO: create event definitions
            LPCTSTR msgs[1] = { msg };
            int r = ReportEvent(eventsrc, error ? EVENTLOG_ERROR_TYPE : EVENTLOG_INFORMATION_TYPE, 0, 4, NULL, 1, 0, msgs, NULL);
            DeregisterEventSource(eventsrc);
        }
    }*/
};

winservice *winservice::callbacks::svc = (winservice *)NULL;

#endif
