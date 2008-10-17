BUILT_SOURCES = \
    tp-stream-engine-signals-marshal.h \
    tp-stream-engine-signals-marshal.c

CLEANFILES = $(BUILT_SOURCES) tp-stream-engine-signals-marshal.list


libexec_PROGRAMS = telepathy-stream-engine

telepathy_stream_engine_SOURCES = \
	stream-engine-main.c \
	tp-stream-engine.h \
	tp-stream-engine.c \
	util.h \
	util.c \
	audiostream.c \
	audiostream.h \
	videostream.c \
	videostream.h \
	videosink.c \
	videosink.h \
	videopreview.c \
	videopreview.h \
	tp-stream-engine-signals-marshal.list

nodist_telepathy_stream_engine_SOURCES = $(BUILT_SOURCES)

AM_CFLAGS = \
	$(ERROR_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(X11_CFLAGS) \
	$(GST_CFLAGS) \
	$(GST_INTERFACES_CFLAGS) \
	$(FARSIGHT2_CFLAGS) \
	$(TELEPATHY_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir)

AM_LDFLAGS = \
	$(GLIB_LIBS) \
	$(DBUS_LIBS) \
	$(X11_LIBS) \
	$(GST_LIBS) \
	$(GST_INTERFACES_LIBS) \
	$(TELEPATHY_LIBS) \
	$(FARSIGHT2_LIBS) \
	$(top_builddir)/api/libstream-engine-api.la \
	$(top_builddir)/telepathy-farsight/libtelepathy-farsight.la

# rules to generate signal marshallers
tp-stream-engine-signals-marshal.list: $(telepathy_stream_engine_SOURCES) Makefile.am
	( cd $(srcdir) && \
	sed -n -e 's/.*tp_stream_engine_marshal_\([[:upper:][:digit:]]*__[[:upper:][:digit:]_]*\).*/\1/p' \
	$(telepathy_stream_engine_SOURCES) ) \
	| sed -e 's/__/:/' -e 'y/_/,/' | sort -u > $@.tmp
	if cmp -s $@.tmp $@; then \
		rm $@.tmp; \
		touch $@; \
	else \
		mv $@.tmp $@; \
	fi

%-signals-marshal.h: %-signals-marshal.list Makefile.in
	glib-genmarshal --header --prefix=$(subst -,_,$*)_marshal $< > $*-signals-marshal.h

%-signals-marshal.c: %-signals-marshal.list Makefile.in
	glib-genmarshal --body --prefix=$(subst -,_,$*)_marshal $< > $*-signals-marshal.c
