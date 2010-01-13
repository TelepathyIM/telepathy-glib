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
		observer.h			\
		channel.h			\
		contact.h			\
		log-entry.h			\
		log-entry-text.h		\
		log-manager.h			\
		log-manager-priv.h		\
		log-store-empathy.h		\
		log-store.h			\
		channel-text.h			\
		datetime.h			\
		utils.h


libtelepathy_logger_la_SOURCES = \
		observer.c			\
		channel.c			\
		channel-text.c			\
		log-entry.c			\
		log-entry-text.c		\
		contact.c			\
		log-manager.c			\
		log-store.c			\
		log-store-empathy.c		\
		utils.c				\
		datetime.c

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_DATA = telepathy-logger.schemas

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) \
			    --makefile-install-rule $(srcdir)/$(schema_DATA)

check_c_sources = \
	$(libtelepathy_logger_la_SOURCES)	\
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

CLEANFILES = $(BUILT_SOURCES)
