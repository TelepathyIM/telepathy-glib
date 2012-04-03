EXTRA_DIST = \
    c-constants-gen.py \
    check-coding-style.mk \
    check-c-style.sh \
    check-misc.sh \
    check-whitespace.sh \
    doc-generator.py \
    doc-generator.xsl \
    glib-client-gen.py \
    glib-client-marshaller-gen.py \
    glib-ginterface-gen.py \
    glib-gtypes-generator.py \
    glib-interfaces-gen.py \
    glib-signals-marshal-gen.py \
    identity.xsl \
    lcov.am \
    libglibcodegen.py \
    libtpcodegen.py \
    log-strip.py \
    make-release-mail.py \
    specparser.py \
    telepathy.am \
    test-wrapper.sh \
    valgrind.mk \
    with-session-bus.sh \
    xincludator.py

CLEANFILES = *.pyc *.pyo

#
# FIXME: The following stuff break makedist
#
#all: $(EXTRA_DIST)

#libglibcodegen.py: libtpcodegen.py
#	test -e $<
#	$(AM_V_GEN)touch $@

#glib-ginterface-gen.py glib-gtypes-generator.py glib-interfaces-gen.py \
#glib-signals-marshal-gen.py c-constants-gen.py: %: libglibcodegen.py
#	test -e $<
#	$(AM_V_GEN)touch $@

#TELEPATHY_GLIB_SRCDIR = $(top_srcdir)/../telepathy-glib
#maintainer-update-from-telepathy-glib:
#	set -e && cd $(srcdir) && \
#	for x in $(EXTRA_DIST); do \
#		if test -f $(TELEPATHY_GLIB_SRCDIR)/tools/$$x; then \
#			cp $(TELEPATHY_GLIB_SRCDIR)/tools/$$x $$x; \
#		fi; \
#	done
