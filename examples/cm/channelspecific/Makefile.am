# Example connection manager with chatrooms where contacts have
# channel-specific handles.

EXAMPLES = telepathy-example-cm-csh
noinst_LTLIBRARIES = libexample-cm-csh.la

if INSTALL_EXAMPLES
libexec_PROGRAMS = $(EXAMPLES)
else
noinst_PROGRAMS = $(EXAMPLES)
endif

libexample_cm_csh_la_SOURCES = \
    conn.c \
    conn.h \
    connection-manager.c \
    connection-manager.h \
    room.c \
    room.h \
    room-manager.c \
    room-manager.h

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the .la path
libexample_cm_csh_la_LIBADD = \
    $(GLIB_LIBS) \
    $(DBUS_LIBS) \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la

telepathy_example_cm_csh_SOURCES = \
    main.c

telepathy_example_cm_csh_LDADD = \
    $(noinst_LTLIBRARIES)

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)

EXTRA_DIST = manager-file.py

servicedir = ${datadir}/dbus-1/services

if INSTALL_EXAMPLES
service_DATA = _gen/org.freedesktop.Telepathy.ConnectionManager.example_csh.service
$(service_DATA): %: Makefile
	$(mkdir_p) _gen
	{ echo "[D-BUS Service]" && \
	echo "Name=org.freedesktop.Telepathy.ConnectionManager.example_csh" && \
	echo "Exec=${libexecdir}/telepathy-example-cm-csh"; } > $@

managerdir = ${datadir}/telepathy/managers
manager_DATA = _gen/example_csh.manager
endif

_gen/example_csh.manager _gen/param-spec-struct.h: \
		manager-file.py $(top_srcdir)/tools/manager-file.py
	$(mkdir_p) _gen
	$(PYTHON) $(top_srcdir)/tools/manager-file.py $(srcdir)/manager-file.py _gen

BUILT_SOURCES = _gen/param-spec-struct.h
CLEANFILES = $(BUILT_SOURCES)

clean-local:
	rm -rf _gen
