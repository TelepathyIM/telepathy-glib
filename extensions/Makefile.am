tools_dir = $(top_srcdir)/tools

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA =
if ENABLE_PUBLIC_EXTENSIONS
pkgconfig_DATA += tpl-extensions.pc
endif # ENABLE_PUBLIC_EXTENSIONS

$(pkgconfig_DATA): $(top_srcdir)/config.status

EXTRA_DIST = \
    all.xml \
    misc.xml \
    Logger.xml \
    $(pkgconfig_DATA:.pc=.pc.in) \
    $(NULL)

noinst_LTLIBRARIES = libtpl-extensions-convenience.la
libtpl_extensions_convenience_la_LIBADD = \
    @LIBTPL_LIBS@

gen_headers = \
    _gen/cli-misc.h \
    _gen/enums.h \
    _gen/gtypes.h \
    _gen/interfaces.h \
    _gen/svc-misc.h \
    $(NULL)

nodist_libtpl_extensions_convenience_la_SOURCES = \
    _gen/cli-misc-body.h \
    _gen/gtypes-body.h \
    _gen/interfaces-body.h \
    _gen/register-dbus-glib-marshallers-body.h \
    _gen/signals-marshal.c \
    _gen/signals-marshal.h \
    _gen/signals-marshal.list \
    _gen/svc-misc.c \
    $(gen_headers) \
    $(NULL)

extension_headers = \
    extensions.h \
    $(NULL)

libtpl_extensions_convenience_la_SOURCES = \
    extensions.c \
    extensions-cli.c \
    $(extension_headers) \
    $(NULL)

BUILT_SOURCES = \
    _gen/all.xml \
    _gen/misc.xml \
    $(nodist_libtpl_extensions_convenience_la_SOURCES) \
    $(gen_headers) \
    $(NULL)

CLEANFILES = \
    $(BUILT_SOURCES) \
    $(pkgconfig_DATA) \
    $(NULL)

if ENABLE_PUBLIC_EXTENSIONS
tplincludedir=$(includedir)/tpl-extensions
genincludedir=$(tplincludedir)/_gen

tplinclude_HEADERS = $(extension_headers)
geninclude_HEADERS = $(gen_headers)

# copy the core library and make it installable
lib_LTLIBRARIES = libtpl-extensions.la

libtpl_extensions_la_SOURCES =
libtpl_extensions_la_LIBADD = libtpl-extensions-convenience.la
libtpl_extensions_la_LDFLAGS = \
    -version-info "$(TPL_EXT_LT_CURRENT)":"$(TPL_EXT_LT_REVISION)":"$(TPL_EXT_LT_AGE)"
endif # ENABLE_PUBLIC_EXTENSIONS

AM_CFLAGS = $(ERROR_CFLAGS) @LIBTPL_CFLAGS@

# Generated stuff

DROP_NAMESPACE = sed -e 's@xmlns:tp="http://telepathy\.freedesktop\.org/wiki/DbusSpec.extensions-v0"@@g'
XSLTPROCFLAGS = --nonet --novalid

_gen/all.xml: all.xml $(wildcard *.xml) Makefile.am
	@$(mkdir_p) _gen
	$(AM_V_GEN)$(XSLTPROC) $(XSLTPROCFLAGS) --xinclude $(tools_dir)/identity.xsl \
		$< > $@

doc/index.html: _gen/all.xml $(tools_dir)/specparser.py $(tools_dir)/doc-generator.py Makefile.am
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/doc-generator.py all.xml doc/ \
	telepathy-spec org.freedesktop.Telepathy

_gen/misc.xml: misc.xml $(wildcard *.xml)
	@$(mkdir_p) _gen
	$(AM_V_GEN)$(XSLTPROC) $(XSLTPROCFLAGS) --xinclude $(tools_dir)/identity.xsl \
		$< > $@

_gen/svc-misc.c _gen/svc-misc.h: _gen/misc.xml $(tools_dir)/glib-ginterface-gen.py \
	Makefile.am
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-ginterface-gen.py \
		--filename=_gen/svc-misc --signal-marshal-prefix=_tpl_ext \
		--include='<telepathy-glib/dbus.h>' \
		--include='"_gen/signals-marshal.h"' \
		--allow-unstable \
		--not-implemented-func='tp_dbus_g_method_return_not_implemented' \
		$< Tpl_Svc_

_gen/cli-misc-body.h _gen/cli-misc.h: _gen/misc.xml $(tools_dir)/glib-client-gen.py \
	Makefile.am
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-client-gen.py \
		--group=misc \
		--subclass=TpProxy \
		--subclass-assert=TP_IS_PROXY \
		--iface-quark-prefix=TPL_IFACE_QUARK \
		--tp-proxy-api=0.10.0 \
		$< Tpl_Cli _gen/cli-misc

_gen/signals-marshal.list: _gen/all.xml \
	$(tools_dir)/glib-signals-marshal-gen.py \
	Makefile.am
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-signals-marshal-gen.py $< > $@

_gen/signals-marshal.h: _gen/signals-marshal.list Makefile.am
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --header --prefix=_tpl_ext_marshal $< > $@

_gen/signals-marshal.c: _gen/signals-marshal.list Makefile.am
	$(AM_V_GEN){ echo '#include "_gen/signals-marshal.h"' && \
	$(GLIB_GENMARSHAL) --body --prefix=_tpl_ext_marshal $< ; } > $@

_gen/register-dbus-glib-marshallers-body.h: _gen/all.xml \
	$(tools_dir)/glib-client-marshaller-gen.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-client-marshaller-gen.py $< \
		_tpl_ext > $@

_gen/enums.h: _gen/all.xml $(tools_dir)/c-constants-gen.py \
	Makefile.am
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/c-constants-gen.py Tpl $< > $@

_gen/interfaces.h _gen/interfaces-body.h: _gen/all.xml \
	$(tools_dir)/glib-interfaces-gen.py \
	Makefile.am
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-interfaces-gen.py \
		Tpl _gen/interfaces-body.h _gen/interfaces.h $<

_gen/gtypes.h _gen/gtypes-body.h: _gen/all.xml \
	$(tools_dir)/glib-gtypes-generator.py Makefile.am
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-gtypes-generator.py \
		$< _gen/gtypes Tpl
