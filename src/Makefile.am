BUILT_SOURCES = \
    tp-stream-engine-glue.h \
    tp-stream-engine-signals-marshal.h \
    tp-stream-engine-signals-marshal.c \
    media-engine-gen.h \
    misc-signals-marshal.h \
    misc-signals-marshal.c

if USE_INFOPRINT
BUILT_SOURCES += statusbar-gen.h
endif

# Correctly clean the generated headers, but keep the xml description
CLEANFILES = $(BUILT_SOURCES)

bin_PROGRAMS = telepathy-stream-engine

CORE_SOURCES = \
	$(BUILT_SOURCES)

#tp-media-session-handler.h 
#tp-media-stream-handler.c 

libstreamengine_convenience_la_SOURCES = \
	$(CORE_SOURCES)

telepathy_stream_engine_SOURCES = \
	channel.h \
	channel.c \
	session.h \
	session.c \
	stream.h \
	stream.c \
	tp-stream-engine.h \
	tp-stream-engine.c \
	types.h \
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

