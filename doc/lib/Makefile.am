## Process this file with automake to produce Makefile.in

abs_top_builddir = @abs_top_builddir@

# We require automake 1.6 at least.
AUTOMAKE_OPTIONS = 1.6

# The name of the module, e.g. 'glib'.
DOC_MODULE=telepathy-farstream

# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
# e.g. DOC_SOURCE_DIR=../../../gtk
DOC_SOURCE_DIR=$(abs_top_srcdir)/telepathy-farstream

# Extra options to pass to gtkdoc-scangobj. Not normally needed.
SCANGOBJ_OPTIONS=

# Extra options to supply to gtkdoc-scan.
# e.g. SCAN_OPTIONS=--deprecated-guards="GTK_DISABLE_DEPRECATED"
SCAN_OPTIONS=

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
HFILE_GLOB=$(top_srcdir)/telepathy-farstream/channel.h $(top_srcdir)/telepathy-farstream/content.h
CFILE_GLOB=$(top_srcdir)/telepathy-farstream/channel.c $(top_srcdir)/telepathy-farstream/content.c

# Header files to ignore when scanning.
# e.g. IGNORE_HFILES=gtkdebug.h gtkintl.h
IGNORE_HFILES=channel-priv.h content-priv.h session-priv.h stream-priv.h \
  stream.h call-channel.h call-content.h call-stream.h \
  media-signalling-channel.h media-signalling-content.h utils.h

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
AM_CPPFLAGS = \
	$(GLIB_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(X11_CFLAGS) \
	$(GST_CFLAGS) \
	$(GST_INTERFACES_CFLAGS) \
	$(FARSTREAM_CFLAGS) \
	$(TELEPATHY_CFLAGS) \
	-I$(top_srcdir) -I$(top_builddir)
GTKDOC_LIBS= \
	$(GLIB_LIBS) \
	$(DBUS_LIBS) \
	$(X11_LIBS) \
	$(GST_LIBS) \
	$(GST_INTERFACES_LIBS) \
	$(TELEPATHY_LIBS) \
	$(FARSTREAM_LIBS) \
	$(top_builddir)/telepathy-farstream/libtelepathy-farstream.la

# This includes the standard gtk-doc make rules, copied by gtkdocize.
include $(top_srcdir)/gtk-doc.make

if ENABLE_GTK_DOC
check-local:
	@if grep '^0 symbols incomplete' \
		$(top_builddir)/doc/lib/telepathy-farstream-undocumented.txt; then\
		:; else \
		cat $(top_builddir)/doc/lib/telepathy-farstream-undocumented.txt; exit 1; fi
	@if grep '^0 not documented' \
		$(top_builddir)/doc/lib/telepathy-farstream-undocumented.txt; then\
		:; else \
		cat $(top_builddir)/doc/lib/telepathy-farstream-undocumented.txt; exit 1; fi
	@if grep . $(top_builddir)/doc/lib/telepathy-farstream-unused.txt; then\
		echo "^^^ Unused symbols" >&2; exit 1; fi
	@if test -e $(top_builddir)/doc/lib/telepathy-farstream-undeclared.txt &&\
		grep . $(top_builddir)/doc/lib/telepathy-farstream-undeclared.txt; then\
		echo "^^^ Undeclared symbols" >&2; exit 1; fi
endif
