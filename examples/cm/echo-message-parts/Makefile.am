EXAMPLES = telepathy-example-cm-echo-2
noinst_LTLIBRARIES = libexample-cm-echo-2.la

if INSTALL_EXAMPLES
libexec_PROGRAMS = $(EXAMPLES)
else
noinst_PROGRAMS = $(EXAMPLES)
endif

libexample_cm_echo_2_la_SOURCES = \
    chan.c \
    chan.h \
    conn.c \
    conn.h \
    connection-manager.c \
    connection-manager.h \
    im-manager.c \
    im-manager.h

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the .la path
libexample_cm_echo_2_la_LIBADD = \
    $(GLIB_LIBS) \
    $(DBUS_LIBS) \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la

telepathy_example_cm_echo_2_SOURCES = \
    main.c

telepathy_example_cm_echo_2_LDADD = \
    $(noinst_LTLIBRARIES)

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)

EXTRA_DIST = manager-file.py

if INSTALL_EXAMPLES
servicedir = ${datadir}/dbus-1/services
service_DATA = _gen/org.freedesktop.Telepathy.ConnectionManager.example_echo_2.service
$(service_DATA): %: Makefile
	$(mkdir_p) _gen
	{ echo "[D-BUS Service]" && \
	echo "Name=org.freedesktop.Telepathy.ConnectionManager.example_echo_2" && \
	echo "Exec=${libexecdir}/telepathy-example-cm-echo-2"; } > $@

managerdir = ${datadir}/telepathy/managers
manager_DATA = _gen/example_echo_2.manager
endif

_gen/example_echo_2.manager _gen/param-spec-struct.h: \
		manager-file.py $(top_srcdir)/tools/manager-file.py
	$(mkdir_p) _gen
	$(PYTHON) $(top_srcdir)/tools/manager-file.py $(srcdir)/manager-file.py _gen

BUILT_SOURCES = _gen/param-spec-struct.h
CLEANFILES = $(BUILT_SOURCES)

clean-local:
	rm -rf _gen
