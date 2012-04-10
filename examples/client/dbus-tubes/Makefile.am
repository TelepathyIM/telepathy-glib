noinst_PROGRAMS = \
    offerer \
    accepter \
    $(NULL)

offerer_SOURCES = offerer.c constants.h
accepter_SOURCES = accepter.c constants.h

# In an external project you'd use $(TP_GLIB_LIBS) (obtained from
# pkg-config via autoconf) instead of the .la path
LDADD = \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
    $(top_builddir)/telepathy-glib/libtelepathy-glib-1-core.la \
    @DBUS_LIBS@ \
    @GLIB_LIBS@

AM_CFLAGS = \
    $(ERROR_CFLAGS) \
    @DBUS_CFLAGS@ \
    @GLIB_CFLAGS@ \
    @TP_GLIB_CFLAGS@
AM_LDFLAGS = \
    $(ERROR_LDFLAGS) \
    $(NULL)
