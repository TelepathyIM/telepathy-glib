noinst_PROGRAMS = \
    test-call-cancellation \
    test-dbus \
    test-disconnection \
    test-example-no-protocols

TESTS = $(noinst_PROGRAMS)

test_call_cancellation_LDADD = $(TP_GLIB_LIBS)
test_dbus_LDADD = $(TP_GLIB_LIBS)
test_disconnection_LDADD = $(TP_GLIB_LIBS)
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
    G_DEBUG=fatal_warnings,fatal_criticals \
    sh $(top_srcdir)/tools/with-session-bus.sh --session --
