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
m4_define([tp_glib_minor_version], [15])
m4_define([tp_glib_micro_version], [1])
m4_define([tp_glib_nano_version], [1])

# If library source has changed since last release, increment revision
# If interfaces have been added, removed or changed since last release,
#  increment current and set revision to 0
# If interfaces have been added since last release, increment age
# If interfaces have been removed since last release, set age to 0
#
# If interfaces have been added, release builds will fail unless you add a new
# file like telepathy-glib/versions/0.7.3.abi to add them to the official ABI.
# This also forces binaries built against devel versions to be rebuilt
# (we don't guarantee that we won't add ABI then remove it again, if it was
# never seen in a release).

m4_define([tp_glib_lt_current], [58])
m4_define([tp_glib_lt_revision], [0])
m4_define([tp_glib_lt_age], [58])

# Some magic
m4_define([tp_glib_base_version],
          [tp_glib_major_version.tp_glib_minor_version.tp_glib_micro_version])
m4_define([tp_glib_version],
          [m4_if(tp_glib_nano_version, 0, [tp_glib_base_version],
              [tp_glib_base_version].[tp_glib_nano_version])])

AC_INIT([Telepathy-GLib], [tp_glib_version],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=Telepathy&component=tp-glib])

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 -Wno-portability])
AM_CONFIG_HEADER(config.h)

AM_SILENT_RULES

dnl check for tools
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AM_PROG_LIBTOOL
AM_PROG_MKDIR_P
# pkg-config 0.21 adds CFLAGS from Requires.private, which we need
PKG_PROG_PKG_CONFIG([0.21])

GTK_DOC_CHECK([1.15])

GOBJECT_INTROSPECTION_CHECK([0.9.6])

LT_CURRENT=tp_glib_lt_current
LT_REVISION=tp_glib_lt_revision
LT_AGE=tp_glib_lt_age
AC_SUBST([LT_CURRENT])
AC_SUBST([LT_REVISION])
AC_SUBST([LT_AGE])

dnl optimizations, etc.
COMPILER_OPTIMISATIONS
COMPILER_COVERAGE
LINKER_OPTIMISATIONS
LINKER_VERSION_SCRIPT

ifelse(tp_glib_nano_version, 0,
    [ official_release=yes ],
    [ official_release=no ])

TP_COMPILER_WARNINGS([ERROR_CFLAGS], [test "x$official_release" = xno],
  [all \
   extra \
   declaration-after-statement \
   shadow \
   strict-prototypes \
   missing-prototypes \
   sign-compare \
   nested-externs \
   pointer-arith \
   format-security \
   init-self],
  [missing-field-initializers \
   unused-parameter])
AC_SUBST([ERROR_CFLAGS])

# these aren't really error flags but they serve a similar purpose for us -
# making the toolchain stricter
if test "x$official_release" = xno; then
  TP_ADD_LINKER_FLAG([ERROR_LDFLAGS], [-Wl,--no-copy-dt-needed-entries])

  # --no-copy-dt-needed-entries is the new name for --no-add-needed which
  # is much more widely supported
  case "$ERROR_LDFLAGS" in
  (*-Wl,--no-copy-dt-needed-entries*)
    ;;
  (*)
    TP_ADD_LINKER_FLAG([ERROR_LDFLAGS], [-Wl,--no-add-needed])
    ;;
  esac
fi

AM_CONDITIONAL([OFFICIAL_RELEASE], [test "x$official_release" = xyes])

AC_ARG_ENABLE(debug,
  AC_HELP_STRING([--disable-debug],[compile without debug code (note that this will disable much of the debug code in all GLib connection managers)]),
    enable_debug=$enableval, enable_debug=yes )

AC_ARG_ENABLE(debug-cache,
  AS_HELP_STRING([--enable-debug-cache],
                 [compile with code to unconditionally cache all debug messages whether or not they are actually emitted]),
                 [enable_debug_cache=$enableval],
                 [enable_debug_cache=yes])

if test x"$enable_debug_cache" != xno
then
  AC_DEFINE([ENABLE_DEBUG_CACHE], [1], [Define to enable debug message cache])
fi

AC_ARG_ENABLE(handle-leak-debug,
  AC_HELP_STRING([--enable-handle-leak-debug],[compile with handle leak debugging code (the CM should be built with -rdynamic)]),
    enable_handle_leak_debug=$enableval, enable_handle_leak_debug=no )

AC_ARG_ENABLE(backtrace,
  AC_HELP_STRING([--enable-backtrace],[enable printing out the backtrace in case of crash (in most GLib connection managers)]),
    enable_backtrace=$enableval, enable_backtrace=no )

ifelse(tp_glib_nano_version, 0,
[ # tp-glib is version x.y.z - disable extra checks by default
  AC_ARG_ENABLE(coding-style-checks,
    AC_HELP_STRING([--enable-coding-style-checks],
                   [check coding style using grep]),
    [ENABLE_CODING_STYLE_CHECKS=$enableval], [ENABLE_CODING_STYLE_CHECKS=no] )
  AC_ARG_ENABLE(doc-checks,
    AC_HELP_STRING([--enable-doc-checks],
                   [check for complete documentation coverage]),
    [ENABLE_DOC_CHECKS=$enableval], [ENABLE_DOC_CHECKS=no] )
],
[ # tp-glib is version x.y.z.1 - enable extra checks by default
  AC_ARG_ENABLE(coding-style-checks,
    AC_HELP_STRING([--disable-coding-style-checks],
                   [do not check coding style using grep]),
    [ENABLE_CODING_STYLE_CHECKS=$enableval], [ENABLE_CODING_STYLE_CHECKS=yes])
  AC_ARG_ENABLE(doc-checks,
    AC_HELP_STRING([--disable-doc-checks],
                   [do not check for complete documentation coverage]),
    [ENABLE_DOC_CHECKS=$enableval], [ENABLE_DOC_CHECKS=yes])
])

AC_SUBST([ENABLE_CODING_STYLE_CHECKS])
AC_SUBST([ENABLE_DOC_CHECKS])

if test x$enable_debug = xyes; then
  AC_DEFINE(ENABLE_DEBUG, [], [Enable debug code])
fi

if test x$enable_backtrace = xyes; then
  AC_DEFINE(ENABLE_BACKTRACE, [], [Enable backtrace output on crashes])
fi

AC_SUBST(COVERAGE_CFLAGS)

AC_ARG_ENABLE([installed-examples],
  AC_HELP_STRING([--enable-installed-examples],
    [install examples as well as the library]),
    [enable_installed_examples="$enableval"],
    [enable_installed_examples=no])
AM_CONDITIONAL([INSTALL_EXAMPLES], [test "$enable_installed_examples" = yes])

dnl docs/Makefile.am needs to know whether it's an out of tree build
dnl (srcdir != builddir)
AM_CONDITIONAL([OUT_OF_TREE_BUILD], [test "z$ac_srcdir" != z.])

dnl Check for Glib
PKG_CHECK_MODULES(GLIB,
  [glib-2.0 >= 2.28.0, gobject-2.0 >= 2.28.0, gio-2.0 >= 2.28.0])

dnl Check for GIO-Unix
PKG_CHECK_MODULES(GIO_UNIX, [gio-unix-2.0],
  have_gio_unix=yes, have_gio_unix=no)

if test "x$have_gio_unix" = "xyes"; then
  AC_DEFINE(HAVE_GIO_UNIX, [], [Define if GIO-Unix is available])

  GLIB_CFLAGS="$GLIB_CFLAGS $GIO_UNIX_CFLAGS"
  GLIB_LIBS="$GLIB_LIBS $GIO_UNIX_LIBS"
fi

AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`
AC_SUBST(GLIB_MKENUMS)

dnl Check for D-Bus
PKG_CHECK_MODULES(DBUS, [dbus-1 >= 0.95, dbus-glib-1 >= 0.82])

AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

AC_ARG_ENABLE(vala-bindings,
  AC_HELP_STRING([--enable-vala-bindings],
                 [build the EXPERIMENTAL Vala bindings]),
                  enable_vala_bindings=$enableval,
                  enable_vala_bindings=no )

if test x$enable_vala_bindings = xyes; then
  if test "x$INTROSPECTION_SCANNER" = x; then
    AC_MSG_ERROR([GObject-Introspection must be enabled for Vala bindings])
  fi

  AM_PROG_VALAC([0.10.0])
  have_vala=yes

  AC_PATH_PROG(VALAC, valac, valac)
  AC_SUBST(VALAC)
  AC_SUBST(VALAFLAGS)

  AC_PATH_PROG([VAPIGEN], [vapigen], [false])

  if test "x$VAPIGEN" = "xfalse"; then
    AC_MSG_ERROR([vapigen not found. Was vala compiled with --enable-vapigen?])
  fi

  AC_SUBST(VAPIGEN)
fi

AM_CONDITIONAL([HAVE_VALA], [test "x$have_vala" = "xyes"])
AC_SUBST(HAVE_VALA)

dnl We want the examples to be able to use TP_GLIB_CFLAGS as though they were
dnl independent programs, but subst'ing TP_GLIB_LIBS interferes with
dnl dependency tracking
TP_GLIB_CFLAGS='-I${top_builddir} -I${top_srcdir}'
AC_SUBST(TP_GLIB_CFLAGS)

dnl Check for code generation tools
XSLTPROC=
AC_CHECK_PROGS([XSLTPROC], [xsltproc])
if test -z "$XSLTPROC"; then
  AC_MSG_ERROR([xsltproc (from the libxslt source package) is required])
fi
AM_PATH_PYTHON([2.5])

dnl glibc backtrace functions
AC_CHECK_FUNCS(backtrace backtrace_symbols_fd)
AC_CHECK_HEADERS(execinfo.h)

dnl signal functions
AC_CHECK_FUNCS(signal)
AC_CHECK_HEADERS(signal.h)

HAVE_LD_VERSION_SCRIPT=no
if test -n "$VERSION_SCRIPT_ARG"; then
	HAVE_LD_VERSION_SCRIPT=yes
fi
AC_CHECK_PROGS([NM], [nm])
if test -z "$NM"; then
	HAVE_LD_VERSION_SCRIPT=no
fi
AM_CONDITIONAL([HAVE_LD_VERSION_SCRIPT], [test $HAVE_LD_VERSION_SCRIPT = yes])

dnl Check for C++ compiler, for syntax checking
AC_PROG_CXX
AC_LANG_PUSH([C++])
_TP_OLD_LIBS="$LIBS"
LIBS="$GLIB_LIBS"
_TP_OLD_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$GLIB_CFLAGS"
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM(
     [[#include <gio/gio.h>]],
     [[g_type_init (); g_object_unref (g_file_new_for_path ("/"));]])
  ],
  [tp_cxx_works=yes],
  [tp_cxx_works=no])
LIBS="$_TP_OLD_LIBS"
CXXFLAGS="$_TP_OLD_CXXFLAGS"
AC_LANG_POP([C++])
AM_CONDITIONAL([HAVE_CXX], [test $tp_cxx_works = yes])

AC_OUTPUT( Makefile \
	   docs/Makefile \
	   docs/reference/Makefile \
	   examples/Makefile \
	   examples/client/Makefile \
	   examples/client/python/Makefile \
	   examples/client/stream-tubes/Makefile \
	   examples/cm/Makefile \
	   examples/cm/callable/Makefile \
	   examples/cm/channelspecific/Makefile \
	   examples/cm/contactlist/Makefile \
	   examples/cm/echo-message-parts/Makefile \
	   examples/cm/extended/Makefile \
	   examples/cm/no-protocols/Makefile \
	   examples/extensions/Makefile \
	   examples/future/Makefile \
	   examples/future/call-cm/Makefile \
	   extensions/Makefile \
	   spec/Makefile \
	   telepathy-glib/Makefile \
	   telepathy-glib/telepathy-glib.pc \
	   telepathy-glib/telepathy-glib-uninstalled.pc \
	   tests/Makefile \
	   tests/lib/Makefile \
	   tests/dbus/Makefile \
	   tests/tools/Makefile \
	   tools/Makefile \
	   m4/Makefile \
	   vala/Makefile
)
