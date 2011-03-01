noinst_PROGRAMS = \
	test-entity 			\
	test-log-manager		\
	test-tpl-log-store-pidgin 	\
	test-tpl-log-store-sqlite	\
	test-tpl-log-store-xml 		\
	test-tpl-observer		\
	$(NULL)

TESTS = $(noinst_PROGRAMS)

LDADD = \
	$(top_builddir)/tests/lib/libtp-logger-tests.la		\
	$(top_builddir)/telepathy-logger/libtelepathy-logger.la	\
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

TESTS_ENVIRONMENT = \
    abs_top_builddir=@abs_top_builddir@				\
    TPL_TEST_MODE=true						\
    TPL_TEST_LOG_DIR=@abs_top_srcdir@/tests/logs		\
    GSETTINGS_SCHEMA_DIR=@abs_srcdir@/data			\
    XDG_DATA_HOME=@abs_top_builddir@/tests/logs			\
    XDG_DATA_DIRS=@abs_srcdir@					\
    G_SLICE=debug-blocks					\
    TPL_DEBUG=all						\
    G_DEBUG=fatal_warnings,fatal_criticals$(maybe_gc_friendly)	\
    $(top_srcdir)/tools/test-wrapper.sh				\
    sh $(top_srcdir)/tools/with-session-bus.sh			\
        --config-file=dbus-1/session.conf --			\
    $(EXTRA_TESTS_ENVIRONMENT)

EXTRA_TESTS_ENVIRONMENT =

check-valgrind:
	$(MAKE) check-TESTS \
		maybe_gc_friendly=,gc-friendly \
		TESTS_ENVIRONMENT="$(VALGRIND_TESTS_ENVIRONMENT)"

include $(top_srcdir)/tools/valgrind.mk

VALGRIND_TESTS_ENVIRONMENT = \
    $(TESTS_ENVIRONMENT)			\
    env G_SLICE=always-malloc CHECK_VERBOSE=1	\
    $(top_builddir)/libtool --mode=execute	\
    $(VALGRIND) --suppressions=$(top_srcdir)/tests/suppressions/tpl.supp $(VALGRIND_FLAGS)

BUILT_SOURCES = \
	dbus-1/session.conf \
	$(NULL)

CLEANFILES = $(BUILT_SOURCES)

distclean-local:
	rm -f capture-*.log
	rm -rf _gen

EXTRA_DIST = \
    dbus-1/session.conf.in 	\
    $(NULL)

dbus-1/%.conf: $(srcdir)/dbus-1/%.conf.in
	$(AM_V_at)$(mkdir_p) dbus-1
	$(AM_V_GEN)sed -e "s|[@]abs_top_builddir[@]|@abs_top_builddir@|g" $< > $@
