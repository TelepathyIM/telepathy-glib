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
  Copyright (C) 2007-2009 Collabora Ltd.
])

# Minimal version required
GLIB_REQUIRED=2.22.0
TELEPATHY_GLIB_REQUIRED=0.9.0

# Use --enable-maintainer-mode to disabled deprecated symbols
GNOME_MAINTAINER_MODE_DEFINES

# Warning if GLib/GDK/GTK headers are included
AC_DEFINE(G_DISABLE_SINGLE_INCLUDES, [], [Disable single includes for GLib])
AC_DEFINE(GDK_PIXBUF_DISABLE_SINGLE_INCLUDES, [], [Disable single includes for GDK pixbuf])
#AC_DEFINE(GTK_DISABLE_SINGLE_INCLUDES, [], [Disable single includes for GTK])

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
#AM_PATH_PYTHON([2.3])

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
# Disabled because some GTK+ headers (like gtkitemfactory.h) are bugged :(
#AS_COMPILER_FLAG(-Wstrict-prototypes, ERROR_CFLAGS="$ERROR_CFLAGS -Wstrict-prototypes")

AC_SUBST(ERROR_CFLAGS)

# -----------------------------------------------------------
# Pkg-Config dependency checks
# -----------------------------------------------------------

PKG_CHECK_MODULES(LIBTPL,
[
   glib-2.0 >= $GLIB_REQUIRED
   gobject-2.0
   libxml-2.0
   telepathy-glib >= $TELEPATHY_GLIB_REQUIRED
])

PKG_CHECK_MODULES(TPL,
[
   glib-2.0 >= $GLIB_REQUIRED
   libxml-2.0
   gobject-2.0
   dbus-glib-1
   telepathy-glib >= $TELEPATHY_GLIB_REQUIRED
])

#PKG_CHECK_MODULES(LIBNOTIFY, libnotify >= $LIBNOTIFY_REQUIRED)

# -----------------------------------------------------------
# Enable debug
# -----------------------------------------------------------

AC_ARG_ENABLE(debug,
  AC_HELP_STRING([--disable-debug],[compile without debug code]),
    enable_debug=$enableval, enable_debug=yes )

if test x$enable_debug = xyes; then
  AC_DEFINE(ENABLE_DEBUG, [], [Enable debug code])
fi

#DISABLED## -----------------------------------------------------------
#DISABLED## Language Support
#DISABLED## -----------------------------------------------------------
#DISABLED#
#DISABLED#GETTEXT_PACKAGE=telepathy_logger
#DISABLED#AC_SUBST(GETTEXT_PACKAGE)
#DISABLED#AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[Gettext package name])
#DISABLED#
#DISABLED#AM_GLIB_GNU_GETTEXT
#DISABLED#
#DISABLED## -----------------------------------------------------------
#DISABLED## Connectivity integration
#DISABLED## -----------------------------------------------------------
#DISABLED#AC_ARG_WITH(connectivity,
#DISABLED#              AS_HELP_STRING([--with-connectivity=@<:@nm/connman/auto/no@:>@],
#DISABLED#                             [build with connectivity support]), ,
#DISABLED#                             with_connectivity=auto)
#DISABLED#
#DISABLED#if test "x$with_connectivity" = "xno"; then
#DISABLED#   have_nm=no
#DISABLED#   have_connman=no
#DISABLED#
#DISABLED#elif test "x$with_connectivity" = "xconnman"; then
#DISABLED#
#DISABLED#   PKG_CHECK_MODULES(CONNMAN,
#DISABLED#   [
#DISABLED#      dbus-glib-1
#DISABLED#   ], have_connman="yes", have_connman="no")
#DISABLED#
#DISABLED#   if test "x$have_connman" = "xyes"; then
#DISABLED#      AC_DEFINE(HAVE_CONNMAN, 1, [Define if you have connman dependencies])
#DISABLED#      have_nm=no
#DISABLED#   fi
#DISABLED#
#DISABLED#else
#DISABLED#
#DISABLED#   PKG_CHECK_MODULES(NETWORK_MANAGER,
#DISABLED#   [
#DISABLED#      libnm_glib >= $NETWORK_MANAGER_REQUIRED
#DISABLED#   ], have_nm="yes", have_nm="no")
#DISABLED#
#DISABLED#   if test "x$have_nm" = "xyes"; then
#DISABLED#      AC_DEFINE(HAVE_NM, 1, [Define if you have libnm-glib])
#DISABLED#      have_connman=no
#DISABLED#   else
#DISABLED#      PKG_CHECK_MODULES(NETWORK_MANAGER,
#DISABLED#      [
#DISABLED#          libnm-glib >= $NETWORK_MANAGER_REQUIRED
#DISABLED#      ], have_nm="yes", have_nm="no")
#DISABLED#
#DISABLED#      if test "x$have_nm" = "xyes"; then
#DISABLED#      AC_DEFINE(HAVE_NM, 1, [Define if you have libnm-glib])
#DISABLED#      have_connman=no
#DISABLED#      fi
#DISABLED#   fi
#DISABLED#fi
#DISABLED#
#DISABLED#if test "x$with_connectivity" = "xconnman" -a "x$have_connman" != "xyes"; then
#DISABLED#   AC_MSG_ERROR([Couldn't find connman dependencies.])
#DISABLED#fi
#DISABLED#
#DISABLED#if test "x$with_connectivity" = "xnm" -a "x$have_nm" != "xyes"; then
#DISABLED#   AC_MSG_ERROR([Couldn't find libnm-glib dependencies.])
#DISABLED#fi
#DISABLED#
#DISABLED#AM_CONDITIONAL(HAVE_NM, test "x$have_nm" = "xyes")
#DISABLED#AM_CONDITIONAL(HAVE_CONNMAN, test "x$have_connman" = "xyes")
#DISABLED#
#DISABLED## -----------------------------------------------------------
#DISABLED## Webkit
#DISABLED## -----------------------------------------------------------
#DISABLED#AC_ARG_ENABLE(webkit,
#DISABLED#              AS_HELP_STRING([--enable-webkit=@<:@no/yes/auto@:>@],
#DISABLED#                             [build with webkit support]), ,
#DISABLED#                             enable_webkit=auto)
#DISABLED#
#DISABLED#if test "x$enable_webkit" != "xno"; then
#DISABLED#   PKG_CHECK_MODULES(WEBKIT,
#DISABLED#   [
#DISABLED#      webkit-1.0 >= $WEBKIT_REQUIRED
#DISABLED#   ], have_webkit="yes", have_webkit="no")
#DISABLED#
#DISABLED#   if test "x$have_webkit" = "xyes"; then
#DISABLED#      AC_DEFINE(HAVE_WEBKIT, 1, [Define if you have libwebkitgtk])
#DISABLED#   fi
#DISABLED#else
#DISABLED#   have_webkit=no
#DISABLED#fi
#DISABLED#
#DISABLED#if test "x$enable_webkit" = "xyes" -a "x$have_webkit" != "xyes"; then
#DISABLED#   AC_MSG_ERROR([Couldn't find webkit dependencies.])
#DISABLED#fi
#DISABLED#AM_CONDITIONAL(HAVE_WEBKIT, test "x$have_webkit" = "xyes")
#DISABLED#
#DISABLED## -----------------------------------------------------------
#DISABLED## spellchecking checks: enchant and iso-codes
#DISABLED## -----------------------------------------------------------
#DISABLED#AC_ARG_ENABLE(spell,
#DISABLED#              AS_HELP_STRING([--enable-spell=@<:@no/yes/auto@:>@],
#DISABLED#                             [Enable spell checking]), ,
#DISABLED#                             enable_spell=auto)
#DISABLED#
#DISABLED#if test "x$enable_spell" != "xno"; then
#DISABLED#   PKG_CHECK_MODULES(ENCHANT,
#DISABLED#   [
#DISABLED#      enchant >= $ENCHANT_REQUIRED,
#DISABLED#      iso-codes >= $ISO_CODES_REQUIRED
#DISABLED#   ], have_enchant="yes", have_enchant="no")
#DISABLED#
#DISABLED#   if test "x$have_enchant" = "xyes"; then
#DISABLED#      AC_MSG_CHECKING([whether iso-codes has iso-639 and iso-3166 domains])
#DISABLED#      if $PKG_CONFIG --variable=domains iso-codes | grep 639 > /dev/null && \
#DISABLED#         $PKG_CONFIG --variable=domains iso-codes | grep 3166 > /dev/null ; then
#DISABLED#            AC_DEFINE_UNQUOTED(ISO_CODES_PREFIX, ["`$PKG_CONFIG --variable=prefix iso-codes`"], [ISO codes prefix])
#DISABLED#            AC_DEFINE(HAVE_ENCHANT, [], [Define if you have libenchant])
#DISABLED#            result=yes
#DISABLED#      else
#DISABLED#            result=no
#DISABLED#            have_enchant="no"
#DISABLED#      fi
#DISABLED#      AC_MSG_RESULT([$result])
#DISABLED#   fi
#DISABLED#else
#DISABLED#   have_enchant=no
#DISABLED#fi
#DISABLED#
#DISABLED#if test "x$enable_spell" = "xyes" -a "x$have_enchant" != "xyes"; then
#DISABLED#   AC_MSG_ERROR([Couldn't find spell dependencies.])
#DISABLED#fi
#DISABLED#
#DISABLED#AM_CONDITIONAL(HAVE_ENCHANT, test "x$have_enchant" = "xyes")
#DISABLED#
#DISABLED## -----------------------------------------------------------
#DISABLED## Map view checks: libchamplain
#DISABLED## -----------------------------------------------------------
#DISABLED#AC_ARG_ENABLE(map,
#DISABLED#              AS_HELP_STRING([--enable-map=@<:@no/yes/auto@:>@],
#DISABLED#                             [Enable map view]), ,
#DISABLED#                             enable_map=auto)
#DISABLED#
#DISABLED#if test "x$enable_map" != "xno"; then
#DISABLED#   PKG_CHECK_MODULES(LIBCHAMPLAIN,
#DISABLED#   [
#DISABLED#      champlain-0.4 >= $LIBCHAMPLAIN_REQUIRED,
#DISABLED#      champlain-gtk-0.4 >= $LIBCHAMPLAIN_GTK_REQUIRED
#DISABLED#      clutter-gtk-0.10 >= $CLUTTER_GTK_REQUIRED
#DISABLED#   ], have_libchamplain="yes", have_libchamplain="no")
#DISABLED#
#DISABLED#   if test "x$have_libchamplain" = "xyes"; then
#DISABLED#      AC_DEFINE(HAVE_LIBCHAMPLAIN, 1, [Define if you have libchamplain])
#DISABLED#   fi
#DISABLED#else
#DISABLED#   have_libchamplain=no
#DISABLED#fi
#DISABLED#
#DISABLED#if test "x$enable_map" = "xyes" -a "x$have_libchamplain" != "xyes"; then
#DISABLED#   AC_MSG_ERROR([Couldn't find map view dependencies.])
#DISABLED#fi
#DISABLED#
#DISABLED#AM_CONDITIONAL(HAVE_LIBCHAMPLAIN, test "x$have_libchamplain" = "xyes")
#DISABLED#
#DISABLED## -----------------------------------------------------------
#DISABLED## location checks: geoclue
#DISABLED## -----------------------------------------------------------
#DISABLED#AC_ARG_ENABLE(location,
#DISABLED#              AS_HELP_STRING([--enable-location=@<:@no/yes/auto@:>@],
#DISABLED#                             [Enable location awareness]), ,
#DISABLED#                             enable_location=auto)
#DISABLED#
#DISABLED#if test "x$enable_location" != "xno"; then
#DISABLED#   PKG_CHECK_MODULES(GEOCLUE,
#DISABLED#   [
#DISABLED#      geoclue >= $GEOCLUE_REQUIRED
#DISABLED#   ], have_geoclue="yes", have_geoclue="no")
#DISABLED#
#DISABLED#   if test "x$have_geoclue" = "xyes"; then
#DISABLED#      AC_DEFINE(HAVE_GEOCLUE, 1, [Define if you have geoclue])
#DISABLED#   fi
#DISABLED#else
#DISABLED#   have_geoclue="no"
#DISABLED#fi
#DISABLED#
#DISABLED#if test "x$enable_location" = "xyes" -a "x$have_geoclue" != "xyes"; then
#DISABLED#   AC_MSG_ERROR([Couldn't find location dependencies.])
#DISABLED#fi
#DISABLED#
#DISABLED#AM_CONDITIONAL(HAVE_GEOCLUE, test "x$have_geoclue" = "xyes")
#DISABLED#AC_SUBST(GEOCLUE_CFLAGS)
#DISABLED#AC_SUBST(GEOCLUE_LIBS)
#DISABLED#
#DISABLED## -----------------------------------------------------------
#DISABLED## moblin widgets support
#DISABLED## -----------------------------------------------------------
#DISABLED#AC_ARG_ENABLE(moblin,
#DISABLED#              AS_HELP_STRING([--enable-moblin=@<:no/yes@:>@],
#DISABLED#                             [Enable moblin widgets]), ,
#DISABLED#                             enable_moblin=no)
#DISABLED#
#DISABLED#if test "x$enable_moblin" != "xno"; then
#DISABLED#    PKG_CHECK_MODULES(MOBLIN,
#DISABLED#    [
#DISABLED#       nbtk-gtk-1.2
#DISABLED#    ], have_moblin="yes", have_moblin="no")
#DISABLED#
#DISABLED#    if test "x$have_moblin" = "xyes"; then
#DISABLED#       AC_DEFINE(HAVE_MOBLIN, 1, [Define if you have moblin])
#DISABLED#    fi
#DISABLED#else
#DISABLED#   have_moblin="no"
#DISABLED#fi
#DISABLED#
#DISABLED#if test "x$enable_moblin" = "xyes" -a "x$have_moblin" != "xyes"; then
#DISABLED#   AC_MSG_ERROR([Couldn't find moblin dependencies.])
#DISABLED#fi
#DISABLED#
#DISABLED#AM_CONDITIONAL(HAVE_MOBLIN, test "x$have_moblin" = "xyes")
#DISABLED#AC_SUBST(MOBLIN_CFLAGS)
#DISABLED#AC_SUBST(MOBLIN_LIBS)
#DISABLED#
#DISABLED## -----------------------------------------------------------
#DISABLED## nautilus-sendto
#DISABLED## -----------------------------------------------------------
#DISABLED#AC_ARG_ENABLE(nautilus-sendto,
#DISABLED#              AS_HELP_STRING([--enable-nautilus-sendto=@<:@no/yes/auto@:>@],
#DISABLED#                             [build nautilus-sendto plugin]), ,
#DISABLED#                             enable_nautilus_sendto=auto)
#DISABLED#
#DISABLED#if test "x$enable_nautilus_sendto" != "xno"; then
#DISABLED#   PKG_CHECK_MODULES(NST,
#DISABLED#   [
#DISABLED#      nautilus-sendto >= $NAUTILUS_SENDTO_REQUIRED
#DISABLED#   ], have_nst="yes", have_nst="no")
#DISABLED#else
#DISABLED#   have_nst=no
#DISABLED#fi
#DISABLED#
#DISABLED#if test "x$enable_nautilus_sendto" = "xyes" -a "x$have_nst" != "xyes"; then
#DISABLED#   AC_MSG_ERROR([Couldn't find nautilus-sendto dependencies.])
#DISABLED#fi
#DISABLED#
#DISABLED#AM_CONDITIONAL(HAVE_NST, test "x$have_nst" = "xyes")

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
   src/libtelepathy-logger.pc
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
