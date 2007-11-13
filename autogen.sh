#!/bin/sh
set -e

if test -z "$MAKE"
then
	MAKE=make
fi

gtkdocize

( cd spec && TOP_SRCDIR=.. sh ../tools/update-spec-gen-am.sh spec-gen.am )

autoreconf -i

run_configure=true
for arg in $*; do
    case $arg in
        --no-configure)
            run_configure=false
            ;;
        *)
            ;;
    esac
done

if test $run_configure = true; then
    ./configure "$@"
fi
