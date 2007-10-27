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

/// <summary>
/// Summary description for IPTools
/// </summary>
public static class IPTools
{
    public static int IPToInt(IPAddress address)
    {
        byte[] ip = address.GetAddressBytes();
        if(ip.Length == 4) return (int)(ip[0] | (ip[1] << 8) | (ip[2] << 16) | (ip[3] << 24));
        else return 0;
    }

    public static IPAddress IntToIp(int address)
    {
        byte[] octets = { (byte)(address & 0xFF), (byte)((address >> 8) & 0xFF), (byte)((address >> 16) & 0xFF), (byte)((address >> 24) & 0xFF) };
        return new IPAddress(octets);
    }
}
