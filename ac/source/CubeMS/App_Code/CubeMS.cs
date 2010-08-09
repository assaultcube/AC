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
using System.Data.SqlClient;
using System.Text;

using AHSoftware.CubeMon;
using AHSoftware.CubeMon.Games.ActionCube.v094;

/// <summary>
/// Summary description for CubeMS
/// </summary>
public class CubeMS
{
    string connectionString;

    public string ConnectionString
    {
        get { return connectionString; }
        set { connectionString = value; }
    }

    private bool AddServer(IPEndPoint address, string name, string description)
    {
        SqlConnection con = new SqlConnection(connectionString);
        SqlCommand cmd = new SqlCommand("AddServer", con);
        cmd.CommandType = CommandType.StoredProcedure;
        cmd.Parameters.AddWithValue("IP", IPTools.IPToInt(address.Address));
        cmd.Parameters.AddWithValue("Port", address.Port);
        cmd.Parameters.AddWithValue("Name", name);
        cmd.Parameters.AddWithValue("Description", description);
        try
        {
            con.Open();
            cmd.ExecuteNonQuery();
            return true;
        }
        catch
        {
            return false;
        }
        finally
        {
            con.Close();
        }
    }

    public bool Register(IPEndPoint address)
    {
        Client c = new Client();
        c.Socket = new IPEndPoint(address.Address, address.Port+1);
        ServerInfo info = null;
        try
        {
            info = (ServerInfo) c.RequestInfo();
        }
        catch
        {
            return false;
        }
        
        string name = string.Empty;
        try 
        { 
            name = Dns.GetHostEntry(info.Socket.Address).HostName;
        }
        catch { };

        return AddServer(address, name, info.Description);
    }

    public string GetServers(bool xml)
    {
        SqlConnection con = new SqlConnection(connectionString);
        SqlCommand cmd = new SqlCommand(xml ? "GetServersXML" : "GetServers", con);
        cmd.CommandType = CommandType.StoredProcedure;
        try
        {
            con.Open();
            SqlDataReader r = cmd.ExecuteReader();
            StringBuilder sb = new StringBuilder();
            
            if(xml)
            {
                // sql server handles xml conversion for us, just embed in a new xmldoc
                sb.AppendLine("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
                sb.AppendLine("<Servers>");
                if(r.Read())
                {
                    sb.AppendLine(r[0] as string);
                }
                sb.AppendLine("</Servers>");
            }
            else
            {
                // construct server list as cubescript
                while(r.Read())
                {
                    sb.AppendFormat("addserver {0} {1};{2}", IPTools.IntToIp((int)r["IP"]).ToString(), r["Port"], Environment.NewLine);
                }
            }

            return sb.ToString();
        }
        catch
        {
            return null;
        }
        finally
        {
            con.Close();
        }
    }

    public string GetClientScript(string HTTPAgent)
    {
        SqlConnection con = new SqlConnection(connectionString);
        SqlCommand cmd = new SqlCommand("GetClientScript", con);
        cmd.CommandType = CommandType.StoredProcedure;
        cmd.Parameters.AddWithValue("HTTPAgent", HTTPAgent);
        try
        {
            con.Open();
            SqlDataReader r = cmd.ExecuteReader();
            r.Read();
            return r["Script"] as string;
        }
        catch
        {
            return null;
        }
        finally
        {
            con.Close();
        }
    }
}
