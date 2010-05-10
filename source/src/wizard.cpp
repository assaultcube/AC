// wizard to start an AssaultCube server and storing the configuration

#include "cube.h"

#ifdef WIN32
    #ifndef __GNUC__
        #pragma warning( disable : 4996 )
    #endif
    #include <direct.h>
    #include "winserviceinstaller.h"
#elif __GNUC__
    #include <sys/types.h>
    #include <sys/stat.h>
#endif

static void addarg(vector<char> &argstr, const char *name)
{
    argstr.add(' ');
    argstr.put(name, strlen(name));
}

static void addarg(vector<char> &argstr, const char *name, const char *val)
{
    addarg(argstr, name);
    bool space = strchr(val, ' ')!=NULL;
    if(space) argstr.add('"');
    argstr.put(val, strlen(val));
    if(space) argstr.add('"');
}

static void readarg(const char *desc, char *val, int len)
{
    printf("%s: ", desc);
    fflush(stdout);
    fgets(val, len, stdin);
    char *end = strchr(val, '\n');
    if(end) *end = '\0';
}

static void readarg(vector<char> &argstr, const char *desc, const char *name)
{
    string val = "";
    readarg(desc, val, sizeof(val));
    if(val[0]) addarg(argstr, name, val);
}

int wizardmain(int argc, char **argv)
{
    int k, i = 0;
    while ( ( k = strncmp(argv[i],"--wizard",8) ) != 0 && i < argc ) i++;
    if ( k != 0 ) return EXIT_FAILURE;

    char *outfile = argv[++i];
    char *relpath = argv[++i];

	printf("AssaultCube Server Wizard\n\n");
    printf("You can now specify some optional server settings.\n"
           "The default settings will be used if you decide to leave the fields empty.\n"
           "If you're not sure: leave the field empty.\n"
           "See README.html for a description of the settings.\n\n");

    vector<char> argstr;
    readarg(argstr, "server description", "-n");
    readarg(argstr, "message of the day", "-o");
    readarg(argstr, "max clients", "-c");
	readarg(argstr, "admin password", "-x");
	readarg(argstr, "player password", "-p");
	readarg(argstr, "server port", "-f");
    string ispub = "";
    readarg("public server (Yes|no)", ispub, sizeof(ispub));
    if(toupper(ispub[0]) == 'N') addarg(argstr, "-m");
    string cmds = "";
    readarg("additional command-line parameters", cmds, sizeof(cmds));
    if(cmds[0]) addarg(argstr, cmds);
#ifdef WIN32

    string wsname = "", wsdisplayname = "";
    readarg("win service name", wsname, sizeof(wsname));
    if(wsname[0])
        readarg("win service display", wsdisplayname, sizeof(wsdisplayname));

#endif

	printf("\nWriting your configuration to %s ... ", outfile); fflush(stdout);

    argstr.add('\0');

    FILE *script = fopen(outfile, "w");
    if(!script)
    {
        printf("Failed!\n");
        return EXIT_FAILURE;
    }

#ifdef WIN32
        fprintf(script, "%s%s\npause\n", relpath, argstr.getbuf());
#elif __GNUC__
        fprintf(script, "#!/bin/sh\n%s%s\n", relpath, argstr.getbuf());
#endif
    fclose(script);

    printf("Done\n\n");
    printf("Note: You can start %s directly the next time you want to use this configuration again.\n\n", outfile);

#ifdef WIN32

    if(wsname[0])
    {
        if(!wsdisplayname[0]) copystring(wsdisplayname, wsname);

        printf("Installing the AC Server as windows service ... "); fflush(stdout);

        vector<char> path;
        databuf<char> cwd = path.reserve(MAX_PATH);    
	    if(!_getcwd(cwd.buf, MAX_PATH))
        {
            printf("Failed!\n");
            printf("Could not get current working directory: %u\n", (uint)GetLastError());
            return EXIT_FAILURE;
        }
        path.advance(strlen(cwd.buf));
        path.add('\\');
        path.put(relpath, strlen(relpath));
        path.put(" -S", 3);
        path.put(wsname, strlen(wsname));
        path.add(' ');
        path.put(argstr.getbuf(), argstr.length());

        winserviceinstaller installer(wsname, wsdisplayname, path.getbuf());

        int r;
        if(!installer.OpenManger())
        {
            printf("Failed!\n");
            printf("Could not open the Service Control Manager: %u\n", (uint)GetLastError());
            installer.CloseManager();
            return EXIT_FAILURE;
        }

        if((r = installer.IsInstalled()) != 0)
        {
            printf("Failed!\n");
            if(r == -1) printf("Error accessing the Service Control Manager\n");
            else if(r == 1) printf("A windows service with this name (%s) is already installed: %u\n", wsname, (uint)GetLastError());
            return EXIT_FAILURE;
        }

        if((r = installer.Install()) != 1)
        {
            printf("Failed!\n");
            if(r == -1) printf("Error accessing the Service Control Manager\n");
            else if(r == 0) printf("Could not create the new windows service: %u\n", (uint)GetLastError());
            return EXIT_FAILURE;
        }

        printf("Done\n\n");
        printf("Note: You can now manage your AC server using services.msc and sc.exe\n\n");
    }

#endif

	printf("Press Enter to start the server now\n");
    fgetc(stdin);
	printf("Starting the AC server ...\n");
    argstr.insert(0, relpath, strlen(relpath));
    system(argstr.getbuf());

	return EXIT_SUCCESS;
}
