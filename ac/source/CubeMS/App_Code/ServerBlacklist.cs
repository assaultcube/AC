using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Xml.Linq;

/// <summary>
/// Summary description for ServerBlacklist
/// </summary>
public class ServerBlacklist
{
    List<string> serverIps = new List<string>();

	public ServerBlacklist(string blacklistFile)
	{
        XDocument document = XDocument.Load(blacklistFile);
        var servers = document.Descendants("Server");
        foreach (var server in servers)
        {
            serverIps.Add(server.Attribute("IP").Value);
        }
	}

    public bool IsBlacklisted(string serverIp)
    {
        return serverIps.Contains(serverIp);
    }

}
