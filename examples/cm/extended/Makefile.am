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

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the .la path, and put it last; we use
# a different format here because we're part of the telepathy-glib source tree.
LDADD = \
    $(top_builddir)/examples/extensions/libexample-extensions.la \
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
AM_LDFLAGS = \
    $(ERROR_LDFLAGS) \
    $(NULL)
