# It's necessary to distinguish between modules that link to
# libtelepathy-glib.la (normally a shared library, containing a copy of
# libtelepathy-glib-internal.la) and libtelepathy-glib-internal.la (a static
# convenience library). Under -Bsymbolic, you can't safely link both.
#
# You can either link to libtp-glib-tests.la and libtelepathy-glib.la,
# or to libtp-glib-tests-internal.la and libtelepathy-glib-internal.la.
# Do not mix the versions.
#
# See https://bugs.freedesktop.org/show_bug.cgi?id=39183
noinst_LTLIBRARIES = \
    libtp-glib-tests.la \
    libtp-glib-tests-internal.la \
    $(NULL)

libtp_glib_tests_la_SOURCES = \
    bug-19101-conn.c \
    bug-19101-conn.h \
    bug16307-conn.c \
    bug16307-conn.h \
    broken-client-types-conn.c \
    broken-client-types-conn.h \
    contacts-conn.c \
    contacts-conn.h \
    contact-list-manager.c \
    contact-list-manager.h \
    debug.h \
    dbus-tube-chan.c \
    dbus-tube-chan.h \
    echo-chan.h \
    echo-chan.c \
    echo-cm.h \
    echo-cm.c \
    echo-conn.h \
    echo-conn.c \
    echo-channel-manager-conn.h \
    echo-channel-manager-conn.c \
    echo-im-manager.h \
    echo-im-manager.c \
    file-transfer-chan.h \
    file-transfer-chan.c \
    myassert.h \
    my-conn-proxy.h \
    my-conn-proxy.c \
    params-cm.h \
    params-cm.c \
    room-list-chan.h \
    room-list-chan.c \
    simple-account.c \
    simple-account.h \
    simple-account-manager.c\
    simple-account-manager.h\
    simple-channel-manager.c \
    simple-channel-manager.h \
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
    tls-certificate.h \
    tls-certificate.c \
    util.c \
    util.h
libtp_glib_tests_internal_la_SOURCES = $(libtp_glib_tests_la_SOURCES)

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

AM_CPPFLAGS = \
    -D_TP_COMPILATION \
    $(NULL)

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)
AM_LDFLAGS = \
    $(ERROR_LDFLAGS) \
    $(NULL)

libtp_glib_tests_internal_la_CPPFLAGS = \
    $(AM_CPPFLAGS) \
    -DTP_GLIB_TESTS_INTERNAL \
    $(NULL)

libtp_glib_tests_internal_la_LIBADD = \
    $(DBUS_LIBS) \
    $(GLIB_LIBS) \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-internal.la

libtp_glib_tests_la_LIBADD = \
    $(DBUS_LIBS) \
    $(GLIB_LIBS) \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la
