#!/bin/sh
set -e

test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

if test -n "$AUTOMAKE"; then
    : # don't override an explicit user request
elif automake-1.11 --version >/dev/null 2>/dev/null && \
     aclocal-1.11 --version >/dev/null 2>/dev/null; then
    # If we have automake-1.11, use it. This is the oldest version (=> least
    # likely to introduce undeclared dependencies) that will give us
    # --enable-silent-rules support.
    AUTOMAKE=automake-1.11
    export AUTOMAKE
    ACLOCAL=aclocal-1.11
    export ACLOCAL
fi

(
    cd "$srcdir"

    # check if gtk-doc is explicitly disabled
    for ag_option in $@
    do
      case $ag_option in
        -disable-gtk-doc | --disable-gtk-doc)
        enable_gtk_doc=no
      ;;
      esac
    done

    if test x$enable_gtk_doc = xno; then
        if test -f gtk-doc.make; then :; else
           echo "EXTRA_DIST = missing-gtk-doc" > gtk-doc.make
        fi
        echo "WARNING: You have disabled gtk-doc."
        echo "         As a result, you will not be able to generate the API"
        echo "         documentation and 'make dist' will not work."
        echo
    else
        gtkdocize || exit $?
    fi

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
