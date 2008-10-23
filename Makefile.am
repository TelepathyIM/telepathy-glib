ACLOCAL_AMFLAGS = -I m4

SUBDIRS = tools telepathy-farsight api src test doc data

confdir = $(sysconfdir)/stream-engine
conf_DATA = gstcodecs.conf

EXTRA_DIST = $(conf_DATA)

dist-hook:
	chmod a+w ${distdir}/ChangeLog
	git log >${distdir}/ChangeLog

DISTCHECK_CONFIGURE_FLAGS=--enable-gtk-doc
