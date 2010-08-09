CubeMS -- masterserver for the AssaultCube v1.0


License:
ZLIB
http://www.gzip.org/zlib/zlib_license.html

Author:
Adrian 'driAn' Henke 
root AT sprintf.org

Version:
v0.3
November 2007

About this Software:
The purpose of this software is to let AssaultCube gameservers register
themselves on a central location and distribute the list of available
servers to all requesting AssaultCube game clients. Also it provides 
additional interfaces for external application and possbilities to
control the storage and monitor various events.
This software has been developed to replace other PHP/Perl/Java based 
solutions that are not easily expandable for future AssaultCube features 
that will require more interaction with the masterserver.
This software is implemented as IIS HTTP-Handler written in C#/ASP.NET 
with a MSSQL backend. Integrating the masterserver as HTTP-Handler and using
various caching possiblities ensures good scalability on higher workload.

Installation:
There is no streamlined installation procedure for this software, you
need some knowledge of IIS 5+, MSSQL and ASP.NET to make any use of it.
Here is a quick list of required steps:

* In IIS, create a new (virtual) directory that points to the CubeMS
  directory. Configure security, ASP.NET 2.0, etc.
* In IIS, configure the application assignement for your CubeMS app.
  Assign the ".do" ending to your aspnet_isapi.dll, restrict to GET requests
  and disable the optional checks for existing files.
* In MSSQL, create a new database as well as tables and stored procedures.
  Use the provided scripts to get this done.
* In MSSQL, create a new user and configure the web.config file in the CubeMS
  directory accordingly.
