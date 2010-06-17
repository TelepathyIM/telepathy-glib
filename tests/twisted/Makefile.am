SUBDIRS = tools

TWISTED_TESTS = \
	channel_text/init.py \
	observer/init.py

TESTS =

TESTS_ENVIRONMENT = \
	PYTHONPATH=@abs_top_srcdir@/tests/twisted:@abs_top_builddir@/tests/twisted

# check-local: check-coding-style check-twisted
check-local: check-coding-style

# set to 6 when using refdbg, to give Gabble time to exit
CHECK_TWISTED_SLEEP=0

check-twisted:
	$(MAKE) -C tools
	rm -f tools/core
	rm -f tools/vgcore.*
	rm -f tools/tpl-testing.log
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

config.py: Makefile
	$(AM_V_GEN) { \
		echo "PACKAGE_STRING = \"$(PACKAGE_STRING)\""; \
		echo "DEBUGGING = $(DEBUGGING_PYBOOL)"; \
	} > $@

BUILT_SOURCES = config.py

EXTRA_DIST = \
	$(TWISTED_TESTS) \
	constants.py \
	tpltest.py \
	servicetest.py

noinst_PROGRAMS = \
	telepathy-logger-debug

telepathy_logger_debug_SOURCES = \
    main-debug.c	\
    ../tpl-channel-test.c

telepathy_logger_debug_LDADD = \
    $(top_builddir)/telepathy-logger/libtelepathy-logger.la \
    $(TPL_LIBS)

telepathy_logger_debug_LDFLAGS = -export-dynamic

AM_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/tests \
	-I$(top_builddir) \
	$(ERROR_CFLAGS)	\
	${TPL_CFLAGS}

CLEANFILES = tpl-[1-9]*.log *.pyc */*.pyc config.py

check_misc_sources = $(TESTS)

include $(top_srcdir)/tools/check-coding-style.mk
