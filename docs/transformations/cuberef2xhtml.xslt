<?xml version="1.0" encoding="utf-8"?>
<!-- ZLIB licensed, (C) 2007 Adrian 'driAn' Henke, http://www.sprintf.org -->

<!--
  transforms a cuberef document to a xhtml document
-->

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:t="http://cubers.net/Schemas/CubeRef">

  <xsl:output method="html" omit-xml-declaration="yes" encoding="utf-8" doctype-public="-//W3C//DTD XHTML 1.0 Transitional//EN"/>
  
  <xsl:template name="identifierLink">
    <a>
      <xsl:attribute name="href">
        <xsl:text>#identifier_</xsl:text>
        <xsl:value-of select="translate(@name, ' ', '_')"/>
      </xsl:attribute>
      <xsl:value-of select="@name"/>
    </a>
  </xsl:template>

  <xsl:template match="/t:cuberef">
    <html>
      <head>
        <title>
          <xsl:value-of select="@name"/>
        </title>
        <meta http-equiv="content-type" content="application/xhtml+xml;charset=utf-8" />
        <link rel="stylesheet" href="styles/cuberef.css"/>
      </head>

      <body>
        <div id="main">

          <!-- version -->
          <div id="version">
            <xsl:value-of select="@version"/>
          </div>

          <!-- reference title -->
          <div id="title">
            <h1>
              <xsl:value-of select="@name"/>
            </h1>
          </div>

          <!-- contents panel -->
          <div id="contentspanel">
            <xsl:if test="t:sections">
              <xsl:for-each select="t:sections/t:section">
                <xsl:sort select="@name[../../@sort = 'true' or ../../@sort = '1']"/>
                <div class="sectiontitle">
                  <a>
                    <xsl:attribute name="href">
                      <xsl:text>#section_</xsl:text>
                      <xsl:value-of select="translate(@name, ' ', '_')"/>
                    </xsl:attribute>
                    <xsl:value-of select="@name"/>
                  </a>
                </div>
                <div class="identifiers">
                  <xsl:if test="t:identifiers">
                    <ul>
                      <xsl:for-each select="t:identifiers/*">
                        <xsl:sort select="@name[../../@sort = 'true' or ../../@sort = '1']"/>
                        <li>
                          <xsl:call-template name="identifierLink"/>
                        </li>
                      </xsl:for-each>
                    </ul>
                  </xsl:if>
                </div>
              </xsl:for-each>
            </xsl:if>
          </div>

          <div id="content">
            
              <!-- contents -->
              <div id="contents">
                <h2>
                  Contents
                </h2>
                <p>
                  <xsl:value-of select="t:description"/>
                </p>
                <p>
                  The following sections are available:
                </p>
                <ul>
                  <xsl:for-each select="t:sections/t:section">
                    <xsl:sort select="@name[../../@sort = 'true' or ../../@sort = '1']"/>
                    <li>
                      <a>
                        <xsl:attribute name="href">
                          <xsl:text>#section_</xsl:text>
                          <xsl:value-of select="translate(@name, ' ', '_')"/>
                        </xsl:attribute>
                        <xsl:value-of select="@name"/>
                      </a>
                    </li>
                  </xsl:for-each>
                </ul>
              </div>
              
              <!-- sections -->
              <xsl:for-each select="t:sections/t:section">
                <xsl:sort select="@name[../../@sort = 'true' or ../../@sort = '1']"/>
                <div class="section">
                  <h2>
                    <!-- section anchor -->
                    <xsl:attribute name="id">
                      <xsl:text>section_</xsl:text>
                      <xsl:value-of select="translate(@name, ' ', '_')"/>
                    </xsl:attribute>
                    <xsl:value-of select="@name"/>
                  </h2>

                  <!-- description -->
                  <p>
                    <xsl:value-of select="t:description"/>
                  </p>
                  
                  <!-- identifiers -->
                  <xsl:for-each select="t:identifiers/*">
                    <xsl:sort select="@name[../../@sort = 'true' or ../../@sort = '1']"/>
                    <div class="identifier">
                      <!-- identifier anchor -->
                      <xsl:attribute name="id">
                        <xsl:text>identifier_</xsl:text>
                        <xsl:value-of select="translate(@name, ' ', '_')"/>
                      </xsl:attribute> 
                      
                      <!-- display name -->
                      <div class="displayname">
                        <xsl:call-template name="identifierLink"/>
                        <xsl:text> </xsl:text>
                        <xsl:choose>
                          <xsl:when test="t:arguments"><!-- command args -->
                            <xsl:for-each select="t:arguments/*">
                              <xsl:value-of select="@token"/>
                              <xsl:text> </xsl:text>
                            </xsl:for-each>                            
                          </xsl:when>
                          <xsl:when test="t:value and t:value/@token and t:value/@readOnly != 'true' and t:value/@readOnly != '1'"><!-- variable value -->
                            <xsl:text> </xsl:text>
                            <xsl:value-of select="t:value/@token"/>  
                          </xsl:when>
                        </xsl:choose>
                      </div>

                      <!-- description -->
                      <div class="description">
                        <xsl:value-of select="t:description"/>
                      </div>

                      <!-- arguments or value description -->
                      <xsl:choose>
                        <xsl:when test="t:arguments"><!-- command args -->
                          <div class="argumentDescriptions">
                            <table>
                              <tr>
                                <th>Argument</th>
                                <th>Description</th>
                                <th>Values</th>
                              </tr>
                              <xsl:for-each select="t:arguments/*">
                                <tr>
                                  <td class="token">
                                    <xsl:value-of select="@token"/>
                                  </td>
                                  <td class="description">
                                    <xsl:value-of select="@description"/>
                                  </td>
                                  <td class="values">
                                    <xsl:value-of select="@valueNotes"/>
                                  </td>
                                </tr>
                              </xsl:for-each>
                            </table>
                          </div>
                        </xsl:when>
                        <xsl:when test="t:value"> <!-- var value -->
                          <div class="valueDescription">
                            <table>
                              <tr>
                                <xsl:if test="t:value/@token and t:value/@readOnly != 'true' and t:value/@readOnly != '1'">
                                <th>
                                  Token
                                </th>
                                </xsl:if>
                                <th>
                                  Description
                                </th>
                                <th>Values</th>
                                <th>Range</th>
                                <th>Default</th>
                              </tr>
                              <tr>
                                <xsl:if test="t:value/@token and t:value/@readOnly != 'true' and t:value/@readOnly != '1'">
                                  <td class="token">
                                    <xsl:value-of select="t:value/@token"/>
                                  </td>
                                </xsl:if>
                                <td class="description">
                                  <xsl:value-of select="t:value/@description"/>
                                </td>
                                <td class="values">
                                  <xsl:value-of select="t:value/@valueNotes"/>
                                </td>
                                <td class="range">
                                  <xsl:value-of select="t:value/@minValue"/>
                                  <xsl:text>..</xsl:text>
                                  <xsl:value-of select="t:value/@maxValue"/>
                                </td>
                                <td class="defaultValue">
                                  <xsl:value-of select="t:value/@defaultValue"/>
                                </td>
                              </tr>
                            </table>
                          </div>
                        </xsl:when>
                      </xsl:choose>
                      
                      <!-- remarks -->
                      <xsl:if test="t:remarks">
                        <div class="remarks">
                          <xsl:for-each select="t:remarks/t:remark">
                            <div class="remark">
                              <xsl:value-of select="."/>
                            </div>
                          </xsl:for-each>
                        </div>
                      </xsl:if>

                      <!-- examples -->

                      <xsl:if test="t:examples">
                        <div class="examples">
                          <xsl:for-each select="t:examples/t:example">
                            <p>
                              Example:
                              <br/>
                              <span class="code">
                                <xsl:value-of select="t:code"/>
                              </span>
                              <xsl:if test="t:explanation">
                                <br/>
                                <span class="exampleExplanation">
                                  <xsl:value-of select="t:explanation"/>
                                </span>
                              </xsl:if>
                            </p>
                          </xsl:for-each>
                        </div>
                      </xsl:if>
                      
                      <!-- references -->
                      <xsl:if test="t:references">
                        <div class="references">
                          see also:
                          <!-- refer to internal resource (identifier anchor) -->
                          <xsl:for-each select="t:references/t:identifierReference">
                            <a>
                              <xsl:attribute name="href">
                                <xsl:text>#identifier_</xsl:text>
                                <xsl:value-of select="translate(@identifier, ' ', '_')"/>
                              </xsl:attribute>
                              <xsl:attribute name="class">internal</xsl:attribute>
                              <xsl:choose>
                                <xsl:when test="@name">
                                  <xsl:value-of select="@name"/>
                                </xsl:when>
                                <xsl:otherwise>
                                  <xsl:value-of select="@identifier"/>
                                </xsl:otherwise>
                              </xsl:choose>
                            </a>
                          </xsl:for-each>
                          <!-- refer to external resource -->
                          <xsl:for-each select="t:references/t:webReference">
                            <a>
                              <xsl:attribute name="href">
                                <xsl:value-of select="@url"/>
                              </xsl:attribute>
                              <xsl:attribute name="class">external</xsl:attribute>
                              <xsl:attribute name="target">_blank</xsl:attribute>
                              <xsl:value-of select="@name"/>
                            </a>                            
                          </xsl:for-each>
                        </div>
                      </xsl:if>
                    </div>
                  </xsl:for-each>
                </div>
              </xsl:for-each>
            </div>
          </div>
          <div id="footer"></div>
      </body>
    </html>
  </xsl:template>
</xsl:stylesheet> 
