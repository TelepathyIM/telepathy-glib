tools_dir = $(top_srcdir)/tools

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TELEPATHY_CFLAGS)

EXTRA_DIST = \
    all.xml \
    misc.xml \
    Channel_Handler.xml \
    Stream_Engine.xml

noinst_LTLIBRARIES = libstream-engine-api.la

libstream_engine_api_la_LIBADD = \
    $(TELEPATHY_LIBS)

# The client-specific parts are built into a separate .o file, so the linker
# can discard them when linking services. The service-specific parts are
# in svc-*.c, so we don't need an extensions-svc.c.
libstream_engine_api_la_SOURCES = \
    api.c \
    api-cli.c \
    api.h

nodist_libstream_engine_api_la_SOURCES = \
    _gen/signals-marshal.c \
    _gen/signals-marshal.h \
    _gen/signals-marshal.list \
    _gen/register-dbus-glib-marshallers-body.h \
    _gen/enums.h \
    _gen/gtypes.h \
    _gen/gtypes-body.h \
    _gen/interfaces.h \
    _gen/interfaces-body.h \
    _gen/cli-misc.h \
    _gen/cli-misc-body.h \
    _gen/svc-misc.h \
    _gen/svc-misc.c

BUILT_SOURCES = \
    _gen/all.xml \
    _gen/misc.xml \
    $(nodist_libstream_engine_api_la_SOURCES) \
    api.html

CLEANFILES = $(BUILT_SOURCES)

XSLTPROCFLAGS = --nonet --novalid

_gen/%.xml: %.xml $(wildcard *.xml) Makefile.am
	$(mkdir_p) _gen
	$(XSLTPROC) $(XSLTPROCFLAGS) --xinclude $(tools_dir)/identity.xsl \
		$< > $@

# Generated files which can be generated for all categories simultaneously

api.html: _gen/all.xml $(tools_dir)/doc-generator.xsl Makefile.am
	$(XSLTPROC) $(XSLTPROCFLAGS) \
		$(tools_dir)/doc-generator.xsl \
		$< > $@

_gen/gtypes.h _gen/gtypes-body.h: _gen/all.xml \
	$(top_srcdir)/tools/glib-gtypes-generator.py Makefile.am
	$(PYTHON) $(top_srcdir)/tools/glib-gtypes-generator.py \
		$< _gen/gtypes Stream_Engine

_gen/signals-marshal.list: _gen/all.xml \
	$(tools_dir)/glib-signals-marshal-gen.py Makefile.am
	$(PYTHON) $(tools_dir)/glib-signals-marshal-gen.py $< > $@

_gen/signals-marshal.h: _gen/signals-marshal.list Makefile.am
	$(GLIB_GENMARSHAL) --header --prefix=_se_api_marshal $< > $@

_gen/signals-marshal.c: _gen/signals-marshal.list Makefile.am
	{ echo '#include "_gen/signals-marshal.h"' && \
	$(GLIB_GENMARSHAL) --body --prefix=_se_api_marshal $< ; } > $@

_gen/register-dbus-glib-marshallers-body.h: _gen/all.xml \
	$(tools_dir)/glib-client-marshaller-gen.py Makefile.am
	$(PYTHON) $(tools_dir)/glib-client-marshaller-gen.py $< \
		_se_api > $@

_gen/enums.h: _gen/all.xml $(tools_dir)/c-constants-generator.xsl Makefile.am
	$(XSLTPROC) $(XSLTPROCFLAGS) \
		--stringparam mixed-case-prefix Stream_Engine \
		$(tools_dir)/c-constants-generator.xsl \
		$< > $@

_gen/interfaces.h: _gen/all.xml \
	$(tools_dir)/glib-interfaces-generator.xsl \
	$(tools_dir)/c-interfaces-generator.xsl Makefile.am
	$(XSLTPROC) $(XSLTPROCFLAGS) \
		--stringparam mixed-case-prefix Stream_Engine \
		$(tools_dir)/glib-interfaces-generator.xsl \
		$< > $@

_gen/interfaces-body.h: _gen/all.xml \
	$(tools_dir)/glib-interfaces-body-generator.xsl \
	$(tools_dir)/c-interfaces-generator.xsl Makefile.am
	$(XSLTPROC) $(XSLTPROCFLAGS) \
		--stringparam mixed-case-prefix Stream_Engine \
		$(tools_dir)/glib-interfaces-body-generator.xsl \
		$< > $@

# Generated files which must be generated per "category". Each TpProxy
# subclass you want to use with --subclass will need to have its own category,
# although you can subdivide further if you want.

_gen/svc-%.c _gen/svc-%.h: _gen/%.xml \
	$(tools_dir)/glib-ginterface-gen.py \
	Makefile.am
	$(PYTHON) $(tools_dir)/glib-ginterface-gen.py \
		--filename=_gen/svc-$* --signal-marshal-prefix=_se_api \
		--include='<telepathy-glib/dbus.h>' \
		--include='"_gen/signals-marshal.h"' \
		--not-implemented-func='tp_dbus_g_method_return_not_implemented' \
		$< Stream_Engine/Svc_

_gen/cli-misc-body.h _gen/cli-misc.h: _gen/misc.xml \
	$(tools_dir)/glib-client-gen.py Makefile.am
	$(PYTHON) $(tools_dir)/glib-client-gen.py \
		--group=misc \
		--iface-quark-prefix=STREAM_ENGINE_IFACE_QUARK \
		$< Stream_Engine_Cli _gen/cli-misc
