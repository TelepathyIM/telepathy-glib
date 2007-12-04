EXTRA_DIST = \
    c-constants-generator.xsl \
    check-coding-style.mk \
    check-c-style.sh \
    check-misc.sh \
    check-whitespace.sh \
    c-interfaces-generator.xsl \
    glib-client-gen.py \
    glib-client-marshaller-gen.py \
    glib-errors-enum-body.xsl \
    glib-errors-enum-header.xsl \
    glib-interfaces-generator.xsl \
    glib-interfaces-body-generator.xsl \
    glib-ginterface-gen.py \
    glib-gtypes-generator.py \
    glib-signals-marshal-gen.py \
    gobject-foo.py \
    identity.xsl \
    libglibcodegen.py \
    ls-interfaces.xsl \
    make-all-async.xsl \
    spec-to-introspect.xsl \
    with-session-bus.sh

CLEANFILES = libglibcodegen.pyc libglibcodegen.pyo

all: $(EXTRA_DIST)

glib-client-marshaller-gen.py: libglibcodegen.py
	touch $@
glib-ginterface-gen.py: libglibcodegen.py
	touch $@
glib-gtypes-generator.py: libglibcodegen.py
	touch $@
glib-signals-marshal-gen.py: libglibcodegen.py
	touch $@

TELEPATHY_SPEC_SRCDIR = $(top_srcdir)/../telepathy-spec
maintainer-update-from-telepathy-spec:
	set -e && cd $(srcdir) && \
	for x in $(EXTRA_DIST); do \
		if test -f $(TELEPATHY_SPEC_SRCDIR)/tools/$$x; then \
			cp $(TELEPATHY_SPEC_SRCDIR)/tools/$$x $$x; \
		fi; \
	done
