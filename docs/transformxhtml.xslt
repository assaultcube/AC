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
          <link rel="stylesheet" href="reference.css"/>
          <link rel="stylesheet" href="docs.css" /> <!-- additional stuff -->
        </head>
        
        <body>
          <div align="center" class="cubescriptference">
            <div id="header">Rabid Viper Productions</div>
            <div id="main">

              <!-- section -->
              <xsl:for-each select="section">
                <div class="section">
                  
                  <h2><xsl:value-of select="@name"/></h2>
                  <!-- command -->
                  <xsl:for-each select="command">
                    <div class="command">

                      <!-- display name -->
                      <div class="displayname">
                        <xsl:value-of select="@name"/>
                        <xsl:text> </xsl:text>
                        <xsl:for-each select="arguments/argument">
                          <xsl:value-of select="@token"/>
                          <xsl:text> </xsl:text>
                        </xsl:for-each>
                      </div>

                      <!-- description -->
                      <div class="description">
                        <xsl:value-of select="description"/>
                      </div>

                      <!-- arguments -->
                      <xsl:if test="arguments/argument">
                        <div class="arguments">
                          <table class="arguments">
                            <tr>
                              <th>
                                Arg
                              </th>
                              <th>
                                Description
                              </th>
                              <th>
                                Values
                              </th>
                            </tr>
                            <xsl:for-each select="arguments/argument">
                              <tr>
                                <td class="token">
                                  <xsl:value-of select="@token"/>
                                </td>
                                <td class="description">
                                  <xsl:value-of select="@description"/>
                                </td>
                                <td class="values">
                                  <xsl:value-of select="@values"/>
                                </td>
                              </tr>
                            </xsl:for-each>
                          </table>
                        </div>
                      </xsl:if>
                        
                      <!-- remarks -->
                      <xsl:if test="remarks">
                        <div class="remarks">
                          <xsl:value-of select="remarks"/>
                        </div>
                      </xsl:if>

                    </div>
                  </xsl:for-each>
                </div>
              </xsl:for-each>
            </div>
          </div>
        </body>
      </html>
  </xsl:template>

</xsl:stylesheet> 
