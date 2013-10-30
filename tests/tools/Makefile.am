dist_noinst_SCRIPTS = \
    test-gobject-foo.sh \
    test-glib-gtypes-generator.sh

EXTRA_DIST = \
    expected-gobject-foo.h \
    expected-gtypes.h \
    expected-gtypes-body.h \
    expected-gtypes-gtk-doc.h \
    expected-ginterface-foo.h \
    glib-gtypes-generator.xml

TESTS = $(dist_noinst_SCRIPTS)

AM_TESTS_ENVIRONMENT = \
    PYTHON=$(PYTHON) \
    srcdir=$(srcdir) \
    top_srcdir=$(top_srcdir) \
    $(NULL)

CLEANFILES = \
	actual.h \
	actual-body.h \
	actual-gtk-doc.h
