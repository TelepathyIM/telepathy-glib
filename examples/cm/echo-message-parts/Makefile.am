noinst_PROGRAMS = telepathy-example-cm-echo-2
noinst_LTLIBRARIES = libexample-cm-echo-2.la

libexample_cm_echo_2_la_SOURCES = \
    chan.c \
    chan.h \
    conn.c \
    conn.h \
    factory.c \
    factory.h \
    manager.c \
    manager.h

telepathy_example_cm_echo_2_SOURCES = \
    main.c

telepathy_example_cm_echo_2_LDADD = \
    $(TP_GLIB_LIBS) \
    libexample-cm-echo-2.la

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)
