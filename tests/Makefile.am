SUBDIRS = $(CHECKTWISTED) suppressions lib dbus

EXTRA_DIST = logs

noinst_PROGRAMS = \
	test-tpl-conf			\
	$(NULL)

TESTS = $(noinst_PROGRAMS)

LDADD = \
	$(top_builddir)/telepathy-logger/libtelepathy-logger.la \
	$(TPL_LIBS)

AM_CFLAGS = \
	$(ERROR_CFLAGS) \
	$(TPL_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(NULL)

TESTS_ENVIRONMENT = \
	G_DEBUG=fatal-warnings,fatal-criticals		\
	TPL_TEST_MODE=true 				\
	TEST_LOG_DIR=@abs_top_srcdir@/tests/logs	\
	$(top_srcdir)/tools/test-wrapper.sh		\
	$(NULL)

check-valgrind: $(TESTS)
	G_SLICE=always-malloc \
	G_DEBUG=gc-friendly \
	$(MAKE) \
		TESTS_ENVIRONMENT="$(TESTS_ENVIRONMENT) \
		libtool --mode=execute valgrind \
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
