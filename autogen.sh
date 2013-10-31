#!/bin/sh
set -e

intltoolize --force --copy --automake || exit 1
gtkdocize || exit 1
autoreconf -i -f

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
