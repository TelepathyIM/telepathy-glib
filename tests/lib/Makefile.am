noinst_LTLIBRARIES = libtp-glib-tests.la

libtp_glib_tests_la_SOURCES = \
    bug-19101-conn.c \
    bug-19101-conn.h \
    bug16307-conn.c \
    bug16307-conn.h \
    contacts-conn.c \
    contacts-conn.h \
    contact-list-manager.c \
    contact-list-manager.h \
    debug.h \
    echo-chan.h \
    echo-chan.c \
    echo-cm.h \
    echo-cm.c \
    echo-conn.h \
    echo-conn.c \
    echo-im-manager.h \
    echo-im-manager.c \
    myassert.h \
    my-conn-proxy.h \
    my-conn-proxy.c \
    params-cm.h \
    params-cm.c \
    simple-account.c \
    simple-account.h \
    simple-account-manager.c\
    simple-account-manager.h\
    simple-channel-dispatch-operation.c \
    simple-channel-dispatch-operation.h \
    simple-channel-dispatcher.c \
    simple-channel-dispatcher.h \
    simple-channel-request.c \
    simple-channel-request.h \
    simple-client.c \
    simple-client.h \
    simple-conn.c \
    simple-conn.h \
    simple-manager.c \
    simple-manager.h \
    stream-tube-chan.c \
    stream-tube-chan.h \
    stub-object.c \
    stub-object.h \
    textchan-null.c \
    textchan-null.h \
    textchan-group.c \
    textchan-group.h \
    util.c \
    util.h

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)
AM_LDFLAGS = \
    $(ERROR_LDFLAGS) \
    $(NULL)

libtp_glib_tests_la_LIBADD = \
    $(DBUS_LIBS) \
    $(GLIB_LIBS) \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la
