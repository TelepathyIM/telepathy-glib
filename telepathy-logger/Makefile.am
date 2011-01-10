include $(top_srcdir)/tools/flymake.mk

AM_CPPFLAGS =						\
	-I$(top_builddir)				\
	-I$(top_builddir)/extensions			\
	-I$(top_srcdir)					\
	$(ERROR_CFLAGS)					\
	-DG_LOG_DOMAIN=\"tp-logger\"			\
	-DTPL_DATA_DIR=\"$(PACKAGE_NAME)\"		\
	$(TPL_CFLAGS)				\
	$(DISABLE_DEPRECATED)				\
	$(WARN_CFLAGS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = telepathy-logger-0.1.pc

lib_LTLIBRARIES = libtelepathy-logger.la

LIBTPLdir = $(includedir)/telepathy-logger-0.1/telepathy-logger
LIBTPL_HEADERS = \
		entity.h			\
		event.h				\
		event-text.h		\
		log-manager.h			\
		telepathy-logger.h \
		$(NULL)

%-marshal.c: %-marshal.list Makefile.am
	$(QUIET_GEN)echo "#include \"tpl-marshal.h\"" > $@
	$(QUIET_GEN)glib-genmarshal --body --prefix=tpl_marshal $< >> $@

%-marshal.h: %-marshal.list Makefile.am
	$(QUIET_GEN)glib-genmarshal --header --prefix=tpl_marshal $< > $@

BUILT_SOURCES = \
	tpl-marshal.c tpl-marshal.h \
	$(NULL)

libtelepathy_logger_la_SOURCES = \
		action-chain.c			\
		action-chain-internal.h		\
		channel-internal.h		\
		channel.c			\
		channel-factory.c		\
		channel-factory-internal.h	\
		channel-text.c			\
		channel-text-internal.h		\
		conf.c				\
		conf-internal.h			\
		entity.c			\
		entity-internal.h		\
		datetime.c			\
		datetime-internal.h		\
		dbus-service.c			\
		dbus-service-internal.h		\
		debug-internal.h		\
		debug.c				\
		event.c			\
		event-internal.h			\
		event-text.c		\
		event-text-internal.h		\
		log-manager.c			\
		log-manager-internal.h		\
		log-store.c			\
		log-store-internal.h		\
		log-store-xml.c			\
		log-store-xml-internal.h	\
		log-store-sqlite.c		\
		log-store-sqlite-internal.h	\
		log-store-pidgin.c		\
		log-store-pidgin-internal.h	\
		log-store-factory.c		\
		log-store-factory-internal.h	\
		observer.c			\
		observer-internal.h		\
		util-internal.h			\
		util.c				\
		$(NULL)

nodist_libtelepathy_logger_la_SOURCES = \
		$(BUILT_SOURCES)

EXTRA_DIST = \
	tpl-marshal.list \
	$(NULL)

libtelepathy_logger_la_LIBADD = \
	$(top_builddir)/extensions/libtpl-extensions-convenience.la

check_c_sources = \
	$(libtelepathy_logger_la_SOURCES)	\
	$(LIBTPL_HEADERS)
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

CLEANFILES = $(BUILT_SOURCES)

include abi.am
