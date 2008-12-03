noinst_PROGRAMS = telepathy-example-cm-extended

telepathy_example_cm_extended_SOURCES = \
    conn.c \
    conn.h \
    main.c \
    manager.c \
    manager.h

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the path to libtelepathy-glib.la
telepathy_example_cm_extended_LDADD = \
    $(GLIB_LIBS) \
    $(DBUS_LIBS) \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la \
    $(top_builddir)/examples/extensions/libexample-extensions.la

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)
