tpgincludedir=$(includedir)/telepathy-1.0/telepathy-glib
genincludedir=$(tpgincludedir)/_gen

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = telepathy-glib.pc

EXTRA_DIST = \
    channel-interfaces.xml \
    connection-interfaces.xml \
    connection-manager-interfaces.xml \
    extra-gtkdoc.h \
    generic-interfaces.xml \
    media-interfaces.xml \
    stable-interfaces.xml

lib_LTLIBRARIES = libtelepathy-glib.la
noinst_LTLIBRARIES = libtelepathy-glib-internal.la

# libtelepathy-glib is just a shared version of libtelepathy-glib-internal.
# The static version is necessary because one of the tests wants to use
# internal symbols (_tp_debug) which are made invisible by GNU ld.
libtelepathy_glib_la_LDFLAGS = \
    -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
    -export-symbols-regex '^tp_'
libtelepathy_glib_la_LIBADD = libtelepathy-glib-internal.la
libtelepathy_glib_la_SOURCES =

tpginclude_HEADERS = \
    base-connection.h \
    base-connection-manager.h \
    channel-iface.h \
    channel-factory-iface.h \
    dbus.h \
    defs.h \
    debug.h \
    debug-ansi.h \
    enums.h \
    errors.h \
    group-mixin.h \
    gtypes.h \
    handle.h \
    handle-repo.h \
    handle-repo-static.h \
    handle-repo-dynamic.h \
    heap.h \
    interfaces.h \
    intset.h \
    presence-mixin.h \
    properties-mixin.h \
    run.h \
    svc-channel.h \
    svc-connection.h \
    svc-connection-manager.h \
    svc-generic.h \
    svc-media-interfaces.h \
    svc-properties-interface.h \
    text-mixin.h \
    util.h

nodist_libtelepathy_glib_internal_la_SOURCES = \
    _gen/signals-marshal.c \
    _gen/signals-marshal.h \
    _gen/signals-marshal.list \
    _gen/telepathy-errors.c \
    _gen/tp-signals-marshal.list \
    _gen/interfaces-body.h \
    _gen/gtypes-body.h \
    _gen/register-dbus-glib-marshallers-body.h \
    _gen/tp-svc-channel-interfaces.c \
    _gen/tp-svc-connection-interfaces.c \
    _gen/tp-svc-connection-manager-interfaces.c \
    _gen/tp-svc-generic-interfaces.c \
    _gen/tp-svc-media-interfaces.c

nodist_geninclude_HEADERS = \
    _gen/telepathy-enums.h \
    _gen/telepathy-interfaces.h \
    _gen/telepathy-errors.h \
    _gen/gtypes.h \
    _gen/tp-svc-channel-interfaces.h \
    _gen/tp-svc-connection-interfaces.h \
    _gen/tp-svc-connection-manager-interfaces.h \
    _gen/tp-svc-generic-interfaces.h \
    _gen/tp-svc-media-interfaces.h

BUILT_SOURCES = \
    $(nodist_libtelepathy_glib_internal_la_SOURCES) \
    $(nodist_geninclude_HEADERS) \
    _gen/stable-spec.xml \
    _gen/glue-stamp \
    _gen/spec-stamp \
    _gen/stable-stamp \
    _gen/stable-interfaces

CLEANFILES = \
    $(BUILT_SOURCES)

distclean-local:
	rm -rf _gen

check_c_sources = \
    $(tpginclude_HEADERS) \
    $(libtelepathy_glib_la_SOURCES)
include $(top_srcdir)/tools/check-coding-style.mk

libtelepathy_glib_internal_la_LIBADD = $(ALL_LIBS)
libtelepathy_glib_internal_la_SOURCES = \
    base-connection.c \
    base-connection-manager.c \
    dbus.c \
    internal-dbus-glib.h \
    debug.c \
    interfaces.c \
    internal-debug.h \
    errors.c \
    group-mixin.c \
    gtypes.c \
    handle.c \
    internal-handle-repo.h \
    handle-repo.c \
    handle-repo-dynamic.c \
    handle-repo-static.c \
    handle-set.c \
    heap.c \
    intset.c \
    channel-iface.c \
    channel-factory-iface.c \
    presence-mixin.c \
    properties-mixin.c \
    run.c \
    signals-marshal.list \
    text-mixin.c \
    util.c

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    @DBUS_CFLAGS@ \
    @GLIB_CFLAGS@ \
    @HANDLE_LEAK_DEBUG_CFLAGS@ \
    @COVERAGE_CFLAGS@ \
    -I$(top_builddir) \
    -I$(top_srcdir)

ALL_LIBS = \
    @DBUS_LIBS@ \
    @GLIB_LIBS@

# Generated stuff

DROP_NAMESPACE = sed -e 's@xmlns:tp="http://telepathy\.freedesktop\.org/wiki/DbusSpec.extensions-v0"@@g'
XSLTPROCFLAGS = --nonet --novalid

# Bootstrapping

_gen/spec-stamp: $(wildcard $(top_srcdir)/spec/*.xml)
	$(mkdir_p) _gen
	touch $@

_gen/stable-stamp: $(wildcard *-interfaces.xml) _gen/spec-stamp
	touch $@

_gen/stable-interfaces: _gen/stable-spec.xml ../tools/ls-interfaces.xsl
	$(XSLTPROC) $(XSLTPROCFLAGS) $(srcdir)/../tools/ls-interfaces.xsl \
		$< > $@

_gen/stable-spec.xml: stable-interfaces.xml _gen/stable-stamp
	$(XSLTPROC) --xinclude $(XSLTPROCFLAGS) \
		$(srcdir)/../tools/identity.xsl \
		$< > $@

# Things generated from the whole spec at once

_gen/gtypes.h _gen/gtypes-body.h: _gen/stable-spec.xml \
	../tools/glib-gtypes-generator.py
	$(PYTHON) $(top_srcdir)/tools/glib-gtypes-generator.py \
		_gen/stable-spec.xml \
		_gen/gtypes Tp

_gen/telepathy-enums.h: _gen/stable-spec.xml \
	../tools/c-constants-generator.xsl
	$(XSLTPROC) --xinclude $(XSLTPROCFLAGS) \
		--stringparam mixed-case-prefix Tp \
		$(srcdir)/../tools/c-constants-generator.xsl \
		_gen/stable-spec.xml > $@

_gen/telepathy-interfaces.h: _gen/stable-spec.xml \
	../tools/glib-interfaces-generator.xsl \
	../tools/c-interfaces-generator.xsl
	$(XSLTPROC) --xinclude $(XSLTPROCFLAGS) \
		--stringparam mixed-case-prefix Tp \
		$(srcdir)/../tools/glib-interfaces-generator.xsl \
		_gen/stable-spec.xml > $@

_gen/interfaces-body.h: _gen/stable-spec.xml \
	../tools/glib-interfaces-body-generator.xsl \
	../tools/c-interfaces-generator.xsl
	$(XSLTPROC) --xinclude $(XSLTPROCFLAGS) \
		--stringparam mixed-case-prefix Tp \
		$(srcdir)/../tools/glib-interfaces-body-generator.xsl \
		_gen/stable-spec.xml > $@

_gen/telepathy-errors.c: _gen/stable-spec.xml \
	../tools/glib-errors-enum-body.xsl
	$(XSLTPROC) $(XSLTPROCFLAGS) \
		$(srcdir)/../tools/glib-errors-enum-body.xsl \
		$< > $@

_gen/telepathy-errors.h: _gen/stable-spec.xml \
	../tools/glib-errors-enum-header.xsl
	$(XSLTPROC) $(XSLTPROCFLAGS) \
		$(srcdir)/../tools/glib-errors-enum-header.xsl \
		$< > $@

_gen/register-dbus-glib-marshallers-body.h: _gen/stable-spec.xml \
	../tools/glib-client-marshaller-gen.py
	$(PYTHON) $(srcdir)/../tools/glib-client-marshaller-gen.py $< _tp > $@

_gen/tp-signals-marshal.list: ../tools/glib-signals-marshal-gen.py \
	_gen/stable-spec.xml
	$(PYTHON) $(srcdir)/../tools/glib-signals-marshal-gen.py \
		_gen/stable-spec.xml > $@

_gen/signals-marshal.list: signals-marshal.list _gen/tp-signals-marshal.list
	sort -u $^ > $@

_gen/signals-marshal.h: _gen/signals-marshal.list
	$(GLIB_GENMARSHAL) --header --prefix=_tp_marshal $< > $@

_gen/signals-marshal.c: _gen/signals-marshal.list
	$(GLIB_GENMARSHAL) --body --prefix=_tp_marshal $< > $@

_gen/glue-stamp: _gen/stable-interfaces \
	../tools/make-all-async.xsl \
	../tools/spec-to-introspect.xsl
	set -e && for x in `cat _gen/stable-interfaces`; do \
		$(XSLTPROC) $(XSLTPROCFLAGS) \
			$(srcdir)/../tools/spec-to-introspect.xsl \
			$(srcdir)/../spec/$$x.xml \
			> _gen/introspect-$$x.xml.tmp; \
		$(DROP_NAMESPACE) < _gen/introspect-$$x.xml.tmp \
			> _gen/introspect-$$x.xml; \
		$(XSLTPROC) $(XSLTPROCFLAGS) \
			$(srcdir)/../tools/make-all-async.xsl \
			_gen/introspect-$$x.xml \
			> _gen/async-$$x.xml; \
		$(DBUS_BINDING_TOOL) --mode=glib-server \
			--output=_gen/svc-$$x-glue.h \
			--prefix=tp_svc_`echo $$x | tr '[:upper:]' '[:lower:]'`\
			_gen/async-$$x.xml; \
		rm -f _gen/introspect-$$x.xml.tmp \
			_gen/introspect-$$x.xml \
			_gen/async-$$x.xml; \
	done; \
	touch $@

# Things generated per interface

_gen/%-interfaces.xml: %-interfaces.xml ../tools/identity.xsl _gen/spec-stamp
	$(XSLTPROC) --xinclude $(XSLTPROCFLAGS) \
		$(srcdir)/../tools/identity.xsl \
		$< > $@

_gen/tp-svc-%-interfaces.c _gen/tp-svc-%-interfaces.h: _gen/%-interfaces.xml \
	../tools/glib-ginterface-gen.py \
	../tools/ls-interfaces.xsl \
	Makefile.am \
	_gen/glue-stamp
	$(XSLTPROC) $(XSLTPROCFLAGS) $(srcdir)/../tools/ls-interfaces.xsl \
		$< > _gen/tp-svc-$*-interfaces.tmp
	flags= ; \
	for x in `cat _gen/tp-svc-$*-interfaces.tmp`; do \
		 flags="$$flags --include-end=\"_gen/svc-$$x-glue.h\""; \
	done; \
	$(PYTHON) $(srcdir)/../tools/glib-ginterface-gen.py \
		--filename=_gen/tp-svc-$*-interfaces \
		--signal-marshal-prefix=_tp \
		--include='<telepathy-glib/dbus.h>' \
		--include="\"_gen/signals-marshal.h\"" \
		$$flags \
		--not-implemented-func='tp_dbus_g_method_return_not_implemented' \
		$< Tp_Svc_
