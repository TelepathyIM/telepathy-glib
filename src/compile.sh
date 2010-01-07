#!/bin/bash

PACKAGE_NAME="TpLogger"
CC=${CC:-gcc}
CCOPTS="-D_POSIX_SOURCE -DPACKAGE_NAME=\"${PACKAGE_NAME}\" --std=c99 -g -I../include -Wall -Werror" # -pedantic"
PKGS="telepathy-glib libxml-2.0"
MODULES="observer.c headless-logger-init.c
	channel.c channel-text.c 
	contact.c
	utils.c
	time.c
	log-manager.c
	log-store.c
	log-store-empathy.c
	log-entry-text.c
	test.c"
EXECUTABLE="telepathy-logger"


${CC} ${CCOPTS} $(pkg-config --libs --cflags ${PKGS}) ${MODULES} \
	-o ${EXECUTABLE}
RET=$?

exit $RET
