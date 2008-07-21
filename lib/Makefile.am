BUILT_SOURCES = \
    tp-stream-engine-signals-marshal.h \
    tp-stream-engine-signals-marshal.c

CLEANFILES = $(BUILT_SOURCES)

libstreamengine_la_SOURCES = 

nodist_libstreamengine_la_SOURCES = $(BUILT_SOURCES)

lib_LTLIBRARIES = libstreamengine.la

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

libstreamengine_la_LIBADD = \
	$(GLIB_LIBS) \
	$(DBUS_LIBS) \
	$(X11_LIBS) \
	$(GST_INTERFACES_LIB) \
	$(GST_LIBS) \
	$(FARSIGHT2_LIBS) \
	$(TELEPATHY_LIBS)

# rules to generate signal marshallers
%-signals-marshal.h: %-signals-marshal.list Makefile.in
	glib-genmarshal --header --prefix=$(subst -,_,$*)_marshal $< > $*-signals-marshal.h

%-signals-marshal.c: %-signals-marshal.list Makefile.in
	glib-genmarshal --body --prefix=$(subst -,_,$*)_marshal $< > $*-signals-marshal.c
