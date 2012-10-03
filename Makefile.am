ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
    m4 \
    tools \
    spec \
    telepathy-glib \
    $(NULL)

if HAVE_INTROSPECTION
if HAVE_VALA
SUBDIRS += vala
endif
endif

SUBDIRS += \
    examples \
    tests \
    docs \
    $(NULL)

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
	rsync -rvzPp --chmod=Dg+s,ug+rwX,o=rX $(builddir)/docs/reference/html/ \
	telepathy.freedesktop.org:/srv/telepathy.freedesktop.org/www/doc/telepathy-glib-0.20.x/

BRANCH = $(shell sh tools/git-which-branch.sh misc | tr -d '\n' | tr -C "[:alnum:]" _)
UPLOAD_BRANCH_TO = people.freedesktop.org:public_html/telepathy-glib

upload-branch-docs: all
	rsync -rtzvPp --chmod=a+rX docs/reference/html/ \
		$(UPLOAD_BRANCH_TO)-$(BRANCH)/

include tools/lcov.am

CHANGELOG_RANGE = telepathy-glib-0.14.0..
CHECK_FOR_UNRELEASED = \
    $(srcdir)/NEWS \
    $(filter-out %/defs.h,$(wildcard $(srcdir)/telepathy-glib/*.[ch])) \
    $(NULL)

include tools/telepathy.am

# Don't just "make distclean", because that would result in forgetting
# the options we used when we ran configure.
#
# However, we do want to distclean in docs (to get rid of xml, html etc.)
# and in telepathy-glib (to get rid of any stray files in _gen which might
# not exist on the new branch).
clean-for-new-branch:
	$(MAKE) clean
	$(MAKE) -C docs distclean
	$(MAKE) -C telepathy-glib distclean
	$(SHELL) ./config.status --recheck
	$(SHELL) ./config.status
