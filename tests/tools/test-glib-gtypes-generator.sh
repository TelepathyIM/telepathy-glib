#!/bin/sh

set -e

${PYTHON} ${top_srcdir}/tools/glib-gtypes-generator.py \
    ${top_srcdir}/tests/tools/glib-gtypes-generator.xml \
    actual The_Prefix

e=0
# We assume POSIX diff, until someone complains
diff -b -c ${srcdir}/expected-gtypes.h actual.h || e=$?
diff -b -c ${srcdir}/expected-gtypes-body.h actual-body.h || e=$?

exit $e
