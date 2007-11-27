dist_noinst_SCRIPTS = \
    test-gobject-foo.sh

EXTRA_DIST = \
    expected-gobject-foo.h \
    expected-ginterface-foo.h

TESTS = $(dist_noinst_SCRIPTS)

TESTS_ENVIRONMENT = \
    PYTHON=$(PYTHON) \
    srcdir=$(srcdir) \
    top_srcdir=$(top_srcdir) \
    sh
