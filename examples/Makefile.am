noinst_PROGRAMS = call-handler

LDADD =  \
   $(GLIB_LIBS) \
   $(DBUS_LIBS) \
   $(GST_LIBS) \
   $(FARSIGHT2_LIBS) \
   $(TELEPATHY_LIBS) \
   ../telepathy-farstream/libtelepathy-farstream.la \
   ../extensions/libfuture-extensions.la

AM_CFLAGS = \
   $(ERROR_CFLAGS) \
   $(GLIB_CFLAGS) \
   $(DBUS_CFLAGS) \
   $(GST_CFLAGS) \
   $(FARSIGHT2_CFLAGS) \
   $(TELEPATHY_CFLAGS) \
   -I$(top_srcdir) \
   -I$(top_builddir)
