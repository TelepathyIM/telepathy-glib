BUILT_SOURCES = \
    tp-stream-engine-signals-marshal.h \
    tp-stream-engine-signals-marshal.c

CLEANFILES = $(BUILT_SOURCES)

libexec_PROGRAMS = telepathy-stream-engine

libstreamengine_convenience_la_SOURCES = \
	channel.h \
	channel.c \
	session.h \
	session.c \
	stream.h \
	stream.c \
	tp-stream-engine.h \
	tp-stream-engine.c \
	util.h \
	util.c \
	xerrorhandler.h \
	xerrorhandler.c \
	tp-stream-engine-signals-marshal.list

nodist_libstreamengine_convenience_la_SOURCES = \
	$(BUILT_SOURCES)

telepathy_stream_engine_SOURCES = \
	stream-engine-main.c

telepathy_stream_engine_LDADD = \
	libstreamengine-convenience.la

noinst_LTLIBRARIES = libstreamengine-convenience.la

AM_CFLAGS = $(ERROR_CFLAGS) \
	@DBUS_CFLAGS@ @GLIB_CFLAGS@ @LIBTELEPATHY_CFLAGS@ @FARSIGHT_CFLAGS@ \
	@GST_PLUGINS_BASE_CFLAGS@ @X11_CFLAGS@ \
	-I$(top_srcdir) -I$(top_builddir)

AM_LDFLAGS = @DBUS_LIBS@ @GLIB_LIBS@ @LIBTELEPATHY_LIBS@ @FARSIGHT_LIBS@ \
	@GST_PLUGINS_BASE_LIBS@ @X11_LIBS@ -lgstinterfaces-0.10 \
	$(top_builddir)/api/libstream-engine-api.la

# rules to generate signal marshallers
%-signals-marshal.h: %-signals-marshal.list Makefile.in
	glib-genmarshal --header --prefix=$(subst -,_,$*)_marshal $< > $*-signals-marshal.h

%-signals-marshal.c: %-signals-marshal.list Makefile.in
	glib-genmarshal --body --prefix=$(subst -,_,$*)_marshal $< > $*-signals-marshal.c
