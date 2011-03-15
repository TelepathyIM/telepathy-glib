# Example connection manager with audio/video calls.

EXAMPLES = telepathy-example-cm-callable
noinst_LTLIBRARIES = libexample-cm-callable.la

if INSTALL_EXAMPLES
libexec_PROGRAMS = $(EXAMPLES)
else
noinst_PROGRAMS = $(EXAMPLES)
endif

libexample_cm_callable_la_SOURCES = \
    conn.c \
    conn.h \
    connection-manager.c \
    connection-manager.h \
    media-channel.c \
    media-channel.h \
    media-manager.c \
    media-manager.h \
    media-stream.c \
    media-stream.h \
    protocol.c \
    protocol.h

libexample_cm_callable_la_LIBADD = $(LDADD)

telepathy_example_cm_callable_SOURCES = \
    main.c

telepathy_example_cm_callable_LDADD = \
    $(noinst_LTLIBRARIES)

servicedir = ${datadir}/dbus-1/services

if INSTALL_EXAMPLES
service_DATA = _gen/org.freedesktop.Telepathy.ConnectionManager.example_callable.service
$(service_DATA): %: Makefile
	$(mkdir_p) _gen
	{ echo "[D-BUS Service]" && \
	echo "Name=org.freedesktop.Telepathy.ConnectionManager.example_callable" && \
	echo "Exec=${libexecdir}/telepathy-example-cm-callable"; } > $@

managerdir = ${datadir}/telepathy/managers
dist_manager_DATA = example_callable.manager
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
    $(GLIB_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(NULL)

AM_CFLAGS = $(ERROR_CFLAGS)
AM_LDFLAGS = \
    $(ERROR_LDFLAGS) \
    $(NULL)
