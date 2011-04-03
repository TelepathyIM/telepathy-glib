SUBDIRS = codegen examples

PYDEFS=`pkg-config --variable=defsdir pygtk-2.0`
GSTPYDEFS=`pkg-config --variable=defsdir gst-python-0.10`

noinst_HEADERS = pygstminiobject.h common.h

AM_CPPFLAGS =					\
	-I.						\
	-I$(top_srcdir) \
	-DDATADIR=\""$(datadir)"\" \
	$(TELEPATHY_CFLAGS) \
	$(PYTHON_INCLUDES) \
	$(PYTPFARSTREAM_CFLAGS) \
	$(FARSIGHT2_CFLAGS) \
	$(DBUS_CFLAGS)

BUILT_SOURCES =						\
	pytpfarstream.c					 

pytpfarstreamdir = $(pyexecdir)
pytpfarstream_LTLIBRARIES = tpfarstream.la

tpfarstream_la_SOURCES =					\
	pytpfarstreammodule.c pygstminiobject.c

nodist_tpfarstream_la_SOURCES = pytpfarstream.c

tpfarstream_la_LIBADD =					\
	$(PYTPFARSTREAM_LIBS)				\
	$(top_builddir)/telepathy-farstream/libtelepathy-farstream.la

tpfarstream_la_LDFLAGS = -module -avoid-version

pytpfarstream.c: pytpfarstream.override pytpfarstream.defs
	$(PYTHON) $(top_srcdir)/python/codegen/codegen.py	\
		--prefix tf	\
		--register $(GSTPYDEFS)/gst-types.defs		\
		--override $(srcdir)/pytpfarstream.override	\
		$(srcdir)/pytpfarstream.defs > $@ 

EXTRA_DIST =			\
	pytpfarstream.override	\
	pytpfarstream.defs		\
	pytpfarstream-filter.defs  \
	rebuild-defs.sh

CLEANFILES = $(BUILT_SOURCES)
