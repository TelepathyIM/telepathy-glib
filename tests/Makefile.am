SUBDIRS = \
    . \
    lib \
    dbus \
    tools

noinst_PROGRAMS = \
    test-asv \
    test-availability-cmp \
    test-heap \
    test-internal-debug \
    test-intset \
    test-util

TESTS = $(noinst_PROGRAMS) \
    all-errors-documented.py

TESTS_ENVIRONMENT = \
    abs_top_builddir=@abs_top_builddir@ \
    abs_top_srcdir=@abs_top_srcdir@ \
    G_DEBUG=fatal_warnings,fatal_criticals \
    PYTHONPATH=@abs_top_srcdir@/tools

VALGRIND_TESTS_ENVIRONMENT = \
    G_SLICE=always-malloc \
    $(TESTS_ENVIRONMENT) \
    $(top_builddir)/libtool --mode=execute \
    valgrind --tool=memcheck --leak-check=full --show-reachable=yes \
        --suppressions=$(top_srcdir)/examples/tp-glib-examples.supp \
        --child-silent-after-fork=yes --num-callers=20 --error-exitcode=42 \
        --gen-suppressions=all \
        $(VALGRIND_FLAGS)

check-valgrind:
	$(MAKE) check-TESTS \
		TESTS_ENVIRONMENT="$(VALGRIND_TESTS_ENVIRONMENT)"
	$(MAKE) -C dbus check-valgrind

EXTRA_DIST = README \
    all-errors-documented.py

test_asv_SOURCES = \
    asv.c

test_heap_SOURCES = \
    heap.c

test_util_SOURCES = \
    util.c

test_intset_SOURCES = \
    intset.c

test_availability_cmp_SOURCES = \
    availability-cmp.c

# this needs to link against the static convenience library so that
# _tp_debug is still visible
test_internal_debug_SOURCES = \
    internal-debug.c
test_internal_debug_LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-internal.la

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)
