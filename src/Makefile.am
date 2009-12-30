include $(top_srcdir)/tools/shave.mk
include $(top_srcdir)/tools/flymake.mk

AM_CPPFLAGS =						\
	$(ERROR_CFLAGS)					\
	-I$(top_srcdir)                          	\
	-I$(top_srcdir)/src				\
	-DG_LOG_DOMAIN=\"TPLogger\"			\
	$(TPL_CFLAGS)					\
	$(LIBTPL_CFLAGS)				\
	$(DISABLE_DEPRECATED)				\
	$(WARN_CFLAGS)

#LDADD = $(TPL_LIBS)					\
#	$(LIBTPL_LIBS)	

BUILT_SOURCES = \
	tpl-dbus-service-server.h	\
	tpl-dbus-service-client.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libtelepathy-logger.pc

lib_LTLIBRARIES = libtelepathy-logger.la

LIBTPLdir = $(includedir)/telepathy-logger
LIBTPL_HEADERS = \
		tpl-channel.h			\
		tpl-contact.h			\
		tpl-log-entry.h			\
		tpl-log-entry-text.h		\
		tpl-log-manager.h		\
		tpl-log-manager-priv.h		\
		tpl-log-store-empathy.h		\
		tpl-log-store.h			\
		tpl-observer.h			\
		tpl-text-channel-context.h	\
		tpl-time.h			\
		tpl-utils.h


libtelepathy_logger_la_SOURCES = \
		tpl-observer.c			\
		tpl-channel.c			\
		tpl-text-channel-context.c	\
		tpl-log-entry.c			\
		tpl-log-entry-text.c		\
		tpl-contact.c			\
		tpl-log-manager.c		\
		tpl-log-store.c			\
		tpl-log-store-empathy.c		\
		tpl-utils.c			\
		tpl-time.c 

bin_PROGRAMS = \
		telepathy-logger-dbus	\
		telepathy-logger	\
		test-api

telepathy_logger_dbus_SOURCES = \
		tpl-dbus-service.c		\
		telepathy-logger-dbus.c
telepathy_logger_dbus_LDADD = libtelepathy-logger.la $(TPL_LIBS)

test_api_SOURCES = test_api.c
test_api_LDADD = libtelepathy-logger.la $(LIBTPL_LIBS)

telepathy_logger_SOURCES = \
		tpl-headless-logger-init.c	\
		telepathy-logger.c
telepathy_logger_LDADD = libtelepathy-logger.la $(TPL_LIBS)

nodist_telepathy_logger_SOURCES = $(BUILT_SOURCES)

check_c_sources = \
	$(libtelepathy_logger_la_SOURCES)	\
	$(test_api_SOURCES)			\
	$(telepathy_logger_SOURCES)		\
	$(telepathy_logger_dbus_SOURCES)
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

EXTRA_DIST = \
		tpl-dbus-service.xml

#Rule to generate the binding headers
tpl-dbus-service-server.h:  tpl-dbus-service.xml
	dbus-binding-tool --prefix=tpl_dbus_service --mode=glib-server $<> $@

tpl-dbus-service-client.h: tpl-dbus-service.xml
	dbus-binding-tool --prefix=tpl_dbus_service --mode=glib-client $<> $@


# rules for making the glib enum objects
%-enumtypes.h: %.h Makefile.in
	$(QUIET_GEN)glib-mkenums \
	--fhead "#ifndef __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__\n#define __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
	--fprod "/* enumerations from \"@filename@\" */\n" \
	--vhead "GType @enum_name@_get_type (void);\n#define $(shell echo $* | tr [:lower:]- [:upper:]_ | sed 's/_.*//')_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n"         \
	--ftail "G_END_DECLS\n\n#endif /* __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__ */" \
	$< > $@

%-enumtypes.c: %.h Makefile.in
	$(QUIET_GEN)glib-mkenums \
	--fhead "#include <$*.h>\n#include <$*-enumtypes.h>" \
	--fprod "\n/* enumerations from \"@filename@\" */" \
	--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {"     \
	--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@VALUENAME@\" }," \
	--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
	$< > $@

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
$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|%bindir%|$(bindir)|" $<> $@
