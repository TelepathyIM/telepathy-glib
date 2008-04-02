noinst_LTLIBRARIES = libtp-glib-tests.la

libtp_glib_tests_la_SOURCES = \
    myassert.h \
    simple-conn.c \
    simple-conn.h \
    simple-manager.c \
    simple-manager.h \
    stub-object.c \
    stub-object.h \
    textchan-null.c \
    textchan-null.h

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)
