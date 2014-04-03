include $(top_srcdir)/tools/flymake.mk

tpgincludedir=$(includedir)/telepathy-glib-1/telepathy-glib
genincludedir=$(tpgincludedir)/_gen
tpgdbusincludedir=$(includedir)/telepathy-glib-1-dbus/telepathy-glib
gendbusincludedir=$(tpgdbusincludedir)/_gen

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = telepathy-glib-1.pc telepathy-glib-1-dbus.pc

EXTRA_DIST = \
    $(ABI_LISTS) \
    $(codegen_extra_dist) \
    $(introspection_extra_dist) \
    extra-gtkdoc.h

# libtelepathy-glib-1 is just a shared version of
# libtelepathy-glib-main-internal, produced by abi.am. Similarly,
# libtelepathy-glib-1-dbus is a shared version of
# libtelepathy-glib-dbus-internal.
#
# The static version is necessary because one of the tests wants to use
# internal symbols (_tp_log) which are made invisible by GNU ld, and it
# makes checking consistency of the ABI easier.

# Put the lowest-level library first, so that libtool will be able to relink
# them during installation.
lib_LTLIBRARIES = \
    libtelepathy-glib-1-dbus.la \
    libtelepathy-glib-1.la \
    $(NULL)

noinst_LTLIBRARIES = \
    libtelepathy-glib-main-internal.la \
    libtelepathy-glib-dbus-internal.la \
    libtelepathy-glib-reentrants.la \
    $(NULL)

# headers that are subject to coding-style checks
tpginclude_HEADERS = \
    account.h \
    account-channel-request.h \
    account-manager.h \
    account-request.h \
    automatic-client-factory.h \
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
    call-channel.h \
    call-content.h \
    call-content-media-description.h \
    call-stream.h \
    call-stream-endpoint.h \
    channel.h \
    channel-iface.h \
    channel-dispatcher.h \
    channel-dispatch-operation.h \
    channel-filter.h \
    channel-manager.h \
    channel-manager-request.h \
    channel-request.h \
    client.h \
    client-factory.h \
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
    dbus.h \
    dbus-properties-mixin.h \
    dbus-tube-channel.h \
    defs.h \
    debug.h \
    debug-sender.h \
    debug-client.h \
    debug-message.h \
    dtmf.h \
    enums.h \
    errors.h \
    exportable-channel.h \
    file-transfer-channel.h \
    gnio-unix.h \
    group-mixin.h \
    handle.h \
    handle-channel-context.h \
    handle-repo.h \
    handle-repo-static.h \
    handle-repo-dynamic.h \
    heap.h \
    intset.h \
    logger.h \
    message.h \
    message-mixin.h \
    observe-channel-context.h \
    presence-mixin.h \
    protocol.h \
    proxy.h \
    proxy-subclass.h \
    room-info.h \
    room-list.h \
    run.h \
    signalled-message.h \
    simple-approver.h \
    simple-handler.h \
    simple-observer.h \
    simple-password-manager.h \
    stream-tube-channel.h \
    stream-tube-connection.h \
    telepathy-glib.h \
    text-channel.h \
    tls-certificate.h \
    tls-certificate-rejection.h \
    util.h \
    variant-util.h \
    $(NULL)

tpgdbusinclude_HEADERS = \
    asv.h \
    cli-call.h \
    cli-channel.h \
    cli-connection.h \
    cli-misc.h \
    cli-proxy.h \
    gnio-util.h \
    gtypes.h \
    interfaces.h \
    sliced-gvalue.h \
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
    svc-interface.h \
    svc-properties-interface.h \
    svc-protocol.h \
    svc-tls.h \
    svc-logger.h \
    telepathy-glib-dbus.h \
    value-array.h \
    $(NULL)

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
    $(tpginclude_HEADERS) \
    $(tpgdbusinclude_HEADERS) \
    $(libtelepathy_glib_dbus_internal_la_SOURCES) \
    $(libtelepathy_glib_main_internal_la_SOURCES) \
    $(NULL)

include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

libtelepathy_glib_dbus_internal_la_LIBADD = \
    $(ALL_LIBS) \
    $(NULL)
libtelepathy_glib_dbus_internal_la_SOURCES = \
    asv.c \
    cli-channel.c \
    cli-connection.c \
    cli-misc.c \
    core-proxy.c \
    gnio-util.c \
    gtypes.c \
    interfaces.c \
    sliced-gvalue.c \
    svc-interface.c \
    value-array.c \
    $(NULL)

libtelepathy_glib_main_internal_la_LIBADD = \
    $(ALL_LIBS) \
    libtelepathy-glib-1-dbus.la \
    $(NULL)

libtelepathy_glib_main_internal_la_SOURCES = \
    account.c \
    account-channel-request.c \
    account-channel-request-internal.h \
    account-internal.h \
    account-manager.c \
    account-manager-internal.h \
    account-request.c \
    automatic-client-factory-internal.h \
    automatic-client-factory.c \
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
    capabilities.c \
    capabilities-internal.h \
    call-channel.c \
    call-content.c \
    call-content-media-description.c \
    call-internal.h \
    call-stream.c \
    call-stream-endpoint.c \
    channel.c \
    channel-filter.c \
    channel-filter-internal.h \
    channel-group.c \
    channel-internal.h \
    channel-dispatcher.c \
    channel-dispatch-operation.c \
    channel-manager.c \
    channel-manager-request.c \
    channel-manager-request-internal.h \
    channel-request.c \
    client.c \
    client-factory.c \
    client-factory-internal.h \
    client-message.c \
    client-message-internal.h \
    connection.c \
    connection-aliasing.c \
    connection-avatars.c \
    connection-contact-info.c \
    connection-contact-list.c \
    connection-internal.h \
    connection-manager.c \
    contact.c \
    contact-internal.h \
    contact-operations.c \
    contact-search.c \
    contact-search-internal.h \
    contact-search-result.c \
    base-contact-list-internal.h \
    base-contact-list.c \
    cm-message.c \
    cm-message-internal.h \
    dbus.c \
    dbus-internal.h \
    dbus-properties-mixin.c \
    dbus-tube-channel.c \
    debug.c \
    debug-client.c \
    debug-sender.c \
    debug-message.c \
    debug-message-internal.h \
    dtmf.c \
    debug-internal.h \
    errors.c \
    exportable-channel.c \
    file-transfer-channel.c \
    gnio-unix.c \
    group-mixin.c \
    handle.c \
    handle-channel-context-internal.h \
    handle-channel-context.c \
    handle-repo.c \
    handle-repo-dynamic.c \
    handle-repo-internal.h \
    handle-repo-static.c \
    handle-set.c \
    heap.c \
    intset.c \
    channel-iface.c \
    logger.c \
    message.c \
    message-internal.h \
    message-mixin.c \
    observe-channel-context-internal.h \
    observe-channel-context.c \
    presence-mixin.c \
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
    simple-handler.c \
    simple-observer.c \
    simple-password-manager.c \
    stream-tube-channel.c \
    stream-tube-connection-internal.h \
    stream-tube-connection.c \
    svc-interface-skeleton.c \
    svc-interface-skeleton-internal.h \
    text-channel.c \
    tls-certificate.c \
    tls-certificate-rejection.c \
    tls-certificate-rejection-internal.h \
    util.c \
    util-internal.h \
    variant-util.c \
    variant-util-internal.h

libtelepathy_glib_reentrants_la_SOURCES = \
    reentrants.c \
    reentrants.h

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
	androgenizer -:PROJECT telepathy-glib \
	\
	 -:SHARED libtelepathy-glib-1-dbus -:TAGS eng debug \
	 -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libtelepathy_glib_dbus_internal_la_SOURCES) \
	  $(nodist_libtelepathy_glib_dbus_internal_la_SOURCES) \
	 -:CFLAGS $(DEFAULT_INCLUDES) $(DEFS) $(CFLAGS) $(GIO_UNIX_CFLAGS) $(DBUS_CFLAGS) $(AM_CFLAGS) \
	 -:CPPFLAGS $(CPPFLAGS) $(AM_CPPFLAGS) $(INCLUDES) \
	 -:LDFLAGS $(ALL_LIBS) \
	  $(libtelepathy_glib_dbus_internal_la_LDFLAGS) \
	\
	 -:SHARED libtelepathy-glib-1 -:TAGS eng debug \
	 -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libtelepathy_glib_main_internal_la_SOURCES) \
	  $(nodist_libtelepathy_glib_main_internal_la_SOURCES) \
	 -:CFLAGS $(DEFAULT_INCLUDES) $(DEFS) $(CFLAGS) $(GIO_UNIX_CFLAGS) $(DBUS_CFLAGS) $(AM_CFLAGS) \
	 -:CPPFLAGS $(CPPFLAGS) $(AM_CPPFLAGS) $(INCLUDES) \
	 -:LDFLAGS $(ALL_LIBS) -ltelepathy-glib-1-dbus \
	  $(libtelepathy_glib_main_internal_la_LDFLAGS) \
	\
	> $@
