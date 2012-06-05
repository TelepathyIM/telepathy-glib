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

libexample_cm_call_la_LIBADD = $(LDADD)

telepathy_example_cm_call_SOURCES = \
    main.c

telepathy_example_cm_call_LDADD = \
    $(noinst_LTLIBRARIES)

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
AM_LDFLAGS = \
    $(ERROR_LDFLAGS) \
    $(NULL)

AM_CFLAGS = $(ERROR_CFLAGS)

