noinst_PROGRAMS = telepathy-example-no-protocols

telepathy_example_no_protocols_SOURCES = \
    main.c \
    manager.c \
    manager.h

telepathy_example_no_protocols_LDADD = \
    @DBUS_LIBS@ \
    @GLIB_LIBS@ \
    @TP_GLIB_LIBS@

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    @DBUS_CFLAGS@ \
    @GLIB_CFLAGS@ \
    @TP_GLIB_CFLAGS@
