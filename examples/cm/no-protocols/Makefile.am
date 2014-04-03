EXAMPLES = telepathy-1-example-no-protocols

if INSTALL_EXAMPLES
libexec_PROGRAMS = $(EXAMPLES)
else
noinst_PROGRAMS = $(EXAMPLES)
endif

telepathy_1_example_no_protocols_SOURCES = \
    connection-manager.c \
    connection-manager.h \
    main.c

if INSTALL_EXAMPLES
servicedir = ${datadir}/dbus-1/services
service_DATA = _gen/im.telepathy.v1.ConnectionManager.example_no_protocols.service
$(service_DATA): %: Makefile
	$(MKDIR_P) _gen
	{ echo "[D-BUS Service]" && \
	echo "Name=im.telepathy.v1.ConnectionManager.example_no_protocols" && \
	echo "Exec=${libexecdir}/telepathy-1-example-cm-no-protocols"; } > $@

managerdir = ${datadir}/telepathy-1/managers
dist_manager_DATA = example_no_protocols.manager
endif

distclean-local:
	rm -rf _gen

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the .la path, and put it last; we use
# a different format here because we're part of the telepathy-glib source tree.
LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
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
