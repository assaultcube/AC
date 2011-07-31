// minimal windows service installer

class winserviceinstaller
{

private:
    LPCSTR name;
    LPCSTR displayName;
    LPCSTR path;

    SC_HANDLE scm;

public:
    winserviceinstaller(LPCSTR name, LPCSTR displayName, LPCSTR path) : name(name), displayName(displayName), path(path), scm(NULL)
    {
    }

    ~winserviceinstaller()
    {
        CloseManager();
    }

    bool OpenManger()
    {
        return ((scm = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) != NULL);
    }

    void CloseManager()
    {
        if(scm) CloseServiceHandle(scm);
    }

    int IsInstalled()
    {
        if(!scm) return -1;
        SC_HANDLE svc = OpenService(scm, name, SC_MANAGER_CONNECT);
        bool installed = svc != NULL;
        CloseServiceHandle(svc);
        return installed ? 1 : 0;
    }
        
    int Install()
    {
        if(!scm) return -1;
        SC_HANDLE svc = CreateService(scm, name, displayName, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, path, NULL, NULL, NULL, NULL, NULL);
        if(svc == NULL) return 0;
        else
        {
            CloseServiceHandle(svc);
            return 1;
        }
    }
};

