<?xml version="1.0" encoding="UTF-8"?>
<!-- remove some items from the GIR

     Tips from http://www.xmlplease.com/xsltidentity -->

<xsl:stylesheet xmlns:gi="http://www.gtk.org/introspection/core/1.0"
                xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
		version="1.0">

  <xsl:template match="@*|node()">
    <xsl:copy>
      <xsl:apply-templates select="@*|node()"/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="gi:alias[@name='IntSet' or @name='IntSetIter' or
    @name='IntSetFastIter']">
   <xsl:message>Kludged around GNOME Bug #629668</xsl:message>
  </xsl:template>

  <xsl:template match="gi:field[@name='dbus_connection']">
   <xsl:message>Kludged around GNOME Bug #616375</xsl:message>
  </xsl:template>

</xsl:stylesheet>
