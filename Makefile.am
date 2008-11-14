ACLOCAL_AMFLAGS = -I m4

SUBDIRS = telepathy-farsight doc

dist-hook:
	chmod a+w ${distdir}/ChangeLog
	git log >${distdir}/ChangeLog

DISTCHECK_CONFIGURE_FLAGS=--enable-gtk-doc
