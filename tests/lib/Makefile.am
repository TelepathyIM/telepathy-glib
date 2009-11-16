noinst_LTLIBRARIES = libtp-glib-tests.la

libtp_glib_tests_la_SOURCES = \
    bug-19101-conn.c \
    bug-19101-conn.h \
    bug16307-conn.c \
    bug16307-conn.h \
    contacts-conn.c \
    contacts-conn.h \
    debug.h \
    myassert.h \
    params-cm.h \
    params-cm.c \
    simple-account-manager.c\
    simple-account-manager.h\
    simple-conn.c \
    simple-conn.h \
    simple-manager.c \
    simple-manager.h \
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

libtp_glib_tests_la_LIBADD = \
    $(DBUS_LIBS) \
    $(GLIB_LIBS) \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la
