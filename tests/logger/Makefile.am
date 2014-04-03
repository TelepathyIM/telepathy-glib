# These tests aren't ready for parallel invocation yet: they all use
# the same log directory.
.NOTPARALLEL:

SUBDIRS = $(CHECKTWISTED) dbus

EXTRA_DIST = logs

noinst_PROGRAMS = \
	test-tpl-conf			\
	$(NULL)

TESTS = $(noinst_PROGRAMS)

LDADD = \
	$(top_builddir)/telepathy-logger/libtelepathy-logger-1.la \
	$(top_builddir)/telepathy-glib/libtelepathy-glib-1.la \
	$(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
	$(TPL_LIBS)

AM_CFLAGS = \
	$(ERROR_CFLAGS) \
	$(TPL_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(NULL)

AM_TESTS_ENVIRONMENT = \
	G_DEBUG=fatal-warnings,fatal-criticals		\
	TPL_TEST_MODE=true 				\
	XDG_CACHE_HOME=@abs_builddir@/tmp-cache \
	XDG_CONFIG_HOME=@abs_builddir@/tmp-config \
	XDG_DATA_HOME=@abs_top_srcdir@/tests/logger/logs \
	XDG_RUNTIME_DIR=@abs_builddir@/tmp-runtime \
	$(NULL)

check-valgrind: $(TESTS)
	G_SLICE=always-malloc \
	G_DEBUG=gc-friendly \
	$(MAKE) \
		LOG_COMPILER="libtool --mode=execute valgrind \
			--leak-check=full \
			--show-reachable=no \
			--gen-suppressions=all \
			--num-callers=20 \
			--suppressions=@abs_top_srcdir@/tests/suppressions/tpl.supp \
			--error-exitcode=1" \
		check-TESTS

check_c_sources = \
	$(dbus_test_sources)	\
	test-tpl-conf.c		\
	$(NULL)

include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

clean-local:
	rm -fr tmp-cache tmp-config tmp-runtime
