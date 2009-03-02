EXAMPLES =

EXAMPLES += telepathy-example-inspect-channel
telepathy_example_inspect_channel_SOURCES = inspect-channel.c

EXAMPLES += telepathy-example-inspect-connection
telepathy_example_inspect_connection_SOURCES = inspect-connection.c

EXAMPLES += telepathy-example-inspect-contact
telepathy_example_inspect_contact_SOURCES = inspect-contact.c

EXAMPLES += telepathy-example-inspect-cm
telepathy_example_inspect_cm_SOURCES = inspect-cm.c

EXAMPLES += telepathy-example-list-connections
telepathy_example_list_connections_SOURCES = list-connections.c

EXAMPLES += telepathy-example-list-managers
telepathy_example_list_managers_SOURCES = list-managers.c

EXAMPLES += telepathy-example-extended-client
telepathy_example_extended_client_SOURCES = extended-client.c
telepathy_example_extended_client_LDADD = \
    $(top_builddir)/examples/extensions/libexample-extensions.la \
    $(LDADD)

if INSTALL_EXAMPLES
bin_PROGRAMS = $(EXAMPLES)
else
noinst_PROGRAMS = $(EXAMPLES)
endif

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the .la path
LDADD = \
    @DBUS_LIBS@ \
    @GLIB_LIBS@ \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    @DBUS_CFLAGS@ \
    @GLIB_CFLAGS@ \
    @TP_GLIB_CFLAGS@
