m4_define([tpl_major_version], [0])
m4_define([tpl_minor_version], [6])
m4_define([tpl_micro_version], [0])
m4_define([tpl_nano_version], [0])

# If Tpl library source has changed since last release, increment revision
# If interfaces have been added, removed or changed since last release,
#  increment current and set revision to 0
# If interfaces have been added since last release, increment age
# If interfaces have been removed since last release, set age to 0
m4_define([tpl_lt_current], [5])
m4_define([tpl_lt_revision], [0])
m4_define([tpl_lt_age], [2])

# The same for the extensions library produced by --enable-public-extensions
m4_define([tpl_ext_lt_current], [3])
m4_define([tpl_ext_lt_revision], [0])
m4_define([tpl_ext_lt_age], [0])

dnl Display the nano_version only if it's not '0'
m4_define([tpl_base_version],
          [tpl_major_version.tpl_minor_version.tpl_micro_version])
m4_define([tpl_full_version],
          [m4_if(tpl_nano_version, 0, [tpl_base_version],
              [tpl_base_version].[tpl_nano_version])])

m4_define(tpl_maybe_datestamp,
          m4_esyscmd([if test x]tpl_nano_version[ != x0; then date +.%Y%m%d | tr -d '\n\r'; fi]))

m4_define(tpl_version, tpl_full_version[]tpl_maybe_datestamp)


AC_INIT([telepathy-logger],[tpl_version],[https://bugs.freedesktop.org/buglist.cgi?product=Telepathy&component=logger])
AM_SILENT_RULES([yes])

DBUS_SERVICES_DIR="$datadir/dbus-1/services"
AC_SUBST(DBUS_SERVICES_DIR)
AC_DEFINE_UNQUOTED(DBUS_SERVICES_DIR, "$DBUS_SERVICES_DIR", [Where services dir for DBUS is])

ifelse(tpl_nano_version, 0,
    [ official_release=yes ],
    [ official_release=no ])

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

AC_PREREQ([2.65])
AC_COPYRIGHT([
  Copyright (C) 2003-2007 Imendio AB
  Copyright (C) 2007-2011 Collabora Ltd.
])

# Minimal version required
GLIB_REQUIRED=2.25.11
DBUS_REQUIRED=1.1.0
DBUS_GLIB_REQUIRED=0.82

TELEPATHY_GLIB_REQUIRED=0.19.2
AC_DEFINE(TP_VERSION_MIN_REQUIRED, TP_VERSION_0_18, [Ignore post 0.18 deprecations])
AC_DEFINE(TP_VERSION_MAX_ALLOWED, TP_VERSION_0_20, [Prevent post 0.20 APIs])

# Warning if GLib/GDK/GTK headers are included
AC_DEFINE(G_DISABLE_SINGLE_INCLUDES, [], [Disable single includes for GLib])
AC_DEFINE(GDK_PIXBUF_DISABLE_SINGLE_INCLUDES, [], [Disable single includes for GDK pixbuf])

# Disable GLib deprecation warnings for now; GValueArray is deprecated but we
# need it for dbus-glib.
AC_DEFINE(GLIB_DISABLE_DEPRECATION_WARNINGS, 1, [Build with GLib deprecated])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_AUX_DIR(.)

AM_INIT_AUTOMAKE(1.9 dist-bzip2 no-define no-dist-gzip tar-ustar -Wno-portability)
AC_SEARCH_LIBS([strerror],[cposix])
AC_PROG_CC
AC_HEADER_STDC
LT_INIT
AM_PROG_MKDIR_P
AM_PATH_GLIB_2_0
AC_PATH_XTRA
IT_PROG_INTLTOOL([0.35.0])
AC_PATH_PROG(DBUS_BINDING_TOOL, dbus-binding-tool)
GLIB_GSETTINGS
GLIB_GENMARSHAL=`$PKG_CONFIG glib-2.0 --variable=glib_genmarshal`
AC_SUBST(GLIB_GENMARSHAL)

GOBJECT_INTROSPECTION_CHECK([0.9.6])

GTK_DOC_CHECK([1.10])

# -----------------------------------------------------------
# Check for code generation tools
# -----------------------------------------------------------
XSLTPROC=
AC_CHECK_PROGS([XSLTPROC], [xsltproc])
if test -z "$XSLTPROC"; then
  AC_MSG_ERROR([xsltproc (from libxslt) is required])
fi

AM_PATH_PYTHON([2.5])

# -----------------------------------------------------------
# Error flags
# -----------------------------------------------------------

TP_COMPILER_WARNINGS([ERROR_CFLAGS], [test "x$official_release" = xno],
  [all \
   extra \
   format-security \
   init-self \
   missing-prototypes \
   nested-externs \
   pointer-arith \
   shadow \
   sign-compare \
   strict-prototypes],
  [missing-field-initializers \
   unused-parameter])

# declaration-after-statement is deliberately not used here

AC_ARG_ENABLE(Werror,
  AS_HELP_STRING([--disable-Werror],[compile without -Werror (normally enabled in development builds)]),
    werror=$enableval, werror=yes)

TP_COMPILER_FLAG(-D_POSIX_SOURCE, ERROR_CFLAGS="$ERROR_CFLAGS -D_POSIX_SOURCE")
TP_COMPILER_FLAG(-std=c99, ERROR_CFLAGS="$ERROR_CFLAGS -std=c99")

AC_SUBST(ERROR_CFLAGS)

# -----------------------------------------------------------
# Pkg-Config dependency checks
# -----------------------------------------------------------

PKG_CHECK_MODULES(TPL,
[
   dbus-1 >= $DBUS_REQUIRED
   dbus-glib-1 >= $DBUS_GLIB_REQUIRED
   gio-2.0
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
   libxml-2.0
   sqlite3
   telepathy-glib >= $TELEPATHY_GLIB_REQUIRED
])


# -----------------------------------------------------------
# Enable public extensions library
# -----------------------------------------------------------

AC_ARG_ENABLE(public_extensions,
  AS_HELP_STRING([--enable-public-extensions],[build a public wrapper for the Telepathy extensions library]),
    enable_public_extensions=$enableval, enable_public_extensions=no )

if test x$enable_public_extensions = xyes; then
  AC_DEFINE(ENABLE_PUBLIC_EXTENSIONS, [], [Enable Public Extensions])
fi
AM_CONDITIONAL([ENABLE_PUBLIC_EXTENSIONS], [test "x$enable_public_extensions" = xyes])

# -----------------------------------------------------------
# Enable debug
# -----------------------------------------------------------

AC_ARG_ENABLE(debug,
  AS_HELP_STRING([--disable-debug],[compile without debug code]),
    enable_debug=$enableval, enable_debug=yes )

if test x$enable_debug = xyes; then
  AC_DEFINE(ENABLE_DEBUG, [], [Enable debug code])
fi
AM_CONDITIONAL([ENABLE_DEBUG], [test "x$enable_debug" = xyes])

# -----------------------------------------------------------
# Coding style checks
# -----------------------------------------------------------
AC_ARG_ENABLE(coding-style-checks,
  AS_HELP_STRING([--disable-coding-style-checks],
                 [do not check coding style using grep]),
    [ENABLE_CODING_STYLE_CHECKS=$enableval], [ENABLE_CODING_STYLE_CHECKS=yes])

AC_SUBST([ENABLE_CODING_STYLE_CHECKS])
# -----------------------------------------------------------

AM_CONDITIONAL([ENABLE_INSTALLED_TESTS], false)

AC_CONFIG_FILES([
   Makefile
   data/Makefile
   doc/Makefile
   doc/reference/Makefile
   doc/reference/libtelepathy-logger/Makefile
   extensions/Makefile
   extensions/tpl-extensions.pc
   po/Makefile.in
   src/Makefile
   telepathy-logger/Makefile
   telepathy-logger/telepathy-logger-0.2.pc
   telepathy-logger/telepathy-logger-0.2-uninstalled.pc
   tools/Makefile
   tests/Makefile
   tests/dbus/Makefile
   tests/lib/Makefile
   tests/suppressions/Makefile
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
