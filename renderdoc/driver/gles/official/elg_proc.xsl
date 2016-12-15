<?xml version="1.0"?>
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
 <xsl:output omit-xml-declaration="yes" indent="yes"/>
 
<!--<xsl:strip-space elements="*"/>-->
<!-- <xsl:preserve-space elements="command registry/commands/command/param"/> -->

<xsl:template match="param"><xsl:value-of select="ptype"/> <xsl:value-of select="name"/>,</xsl:template>

<xsl:template match="/registry/commands/command">
typedef <xsl:value-of select="proto/ptype"/> (*<xsl:value-of select="proto/name"/>) (<xsl:apply-templates select="param"/>);
</xsl:template>

</xsl:stylesheet>
    
