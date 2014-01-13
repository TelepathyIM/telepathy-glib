tfincludedir = $(includedir)/telepathy-farstream-1/telepathy-farstream
tfinclude_HEADERS = \
	telepathy-farstream.h \
	channel.h \
	content.h

apisources = \
	content.c \
	channel.c

libtelepathy_farstream_1_la_SOURCES = \
	$(tfinclude_HEADERS) \
	$(apisources) \
	content-priv.h \
	channel-priv.h \
	call-channel.c \
	call-channel.h \
	call-content.h \
	call-content.c \
	call-stream.h \
	call-stream.c \
	call-priv.h \
	utils.h

nodist_libtelepathy_farstream_1_la_SOURCES = $(BUILT_SOURCES)

lib_LTLIBRARIES = libtelepathy-farstream-1.la

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = telepathy-farstream-1.pc

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(GLIB_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GST_CFLAGS) \
	$(FARSTREAM_CFLAGS) \
	$(NULL)

AM_CFLAGS = \
	-DG_LOG_DOMAIN=\"tp-fs\" \
	$(ERROR_CFLAGS)

libtelepathy_farstream_1_la_LIBADD = \
	$(top_builddir)/telepathy-glib/libtelepathy-glib-1.la \
	$(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
	$(top_builddir)/telepathy-glib/libtelepathy-glib-1-core.la \
	$(GLIB_LIBS) \
	$(DBUS_LIBS) \
	$(GST_LIBS) \
	$(FARSTREAM_LIBS) \
	$(NULL)

libtelepathy_farstream_1_la_LDFLAGS =  -no-undefined \
	-export-symbols-regex "^tf_(init|content_|channel_).*" \
	-version-info "$(TPFS_LT_CURRENT)":"$(TPFS_LT_REVISION)":"$(TPFS_LT_AGE)"

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
	--add-include-path=$(srcdir) \
	--add-include-path=$(top_builddir)/telepathy-glib \
	$(NULL)
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(srcdir) \
	--includedir=$(top_builddir)/telepathy-glib \
	$(NULL)

if HAVE_INTROSPECTION
introspection_sources = $(tfinclude_HEADERS) $(apisources)

TelepathyFarstream-1.gir: libtelepathy-farstream-1.la
TelepathyFarstream_1_gir_INCLUDES = \
	GObject-2.0 \
	Gst-1.0 \
	Farstream-0.2 \
	TelepathyGLib-1 \
	$(NULL)
TelepathyFarstream_1_gir_PACKAGES = \
	glib-2.0 \
	gobject-2.0 \
	gio-2.0 \
	telepathy-glib-1 \
	$(NULL)
TelepathyFarstream_1_gir_EXPORT_PACKAGES = \
	telepathy-farstream-1 \
	$(NULL)
TelepathyFarstream_1_gir_CFLAGS = $(AM_CPPFLAGS)
TelepathyFarstream_1_gir_LIBS = libtelepathy-farstream-1.la
TelepathyFarstream_1_gir_FILES = $(introspection_sources)
TelepathyFarstream_1_gir_SCANNERFLAGS = --identifier-prefix=tf_ --identifier-prefix=Tf
INTROSPECTION_GIRS += TelepathyFarstream-1.gir
INTROSPECTION_SCANNER_ENV = \
	PKG_CONFIG_PATH=$(builddir):$(top_builddir)/telepathy-glib:$${PKG_CONFIG_PATH}

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(gir_DATA) $(typelib_DATA)
endif

SUBDIRS = . examples
