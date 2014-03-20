# These tests aren't ready for parallel invocation yet: they all use
# the same log directory.
.NOTPARALLEL:

noinst_PROGRAMS = \
	test-entity 			\
	test-log-manager		\
	test-tpl-log-store-pidgin 	\
	test-tpl-log-iter-pidgin	\
	test-tpl-log-store-sqlite	\
	test-tpl-log-store-xml 		\
	test-tpl-log-iter-xml		\
	test-tpl-log-walker		\
	test-tpl-observer		\
	$(NULL)

TESTS = $(noinst_PROGRAMS)

LDADD = \
	$(top_builddir)/telepathy-logger/libtelepathy-logger-1.la	\
	$(top_builddir)/tests/lib/libtp-logger-tests.la		\
	$(TPL_LIBS) 						\
	$(NULL)

test_entity_LDADD = \
	$(top_builddir)/tests/lib/libtp-logger-tests.la \
	$(LDADD) \
	$(NULL)

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

AM_CFLAGS = \
	$(ERROR_CFLAGS) \
	$(TPL_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/tests
	$(NULL)

AM_TESTS_ENVIRONMENT = \
    abs_top_builddir=@abs_top_builddir@				\
    TPL_TEST_MODE=true						\
    TPL_TEST_LOG_DIR=@abs_top_srcdir@/tests/logger/logs		\
    HOME=@abs_top_srcdir@/tests/logger/logs		\
    GSETTINGS_SCHEMA_DIR=@abs_srcdir@/data			\
    XDG_DATA_HOME=@abs_top_srcdir@/tests/logger/logs		\
    XDG_DATA_DIRS=@abs_srcdir@					\
    G_SLICE=debug-blocks					\
    TPL_DEBUG=all						\
    G_DEBUG=fatal_warnings,fatal_criticals$(maybe_gc_friendly)	\
    DBUS_SESSION_BUS_ADDRESS=this-is-clearly-not-valid		\
    DBUS_STARTER_ADDRESS=this-is-clearly-not-valid		\
    DBUS_STARTER_BUS_TYPE=this-is-clearly-not-valid		\
    $(NULL)

check-valgrind:
	$(MAKE) check-TESTS \
		maybe_gc_friendly=,gc-friendly \
		LOG_COMPILER="$(VALGRIND_LOG_COMPILER)"

include $(top_srcdir)/tools/valgrind.mk

VALGRIND_LOG_COMPILER = \
    env G_SLICE=always-malloc CHECK_VERBOSE=1	\
    $(top_builddir)/libtool --mode=execute	\
    $(VALGRIND) --suppressions=$(top_srcdir)/tests/suppressions/tpl.supp $(VALGRIND_FLAGS) \
    $(NULL)

CLEANFILES = $(BUILT_SOURCES)

distclean-local:
	rm -f capture-*.log
	rm -rf _gen
