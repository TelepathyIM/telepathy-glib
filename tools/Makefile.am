EXTRA_DIST = \
    glib-ginterface-gen.py \
    glib-signals-marshal-gen.py \
    identity.xsl \
    libglibcodegen.py

CLEANFILES = libglibcodegen.pyc libglibcodegen.pyo

all: $(EXTRA_DIST)

glib-ginterface-gen.py: libglibcodegen.py
	touch $@

glib-signals-marshal-gen.py: libglibcodegen.py
	touch $@
