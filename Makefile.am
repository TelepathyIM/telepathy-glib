ACLOCAL_AMFLAGS = -I m4

SUBDIRS = m4 tools spec telepathy-glib tests examples docs

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --disable-debug

EXTRA_DIST = \
    autogen.sh \
    gtk-doc.make

CLEANFILES = FIXME.out

check-local::
	egrep -A 5 '[F]IXME|[T]ODO|[X]XX' $(srcdir)/telepathy-glib/*.[ch] \
		> FIXME.out || true

dist-hook:
	chmod u+w ${distdir}/ChangeLog
	if test -d _darcs; then darcs changes >${distdir}/ChangeLog; fi

maintainer-upload-docs:
	rsync -rtvzPp --chmod=Dg+s,ug+rwX,o=rX docs/reference/html/ \
		telepathy.freedesktop.org:/srv/telepathy.freedesktop.org/www/doc/telepathy-glib
