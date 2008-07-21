ACLOCAL_AMFLAGS = -I m4

SUBDIRS = tools telepathy-farsight api src test doc data

confdir = $(sysconfdir)/xdg/stream-engine

conf_DATA = gstcodecs.conf

dist-hook:
	chmod a+w ${distdir}/ChangeLog
	darcs changes >${distdir}/ChangeLog
