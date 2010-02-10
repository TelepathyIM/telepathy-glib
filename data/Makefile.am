schemasdir	 = $(GCONF_SCHEMA_FILE_DIR)
schemas_DATA 	 = telepathy-logger.schemas

servicefiledir = $(datadir)/dbus-1/services
servicefile_in_files = \
	org.freedesktop.Telepathy.Client.TelepathyLogger.service.in	\
	org.freedesktop.Telepathy.Logger.service.in
servicefile_DATA = $(servicefile_in_files:.service.in=.service)
%.service: %.service.in Makefile
	$(QUIET_GEN)sed -e "s|%bindir%|$(bindir)|" $< > $@

clientfiledir = $(datarootdir)/telepathy/clients
clientfile_DATA = TelepathyLogger.client

if GCONF_SCHEMAS_INSTALL
install-data-local:
	if test -z "$(DESTDIR)" ; then \
	  for p in $(schemas_DATA) ; do \
	    GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p ; \
	  done \
	fi
else
install-data-local:
endif

EXTRA_DIST = 				\
	$(schemas_in_files)		\
	$(schemas_DATA)			\
	$(clientfile_DATA)		\
	$(servicefile_in_files)		\
	dbus-service.xml

DISTCLEANFILES = 		\
	$(servicefile_DATA)
