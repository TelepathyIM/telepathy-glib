include $(top_srcdir)/tools/flymake.mk

tpgincludedir=$(includedir)/telepathy-1.0/telepathy-glib
genincludedir=$(tpgincludedir)/_gen

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = telepathy-glib.pc

ABI_LISTS = \
    versions/0.7.0.abi \
    versions/0.7.1.abi \
    versions/0.7.2.abi \
    versions/0.7.3.abi \
    versions/0.7.5.abi \
    versions/0.7.6.abi \
    versions/0.7.7.abi \
    versions/0.7.8.abi \
    versions/0.7.9.abi \
    versions/0.7.10.abi \
    versions/0.7.12.abi \
    versions/0.7.13.abi \
    versions/0.7.14.abi \
    versions/0.7.15.abi \
    versions/0.7.16.abi \
    versions/0.7.17.abi \
    versions/0.7.18.abi \
    versions/0.7.19.abi \
    versions/0.7.20.abi \
    versions/0.7.21.abi \
    versions/0.7.23.abi \
    versions/0.7.24.abi \
    versions/0.7.26.abi \
    versions/0.7.27.abi \
    versions/0.7.28.abi \
    versions/0.7.29.abi \
    versions/0.7.30.abi \
    versions/0.7.31.abi \
    versions/0.7.32.abi \
    versions/0.7.34.abi \
    versions/0.7.35.abi \
    versions/0.7.36.abi \
    versions/0.7.37.abi \
    versions/0.9.0.abi \
    versions/0.9.2.abi

EXTRA_DIST = \
    $(ABI_LISTS) \
    account.xml \
    account-manager.xml \
    channel.xml \
    channel-dispatcher.xml \
    channel-dispatch-operation.xml \
    channel-request.xml \
    client.xml \
    debug.xml \
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
nodist_libtelepathy_glib_la_SOURCES =
libtelepathy_glib_la_DEPENDENCIES = libtelepathy-glib-internal.la Makefile.am

if HAVE_LD_VERSION_SCRIPT
nodist_libtelepathy_glib_la_SOURCES += _gen/version-script.txt
libtelepathy_glib_la_DEPENDENCIES += _gen/version-script.txt _gen/abi.txt

if OFFICIAL_RELEASE
# don't allow undocumented ABI
MAKE_VERSION_SCRIPT_FLAGS =
else
# allow new ABI, and silently put it in a dummy version (which can be used to
# check whether binaries have been linked against unguaranteed ABI)
MAKE_VERSION_SCRIPT_FLAGS = \
    --unreleased-version=TELEPATHY_GLIB_@VERSION@_UNRELEASED
endif

_gen/version-script.txt: $(ABI_LISTS) _gen/abi.txt Makefile.am \
	$(top_srcdir)/tools/make-version-script.py
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/tools/make-version-script.py \
		 --symbols=_gen/abi.txt $(MAKE_VERSION_SCRIPT_FLAGS) \
		 $(ABI_LISTS:%=$(srcdir)/%) > $@
	$(PYTHON) $(top_srcdir)/tools/make-version-script.py \
		 --symbols=_gen/abi.txt $(MAKE_VERSION_SCRIPT_FLAGS) \
		 --dpkg "libtelepathy-glib.so.0 libtelepathy-glib0 #MINVER#" \
		 --dpkg-build-depends-package "libtelepathy-glib-dev" \
		 $(ABI_LISTS:%=$(srcdir)/%) > _gen/libtelepathy-glib0.symbols
	sed -n -e "s/^[	 ]*\\(tp_.*\\);/\\1/p" < $@ > _gen/versioned-abi.tmp
	sort -u < _gen/versioned-abi.tmp > _gen/versioned-abi.txt
	: # the versioned API should always match the ^tp API
	diff -c _gen/versioned-abi.txt _gen/abi.txt

_gen/abi.txt: libtelepathy-glib-internal.la Makefile.am
	$(NM) .libs/libtelepathy-glib-internal.a > _gen/abi.nm
	grep " [DT] " < _gen/abi.nm > _gen/abi.funcs
	cut -d" " -f3 < _gen/abi.funcs > _gen/abi.funcnames
	grep "^tp" < _gen/abi.funcnames > _gen/abi.tpfuncnames
	$(AM_V_GEN)sort -u < _gen/abi.tpfuncnames > $@

libtelepathy_glib_la_LDFLAGS += \
    $(VERSION_SCRIPT_ARG)=_gen/version-script.txt

else # !HAVE_LD_VERSION_SCRIPT

libtelepathy_glib_la_LDFLAGS += -export-symbols-regex '^tp'

endif # !HAVE_LD_VERSION_SCRIPT

# headers that are subject to coding-style checks
our_headers = \
    account.h \
    account-manager.h \
    base-connection.h \
    base-connection-manager.h \
    channel.h \
    channel-iface.h \
    channel-dispatcher.h \
    channel-dispatch-operation.h \
    channel-factory-iface.h \
    channel-manager.h \
    channel-request.h \
    client.h \
    connection.h \
    connection-manager.h \
    contact.h \
    contacts-mixin.h \
    dbus.h \
    dbus-daemon.h \
    dbus-properties-mixin.h \
    defs.h \
    debug.h \
    debug-sender.h \
    debug-ansi.h \
    enums.h \
    errors.h \
    exportable-channel.h \
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
    message-mixin.h \
    presence-mixin.h \
    properties-mixin.h \
    proxy.h \
    proxy-subclass.h \
    run.h \
    svc-account.h \
    svc-account-manager.h \
    svc-channel-dispatcher.h \
    svc-channel-dispatch-operation.h \
    svc-channel.h \
    svc-channel-request.h \
    svc-client.h \
    svc-connection.h \
    svc-connection-manager.h \
    svc-debug.h \
    svc-generic.h \
    svc-media-interfaces.h \
    svc-properties-interface.h \
    telepathy-glib.h \
    text-mixin.h \
    util.h

tpginclude_HEADERS = \
    $(our_headers) \
    verify.h

nodist_libtelepathy_glib_internal_la_SOURCES = \
    _gen/error-str.c \
    _gen/signals-marshal.c \
    _gen/signals-marshal.h \
    _gen/signals-marshal.list \
    _gen/tp-signals-marshal.list \
    _gen/interfaces-body.h \
    _gen/gtypes-body.h \
    _gen/register-dbus-glib-marshallers-body.h \
    _gen/tp-cli-account-body.h \
    _gen/tp-cli-account-manager-body.h \
    _gen/tp-cli-channel-body.h \
    _gen/tp-cli-channel-dispatcher-body.h \
    _gen/tp-cli-channel-dispatch-operation-body.h \
    _gen/tp-cli-channel-request-body.h \
    _gen/tp-cli-client-body.h \
    _gen/tp-cli-connection-body.h \
    _gen/tp-cli-connection-manager-body.h \
    _gen/tp-cli-dbus-daemon-body.h \
    _gen/tp-cli-generic-body.h \
    _gen/tp-cli-media-session-handler-body.h \
    _gen/tp-cli-media-stream-handler-body.h \
    _gen/tp-svc-account.c \
    _gen/tp-svc-account-manager.c \
    _gen/tp-svc-channel.c \
    _gen/tp-svc-channel-dispatcher.c \
    _gen/tp-svc-channel-dispatch-operation.c \
    _gen/tp-svc-channel-request.c \
    _gen/tp-svc-client.c \
    _gen/tp-svc-connection.c \
    _gen/tp-svc-connection-manager.c \
    _gen/tp-svc-debug.c \
    _gen/tp-svc-generic.c \
    _gen/tp-svc-media-session-handler.c \
    _gen/tp-svc-media-stream-handler.c

nodist_geninclude_HEADERS = \
    _gen/error-str.h \
    _gen/telepathy-enums.h \
    _gen/telepathy-interfaces.h \
    _gen/gtypes.h \
    _gen/tp-cli-account.h \
    _gen/tp-cli-account-manager.h \
    _gen/tp-cli-channel.h \
    _gen/tp-cli-channel-dispatcher.h \
    _gen/tp-cli-channel-dispatch-operation.h \
    _gen/tp-cli-channel-request.h \
    _gen/tp-cli-client.h \
    _gen/tp-cli-connection.h \
    _gen/tp-cli-connection-manager.h \
    _gen/tp-cli-dbus-daemon.h \
    _gen/tp-cli-generic.h \
    _gen/tp-cli-media-session-handler.h \
    _gen/tp-cli-media-stream-handler.h \
    _gen/tp-svc-account.h \
    _gen/tp-svc-account-manager.h \
    _gen/tp-svc-channel.h \
    _gen/tp-svc-channel-dispatcher.h \
    _gen/tp-svc-channel-dispatch-operation.h \
    _gen/tp-svc-channel-request.h \
    _gen/tp-svc-client.h \
    _gen/tp-svc-connection.h \
    _gen/tp-svc-connection-manager.h \
    _gen/tp-svc-debug.h \
    _gen/tp-svc-generic.h \
    _gen/tp-svc-media-session-handler.h \
    _gen/tp-svc-media-stream-handler.h

BUILT_SOURCES = \
    $(nodist_libtelepathy_glib_internal_la_SOURCES) \
    $(nodist_geninclude_HEADERS) \
    _gen/stable-spec.xml \
    _gen/spec-stamp \
    _gen/stable-stamp

CLEANFILES = \
    $(BUILT_SOURCES)

distclean-local:
	rm -rf _gen

check_c_sources = \
    $(our_headers) \
    $(libtelepathy_glib_internal_la_SOURCES)

include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

libtelepathy_glib_internal_la_LIBADD = $(ALL_LIBS)
libtelepathy_glib_internal_la_SOURCES = \
    account.c \
    account-internal.h \
    account-manager.c \
    account-manager-internal.h \
    base-connection.c \
    base-connection-manager.c \
    channel.c \
    channel-group.c \
    channel-internal.h \
    channel-dispatcher.c \
    channel-dispatch-operation.c \
    channel-manager.c \
    channel-request.c \
    client.c \
    connection.c \
    connection-internal.h \
    connection-handles.c \
    connection-manager.c \
    contact.c \
    contacts-mixin.c \
    dbus.c \
    dbus-daemon.c \
    dbus-internal.h \
    dbus-properties-mixin.c \
    debug.c \
    debug-sender.c \
    interfaces.c \
    debug-internal.h \
    errors.c \
    exportable-channel.c \
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
    message-mixin.c \
    presence-mixin.c \
    properties-mixin.c \
    proxy.c \
    proxy-internal.h \
    proxy-methods.c \
    proxy-signals.c \
    run.c \
    signals-marshal.list \
    text-mixin.c \
    util.c \
    util-internal.h

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

# Generated stuff

DROP_NAMESPACE = sed -e 's@xmlns:tp="http://telepathy\.freedesktop\.org/wiki/DbusSpec.extensions-v0"@@g'
tools_dir = $(top_srcdir)/tools

# Bootstrapping

_gen/spec-stamp: $(wildcard $(top_srcdir)/spec/*.xml)
	$(mkdir_p) _gen
	touch $@

_gen/stable-stamp: $(wildcard *.xml) _gen/spec-stamp
	touch $@

_gen/stable-spec.xml: stable-interfaces.xml _gen/stable-stamp $(tools_dir)/xincludator.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/xincludator.py $< > $@

# Things generated from the whole spec at once

_gen/gtypes.h _gen/gtypes-body.h: _gen/stable-spec.xml \
	$(tools_dir)/glib-gtypes-generator.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-gtypes-generator.py \
		_gen/stable-spec.xml \
		_gen/gtypes Tp

_gen/telepathy-enums.h: _gen/stable-spec.xml \
	$(tools_dir)/c-constants-gen.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/c-constants-gen.py \
		Tp \
		$< > $@

_gen/interfaces-body.h _gen/telepathy-interfaces.h: _gen/stable-spec.xml \
	$(tools_dir)/glib-interfaces-gen.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-interfaces-gen.py \
		Tp _gen/interfaces-body.h _gen/telepathy-interfaces.h $<

_gen/register-dbus-glib-marshallers-body.h: _gen/stable-spec.xml \
	$(tools_dir)/glib-client-marshaller-gen.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-client-marshaller-gen.py $< _tp > $@

_gen/tp-signals-marshal.list: $(tools_dir)/glib-signals-marshal-gen.py \
	_gen/stable-spec.xml
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-signals-marshal-gen.py \
		_gen/stable-spec.xml > $@

_gen/signals-marshal.list: signals-marshal.list _gen/tp-signals-marshal.list
	$(AM_V_GEN)sort -u $^ > $@

_gen/signals-marshal.h: _gen/signals-marshal.list Makefile.am
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --header --prefix=_tp_marshal $< > $@

_gen/signals-marshal.c: _gen/signals-marshal.list Makefile.am
	$(AM_V_GEN){ echo '#include "_gen/signals-marshal.h"' && \
	$(GLIB_GENMARSHAL) --body --prefix=_tp_marshal $< ; } > $@

_gen/error-str.h _gen/error-str.c: _gen/stable-spec.xml \
	$(tools_dir)/glib-errors-str-gen.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-errors-str-gen.py \
		_gen/error-str $<

# Things generated per interface

_gen/tp-spec-%.xml: %.xml _gen/spec-stamp $(tools_dir)/xincludator.py
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/xincludator.py $< > $@

_gen/tp-svc-%.c _gen/tp-svc-%.h: _gen/tp-spec-%.xml \
	$(tools_dir)/glib-ginterface-gen.py \
	Makefile.am
	$(AM_V_GEN)$(PYTHON) $(tools_dir)/glib-ginterface-gen.py \
		--filename=_gen/tp-svc-$* \
		--signal-marshal-prefix=_tp \
		--include='<telepathy-glib/dbus.h>' \
		--include="\"_gen/signals-marshal.h\"" \
		--not-implemented-func='tp_dbus_g_method_return_not_implemented' \
		$< Tp_Svc_

_gen/tp-cli-%-body.h _gen/tp-cli-%.h: _gen/tp-spec-%.xml \
	$(tools_dir)/glib-client-gen.py \
	Makefile.am
	$(AM_V_GEN)set -e; \
	subclass= ; \
	subclass_assert= ; \
	case $* in \
		generic) \
			maybe_reentrant=--generate-reentrant; \
			;; \
		account-manager) \
			subclass=--subclass=TpAccountManager; \
			subclass_assert=--subclass-assert=TP_IS_ACCOUNT_MANAGER; \
			;; \
		account) \
			subclass=--subclass=TpAccount; \
			subclass_assert=--subclass-assert=TP_IS_ACCOUNT; \
			;; \
		channel) \
			subclass=--subclass=TpChannel; \
			subclass_assert=--subclass-assert=TP_IS_CHANNEL; \
			maybe_reentrant=--generate-reentrant; \
			;; \
		channel-dispatcher) \
			subclass=--subclass=TpChannelDispatcher; \
			subclass_assert=--subclass-assert=TP_IS_CHANNEL_DISPATCHER; \
			;; \
		channel-dispatch-operation) \
			subclass=--subclass=TpChannelDispatchOperation; \
			subclass_assert=--subclass-assert=TP_IS_CHANNEL_DISPATCH_OPERATION; \
			;; \
		channel-request) \
			subclass=--subclass=TpChannelRequest; \
			subclass_assert=--subclass-assert=TP_IS_CHANNEL_REQUEST; \
			;; \
		client) \
			subclass=--subclass=TpClient; \
			subclass_assert=--subclass-assert=TP_IS_CLIENT; \
			;; \
		connection-manager) \
			subclass=--subclass=TpConnectionManager; \
			subclass_assert=--subclass-assert=TP_IS_CONNECTION_MANAGER \
			maybe_reentrant=--generate-reentrant; \
			;; \
		connection) \
			subclass=--subclass=TpConnection; \
			subclass_assert=--subclass-assert=TP_IS_CONNECTION; \
			maybe_reentrant=--generate-reentrant; \
			;; \
		media-session-handler) \
			subclass=--subclass=TpMediaSessionHandler; \
			subclass_assert=--subclass-assert=TP_IS_MEDIA_SESSION_HANDLER; \
			maybe_reentrant=--generate-reentrant; \
			;; \
		media-stream-handler) \
			subclass=--subclass=TpMediaStreamHandler; \
			subclass_assert=--subclass-assert=TP_IS_MEDIA_STREAM_HANDLER; \
			maybe_reentrant=--generate-reentrant; \
			;; \
		dbus-daemon) \
			subclass=--subclass=TpDBusDaemon; \
			subclass_assert=--subclass-assert=TP_IS_DBUS_DAEMON; \
			maybe_reentrant=--generate-reentrant; \
			;; \
	esac; \
	$(PYTHON) $(tools_dir)/glib-client-gen.py \
		$$subclass $$subclass_assert \
		--group `echo $* | tr - _` \
		--iface-quark-prefix=TP_IFACE_QUARK \
		--tp-proxy-api=0.7.6 \
		$$maybe_reentrant \
		$< Tp_Cli _gen/tp-cli-$*
