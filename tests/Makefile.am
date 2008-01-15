SUBDIRS = \
    . \
    dbus \
    tools

noinst_PROGRAMS = \
    test-handle-set \
    test-heap \
    test-intset \
    test-util \
    test-internal-debug

TESTS = $(noinst_PROGRAMS)
TESTS_ENVIRONMENT = \
    G_DEBUG=fatal_warnings,fatal_criticals

# myassert.h is used by some of the tests in dbus/
EXTRA_DIST = myassert.h README

test_handle_set_SOURCES = \
    test-intset.c
test_handle_set_LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la

test_heap_SOURCES = \
    test-heap.c
test_heap_LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la

test_util_SOURCES = \
    test-util.c
test_util_LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la

test_intset_SOURCES = \
    test-intset.c
test_intset_LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la

# this needs to link against the static convenience library so that
# _tp_debug is still visible
test_internal_debug_SOURCES = \
    test-internal-debug.c
test_internal_debug_LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-internal.la

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)
