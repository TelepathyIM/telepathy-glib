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
    account-request.h \
    automatic-client-factory.h \
    automatic-proxy-factory.h \
    add-dispatch-operation-context.h \
    base-call-channel.h \
    base-call-content.h \
    base-call-stream.h \
    base-client.h \
    base-channel.h \
    base-connection.h \
    base-connection-manager.h \
    base-media-call-channel.h \
    base-media-call-content.h \
    base-media-call-stream.h \
    base-password-channel.h \
    base-protocol.h \
    base-room-config.h \
    basic-proxy-factory.h \
    call-channel.h \
    call-content.h \
    call-content-media-description.h \
    call-misc.h \
    call-stream.h \
    call-stream-endpoint.h \
    channel.h \
    channel-iface.h \
    channel-dispatcher.h \
    channel-dispatch-operation.h \
    channel-factory-iface.h \
    channel-manager.h \
    channel-request.h \
    client.h \
    client-channel-factory.h \
    client-message.h \
    cm-message.h \
    connection.h \
    connection-contact-list.h \
    connection-manager.h \
    contact-search.h \
    contact-search-result.h \
    capabilities.h \
    contact.h \
    contact-operations.h \
    base-contact-list.h \
    contacts-mixin.h \
    dbus.h \
    dbus-daemon.h \
    dbus-properties-mixin.h \
    dbus-tube-channel.h \
    defs.h \
    debug.h \
    debug-sender.h \
    debug-client.h \
    debug-ansi.h \
    debug-message.h \
    dtmf.h \
    enums.h \
    errors.h \
    exportable-channel.h \
    file-transfer-channel.h \
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
    message.h \
    message-mixin.h \
    observe-channels-context.h \
    presence-mixin.h \
    properties-mixin.h \
    protocol.h \
    proxy.h \
    proxy-subclass.h \
    room-info.h \
    room-list.h \
    run.h \
    signalled-message.h \
    simple-approver.h \
    simple-client-factory.h \
    simple-handler.h \
    simple-observer.h \
    simple-password-manager.h \
    stream-tube-channel.h \
    stream-tube-connection.h \
    svc-account.h \
    svc-account-manager.h \
    svc-call.h \
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
    telepathy-glib-dbus.h \
    text-channel.h \
    text-mixin.h \
    tls-certificate.h \
    tls-certificate-rejection.h \
    util.h \
    variant-util.h

tpginclude_HEADERS = \
    $(our_headers) \
    verify.h

BUILT_SOURCES = $(codegen_sources)

CLEANFILES = \
    $(BUILT_SOURCES)

nodist_tpginclude_HEADERS = \
    version.h \
    $(NULL)

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
    account-channel-request-internal.h \
    account-internal.h \
    account-manager.c \
    account-manager-internal.h \
    account-request.c \
    automatic-client-factory-internal.h \
    automatic-client-factory.c \
    automatic-proxy-factory.c \
    add-dispatch-operation-context-internal.h \
    add-dispatch-operation-context.c \
    base-call-channel.c \
    base-call-content.c \
    base-call-stream.c \
    base-call-internal.h \
    base-channel.c \
    base-client.c \
    base-client-internal.h \
    base-connection.c \
    base-connection-internal.h \
    base-connection-manager.c \
    base-media-call-channel.c \
    base-media-call-content.c \
    base-media-call-stream.c \
    base-password-channel.c \
    base-protocol-internal.h \
    base-protocol.c \
    base-room-config.c \
    basic-proxy-factory.c \
    capabilities.c \
    capabilities-internal.h \
    call-channel.c \
    call-content.c \
    call-content-media-description.c \
    call-internal.h \
    call-misc.c \
    call-stream.c \
    call-stream-endpoint.c \
    channel.c \
    channel-contacts.c \
    channel-group.c \
    channel-internal.h \
    channel-dispatcher.c \
    channel-dispatch-operation.c \
    channel-dispatch-operation-internal.h \
    channel-manager.c \
    channel-request.c \
    client.c \
    client-channel-factory.c \
    client-message.c \
    client-message-internal.h \
    connection.c \
    connection-aliasing.c \
    connection-avatars.c \
    connection-contact-info.c \
    connection-contact-list.c \
    connection-internal.h \
    connection-handles.c \
    connection-manager.c \
    contact.c \
    contact-internal.h \
    contact-list-channel-internal.h \
    contact-list-channel.c \
    contact-operations.c \
    contact-search.c \
    contact-search-internal.h \
    contact-search-result.c \
    base-contact-list.c \
    base-contact-list-internal.h \
    cm-message.c \
    cm-message-internal.h \
    contacts-mixin.c \
    dbus.c \
    dbus-daemon.c \
    dbus-internal.h \
    dbus-properties-mixin.c \
    dbus-properties-mixin-internal.h \
    dbus-tube-channel.c \
    debug.c \
    debug-client.c \
    debug-sender.c \
    debug-message.c \
    debug-message-internal.h \
    deprecated-internal.h \
    dtmf.c \
    interfaces.c \
    debug-internal.h \
    errors.c \
    exportable-channel.c \
    file-transfer-channel.c \
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
    message.c \
    message-internal.h \
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
    room-info.c \
    room-info-internal.h \
    room-list.c \
    run.c \
    signalled-message.c \
    signalled-message-internal.h \
    simple-approver.c \
    simple-client-factory.c \
    simple-client-factory-internal.h \
    simple-handler.c \
    simple-observer.c \
    simple-password-manager.c \
    stream-tube-channel.c \
    stream-tube-connection-internal.h \
    stream-tube-connection.c \
    text-channel.c \
    text-mixin.c \
    tls-certificate.c \
    tls-certificate-rejection.c \
    tls-certificate-rejection-internal.h \
    util.c \
    util-internal.h \
    variant-util.c \
    variant-util-internal.h

AM_CPPFLAGS = \
    -DG_LOG_DOMAIN=\"tp-glib\" \
    -D_TP_COMPILATION \
    $(NULL)

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    @DBUS_CFLAGS@ \
    @GLIB_CFLAGS@ \
    -I$(top_builddir) \
    -I$(top_srcdir)
AM_LDFLAGS = \
    $(ERROR_LDFLAGS) \
    $(NULL)

ALL_LIBS = \
    @DBUS_LIBS@ \
    @GLIB_LIBS@

include abi.am
include codegen.am
include introspection.am

Android.mk: Makefile.am $(codegen_sources)
	androgenizer -:PROJECT telepathy-glib -:SHARED libtelepathy-glib -:TAGS eng debug \
	 -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libtelepathy_glib_internal_la_SOURCES) \
	  $(nodist_libtelepathy_glib_internal_la_SOURCES) \
	 -:CFLAGS $(DEFAULT_INCLUDES) $(DEFS) $(CFLAGS) $(GIO_UNIX_CFLAGS) $(DBUS_CFLAGS) $(AM_CFLAGS) \
	 -:CPPFLAGS $(CPPFLAGS) $(AM_CPPFLAGS) $(INCLUDES) \
	 -:LDFLAGS $(libtelepathy_glib_internal_la_LIBADD) \
	  $(libtelepathy_glib_internal_la_LDFLAGS) \
	> $@
