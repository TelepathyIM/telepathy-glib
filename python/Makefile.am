SUBDIRS = codegen

PYDEFS=`pkg-config --variable=defsdir pygtk-2.0`
GSTPYDEFS=`pkg-config --variable=defsdir gst-python-0.10`

noinst_HEADERS = pygstminiobject.h common.h

AM_CPPFLAGS =					\
	-I.						\
	-I$(top_srcdir) \
	-DDATADIR=\""$(datadir)"\" \
	$(TELEPATHY_CFLAGS) \
	$(PYTHON_INCLUDES) \
	$(PYTPFARSIGHT_CFLAGS) \
	$(FARSIGHT2_CFLAGS)

BUILT_SOURCES =						\
	pytpfarsight.c					 

pytpfarsightdir = $(pyexecdir)
pytpfarsight_LTLIBRARIES = tpfarsight.la

tpfarsight_la_SOURCES =					\
	pytpfarsightmodule.c pygstminiobject.c

nodist_tpfarsight_la_SOURCES = pytpfarsight.c

tpfarsight_la_LIBADD =					\
	$(PYTPFARSIGHT_LIBS)				\
	$(top_builddir)/telepathy-farsight/libtelepathy-farsight.la

tpfarsight_la_LDFLAGS =					\
	`python-config --libs`				\
	-module -avoid-version

pytpfarsight.c: pytpfarsight.override pytpfarsight.defs
	$(PYTHON) $(top_srcdir)/python/codegen/codegen.py	\
		--prefix tf	\
		--register $(GSTPYDEFS)/gst-types.defs		\
		--override $(srcdir)/pytpfarsight.override	\
		$(srcdir)/pytpfarsight.defs > $@ 

EXTRA_DIST =			\
	pytpfarsight.override	\
	pytpfarsight.defs		\
	pytpfarsight-filter.defs  \
	rebuild-defs.sh

CLEANFILES = $(BUILT_SOURCES)
