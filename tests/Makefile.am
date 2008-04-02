SUBDIRS = \
    . \
    lib \
    dbus \
    tools

DYNAMIC_TESTS = \
    test-heap \
    test-intset \
    test-util
STATIC_TESTS = \
    test-internal-debug

noinst_PROGRAMS = $(DYNAMIC_TESTS) $(STATIC_TESTS)
TESTS = $(noinst_PROGRAMS)
TESTS_ENVIRONMENT = \
    G_DEBUG=fatal_warnings,fatal_criticals

VALGRIND_TESTS_ENVIRONMENT = \
    G_SLICE=always-malloc \
    $(TESTS_ENVIRONMENT) \
    $(top_builddir)/tools/telepathy-glib-env \
    valgrind --tool=memcheck --leak-check=full --show-reachable=yes \
        --suppressions=$(top_srcdir)/examples/tp-glib-examples.supp \
        --child-silent-after-fork=yes --num-callers=20 --error-exitcode=42 \
        --gen-suppressions=all \
        $(VALGRIND_FLAGS)

check-valgrind:
	$(MAKE) check-TESTS \
		TESTS_ENVIRONMENT="$(VALGRIND_TESTS_ENVIRONMENT)" \
		TESTS="$(STATIC_TESTS) $(DYNAMIC_TESTS:%=.libs/%)"
	$(MAKE) -C dbus check-valgrind

EXTRA_DIST = README

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
