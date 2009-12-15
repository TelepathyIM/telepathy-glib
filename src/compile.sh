#!/bin/bash

PACKAGE_NAME="TpLogger"
CC=${CC:-gcc}
CCOPTS="-D_POSIX_SOURCE -DPACKAGE_NAME=\"${PACKAGE_NAME}\" --std=c99 -g -I../include -Wall -Werror" # -pedantic"
PKGS="telepathy-glib libxml-2.0"
MODULES="tpl-observer.c tpl-headless-logger-init.c
	tpl-channel.c tpl-text-channel-context.c 
	tpl-contact.c
	tpl-utils.c
	tpl-time.c
	tpl-log-manager.c
	tpl-log-store.c
	tpl-log-store-empathy.c
	tpl-log-entry-text.c
	test.c"
EXECUTABLE="telepathy-logger"


${CC} ${CCOPTS} $(pkg-config --libs --cflags ${PKGS}) ${MODULES} \
	-o ${EXECUTABLE}
RET=$?

exit $RET
