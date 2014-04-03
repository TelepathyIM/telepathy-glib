include $(top_srcdir)/tools/flymake.mk

if HAVE_INTROSPECTION
include $(INTROSPECTION_MAKEFILE)
endif
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
	--add-include-path=$(srcdir) \
	--add-include-path=$(top_builddir)/telepathy-glib \
	--add-include-path=$(top_srcdir) \
	-I$(top_srcdir) \
	$(NULL)
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(srcdir) \
	--includedir=$(top_builddir)/telepathy-glib \
	--includedir=$(top_srcdir) \
	$(NULL)

AM_CPPFLAGS =						\
	-I$(top_builddir)				\
	-I$(top_srcdir)					\
	$(ERROR_CFLAGS)					\
	-DG_LOG_DOMAIN=\"tp-logger\"			\
	-DTPL_DATA_DIR=\"$(PACKAGE_NAME)\"		\
	-DTP_DISABLE_SINGLE_INCLUDE			\
	$(TPL_CFLAGS)					\
	$(DISABLE_DEPRECATED)				\
	$(WARN_CFLAGS)

libexec_PROGRAMS = \
	telepathy-logger-1

telepathy_logger_1_SOURCES = telepathy-logger.c

telepathy_logger_1_LDADD = \
	libtelepathy-logger-1.la \
	$(top_builddir)/telepathy-glib/libtelepathy-glib-1.la \
	$(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
	$(TPL_LIBS) \
	$(NULL)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = telepathy-logger-1.pc

lib_LTLIBRARIES = libtelepathy-logger-1.la
noinst_LTLIBRARIES = libtelepathy-logger-internal.la

EXTRA_DIST = \
	tpl-marshal.list \
	$(ABI_LISTS) \
	$(NULL)

LIBTPLdir = $(includedir)/telepathy-logger-1/telepathy-logger
LIBTPL_HEADERS = \
		debug.h				\
		entity.h			\
		event.h				\
		log-manager.h			\
		log-walker.h			\
		telepathy-logger.h 		\
		text-event.h			\
		call-event.h			\
		$(NULL)

%-marshal.c: %-marshal.list Makefile.am
	$(AM_V_GEN)echo "#include \"tpl-marshal.h\"" > $@
	$(AM_V_GEN)glib-genmarshal --body --prefix=tpl_marshal $< >> $@

%-marshal.h: %-marshal.list Makefile.am
	$(AM_V_GEN)glib-genmarshal --header --prefix=tpl_marshal $< > $@

BUILT_SOURCES = \
	tpl-marshal.c tpl-marshal.h \
	$(NULL)

libtelepathy_logger_internal_la_SOURCES = \
		action-chain.c			\
		action-chain-internal.h		\
		call-event.c                    \
		call-event-internal.h           \
		client-factory.c		\
		client-factory-internal.h	\
		conf.c				\
		conf-internal.h			\
		entity.c			\
		entity-internal.h		\
		dbus-service.c			\
		dbus-service-internal.h		\
		debug-internal.h		\
		debug.c				\
		event.c				\
		event-internal.h		\
		log-iter.c			\
		log-iter-internal.h		\
		log-iter-pidgin.c		\
		log-iter-pidgin-internal.h	\
		log-iter-xml.c			\
		log-iter-xml-internal.h		\
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
		log-walker.c			\
		log-walker-internal.h		\
		observer.c			\
		observer-internal.h		\
		text-channel.c			\
		text-channel-internal.h		\
		text-event.c			\
		text-event-internal.h		\
		util-internal.h			\
		util.c				\
		call-channel.c                  \
		call-channel-internal.h         \
		$(NULL)

nodist_libtelepathy_logger_internal_la_SOURCES = \
		$(BUILT_SOURCES)

libtelepathy_logger_internal_la_LIBADD = \
	$(top_builddir)/telepathy-glib/libtelepathy-glib-1.la \
	$(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
	$(TPL_LIBS) \
	$(NULL)

libtelepathy_logger_1_la_SOURCES=
libtelepathy_logger_1_la_LIBADD = libtelepathy-logger-internal.la

check_c_sources = \
	$(libtelepathy_logger_internal_la_SOURCES)	\
	$(telepathy_logger_SOURCES)		\
	$(LIBTPL_HEADERS)
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

CLEANFILES = $(BUILT_SOURCES)

if HAVE_INTROSPECTION
introspection_sources = \
	$(libtelepathy_logger_internal_la_SOURCES) \
	$(LIBTPL_HEADERS) \
	$(NULL)

TelepathyLogger-1.gir: libtelepathy-logger-1.la
TelepathyLogger_1_gir_SCANNERFLAGS = --identifier-prefix=Tpl --warn-all
TelepathyLogger_1_gir_INCLUDES = GObject-2.0 TelepathyGLib-1
TelepathyLogger_1_gir_CFLAGS = $(INCLUDES)
TelepathyLogger_1_gir_LIBS = libtelepathy-logger-1.la
TelepathyLogger_1_gir_FILES = $(filter-out %-internal.h, $(introspection_sources))
TelepathyLogger_1_gir_PACKAGES = \
	glib-2.0 \
	gobject-2.0 \
	gio-2.0 \
	telepathy-glib-1 \
	$(NULL)
TelepathyLogger_1_gir_EXPORT_PACKAGES = \
	telepathy-logger-1 \
	$(NULL)

INTROSPECTION_GIRS += TelepathyLogger-1.gir

INTROSPECTION_SCANNER_ENV = \
	PKG_CONFIG_PATH=$(builddir):$(top_builddir)/telepathy-glib:$${PKG_CONFIG_PATH}

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0/
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += \
	$(gir_DATA) \
	$(typelib_DATA)

endif

include abi.am
