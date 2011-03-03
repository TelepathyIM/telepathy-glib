## Process this file with automake to produce Makefile.in

abs_top_builddir = @abs_top_builddir@

# We require automake 1.6 at least.
AUTOMAKE_OPTIONS = 1.6

# This is a blank Makefile.am for using gtk-doc.
# Copy this to your project's API docs directory and modify the variables to
# suit your project. See the GTK+ Makefiles in gtk+/docs/reference for examples
# of using the various options.

# The name of the module, e.g. 'glib'.
DOC_MODULE=telepathy-logger

# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE=docs.xml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
# e.g. DOC_SOURCE_DIR=../../../gtk
DOC_SOURCE_DIR=../../../telepathy-logger

# Extra options to pass to gtkdoc-scangobj. Not normally needed.
SCANGOBJ_OPTIONS=

# Extra options to supply to gtkdoc-scan.
# e.g. SCAN_OPTIONS=--deprecated-guards="GTK_DISABLE_DEPRECATED"
# --rebuild-sections means we get a section (xml/*.xml) per header file: this
# should be removed if we want to maintain the sections file by hand, to split
# or combine sections.
SCAN_OPTIONS= \
    --deprecated-guards=TP_DISABLE_DEPRECATED \
    --rebuild-sections \
    --rebuild-types

# Extra options to supply to gtkdoc-mkdb.
# e.g. MKDB_OPTIONS=--sgml-mode --output-format=xml
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-mktmpl
# e.g. MKTMPL_OPTIONS=--only-section-tmpl
MKTMPL_OPTIONS=

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
FIXXREF_OPTIONS=

# Used for dependencies. The docs will be rebuilt if any of these change.
# e.g. HFILE_GLOB=$(top_srcdir)/gtk/*.h
# e.g. CFILE_GLOB=$(top_srcdir)/gtk/*.c
HFILE_GLOB=$(top_srcdir)/telepathy-logger/*.h
CFILE_GLOB=$(top_srcdir)/telepathy-logger/*.c

# Header files to ignore when scanning.
# e.g. IGNORE_HFILES=gtkdebug.h gtkintl.h
IGNORE_HFILES=\
    action-chain-internal.h \
    channel-factory-internal.h \
    channel-internal.h \
    conf-internal.h \
    contact-internal.h \
    dbus-service-internal.h \
    debug-internal.h \
    event-internal.h \
    log-manager-internal.h \
    log-store-factory-internal.h \
    log-store-internal.h \
    log-store-sqlite-internal.h \
    log-store-xml-internal.h \
    observer-internal.h \
    text-channel-internal.h \
    text-event-internal.h \
    util-internal.h

# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
HTML_IMAGES=

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
content_files=

# SGML files where gtk-doc abbrevations (#GtkWidget) are expanded
# These files must be listed here *and* in content_files
# e.g. expand_content_files=running.sgml
expand_content_files=

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
# Only needed if you are using gtkdoc-scangobj to dynamically query widget
# signals and properties.
# e.g. INCLUDES=-I$(top_srcdir) -I$(top_builddir) $(GTK_DEBUG_FLAGS)
# e.g. GTKDOC_LIBS=$(top_builddir)/gtk/$(gtktargetlib)
INCLUDES= \
	-I$(top_srcdir) -I$(top_builddir) \
	$(TPL_CFLAGS)
GTKDOC_LIBS= \
	$(TPL_LIBS) \
	$(top_builddir)/telepathy-logger/libtelepathy-logger.la

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

if ENABLE_GTK_DOC
# Enable this enforce the documenting of symbols in `make check`
# check-local: check-undocumented

check-undocumented:
	@if grep '^0 symbols incomplete' \
		$(srcdir)/telepathy-logger-undocumented.txt; then\
		:; else \
		cat $(srcdir)/telepathy-logger-undocumented.txt; exit 1; fi
	@if grep '^0 not documented' \
		$(srcdir)/telepathy-logger-undocumented.txt; then\
		:; else \
		cat $(srcdir)/telepathy-logger-undocumented.txt; exit 1; fi
	@if grep . $(srcdir)/telepathy-logger-unused.txt; then\
		echo "^^^ Unused symbols" >&2; exit 1; fi
	@if test -e $(srcdir)/telepathy-logger-undeclared.txt &&\
		grep . $(srcdir)/telepathy-logger-undeclared.txt; then\
		echo "^^^ Undeclared symbols" >&2; exit 1; fi
endif
