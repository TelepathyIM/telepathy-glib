gsettings_files = \
	org.freedesktop.Telepathy.Logger.gschema.xml.in \
	$(NULL)
gsettings_SCHEMAS = $(gsettings_files:.xml.in=.xml)
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

servicefiledir = $(datadir)/dbus-1/services
servicefile_in_files = \
	org.freedesktop.Telepathy.Client.Logger.service.in	\
	org.freedesktop.Telepathy.Logger.service.in
servicefile_DATA = $(servicefile_in_files:.service.in=.service)
%.service: %.service.in Makefile
	$(AM_V_GEN)sed -e "s|[@]libexecdir[@]|$(libexecdir)|" $< > $@

clientfiledir = $(datarootdir)/telepathy/clients
clientfile_DATA = Logger.client

EXTRA_DIST = 				\
	$(gsettings_files)		\
	$(clientfile_DATA)		\
	$(servicefile_in_files)

CLEANFILES = \
	$(gsettings_SCHEMAS) \
	$(servicefile_DATA)
