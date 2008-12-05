ACLOCAL_AMFLAGS = -I m4


if WANT_PYTHON
  PYTHON_SUBDIR = python
endif

SUBDIRS = m4 tools telepathy-farsight doc $(PYTHON_SUBDIR)

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

EXTRA_DIST = \
    autogen.sh \
    gtk-doc.make

maintainer-upload-release: _maintainer-upload-release-local
_maintainer-upload-release-local: _maintainer-upload-release-check
	rsync -rtvzPp --chmod=Dg+s,ug+rwX,o=rX doc/lib/html/ \
	telepathy.freedesktop.org:/srv/telepathy.freedesktop.org/www/doc/telepathy-farsight/

BRANCH = misc
UPLOAD_BRANCH_TO = people.freedesktop.org:public_html/telepathy-farsight

upload-branch-docs: all
	rsync -rtzvPp --chmod=a+rX doc/lib/html/ \
		$(UPLOAD_BRANCH_TO)-$(BRANCH)/

include tools/telepathy.am
