AC_PREREQ([2.59])

# Making releases:
#   set the new version number:
#     odd minor -> development series
#     even minor -> stable series
#     increment micro for each release within a series
#   set tp_farstream_release to 1

m4_define([tp_farstream_major_version], [0])
m4_define([tp_farstream_minor_version], [6])
m4_define([tp_farstream_micro_version], [0])
m4_define([tp_farstream_nano_version], [1])

dnl CURRENT, REVISION, AGE
dnl - library source changed -> increment REVISION
dnl - interfaces added/removed/changed -> increment CURRENT, REVISION = 0
dnl - interfaces added -> increment AGE
dnl - interfaces removed -> AGE = 0
m4_define([tp_farstream_lt_current], [0])
m4_define([tp_farstream_lt_revision], [0])
m4_define([tp_farstream_lt_age], [0])


# Some magic
m4_define([tp_farstream_base_version],
          [tp_farstream_major_version.tp_farstream_minor_version.tp_farstream_micro_version])
m4_define([tp_farstream_version],
          [m4_if(tp_farstream_nano_version, 0, [tp_farstream_base_version],
              [tp_farstream_base_version].[tp_farstream_nano_version])])

AC_INIT([Telepathy-Farstream], [tp_farstream_version],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=Telepathy&component=tp-farstream])

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 -Wno-portability])
AC_CONFIG_HEADERS(config.h)

dnl use pretty build output with automake >= 1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],
  [AM_DEFAULT_VERBOSITY=1
   AC_SUBST(AM_DEFAULT_VERBOSITY)])

dnl check for tools
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

dnl decide error flags
AS_COMPILER_FLAG(-Wall, ERROR_CFLAGS="-Wall", ERROR_CFLAGS="")
AS_COMPILER_FLAG(-Werror -Wno-error=deprecated-declarations, werror=yes, werror=no)

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

ifelse(tp_farstream_nano_version, 0, [],
    [
        AS_IF([test x$werror = xyes],
            [ERROR_CFLAGS="$ERROR_CFLAGS -Werror -Wno-error=deprecated-declarations"])
	AS_IF([test x$wextra = xyes -a \
            x$wno_missing_field_initializers = xyes -a \
            x$wno_unused_parameter = xyes].
            [ERROR_CFLAGS="$ERROR_CFLAGS -Wextra -Wno-missing-field-initializers -Wno-unused-parameter"])
    ])

AC_SUBST(ERROR_CFLAGS)

AC_ARG_ENABLE(coverage,       [  --enable-coverage         compile with coverage info],[enable_coverage=${enableval}],enable_coverage=no)
AS_IF([test "x$enable_coverage" = "xyes"],
	    [CFLAGS="$CFLAGS -g -fprofile-arcs -ftest-coverage"])

CFLAGS="$CFLAGS"

dnl Check for Glib
PKG_CHECK_MODULES(GLIB, gobject-2.0 >= 2.32 glib-2.0 >= 2.32 gio-2.0)
AC_DEFINE(GLIB_VERSION_MIN_REQUIRED, GLIB_VERSION_2_32, [Ignore post 2.32 deprecations])
AC_DEFINE(GLIB_VERSION_MAX_ALLOWED, GLIB_VERSION_2_32, [Prevent post 2.32 APIs])

AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

dnl Check for DBus
PKG_CHECK_MODULES(DBUS, [dbus-1 >= 0.60, dbus-glib-1 >= 0.60])

AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

dnl Check for Telepathy libraries
PKG_CHECK_MODULES([TELEPATHY], [telepathy-glib-1 >= 0.99.1, telepathy-glib-1-dbus >= 0.99.1])
AC_DEFINE(TP_VERSION_MIN_REQUIRED, TP_VERSION_0_22, [Ignore post 0.22 deprecations])
AC_DEFINE(TP_VERSION_MAX_ALLOWED, TP_VERSION_0_22, [Prevent post 0.22 APIs])
AC_DEFINE(TP_SEAL_ENABLE, 1, [Prevent to use sealed variables])
AC_DEFINE(TP_DISABLE_SINGLE_INCLUDE, 1, [Disable single include header])

AC_SUBST(TELEPATHY_CFLAGS)
AC_SUBST(TELEPATHY_LIBS)

dnl Check for farstream
PKG_CHECK_MODULES(FARSTREAM, [farstream-0.2])

AC_SUBST(FARSTREAM_CFLAGS)
AC_SUBST(FARSTREAM_LIBS)

GOBJECT_INTROSPECTION_CHECK([1.30.0])

GTK_DOC_CHECK([1.17], [--flavour no-tmpl])

LT_CURRENT=tp_farstream_lt_current
LT_REVISION=tp_farstream_lt_revision
LT_AGE=tp_farstream_lt_age
AC_SUBST([LT_CURRENT])
AC_SUBST([LT_REVISION])
AC_SUBST([LT_AGE])


AC_OUTPUT( Makefile \
          docs/Makefile \
          docs/reference/Makefile \
          docs/reference/telepathy-farstream/Makefile \
          m4/Makefile \
          examples/Makefile \
          examples/python/Makefile \
          telepathy-farstream/Makefile  \
          telepathy-farstream/telepathy-farstream-1.pc \
          telepathy-farstream/telepathy-farstream-uninstalled-1.pc
          tools/Makefile
)
