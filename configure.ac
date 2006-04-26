AC_INIT(Makefile.am)

AS_VERSION(telepathy-voip-engine, TELEPATHY_VOIP_ENGINE_VERSION, 0, 1, 4, 1, WERROR="no", WERROR="yes")

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
PKG_CHECK_MODULES(LIBTELEPATHY, [libtelepathy], have_libtelepathy=yes, have_libtelepathy=no)

AC_SUBST(LIBTELEPATHY_CFLAGS)
AC_SUBST(LIBTELEPATHY_LIBS)

if test x$have_libtelepathy = xno ; then
    AC_MSG_ERROR([libtelepathy development libraries not found])
fi

dnl Check for farsight 
PKG_CHECK_MODULES(FARSIGHT, [farsight-0.1], have_farsight=yes, have_farsight=no)

AC_SUBST(FARSIGHT_CFLAGS)
AC_SUBST(FARSIGHT_LIBS)

if test x$have_farsight = xno ; then
    AC_MSG_ERROR([Farsight development libraries not found])
fi


AC_OUTPUT( Makefile \
          common/Makefile \
          data/Makefile \
          doc/Makefile \
          src/Makefile  \
          test/Makefile \
          test/python/Makefile
)



