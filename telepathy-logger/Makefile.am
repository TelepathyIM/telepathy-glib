include $(top_srcdir)/tools/flymake.mk

AM_CPPFLAGS =						\
	-I$(top_builddir)				\
	-I$(top_srcdir)					\
	$(ERROR_CFLAGS)					\
	-DG_LOG_DOMAIN=\"tp-logger\"			\
	-DTPL_DATA_DIR=\"$(PACKAGE_NAME)\"		\
	$(LIBTPL_CFLAGS)				\
	$(DISABLE_DEPRECATED)				\
	$(WARN_CFLAGS)

BUILT_SOURCES =

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = telepathy-logger.pc

lib_LTLIBRARIES = libtelepathy-logger.la

LIBTPLdir = $(includedir)/telepathy-logger
LIBTPL_HEADERS = \
		contact.h			\
		log-entry.h			\
		log-entry-text.h		\
		log-manager.h			\
		$(NULL)

libtelepathy_logger_la_SOURCES = \
		action-chain.c			\
		action-chain-internal.h		\
		channel.h			\
		channel.c			\
		channel-factory.c		\
		channel-factory.h		\
		channel-text.c			\
		channel-text.h			\
		conf.c				\
		conf.h				\
		contact.c			\
		datetime.c			\
		datetime-internal.h		\
		dbus-service.c			\
		dbus-service.h			\
		debug-internal.h		\
		debug.c				\
		log-entry.c			\
		log-entry-internal.h			\
		log-entry-text.c		\
		log-entry-text-internal.h		\
		log-manager.c			\
		log-manager-priv.h		\
		log-store.c			\
		log-store.h			\
		log-store-xml.c			\
		log-store-xml.h			\
		log-store-sqlite.c		\
		log-store-sqlite.h		\
		log-store-factory.c		\
		log-store-factory.h		\
		observer.c			\
		observer.h			\
		util-internal.h			\
		util.c				\
		$(NULL)

libtelepathy_logger_la_LIBADD = \
	$(top_builddir)/extensions/libtpl-extensions-convenience.la

check_c_sources = \
	$(libtelepathy_logger_la_SOURCES)	\
	$(LIBTPL_HEADERS)
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

CLEANFILES = $(BUILT_SOURCES)
