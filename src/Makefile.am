include $(top_srcdir)/tools/shave.mk
include $(top_srcdir)/tools/flymake.mk

AM_CPPFLAGS =						\
	$(ERROR_CFLAGS)					\
	-I$(top_srcdir)					\
	-DG_LOG_DOMAIN=\"TPLogger\"			\
	$(LIBTPL_CFLAGS)				\
	$(TPL_CFLAGS)					\
	$(DISABLE_DEPRECATED)				\
	$(WARN_CFLAGS)


LDADD = \
	$(top_builddir)/telepathy-logger/libtelepathy-logger.la \
	$(LIBTPL_LIBS)

# TODO removeme: kludge to let the compilation work, or it doesn't know how to buld the lib
$(top_builddir)/telepathy-logger/libtelepathy-logger.la:
	$(MAKE) -C $(top_builddir)/libtelepathy-logger libtelepathy-logger.la

BUILT_SOURCES = \
	dbus-service-server.h	\
	dbus-service-client.h

bin_PROGRAMS = \
		telepathy-logger-dbus	\
		telepathy-logger	\
		test-api

telepathy_logger_dbus_SOURCES = \
		dbus-service.c		\
		telepathy-logger-dbus.c
#telepathy_logger_dbus_LDADD = $(LDADD) $(TPL_LIBS)

test_api_SOURCES = test_api.c
#test_api_LDADD = $(LDADD) $(LIBTPL_LIBS)

telepathy_logger_SOURCES = telepathy-logger.c
#telepathy_logger_LDADD = $(LDADD) $(TPL_LIBS)

nodist_telepathy_logger_SOURCES = $(BUILT_SOURCES)

check_c_sources = \
	$(test_api_SOURCES)			\
	$(telepathy_logger_SOURCES)		\
	$(telepathy_logger_dbus_SOURCES)
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

EXTRA_DIST = dbus-service.xml

#Rule to generate the binding headers
dbus-service-server.h:  dbus-service.xml
	dbus-binding-tool --prefix=tpl_dbus_service --mode=glib-server $< > $@

dbus-service-client.h: dbus-service.xml
	dbus-binding-tool --prefix=tpl_dbus_service --mode=glib-client $< > $@


CLEANFILES = $(BUILT_SOURCES)

# Dbus service file
servicedir = $(DBUS_SERVICES_DIR)
service_in_files = \
	org.freedesktop.Telepathy.TelepathyLoggerService.service.in	\
	org.freedesktop.Telepathy.Client.TelepathyLogger.service.in
service_DATA = \
	$(service_in_files:.service.in=.service) \
	org.freedesktop.Telepathy.Client.TelepathyLogger.client

# Rule to make the service file with bindir expanded
%.service: %.service.in
	sed -e "s|%bindir%|$(bindir)|" $< > $@
