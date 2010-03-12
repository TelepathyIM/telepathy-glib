include $(top_srcdir)/tools/shave.mk
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
		action-chain.h			\
		channel.h			\
		channel-factory.h		\
		channel-text.h			\
		conf.h				\
		contact.h			\
		datetime.h			\
		dbus-service.h			\
		debug.h				\
		log-entry.h			\
		log-entry-text.h		\
		log-manager.h			\
		log-manager-priv.h		\
		log-store.h			\
		log-store-xml.h			\
		log-store-sqlite.h		\
		log-store-factory.h		\
		observer.h			\
		util.h				\
		$(NULL)

libtelepathy_logger_la_SOURCES = \
		action-chain.c			\
		channel.c			\
		channel-factory.c		\
		channel-text.c			\
		conf.c				\
		contact.c			\
		datetime.c			\
		dbus-service.c			\
		debug.c				\
		log-entry.c			\
		log-entry-text.c		\
		log-manager.c			\
		log-store.c			\
		log-store-xml.c			\
		log-store-sqlite.c		\
		log-store-factory.c		\
		observer.c			\
		util.c				\
		$(NULL)

libtelepathy_logger_la_LIBADD = \
	$(top_builddir)/extensions/libtpl-extensions.la

check_c_sources = \
	$(libtelepathy_logger_la_SOURCES)	\
	$(LIBTPL_HEADERS)
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

CLEANFILES = $(BUILT_SOURCES)
