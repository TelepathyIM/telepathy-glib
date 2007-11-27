#!/bin/sh

set -e

${PYTHON} ${top_srcdir}/tools/gobject-foo.py Xyz_Badger Mushroom_Snake \
    > gobject-foo.h
${PYTHON} ${top_srcdir}/tools/gobject-foo.py --interface \
    Xyz_Badger Mushroom_Snake > ginterface-foo.h

e=0
# We assume POSIX diff, until someone complains
diff -b -c ${srcdir}/expected-gobject-foo.h gobject-foo.h || e=$?
diff -b -c ${srcdir}/expected-ginterface-foo.h ginterface-foo.h || e=$?

rm -f gobject-foo.h
rm -f ginterface-foo.h

exit $e
