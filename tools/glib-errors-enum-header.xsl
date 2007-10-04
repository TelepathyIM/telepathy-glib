<!-- Stylesheet to extract GLib error enumerations from the Telepathy spec.
The master copy of this stylesheet is in libtelepathy-glib - please make any
changes there.

Copyright (C) 2006, 2007 Collabora Limited

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
-->

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:tp="http://telepathy.freedesktop.org/wiki/DbusSpec#extensions-v0"
  exclude-result-prefixes="tp">

  <xsl:output method="text" indent="no" encoding="ascii"/>

  <xsl:template match="tp:error" mode="gtkdoc">
 * @TP_ERROR_<xsl:value-of select="translate(@name, 'abcdefghijklmnopqrstuvwxyz .', 'ABCDEFGHIJKLMNOPQRSTUVWXYZ__')"/>: <xsl:value-of select="concat(../@namespace, '.', translate(@name, ' ', ''))"/>:
 * <xsl:value-of select="translate(tp:docstring, '&#13;&#10;', '')"/>
  </xsl:template>

  <xsl:template match="tp:error">
<xsl:text>    TP_ERROR_</xsl:text><xsl:value-of select="translate(@name, 'abcdefghijklmnopqrstuvwxyz .', 'ABCDEFGHIJKLMNOPQRSTUVWXYZ__')"/>,
</xsl:template>

  <xsl:template match="text()"/>

  <xsl:template match="/tp:errors">/* Generated from the Telepathy spec

<xsl:for-each select="tp:copyright">
<xsl:value-of select="."/><xsl:text>
</xsl:text></xsl:for-each><xsl:text>
</xsl:text><xsl:value-of select="tp:license"/>
*/

#include &lt;glib-object.h&gt;

G_BEGIN_DECLS

GType tp_error_get_type (void);

/**
 * TP_TYPE_ERROR:
 *
 * The GType of the Telepathy error enumeration.
 */
#define TP_TYPE_ERROR (tp_error_get_type())

/**
 * TpError:<xsl:apply-templates select="tp:error" mode="gtkdoc"/>
 *
 * Enumerated type representing the Telepathy D-Bus errors.
 */
typedef enum {
<xsl:apply-templates select="tp:error"/>} TpError;

G_END_DECLS
</xsl:template>

</xsl:stylesheet>

<!-- vim:set sw=2 sts=2 et noai noci: -->
