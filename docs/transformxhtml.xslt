<?xml version="1.0" encoding="utf-8"?>

<!-- (C) 2007 Adrian Henke, ZLIB license -->

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <!-- avoid IE quirks mode -->
  <xsl:output method="html" omit-xml-declaration="yes" encoding="utf-8" doctype-public="-//W3C//DTD XHTML 1.0 Transitional//EN"/> 
  
  <xsl:template match="/cubescriptreference">
      <html>
        <head>
          <title>Command Reference</title>
          <meta http-equiv="content-type" content="application/xhtml+xml;charset=utf-8" />
          <link rel="stylesheet" href="docs.css" />
        </head>
        
        <body>
          <div align="center">
            <div id="header">Rabid Viper Productions</div>
            <div id="main">

              <xsl:for-each select="section">
                <h2><xsl:value-of select="@name"/></h2>
                <xsl:for-each select="command">
                  <div class="code">
                    <xsl:value-of select="@name"/>
                    <xsl:text> </xsl:text>
                    <xsl:for-each select="arguments/argument">
                      <xsl:value-of select="@token"/>
                      <xsl:text> </xsl:text>
                    </xsl:for-each>
                  </div>
                  <xsl:value-of select="description"/>
                </xsl:for-each>
              </xsl:for-each>

            </div>
          </div>
        </body>
      </html>
  </xsl:template>

</xsl:stylesheet> 
