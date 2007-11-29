ACLOCAL_AMFLAGS = -I m4

SUBDIRS = tools api src test doc data

dist-hook:
	chmod a+w ${distdir}/ChangeLog
	darcs changes >${distdir}/ChangeLog
