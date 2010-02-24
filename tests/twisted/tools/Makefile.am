exec-with-log.sh: exec-with-log.sh.in
	$(AM_V_GEN)sed -e "s|[@]abs_top_builddir[@]|@abs_top_builddir@|g" \
		-e "s|[@]abs_top_srcdir[@]|@abs_top_srcdir@|g" $< > $@
	@chmod +x $@

%.conf: %.conf.in
	$(AM_V_GEN)sed -e "s|[@]abs_top_builddir[@]|@abs_top_builddir@|g" $< > $@

%.service: %.service.in
	$(AM_V_GEN)sed -e "s|[@]abs_top_builddir[@]|@abs_top_builddir@|g" $< > $@

# D-Bus service file for testing
# Don't use the full filename for the .in if > 99 character: >99 char filenames
# in tarballs are non-portable (and automake 1.8 doesn't let us build
# non-archaic tarballs)
service_in_files = org.freedesktop.Telepathy.Client.Logger.service.in
service_files = $(service_in_files:.service.in=.service)

# D-Bus config file for testing
conf_in_files = tmp-session-bus.conf.in
conf_files = $(conf_in_files:.conf.in=.conf)

BUILT_SOURCES = $(service_files) $(conf_files) exec-with-log.sh

EXTRA_DIST = \
	$(service_in_files) \
	$(conf_in_files) \
	exec-with-log.sh.in \
	with-session-bus.sh

CLEANFILES = \
    $(BUILT_SOURCES) \
    tpl-testing.log
