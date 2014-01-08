ACLOCAL_AMFLAGS = -I m4


SUBDIRS= m4 \
          tools \
          telepathy-farstream \
          docs \
          $(NULL)

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --enable-introspection

EXTRA_DIST = \
    autogen.sh \
    gtk-doc.make

maintainer-upload-release: _maintainer-upload-release-local
_maintainer-upload-release-local: _maintainer-upload-release-check
	rsync -rvzPp --chmod=Dg+s,ug+rwX,o=rX docs/reference/telepathy-farstream/html/ \
	telepathy.freedesktop.org:/srv/telepathy.freedesktop.org/www/doc/telepathy-farstream-1/

BRANCH = misc
UPLOAD_BRANCH_TO = people.freedesktop.org:public_html/telepathy-farstream

upload-branch-docs: all
	rsync -rzvPp --chmod=a+rX docs/reference/telepathy-farstream/html/ \
		$(UPLOAD_BRANCH_TO)-$(BRANCH)/

include tools/telepathy.am
