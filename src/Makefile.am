BUILT_SOURCES = \
    tp-stream-engine-glue.h \
    tp-stream-engine-signals-marshal.h \
    tp-stream-engine-signals-marshal.c

if USE_INFOPRINT
BUILT_SOURCES += statusbar-gen.h
endif

CLEANFILES = $(BUILT_SOURCES)

bin_PROGRAMS = telepathy-stream-engine

libstreamengine_convenience_la_SOURCES = \
	channel.h \
	channel.c \
	session.h \
	session.c \
	stream.h \
	stream.c \
	tp-stream-engine.h \
	tp-stream-engine.c \
	types.h \
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
	libstreamengine-convenience.la \
	$(top_builddir)/common/libcommon-convenience.la

noinst_LTLIBRARIES = libstreamengine-convenience.la

AM_CFLAGS = $(ERROR_CFLAGS) \
	@DBUS_CFLAGS@ @GLIB_CFLAGS@ @LIBTELEPATHY_CFLAGS@ @FARSIGHT_CFLAGS@ \
	@GST_PLUGINS_BASE_CFLAGS@ @X11_CFLAGS@ -I $(top_srcdir)

AM_LDFLAGS = @DBUS_LIBS@ @GLIB_LIBS@ @LIBTELEPATHY_LIBS@ @FARSIGHT_LIBS@ \
	@GST_PLUGINS_BASE_LIBS@ @X11_LIBS@ -lgstinterfaces-0.10


#Rule to generate the binding headers
%-gen.h: ../generate/xml-modified/%.xml Makefile.in
	dbus-binding-tool --prefix=$(subst -,_,$*) --mode=glib-client $< > $*-gen.h

%-glue.h: ../generate/xml-modified/%.xml Makefile.in
	dbus-binding-tool --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $<

%-signals-marshal.h: %-signals-marshal.list Makefile.in
	glib-genmarshal --header --prefix=$(subst -,_,$*)_marshal $< > $*-signals-marshal.h

%-signals-marshal.c: %-signals-marshal.list Makefile.in
	glib-genmarshal --body --prefix=$(subst -,_,$*)_marshal $< > $*-signals-marshal.c

%-marshal.h: %-marshal.list Makefile.in
	glib-genmarshal --header --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.h

%-marshal.c: %-marshal.list Makefile.in
	glib-genmarshal --body --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.c

