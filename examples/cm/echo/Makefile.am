noinst_PROGRAMS = telepathy-example-cm-echo

telepathy_example_cm_echo_SOURCES = \
    chan.c \
    chan.h \
    conn.c \
    conn.h \
    factory.c \
    factory.h \
    main.c \
    manager.c \
    manager.h

telepathy_example_cm_echo_LDADD = \
    $(TP_GLIB_LIBS)

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)
