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


# rules for making the glib enum objects
%-enumtypes.h: %.h Makefile
	glib-mkenums \
	--fhead "#ifndef __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__\n#define __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
	--fprod "/* enumerations from \"@filename@\" */\n" \
	--vhead "GType @enum_name@_get_type (void);\n#define $(shell echo $* | tr [:lower:]- [:upper:]_ | sed 's/_.*//')_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n"         \
	--ftail "G_END_DECLS\n\n#endif /* __$(shell echo $* | tr [:lower:]- [:upper:]_)_ENUM_TYPES_H__ */" \
	$< > $@

%-enumtypes.c: %.h Makefile
	glib-mkenums \
	--fhead "#include <$*.h>" \
	--fprod "\n/* enumerations from \"@filename@\" */" \
	--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {"     \
	--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@VALUENAME@\" }," \
	--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
	$< > $@
