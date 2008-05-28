# Example connection manager with chatrooms where contacts have
# channel-specific handles.

noinst_PROGRAMS = telepathy-example-cm-csh
noinst_LTLIBRARIES = libexample-cm-csh.la

libexample_cm_csh_la_SOURCES = \
    conn.c \
    conn.h \
    room.c \
    room.h \
    room-factory.c \
    room-factory.h \
    manager.c \
    manager.h

libexample_cm_csh_la_LIBADD = \
    $(TP_GLIB_LIBS)

telepathy_example_cm_csh_SOURCES = \
    main.c

telepathy_example_cm_csh_LDADD = \
    $(noinst_LTLIBRARIES)

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)
