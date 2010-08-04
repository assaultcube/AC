using System;
using System.Data;
using System.Configuration;
using System.Web;
using System.Web.Security;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Web.UI.WebControls.WebParts;
using System.Web.UI.HtmlControls;

using System.IO;
using System.Xml;
using System.Net;
using System.Data.SqlClient;

/// <summary>
/// Summary description for Log
/// </summary>
public class Log
{
    public enum EventType : int
    {
        RegistrationFailed = 0,
        RegistrationSuceeeded,
        RetrieveServers,
        Exception,
    }

    public void AddEvent(EventType type, string data, IPEndPoint address)
    {
        SqlConnection con = new SqlConnection(ConfigurationManager.ConnectionStrings["CubeMS"].ConnectionString as string);
        SqlCommand cmd = new SqlCommand("AddEvent", con);
        cmd.CommandType = CommandType.StoredProcedure;
        cmd.Parameters.AddWithValue("Type", type);
        cmd.Parameters.AddWithValue("Data", data);
        cmd.Parameters.AddWithValue("IP", IPTools.IPToInt(address.Address));
        con.Open();
        cmd.ExecuteNonQuery();
        con.Close();
    }
}
