noinst_PROGRAMS = \
    test-dbus

TESTS = $(noinst_PROGRAMS)

test_dbus_SOURCES = \
    test-dbus.c
test_dbus_LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(COVERAGE_CFLAGS) \
    $(TP_GLIB_CFLAGS)

TESTS_ENVIRONMENT = sh $(top_srcdir)/tools/with-session-bus.sh --session --
