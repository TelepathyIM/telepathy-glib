EXTRA_DIST = \
    c-constants-generator.xsl \
    check-coding-style.mk \
    check-c-style.sh \
    check-misc.sh \
    check-whitespace.sh \
    c-interfaces-generator.xsl \
    genginterface.py \
    glib-client-marshaller-gen.py \
    glib-errors-enum-body.xsl \
    glib-errors-enum-header.xsl \
    glib-interfaces-generator.xsl \
    glib-interfaces-body-generator.xsl \
    glib-gtypes-generator.py \
    identity.xsl \
    libglibcodegen.py \
    ls-interfaces.xsl \
    make-all-async.xsl \
    spec-to-introspect.xsl \
    update-spec-gen-am.sh

CLEANFILES = genginterface.pyc genginterface.pyo

all: $(EXTRA_DIST)

genginterface.py: libglibcodegen.py
	touch $@
glib-gtypes-generator.py: libglibcodegen.py
	touch $@
