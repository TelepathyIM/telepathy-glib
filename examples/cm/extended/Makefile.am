noinst_PROGRAMS = telepathy-example-cm-extended

telepathy_example_cm_extended_SOURCES = \
    conn.c \
    conn.h \
    main.c \
    manager.c \
    manager.h

telepathy_example_cm_extended_LDADD = \
    $(top_builddir)/examples/extensions/libexample-extensions.la \
    $(TP_GLIB_LIBS)

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)
