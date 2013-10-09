<?xml version="1.0" encoding="UTF-8"?>
<xsl:transform version="1.0" xmlns="http://www.w3.org/1999/xhtml" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:t="http://assault.cubers.net/docs/xml">
<!-- 
  This file transforms the ../reference.xml document into an XHTML webpage.

  Written by:	Rabid Viper Productions.
  Special thanks to Adrian 'driAn' Henke for the original document
  that all these XML-related files are based upon.

  You may be able to redistribute this content under specific
  conditions. Please read the licensing information, available
  at http://assault.cubers.net/docs/license.html for the
  conditions that would apply to what you may be redistributing.
-->

  <xsl:output method="html" omit-xml-declaration="yes" encoding="utf-8" doctype-public="-//W3C//DTD XHTML 1.0 Transitional//EN" doctype-system="http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd"/>

  <xsl:template match="t:key">
    <i>
      <xsl:choose>
        <xsl:when test="@name">
          <xsl:value-of select="@name"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="@alias"/>
        </xsl:otherwise>
      </xsl:choose>
    </i>
    <xsl:if test="@description">
      <xsl:text> - </xsl:text>
      <xsl:value-of select="@description"/>
    </xsl:if>
  </xsl:template>
  
  <xsl:template name="identifierLink">
    <a>
      <xsl:attribute name="href">
        <xsl:text>#identifier_</xsl:text>
        <xsl:value-of select="translate(@name, ' ', '_')"/>
      </xsl:attribute>
      <xsl:attribute name="title">
        <xsl:value-of select="@name"/>
      </xsl:attribute>
      <xsl:value-of select="@name"/>
    </a>
  </xsl:template>

  <xsl:template match="/t:cuberef">
    <html>
      <head>
        <meta http-equiv="Content-Type" content="application/xhtml+xml;charset=UTF-8" />
        <meta name="robots" content="NOODP" />
        <meta name="author" content="Rabid Viper Productions" />
        <meta name="copyright" content="You may be able to redistribute this content under specific conditions.
        Please read the licensing information, available @ http://assault.cubers.net/docs/license.html for the
        conditions that would apply to what you may be redistributing." />
        <title>AssaultCube Documentation :: CubeScript</title>
        <link rel="stylesheet" href="css/main.css" />
        <link rel="stylesheet" href="css/docs.css" />
        <link rel="stylesheet" href="css/referencexml.css" />
        <link rel="shortcut icon" href="images/favicon.ico" />
      </head>
      <body>
        <div id="container">
          <div id="docsheader">
            AssaultCube Documentation
          </div>
          <div id="logo">
            <a href="index.html"><img src="images/aclogo.png"
            alt="AssaultCube" width="193px" height="81px" /></a>
          </div>
          <div id="menubar">
            &#160;
          </div>
          <div class="docsmain">
            <div id="gohome">
              <a href="index.html">Go to documentation index &#8629;</a>
            </div>
            <div id="topbutton">
              <a href="#">&#8593; Top of the page?</a>
            </div>
            <h2>
              CubeScript
            </h2>
            <p>
              CubeScript is the scripting language of AssaultCube and it is similar to scripting languages of other games, except
              that its a bit more powerful because it is almost a full programming language.
            </p>
            <p>
              CubeScript consists of the command itself, followed by any number of arguments separated by whitespace. You can:
            </p>
            <!-- Try to keep these examples very, very simple... -->
            <ul style="list-style-type : square;">
              <li>
                Use <span class="code">""</span> to quote strings that have whitespace within them.
                <ul><li>Input example: <span class="code">alias grass "is greener"</span></li></ul>
              </li>
              <li>
                Use <span class="code">$</span> to capture the value of a variable/alias/bracketed-string as an argument for a command.
                <ul>
                  <li>Input example: <span class="code">echo "The grass" $grass</span></li>
                  <li>Output: <span class="code">The grass is greener</span></li>
                  <li>Input example: <span class="code">echo "Your FOV is" $fov</span></li>
                  <li>Output: <span class="code">Your FOV is 90.0</span></li>
                </ul>
              </li>
              <li>
                Use <span class="code">;</span> to sequence multiple commands into one.
                <ul>
                  <li>Input example: <span class="code">alias grass "is yellow"; echo "The grass" $grass</span></li>
                  <li>Output: <span class="code">The grass is yellow</span></li>
                </ul>
              </li>
              <li>
                You can also use <span class="code">()</span> or <span class="code">[]</span> to quote strings (i.e. instead of "").
                These can be nested infinitely and may contain linefeeds.
                The normal brackets <span class="code">()</span> are special, as they evaluate commands contained within them,
                before evaluating the surrounding command.
                <ul>
                  <li>Input example: <span class="code">alias 4x2 (* 2 (+ 1 3))</span></li> 
                  <li>Input command: <span class="code">echo $4x2</span></li>
                  <li>Output: <span class="code">8</span></li>
                  <li>Input example: <span class="code">alias grass [ is greener ]</span></li>
                </ul>
              </li>
              <li>
                Lastly, you can use the automatically-set aliases of <span class="code">$arg1</span>
                (.. and <span class="code">$arg2</span>, <span class="code">$arg3</span>, etc) in your scripts. 
                <ul>
                  <li>Input example: <span class="code">alias grass [ if (= $arg1 1) [ echo "The grass is green" ] [ if (= $arg1 2) [ echo "The grass is yellow" ] ] ]</span></li>
                  <li>Input command: <span class="code">grass 1</span></li>
                  <li>Output: <span class="code">The grass is green</span></li>
                  <li>Input command: <span class="code">grass 2</span></li>
                  <li>Output: <span class="code">The grass is brown</span></li>
                </ul>
              </li>
            </ul>
            <p>
              It is recommended to read examples of this in your ./config/ folder to understand them better. The documentation below
              contains a reference of ALL of the CubeScript commands at your disposal. Have fun!
            </p>
            <div id="content">
              <!-- contents -->
              <div id="contents">
                <p>
                  <xsl:value-of select="t:description"/>
                </p>
                <div class="dottedline"></div>
                <h3>CATEGORIES:</h3>
                <ul id="categorylist">
                  <xsl:for-each select="t:sections/t:section">
                    <xsl:sort select="@sortindex[not(../../@sort) or ../../@sort = 'true' or ../../@sort = '1']" />
                    <li>
                      <a>
                        <xsl:attribute name="href">
                          <xsl:text>#section_</xsl:text>
                          <xsl:value-of select="translate(@name, ' ', '_')" />
                        </xsl:attribute>
                        <xsl:value-of select="@name"/>
                      </a>
                    </li>
                  </xsl:for-each>
                </ul>
              </div>
              
              <!-- sections -->
              <xsl:for-each select="t:sections/t:section">
                <xsl:sort select="@sortindex[not(../../@sort) or ../../@sort = 'true' or ../../@sort = '1']"/>
                <div class="dottedline"></div>
                <div class="section">
                  <h3>
                    <!-- section anchor -->
                    <xsl:attribute name="id">
                      <xsl:text>section_</xsl:text>
                      <xsl:value-of select="translate(@name, ' ', '_')"/>
                    </xsl:attribute>
                    <xsl:value-of select="@name"/>
                  </h3>

                  <!-- description -->
                  <p>
                    <xsl:value-of select="t:description"/>
                  </p>
                  
                  <!-- identifiers -->
                  <xsl:for-each select="t:identifiers/*">
                    <xsl:sort select="@name[not(../../@sort) or ../../@sort = 'true' or ../../@sort = '1']"/>
                    <xsl:variable name="readOnly" select="t:value/@readOnly and (t:value/@readOnly = 'true' or t:value/@readOnly = '1')"/>
                    
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
                          <xsl:when test="t:value and not($readOnly)"><!-- variable value -->
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
                        <xsl:when test="t:arguments/*"><!-- command args -->
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
                                    <xsl:if test="@optional">
                                      <xsl:text> (optional)</xsl:text>
                                    </xsl:if>
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
                                <xsl:if test="not($readOnly)">
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
                                <xsl:if test="not($readOnly)">
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
                              <span class="code">
                                <xsl:value-of select="t:code"/>
                              </span>
                              <xsl:if test="t:explanation">
                                <xsl:for-each select="t:explanation">
                                <span class="exampleExplanation">
                                  <xsl:value-of select="."/>
                                </span>
								</xsl:for-each>
                              </xsl:if>
                            </p>
                          </xsl:for-each>
                        </div>
                      </xsl:if>
                      
                      <!-- return value -->
                      
                      <xsl:if test="t:return">
                        <p>
                          Return value: <xsl:value-of select="t:return/@description"/> <xsl:if test="t:return/@valueNotes">, <xsl:value-of select="t:return/@valueNotes"/></xsl:if>
                        </p>
                      </xsl:if>
                        
                      <!-- default keys -->
                      <xsl:if test="t:defaultKeys/*">
                        <div class="defaultKeys">
                          <p>
                            <xsl:choose>
                              <xsl:when test="count(t:defaultKeys/*) = 1">
                                <xsl:text>default key: </xsl:text>
                                <xsl:apply-templates select="t:defaultKeys/*"/>
                              </xsl:when>
                              <xsl:otherwise>
                                <xsl:text>default keys:</xsl:text>
                                <br/>
                                <xsl:for-each select="t:defaultKeys/*">
                                  <xsl:apply-templates select="."/>
                                  <br/>
                                </xsl:for-each>
                              </xsl:otherwise>
                            </xsl:choose>
                          </p>
                        </div>
                      </xsl:if>
                      
                      <!-- references -->
                      <xsl:if test="t:references">
                        <div class="references">
                          see also:
                          <!-- refer to identifiers (identifier anchor) -->
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
                            <xsl:if test="position() != last()">
                              <xsl:text>, </xsl:text>
                            </xsl:if>
                          </xsl:for-each>
                          <!-- refer to web resources -->
                          <xsl:for-each select="t:references/t:webReference">
						    <xsl:text>, </xsl:text><!-- we assume it's never the only entry! -->
                            <a>
                              <xsl:attribute name="href">
                                <xsl:value-of select="@url"/>
                              </xsl:attribute>
                              <xsl:attribute name="class">external</xsl:attribute>
                              <xsl:attribute name="target">_blank</xsl:attribute>
                              <xsl:value-of select="@name"/>
                            </a>
                          </xsl:for-each>
						  <!-- refer to wiki resources -->
                          <xsl:for-each select="t:references/t:wikiReference">
						    <xsl:text>, </xsl:text><!-- we assume it's never the only entry! -->
                            <a>
                              <xsl:attribute name="href">http://wiki.cubers.net/action/view/<xsl:value-of select="@article"/></xsl:attribute>
                              <xsl:attribute name="class">external</xsl:attribute>
                              <xsl:attribute name="target">_blank</xsl:attribute>
                              <xsl:value-of select="@article"/>
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
          <!-- XML: CONTENTS PANEL -->
          <div class="commandlist">
            CubeScript command &amp; variable list
          </div>
          <div id="contentspanel">
            <xsl:if test="t:sections">
              <xsl:for-each select="t:sections/t:section">
                <xsl:sort select="@sortindex[not(../../@sort) or ../../@sort = 'true' or ../../@sort = '1']"/>
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
                        <xsl:sort select="@name[not(../../@sort) or ../../@sort = 'true' or ../../@sort = '1']"/>
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
        </div>
      </body>
    </html>
  </xsl:template>
</xsl:transform> 
