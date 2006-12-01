ACLOCAL_AMFLAGS = -I m4

SUBDIRS = common src test doc data generate

dist-hook:
	chmod a+w ${distdir}/ChangeLog
	darcs changes >${distdir}/ChangeLog
