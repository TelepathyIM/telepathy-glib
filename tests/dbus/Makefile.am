noinst_PROGRAMS = \
    test-dbus \
    test-example-no-protocols

TESTS = $(noinst_PROGRAMS)

test_dbus_SOURCES = \
    test-dbus.c
test_dbus_LDADD = $(TP_GLIB_LIBS)

test_example_no_protocols_SOURCES = \
    test-example-no-protocols.c
test_example_no_protocols_LDADD = $(TP_GLIB_LIBS)

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(COVERAGE_CFLAGS) \
    $(TP_GLIB_CFLAGS)

TESTS_ENVIRONMENT = \
    abs_top_builddir=@abs_top_builddir@ \
    XDG_DATA_HOME=@abs_builddir@ \
    XDG_DATA_DIRS=@abs_srcdir@ \
    sh $(top_srcdir)/tools/with-session-bus.sh --session --
