noinst_PROGRAMS =

noinst_PROGRAMS += telepathy-example-inspect-channel
telepathy_example_inspect_channel_SOURCES = inspect-channel.c
telepathy_example_inspect_channel_LDADD = $(ALL_LIBS)

noinst_PROGRAMS += telepathy-example-inspect-connection
telepathy_example_inspect_connection_SOURCES = inspect-connection.c
telepathy_example_inspect_connection_LDADD = $(ALL_LIBS)

noinst_PROGRAMS += telepathy-example-inspect-cm
telepathy_example_inspect_cm_SOURCES = inspect-cm.c
telepathy_example_inspect_cm_LDADD = $(ALL_LIBS)

noinst_PROGRAMS += telepathy-example-list-managers
telepathy_example_list_managers_SOURCES = list-managers.c
telepathy_example_list_managers_LDADD = $(ALL_LIBS)

noinst_PROGRAMS += telepathy-example-extended-client
telepathy_example_extended_client_SOURCES = extended-client.c
telepathy_example_extended_client_LDADD = \
    $(top_builddir)/examples/extensions/libexample-extensions.la \
    $(ALL_LIBS)

# Sadly there is no AM_LDADD
ALL_LIBS = \
    @DBUS_LIBS@ \
    @GLIB_LIBS@ \
    @TP_GLIB_LIBS@

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    @DBUS_CFLAGS@ \
    @GLIB_CFLAGS@ \
    @TP_GLIB_CFLAGS@
