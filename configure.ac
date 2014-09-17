AC_PREREQ([2.65])
AC_COPYRIGHT([
  Copyright (C) 2003-2007 Imendio AB
  Copyright (C) 2007-2013 Collabora Ltd.
])

# Making releases:
#   set the new version number:
#     odd minor -> development series
#     even minor -> stable series
#     increment micro for each release within a series
#   set nano_version to 0
#   make the release, tag it
#   set nano_version to 1

m4_define([tp_glib_major_version], [0])
m4_define([tp_glib_minor_version], [99])
m4_define([tp_glib_micro_version], [11])
m4_define([tp_glib_nano_version], [1])

# If library source has changed since last release, increment revision
# If interfaces have been added, removed or changed since last release,
#  increment current and set revision to 0
# If interfaces have been added since last release, increment age
# If interfaces have been removed since last release, set age to 0
#
# If interfaces have been added, release builds will fail unless you add a new
# file like telepathy-glib/versions/main-1.2.abi to add them to the official
# ABI of the next stable branch. This also forces binaries built against
# devel versions to be rebuilt (we don't guarantee that we won't add ABI in
# a development branch then remove it again, particularly if it was never
# seen in a release).

# libtelepathy-glib-1
m4_define([tp_glib_lt_current], [0])
m4_define([tp_glib_lt_revision], [0])
m4_define([tp_glib_lt_age], [0])

# The same, but for the generated code in libtelepathy-glib-1-dbus; this
# breaks ABI more often
m4_define([tp_glib_lt_dbus_current], [0])
m4_define([tp_glib_lt_dbus_revision], [0])
m4_define([tp_glib_lt_dbus_age], [0])

# The same for the telepathy-logger library
m4_define([tpl_lt_current], [0])
m4_define([tpl_lt_revision], [0])
m4_define([tpl_lt_age], [0])

# The same for the extensions library produced by --enable-public-extensions
m4_define([tpl_ext_lt_current], [0])
m4_define([tpl_ext_lt_revision], [0])
m4_define([tpl_ext_lt_age], [0])

# The same for telepathy-farstream
m4_define([tp_farstream_lt_current], [0])
m4_define([tp_farstream_lt_revision], [0])
m4_define([tp_farstream_lt_age], [0])

# Some magic
m4_define([tp_glib_base_version],
          [tp_glib_major_version.tp_glib_minor_version.tp_glib_micro_version])
m4_define([tp_glib_version],
          [m4_if(tp_glib_nano_version, 0, [tp_glib_base_version],
              [tp_glib_base_version].[tp_glib_nano_version])])

AC_INIT([Telepathy-GLib], [tp_glib_version],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=Telepathy&component=tp-glib])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.12 -Wno-portability subdir-objects parallel-tests tar-ustar])
AC_CONFIG_HEADERS(config.h)

AM_SILENT_RULES

dnl check for tools
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AM_PROG_LIBTOOL
AC_PROG_MKDIR_P
# pkg-config 0.21 adds CFLAGS from Requires.private, which we need
PKG_PROG_PKG_CONFIG([0.21])

GTK_DOC_CHECK([1.17],[--flavour no-tmpl])

GOBJECT_INTROSPECTION_CHECK([1.41])

TP_MAJOR_VERSION=tp_glib_major_version
TP_MINOR_VERSION=tp_glib_minor_version
TP_MICRO_VERSION=tp_glib_micro_version
AC_SUBST([TP_MAJOR_VERSION])
AC_SUBST([TP_MINOR_VERSION])
AC_SUBST([TP_MICRO_VERSION])

LT_CURRENT=tp_glib_lt_current
LT_REVISION=tp_glib_lt_revision
LT_AGE=tp_glib_lt_age
AC_SUBST([LT_CURRENT])
AC_SUBST([LT_REVISION])
AC_SUBST([LT_AGE])

LT_DBUS_CURRENT=tp_glib_lt_dbus_current
LT_DBUS_REVISION=tp_glib_lt_dbus_revision
LT_DBUS_AGE=tp_glib_lt_dbus_age
AC_SUBST([LT_DBUS_CURRENT])
AC_SUBST([LT_DBUS_REVISION])
AC_SUBST([LT_DBUS_AGE])

TPL_LT_CURRENT=tpl_lt_current
TPL_LT_REVISION=tpl_lt_revision
TPL_LT_AGE=tpl_lt_age
AC_SUBST([TPL_LT_CURRENT])
AC_SUBST([TPL_LT_REVISION])
AC_SUBST([TPL_LT_AGE])
# The ABI version is the end of the SONAME on Linux, and would appear in the
# name of a plugin directory, for instance
TPL_ABI_VERSION=`expr ${TPL_LT_CURRENT} - ${TPL_LT_AGE}`
AC_SUBST([TPL_ABI_VERSION])

TPL_EXT_LT_CURRENT=tpl_ext_lt_current
TPL_EXT_LT_REVISION=tpl_ext_lt_revision
TPL_EXT_LT_AGE=tpl_ext_lt_age
AC_SUBST([TPL_EXT_LT_CURRENT])
AC_SUBST([TPL_EXT_LT_REVISION])
AC_SUBST([TPL_EXT_LT_AGE])

TPFS_LT_CURRENT=tp_farstream_lt_current
TPFS_LT_REVISION=tp_farstream_lt_revision
TPFS_LT_AGE=tp_farstream_lt_age
AC_SUBST([TPFS_LT_CURRENT])
AC_SUBST([TPFS_LT_REVISION])
AC_SUBST([TPFS_LT_AGE])

dnl optimizations, etc.
COMPILER_OPTIMISATIONS
COMPILER_COVERAGE
LINKER_OPTIMISATIONS
LINKER_VERSION_SCRIPT

ifelse(tp_glib_nano_version, 0,
[ # tp-glib is version x.y.z - "official release",
  # disable extra checks by default
  official_release=yes
  AC_ARG_ENABLE([fatal-warnings],
    [AC_HELP_STRING([--enable-fatal-warnings],
                    [make various warnings fatal])],
    [],
    [enable_fatal_warnings=no])
],
[ # tp-glib is version x.y.z.1 - development snapshot,
  # enable extra checks by default
  official_release=no
  AC_ARG_ENABLE([fatal-warnings],
    [AC_HELP_STRING([--disable-fatal-warnings],
                    [make various warnings non-fatal])],
    [],
    [enable_fatal_warnings=yes])
])

AC_SUBST([enable_fatal_warnings])

TP_COMPILER_WARNINGS([ERROR_CFLAGS], [test "x$enable_fatal_warnings" = xyes],
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
AS_IF([test "x$enable_fatal_warnings" = xyes],
[
  TP_ADD_LINKER_FLAG([ERROR_LDFLAGS], [-Wl,--no-copy-dt-needed-entries])

  # --no-copy-dt-needed-entries is the new name for --no-add-needed which
  # is much more widely supported
  AS_CASE(["$ERROR_LDFLAGS"],
    [*-Wl,--no-copy-dt-needed-entries*],
      [],
    [*],
      [TP_ADD_LINKER_FLAG([ERROR_LDFLAGS], [-Wl,--no-add-needed])])
])

AS_IF([test "x$enable_fatal_warnings" = xyes],
  [ERROR_SCANNERFLAGS=--warn-error],
  [ERROR_SCANNERFLAGS=])
AC_SUBST([ERROR_SCANNERFLAGS])

# Only use this where we really do want things to depend on whether it's
# a release or not (like ABI-stability enforcement). For fatal warnings,
# use ${enable_fatal_warnings} instead.
AM_CONDITIONAL([OFFICIAL_RELEASE], [test "x$official_release" = xyes])

AC_ARG_ENABLE(debug-cache,
  AS_HELP_STRING([--enable-debug-cache],
                 [compile with code to unconditionally cache all debug messages whether or not they are actually emitted]),
                 [enable_debug_cache=$enableval],
                 [enable_debug_cache=yes])

AS_IF([test x"$enable_debug_cache" != xno],
  [AC_DEFINE([ENABLE_DEBUG_CACHE], [1], [Define to enable debug message cache])])

AC_ARG_ENABLE(backtrace,
  AC_HELP_STRING([--enable-backtrace],[enable printing out the backtrace in case of crash (in most GLib connection managers)]),
    enable_backtrace=$enableval, enable_backtrace=no )

AS_IF([test x$enable_backtrace = xyes],
  [AC_DEFINE([ENABLE_BACKTRACE], [], [Enable backtrace output on crashes])])

AC_SUBST(COVERAGE_CFLAGS)

AC_ARG_ENABLE([installed-tests],
 AC_HELP_STRING([--enable-installed-tests],
 [make tests installable]),
 [installed_tests=$enableval], [installed_tests=no])

AS_IF([test x$installed_tests = xyes],
  [AC_DEFINE([ENABLE_INSTALLED_TESTS], [], [Make tests installable])])
AM_CONDITIONAL([ENABLE_INSTALLED_TESTS], [test "x$installed_tests" = xyes])

AC_ARG_ENABLE([installed-examples],
  AC_HELP_STRING([--enable-installed-examples],
    [install examples as well as the library]),
    [enable_installed_examples="$enableval"],
    [enable_installed_examples=$installed_tests])
AM_CONDITIONAL([INSTALL_EXAMPLES], [test "$enable_installed_examples" = yes])

AS_IF(
  [test x$installed_tests = xyes && test x$enable_installed_examples != xyes],
  [AC_MSG_ERROR([examples must be installed when tests are installed.])])

dnl docs/Makefile.am needs to know whether it's an out of tree build
dnl (srcdir != builddir)
AM_CONDITIONAL([OUT_OF_TREE_BUILD], [test "z$ac_srcdir" != z.])

tpglibtestsdir=${libdir}/telepathy-glib-1-tests
AC_SUBST(tpglibtestsdir)

dnl Check for Glib
PKG_CHECK_MODULES(GLIB,
  [glib-2.0 >= 2.39.90, gobject-2.0 >= 2.39.90, gio-2.0 >= 2.39.90])

AC_DEFINE([GLIB_VERSION_MIN_REQUIRED], [GLIB_VERSION_2_40], [Ignore post 2.40 deprecations])
AC_DEFINE([GLIB_VERSION_MAX_ALLOWED], [GLIB_VERSION_2_40], [Prevent post 2.40 APIs])

dnl Check for GIO-Unix
PKG_CHECK_MODULES(GIO_UNIX, [gio-unix-2.0],
  have_gio_unix=yes, have_gio_unix=no)

AS_IF([test "x$have_gio_unix" = "xyes"],
  [AC_DEFINE(HAVE_GIO_UNIX, [], [Define if GIO-Unix is available])

  GLIB_CFLAGS="$GLIB_CFLAGS $GIO_UNIX_CFLAGS"
  GLIB_LIBS="$GLIB_LIBS $GIO_UNIX_LIBS"
  ])

AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`
AC_SUBST(GLIB_MKENUMS)

GLIB_GSETTINGS

dnl Check for D-Bus
PKG_CHECK_MODULES(DBUS, [dbus-1 >= 1.1, dbus-glib-1 >= 0.102])

AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

AC_ARG_ENABLE(vala-bindings,
  AC_HELP_STRING([--enable-vala-bindings],
                 [build the EXPERIMENTAL Vala bindings]),
                  enable_vala_bindings=$enableval,
                  enable_vala_bindings=no )

AS_IF([test x$enable_vala_bindings = xyes],
  [
  AS_IF([test "x$INTROSPECTION_SCANNER" = x],
    [AC_MSG_ERROR([GObject-Introspection must be enabled for Vala bindings])])

  AM_PROG_VALAC([0.16.0])
  have_vala=yes

  AC_PATH_PROG(VALAC, valac, valac)
  AC_SUBST(VALAC)
  AC_SUBST(VALAFLAGS)

  AC_PATH_PROG([VAPIGEN], [vapigen], [false])

  AS_IF([test "x$VAPIGEN" = "xfalse"],
    [AC_MSG_ERROR([vapigen not found. Was vala compiled with --enable-vapigen?])])

  AC_SUBST(VAPIGEN)
  ])

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
AS_IF([test -z "$XSLTPROC"],
  [AC_MSG_ERROR([xsltproc (from the libxslt source package) is required])])
AM_PATH_PYTHON([2.5])

dnl glibc backtrace functions
AC_CHECK_FUNCS(backtrace backtrace_symbols_fd)
AC_CHECK_HEADERS(execinfo.h)

dnl signal functions
AC_CHECK_FUNCS(signal)
AC_CHECK_HEADERS(signal.h)

HAVE_LD_VERSION_SCRIPT=no
AS_IF([test -n "$VERSION_SCRIPT_ARG"], [HAVE_LD_VERSION_SCRIPT=yes])
AC_CHECK_PROGS([NM], [nm])
AS_IF([test -z "$NM"], [HAVE_LD_VERSION_SCRIPT=no])
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
     [[g_object_unref (g_file_new_for_path ("/"));]])
  ],
  [tp_cxx_works=yes],
  [tp_cxx_works=no])
LIBS="$_TP_OLD_LIBS"
CXXFLAGS="$_TP_OLD_CXXFLAGS"
AC_LANG_POP([C++])
AM_CONDITIONAL([HAVE_CXX], [test $tp_cxx_works = yes])

IT_PROG_INTLTOOL([0.35.0])

AC_ARG_ENABLE([logger],
  [AS_HELP_STRING([--enable-logger],
    [build the Telepathy Logger (default=auto)])],
  [],
  [enable_logger=auto])

AS_IF([test "x$enable_logger" = xauto],
  [
    PKG_CHECK_MODULES([LOGGER_DEPENDENCIES],
      [sqlite3, libxml-2.0],
      [enable_logger=yes],
      [enable_logger=no])
  ])

AS_IF([test "x$enable_logger" = xyes],
  [
    PKG_CHECK_MODULES([LIBXML], [libxml-2.0])
    PKG_CHECK_MODULES([SQLITE], [sqlite3])

    TPL_CFLAGS="$DBUS_CFLAGS $GLIB_CFLAGS $LIBXML_CFLAGS $SQLITE_CFLAGS"
    TPL_LIBS="$DBUS_LIBS $GLIB_LIBS $LIBXML_LIBS $SQLITE_LIBS"
    AC_SUBST([TPL_CFLAGS])
    AC_SUBST([TPL_LIBS])
  ])

AM_CONDITIONAL([ENABLE_LOGGER], [test "x$enable_logger" = xyes])

AC_ARG_ENABLE([public_extensions],
  AS_HELP_STRING([--enable-public-extensions],
    [build a public wrapper for the Telepathy Logger extensions library]),
    [],
    [enable_public_extensions=no])

AS_IF([test x$enable_public_extensions = xyes],
  [AC_DEFINE(ENABLE_PUBLIC_EXTENSIONS, [], [Enable Public Extensions])])
AM_CONDITIONAL([ENABLE_PUBLIC_EXTENSIONS],
  [test "x$enable_public_extensions" = xyes])

AC_ARG_ENABLE([farstream],
  [AS_HELP_STRING([--enable-farstream],
    [build the Telepathy-Farstream VoIP library (default=auto)])],
  [],
  [enable_farstream=auto])

AS_CASE([$enable_farstream],
  [yes],
    [PKG_CHECK_MODULES([FARSTREAM], [farstream-0.2])],
  [auto],
    [
      PKG_CHECK_MODULES([FARSTREAM],
        [farstream-0.2],
        [enable_farstream=yes],
        [enable_farstream=no])
    ])

AM_CONDITIONAL([ENABLE_FARSTREAM], [test "x$enable_farstream" = xyes])

AC_CONFIG_FILES([
	   Makefile \
	   data/Makefile \
	   docs/Makefile \
	   docs/reference/Makefile \
	   docs/reference/libtelepathy-logger/Makefile \
	   docs/reference/telepathy-farstream/Makefile \
	   docs/reference/telepathy-glib/Makefile \
	   docs/reference/telepathy-glib/version.xml \
	   examples/Makefile \
	   examples/client/Makefile \
	   examples/client/js/Makefile \
	   examples/client/python/Makefile \
	   examples/client/stream-tubes/Makefile \
	   examples/client/dbus-tubes/Makefile \
	   examples/cm/Makefile \
	   examples/cm/call/Makefile \
	   examples/cm/channelspecific/Makefile \
	   examples/cm/contactlist/Makefile \
	   examples/cm/echo-message-parts/Makefile \
	   examples/cm/extended/Makefile \
	   examples/cm/no-protocols/Makefile \
	   examples/extensions/Makefile \
	   po/Makefile.in \
	   spec/Makefile \
	   telepathy-farstream/Makefile \
	   telepathy-farstream/examples/Makefile \
	   telepathy-farstream/examples/python/Makefile \
	   telepathy-farstream/telepathy-farstream-1.pc \
	   telepathy-farstream/telepathy-farstream-uninstalled-1.pc \
	   telepathy-glib/Makefile \
	   telepathy-glib/telepathy-glib-1-dbus.pc \
	   telepathy-glib/telepathy-glib-1-dbus-uninstalled.pc \
	   telepathy-glib/telepathy-glib-1.pc \
	   telepathy-glib/telepathy-glib-1-uninstalled.pc \
	   telepathy-glib/version.h \
	   telepathy-logger/Makefile \
	   telepathy-logger/telepathy-logger-1.pc \
	   telepathy-logger/telepathy-logger-1-uninstalled.pc \
	   tests/Makefile \
	   tests/lib/Makefile \
	   tests/dbus/Makefile \
	   tests/logger/Makefile \
	   tests/logger/dbus/Makefile \
	   tests/suppressions/Makefile \
	   tests/tools/Makefile \
	   tools/Makefile \
	   m4/Makefile \
	   vala/Makefile
])
AC_OUTPUT

echo "
Configure summary:

	Compiler....................:  ${CC}
	Compiler Flags..............:  ${CFLAGS} ${ERROR_CFLAGS}
	Prefix......................:  ${prefix}
	Coding style checks.........:  ${ENABLE_CODING_STYLE_CHECKS}
	Bugreporting URL............:  ${PACKAGE_BUGREPORT}
	Public extensions library...:  ${enable_public_extensions}
	Introspection support.......:  ${found_introspection}
"
