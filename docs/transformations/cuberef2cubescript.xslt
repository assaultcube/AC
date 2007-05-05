<?xml version="1.0" encoding="utf-8"?>
<!-- (C) 2007 Adrian 'driAn' Henke - http://www.sprintf.org - ZLIB licensed -->

<!--
  transforms a cuberef document to a cubescript document
-->

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:r="http://cubers.net/Schemas/CubeRef/Reference" xmlns:t="http://cubers.net/Schemas/CubeRef/Types">

  <xsl:output method="text" encoding="utf-8"/>

  <xsl:template match="/r:cuberef">
    <xsl:text>// auto generated script to make the command reference readable for AssaultCube&#13;&#10;</xsl:text>

    <!-- sections -->
    <xsl:for-each select="t:sections/t:section">
      <xsl:text>docsection </xsl:text>
      <xsl:text>[</xsl:text>
      <xsl:value-of select="@name"/>
      <xsl:text>]</xsl:text>
      <xsl:text>&#13;&#10; &#13;&#10;</xsl:text>

      <!-- identifiers -->
      <xsl:for-each select="t:identifiers/*">

        <xsl:text>docident </xsl:text>
        <xsl:text>[</xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>] [</xsl:text>
        <xsl:value-of select="normalize-space(t:description)"/>
        <xsl:text>];</xsl:text>
        <xsl:text>&#13;&#10;</xsl:text>
        
        <!-- argument descriptions -->
        <xsl:for-each select="t:arguments/t:argument">
          <xsl:text>docargument </xsl:text>
          <xsl:text>[</xsl:text>
          <xsl:value-of select="@token"/>
          <xsl:text>] [</xsl:text>
          <xsl:value-of select="@description"/>
          <xsl:text>] [</xsl:text>
          <xsl:value-of select="@valueNotes"/>
          <xsl:text>];</xsl:text>
          <xsl:text>&#13;&#10;</xsl:text>
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
          <xsl:text>&#13;&#10;</xsl:text>
        </xsl:if>

        <!-- remarks -->
        <xsl:for-each select="t:remarks/t:remark">
          <xsl:text>docremark </xsl:text>
          <xsl:text>[</xsl:text>
          <xsl:value-of select="normalize-space(.)"/>
          <xsl:text>];</xsl:text>
          <xsl:text>&#13;&#10;</xsl:text>          
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
          <xsl:text>&#13;&#10;</xsl:text>
        </xsl:for-each>

        <xsl:text> &#13;&#10;</xsl:text>
      </xsl:for-each>
    </xsl:for-each>

  </xsl:template>

</xsl:stylesheet>
