tools_dir = $(top_srcdir)/tools

EXTRA_DIST = \
    all.xml \
    Channel_Handler.xml \
    Stream_Engine.xml

noinst_LTLIBRARIES = libstream-engine-api.la

libstream_engine_api_la_SOURCES = \
    api.h

nodist_libstream_engine_api_la_SOURCES = \
    _gen/signals-marshal.c \
    _gen/signals-marshal.h \
    _gen/signals-marshal.list \
    _gen/svc.h \
    _gen/svc.c

BUILT_SOURCES = \
    _gen/all.xml \
    $(nodist_libstream_engine_api_la_SOURCES)

CLEANFILES = $(BUILT_SOURCES)

AM_CFLAGS = $(ERROR_CFLAGS) @DBUS_CFLAGS@ @GLIB_CFLAGS@ @LIBTELEPATHY_CFLAGS@
AM_LDFLAGS = @DBUS_LIBS@ @GLIB_LIBS@ @LIBTELEPATHY_LIBS@

DROP_NAMESPACE = sed -e 's@xmlns:tp="http://telepathy\.freedesktop\.org/wiki/DbusSpec.extensions-v0"@@g'
XSLTPROCFLAGS = --nonet --novalid

_gen/all.xml: all.xml $(wildcard *.xml)
	$(mkdir_p) _gen
	$(XSLTPROC) $(XSLTPROCFLAGS) --xinclude $(tools_dir)/identity.xsl \
		$< > $@

_gen/svc.c _gen/svc.h: _gen/all.xml \
	$(tools_dir)/glib-ginterface-gen.py \
	Makefile.am
	$(PYTHON) $(tools_dir)/glib-ginterface-gen.py \
		--filename=_gen/svc --signal-marshal-prefix=_se_api \
		--include='<telepathy-glib/dbus.h>' \
		--include='"_gen/signals-marshal.h"' \
		--not-implemented-func='tp_dbus_g_method_return_not_implemented' \
		$< Stream_Engine/Svc_

_gen/signals-marshal.list: _gen/all.xml $(tools_dir)/glib-signals-marshal-gen.py
	$(PYTHON) $(tools_dir)/glib-signals-marshal-gen.py $< > $@

_gen/signals-marshal.h: _gen/signals-marshal.list
	$(GLIB_GENMARSHAL) --header --prefix=_se_api_marshal $< > $@

_gen/signals-marshal.c: _gen/signals-marshal.list
	$(GLIB_GENMARSHAL) --body --prefix=_se_api_marshal $< > $@
