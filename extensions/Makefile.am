tools_dir = $(top_srcdir)/tools

pkgconfigdir = $(libdir)/pkgconfig

pkgconfig_DATA =
if ENABLE_PUBLIC_EXTENSIONS
pkgconfig_DATA += tpl-extensions.pc
endif # ENABLE_PUBLIC_EXTENSIONS

$(pkgconfig_DATA): $(top_builddir)/config.status

EXTRA_DIST = \
    all.xml \
    misc.xml \
    Logger.xml \
    $(pkgconfig_DATA:.pc=.pc.in) \
    $(NULL)

noinst_LTLIBRARIES = libtpl-extensions-convenience.la
libtpl_extensions_convenience_la_LIBADD = \
    @TPL_LIBS@

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
    $(gen_headers:.h=-gtk-doc.h) \
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

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    @TPL_CFLAGS@ \
    -DTP_DISABLE_SINGLE_INCLUDE \
    $(NULL)

# Generated stuff

DROP_NAMESPACE = sed -e 's@xmlns:tp="http://telepathy\.freedesktop\.org/wiki/DbusSpec.extensions-v0"@@g'
XSLTPROCFLAGS = --nonet --novalid

# Generated files which can be generated for all categories simultaneously

_gen/all.xml: all.xml $(wildcard *.xml) $(tools_dir)/xincludator.py
	$(MKDIR_P) _gen
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/xincludator.py $< > $@

doc/index.html: _gen/all.xml $(tools_dir)/doc-generator.xsl
	$(AM_V_GEN)$(XSLTPROC) $(XSLTPROCFLAGS) \
		$(tools_dir)/doc-generator.xsl \
		$< > $@

# do nothing, output as a side-effect
_gen/gtypes.h: _gen/gtypes-body.h
	@:

_gen/gtypes-body.h: _gen/all.xml \
	$(top_srcdir)/tools/glib-gtypes-generator.py
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/tools/glib-gtypes-generator.py \
		$< _gen/gtypes Tpl

_gen/register-dbus-glib-marshallers-body.h: _gen/all.xml \
	$(tools_dir)/glib-client-marshaller-gen.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-client-marshaller-gen.py $< \
		_tpl_ext > $@

_gen/enums.h: _gen/all.xml \
	$(tools_dir)/c-constants-gen.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/c-constants-gen.py \
		Tpl \
		$< _gen/enums

# do nothing, output as a side-effect
_gen/interfaces-body.h: _gen/interfaces.h
	@:

_gen/interfaces.h: _gen/all.xml \
	$(tools_dir)/glib-interfaces-gen.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-interfaces-gen.py \
		Tpl _gen/interfaces-body.h _gen/interfaces.h $<

# Generated files which must be generated per "category". Each TpProxy
# subclass you want to use with --subclass will need to have its own category,
# although you can subdivide further if you want.

_gen/misc.xml: misc.xml $(wildcard *.xml) $(tools_dir)/xincludator.py
	$(MKDIR_P) _gen
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/xincludator.py $< > $@

# do nothing, output as a side-effect
_gen/cli-misc.h: _gen/cli-misc-body.h
	@:

_gen/cli-misc-body.h: _gen/misc.xml \
	$(tools_dir)/glib-client-gen.py Makefile.am
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-client-gen.py \
		--group=misc \
		--subclass=TpProxy \
		--subclass-assert=TP_IS_PROXY \
		--iface-quark-prefix=TPL_IFACE_QUARK \
		--tp-proxy-api=0.10.0 \
		$< Tpl_Cli _gen/cli-misc

# do nothing, output as a side-effect
_gen/svc-misc.h: _gen/svc-misc.c
	@:

_gen/svc-misc.c: _gen/misc.xml \
	$(tools_dir)/glib-ginterface-gen.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-ginterface-gen.py \
		--filename=_gen/svc-misc \
		--signal-marshal-prefix=_tpl_ext \
		--include='<telepathy-glib/telepathy-glib.h>' \
		--not-implemented-func='tp_dbus_g_method_return_not_implemented' \
		--allow-unstable \
		$< Tpl_Svc_
