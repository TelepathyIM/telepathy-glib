SUBDIRS=python

noinst_PROGRAMS = call-handler

LDADD =  \
   $(top_builddir)/telepathy-farstream/libtelepathy-farstream-1.la \
   $(top_builddir)/telepathy-glib/libtelepathy-glib-1.la \
   $(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
   $(GLIB_LIBS) \
   $(DBUS_LIBS) \
   $(GST_LIBS) \
   $(FARSTREAM_LIBS) \
   $(NULL)

AM_CFLAGS = \
   -I$(top_srcdir) \
   -I$(top_builddir) \
   $(ERROR_CFLAGS) \
   $(GLIB_CFLAGS) \
   $(DBUS_CFLAGS) \
   $(GST_CFLAGS) \
   $(FARSTREAM_CFLAGS) \
   $(NULL)
