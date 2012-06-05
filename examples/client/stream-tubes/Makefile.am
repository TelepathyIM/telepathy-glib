noinst_PROGRAMS = \
    offerer \
    accepter \
    $(NULL)

offerer_SOURCES = offerer.c
accepter_SOURCES = accepter.c

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the .la path
LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib.la \
    @DBUS_LIBS@ \
    @GLIB_LIBS@

AM_CPPFLAGS = \
    -DTP_DISABLE_SINGLE_INCLUDE \
    $(NULL)

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    @DBUS_CFLAGS@ \
    @GLIB_CFLAGS@ \
    @TP_GLIB_CFLAGS@
AM_LDFLAGS = \
    $(ERROR_LDFLAGS) \
    $(NULL)
