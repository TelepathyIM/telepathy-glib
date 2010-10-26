include $(top_srcdir)/tools/flymake.mk

tpgincludedir=$(includedir)/telepathy-1.0/telepathy-glib
genincludedir=$(tpgincludedir)/_gen

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = telepathy-glib.pc

EXTRA_DIST = \
    $(ABI_LISTS) \
    $(codegen_extra_dist) \
    $(introspection_extra_dist) \
    extra-gtkdoc.h

# libtelepathy-glib is just a shared version of libtelepathy-glib-internal,
# produced by abi.am.
#
# The static version is necessary because one of the tests wants to use
# internal symbols (_tp_log) which are made invisible by GNU ld.

lib_LTLIBRARIES = libtelepathy-glib.la
noinst_LTLIBRARIES = libtelepathy-glib-internal.la

# headers that are subject to coding-style checks
our_headers = \
    account.h \
    account-channel-request.h \
    account-manager.h \
    automatic-proxy-factory.h \
    add-dispatch-operation-context.h \
    base-client.h \
    base-channel.h \
    base-connection.h \
    base-connection-manager.h \
    base-protocol.h \
    basic-proxy-factory.h \
    channel.h \
    channel-iface.h \
    channel-dispatcher.h \
    channel-dispatch-operation.h \
    channel-factory-iface.h \
    channel-manager.h \
    channel-request.h \
    client.h \
    client-channel-factory.h \
    connection.h \
    connection-manager.h \
    capabilities.h \
    contact.h \
    base-contact-list.h \
    contacts-mixin.h \
    dbus.h \
    dbus-daemon.h \
    dbus-properties-mixin.h \
    defs.h \
    debug.h \
    debug-sender.h \
    debug-ansi.h \
    dtmf.h \
    enums.h \
    errors.h \
    exportable-channel.h \
    gnio-util.h \
    group-mixin.h \
    gtypes.h \
    handle.h \
    handle-channels-context.h \
    handle-repo.h \
    handle-repo-static.h \
    handle-repo-dynamic.h \
    heap.h \
    interfaces.h \
    intset.h \
    media-interfaces.h \
    message-mixin.h \
    observe-channels-context.h \
    presence-mixin.h \
    properties-mixin.h \
    protocol.h \
    proxy.h \
    proxy-subclass.h \
    run.h \
    simple-approver.h \
    simple-handler.h \
    simple-observer.h \
    stream-tube-channel.h \
    stream-tube-connection.h \
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
    svc-protocol.h \
    svc-tls.h \
    telepathy-glib.h \
    text-mixin.h \
    util.h

tpginclude_HEADERS = \
    $(our_headers) \
    verify.h

BUILT_SOURCES = $(codegen_sources)

CLEANFILES = \
    $(BUILT_SOURCES)

distclean-local:
	rm -rf _gen

clean-local:
	rm -rf tmp-introspect*

check_c_sources = \
    $(our_headers) \
    $(libtelepathy_glib_internal_la_SOURCES)

include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

libtelepathy_glib_internal_la_LIBADD = $(ALL_LIBS)
libtelepathy_glib_internal_la_SOURCES = \
    account.c \
    account-channel-request.c \
    account-internal.h \
    account-manager.c \
    account-manager-internal.h \
    automatic-proxy-factory.c \
    add-dispatch-operation-context-internal.h \
    add-dispatch-operation-context.c \
    base-channel.c \
    base-client.c \
    base-client-internal.h \
    base-connection.c \
    base-connection-internal.h \
    base-connection-manager.c \
    base-protocol-internal.h \
    base-protocol.c \
    basic-proxy-factory.c \
    capabilities.c \
    capabilities-internal.h \
    channel.c \
    channel-group.c \
    channel-internal.h \
    channel-dispatcher.c \
    channel-dispatch-operation.c \
    channel-dispatch-operation-internal.h \
    channel-manager.c \
    channel-request.c \
    client.c \
    client-channel-factory.c \
    connection.c \
    connection-avatars.c \
    connection-contact-info.c \
    connection-internal.h \
    connection-handles.c \
    connection-manager.c \
    contact.c \
    contact-list-channel-internal.h \
    contact-list-channel.c \
    base-contact-list.c \
    base-contact-list-internal.h \
    contacts-mixin.c \
    dbus.c \
    dbus-daemon.c \
    dbus-internal.h \
    dbus-properties-mixin.c \
    debug.c \
    debug-sender.c \
    dtmf.c \
    interfaces.c \
    debug-internal.h \
    errors.c \
    exportable-channel.c \
    gnio-util.c \
    group-mixin.c \
    gtypes.c \
    handle.c \
    handle-channels-context-internal.h \
    handle-channels-context.c \
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
    observe-channels-context-internal.h \
    observe-channels-context.c \
    presence-mixin.c \
    properties-mixin.c \
    protocol.c \
    protocol-internal.h \
    proxy.c \
    proxy-internal.h \
    proxy-methods.c \
    proxy-signals.c \
    run.c \
    signals-marshal.list \
    simple-approver.c \
    simple-handler.c \
    simple-observer.c \
    stream-tube-channel.c \
    stream-tube-connection-internal.h \
    stream-tube-connection.c \
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

include abi.am
include codegen.am
include introspection.am
