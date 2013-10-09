<?xml version="1.0" encoding="utf-8"?>
<!-- 
  Transforms the ../reference.xml document into a CubeScript document.

  Written by:	Adrian 'driAn' Henke (of Rabid Viper Productions).

  You may be able to redistribute this content under specific
  conditions. Please read the licensing information, available
  at http://assault.cubers.net/docs/license.html for the
  conditions that would apply to what you may be redistributing.
-->

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:t="http://assault.cubers.net/docs/xml">

  <xsl:output method="text" encoding="ASCII"/>
  
  <!-- horizontal tab -->
  <xsl:variable name="indent">
    <xsl:text>&#09;</xsl:text>
  </xsl:variable>

  <!-- line feed, CRLF (dos/win) -->
  <xsl:variable name="newline">
    <xsl:text>&#13;&#10;</xsl:text>
  </xsl:variable>

  <xsl:template match="/t:cuberef">
    <xsl:text>// This CubeScript file has been automatically generated from AssaultCube's ./docs/reference.xml</xsl:text>
    <xsl:value-of select="$newline"/>
    <xsl:text>// DO NOT MODIFY THIS FILE - Instead, modify ./docs/reference.xml and generate this file automatically.</xsl:text>
    <xsl:value-of select="$newline"/>
    <xsl:text>// To generate this file automatically, please carefully read the comment at the top of reference.xml</xsl:text>
    <xsl:value-of select="$newline"/>
    <xsl:value-of select="$newline"/>

    <!-- sections -->
    <xsl:for-each select="t:sections/t:section">
      <xsl:text>docsection </xsl:text>
      <xsl:text>[</xsl:text>
      <xsl:value-of select="@name"/>
      <xsl:text>]</xsl:text>
      <xsl:value-of select="$newline"/>

      <!-- identifiers -->
      <xsl:for-each select="t:identifiers/*">
        <xsl:sort select="@name"/> <!-- always sort to avoid excessive sorting inside the Cube games -->
        <xsl:text>docident </xsl:text>
        <xsl:text>[</xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>] [</xsl:text>
        <xsl:value-of select="normalize-space(t:description)"/>
        <xsl:text>];</xsl:text>
        <xsl:value-of select="$newline"/>
        
        <!-- argument descriptions -->
        <xsl:for-each select="t:arguments/*">
          <xsl:text>docargument </xsl:text>
          <xsl:text>[</xsl:text>
          <xsl:value-of select="@token"/>
          <xsl:text>] [</xsl:text>
          <xsl:value-of select="@description"/>
          <xsl:text>] [</xsl:text>
          <xsl:value-of select="@valueNotes"/>
          <xsl:text>] [</xsl:text>
          <xsl:choose>
            <xsl:when test="local-name() = 'variableArgument'">
              <xsl:text>1</xsl:text>
            </xsl:when>
            <xsl:otherwise>
              <xsl:text>0</xsl:text>
            </xsl:otherwise>
          </xsl:choose>
          <xsl:text>];</xsl:text>
          <xsl:value-of select="$newline"/>
        </xsl:for-each>
        
        <!-- var descriptions (fake arg) -->
        <xsl:if test="t:value">
          <xsl:text>docargument </xsl:text>
          <xsl:text>[</xsl:text>
          <xsl:value-of select="t:value/@token"/>
          <xsl:text>] [</xsl:text>
          <xsl:value-of select="t:value/@description"/>
          <xsl:text>] [</xsl:text>
          <xsl:value-of select="t:value/@valueNotes"/>
          <xsl:choose>
            <xsl:when test="t:value/@readOnly">
              <xsl:text> read-only</xsl:text>
            </xsl:when>
            <xsl:otherwise>
              <xsl:if test="t:value/@valueNotes">
                <xsl:text> </xsl:text>
              </xsl:if>
              <xsl:text>min </xsl:text>
              <xsl:value-of select="t:value/@minValue"/>
              <xsl:text>/max </xsl:text>
              <xsl:value-of select="t:value/@maxValue"/>
              <xsl:text>/default </xsl:text>
              <xsl:value-of select="t:value/@defaultValue"/>
            </xsl:otherwise>
          </xsl:choose>
          <xsl:text>];</xsl:text>
          <xsl:value-of select="$newline"/>
        </xsl:if>

        <!-- remarks -->
        <xsl:for-each select="t:remarks/t:remark">
          <xsl:text>docremark </xsl:text>
          <xsl:text>[</xsl:text>
          <xsl:value-of select="normalize-space(.)"/>
          <xsl:text>];</xsl:text>
          <xsl:value-of select="$newline"/>
        </xsl:for-each>
        
        <!-- examples -->
        <xsl:for-each select="t:examples/*">
          <xsl:text>docexample </xsl:text>
          <xsl:text>[</xsl:text>
          <xsl:value-of select="normalize-space(t:code)"/>
          <xsl:text>] [</xsl:text>
          <xsl:value-of select="normalize-space(t:explanation)"/><!-- FIXME: will only use the 1st explanation (2010oct06:flowtron) -->
          <xsl:text>];</xsl:text>
          <xsl:value-of select="$newline"/>
        </xsl:for-each>

        <!-- default keys -->
        <xsl:for-each select="t:defaultKeys/*">
          <xsl:text>dockey </xsl:text>
          <xsl:text>[</xsl:text>
          <xsl:value-of select="@alias"/>
          <xsl:text>] [</xsl:text>
          <xsl:value-of select="@name"/>
          <xsl:text>] [</xsl:text>
          <xsl:value-of select="@description"/>
          <xsl:text>];</xsl:text>
          <xsl:value-of select="$newline"/>
        </xsl:for-each>

        <!-- references -->
        <xsl:for-each select="t:references/t:reference">
          <xsl:text>docref </xsl:text>
          <xsl:text>[</xsl:text>
          <xsl:value-of select="@name"/>
          <xsl:text>] [</xsl:text>
          <xsl:value-of select="@identifier"/>
          <xsl:text>] [</xsl:text>
          <xsl:value-of select="@url"/>
          <xsl:text>];</xsl:text>
          <xsl:value-of select="$newline"/>
        </xsl:for-each>

      </xsl:for-each>
    </xsl:for-each>

  </xsl:template>

</xsl:stylesheet>
