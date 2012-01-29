SUBDIRS=python

noinst_PROGRAMS = call-handler

LDADD =  \
   ../telepathy-farstream/libtelepathy-farstream.la \
   $(GLIB_LIBS) \
   $(DBUS_LIBS) \
   $(GST_LIBS) \
   $(FARSTREAM_LIBS) \
   $(TELEPATHY_LIBS)

AM_CFLAGS = \
   -I$(top_srcdir) \
   -I$(top_builddir) \
   $(ERROR_CFLAGS) \
   $(GLIB_CFLAGS) \
   $(DBUS_CFLAGS) \
   $(GST_CFLAGS) \
   $(FARSTREAM_CFLAGS) \
   $(TELEPATHY_CFLAGS)
