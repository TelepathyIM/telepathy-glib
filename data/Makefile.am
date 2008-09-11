# Dbus service file
servicedir = $(DBUS_SERVICES_DIR)
service_in_files = org.maemo.Telepathy.StreamEngine.service.in
EXTRA_DIST=$(service_in_files)
service_DATA = $(service_in_files:.service.in=.service)
CLEANFILES=$(service_DATA)

# Rule to make the service file with libexecdir expanded
$(service_DATA): $(service_in_files) Makefile.in
	sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@
