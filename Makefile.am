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

check-valgrind:
	$(MAKE) -C tests check-valgrind 2>&1 | tee valgrind.log

maintainer-upload-release: _maintainer-upload-release-local
_maintainer-upload-release-local: _maintainer-upload-release-check
	rsync -rvzPp --chmod=Dg+s,ug+rwX,o=rX docs/reference/html/ \
	telepathy.freedesktop.org:/srv/telepathy.freedesktop.org/www/doc/telepathy-glib/

BRANCH = $(shell sh tools/git-which-branch.sh misc | tr -d '\n' | tr -C "[:alnum:]" _)
UPLOAD_BRANCH_TO = people.freedesktop.org:public_html/telepathy-glib

upload-branch-docs: all
	rsync -rtzvPp --chmod=a+rX docs/reference/html/ \
		$(UPLOAD_BRANCH_TO)-$(BRANCH)/

include tools/lcov.am

CHANGELOG_RANGE = telepathy-glib-0.6.0..
include tools/telepathy.am
