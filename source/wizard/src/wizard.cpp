// wizard to start an AssaultCube server and storing the configuration

#include <stdio.h>
#include <map>
#include <string>
#include <iostream>
#include <fstream>

using namespace std;

#ifdef WIN32
    #pragma warning( disable : 4996 )
    #include <windows.h>
    #include <direct.h>
    #include <tchar.h>
    #include "winserviceinstaller.h"
#elif __GNUC__
    #include <sys/types.h>
    #include <sys/stat.h>
#endif

int main(int argc, char **argv)
{
    if(argc < 3)
    {
        cout << "usage: server_wizard <outfile> <relbinarypath>";
        return EXIT_FAILURE;
    }

    string outfile(argv[1]);
    string relpath(argv[2]);

	map<string, string> args;

	cout << "AssaultCube Server Wizard" << endl << endl;
    cout << "You can now specify some optional server settings. The default settings will be used if you decide to leave the fields empty. See README.html for a description of the settings." << endl << endl;

	cout << "server description:\t";
	getline(cin, args["n"]);

	cout << "max clients:\t\t"; 
	getline(cin, args["c"]);

	cout << "password:\t\t";
	getline(cin, args["p"]);

	cout << "admin password:\t\t";
	getline(cin, args["x"]);

	cout << "message of the day:\t";
	getline(cin, args["o"]);

	cout << "server port:\t\t";
	getline(cin, args["f"]);

	cout << "masterserver:\t\t";
	getline(cin, args["m"]);

	cout << "maprotation file:\t";
	getline(cin, args["r"]);

	cout << "score threshold:\t";
	getline(cin, args["k"]);

	cout << "upstream bandwidth:\t";
	getline(cin, args["u"]);

	cout << "ip:\t\t\t";
	getline(cin, args["i"]);

#ifdef WIN32

    string wsname, wsdisplayname;
	cout << "win service name:\t";
	getline(cin, wsname);

    if(!wsname.empty())
    {
        cout << "win service display:\t";
        getline(cin, wsdisplayname);
    }

#endif

    string argstr;
	
	for(map<string, string>::iterator i = args.begin(); i != args.end(); i++)
	{
		if((*i).second.empty()) continue; // arg value not set
		else
		{
			argstr += " -" + (*i).first;
			if((*i).second.find(" ") == string::npos) argstr += (*i).second;
			else argstr += '"' + (*i).second + '"'; // escape spaces
		}
	}

	cout << endl << "Writing your configuration to " << outfile << " ... ";

	try
	{
        fstream startupScript(outfile.c_str(), ios::out);
#ifdef WIN32
        startupScript << relpath << argstr << endl << "pause" << endl;
#elif __GUNC__
	startupScript << relpath << argstr << endl;
#endif
		startupScript.close();
	}
	catch(...)
	{
		cout << "Failed!" << endl;
		return EXIT_FAILURE;
	}

    cout << "Done" << endl << endl;
    cout << "Note: You can start " << outfile << " directly the next time you want to use this configuration again." << endl << endl;

#ifdef WIN32

    if(!wsname.empty())
    {
        if(wsdisplayname.empty()) wsdisplayname = wsname;

        cout << "Installing the AC Server as windows service ... ";

        char path[MAX_PATH];
	    _getcwd(path, MAX_PATH);
        strncat(path, ("\\" + relpath + " -s" + wsname + " " + argstr).c_str(), MAX_PATH);

        winserviceinstaller installer(wsname.c_str(), wsdisplayname.c_str(), path);
        
        int r;
        if(!installer.OpenManger()) 
        { 
            cout << "Failed!" << endl;
            cout << "Could not open the Service Control Manager: " << GetLastError() << endl;
            installer.CloseManager();
            return EXIT_FAILURE;
        }
        
        if((r = installer.IsInstalled()) != 0)
        {
            cout << "Failed!" << endl;
            if(r == -1) cout << "Error accessing the Service Control Manager" << endl;
            else if(r == 1) cout << "A windows service with this name (" << wsname << ")is already installed: " << GetLastError() << endl;
            return EXIT_FAILURE;
        }

        if((r = installer.Install()) != 1)
        {
            cout << "Failed!" << endl;
            if(r == -1) cout << "Error accessing the Service Control Manager" << endl;
            else if(r == 0) cout << "Could not create the new windows service: " << GetLastError() << endl;
            return EXIT_FAILURE;
        }

        cout << "Done" << endl << endl;
        cout << "Note: You can now manage your AC server using services.msc and sc.exe" << endl << endl;
    }

#endif

	cout << "Press Enter to start the server now" << endl;
	cin.get();
	cout << "Starting the AC server ..." << endl;
	system((relpath + argstr).c_str());

	return EXIT_SUCCESS;
}
