AC_INIT(Makefile.am)

AS_VERSION(telepathy-stream-engine, TELEPATHY_STREAM_ENGINE_VERSION, 0, 3, 2, 1, WERROR="no", WERROR="yes")

AM_INIT_AUTOMAKE($PACKAGE, $VERSION)

AM_CONFIG_HEADER(config.h)
AC_CANONICAL_HOST

AC_CONFIG_MACRO_DIR([m4])

AM_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

dnl decide on error flags
AS_COMPILER_FLAG(-Wall, WALL="yes", WALL="no")

if test "x$WALL" = "xyes"; then
  ERROR_CFLAGS="-Wall"

  if test "x$WERROR" = "xyes"; then
    AS_COMPILER_FLAG(-Werror,ERROR_CFLAGS="$ERROR_CFLAGS -Werror",ERROR_CFLAGS="$ERROR_CFLAGS")
  fi
fi

AC_SUBST(ERROR_CFLAGS)

AC_HEADER_STDC([])
AC_C_INLINE

dnl options for building sequence diagrams
SEQ_DIAS

dnl Check for Glib 
PKG_CHECK_MODULES(GLIB, gobject-2.0 >= 2.4, have_glib=yes, have_glib=no)

if test x$have_glib = xno ; then
    AC_MSG_ERROR([GLib development libraries not found])
fi
GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)
AC_SUBST(GLIB_GENMARSHAL)

dnl Check for DBus
PKG_CHECK_MODULES(DBUS, [dbus-1 >= 0.60, dbus-glib-1 >= 0.60], have_dbus=yes, have_dbus=no)

AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

if test x$have_dbus = xno ; then
    AC_MSG_ERROR([DBus development libraries not found])
fi

AS_AC_EXPAND(DATADIR, $datadir)
DBUS_SERVICES_DIR="$DATADIR/dbus-1/services"
AC_SUBST(DBUS_SERVICES_DIR)
AC_DEFINE_UNQUOTED(DBUS_SERVICES_DIR, "$DBUS_SERVICES_DIR", [DBus services directory])

dnl Check for libtelepathy
PKG_CHECK_MODULES(LIBTELEPATHY, libtelepathy >= 0.0.33)

AC_SUBST(LIBTELEPATHY_CFLAGS)
AC_SUBST(LIBTELEPATHY_LIBS)

dnl Check for farsight 
PKG_CHECK_MODULES(FARSIGHT, [farsight-0.1 >= 0.1.8], have_farsight=yes, have_farsight=no)

AC_SUBST(FARSIGHT_CFLAGS)
AC_SUBST(FARSIGHT_LIBS)

if test x$have_farsight = xno ; then
    AC_MSG_ERROR([Farsight development libraries not found])
fi

dnl Check for gst-plugins-base
PKG_CHECK_MODULES(GST_PLUGINS_BASE, [gstreamer-plugins-base-0.10])
AC_SUBST(GST_PLUGINS_BASE_CFLAGS)
AC_SUBST(GST_PLUGINS_BASE_LIBS)

dnl libx11
PKG_CHECK_MODULES(X11, [x11])
AC_SUBST(X11_CFLAGS)
AC_SUBST(X11_LIBS)

dnl maemo support
AC_ARG_ENABLE( maemo-support,
  AC_HELP_STRING([--enable-maemo-support],[compile with Maemo support]),
  maemo_support=$enableval, maemo_support=no )

if test x$maemo_support = xyes; then
  AC_DEFINE(MAEMO_OSSO_SUPPORT, , [Enable Maemo support])
fi

AM_CONDITIONAL(MAEMO_OSSO_SUPPORT, test x$MAEMO_OSSO_SUPPORT = xyes)

dnl use infoprint 
AC_ARG_ENABLE( infoprint,
  AC_HELP_STRING([--enable-infoprint],[compile with infoprint support]),
  use_infoprint=$enableval, use_infoprint=no )

if test x$MAEMO_OSSO_SUPPORT = xyes -a x$use_infoprint = xyes; then
  AC_DEFINE(USE_INFOPRINT, , [Use infoprint])
fi

AM_CONDITIONAL(USE_INFOPRINT, test x$USE_INFOPRINT = xyes)

dnl glibc backtrace functions
AC_CHECK_FUNCS(backtrace backtrace_symbols_fd)
AC_CHECK_HEADERS(execinfo.h)

AC_OUTPUT( Makefile \
          common/Makefile \
          data/Makefile \
          doc/Makefile \
          src/Makefile  \
          test/Makefile \
          test/python/Makefile
)



