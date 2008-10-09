ACLOCAL_AMFLAGS = -I m4

SUBDIRS = m4 tools spec telepathy-glib examples tests docs

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --disable-debug

EXTRA_DIST = \
    autogen.sh \
    gtk-doc.make

CLEANFILES = FIXME.out

check-local::
	egrep -A 5 '[F]IXME|[T]ODO|[X]XX' $(srcdir)/telepathy-glib/*.[ch] \
		> FIXME.out || true

include tools/lcov.am
include tools/telepathy.am
