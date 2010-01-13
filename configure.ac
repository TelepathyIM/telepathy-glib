dnl If not 1, append datestamp to the version number
m4_define(tpl_released, 0)

m4_define([tpl_major_version], [0])
m4_define([tpl_minor_version], [1])
m4_define([tpl_micro_version], [0])
m4_define([tpl_nano_version], [0])

dnl Display the nano_version only if it's not '0'
m4_define([tpl_base_version],
          [tpl_major_version.tpl_minor_version.tpl_micro_version])
m4_define([tpl_full_version],
          [m4_if(tpl_nano_version, 0, [tpl_base_version],
              [tpl_base_version].[tpl_nano_version])])

m4_define(tpl_maybe_datestamp,
          m4_esyscmd([if test x]tpl_released[ != x1; then date +.%Y%m%d | tr -d '\n\r'; fi]))

m4_define(tpl_version, tpl_full_version[]tpl_maybe_datestamp)


AC_INIT(TelepathyLogger, tpl_version, http://telepathy.freedesktop.org/wiki/Logger)

DBUS_SERVICES_DIR="$datadir/dbus-1/services"
AC_SUBST(DBUS_SERVICES_DIR)
AC_DEFINE_UNQUOTED(DBUS_SERVICES_DIR, "$DBUS_SERVICES_DIR", [Where services dir for DBUS is])

AC_PREREQ(2.59)
AC_COPYRIGHT([
  Copyright (C) 2003-2007 Imendio AB
  Copyright (C) 2007-20010 Collabora Ltd.
])

# Minimal version required
GLIB_REQUIRED=2.22.0
TELEPATHY_GLIB_REQUIRED=0.9.0

# Use --enable-maintainer-mode to disabled deprecated symbols
GNOME_MAINTAINER_MODE_DEFINES

# Warning if GLib/GDK/GTK headers are included
AC_DEFINE(G_DISABLE_SINGLE_INCLUDES, [], [Disable single includes for GLib])
AC_DEFINE(GDK_PIXBUF_DISABLE_SINGLE_INCLUDES, [], [Disable single includes for GDK pixbuf])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_AUX_DIR(.)

GNOME_COMMON_INIT

AM_INIT_AUTOMAKE(1.9 dist-bzip2 no-define -Wno-portability)
AC_ISC_POSIX
AC_PROG_CC
AC_HEADER_STDC
AM_PROG_LIBTOOL
AM_PROG_MKDIR_P
AM_PATH_GLIB_2_0
AC_PATH_XTRA
IT_PROG_INTLTOOL([0.35.0])
GNOME_DOC_INIT([0.17.3])
IDT_COMPILE_WARNINGS
AC_PATH_PROG(DBUS_BINDING_TOOL, dbus-binding-tool)
AC_PATH_PROG(GCONFTOOL, gconftool-2)
AM_GCONF_SOURCE_2
GLIB_GENMARSHAL=`$PKG_CONFIG glib-2.0 --variable=glib_genmarshal`
AC_SUBST(GLIB_GENMARSHAL)

AC_CHECK_PROGS([XSLTPROC], [xsltproc])
if test -z "$XSLTPROC"; then
  AC_MSG_ERROR([xsltproc (from libxslt) is required])
fi

TPL_ARG_VALGRIND


# -----------------------------------------------------------
# Error flags
# -----------------------------------------------------------
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

ifelse(tpl_released, 1, [],
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

AS_COMPILER_FLAG(-D_POSIX_SOURCE, ERROR_CFLAGS="$ERROR_CFLAGS -D_POSIX_SOURCE")
AS_COMPILER_FLAG(-std=c99, ERROR_CFLAGS="$ERROR_CFLAGS -std=c99")
#AS_COMPILER_FLAG(-Wdeclaration-after-statement, ERROR_CFLAGS="$ERROR_CFLAGS -Wdeclaration-after-statement")
AS_COMPILER_FLAG(-Wshadow, ERROR_CFLAGS="$ERROR_CFLAGS -Wshadow")
AS_COMPILER_FLAG(-Wmissing-prototypes, ERROR_CFLAGS="$ERROR_CFLAGS -Wmissing-prototypes")
AS_COMPILER_FLAG(-Wmissing-declarations, ERROR_CFLAGS="$ERROR_CFLAGS -Wmissing-declarations")
AS_COMPILER_FLAG(-Wstrict-prototypes, ERROR_CFLAGS="$ERROR_CFLAGS -Wstrict-prototypes")

AC_SUBST(ERROR_CFLAGS)

# -----------------------------------------------------------
# Pkg-Config dependency checks
# -----------------------------------------------------------

PKG_CHECK_MODULES(LIBTPL,
[
   gconf-2.0
   gio-2.0
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
   libxml-2.0
   telepathy-glib >= $TELEPATHY_GLIB_REQUIRED
])

PKG_CHECK_MODULES(TPL,
[
   dbus-glib-1
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
   libxml-2.0
   telepathy-glib >= $TELEPATHY_GLIB_REQUIRED
])

# -----------------------------------------------------------
# Enable debug
# -----------------------------------------------------------

AC_ARG_ENABLE(debug,
  AC_HELP_STRING([--disable-debug],[compile without debug code]),
    enable_debug=$enableval, enable_debug=yes )

if test x$enable_debug = xyes; then
  AC_DEFINE(ENABLE_DEBUG, [], [Enable debug code])
fi

# -----------------------------------------------------------
# Coding style checks
# -----------------------------------------------------------
AC_ARG_ENABLE(coding-style-checks,
  AC_HELP_STRING([--disable-coding-style-checks],
                 [don't check coding style using grep]),
    [ENABLE_CODING_STYLE_CHECKS=$enableval], [ENABLE_CODING_STYLE_CHECKS=yes])

AC_SUBST([ENABLE_CODING_STYLE_CHECKS])
# -----------------------------------------------------------

SHAVE_INIT(.)

AC_OUTPUT([
   Makefile
   src/Makefile
   telepathy-logger/Makefile
   telepathy-logger/libtelepathy-logger.pc
   tools/Makefile
   shave
   shave-libtool
])

echo "
Configure summary:

	Compiler....................:  ${CC}
	Compiler Flags..............:  ${CFLAGS} ${ERROR_CFLAGS}
	Prefix......................:  ${prefix}
	Shaved build................:  ${enable_shave}
	Coding style checks.........:  ${ENABLE_CODING_STYLE_CHECKS}

    Features:
	Spell checking (enchant)....:  ${have_enchant}
	Display maps (libchamplain).:  ${have_libchamplain}
	Location awareness (Geoclue):  ${have_geoclue}
	Adium themes (Webkit).......:  ${have_webkit}
	Moblin widgets .............:  ${have_moblin}

    Connectivity:
	NetworkManager integration..:  ${have_nm}
	ConnMan integration.........:  ${have_connman}

    Extras:
	Nautilus-sendto plugin......:  ${have_nst}
"
