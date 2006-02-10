BUILT_SOURCES = \
    tp-voip-engine-glue.h \
    tp-voip-engine-signals-marshal.h \
    tp-voip-engine-signals-marshal.c \
    tp-media-session-handler-gen.h \
    tp-media-stream-handler-gen.h \
    tp-voip-engine-signals-marshal.h \
    tp-voip-engine-signals-marshal.c \
    misc-signals-marshal.h \
    misc-signals-marshal.c


# Correctly clean the generated headers, but keep the xml description
CLEANFILES = $(BUILT_SOURCES)

bin_PROGRAMS = telepathy-voip-engine

CORE_SOURCES = \
	$(BUILT_SOURCES)

#tp-media-session-handler.h 
#tp-media-stream-handler.c 

libvoipengine_convenience_la_SOURCES = \
	$(CORE_SOURCES)

telepathy_voip_engine_SOURCES = \
	tp-voip-engine.c \
	voip-engine-main.c

telepathy_voip_engine_LDADD = \
	libvoipengine-convenience.la \
	$(top_builddir)/common/libcommon-convenience.la

noinst_LTLIBRARIES = libvoipengine-convenience.la

AM_CFLAGS = $(ERROR_CFLAGS) \
	@DBUS_CFLAGS@ @GLIB_CFLAGS@ @LIBTELEPATHY_CFLAGS@ @FARSIGHT_CFLAGS@

AM_LDFLAGS = @DBUS_LIBS@ @GLIB_LIBS@ @LIBTELEPATHY_LIBS@ @FARSIGHT_LIBS@



#Rule to generate the binding headers
%-gen.h: ../generate/xml-modified/%.xml Makefile.am
	dbus-binding-tool --prefix=$(subst -,_,$*) --mode=glib-client $< > $*-gen.h

%-glue.h: ../generate/xml-modified/%.xml Makefile
	dbus-binding-tool --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $<

%-signals-marshal.h: %-signals-marshal.list Makefile
	glib-genmarshal --header --prefix=$(subst -,_,$*)_marshal $< > $*-signals-marshal.h

%-signals-marshal.c: %-signals-marshal.list Makefile
	glib-genmarshal --body --prefix=$(subst -,_,$*)_marshal $< > $*-signals-marshal.c

%-marshal.h: %-marshal.list Makefile
	glib-genmarshal --header --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.h

%-marshal.c: %-marshal.list Makefile
	glib-genmarshal --body --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.c

