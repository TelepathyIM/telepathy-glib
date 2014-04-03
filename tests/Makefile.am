SUBDIRS = \
    lib \
    . \
    dbus \
    suppressions \
    tools

if ENABLE_LOGGER
SUBDIRS += logger
endif

programs_list = \
    test-asv \
    test-capabilities \
    test-channel-filter \
    test-availability-cmp \
    test-dtmf-player \
    test-enums \
    test-gnio-util \
    test-heap \
    test-internal-debug \
    test-intset \
    test-message \
    test-signal-connect-object \
    test-util \
    test-debug-domain \
    test-contact-search-result \
    $(NULL)

if HAVE_CXX
programs_list += \
    test-util-cxx
endif

CLEANFILES =

if ENABLE_INSTALLED_TESTS
testsdir = @tpglibtestsdir@
tests_PROGRAMS = $(programs_list)
tests_DATA = tpglib-tests.list
CLEANFILES += $(tests_DATA)

insttestdir = ${datadir}/installed-tests/telepathy-glib
insttest_DATA = $(patsubst %,%.test,$(programs_list))
CLEANFILES += $(insttest_DATA)

else
noinst_PROGRAMS = $(programs_list)
endif

%.test: Makefile
	$(AM_V_GEN)( echo '[Test]'; \
		echo 'Exec=${testsdir}/$* --tap'; \
		echo 'Type=session'; \
		echo 'Output=TAP'; \
	) > $@.tmp
	@mv $@.tmp $@

tpglib-tests.list: Makefile
	$(AM_V_GEN)echo $(programs_list) > $@

TESTS = $(programs_list) \
    all-errors-documented.py

AM_TESTS_ENVIRONMENT = \
    abs_top_builddir=@abs_top_builddir@ \
    abs_top_srcdir=@abs_top_srcdir@ \
    G_SLICE=debug-blocks \
    G_DEBUG=fatal_warnings,fatal_criticals$(maybe_gc_friendly) \
    G_MESSAGES_DEBUG=all \
    PYTHONPATH=@abs_top_srcdir@/tools \
    XDG_CACHE_HOME=@abs_builddir@/tmp-cache \
    XDG_CONFIG_HOME=@abs_builddir@/tmp-config \
    XDG_DATA_HOME=@abs_builddir@/tmp-data \
    XDG_RUNTIME_DIR=@abs_builddir@/tmp-runtime \
    $(NULL)
LOG_COMPILER =

include $(top_srcdir)/tools/valgrind.mk

VALGRIND_LOG_COMPILER = \
    env G_SLICE=always-malloc CHECK_VERBOSE=1 \
    $(top_builddir)/libtool --mode=execute \
    $(VALGRIND) --suppressions=$(top_srcdir)/tests/tests.supp $(VALGRIND_FLAGS)

check-valgrind:
	$(MAKE) check-TESTS \
		maybe_gc_friendly=,gc-friendly \
		TESTS='$$(programs_list)' \
		LOG_COMPILER="$(VALGRIND_LOG_COMPILER)"
	$(MAKE) -C dbus check-valgrind

EXTRA_DIST = README \
    all-errors-documented.py \
    tests.supp

# this one uses internal ABI
test_asv_SOURCES = \
    asv.c
test_asv_LDADD = \
    $(top_builddir)/tests/lib/libtp-glib-tests-internal.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-main-internal.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
    $(GLIB_LIBS)

# this one uses internal ABI
test_capabilities_SOURCES = \
    capabilities.c
test_capabilities_LDADD = \
    $(top_builddir)/tests/lib/libtp-glib-tests-internal.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-main-internal.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
    $(GLIB_LIBS)

# this one uses internal ABI
test_channel_filter_SOURCES = \
    channel-filter.c
test_channel_filter_LDADD = \
    $(top_builddir)/tests/lib/libtp-glib-tests-internal.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-main-internal.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
    $(GLIB_LIBS)

# this one uses internal ABI
test_contact_search_result_SOURCES = \
    contact-search-result.c
test_contact_search_result_LDADD = \
    $(top_builddir)/tests/lib/libtp-glib-tests-internal.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-main-internal.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
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

test_message_SOURCES = \
    message.c
test_message_LDADD = \
    $(top_builddir)/tests/lib/libtp-glib-tests.la \
    $(LDADD)

test_signal_connect_object_SOURCES = \
    signal-connect-object.c
test_signal_connect_object_LDADD = \
    $(top_builddir)/tests/lib/libtp-glib-tests.la \
    $(LDADD)

# this needs to link against the static convenience library so that
# _tp_log is still visible
test_debug_domain_LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-main-internal.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
    $(NULL)
test_internal_debug_LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-main-internal.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
    $(NULL)

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
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

clean-local:
	rm -fr tmp-cache tmp-config tmp-data tmp-runtime
