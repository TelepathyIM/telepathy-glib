#!/bin/sh
set -e

test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

(
    cd "$srcdir"
    intltoolize --force --copy --automake
    gtkdocize
    autoreconf -i -f
)

# Honor NOCONFIGURE for compatibility with gnome-autogen.sh
if test x"$NOCONFIGURE" = x; then
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
else
    run_configure=false
fi

if test $run_configure = true; then
    "$srcdir/configure" "$@"
fi
