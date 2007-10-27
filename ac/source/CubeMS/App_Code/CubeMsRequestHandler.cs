using System;
using System.Data;
using System.Configuration;
using System.Web;
using System.Web.Security;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Web.UI.WebControls.WebParts;
using System.Web.UI.HtmlControls;

using System.Net;
using System.Xml;
using System.IO;
using System.Web.Configuration;
using System.Web.Caching;

using AHSoftware.CubeMon;
using AHSoftware.CubeMon.Games.ActionCube.v094;

/// <summary>
/// Summary description for RequestHandler
/// </summary>
public class CubeMsRequestHandler : IHttpHandler
{
    Log log = new Log();
    CubeMS master;
    int serverListCacheMinutes;

    CubeMsRequestHandler()
    {
        master = new CubeMS();
        master.ConnectionString = WebConfigurationManager.ConnectionStrings["CubeMS"].ConnectionString;
        serverListCacheMinutes = int.Parse(WebConfigurationManager.AppSettings["ServerListCacheMinutes"] ?? "0");
    }

    #region IHttpHandler Members

    public bool IsReusable
    {
        get { return false; }
    }

    public void ProcessRequest(HttpContext context)
    {
        IPEndPoint address = new IPEndPoint(IPAddress.Parse(context.Request.UserHostAddress), 0);
        string requestFile = Path.GetFileName(context.Request.FilePath);

        try
        {
            if(requestFile == "register.do" && context.Request.QueryString["action"] == "add")
            {
                context.Response.ContentType = "text/plain";

                try
                {
                    address.Port = int.Parse(context.Request.QueryString["port"]);
                }
                catch
                {
                    context.Response.Write("Server not registered, invalid request. Maybe you use an older server version?");
                    log.AddEvent(Log.EventType.RegistrationFailed, "invalid request", address);
                    return;
                }

                if(master.Register(address))
                {
                    // TODO: *DOS safety
                    context.Response.Write("Registration successful. Due to caching it might take a few minutes to see the your server in the serverlist");
                    log.AddEvent(Log.EventType.RegistrationSuceeeded, null, address);
                }
                else
                {
                    context.Response.Write("Server not registered, could not ping you. Make sure your server is accessible from the internet.");
                    log.AddEvent(Log.EventType.RegistrationFailed, "ping", address);
                }
            }
            else if(requestFile == "retrieve.do")
            {
                string type = context.Request.QueryString["item"];
                if(type == "list")
                {
                    context.Response.ContentType = "text/plain";

                    try
                    {

                        // send server list
                        string servers = context.Cache[type] as string;
                        if(servers == null || servers.Length == 0) // empty cache, or empty server list
                        {
                            servers = master.GetServers(false);
                            if(serverListCacheMinutes > 0) context.Cache.Add(type, servers, null, Cache.NoAbsoluteExpiration, new TimeSpan(0, serverListCacheMinutes, 0), CacheItemPriority.Normal, null);
                        }

                        context.Response.Write(servers);
                        context.Response.Flush();
                        log.AddEvent(Log.EventType.RetrieveServers, "list", address);

                        // append client script if its an older or unknown client
                        if(bool.Parse((WebConfigurationManager.AppSettings["EnableClientScripts"] ?? "false")) && context.Request.UserAgent != WebConfigurationManager.AppSettings["ClientAgent"])
                        {
                            string script = context.Cache["script"] as string;
                            if(script == null)
                            {
                                if((script = master.GetClientScript(context.Request.UserAgent)) != null)
                                {
                                    int cacheminutes = int.Parse(WebConfigurationManager.AppSettings["ClientScriptCacheMinutes"] ?? "0");
                                    context.Cache.Add("script", script, null, Cache.NoAbsoluteExpiration, new TimeSpan(0, cacheminutes, 0), CacheItemPriority.Normal, null);
                                }
                            }

                            if(script != null)
                            {
                                context.Response.Write(script);
                                context.Response.Flush();
                                log.AddEvent(Log.EventType.RetrieveServers, "script", address);
                            }
                        }
                    }
                    catch
                    {
                        // send error to the client
                        context.Response.Write("echo [An error ocurred on the masterserver]");
                        context.Response.Flush();
                        throw; // log errors globally
                    }
                }
                else if(context.Request.QueryString["item"] == "xml")
                {
                    context.Response.ContentType = "text/xml";

                    string serversXml = context.Cache[type] as string;
                    if(serversXml == null || serversXml.Length == 0) // empty cache, or empty server list
                    {
                        serversXml = master.GetServers(true);
                        if(serverListCacheMinutes > 0) context.Cache.Add(type, serversXml, null, Cache.NoAbsoluteExpiration, new TimeSpan(0, serverListCacheMinutes, 0), CacheItemPriority.Normal, null);
                    }

                    context.Response.Write(serversXml);
                    context.Response.Flush();

                    log.AddEvent(Log.EventType.RetrieveServers, "xml", address);
                }
            }
        }
        catch(Exception ex)
        {
            try
            {
                // TODO: log all uncaught exceptions
                // log.AddEvent(Log.EventType.Exception, ex.Message, address);
            }
            catch { }
        }
    }

    #endregion
}
