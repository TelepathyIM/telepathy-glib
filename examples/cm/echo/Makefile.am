noinst_PROGRAMS = telepathy-example-cm-echo
noinst_LTLIBRARIES = libexample-cm-echo.la

libexample_cm_echo_la_SOURCES = \
    chan.c \
    chan.h \
    conn.c \
    conn.h \
    factory.c \
    factory.h \
    manager.c \
    manager.h

libexample_cm_echo_la_LIBADD = \
    $(TP_GLIB_LIBS)

telepathy_example_cm_echo_SOURCES = \
    main.c

telepathy_example_cm_echo_LDADD = \
    $(noinst_LTLIBRARIES)

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)
