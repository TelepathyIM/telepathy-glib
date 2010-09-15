EXAMPLES = telepathy-example-cm-extended

if INSTALL_EXAMPLES
libexec_PROGRAMS = $(EXAMPLES)
else
noinst_PROGRAMS = $(EXAMPLES)
endif

telepathy_example_cm_extended_SOURCES = \
    conn.c \
    conn.h \
    connection-manager.c \
    connection-manager.h \
    protocol.c \
    protocol.h \
    main.c

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the path to libtelepathy-glib.la
telepathy_example_cm_extended_LDADD = \
    $(GLIB_LIBS) \
    $(DBUS_LIBS) \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la \
    $(top_builddir)/examples/extensions/libexample-extensions.la

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)

if INSTALL_EXAMPLES
servicedir = ${datadir}/dbus-1/services
service_DATA = _gen/org.freedesktop.Telepathy.ConnectionManager.example_extended.service
$(service_DATA): %: Makefile
	$(mkdir_p) _gen
	{ echo "[D-BUS Service]" && \
	echo "Name=org.freedesktop.Telepathy.ConnectionManager.example_extended" && \
	echo "Exec=${libexecdir}/telepathy-example-cm-extended"; } > $@

managerdir = ${datadir}/telepathy/managers
dist_manager_DATA = example_extended.manager
endif

clean-local:
	rm -rf _gen