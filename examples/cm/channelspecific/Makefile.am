# Example connection manager with chatrooms where contacts have
# channel-specific handles.

noinst_PROGRAMS = telepathy-example-cm-csh
noinst_LTLIBRARIES = libexample-cm-csh.la

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
