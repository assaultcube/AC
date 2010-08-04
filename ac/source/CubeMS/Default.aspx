<%@ Page Language="C#" AutoEventWireup="true"  CodeFile="Default.aspx.cs" Inherits="_Default" %>

<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">

<html xmlns="http://www.w3.org/1999/xhtml" >
<head runat="server">
    <title>AssaultCube Master Server</title>
</head>
<body>
    <form id="form1" runat="server">
    <div title="AssaultCube Master Server">
        <h1>AssaultCube Master Server</h1>
        <h2>Server List</h2>
        &nbsp;
        
        <asp:GridView ID="GridView1" AutoGenerateColumns="False" CellPadding="4"
            DataKeyNames="IP" DataSourceID="SqlDataSource1" ForeColor="#333333" GridLines="None">
            <FooterStyle BackColor="#5D7B9D" Font-Bold="True" ForeColor="White" />
            <Columns>
                <asp:TemplateField>
                    <ItemTemplate>
                        <%-- <%# IPTools.IntToIp((int)DataBinder.Eval(Container.DataItem, "IP")).ToString() %> --%>
                    </ItemTemplate>
                </asp:TemplateField>
                <asp:BoundField DataField="Port" HeaderText="Port" SortExpression="Port" />
                <asp:BoundField DataField="Name" HeaderText="Name" SortExpression="Name" />
                <asp:BoundField DataField="Description" HeaderText="Description" SortExpression="Description" />
                <asp:BoundField DataField="LastModified" HeaderText="LastModified" SortExpression="LastModified" />
            </Columns>
            <RowStyle BackColor="#F7F6F3" ForeColor="#333333" />
            <EditRowStyle BackColor="#999999" />
            <SelectedRowStyle BackColor="#E2DED6" Font-Bold="True" ForeColor="#333333" />
            <PagerStyle BackColor="#284775" ForeColor="White" HorizontalAlign="Center" />
            <HeaderStyle BackColor="#5D7B9D" Font-Bold="True" ForeColor="White" />
            <AlternatingRowStyle BackColor="White" ForeColor="#284775" />
        </asp:GridView>
        <asp:SqlDataSource ID="SqlDataSource1" runat="server" ConnectionString="<%$ ConnectionStrings:CubeMS %>"
            SelectCommand="GetServers" SelectCommandType="StoredProcedure"></asp:SqlDataSource>
        
        <ul>
            <li><a href="retrieve.do?item=list">CubeScript Server List</a></li>
        </ul>
    
    </div>
    </form>
</body>
</html>
