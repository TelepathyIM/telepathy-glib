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
    test-util \
    test-debug-domain

TESTS = $(noinst_PROGRAMS) \
    all-errors-documented.py

TESTS_ENVIRONMENT = \
    abs_top_builddir=@abs_top_builddir@ \
    abs_top_srcdir=@abs_top_srcdir@ \
    G_SLICE=debug-blocks \
    G_DEBUG=fatal_warnings,fatal_criticals$(maybe_gc_friendly) \
    PYTHONPATH=@abs_top_srcdir@/tools

include $(top_srcdir)/tools/valgrind.mk

VALGRIND_TESTS_ENVIRONMENT = \
    $(TESTS_ENVIRONMENT) \
    G_SLICE=always-malloc \
    $(top_builddir)/libtool --mode=execute \
    $(VALGRIND) $(VALGRIND_FLAGS)

check-valgrind:
	$(MAKE) check-TESTS \
		maybe_gc_friendly=,gc-friendly \
		TESTS='$$(noinst_PROGRAMS)' \
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

test_debug_domain_SOURCES = \
    debug-domain.c

test_internal_debug_SOURCES = \
    internal-debug.c

# this needs to link against the static convenience library so that
# _tp_debug is still visible
test_debug_domain_LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-internal.la
test_internal_debug_LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-internal.la

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la \
    $(GLIB_LIBS)

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)
