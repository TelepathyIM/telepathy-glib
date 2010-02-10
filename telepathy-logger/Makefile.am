include $(top_srcdir)/tools/shave.mk
include $(top_srcdir)/tools/flymake.mk

AM_CPPFLAGS =						\
	$(ERROR_CFLAGS)					\
	-DG_LOG_DOMAIN=\"telepathy-logger\"		\
	$(LIBTPL_CFLAGS)				\
	$(DISABLE_DEPRECATED)				\
	$(WARN_CFLAGS)

BUILT_SOURCES = \
	dbus-service-server.h	\
	dbus-service-client.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libtelepathy-logger.pc

lib_LTLIBRARIES = libtelepathy-logger.la

LIBTPLdir = $(includedir)/telepathy-logger
LIBTPL_HEADERS = \
		contact.h			\
		channel.h			\
		channel-text.h			\
		datetime.h			\
		dbus-service.h			\
		dbus-service-client.h		\
		log-entry.h			\
		log-entry-text.h		\
		log-manager.h			\
		log-store.h			\
		log-store-empathy.h		\
		util.h


libtelepathy_logger_la_SOURCES = \
		channel.c			\
		channel-factory.c		\
		channel-text.c			\
		conf.c				\
		contact.c			\
		datetime.c			\
		dbus-service.c			\
		log-entry.c			\
		log-entry-text.c		\
		log-manager.c			\
		log-store.c			\
		log-store-empathy.c		\
		log-store-factory.c		\
		observer.c			\
		util.c

check_c_sources = \
	$(libtelepathy_logger_la_SOURCES)	\
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

CLEANFILES = $(BUILT_SOURCES)

#Rule to generate the binding headers
dbus-service-server.h:  $(top_srcdir)/data/dbus-service.xml
	dbus-binding-tool --prefix=tpl_dbus_service --mode=glib-server $< > $@

dbus-service-client.h: $(top_srcdir)/data/dbus-service.xml
	dbus-binding-tool --prefix=tpl_dbus_service --mode=glib-client $< > $@

