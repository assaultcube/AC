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
    CubeMasterDAL masterDal;
    ServerBlacklist blacklist;
    int serverListCacheMinutes;

    public CubeMsRequestHandler()
    {
        masterDal = new CubeMasterDAL();
        masterDal.ConnectionString = WebConfigurationManager.ConnectionStrings["CubeMS"].ConnectionString;
        serverListCacheMinutes = int.Parse(WebConfigurationManager.AppSettings["ServerListCacheMinutes"] ?? "0");

        blacklist = new ServerBlacklist(HttpContext.Current.Server.MapPath("/Blacklist.xml"));
    }

    #region IHttpHandler Members

    public bool IsReusable
    {
        get { return false; }
    }

    public void ProcessRequest(HttpContext context)
    {

        string serverAddress = context.Request.UserHostAddress;

        IPEndPoint address = new IPEndPoint(IPAddress.Parse(serverAddress), 0);
        string requestFile = Path.GetFileName(context.Request.FilePath);

        try
        {
            if(requestFile == "register.do" && context.Request.QueryString["action"] == "add")
            {
				// fixme
                //context.Response.ContentType = "text/plain";

                try
                {
                    address.Port = int.Parse(context.Request.QueryString["port"]);
                }
                catch
                {
                    context.Response.Write("Server not registered, invalid request. Maybe you use an older server version?");
                    masterDal.AddEvent(CubeMasterDAL.EventType.RegistrationFailed, "invalid request", address);
                    return;
                }

                if (blacklist.IsBlacklisted(serverAddress) || context.Request.QueryString["bansample"] == "1")
                {
                    /*
                    context.Response.Write(RickRoll.RickRollMeNow());
                    context.Response.Flush();
                    */

                    
                    string lyrics = RickRoll.RickRollMeNow();

                    using(StringReader reader = new StringReader(lyrics))
                    {
                        string line = reader.ReadLine();
                        while(line != null)
                        {
                            // rick roll them.. line by line
                            context.Response.Write(line);
                            context.Response.Write(Environment.NewLine);
                            context.Response.Flush();

                            //System.Threading.Thread.Sleep(250);
                            line = reader.ReadLine();
                        }
                    }
                     

                    context.Response.Write(Environment.NewLine);
                    context.Response.Write("Behave and try again in a few days..");
                    masterDal.AddEvent(CubeMasterDAL.EventType.RegistrationFailed, "blacklisted", address);
                    return;
                }

                try
                {
                    masterDal.Register(address);
					
                    // TODO: *DOS safety
                    context.Response.Write("Registration successful. Due to caching it might take a few minutes to see the your server in the serverlist");
                    masterDal.AddEvent(CubeMasterDAL.EventType.RegistrationSuceeeded, null, address);
				}
                catch (Exception ex)
                {
                    context.Response.Write("Server not registered: " + ex.Message + ex.StackTrace);
                    masterDal.AddEvent(CubeMasterDAL.EventType.RegistrationFailed, null, address);
                }
            }
            else if(requestFile == "retrieve.do")
            {
				
                string type = context.Request.QueryString["item"];
                if(type == "list")
                {
                    // fixme
		    context.Response.ContentType = "text/plain";


                    try
                    {

                        // send server list
                        string servers = context.Cache[type] as string;
                        if(servers == null || servers.Length == 0) // empty cache, or empty server list
                        {
                            servers = masterDal.GetServers(false);
                            if(serverListCacheMinutes > 0) context.Cache.Add(type, servers, null, Cache.NoAbsoluteExpiration, new TimeSpan(0, serverListCacheMinutes, 0), CacheItemPriority.Normal, null);
                        }

			context.Response.Write(servers);
                        context.Response.Flush();
                        masterDal.AddEvent(CubeMasterDAL.EventType.RetrieveServers, "list", address);

                        // append client script if its an older or unknown client
                        if(bool.Parse((WebConfigurationManager.AppSettings["EnableClientScripts"] ?? "false")) && context.Request.UserAgent != WebConfigurationManager.AppSettings["ClientAgent"])
                        {
                            string script = context.Cache["script"] as string;
                            if(script == null)
                            {
                                if((script = masterDal.GetClientScript(context.Request.UserAgent)) != null)
                                {
                                    int cacheminutes = int.Parse(WebConfigurationManager.AppSettings["ClientScriptCacheMinutes"] ?? "0");
                                    context.Cache.Add("script", script, null, Cache.NoAbsoluteExpiration, new TimeSpan(0, cacheminutes, 0), CacheItemPriority.Normal, null);
                                }
                            }

                            if(script != null)
                            {
                                context.Response.Write(script);
                                context.Response.Flush();
                                masterDal.AddEvent(CubeMasterDAL.EventType.RetrieveServers, "script", address);
                            }
                        }
                    }
					/*
                    catch
                    {
                        // send error to the client
                        context.Response.Write("echo [An error ocurred on the masterserver]");
                        context.Response.Flush();
                        throw; // log errors globally
                    }
                    */
					finally
					{
					}
                }
                else if(context.Request.QueryString["item"] == "xml")
                {
                    context.Response.ContentType = "text/xml";

                    string serversXml = context.Cache[type] as string;
                    if(serversXml == null || serversXml.Length == 0) // empty cache, or empty server list
                    {
                        serversXml = masterDal.GetServers(true);
                        if(serverListCacheMinutes > 0) context.Cache.Add(type, serversXml, null, Cache.NoAbsoluteExpiration, new TimeSpan(0, serverListCacheMinutes, 0), CacheItemPriority.Normal, null);
                    }

                    context.Response.Write(serversXml);
                    context.Response.Flush();

                    masterDal.AddEvent(CubeMasterDAL.EventType.RetrieveServers, "xml", address);
                }
            }
        }
		/*
        catch(Exception ex)
        {
            try
            {
                // TODO: log all uncaught exceptions
                // log.AddEvent(Log.EventType.Exception, ex.Message, address);
            }
            catch { }
        }*/
		finally
		{
		}
    }

    #endregion
}
