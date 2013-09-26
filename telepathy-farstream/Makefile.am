tfincludedir = $(includedir)/telepathy-1.0/telepathy-farstream
tfinclude_HEADERS = \
	telepathy-farstream.h \
	channel.h \
	content.h

apisources = \
	content.c \
	channel.c

libtelepathy_farstream_la_SOURCES = \
	$(tfinclude_HEADERS) \
	$(apisources) \
	stream.c \
	stream.h \
	content-priv.h \
	session.c \
	session-priv.h \
	channel-priv.h \
	media-signalling-channel.c \
	media-signalling-channel.h \
	media-signalling-content.c \
	media-signalling-content.h \
	call-channel.c \
	call-channel.h \
	call-content.h \
	call-content.c \
	call-stream.h \
	call-stream.c \
	call-priv.h \
	utils.h

nodist_libtelepathy_farstream_la_SOURCES = $(BUILT_SOURCES)

lib_LTLIBRARIES = libtelepathy-farstream.la

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = telepathy-farstream.pc

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(GLIB_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GST_CFLAGS) \
	$(FARSTREAM_CFLAGS) \
	$(TELEPATHY_CFLAGS)

AM_CFLAGS = \
	-DG_LOG_DOMAIN=\"tp-fs\" \
	$(ERROR_CFLAGS)

libtelepathy_farstream_la_LIBADD = \
	$(GLIB_LIBS) \
	$(DBUS_LIBS) \
	$(GST_LIBS) \
	$(FARSTREAM_LIBS) \
	$(TELEPATHY_LIBS)

libtelepathy_farstream_la_LDFLAGS =  -no-undefined \
	-export-symbols-regex "^tf_(init|content_|channel_).*" \
	-version-info "$(LT_CURRENT)":"$(LT_REVISION)":"$(LT_AGE)"

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
introspection_sources = $(tfinclude_HEADERS) $(apisources)

TelepathyFarstream-0.6.gir: libtelepathy-farstream.la
TelepathyFarstream_0_6_gir_INCLUDES = \
	GObject-2.0 \
	Gst-1.0 \
	Farstream-0.2 \
	TelepathyGLib-0.12
TelepathyFarstream_0_6_gir_CFLAGS = $(AM_CPPFLAGS)
TelepathyFarstream_0_6_gir_LIBS = libtelepathy-farstream.la
TelepathyFarstream_0_6_gir_FILES = $(introspection_sources)
TelepathyFarstream_0_6_gir_SCANNERFLAGS = --identifier-prefix=tf_ --identifier-prefix=Tf
INTROSPECTION_GIRS += TelepathyFarstream-0.6.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES = $(gir_DATA) $(typelib_DATA)
endif
