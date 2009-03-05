EXAMPLES = telepathy-example-no-protocols

if INSTALL_EXAMPLES
libexec_PROGRAMS = $(EXAMPLES)
else
noinst_PROGRAMS = $(EXAMPLES)
endif

telepathy_example_no_protocols_SOURCES = \
    connection-manager.c \
    connection-manager.h \
    main.c

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the .la path
telepathy_example_no_protocols_LDADD = \
    $(GLIB_LIBS) \
    $(DBUS_LIBS) \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    @DBUS_CFLAGS@ \
    @GLIB_CFLAGS@ \
    @TP_GLIB_CFLAGS@

EXTRA_DIST = manager-file.py

if INSTALL_EXAMPLES
servicedir = ${datadir}/dbus-1/services
service_DATA = _gen/org.freedesktop.Telepathy.ConnectionManager.example_no_protocols.service
$(service_DATA): %: Makefile
	$(mkdir_p) _gen
	{ echo "[D-BUS Service]" && \
	echo "Name=org.freedesktop.Telepathy.ConnectionManager.example_no_protocols" && \
	echo "Exec=${libexecdir}/telepathy-example-cm-no-protocols"; } > $@

managerdir = ${datadir}/telepathy/managers
manager_DATA = _gen/example_no_protocols.manager
endif

_gen/example_no_protocols.manager _gen/param-spec-struct.h: \
		manager-file.py $(top_srcdir)/tools/manager-file.py
	$(mkdir_p) _gen
	$(PYTHON) $(top_srcdir)/tools/manager-file.py $(srcdir)/manager-file.py _gen

BUILT_SOURCES = _gen/param-spec-struct.h
CLEANFILES = $(BUILT_SOURCES)

clean-local:
	rm -rf _gen
