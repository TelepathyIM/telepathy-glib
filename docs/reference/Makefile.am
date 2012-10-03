## Process this file with automake to produce Makefile.in

# We require automake 1.6 at least.
AUTOMAKE_OPTIONS = 1.6

# This is a blank Makefile.am for using gtk-doc.
# Copy this to your project's API docs directory and modify the variables to
# suit your project. See the GTK+ Makefiles in gtk+/docs/reference for examples
# of using the various options.

# The name of the module, e.g. 'glib'.
DOC_MODULE=telepathy-glib

# Uncomment for versioned docs and specify the version of the module, e.g. '2'.
#DOC_MODULE_VERSION=2


# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# Directories containing the source code, relative to $(srcdir).
# gtk-doc will search all .c and .h files beneath these paths
# for inline comments documenting functions and macros.
# e.g. DOC_SOURCE_DIR=../../../gtk ../../../gdk
DOC_SOURCE_DIR=$(abs_top_srcdir)/telepathy-glib $(abs_top_builddir)/telepathy-glib

# Extra options to pass to gtkdoc-scangobj. Not normally needed.
SCANGOBJ_OPTIONS=

# Extra options to supply to gtkdoc-scan.
# e.g. SCAN_OPTIONS=--deprecated-guards="GTK_DISABLE_DEPRECATED"
SCAN_OPTIONS=\
    --deprecated-guards=TP_DISABLE_DEPRECATED \
    --ignore-decorators='_TP_GNUC_DEPRECATED|_TP_GNUC_DEPRECATED_FOR\s*\([^\)]+\)' \
    --rebuild-types

# Extra options to supply to gtkdoc-mkdb.
# e.g. MKDB_OPTIONS=--xml-mode --output-format=xml
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-mktmpl
# e.g. MKTMPL_OPTIONS=--only-section-tmpl
MKTMPL_OPTIONS=

# Extra options to supply to gtkdoc-mkhtml
MKHTML_OPTIONS=

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
FIXXREF_OPTIONS=

# Used for dependencies. The docs will be rebuilt if any of these change.
# e.g. HFILE_GLOB=$(top_srcdir)/gtk/*.h
# e.g. CFILE_GLOB=$(top_srcdir)/gtk/*.c
HFILE_GLOB=$(top_srcdir)/telepathy-glib/*.h $(top_builddir)/telepathy-glib/_gen/*.h
CFILE_GLOB=$(top_srcdir)/telepathy-glib/*.c $(top_builddir)/telepathy-glib/_gen/*.c

# Extra header to include when scanning, which are not under DOC_SOURCE_DIR
# e.g. EXTRA_HFILES=$(top_srcdir}/contrib/extra.h
EXTRA_HFILES=

# Header files or dirs to ignore when scanning. Use base file/dir names
# e.g. IGNORE_HFILES=gtkdebug.h gtkintl.h private_code
IGNORE_HFILES=\
    proxy-introspectable.h \
    $(NULL)

# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
HTML_IMAGES=

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
content_files=tp-svc.xml version.xml

# SGML files where gtk-doc abbrevations (#GtkWidget) are expanded
# These files must be listed here *and* in content_files
# e.g. expand_content_files=running.sgml
expand_content_files=

# CFLAGS and LDFLAGS for compiling gtkdoc-scangobj with your library.
# Only needed if you are using gtkdoc-scangobj to dynamically query widget
# signals and properties.
# e.g. GTKDOC_CFLAGS=-I$(top_srcdir) -I$(top_builddir) $(GTK_DEBUG_FLAGS)
# e.g. GTKDOC_LIBS=$(top_builddir)/gtk/$(gtktargetlib)
GTKDOC_CFLAGS=-I$(top_srcdir) -I$(top_builddir) @DBUS_CFLAGS@ @GLIB_CFLAGS@
GTKDOC_LIBS=$(top_builddir)/telepathy-glib/libtelepathy-glib.la \
            @DBUS_LIBS@ @GLIB_LIBS@

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in
EXTRA_DIST += version.xml.in

# Files not to distribute
# for --rebuild-types in $(SCAN_OPTIONS), e.g. $(DOC_MODULE).types
# for --rebuild-sections in $(SCAN_OPTIONS) e.g. $(DOC_MODULE)-sections.txt
#DISTCLEANFILES +=

# Work around gtk-doc using an outdated -sections.txt in out-of-tree builds:
# redo the copy every time the files in the srcdir change
setup-build.stamp: $(srcdir)/$(DOC_MODULE)-overrides.txt
setup-build.stamp: $(srcdir)/$(DOC_MODULE)-sections.txt
setup-build.stamp: $(srcdir)/$(DOC_MAIN_SGML_FILE)
setup-build.stamp: $(srcdir)/tp-svc.xml

if ENABLE_GTK_DOC
check-local:
	@err=0; \
	if grep '^0 symbols incomplete' \
		telepathy-glib-undocumented.txt; then\
		:; \
	else \
		cat telepathy-glib-undocumented.txt; \
		err=1; \
	fi; \
	if grep '^0 not documented' \
		telepathy-glib-undocumented.txt; then\
		:; \
	else \
		cat telepathy-glib-undocumented.txt; \
		err=1; \
	fi; \
	if grep . telepathy-glib-unused.txt; then\
		echo "^^^ Unused symbols" >&2; \
		err=1; \
	fi; \
	if test -e telepathy-glib-undeclared.txt &&\
		grep . telepathy-glib-undeclared.txt; then\
		echo "^^^ Undeclared symbols" >&2; \
		err=1; \
	fi; \
	sed -n -e 's,<FILE>\(.*\)</FILE>,\1,p' telepathy-glib-sections.txt |\
	while read file; do \
		if grep -F "xml/$$file.xml" telepathy-glib-docs.sgml \
			>/dev/null;\
		then \
		:; \
		else \
			echo "$$file missing from telepathy-glib-docs.sgml"; \
			err=1; \
		fi; \
	done
endif
# ... but don't fail on incomplete documentation, this is a stable branch
