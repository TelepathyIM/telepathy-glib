SUBDIRS = stream-tubes dbus-tubes python js

EXAMPLES =

EXAMPLES += telepathy-1-example-inspect-channel
telepathy_1_example_inspect_channel_SOURCES = inspect-channel.c

EXAMPLES += telepathy-1-example-inspect-connection
telepathy_1_example_inspect_connection_SOURCES = inspect-connection.c

EXAMPLES += telepathy-1-example-inspect-contact
telepathy_1_example_inspect_contact_SOURCES = inspect-contact.c

EXAMPLES += telepathy-1-example-inspect-cm
telepathy_1_example_inspect_cm_SOURCES = inspect-cm.c

EXAMPLES += telepathy-1-example-list-managers
telepathy_1_example_list_managers_SOURCES = list-managers.c

EXAMPLES += telepathy-1-example-extended-client
telepathy_1_example_extended_client_SOURCES = extended-client.c
telepathy_1_example_extended_client_LDADD = \
    $(top_builddir)/examples/extensions/libexample-extensions.la \
    $(LDADD)

EXAMPLES += telepathy-1-example-media-observer
telepathy_1_example_media_observer_SOURCES = media-observer.c

EXAMPLES += telepathy-1-example-approver
telepathy_1_example_approver_SOURCES = approver.c

EXAMPLES += telepathy-1-example-text-handler
telepathy_1_example_text_handler_SOURCES = text-handler.c

EXAMPLES += telepathy-1-example-contact-list
telepathy_1_example_contact_list_SOURCES = contact-list.c

if INSTALL_EXAMPLES
bin_PROGRAMS = $(EXAMPLES)
else
noinst_PROGRAMS = $(EXAMPLES)
endif

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the .la path, and put it last; we use
# a different format here because we're part of the telepathy-glib source tree.
# Similarly, in an external project you'd put $(TP_GLIB_CFLAGS) at the end of
# AM_CPPFLAGS.
LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1-core.la \
    $(GLIB_LIBS) \
    $(DBUS_LIBS) \
    $(NULL)

AM_CPPFLAGS = \
    -I${top_srcdir} -I${top_builddir} \
    $(GLIB_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(NULL)

AM_CFLAGS = $(ERROR_CFLAGS)
AM_LDFLAGS = \
    $(ERROR_LDFLAGS) \
    $(NULL)
