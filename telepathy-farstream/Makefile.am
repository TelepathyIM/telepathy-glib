tfincludedir = $(includedir)/telepathy-1.0/telepathy-farstream
tfinclude_HEADERS = \
	telepathy-farstream.h \
	channel.h \
	content.h

libtelepathy_farstream_la_SOURCES = \
	$(tfinclude_HEADERS)
	stream.c \
	stream.h \
	content.c \
	content-priv.h \
	session.c \
	channel.c \
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

AM_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-DG_LOG_DOMAIN=\"tp-fs\" \
	$(ERROR_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GST_CFLAGS) \
	$(FARSTREAM_CFLAGS) \
	$(TELEPATHY_CFLAGS)

libtelepathy_farstream_la_LIBADD = \
	$(GLIB_LIBS) \
	$(DBUS_LIBS) \
	$(GST_LIBS) \
	$(FARSTREAM_LIBS) \
	$(TELEPATHY_LIBS)

libtelepathy_farstream_la_LDFLAGS =  -no-undefined \
	-export-symbols-regex "^tf_(init|content_|channel_).*" \
	-version-info "$(LT_CURRENT)":"$(LT_REVISION)":"$(LT_AGE)"
