tfincludedir = $(includedir)/telepathy-farstream-1/telepathy-farstream
tfinclude_HEADERS = \
	telepathy-farstream.h \
	channel.h \
	content.h

apisources = \
	content.c \
	channel.c

libtelepathy_farstream_internal_la_SOURCES = \
	$(tfinclude_HEADERS) \
	$(apisources) \
	content-priv.h \
	channel-priv.h \
	call-channel.c \
	call-channel.h \
	call-content.h \
	call-content.c \
	call-stream.h \
	call-stream.c \
	call-priv.h \
	utils.h

nodist_libtelepathy_farstream_internal_la_SOURCES = $(BUILT_SOURCES)

lib_LTLIBRARIES = libtelepathy-farstream-1.la
noinst_LTLIBRARIES = libtelepathy-farstream-internal.la

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = telepathy-farstream-1.pc

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(GLIB_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GST_CFLAGS) \
	$(FARSTREAM_CFLAGS) \
	$(NULL)

AM_CFLAGS = \
	-DG_LOG_DOMAIN=\"tp-fs\" \
	$(ERROR_CFLAGS)

libtelepathy_farstream_internal_la_LIBADD = \
	$(top_builddir)/telepathy-glib/libtelepathy-glib-1.la \
	$(top_builddir)/telepathy-glib/libtelepathy-glib-1-dbus.la \
	$(top_builddir)/telepathy-glib/libtelepathy-glib-1-core.la \
	$(GLIB_LIBS) \
	$(DBUS_LIBS) \
	$(GST_LIBS) \
	$(FARSTREAM_LIBS) \
	$(NULL)

libtelepathy_farstream_internal_la_LDFLAGS = \
	-no-undefined \
	$(NULL)

libtelepathy_farstream_1_la_SOURCES =
nodist_libtelepathy_farstream_1_la_SOURCES =
libtelepathy_farstream_1_la_LIBADD = libtelepathy-farstream-internal.la
libtelepathy_farstream_1_la_LDFLAGS = \
	-version-info "$(TPFS_LT_CURRENT)":"$(TPFS_LT_REVISION)":"$(TPFS_LT_AGE)"

libtelepathy_farstream_1_la_DEPENDENCIES = \
	libtelepathy-farstream-internal.la \
	Makefile.am \
	$(NULL)

if HAVE_LD_VERSION_SCRIPT

libtelepathy_farstream_1_la_LDFLAGS += \
	$(VERSION_SCRIPT_ARG)=_gen/version-script.txt \
	$(NULL)

libtelepathy_farstream_1_la_DEPENDENCIES += \
	_gen/version-script.txt \
	$(NULL)

else

libtelepathy_farstream_1_la_LDFLAGS +=
	-export-symbols-regex "^tf_.*" \
	$(NULL)

endif

# Must put oldest versions first
ABI_LISTS = \
	1.0.abi \
	$(NULL)

EXTRA_DIST = $(ABI_LISTS)

CLEANFILES = \
	_gen/abi.funcnames \
	_gen/abi.funcs \
	_gen/abi.nm \
	_gen/abi.tpfuncnames \
	_gen/abi.txt \
	_gen/version-script.txt \
	_gen/libtelepathy-farstream-1-0.symbols \
	_gen/versioned-abi.tmp \
	_gen/versioned-abi.txt \
	$(NULL)

_gen/abi.txt: libtelepathy-farstream-internal.la Makefile.am
	$(MKDIR_P) _gen
	$(NM) .libs/libtelepathy-farstream-internal.a > _gen/abi.nm
	grep " [DT] " < _gen/abi.nm > _gen/abi.funcs
	cut -d" " -f3 < _gen/abi.funcs > _gen/abi.funcnames
	grep "^tf_" < _gen/abi.funcnames > _gen/abi.tpfuncnames
	$(AM_V_GEN)sort -u < _gen/abi.tpfuncnames > $@

MAKE_VERSION_SCRIPT_FLAGS =

if !OFFICIAL_RELEASE
# allow new ABI, and silently put it in a dummy version (which can be used to
# check whether binaries have been linked against unguaranteed ABI)
MAKE_VERSION_SCRIPT_FLAGS += \
	--unreleased-version=TELEPATHY_FARSTREAM_@VERSION@_UNRELEASED \
	$(NULL)
endif

_gen/version-script.txt: \
    $(ABI_LISTS) \
    _gen/abi.txt \
    Makefile.am \
    $(top_srcdir)/tools/make-version-script.py \
    $(NULL)
	$(AM_V_GEN)set -e; \
	$(PYTHON) $(top_srcdir)/tools/make-version-script.py \
		 --symbols=_gen/abi.txt $(MAKE_VERSION_SCRIPT_FLAGS) \
		 $(patsubst %,$(srcdir)/%,$(ABI_LISTS)) > $@
	$(AM_V_GEN)set -e; \
	$(PYTHON) $(top_srcdir)/tools/make-version-script.py \
		 --symbols=_gen/abi.txt $(MAKE_VERSION_SCRIPT_FLAGS) \
		 --dpkg "libtelepathy-farstream-1.so.0 #PACKAGE# #MINVER#" \
		 --dpkg-build-depends-package "libtelepathy-farstream-1-dev" \
		 $(patsubst %,$(srcdir)/%,$(ABI_LISTS)) \
		 > _gen/libtelepathy-farstream-1-0.symbols
	sed -n -e "s/^[	 ]*\\(tf_.*\\);/\\1/p" < $@ > _gen/versioned-abi.tmp
	sort -u < _gen/versioned-abi.tmp > _gen/versioned-abi.txt
	: # the versioned API should always match the ^tp API
	diff -c _gen/versioned-abi.txt _gen/abi.txt

# vim:ft=automake:

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
	--add-include-path=$(srcdir) \
	--add-include-path=$(top_builddir)/telepathy-glib \
	$(NULL)
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(srcdir) \
	--includedir=$(top_builddir)/telepathy-glib \
	$(NULL)

if HAVE_INTROSPECTION
introspection_sources = $(tfinclude_HEADERS) $(apisources)

TelepathyFarstream-1.gir: libtelepathy-farstream-1.la
TelepathyFarstream_1_gir_INCLUDES = \
	GObject-2.0 \
	Gst-1.0 \
	Farstream-0.2 \
	TelepathyGLib-1 \
	$(NULL)
TelepathyFarstream_1_gir_PACKAGES = \
	glib-2.0 \
	gobject-2.0 \
	gio-2.0 \
	telepathy-glib-1 \
	$(NULL)
TelepathyFarstream_1_gir_EXPORT_PACKAGES = \
	telepathy-farstream-1 \
	$(NULL)
TelepathyFarstream_1_gir_CFLAGS = $(AM_CPPFLAGS)
TelepathyFarstream_1_gir_LIBS = libtelepathy-farstream-1.la
TelepathyFarstream_1_gir_FILES = $(introspection_sources)
TelepathyFarstream_1_gir_SCANNERFLAGS = --identifier-prefix=tf_ --identifier-prefix=Tf
INTROSPECTION_GIRS += TelepathyFarstream-1.gir
INTROSPECTION_SCANNER_ENV = \
	PKG_CONFIG_PATH=$(builddir):$(top_builddir)/telepathy-glib:$${PKG_CONFIG_PATH}

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

SUBDIRS = . examples
