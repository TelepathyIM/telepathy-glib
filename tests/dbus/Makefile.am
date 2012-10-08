tests_list = \
    test-account \
    test-account-channel-request \
    test-account-manager \
    test-account-request \
    test-base-client \
    test-call-cancellation \
    test-call-channel \
    test-channel \
    test-channel-dispatcher \
    test-channel-dispatch-operation \
    test-channel-introspect \
    test-channel-request \
    test-channel-manager-request-properties \
    test-cli-group \
    test-client \
    test-client-channel-factory \
    test-cm \
    test-cm-message \
    test-connection \
    test-connection-aliasing \
    test-connection-balance \
    test-connection-bug-18845 \
    test-connection-error \
    test-connection-handles \
    test-connection-inject-bug16307 \
    test-connection-interests \
    test-connection-getinterfaces-failure \
    test-contact-lists \
    test-contact-list-client \
    test-contacts \
    test-contacts-bug-19101 \
    test-contacts-mixin \
    test-contacts-slow-path \
    test-dbus \
    test-dbus-tube \
    test-debug-client \
    test-disconnection \
    test-error-enum \
    test-example-no-protocols \
    test-file-transfer-channel \
    test-finalized-in-invalidated-handler \
    test-get-interface-after-invalidate \
    test-group-mixin \
    test-handle-repo \
    test-handle-set \
    test-invalidated-while-invoking-signals \
    test-long-connection-name \
    test-message-mixin \
    test-params-cm \
    test-properties \
    test-protocol-objects \
    test-proxy-preparation \
    test-room-list \
    test-self-handle \
    test-self-presence \
    test-simple-approver \
    test-simple-handler \
    test-simple-observer \
    test-stream-tube \
    test-text-channel \
    test-text-mixin \
    test-text-respawn \
    test-tls-certificate \
    test-unsupported-interface

if ENABLE_INSTALLED_TESTS
managersdir = @tpglibtestsdir@/telepathy/managers/
managers_DATA = \
	telepathy/managers/spurious.manager \
	telepathy/managers/test_manager_file_invalid.manager \
	telepathy/managers/test_manager_file.manager \
	$(NULL)

testsdir = @tpglibtestsdir@
tests_PROGRAMS = $(tests_list)
tests_SCRIPTS = run-test.sh
tests_DATA = tpglib-dbus-tests.list
else
noinst_PROGRAMS = $(tests_list)
endif

TESTS = $(tests_list)

tpglib-dbus-tests.list:
	$(AM_V_GEN)echo $(tests_list) > $@

LDADD = \
    $(top_builddir)/tests/lib/libtp-glib-tests.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la \
    $(GLIB_LIBS) \
    $(DBUS_LIBS) \
    $(NULL)

test_account_SOURCES = account.c

# this one uses internal ABI
test_account_channel_request_SOURCES = account-channel-request.c
test_account_channel_request_LDADD = \
    $(top_builddir)/tests/lib/libtp-glib-tests-internal.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-internal.la \
    $(GLIB_LIBS)

test_account_request_SOURCES = account-request.c

test_account_manager_SOURCES = account-manager.c

test_base_client_SOURCES = base-client.c

test_call_cancellation_SOURCES = call-cancellation.c

test_channel_SOURCES = channel.c

test_channel_dispatcher_SOURCES = channel-dispatcher.c

test_channel_dispatch_operation_SOURCES = channel-dispatch-operation.c

test_channel_introspect_SOURCES = channel-introspect.c

test_channel_request_SOURCES = channel-request.c

test_call_channel_SOURCES = call-channel.c
test_call_channel_LDADD = \
    $(LDADD) \
    $(top_builddir)/examples/cm/call/libexample-cm-call.la

test_client_SOURCES = client.c

test_cli_group_SOURCES = cli-group.c

test_cm_SOURCES = cm.c

test_connection_SOURCES = connection.c

test_contact_lists_SOURCES = contact-lists.c
test_contact_lists_LDADD = \
    $(LDADD) \
    $(top_builddir)/examples/cm/contactlist/libexample-cm-contactlist.la

test_contact_list_client_SOURCES = contact-list-client.c
test_contact_list_client_LDADD = \
    $(LDADD) \
    $(top_builddir)/examples/cm/contactlist/libexample-cm-contactlist.la

test_connection_aliasing_SOURCES = connection-aliasing.c
test_connection_aliasing_LDADD = \
    $(LDADD) \
    $(top_builddir)/examples/cm/contactlist/libexample-cm-contactlist.la

test_connection_balance_SOURCES = connection-balance.c

test_connection_bug_18845_SOURCES = connection-bug-18845.c

test_connection_handles_SOURCES = connection-handles.c

test_connection_error_SOURCES = connection-error.c

test_connection_inject_bug16307_SOURCES = connection-inject-bug16307.c

test_connection_interests_SOURCES = connection-interests.c

test_connection_getinterfaces_failure_SOURCES = \
    connection-getinterfaces-failure.c

test_contacts_SOURCES = contacts.c

test_contacts_bug_19101_SOURCES = contacts-bug-19101.c

test_contacts_mixin_SOURCES = contacts-mixin.c

test_contacts_slow_path_SOURCES = contacts-slow-path.c

test_dbus_SOURCES = dbus.c

test_disconnection_SOURCES = disconnection.c

test_error_enum_SOURCES = error-enum.c
nodist_test_error_enum_SOURCES = _gen/errors-check.h

test_example_no_protocols_SOURCES = example-no-protocols.c

test_file_transfer_channel_SOURCES = file-transfer-channel.c

test_finalized_in_invalidated_handler_SOURCES = \
    finalized-in-invalidated-handler.c

test_get_interface_after_invalidate_SOURCES = \
    get-interface-after-invalidate.c

test_group_mixin_SOURCES = group-mixin.c

test_handle_repo_SOURCES = handle-repo.c

test_handle_set_SOURCES = handle-set.c

test_invalidated_while_invoking_signals_SOURCES = \
    invalidated-while-invoking-signals.c

test_long_connection_name_SOURCES = long-connection-name.c

test_message_mixin_LDADD = \
    $(LDADD) \
    $(top_builddir)/examples/cm/echo-message-parts/libexample-cm-echo-2.la
test_message_mixin_SOURCES = \
    message-mixin.c

test_properties_SOURCES = properties.c
nodist_test_properties_SOURCES = \
    _gen/svc.h \
    _gen/svc.c

test_protocol_objects_LDADD = \
    $(LDADD) \
    $(top_builddir)/examples/cm/echo-message-parts/libexample-cm-echo-2.la
test_protocol_objects_SOURCES = protocol-objects.c

test_self_handle_SOURCES = self-handle.c
test_self_handle_LDADD = \
    $(top_builddir)/tests/lib/libtp-glib-tests-internal.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-internal.la \
    $(GLIB_LIBS) \
    $(NULL)

test_self_presence_SOURCES = self-presence.c

test_text_mixin_SOURCES = text-mixin.c

test_text_respawn_SOURCES = text-respawn.c

test_unsupported_interface_SOURCES = unsupported-interface.c

test_params_cm_SOURCES = \
    params-cm.c

test_simple_observer_SOURCES = simple-observer.c

test_simple_approver_SOURCES = simple-approver.c

test_simple_handler_SOURCES = simple-handler.c

test_stream_tube_SOURCES = stream-tube.c

test_client_channel_factory_SOURCES = client-channel-factory.c

test_proxy_preparation_SOURCES = proxy-preparation.c

test_channel_manager_request_properties_SOURCES = channel-manager-request-properties.c

test_dbus_tube_SOURCES = dbus-tube.c

# this one uses internal ABI
test_cm_message_SOURCES = \
    cm-message.c
test_cm_message_LDADD = \
    $(top_builddir)/tests/lib/libtp-glib-tests-internal.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-internal.la \
    $(GLIB_LIBS)

test_text_channel_SOURCES = text-channel.c

test_text_channel_LDADD = \
    $(LDADD) \
    $(top_builddir)/examples/cm/echo-message-parts/libexample-cm-echo-2.la

test_debug_client_SOURCES = debug-client.c

test_room_list_SOURCES = room-list.c

test_tls_certificate_SOURCES = tls-certificate.c

check_c_sources = *.c
include $(top_srcdir)/tools/check-coding-style.mk
check-local: check-coding-style

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

TESTS_ENVIRONMENT = \
    abs_top_builddir=@abs_top_builddir@ \
    XDG_DATA_HOME=@abs_builddir@ \
    XDG_DATA_DIRS=@abs_srcdir@:$${XDG_DATA_DIRS:=/usr/local/share:/usr/share} \
    G_SLICE=debug-blocks \
    G_DEBUG=fatal_warnings,fatal_criticals$(maybe_gc_friendly) \
    G_MESSAGES_DEBUG=all \
    $(top_srcdir)/tools/test-wrapper.sh \
    sh $(top_srcdir)/tools/with-session-bus.sh \
        --config-file=dbus-uninstalled/session.conf -- \
    $(EXTRA_TESTS_ENVIRONMENT)

EXTRA_TESTS_ENVIRONMENT =

check-valgrind:
	$(MAKE) check-TESTS \
		maybe_gc_friendly=,gc-friendly \
		TESTS_ENVIRONMENT="$(VALGRIND_TESTS_ENVIRONMENT)"

include $(top_srcdir)/tools/valgrind.mk

VALGRIND_TESTS_ENVIRONMENT = \
    $(TESTS_ENVIRONMENT) \
    env G_SLICE=always-malloc CHECK_VERBOSE=1 \
    $(top_builddir)/libtool --mode=execute \
    $(VALGRIND) --suppressions=$(top_srcdir)/tests/tests.supp $(VALGRIND_FLAGS)

BUILT_SOURCES = \
	dbus-installed/session.conf \
	dbus-uninstalled/session.conf \
	_gen/errors-check.h \
	_gen/svc.h \
	_gen/svc.c \
	run-test.sh \
	$(NULL)

CLEANFILES = \
    $(BUILT_SOURCES)

distclean-local:
	rm -f capture-*.log
	rm -rf _gen

EXTRA_DIST = \
    dbus-installed/session.conf.in \
    dbus-installed/services/spurious.service \
    dbus-uninstalled/session.conf.in \
    dbus-uninstalled/services/spurious.service \
    telepathy/managers/spurious.manager \
    telepathy/managers/test_manager_file.manager \
    telepathy/managers/test_manager_file_invalid.manager \
    with-properties.xml \
    run-test.sh.in \
    $(NULL)

run-test.sh: run-test.sh.in
	$(AM_V_GEN)sed \
		-e "s|[@]tpglibtestsdir[@]|@tpglibtestsdir@|g" \
		-e "s|[@]libexec[@]|${libexecdir}|g" \
		$< > $@
	@chmod +x $@

dbus-uninstalled/%.conf: $(srcdir)/dbus-uninstalled/%.conf.in
	$(AM_V_at)$(mkdir_p) dbus-uninstalled
	$(AM_V_GEN)sed \
		-e "s|[@]abs_top_builddir[@]|@abs_top_builddir@|g" \
		-e "s|[@]abs_top_srcdir[@]|@abs_top_srcdir@|g" \
		$< > $@

dbus-installed/%.conf: $(srcdir)/dbus-installed/%.conf.in
	$(AM_V_at)$(mkdir_p) dbus-installed
	$(AM_V_GEN)sed -e "s|[@]tpglibtestsdir[@]|@tpglibtestsdir@|g" $< > $@

service_files = dbus-installed/services/spurious.service
conf_in_files = dbus-installed/session.conf.in
conf_files = $(conf_in_files:.conf.in=.conf)

if ENABLE_INSTALLED_TESTS
dbusservicedir = @tpglibtestsdir@/dbus-installed/services
dbusservice_DATA = $(service_files)

dbusconfigdir = @tpglibtestsdir@/dbus-installed
dbusconfig_DATA = $(conf_files)
endif

_gen/svc.h: _gen/svc.c
	@:

_gen/svc.c: with-properties.xml \
	$(top_srcdir)/tools/glib-ginterface-gen.py \
	Makefile.am
	$(AM_V_at)$(mkdir_p) _gen
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/tools/glib-ginterface-gen.py \
		--filename=_gen/svc \
		--signal-marshal-prefix=NOT_NEEDED \
		--include='<telepathy-glib/dbus-properties-mixin.h>' \
		$< Test_Svc_

_gen/errors-check.h: $(top_srcdir)/spec/errors.xml \
	$(top_srcdir)/tools/glib-errors-check-gen.py
	$(AM_V_at)$(mkdir_p) _gen
	$(AM_V_GEN)$(PYTHON) $(top_srcdir)/tools/glib-errors-check-gen.py $< > $@
