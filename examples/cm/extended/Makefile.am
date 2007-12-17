# This first part is much the same as for the Echo connection manager.
# Typically this would be in the src/ directory.

noinst_PROGRAMS = telepathy-example-cm-extended

telepathy_example_cm_extended_SOURCES = \
    conn.c \
    conn.h \
    main.c \
    manager.c \
    manager.h

telepathy_example_cm_extended_LDADD = \
    libexample-extensions.la \
    $(TP_GLIB_LIBS)

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)

# Below this point is auto-generation machinery for the extensions.
# Typically this would be in an extensions/ directory.

tools_dir = $(top_srcdir)/tools

EXTRA_DIST = \
    extensions.xml \
    Connection_Interface_Hats.xml

noinst_LTLIBRARIES = libexample-extensions.la

libexample_extensions_la_LIBADD = \
    $(TP_GLIB_LIBS)

libexample_extensions_la_SOURCES = \
    extensions.c \
    extensions.h

nodist_libexample_extensions_la_SOURCES = \
    _gen/signals-marshal.c \
    _gen/signals-marshal.h \
    _gen/signals-marshal.list \
    _gen/enums.h \
    _gen/gtypes.h \
    _gen/gtypes-body.h \
    _gen/interfaces.h \
    _gen/interfaces-body.h \
    _gen/svc.h \
    _gen/svc.c

BUILT_SOURCES = \
    _gen/extensions.xml \
    $(nodist_libexample_extensions_la_SOURCES) \
    extensions.html

CLEANFILES = $(BUILT_SOURCES)

DROP_NAMESPACE = sed -e 's@xmlns:tp="http://telepathy\.freedesktop\.org/wiki/DbusSpec.extensions-v0"@@g'
XSLTPROCFLAGS = --nonet --novalid

_gen/extensions.xml: extensions.xml $(wildcard *.xml)
	$(mkdir_p) _gen
	$(XSLTPROC) $(XSLTPROCFLAGS) --xinclude $(tools_dir)/identity.xsl \
		$< > $@

extensions.html: _gen/extensions.xml $(tools_dir)/doc-generator.xsl
	$(XSLTPROC) $(XSLTPROCFLAGS) \
		$(tools_dir)/doc-generator.xsl \
		$< > $@

_gen/gtypes.h _gen/gtypes-body.h: _gen/extensions.xml \
	$(top_srcdir)/tools/glib-gtypes-generator.py
	$(PYTHON) $(top_srcdir)/tools/glib-gtypes-generator.py \
		$< _gen/gtypes Example

_gen/svc.c _gen/svc.h: _gen/extensions.xml \
	$(tools_dir)/glib-ginterface-gen.py
	$(PYTHON) $(tools_dir)/glib-ginterface-gen.py \
		--filename=_gen/svc --signal-marshal-prefix=_example_ext \
		--include='<telepathy-glib/dbus.h>' \
		--include='"_gen/signals-marshal.h"' \
		--not-implemented-func='tp_dbus_g_method_return_not_implemented' \
		$< Example_Svc_

_gen/signals-marshal.list: _gen/extensions.xml \
	$(tools_dir)/glib-signals-marshal-gen.py
	$(PYTHON) $(tools_dir)/glib-signals-marshal-gen.py $< > $@

_gen/signals-marshal.h: _gen/signals-marshal.list
	$(GLIB_GENMARSHAL) --header --prefix=_example_ext_marshal $< > $@

_gen/signals-marshal.c: _gen/signals-marshal.list
	$(GLIB_GENMARSHAL) --body --prefix=_example_ext_marshal $< > $@

_gen/enums.h: _gen/extensions.xml $(tools_dir)/c-constants-generator.xsl
	$(XSLTPROC) $(XSLTPROCFLAGS) \
		--stringparam mixed-case-prefix Example \
		$(tools_dir)/c-constants-generator.xsl \
		$< > $@

_gen/interfaces.h: _gen/extensions.xml \
	$(tools_dir)/glib-interfaces-generator.xsl \
	$(tools_dir)/c-interfaces-generator.xsl
	$(XSLTPROC) $(XSLTPROCFLAGS) \
		--stringparam mixed-case-prefix Example \
		$(tools_dir)/glib-interfaces-generator.xsl \
		$< > $@

_gen/interfaces-body.h: _gen/extensions.xml \
	$(tools_dir)/glib-interfaces-body-generator.xsl \
	$(tools_dir)/c-interfaces-generator.xsl
	$(XSLTPROC) $(XSLTPROCFLAGS) \
		--stringparam mixed-case-prefix Example \
		$(tools_dir)/glib-interfaces-body-generator.xsl \
		$< > $@
