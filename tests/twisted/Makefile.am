TWISTED_TESTS = \
	avatar-requirements.py \
	caps_helper.py \
	gateways.py \
	text/initiate.py \
	text/initiate-requestotron.py \
	text/destroy.py \
	text/ensure.py \
	text/respawn.py \
	text/send-error.py \
	text/send-to-correct-resource.py \
	text/test-chat-state.py \
	text/test-text-delayed.py \
	text/test-text-no-body.py \
	text/test-text.py \
	test-debug.py \
	test-disco.py \
	test-disco-no-reply.py \
	test-fallback-socks5-proxy.py \
	test-register.py \
	test-set-status-idempotence.py \
	test-location.py \
	pubsub.py \
	sidecars.py

TESTS =

TESTS_ENVIRONMENT = \
	PYTHONPATH=@abs_top_srcdir@/tests/twisted:@abs_top_builddir@/tests/twisted

check-local: check-coding-style check-twisted

# set to 6 when using refdbg, to give Gabble time to exit
CHECK_TWISTED_SLEEP=0

check-twisted:
	$(MAKE) -C tools
	rm -f tools/core
	rm -f tools/vgcore.*
	rm -f tools/gabble-testing.log
	rm -f tools/strace.log
	if test -n "$$GABBLE_TEST_REFDBG"; then \
	  sleep=6; \
        else \
	  sleep=$(CHECK_TWISTED_SLEEP); \
	fi; \
	sh $(srcdir)/tools/with-session-bus.sh \
		--sleep=$$sleep \
		--config-file=tools/tmp-session-bus.conf \
		-- $(MAKE) check-TESTS \
		TESTS="$(TWISTED_TESTS)" \
		TESTS_ENVIRONMENT="$(TESTS_ENVIRONMENT) $(TEST_PYTHON) -u"
	@if test -e tools/core; then\
		echo "Core dump exists: tools/core";\
		exit 1;\
	fi

if ENABLE_DEBUG
DEBUGGING_PYBOOL = True
else
DEBUGGING_PYBOOL = False
endif

if ENABLE_PLUGINS
PLUGINS_ENABLED_PYBOOL = True
else
PLUGINS_ENABLED_PYBOOL = False
endif

if ENABLE_CHANNEL_TYPE_CALL
CHANNEL_TYPE_CALL_ENABLED_PYBOOL = True
else
CHANNEL_TYPE_CALL_ENABLED_PYBOOL = False
endif

config.py: Makefile
	$(AM_V_GEN) { \
		echo "PACKAGE_STRING = \"$(PACKAGE_STRING)\""; \
		echo "DEBUGGING = $(DEBUGGING_PYBOOL)"; \
		echo "PLUGINS_ENABLED = $(PLUGINS_ENABLED_PYBOOL)"; \
		echo "CHANNEL_TYPE_CALL_ENABLED = $(CHANNEL_TYPE_CALL_ENABLED_PYBOOL)"; \
	} > $@

BUILT_SOURCES = config.py

EXTRA_DIST = \
	$(TWISTED_TESTS) \
	bytestream.py \
	constants.py \
	gabbletest.py \
	httptest.py \
	servicetest.py \
	jingle/jingletest.py \
	jingle/jingletest2.py \
	mucutil.py \
	ns.py \
	olpc/util.py \
	tubes/muctubeutil.py \
	tubes/tubetestutil.py \
	search/search_helper.py \
	file-transfer/file_transfer_helper.py

noinst_PROGRAMS = \
	telepathy-gabble-debug

telepathy_gabble_debug_SOURCES = \
    main-debug.c \
    test-resolver.c \
    test-resolver.h

telepathy_gabble_debug_LDADD = \
    $(top_builddir)/src/libgabble-convenience.la \
    $(ALL_LIBS)

telepathy_gabble_debug_LDFLAGS = -export-dynamic

AM_CFLAGS = $(ERROR_CFLAGS) @DBUS_CFLAGS@ @GLIB_CFLAGS@ @WOCKY_CFLAGS@ \
    @TP_GLIB_CFLAGS@ \
    -I $(top_srcdir)/src -I $(top_builddir)/src \
     -I $(top_srcdir)/lib -I $(top_builddir)/lib \
    -I $(top_srcdir) -I $(top_builddir)
ALL_LIBS = @DBUS_LIBS@ @GLIB_LIBS@ @WOCKY_LIBS@ @TP_GLIB_LIBS@

CLEANFILES = gabble-[1-9]*.log *.pyc */*.pyc config.py

check_misc_sources = $(TESTS)

include $(top_srcdir)/tools/check-coding-style.mk

SUBDIRS = tools
