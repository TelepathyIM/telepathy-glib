AC_PREREQ([2.59])

# Making releases:
#   set the new version number:
#     odd minor -> development series
#     even minor -> stable series
#     increment micro for each release within a series
#   set tp_farsight_release to 1

m4_define([tp_farsight_major_version], [0])
m4_define([tp_farsight_minor_version], [0])
m4_define([tp_farsight_micro_version], [3])
m4_define([tp_farsight_nano_version], [1])

m4_define([tp_farsight_lt_current], [0])
m4_define([tp_farsight_lt_revision], [0])
m4_define([tp_farsight_lt_age], [0])


# Some magic
m4_define([tp_farsight_base_version],
          [tp_farsight_major_version.tp_farsight_minor_version.tp_farsight_micro_version])
m4_define([tp_farsight_version],
          [m4_if(tp_farsight_nano_version, 0, [tp_farsight_base_version],
              [tp_farsight_base_version].[tp_farsight_nano_version])])

AC_INIT([Telepathy-Farsight], [tp_farsight_version],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=Telepathy&component=stream-engine])

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.9 -Wno-portability])
AM_CONFIG_HEADER(config.h)

dnl check for tools
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

dnl decide error flags
AS_COMPILER_FLAG(-Wall, ERROR_CFLAGS="-Wall", ERROR_CFLAGS="")
AS_COMPILER_FLAG(-Werror, werror=yes, werror=no)

AC_ARG_ENABLE(Werror,
  AC_HELP_STRING([--disable-Werror],[compile without -Werror (normally enabled in development builds)]),
    werror=$enableval, :)

AS_COMPILER_FLAG(-Wextra, wextra=yes, wextra=no)
AS_COMPILER_FLAG(-Wno-missing-field-initializers,
        wno_missing_field_initializers=yes,
        wno_missing_field_initializers=no)
AS_COMPILER_FLAG(-Wno-unused-parameter,
        wno_unused_parameter=yes,
        wno_unused_parameter=no)

ifelse(tp_farsight_nano_version, 0, [],
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

AC_ARG_ENABLE(coverage,       [  --enable-coverage         compile with coverage info],[enable_coverage=${enableval}],enable_coverage=no)
if test "x$enable_coverage" = "xyes"; then
       CFLAGS="$CFLAGS -g -fprofile-arcs -ftest-coverage"
fi

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

dnl Check for Telepathy libraries
PKG_CHECK_MODULES([TELEPATHY], [telepathy-glib >= 0.7.8])

AC_SUBST(TELEPATHY_CFLAGS)
AC_SUBST(TELEPATHY_LIBS)

dnl Check for farsight
PKG_CHECK_MODULES(FARSIGHT2, [farsight2-0.10 >= 0.0.3])

AC_SUBST(FARSIGHT2_CFLAGS)
AC_SUBST(FARSIGHT2_LIBS)

dnl Check for gst-interfaces or gst-plugins-base
PKG_CHECK_MODULES(GST_INTERFACES, [gstreamer-interfaces-0.10 >= 0.10.18],[],
       [AC_MSG_ERROR([gst-plugins-base-0.10.18. is required])])
AC_SUBST(GST_INTERFACES_CFLAGS)
AC_SUBST(GST_INTERFACES_LIBS)

GTK_DOC_CHECK([1.10])

LT_CURRENT=tp_farsight_lt_current
LT_REVISION=tp_farsight_lt_revision
LT_AGE=tp_farsight_lt_age
AC_SUBST([LT_CURRENT])
AC_SUBST([LT_REVISION])
AC_SUBST([LT_AGE])


AC_OUTPUT( Makefile \
          doc/Makefile \
          doc/lib/Makefile \
          m4/Makefile \
          telepathy-farsight/Makefile  \
          telepathy-farsight/telepathy-farsight.pc \
          telepathy-farsight/telepathy-farsight-uninstalled.pc
          tools/Makefile \
)

