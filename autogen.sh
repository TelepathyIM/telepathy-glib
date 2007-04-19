#!/bin/sh
set -e

if test -z "$MAKE"
then
	MAKE=make
fi

gtkdocize

( cd spec && TOP_SRCDIR=.. sh ../tools/update-spec-gen-am.sh spec-gen.am )
$MAKE -C telepathy-glib -f stable-interfaces.mk _gen/stable-interfaces.txt
( cd telepathy-glib && TOP_SRCDIR=.. sh ../tools/update-spec-gen-am.sh _gen/spec-gen.am _gen _gen/stable-interfaces.txt )

autoreconf -i

./configure "$@"
