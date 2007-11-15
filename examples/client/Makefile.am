noinst_PROGRAMS =

noinst_PROGRAMS += telepathy-example-inspect-channel
telepathy_example_inspect_channel_SOURCES = inspect-channel.c
telepathy_example_inspect_channel_LDADD = $(ALL_LIBS)

noinst_PROGRAMS += telepathy-example-inspect-connection
telepathy_example_inspect_connection_SOURCES = inspect-connection.c
telepathy_example_inspect_connection_LDADD = $(ALL_LIBS)

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
