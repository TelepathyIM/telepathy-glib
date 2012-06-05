# Example connection manager with ContactList channels.

EXAMPLES = telepathy-example-cm-contactlist
noinst_LTLIBRARIES = libexample-cm-contactlist.la

if INSTALL_EXAMPLES
libexec_PROGRAMS = $(EXAMPLES)
else
noinst_PROGRAMS = $(EXAMPLES)
endif

libexample_cm_contactlist_la_SOURCES = \
    conn.c \
    conn.h \
    connection-manager.c \
    connection-manager.h \
    contact-list.c \
    contact-list.h \
    protocol.c \
    protocol.h

libexample_cm_contactlist_la_LIBADD = $(LDADD)

telepathy_example_cm_contactlist_SOURCES = \
    main.c

telepathy_example_cm_contactlist_LDADD = \
    $(noinst_LTLIBRARIES)

if INSTALL_EXAMPLES
servicedir = ${datadir}/dbus-1/services
service_DATA = _gen/org.freedesktop.Telepathy.ConnectionManager.example_contact_list.service
$(service_DATA): %: Makefile
	$(mkdir_p) _gen
	{ echo "[D-BUS Service]" && \
	echo "Name=org.freedesktop.Telepathy.ConnectionManager.example_contact_list" && \
	echo "Exec=${libexecdir}/telepathy-example-cm-contactlist"; } > $@

managerdir = ${datadir}/telepathy/managers
dist_manager_DATA = example_contact_list.manager
endif

clean-local:
	rm -rf _gen

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the .la path, and put it last; we use
# a different format here because we're part of the telepathy-glib source tree.
LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la \
    $(GLIB_LIBS) \
    $(DBUS_LIBS) \
    $(NULL)

# Similarly, in an external project you'd put $(TP_GLIB_CFLAGS) at the end of
# AM_CPPFLAGS.
AM_CPPFLAGS = \
    -I${top_srcdir} -I${top_builddir} \
    -DTP_DISABLE_SINGLE_INCLUDE \
    $(GLIB_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(NULL)

AM_CFLAGS = $(ERROR_CFLAGS)
