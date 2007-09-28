AC_PREREQ([2.59])

# Making releases:
#   set the new version number:
#     odd minor -> development series
#     even minor -> stable series
#     increment micro for each release within a series
#   set nano_version to 0
#   make the release, tag it
#   set nano_version to 1

m4_define([tp_glib_major_version], [0])
m4_define([tp_glib_minor_version], [6])
m4_define([tp_glib_micro_version], [999])
m4_define([tp_glib_nano_version], [1])

# When deciding the following, only consider the shared library, not the
# -unstable library.

# If library source has changed since last release, increment revision
# If interfaces have been added, removed or changed since last release,
#  increment current and set revision to 0
# If interfaces have been added since last release, increment age
# If interfaces have been removed since last release, set age to 0

m4_define([tp_glib_lt_current], [3])
m4_define([tp_glib_lt_revision], [1])
m4_define([tp_glib_lt_age], [3])

# Some magic
m4_define([tp_glib_base_version],
          [tp_glib_major_version.tp_glib_minor_version.tp_glib_micro_version])
m4_define([tp_glib_version],
          [m4_if(tp_glib_nano_version, 0, [tp_glib_base_version],
              [tp_glib_base_version].[tp_glib_nano_version])])

AC_INIT([Telepathy-GLib], [tp_glib_version],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=Telepathy])

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.8])
AM_CONFIG_HEADER(config.h)

dnl check for tools
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AM_PROG_LIBTOOL

GTK_DOC_CHECK([1.5])

LT_CURRENT=tp_glib_lt_current
LT_REVISION=tp_glib_lt_revision
LT_AGE=tp_glib_lt_age
AC_SUBST([LT_CURRENT])
AC_SUBST([LT_REVISION])
AC_SUBST([LT_AGE])

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

ifelse(tp_glib_nano_version, 0, [],
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

AC_ARG_ENABLE(debug,
  AC_HELP_STRING([--disable-debug],[compile without debug code (note that this will disable much of the debug code in all GLib connection managers)]),
    enable_debug=$enableval, enable_debug=yes )

AC_ARG_ENABLE(handle-leak-debug,
  AC_HELP_STRING([--enable-handle-leak-debug],[compile with handle leak debugging code (the CM should be built with -rdynamic)]),
    enable_handle_leak_debug=$enableval, enable_handle_leak_debug=no )

AC_ARG_ENABLE(backtrace,
  AC_HELP_STRING([--enable-backtrace],[enable printing out the backtrace in case of crash (in most GLib connection managers)]),
    enable_backtrace=$enableval, enable_backtrace=no )

AC_ARG_ENABLE(coverage,
  AC_HELP_STRING([--enable-coverage],[compile with coverage info]),
    enable_coverage=$enableval, enable_coverage=no )

ifelse(tp_glib_nano_version, 0,
    [ # tp-glib is version x.y.z - disable coding style checks by default
AC_ARG_ENABLE(coding-style-checks,
  AC_HELP_STRING([--enable-coding-style-checks],
                 [check coding style using grep]),
    [ENABLE_CODING_STYLE_CHECKS=$enableval], [ENABLE_CODING_STYLE_CHECKS=no] )
    ],
    [ # tp-glib is version x.y.z.1 - enable coding style checks by default
AC_ARG_ENABLE(coding-style-checks,
  AC_HELP_STRING([--disable-coding-style-checks],
                 [don't check coding style using grep]),
    [ENABLE_CODING_STYLE_CHECKS=$enableval], [ENABLE_CODING_STYLE_CHECKS=yes])
    ])

if test x$enable_debug = xyes; then
  AC_DEFINE(ENABLE_DEBUG, [], [Enable debug code])
fi

if test x$enable_handle_leak_debug = xyes; then
  AC_DEFINE(ENABLE_HANDLE_LEAK_DEBUG, [], [Enable handle leak debug code])
  HANDLE_LEAK_DEBUG_CFLAGS="-rdynamic"
fi

if test x$enable_backtrace = xyes; then
  AC_DEFINE(ENABLE_BACKTRACE, [], [Enable backtrace output on crashes])
fi

if test x$enable_coverage = xyes; then
  COVERAGE_CFLAGS="-g -fprofile-arcs -ftest-coverage"
fi

AC_SUBST(HANDLE_LEAK_DEBUG_CFLAGS)
AC_SUBST(COVERAGE_CFLAGS)

AC_SUBST([ENABLE_CODING_STYLE_CHECKS])

dnl docs/Makefile.am needs to know whether it's an out of tree build
dnl (srcdir != builddir)
AM_CONDITIONAL([OUT_OF_TREE_BUILD], [test "z$ac_srcdir" != z.])

dnl Check for Glib
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.4, gobject-2.0 >= 2.4])

AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

dnl Check for D-Bus
PKG_CHECK_MODULES(DBUS, [dbus-1 >= 0.61, dbus-glib-1 >= 0.72])

AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

dnl Check for code generation tools
XSLTPROC=
AC_CHECK_PROGS([XSLTPROC], [xsltproc])
if test -z "$XSLTPROC"; then
  AC_MSG_ERROR([xsltproc (from the libxslt source package) is required])
fi
DBUS_BINDING_TOOL=
AC_CHECK_PROGS([DBUS_BINDING_TOOL], [dbus-binding-tool])
if test -z "$DBUS_BINDING_TOOL"; then
  AC_MSG_ERROR([dbus-binding-tool (from dbus-glib) is required])
fi
PYTHON=
AC_CHECK_PROGS([PYTHON], [python2.3 python2.4 python2.5 python])
if test -z "$PYTHON"; then
  AC_MSG_ERROR([Python is required to compile this package])
fi

dnl glibc backtrace functions
AC_CHECK_FUNCS(backtrace backtrace_symbols_fd)
AC_CHECK_HEADERS(execinfo.h)

dnl signal functions
AC_CHECK_FUNCS(signal)
AC_CHECK_HEADERS(signal.h)

AC_OUTPUT( Makefile \
	   telepathy-glib.pc \
	   telepathy-glib-unstable.pc \
	   docs/Makefile \
	   docs/reference/Makefile \
	   spec/Makefile \
	   telepathy-glib/Makefile \
	   tests/Makefile \
	   tools/Makefile \
	   m4/Makefile \
)
