tpgincludedir=$(includedir)/telepathy-1.0/telepathy-glib
genincludedir=$(tpgincludedir)/_gen

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = telepathy-glib.pc

EXTRA_DIST = \
    channel.xml \
    connection.xml \
    connection-manager.xml \
    dbus-daemon.xml \
    dbus-introspectable.xml \
    dbus-peer.xml \
    dbus-properties.xml \
    extra-gtkdoc.h \
    generic.xml \
    media-session-handler.xml \
    media-stream-handler.xml \
    stable-interfaces.xml

lib_LTLIBRARIES = libtelepathy-glib.la
noinst_LTLIBRARIES = libtelepathy-glib-internal.la

# libtelepathy-glib is just a shared version of libtelepathy-glib-internal.
# The static version is necessary because one of the tests wants to use
# internal symbols (_tp_debug) which are made invisible by GNU ld.

# The quoting here is unnecessary but harmless, and has the useful side-effect
# that vim quickfix mode (:make) doesn't interpret the libtool --mode=link
# command as an error message in a bizarrely named file
libtelepathy_glib_la_LDFLAGS = \
    -version-info "$(LT_CURRENT)":"$(LT_REVISION)":"$(LT_AGE)"
libtelepathy_glib_la_LIBADD = libtelepathy-glib-internal.la
libtelepathy_glib_la_SOURCES =
libtelepathy_glib_la_DEPENDENCIES =

if HAVE_LD_VERSION_SCRIPT
libtelepathy_glib_la_SOURCES += version-script.txt
libtelepathy_glib_la_DEPENDENCIES += version-script.txt _gen/abi.txt

_gen/abi.txt: libtelepathy-glib-internal.la _gen/versioned-abi.txt Makefile.am
	$(NM) .libs/libtelepathy-glib-internal.a > _gen/abi.nm
	grep " T " < _gen/abi.nm > _gen/abi.funcs
	cut -d" " -f3 < _gen/abi.funcs > _gen/abi.funcnames
	grep "^tp" < _gen/abi.funcnames > _gen/abi.tpfuncnames
	sort -u < _gen/abi.tpfuncnames > $@
	diff -c $@ _gen/versioned-abi.txt

_gen/versioned-abi.txt: version-script.txt Makefile.am
	sed -n -e "s/^[	 ]*\\(tp_.*\\);/\\1/p" < $< > $@.tmp
	sort -u < $@.tmp > $@

libtelepathy_glib_la_LDFLAGS += \
    $(VERSION_SCRIPT_ARG)=$(srcdir)/version-script.txt

else # !HAVE_LD_VERSION_SCRIPT

libtelepathy_glib_la_LDFLAGS += -export-symbols-regex '^tp'

endif # !HAVE_LD_VERSION_SCRIPT

tpginclude_HEADERS = \
    base-connection.h \
    base-connection-manager.h \
    channel.h \
    channel-iface.h \
    channel-factory-iface.h \
    media-interfaces.h \
    connection.h \
    connection-manager.h \
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
    media-interfaces.h \
    presence-mixin.h \
    properties-mixin.h \
    proxy.h \
    proxy-subclass.h \
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
    _gen/tp-cli-channel-body.h \
    _gen/tp-cli-connection-body.h \
    _gen/tp-cli-connection-manager-body.h \
    _gen/tp-cli-dbus-daemon-body.h \
    _gen/tp-cli-generic-body.h \
    _gen/tp-cli-media-session-handler-body.h \
    _gen/tp-cli-media-stream-handler-body.h \
    _gen/tp-svc-channel.c \
    _gen/tp-svc-connection.c \
    _gen/tp-svc-connection-manager.c \
    _gen/tp-svc-generic.c \
    _gen/tp-svc-media-session-handler.c \
    _gen/tp-svc-media-stream-handler.c

nodist_geninclude_HEADERS = \
    _gen/telepathy-enums.h \
    _gen/telepathy-interfaces.h \
    _gen/telepathy-errors.h \
    _gen/gtypes.h \
    _gen/tp-cli-channel.h \
    _gen/tp-cli-connection.h \
    _gen/tp-cli-connection-manager.h \
    _gen/tp-cli-dbus-daemon.h \
    _gen/tp-cli-generic.h \
    _gen/tp-cli-media-session-handler.h \
    _gen/tp-cli-media-stream-handler.h \
    _gen/tp-svc-channel.h \
    _gen/tp-svc-connection.h \
    _gen/tp-svc-connection-manager.h \
    _gen/tp-svc-generic.h \
    _gen/tp-svc-media-session-handler.h \
    _gen/tp-svc-media-stream-handler.h

BUILT_SOURCES = \
    $(nodist_libtelepathy_glib_internal_la_SOURCES) \
    $(nodist_geninclude_HEADERS) \
    _gen/stable-spec.xml \
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
    channel.c \
    connection.c \
    connection-manager.c \
    dbus.c \
    dbus-internal.h \
    debug.c \
    interfaces.c \
    debug-internal.h \
    errors.c \
    group-mixin.c \
    gtypes.c \
    handle.c \
    handle-repo.c \
    handle-repo-dynamic.c \
    handle-repo-internal.h \
    handle-repo-static.c \
    handle-set.c \
    heap.c \
    intset.c \
    channel-iface.c \
    channel-factory-iface.c \
    media-interfaces.c \
    presence-mixin.c \
    properties-mixin.c \
    proxy.c \
    run.c \
    signals-marshal.list \
    text-mixin.c \
    util.c

AM_CFLAGS = \
    -DG_LOG_DOMAIN=\"tp-glib\" \
    $(ERROR_CFLAGS) \
    @DBUS_CFLAGS@ \
    @GLIB_CFLAGS@ \
    @HANDLE_LEAK_DEBUG_CFLAGS@ \
    -I$(top_builddir) \
    -I$(top_srcdir)

ALL_LIBS = \
    @DBUS_LIBS@ \
    @GLIB_LIBS@

if OFFICIAL_RELEASE
dist-hook: version-script.txt
	@if grep '^[^#]*UNRELEASED' $< >&2; then \
		echo "*** You need to update version-script.txt to not say"; \
		echo "*** UNRELEASED before making a release tarball."; \
	fi >&2
endif

# Generated stuff

DROP_NAMESPACE = sed -e 's@xmlns:tp="http://telepathy\.freedesktop\.org/wiki/DbusSpec.extensions-v0"@@g'
XSLTPROCFLAGS = --nonet --novalid

# Bootstrapping

_gen/spec-stamp: $(wildcard $(top_srcdir)/spec/*.xml)
	$(mkdir_p) _gen
	touch $@

_gen/stable-stamp: $(wildcard *.xml) _gen/spec-stamp
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
	$(XSLTPROC) $(XSLTPROCFLAGS) \
		--stringparam mixed-case-prefix Tp \
		$(srcdir)/../tools/c-constants-generator.xsl \
		_gen/stable-spec.xml > $@

_gen/telepathy-interfaces.h: _gen/stable-spec.xml \
	../tools/glib-interfaces-generator.xsl \
	../tools/c-interfaces-generator.xsl
	$(XSLTPROC) $(XSLTPROCFLAGS) \
		--stringparam mixed-case-prefix Tp \
		$(srcdir)/../tools/glib-interfaces-generator.xsl \
		_gen/stable-spec.xml > $@

_gen/interfaces-body.h: _gen/stable-spec.xml \
	../tools/glib-interfaces-body-generator.xsl \
	../tools/c-interfaces-generator.xsl
	$(XSLTPROC) $(XSLTPROCFLAGS) \
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

# Things generated per interface

_gen/tp-spec-%.xml: %.xml ../tools/identity.xsl _gen/spec-stamp
	$(XSLTPROC) --xinclude $(XSLTPROCFLAGS) \
		$(srcdir)/../tools/identity.xsl \
		$< > $@

_gen/tp-svc-%.c _gen/tp-svc-%.h: _gen/tp-spec-%.xml \
	../tools/glib-ginterface-gen.py \
	Makefile.am
	$(PYTHON) $(srcdir)/../tools/glib-ginterface-gen.py \
		--filename=_gen/tp-svc-$* \
		--signal-marshal-prefix=_tp \
		--include='<telepathy-glib/dbus.h>' \
		--include="\"_gen/signals-marshal.h\"" \
		--not-implemented-func='tp_dbus_g_method_return_not_implemented' \
		$< Tp_Svc_

_gen/tp-cli-%-body.h _gen/tp-cli-%.h: _gen/tp-spec-%.xml \
	../tools/glib-client-gen.py \
	../tools/ls-interfaces.xsl \
	Makefile.am
	set -e; \
	subclass= ; \
	subclass_assert= ; \
	case $* in \
		channel) \
			subclass=--subclass=TpChannel; \
			subclass_assert=--subclass-assert=TP_IS_CHANNEL; \
			;; \
		connection-manager) \
			subclass=--subclass=TpConnectionManager; \
			subclass_assert=--subclass-assert=TP_IS_CONNECTION_MANAGER \
			;; \
		connection) \
			subclass=--subclass=TpConnection; \
			subclass_assert=--subclass-assert=TP_IS_CONNECTION; \
			;; \
		media-session-handler) \
			subclass=--subclass=TpMediaSessionHandler; \
			subclass_assert=--subclass-assert=TP_IS_MEDIA_SESSION_HANDLER; \
			;; \
		media-stream-handler) \
			subclass=--subclass=TpMediaStreamHandler; \
			subclass_assert=--subclass-assert=TP_IS_MEDIA_STREAM_HANDLER; \
			;; \
		dbus-daemon) \
			subclass=--subclass=TpDBusDaemon; \
			subclass_assert=--subclass-assert=TP_IS_DBUS_DAEMON; \
			;; \
	esac; \
	$(PYTHON) $(srcdir)/../tools/glib-client-gen.py \
		$$subclass $$subclass_assert \
		--group `echo $* | tr - _` \
		--iface-quark-prefix=TP_IFACE_QUARK \
		$< Tp_Cli _gen/tp-cli-$*
