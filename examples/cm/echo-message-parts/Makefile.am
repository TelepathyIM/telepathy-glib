noinst_PROGRAMS = telepathy-example-cm-echo-2
noinst_LTLIBRARIES = libexample-cm-echo-2.la

libexample_cm_echo_2_la_SOURCES = \
    chan.c \
    chan.h \
    conn.c \
    conn.h \
    connection-manager.c \
    connection-manager.h \
    im-manager.c \
    im-manager.h

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the .la path
libexample_cm_echo_2_la_LIBADD = \
    $(GLIB_LIBS) \
    $(DBUS_LIBS) \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la

telepathy_example_cm_echo_2_SOURCES = \
    main.c

telepathy_example_cm_echo_2_LDADD = \
    $(noinst_LTLIBRARIES)

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(TP_GLIB_CFLAGS)
