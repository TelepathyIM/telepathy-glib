noinst_PROGRAMS = \
    test-call-cancellation \
    test-callable-example \
    test-channel-introspect \
    test-cli-group \
    test-cm \
    test-connection \
    test-connection-bug-18845 \
    test-connection-error \
    test-connection-handles \
    test-connection-inject-bug16307 \
    test-connection-getinterfaces-failure \
    test-contacts \
    test-contacts-bug-19101 \
    test-contacts-mixin \
    test-dbus \
    test-disconnection \
    test-error-enum \
    test-example-no-protocols \
    test-finalized-in-invalidated-handler \
    test-group-mixin \
    test-handle-set \
    test-invalidated-while-invoking-signals \
    test-message-mixin \
    test-properties \
    test-self-handle \
    test-self-presence \
    test-text-mixin \
    test-text-respawn \
    test-unsupported-interface

TESTS = $(noinst_PROGRAMS)

LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la \
    $(top_builddir)/tests/lib/libtp-glib-tests.la

test_call_cancellation_SOURCES = call-cancellation.c

test_channel_introspect_SOURCES = channel-introspect.c

test_callable_example_SOURCES = callable-example.c
test_callable_example_LDADD = \
    $(LDADD) \
    $(top_builddir)/examples/cm/callable/libexample-cm-callable.la

test_cli_group_SOURCES = cli-group.c

test_cm_SOURCES = cm.c
test_cm_LDADD = \
    $(LDADD) \
    $(top_builddir)/examples/cm/echo/libexample-cm-echo.la

test_connection_SOURCES = connection.c

test_connection_bug_18845_SOURCES = connection-bug-18845.c

test_connection_handles_SOURCES = connection-handles.c

test_connection_error_SOURCES = connection-error.c

test_connection_inject_bug16307_SOURCES = connection-inject-bug16307.c

test_connection_getinterfaces_failure_SOURCES = \
    connection-getinterfaces-failure.c

test_contacts_SOURCES = contacts.c

test_contacts_bug_19101_SOURCES = contacts-bug-19101.c

test_contacts_mixin_SOURCES = contacts-mixin.c

test_dbus_SOURCES = dbus.c

test_disconnection_SOURCES = disconnection.c

test_error_enum_SOURCES = error-enum.c
nodist_test_error_enum_SOURCES = _gen/errors-check.h

test_example_no_protocols_SOURCES = example-no-protocols.c

test_finalized_in_invalidated_handler_SOURCES = \
    finalized-in-invalidated-handler.c

test_group_mixin_SOURCES = group-mixin.c

test_handle_set_SOURCES = handle-set.c

test_invalidated_while_invoking_signals_SOURCES = \
    invalidated-while-invoking-signals.c

test_message_mixin_LDADD = \
    $(LDADD) \
    $(top_builddir)/examples/cm/echo-message-parts/libexample-cm-echo-2.la
test_message_mixin_SOURCES = \
    message-mixin.c

test_properties_SOURCES = properties.c
nodist_test_properties_SOURCES = \
    _gen/svc.h \
    _gen/svc.c

test_self_handle_SOURCES = self-handle.c

test_self_presence_SOURCES = self-presence.c

test_text_mixin_LDADD = \
    $(LDADD) \
    $(top_builddir)/examples/cm/echo/libexample-cm-echo.la
test_text_mixin_SOURCES = text-mixin.c

test_text_respawn_LDADD = \
    $(LDADD) \
    $(top_builddir)/examples/cm/echo/libexample-cm-echo.la
test_text_respawn_SOURCES = text-respawn.c

test_unsupported_interface_SOURCES = unsupported-interface.c

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)

TESTS_ENVIRONMENT = \
    abs_top_builddir=@abs_top_builddir@ \
    XDG_DATA_HOME=@abs_builddir@ \
    XDG_DATA_DIRS=@abs_srcdir@ \
    G_DEBUG=fatal_warnings,fatal_criticals

check-valgrind:
	$(MAKE) check-TESTS \
		TESTS_ENVIRONMENT="$(VALGRIND_TESTS_ENVIRONMENT)"

VALGRIND_TESTS_ENVIRONMENT = \
    G_SLICE=always-malloc \
    $(TESTS_ENVIRONMENT) \
    $(top_builddir)/libtool --mode=execute \
    valgrind --tool=memcheck --leak-check=full --show-reachable=yes \
        --suppressions=$(top_srcdir)/examples/tp-glib-examples.supp \
        --child-silent-after-fork=yes --num-callers=20 --error-exitcode=42 \
        --gen-suppressions=all \
        $(VALGRIND_FLAGS)

BUILT_SOURCES = \
	_gen/errors-check.h \
	_gen/svc.h \
	_gen/svc.c

CLEANFILES = \
    $(BUILT_SOURCES)

distclean-local:
	rm -rf _gen

EXTRA_DIST = \
    dbus-1/services/spurious.service \
    telepathy/managers/spurious.manager \
    telepathy/managers/test_manager_file.manager \
    telepathy/managers/test_manager_file_invalid.manager \
    with-properties.xml

_gen/svc.c _gen/svc.h: with-properties.xml \
	$(top_srcdir)/tools/glib-ginterface-gen.py \
	Makefile.am
	$(mkdir_p) _gen
	$(PYTHON) $(top_srcdir)/tools/glib-ginterface-gen.py \
		--filename=_gen/svc \
		--signal-marshal-prefix=NOT_NEEDED \
		$< Test_Svc_

_gen/errors-check.h: $(top_srcdir)/spec/errors.xml \
	$(top_srcdir)/tools/glib-errors-check-gen.py
	$(mkdir_p) _gen
	$(PYTHON) $(top_srcdir)/tools/glib-errors-check-gen.py $< > $@
