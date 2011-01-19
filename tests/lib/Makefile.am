noinst_LTLIBRARIES = libtp-logger-tests.la

libtp_logger_tests_la_SOURCES = \
    contacts-conn.c \
    contacts-conn.h \
    simple-conn.c \
    simple-conn.h \
    textchan-null.c \
    textchan-null.h \
    util.c \
    util.h

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TPL_CFLAGS)

libtp_logger_tests_la_LIBADD = \
    $(DBUS_LIBS) \
    $(GLIB_LIBS) \
    $(top_builddir)/telepathy-logger/libtelepathy-logger.la
