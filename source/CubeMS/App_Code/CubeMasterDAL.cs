using System;
using System.Data;
using System.Data.Common;
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
//using System.Data.SqlClient;
using System.Text;
using System.Linq;
using MySql.Data.MySqlClient;


using AHSoftware.CubeMon;
using AHSoftware.CubeMon.Games.ActionCube.v094;

/// <summary>
/// Summary description for CubeMS
/// </summary>
public class CubeMasterDAL
{
    string connectionString;
	//DbProviderFactory factory;
	
    public string ConnectionString
    {
        get { return connectionString; }
        set { connectionString = value; }
    }	
	
	public CubeMasterDAL()
	{
		// factory = new MySql.Data.MySqlClient.MySqlClientFactory();
    }
		
	IDbConnection CreateConnection()
	{
        IDbConnection con = new MySqlConnection(); //factory.CreateConnection();
		con.ConnectionString = ConnectionString;
		//con.Open();
		return con;
    }
		
    private void AddServer(IPEndPoint address, string name, string description)
    {
        //SqlConnection con = new SqlConnection(connectionString);
        //SqlCommand cmd = new SqlCommand("AddServer", con);
		
		IDbConnection con = CreateConnection();
		IDbCommand cmd = con.CreateCommand();
		cmd.CommandText = "DELETE FROM Servers WHERE IP=@IP AND Port=@Port; INSERT INTO Servers VALUES(@IP, @Port, @Name, @Description, @Date)";
        
		var param = cmd.CreateParameter();
		param.ParameterName = "IP";
		param.Value = IPTools.IPToInt(address.Address);
		cmd.Parameters.Add(param);
		
		param = cmd.CreateParameter();
		param.ParameterName = "Port";
		param.Value = address.Port;
		cmd.Parameters.Add(param);
		
		param = cmd.CreateParameter();
		param.ParameterName = "Name";
		param.Value = name;
		cmd.Parameters.Add(param);
		
		param = cmd.CreateParameter();
		param.ParameterName = "Description";
		param.Value = description;
		cmd.Parameters.Add(param);		
		
		param = cmd.CreateParameter();
		param.ParameterName = "Date";
		param.Value = DateTime.Now;
		cmd.Parameters.Add(param);		
		
		Console.WriteLine("opening");
        con.Open();
        cmd.ExecuteNonQuery();
		Console.WriteLine("done");
    }

    public void Register(IPEndPoint address)
    {
        Client c = new Client();
        c.Socket = new IPEndPoint(address.Address, address.Port + 1);
        ServerInfo info = null;
        try
        {
            info = (ServerInfo)c.RequestInfo();
        }
        catch
        {
            throw new ApplicationException("Could not ping you. Make sure your server is accessible from the internet.");
        }

        string name = string.Empty;
        try
        {
            name = Dns.GetHostEntry(info.Socket.Address).HostName;
        }
        catch { };

        if (info.MaxClients > 20)
        {
            throw new ApplicationException("Your server must have a maxclients setting of 20 or less. Check your configuration.");
        }

        AddServer(address, name, info.Description);
    }

    public string GetServers(bool xml)
    {
		System.Diagnostics.Debug.Assert(xml == false);
		
	
	
        IDbConnection con = CreateConnection();
        IDbCommand cmd = con.CreateCommand(); //new SqlCommand(xml ? "GetServersXML" : "GetServers", con);
        cmd.CommandText = "DELETE FROM Servers WHERE LastModified + INTERVAL 70 MINUTE < NOW();  SELECT * FROM Servers;"; // fixme, xml
		
        try
        {
            con.Open();
            IDataReader r = cmd.ExecuteReader();
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
        IDbConnection con = CreateConnection();
        IDbCommand cmd = con.CreateCommand(); // new SqlCommand("GetClientScript", con);
        //cmd.Parameters.AddWithValue("HTTPAgent", HTTPAgent);
		cmd.CommandText = "SELECT * FROM ClientScripts WHERE HTTPAgent = @HTTPAgent";
		
		var param = cmd.CreateParameter();
		param.ParameterName = "HTTPAgent";
		param.Value = HTTPAgent;
		cmd.Parameters.Add(param);
		
        try
        {
            con.Open();
            IDataReader r = cmd.ExecuteReader();
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

    public enum EventType : int
    {
        RegistrationFailed = 0,
        RegistrationSuceeeded,
        RetrieveServers,
        Exception,
    }

    public void AddEvent(EventType type, string data, IPEndPoint address)
    {
        IDbConnection con = CreateConnection();
        IDbCommand cmd = con.CreateCommand(); //new SqlCommand("AddEvent", con);
        cmd.CommandText = "INSERT INTO Events (Type, Data, IP, Time) VALUES (@Type, @Data, @IP, @Time)";
		
		var param = cmd.CreateParameter();
		param.ParameterName = "Type";
		param.Value = type;
		cmd.Parameters.Add(param);
		
		param = cmd.CreateParameter();
		param.ParameterName = "Data";
		param.Value = data;
		cmd.Parameters.Add(param);
		
		param = cmd.CreateParameter();
		param.ParameterName = "IP";
		param.Value = IPTools.IPToInt(address.Address);
		cmd.Parameters.Add(param);
		
		param = cmd.CreateParameter();
		param.ParameterName = "Time";
		param.Value = DateTime.Now;
		cmd.Parameters.Add(param);
		
        con.Open();
        cmd.ExecuteNonQuery();
        con.Close();
    }
}
