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
    versions/0.9.2.abi \
    versions/0.11.0.abi \
    versions/0.11.1.abi \
    versions/0.11.3.abi

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
    stable-interfaces.xml \
    kludge-616375.xsl

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
    capabilities.h \
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
    gnio-util.h \
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

BUILT_SOURCES = $(codegen_sources)

CLEANFILES = \
    $(BUILT_SOURCES) \
    $(BUILT_GIRFILES)

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
    capabilities.c \
    capabilities-internal.h \
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
    gnio-util.c \
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

include codegen.am
include introspection.am
