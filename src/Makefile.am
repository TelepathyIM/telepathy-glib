EXTRA_DIST = \
    tp-stream-engine.xml

BUILT_SOURCES = \
    telepathy-errors-enum-types.h \
    telepathy-errors-enum-types.c \
    tp-stream-engine-glue.h \
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
	telepathy-errors.h \
	telepathy-errors.c \
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
	libstreamengine-convenience.la

noinst_LTLIBRARIES = libstreamengine-convenience.la

AM_CFLAGS = $(ERROR_CFLAGS) \
	@DBUS_CFLAGS@ @GLIB_CFLAGS@ @LIBTELEPATHY_CFLAGS@ @FARSIGHT_CFLAGS@ \
	@GST_PLUGINS_BASE_CFLAGS@ @X11_CFLAGS@ -I $(top_srcdir)

AM_LDFLAGS = @DBUS_LIBS@ @GLIB_LIBS@ @LIBTELEPATHY_LIBS@ @FARSIGHT_LIBS@ \
	@GST_PLUGINS_BASE_LIBS@ @X11_LIBS@ -lgstinterfaces-0.10


#Rule to generate the binding headers
%-glue.h: %.xml Makefile.in
	dbus-binding-tool --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $<

%-signals-marshal.h: %-signals-marshal.list Makefile.in
	glib-genmarshal --header --prefix=$(subst -,_,$*)_marshal $< > $*-signals-marshal.h

%-signals-marshal.c: %-signals-marshal.list Makefile.in
	glib-genmarshal --body --prefix=$(subst -,_,$*)_marshal $< > $*-signals-marshal.c

%-marshal.h: %-marshal.list Makefile.in
	glib-genmarshal --header --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.h

%-marshal.c: %-marshal.list Makefile.in
	glib-genmarshal --body --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.c

# rules for making the glib enum types
%-enum-types.h: %.h Makefile.in
	glib-mkenums \
	--fhead "#ifndef __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__\n#define __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
	--fprod "/* enumerations from \"@filename@\" */\n" \
	--vhead "GType @enum_name@_get_type (void);\n#define $(shell echo $* | tr [:lower:]- [:upper:]_ | sed 's/_.*//')_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n"         \
	--ftail "G_END_DECLS\n\n#endif /* __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__ */" \
	$< > $@

%-enum-types.c: %.h Makefile.in
	glib-mkenums \
	--fhead "#include <$*.h>" \
	--fprod "\n/* enumerations from \"@filename@\" */" \
	--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {"     \
	--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@VALUENAME@\" }," \
	--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
	$< > $@
