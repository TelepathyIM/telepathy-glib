#!/bin/sh

cd "/home/kalfa/moblin/src/telepathy-gabble/tests/twisted/tools"

export GABBLE_DEBUG=all LM_DEBUG=net GIBBER_DEBUG=all WOCKY_DEBUG=all
export GABBLE_TIMING=1
export GABBLE_PLUGIN_DIR="/home/kalfa/moblin/src/telepathy-gabble/plugins/.libs"
ulimit -c unlimited
exec >> gabble-testing.log 2>&1

if test -n "$GABBLE_TEST_VALGRIND"; then
        export G_DEBUG=${G_DEBUG:+"${G_DEBUG},"}gc-friendly
        export G_SLICE=always-malloc
        export DBUS_DISABLE_MEM_POOLS=1
        GABBLE_WRAPPER="valgrind --leak-check=full --num-callers=20"
        GABBLE_WRAPPER="$GABBLE_WRAPPER --show-reachable=yes"
        GABBLE_WRAPPER="$GABBLE_WRAPPER --gen-suppressions=all"
        GABBLE_WRAPPER="$GABBLE_WRAPPER --child-silent-after-fork=yes"
        GABBLE_WRAPPER="$GABBLE_WRAPPER --suppressions=/home/kalfa/moblin/src/telepathy-gabble/tests/suppressions/tp-glib.supp"
        GABBLE_WRAPPER="$GABBLE_WRAPPER --suppressions=/home/kalfa/moblin/src/telepathy-gabble/tests/suppressions/gabble.supp"
elif test -n "$GABBLE_TEST_REFDBG"; then
        if test -z "$REFDBG_OPTIONS" ; then
                export REFDBG_OPTIONS="btnum=10"
        fi
        if test -z "$GABBLE_WRAPPER" ; then
                GABBLE_WRAPPER="refdbg"
        fi
elif test -n "$GABBLE_TEST_STRACE"; then
        GABBLE_WRAPPER="strace -o strace.log"
fi

export G_DEBUG=fatal-warnings,fatal-criticals" ${G_DEBUG}"
exec /home/kalfa/moblin/src/telepathy-gabble/libtool --mode=execute $GABBLE_WRAPPER ../telepathy-gabble-debug
