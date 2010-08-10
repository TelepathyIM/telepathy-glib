# Example connection manager with audio/video calls.

EXAMPLES = telepathy-example-cm-call
noinst_LTLIBRARIES = libexample-cm-call.la

if INSTALL_EXAMPLES
libexec_PROGRAMS = $(EXAMPLES)
else
noinst_PROGRAMS = $(EXAMPLES)
endif

libexample_cm_call_la_SOURCES = \
    cm.c \
    cm.h \
    conn.c \
    conn.h \
    call-channel.c \
    call-channel.h \
    call-manager.c \
    call-manager.h \
    call-content.c \
    call-content.h \
    call-stream.c \
    call-stream.h \
    protocol.c \
    protocol.h

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the .la path
libexample_cm_call_la_LIBADD = \
    $(GLIB_LIBS) \
    $(DBUS_LIBS) \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la \
    $(top_builddir)/extensions/libfuture-extensions.la

telepathy_example_cm_call_SOURCES = \
    main.c

telepathy_example_cm_call_LDADD = \
    $(noinst_LTLIBRARIES)

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)

servicedir = ${datadir}/dbus-1/services

if INSTALL_EXAMPLES
service_DATA = _gen/org.freedesktop.Telepathy.ConnectionManager.example_call.service
$(service_DATA): %: Makefile
	$(mkdir_p) _gen
	{ echo "[D-BUS Service]" && \
	echo "Name=org.freedesktop.Telepathy.ConnectionManager.example_call" && \
	echo "Exec=${libexecdir}/telepathy-example-cm-call"; } > $@

managerdir = ${datadir}/telepathy/managers
dist_manager_DATA = example_call.manager
endif

clean-local:
	rm -rf _gen
