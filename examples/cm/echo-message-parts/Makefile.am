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
    protocol.c \
    protocol.h \
    connection-manager.c \
    connection-manager.h \
    im-manager.c \
    im-manager.h

libexample_cm_echo_2_la_LIBADD = $(LDADD)

telepathy_example_cm_echo_2_SOURCES = \
    main.c

telepathy_example_cm_echo_2_LDADD = \
    $(noinst_LTLIBRARIES)

if INSTALL_EXAMPLES
servicedir = ${datadir}/dbus-1/services
service_DATA = _gen/org.freedesktop.Telepathy.ConnectionManager.example_echo_2.service
$(service_DATA): %: Makefile
	$(mkdir_p) _gen
	{ echo "[D-BUS Service]" && \
	echo "Name=org.freedesktop.Telepathy.ConnectionManager.example_echo_2" && \
	echo "Exec=${libexecdir}/telepathy-example-cm-echo-2"; } > $@

managerdir = ${datadir}/telepathy/managers
dist_manager_DATA = example_echo_2.manager
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
AM_LDFLAGS = \
    $(ERROR_LDFLAGS) \
    $(NULL)
