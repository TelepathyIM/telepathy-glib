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
    contact-list-manager.c \
    contact-list-manager.h

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the .la path
libexample_cm_contactlist_la_LIBADD = \
    $(GLIB_LIBS) \
    $(DBUS_LIBS) \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la

telepathy_example_cm_contactlist_SOURCES = \
    main.c

telepathy_example_cm_contactlist_LDADD = \
    $(noinst_LTLIBRARIES)

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)

EXTRA_DIST = manager-file.py

if INSTALL_EXAMPLES
servicedir = ${datadir}/dbus-1/services
service_DATA = _gen/org.freedesktop.Telepathy.ConnectionManager.example_contact_list.service
$(service_DATA): %: Makefile
	$(mkdir_p) _gen
	{ echo "[D-BUS Service]" && \
	echo "Name=org.freedesktop.Telepathy.ConnectionManager.example_contact_list" && \
	echo "Exec=${libexecdir}/telepathy-example-cm-contactlist"; } > $@

managerdir = ${datadir}/telepathy/managers
manager_DATA = _gen/example_contact_list.manager
endif

_gen/example_contact_list.manager _gen/param-spec-struct.h: \
		manager-file.py $(top_srcdir)/tools/manager-file.py
	$(mkdir_p) _gen
	$(PYTHON) $(top_srcdir)/tools/manager-file.py $(srcdir)/manager-file.py _gen

BUILT_SOURCES = _gen/param-spec-struct.h
CLEANFILES = $(BUILT_SOURCES)

clean-local:
	rm -rf _gen
