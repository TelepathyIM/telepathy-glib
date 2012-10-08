SUBDIRS = \
    lib \
    . \
    dbus \
    tools

programs_list = \
    test-asv \
    test-capabilities \
    test-availability-cmp \
    test-dtmf-player \
    test-enums \
    test-gnio-util \
    test-heap \
    test-internal-debug \
    test-intset \
    test-signal-connect-object \
    test-util \
    test-debug-domain \
    test-contact-search-result \
    $(NULL)

if HAVE_CXX
programs_list += \
    test-util-cxx
endif

if ENABLE_INSTALLED_TESTS
testsdir = @tpglibtestsdir@
tests_PROGRAMS = $(programs_list)
tests_DATA = tpglib-tests.list
else
noinst_PROGRAMS = $(programs_list)
endif

tpglib-tests.list:
	$(AM_V_GEN)echo $(programs_list) > $@

TESTS = $(programs_list) \
    all-errors-documented.py

TESTS_ENVIRONMENT = \
    abs_top_builddir=@abs_top_builddir@ \
    abs_top_srcdir=@abs_top_srcdir@ \
    G_SLICE=debug-blocks \
    G_DEBUG=fatal_warnings,fatal_criticals$(maybe_gc_friendly) \
    G_MESSAGES_DEBUG=all \
    PYTHONPATH=@abs_top_srcdir@/tools \
    $(top_srcdir)/tools/test-wrapper.sh \
    $(EXTRA_TESTS_ENVIRONMENT)

EXTRA_TESTS_ENVIRONMENT =

include $(top_srcdir)/tools/valgrind.mk

VALGRIND_TESTS_ENVIRONMENT = \
    $(TESTS_ENVIRONMENT) \
    env G_SLICE=always-malloc CHECK_VERBOSE=1 \
    $(top_builddir)/libtool --mode=execute \
    $(VALGRIND) --suppressions=$(top_srcdir)/tests/tests.supp $(VALGRIND_FLAGS)

check-valgrind:
	$(MAKE) check-TESTS \
		maybe_gc_friendly=,gc-friendly \
		TESTS='$$(programs_list)' \
		TESTS_ENVIRONMENT="$(VALGRIND_TESTS_ENVIRONMENT)"
	$(MAKE) -C dbus check-valgrind

EXTRA_DIST = README \
    all-errors-documented.py \
    tests.supp

# this one uses internal ABI
test_asv_SOURCES = \
    asv.c
test_asv_LDADD = \
    $(top_builddir)/tests/lib/libtp-glib-tests-internal.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-internal.la \
    $(GLIB_LIBS)

# this one uses internal ABI
test_capabilities_SOURCES = \
    capabilities.c
test_capabilities_LDADD = \
    $(top_builddir)/tests/lib/libtp-glib-tests-internal.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-internal.la \
    $(GLIB_LIBS)

# this one uses internal ABI
test_contact_search_result_SOURCES = \
    contact-search-result.c
test_contact_search_result_LDADD = \
    $(top_builddir)/tests/lib/libtp-glib-tests-internal.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-internal.la \
    $(GLIB_LIBS)

test_dtmf_player_SOURCES = dtmf-player.c
test_dtmf_player_LDADD = \
    $(top_builddir)/tests/lib/libtp-glib-tests.la \
    $(LDADD)

test_heap_SOURCES = \
    heap.c

test_gnio_util_SOURCES = \
    gnio-util.c

test_util_SOURCES = \
    util.c

test_intset_SOURCES = \
    intset.c

test_availability_cmp_SOURCES = \
    availability-cmp.c

test_enums_SOURCES = \
    enums.c

test_debug_domain_SOURCES = \
    debug-domain.c

test_internal_debug_SOURCES = \
    internal-debug.c

test_signal_connect_object_SOURCES = \
    signal-connect-object.c
test_signal_connect_object_LDADD = \
    $(top_builddir)/tests/lib/libtp-glib-tests.la \
    $(LDADD)

# this needs to link against the static convenience library so that
# _tp_log is still visible
test_debug_domain_LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-internal.la
test_internal_debug_LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-internal.la

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la \
    $(GLIB_LIBS) \
    $(DBUS_LIBS) \
    $(NULL)

AM_CPPFLAGS = \
    -I${top_srcdir} -I${top_builddir} \
    -D_TP_COMPILATION \
    -D_TP_IGNORE_DEPRECATIONS \
    $(GLIB_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(NULL)
AM_LDFLAGS = \
    $(ERROR_LDFLAGS) \
    $(NULL)

AM_CFLAGS = $(ERROR_CFLAGS)
AM_CXXFLAGS = $(ERROR_CXXFLAGS)

if HAVE_CXX
test_util_cxx_SOURCES = util-cxx.cpp
endif
