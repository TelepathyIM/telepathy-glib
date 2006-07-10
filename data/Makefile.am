# Mission Control's channel handler file
chandlerdir = $(datadir)/telepathy/managers
chandler_DATA = org.freedesktop.Telepathy.VoipEngine.chandler

# Dbus service file
servicedir = $(DBUS_SERVICES_DIR)
service_in_files = org.freedesktop.Telepathy.VoipEngine.service.in
EXTRA_DIST=$(service_in_files) $(chandler_DATA)
service_DATA = $(service_in_files:.service.in=.service)
CLEANFILES=$(service_DATA)

# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile.in
	sed -e "s|\@bindir\@|$(bindir)|" $< > $@
