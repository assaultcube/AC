<?xml version="1.0" encoding="utf-8"?>

<!-- (C) 2007 Adrian Henke, ZLIB license -->

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">

  <xsl:output method="text" encoding="utf-8"/>

  <xsl:template match="/cuberef">
    <xsl:text>// auto generated script to make the command reference readable for ActionCube&#13;&#10;</xsl:text>
      
    <xsl:for-each select="section">
      <xsl:text>docsection </xsl:text>
      <xsl:text>[</xsl:text>
      <xsl:value-of select="@name"/>
      <xsl:text>]</xsl:text>
      <xsl:text>&#13;&#10; &#13;&#10;</xsl:text>

      <xsl:for-each select="command">

        <xsl:text>docident </xsl:text>
        <xsl:text>[</xsl:text>
        <xsl:value-of select="@name"/>
        <xsl:text>] [</xsl:text>
        <xsl:value-of select="description"/>
        <xsl:text>] [</xsl:text>
        <xsl:value-of select="remarks"/>
        <xsl:text>];</xsl:text>
        <xsl:text>&#13;&#10;</xsl:text>
        
        <xsl:for-each select="arguments/argument">
          <xsl:text>docargument </xsl:text>
          <xsl:text>[</xsl:text>
          <xsl:value-of select="@token"/>
          <xsl:text>] [</xsl:text>
          <xsl:value-of select="@description"/>
          <xsl:text>] [</xsl:text>
          <xsl:value-of select="@values"/>
          <xsl:text>];</xsl:text>
          <xsl:text>&#13;&#10;</xsl:text>
        </xsl:for-each>

        <xsl:text> &#13;&#10;</xsl:text>
      </xsl:for-each>
    </xsl:for-each>

  </xsl:template>

</xsl:stylesheet>
