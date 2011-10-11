BUILT_SOURCES = \
    tf-signals-marshal.h \
    tf-signals-marshal.c

CLEANFILES = $(BUILT_SOURCES) tf-signals-marshal.list


tfincludedir = $(includedir)/telepathy-1.0/telepathy-farstream
tfinclude_HEADERS = \
	telepathy-farstream.h \
	channel.h \
	content.h

libtelepathy_farstream_la_SOURCES = \
	stream.c \
	stream.h \
	content.c \
	content.h \
	content-priv.h \
	session.c \
	channel.c \
	channel.h \
	stream-priv.h \
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
	telepathy-farstream.h \
	telepathy-farstream.c \
	utils.h

nodist_libtelepathy_farstream_la_SOURCES = $(BUILT_SOURCES)

lib_LTLIBRARIES = libtelepathy-farstream.la

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = telepathy-farstream.pc

AM_CFLAGS = \
	-DG_LOG_DOMAIN=\"tp-fs\" \
	$(ERROR_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GST_CFLAGS) \
	$(FARSTREAM_CFLAGS) \
	$(TELEPATHY_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir)

libtelepathy_farstream_la_LIBADD = \
	$(GLIB_LIBS) \
	$(DBUS_LIBS) \
	$(GST_LIBS) \
	$(FARSTREAM_LIBS) \
	$(TELEPATHY_LIBS) \
	../extensions/libfuture-extensions.la

libtelepathy_farstream_la_LDFLAGS =  -no-undefined \
	-export-symbols-regex "^tf_(init|content_|channel_).*" \
	-version-info "$(LT_CURRENT)":"$(LT_REVISION)":"$(LT_AGE)"


# rules to generate signal marshallers
tf-signals-marshal.list: $(libtelepathy_farstream_la_SOURCES) Makefile.am
	( cd $(srcdir) && \
	sed -n -e 's/.*_tf_marshal_\([[:upper:][:digit:]]*__[[:upper:][:digit:]_]*\).*/\1/p' \
	$(libtelepathy_farstream_la_SOURCES) ) \
	| sed -e 's/__/:/' -e 'y/_/,/' | sort -u > $@.tmp
	if cmp -s $@.tmp $@; then \
		rm $@.tmp; \
		touch $@; \
	else \
		mv $@.tmp $@; \
	fi

%-signals-marshal.h: %-signals-marshal.list Makefile.in
	glib-genmarshal --header --prefix=_$(subst -,_,$*)_marshal $< > $*-signals-marshal.h

%-signals-marshal.c: %-signals-marshal.list Makefile.in
	glib-genmarshal --body --prefix=_$(subst -,_,$*)_marshal $< > $*-signals-marshal.c
