#! /bin/bash

SCRIPTNAME=$0
WRAPPED_SCRIPT=$1
shift

function die() 
{
    if ! test -z "$DBUS_SESSION_BUS_PID" ; then
        echo "killing message bus "$DBUS_SESSION_BUS_PID >&2
        kill -9 $DBUS_SESSION_BUS_PID
    fi
    echo $SCRIPTNAME: $* >&2
    exit 1
}


## convenient to be able to ctrl+C without leaking the message bus process
trap 'die "Received SIGINT"' SIGINT

CONFIG_FILE=./run-with-tmp-session-bus.conf
SERVICE_DIR="$PWD/services"
EXEC_DIR=`echo $PWD/../ | sed -e 's/\//\\\\\\//g'`
ESCAPED_SERVICE_DIR=`echo $SERVICE_DIR | sed -e 's/\//\\\\\\//g'`
echo "escaped service dir is: $ESCAPED_SERVICE_DIR" >&2

## create a configuration file based on the standard session.conf
cat session.conf |  \
    sed -e 's/<servicedir>.*$/<servicedir>'$ESCAPED_SERVICE_DIR'<\/servicedir>/g' |  \
    sed -e 's/<include.*$//g'                \
  > $CONFIG_FILE

rm -rf services
mkdir -p services
for i in services.in/*; do
    echo mangling $i
    cat $i | sed -e "s/@EXEC_DIR@/$EXEC_DIR/g" > ${i/services.in\//services\//}
done

echo "Created configuration file $CONFIG_FILE" >&2

export PATH=$PWD:$PATH

unset DBUS_SESSION_BUS_ADDRESS
unset DBUS_SESSION_BUS_PID

echo "Running dbus-launch --sh-syntax --config-file=$CONFIG_FILE" >&2

eval `dbus-launch --sh-syntax --config-file=$CONFIG_FILE`

if test -z "$DBUS_SESSION_BUS_PID" ; then
    die "Failed to launch message bus for introspection generation to run"
fi

echo "Started bus pid $DBUS_SESSION_BUS_PID at $DBUS_SESSION_BUS_ADDRESS" >&2

# Execute wrapped script
echo "Running $WRAPPED_SCRIPT $@" >&2
$WRAPPED_SCRIPT "$@" || die "script \"$WRAPPED_SCRIPT\" failed"

kill -TERM $DBUS_SESSION_BUS_PID || die "Message bus vanished! should not have happened" && echo "Killed daemon $DBUS_SESSION_BUS_PID" >&2

sleep 2

## be sure it really died 
kill -9 $DBUS_SESSION_BUS_PID > /dev/null 2>&1 || true

exit 0
