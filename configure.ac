AC_PREREQ([2.59])

# Making releases:
#   set the new version number:
#     odd minor -> development series
#     even minor -> stable series
#     increment micro for each release within a series
#   set stream_engine_release to 1.

m4_define([stream_engine_major_version], [0])
m4_define([stream_engine_minor_version], [3])
m4_define([stream_engine_micro_version], [15])
m4_define([stream_engine_nano_version], [1])

# Some magic
m4_define([stream_engine_base_version],
          [stream_engine_major_version.stream_engine_minor_version.stream_engine_micro_version])
m4_define([stream_engine_version],
          [m4_if(stream_engine_nano_version, 0, [stream_engine_base_version],
              [stream_engine_base_version].[stream_engine_nano_version])])

AC_INIT([Telepathy Stream Engine], [stream_engine_version],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=Telepathy])

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.8])
AM_CONFIG_HEADER(config.h)

dnl check for tools
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

dnl decide error flags
AS_COMPILER_FLAG(-Wall, ERROR_CFLAGS="-Wall", ERROR_CFLAGS="")
AS_COMPILER_FLAG(-Werror, werror=yes, werror=no)
AS_COMPILER_FLAG(-Wextra, wextra=yes, wextra=no)
AS_COMPILER_FLAG(-Wno-missing-field-initializers,
        wno_missing_field_initializers=yes,
        wno_missing_field_initializers=no)
AS_COMPILER_FLAG(-Wno-unused-parameter,
        wno_unused_parameter=yes,
        wno_unused_parameter=no)

ifelse(stream_engine_nano_version, 0, [],
    [
        if test x$werror = xyes; then
            ERROR_CFLAGS="$ERROR_CFLAGS -Werror"
        fi
        if test x$wextra = xyes -a \
            x$wno_missing_field_initializers = xyes -a \
            x$wno_unused_parameter = xyes; then
            ERROR_CFLAGS="$ERROR_CFLAGS -Wextra -Wno-missing-field-initializers -Wno-unused-parameter"
        fi
    ])

AC_SUBST(ERROR_CFLAGS)

AC_ARG_ENABLE(backtrace,
  AC_HELP_STRING([--enable-backtrace],[enable printing out the backtrace in case of crash]),
    enable_backtrace=$enableval, enable_backtrace=no )

if test x$enable_backtrace = xyes; then
  AC_DEFINE(ENABLE_BACKTRACE, [], [Enable backtrace output on crashes])
fi

dnl options for building sequence diagrams
SEQ_DIAS

dnl Check for Glib 
PKG_CHECK_MODULES(GLIB, gobject-2.0 >= 2.4)

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)
AC_SUBST(GLIB_GENMARSHAL)

dnl Check for DBus
PKG_CHECK_MODULES(DBUS, [dbus-1 >= 0.60, dbus-glib-1 >= 0.60])

AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

AS_AC_EXPAND(DATADIR, $datadir)
DBUS_SERVICES_DIR="$DATADIR/dbus-1/services"
AC_SUBST(DBUS_SERVICES_DIR)
AC_DEFINE_UNQUOTED(DBUS_SERVICES_DIR, "$DBUS_SERVICES_DIR", [DBus services directory])

dnl Check for libtelepathy
PKG_CHECK_MODULES(LIBTELEPATHY, libtelepathy >= 0.0.33)

AC_SUBST(LIBTELEPATHY_CFLAGS)
AC_SUBST(LIBTELEPATHY_LIBS)

dnl Check for farsight 
PKG_CHECK_MODULES(FARSIGHT, [farsight-0.1 >= 0.1.8])

AC_SUBST(FARSIGHT_CFLAGS)
AC_SUBST(FARSIGHT_LIBS)

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

