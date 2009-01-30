dist_noinst_SCRIPTS = \
    test-gobject-foo.sh \
    test-glib-gtypes-generator.sh

EXTRA_DIST = \
    expected-gobject-foo.h \
    expected-gtypes.h \
    expected-gtypes-body.h \
    expected-ginterface-foo.h \
    glib-gtypes-generator.xml

TESTS = $(dist_noinst_SCRIPTS)

TESTS_ENVIRONMENT = \
    PYTHON=$(PYTHON) \
    srcdir=$(srcdir) \
    top_srcdir=$(top_srcdir) \
    sh

CLEANFILES = actual.h actual-body.h
