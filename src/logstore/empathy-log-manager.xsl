<xsl:stylesheet version = '1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>

  <xsl:output method="html" encoding="utf-8" indent="yes"/>

  <xsl:template match="/">
    <html>
      <head>
        <style type="text/css">
          <xsl:text>
            body {
              background: #fff;
	      font-family: Verdana, "Bitstream Vera Sans", Sans-Serif; 
	      font-size: 10pt;
            }
            .stamp {
              color: #999;
            }
            .top-day-stamp {
              color: #999;
              text-align: center;
              margin-bottom: 1em;
            }
            .new-day-stamp {
              color: #999;
              text-align: center;
              margin-bottom: 1em;
              margin-top: 1em;
            }
            .nick {
              color: rgb(54,100, 139);
            }
            .nick-self {
              color: rgb(46,139,87);
            }
           .nick-highlight {
              color: rgb(205,92,92);
            }
          </xsl:text>
        </style>
        <title><xsl:value-of select="$title"/></title>
      </head>
      <body>
        <xsl:apply-templates/>
      </body>
    </html>
  </xsl:template>

  <xsl:template name="get-day">
    <xsl:param name="stamp"/>
    <xsl:value-of select="substring ($stamp, 1, 8)"/>
  </xsl:template>

  <xsl:template name="format-stamp">
    <xsl:param name="stamp"/>
    <xsl:variable name="hour" select="substring ($stamp, 10, 2)"/>
    <xsl:variable name="min" select="substring ($stamp, 13, 2)"/>

    <xsl:value-of select="$hour"/>:<xsl:value-of select="$min"/>
  </xsl:template>

  <xsl:template name="format-day-stamp">
    <xsl:param name="stamp"/>
    <xsl:variable name="year" select="substring ($stamp, 1, 4)"/>
    <xsl:variable name="month" select="substring ($stamp, 5, 2)"/>
    <xsl:variable name="day" select="substring ($stamp, 7, 2)"/>

    <xsl:value-of select="$year"/>-<xsl:value-of select="$month"/>-<xsl:value-of select="$day"/>
  </xsl:template>

  <xsl:template name="header">
    <xsl:param name="stamp"/>
    <div class="top-day-stamp">
      <xsl:call-template name="format-day-stamp">
        <xsl:with-param name="stamp" select="@time"/>
      </xsl:call-template>
    </div>
  </xsl:template>  

  <xsl:template match="a">
    <xsl:text disable-output-escaping="yes">&lt;a href="</xsl:text>

    <xsl:value-of disable-output-escaping="yes" select="@href"/>

    <xsl:text disable-output-escaping="yes">"&gt;</xsl:text>

    <xsl:value-of select="@href"/>
    <xsl:text disable-output-escaping="yes">&lt;/a&gt;</xsl:text>
  </xsl:template>

  <xsl:template match="log">

    <div class="top-day-stamp">
      <xsl:call-template name="format-day-stamp">
        <xsl:with-param name="stamp" select="//message[1]/@time"/>
      </xsl:call-template>
    </div>

    <xsl:for-each select="*">

      <xsl:variable name="prev-time">
        <xsl:call-template name="get-day">
          <xsl:with-param name="stamp" select="preceding-sibling::*[1]/@time"/>
        </xsl:call-template>
      </xsl:variable>

      <xsl:variable name="this-time">
        <xsl:call-template name="get-day">
          <xsl:with-param name="stamp" select="@time"/>
        </xsl:call-template>
      </xsl:variable>

      <xsl:if test="$prev-time &lt; $this-time">
        <div class="new-day-stamp">
        <xsl:call-template name="format-day-stamp">
          <xsl:with-param name="stamp" select="@time"/>
        </xsl:call-template>
        </div>
      </xsl:if>

      <xsl:variable name="stamp">
        <xsl:call-template name="format-stamp">
          <xsl:with-param name="stamp" select="@time"/>
        </xsl:call-template>
      </xsl:variable>

      <span class="stamp">
       <xsl:value-of select="$stamp"/>
      </span>

      <xsl:variable name="nick-class">
        <xsl:choose>
          <xsl:when test="not(string(@id))">nick-self</xsl:when>
          <xsl:otherwise>nick</xsl:otherwise>
        </xsl:choose>
      </xsl:variable>

      <span class="{$nick-class}">
        &lt;<xsl:value-of select="@name"/>&gt;
      </span>
 
      <xsl:apply-templates/>
      <br/>

    </xsl:for-each>

  </xsl:template>

</xsl:stylesheet>
