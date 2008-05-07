noinst_PROGRAMS = \
    test-call-cancellation \
    test-channel-introspect \
    test-connection \
    test-connection-getinterfaces-failure \
    test-dbus \
    test-disconnection \
    test-example-no-protocols \
    test-finalized-in-invalidated-handler \
    test-handle-set \
    test-invalidated-while-invoking-signals \
    test-properties \
    test-unsupported-interface

TESTS = $(noinst_PROGRAMS)

test_call_cancellation_LDADD = \
    $(TP_GLIB_LIBS) \
    ../lib/libtp-glib-tests.la

test_channel_introspect_LDADD = \
    $(TP_GLIB_LIBS) \
    ../lib/libtp-glib-tests.la
test_channel_introspect_SOURCES = channel-introspect.c

test_connection_LDADD = \
    $(TP_GLIB_LIBS) \
    ../lib/libtp-glib-tests.la
test_connection_SOURCES = connection.c

test_connection_getinterfaces_failure_LDADD = \
    $(TP_GLIB_LIBS) \
    ../lib/libtp-glib-tests.la
test_connection_getinterfaces_failure_SOURCES = \
    connection-getinterfaces-failure.c

test_dbus_LDADD = $(TP_GLIB_LIBS)
test_disconnection_LDADD = \
    $(TP_GLIB_LIBS) \
    ../lib/libtp-glib-tests.la
test_example_no_protocols_LDADD = $(TP_GLIB_LIBS)

test_finalized_in_invalidated_handler_SOURCES = \
    finalized-in-invalidated-handler.c
test_finalized_in_invalidated_handler_LDADD = \
    $(TP_GLIB_LIBS) \
    ../lib/libtp-glib-tests.la

test_handle_set_SOURCES = handle-set.c
test_handle_set_LDADD = $(TP_GLIB_LIBS)

test_invalidated_while_invoking_signals_LDADD = \
    $(TP_GLIB_LIBS) \
    ../lib/libtp-glib-tests.la
test_invalidated_while_invoking_signals_SOURCES = \
    invalidated-while-invoking-signals.c

test_properties_LDADD = $(TP_GLIB_LIBS)
test_properties_SOURCES = \
    properties.c \
    _gen/svc.h \
    _gen/svc.c

test_unsupported_interface_LDADD = $(TP_GLIB_LIBS)
test_unsupported_interface_SOURCES = unsupported-interface.c

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)

TESTS_ENV = \
    abs_top_builddir=@abs_top_builddir@ \
    XDG_DATA_HOME=@abs_builddir@ \
    XDG_DATA_DIRS=@abs_srcdir@ \
    G_DEBUG=fatal_warnings,fatal_criticals

TESTS_ENVIRONMENT = \
    $(TESTS_ENV) \
    sh $(top_srcdir)/tools/with-session-bus.sh --session --

check-valgrind:
	$(MAKE) check-TESTS \
		TESTS_ENVIRONMENT="$(VALGRIND_TESTS_ENVIRONMENT)" \
		TESTS="$(TESTS:%=.libs/%)"

VALGRIND_TESTS_ENVIRONMENT = \
    G_SLICE=always-malloc \
    $(TESTS_ENV) \
    $(top_builddir)/tools/telepathy-glib-env \
    sh $(top_srcdir)/tools/with-session-bus.sh --session -- \
    valgrind --tool=memcheck --leak-check=full --show-reachable=yes \
        --suppressions=$(top_srcdir)/examples/tp-glib-examples.supp \
        --child-silent-after-fork=yes --num-callers=20 --error-exitcode=42 \
        --gen-suppressions=all \
        $(VALGRIND_FLAGS)

BUILT_SOURCES = \
	_gen/svc.h \
	_gen/svc.c

CLEANFILES = \
    $(BUILT_SOURCES)

distclean-local:
	rm -rf _gen

EXTRA_DIST = with-properties.xml

_gen/svc.c _gen/svc.h: with-properties.xml \
	$(top_srcdir)/tools/glib-ginterface-gen.py \
	Makefile.am
	$(mkdir_p) _gen
	$(PYTHON) $(top_srcdir)/tools/glib-ginterface-gen.py \
		--filename=_gen/svc \
		--signal-marshal-prefix=NOT_NEEDED \
		$< Test_Svc_
