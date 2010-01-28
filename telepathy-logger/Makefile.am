include $(top_srcdir)/tools/shave.mk
include $(top_srcdir)/tools/flymake.mk

AM_CPPFLAGS =						\
	$(ERROR_CFLAGS)					\
	-DG_LOG_DOMAIN=\"telepathy-logger\"		\
	$(LIBTPL_CFLAGS)				\
	$(DISABLE_DEPRECATED)				\
	$(WARN_CFLAGS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libtelepathy-logger.pc

lib_LTLIBRARIES = libtelepathy-logger.la

LIBTPLdir = $(includedir)/telepathy-logger
LIBTPL_HEADERS = \
		util.h				\
		contact.h			\
		log-entry.h			\
		log-entry-text.h		\
		log-manager.h			\
		datetime.h


libtelepathy_logger_la_SOURCES = \
		datetime.c			\
		channel.c			\
		channel-factory.c		\
		channel-text.c			\
		conf.c				\
		contact.c			\
		log-entry.c			\
		log-entry-text.c		\
		log-manager.c			\
		log-store.c			\
		log-store-empathy.c		\
		observer.c			\
		util.c

check_c_sources = \
	$(libtelepathy_logger_la_SOURCES)	\
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

CLEANFILES = $(BUILT_SOURCES)
