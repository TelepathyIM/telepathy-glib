BUILT_SOURCES = \
    tf-signals-marshal.h \
    tf-signals-marshal.c

CLEANFILES = $(BUILT_SOURCES) tf-signals-marshal.list


tfincludedir = $(includedir)/telepathy-1.0/telepathy-farsight
tfinclude_HEADERS = channel.h stream.h

libtelepathy_farsight_la_SOURCES = \
	stream.c \
	stream.h \
	session.c \
	channel.c \
	channel.h \
	stream-priv.h \
	session-priv.h \
	channel-priv.h

nodist_libtelepathy_farsight_la_SOURCES = $(BUILT_SOURCES)

lib_LTLIBRARIES = libtelepathy-farsight.la

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = telepathy-farsight.pc

AM_CFLAGS = \
	-DG_LOG_DOMAIN=\"tp-fs\" \
	$(ERROR_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GST_CFLAGS) \
	$(FARSIGHT2_CFLAGS) \
	$(TELEPATHY_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir)

libtelepathy_farsight_la_LIBADD = \
	$(GLIB_LIBS) \
	$(DBUS_LIBS) \
	$(GST_LIBS) \
	$(FARSIGHT2_LIBS) \
	$(TELEPATHY_LIBS)

libtelepathy_farsight_la_LDFLAGS =  -no-undefined \
	-export-symbols-regex "^tf_.*" \
	-version-info "$(LT_CURRENT)":"$(LT_REVISION)":"$(LT_AGE)"


# rules to generate signal marshallers
tf-signals-marshal.list: $(libtelepathy_farsight_la_SOURCES) Makefile.am
	( cd $(srcdir) && \
	sed -n -e 's/.*_tf_marshal_\([[:upper:][:digit:]]*__[[:upper:][:digit:]_]*\).*/\1/p' \
	$(libtelepathy_farsight_la_SOURCES) ) \
	| sed -e 's/__/:/' -e 'y/_/,/' | sort -u > $@.tmp
	if cmp -s $@.tmp $@; then \
		rm $@.tmp; \
		touch $@; \
	else \
		mv $@.tmp $@; \
	fi

%-signals-marshal.h: %-signals-marshal.list Makefile.in
	glib-genmarshal --header --prefix=_$(subst -,_,$*)_marshal $< > $*-signals-marshal.h

%-signals-marshal.c: %-signals-marshal.list Makefile.in
	glib-genmarshal --body --prefix=_$(subst -,_,$*)_marshal $< > $*-signals-marshal.c
