if WANT_TWISTED_TESTS
CHECKTWISTED = twisted
endif

SUBDIRS = $(CHECKTWISTED) suppressions

LDADD = \
	$(top_builddir)/telepathy-logger/libtelepathy-logger.la \
	$(LIBTPL_LIBS)

noinst_PROGRAMS = \
	test-tpl-channel	\
	test-tpl-conf		\
	test-tpl-log-entry	\
	test-tpl-log-manager	\
	test-tpl-observer	\
	test-searches

test_tpl_channel_SOURCES = \
	test-tpl-channel.c	\
	tpl-channel-test.c

test_tpl_observer_SOURCES = \
	test-tpl-observer.c	\
	tpl-channel-test.c


AM_CFLAGS = $(ERROR_CFLAGS) $(LIBTPL_CFLAGS) \
    -I $(top_srcdir)/telepathy-logger -I $(top_builddir)/src

TESTS = $(noinst_PROGRAMS)

TESTS_ENVIRONMENT = G_DEBUG=fatal-warnings,fatal-criticals

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
	tpl-channel-test.c	\
	test-tpl-channel.c	\
	test-tpl-conf.c		\
	test-tpl-log-entry.c	\
	test-tpl-observer.c	\
	test-searches.c

include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style
