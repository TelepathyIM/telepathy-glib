BUILT_SOURCES = \
    test-streamed-media-channel-glue.h \
    test-streamed-media-channel-signals-marshal.h \
    test-streamed-media-channel-signals-marshal.c \
    tp-media-session-handler-glue.h \
    tp-media-session-handler-signals-marshal.h \
    tp-media-session-handler-signals-marshal.c \
    tp-media-stream-handler-glue.h \
    tp-media-stream-handler-signals-marshal.h \
    tp-media-stream-handler-signals-marshal.c 

# correctly clean the generated source files
CLEANFILES = $(BUILT_SOURCES)

noinst_PROGRAMS=voip-engine-tester

CORE_SOURCES = \
    test-voip-engine.c \
    test-streamed-media-channel.c \
    tp-media-session-handler.c \
    tp-media-stream-handler.c \
    $(BUILT_SOURCES)

voip_engine_tester_SOURCES = \
    $(CORE_SOURCES)

voip_engine_tester_LDADD = \
    $(top_builddir)/common/libcommon-convenience.la

AM_CFLAGS = $(ERROR_CFLAGS) @DBUS_CFLAGS@ @GLIB_CFLAGS@
AM_LDFLAGS = @DBUS_LIBS@ @GLIB_LIBS@


# rule to generate the binding headers
%-glue.h: ../xml/%.xml Makefile
	dbus-binding-tool --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $<

%-signals-marshal.h: %-signals-marshal.list Makefile
	glib-genmarshal --header --prefix=$(subst -,_,$*)_marshal $< > $*-signals-marshal.h

%-signals-marshal.c: %-signals-marshal.list Makefile
	glib-genmarshal --body --prefix=$(subst -,_,$*)_marshal $< > $*-signals-marshal.c

%-marshal.h: %-marshal.list Makefile
	glib-genmarshal --header --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.h

%-marshal.c: %-marshal.list Makefile
	glib-genmarshal --body --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.c


