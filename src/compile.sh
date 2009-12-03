#!/bin/bash

PACKAGE_NAME="TpHeadlessLogger"
CC=${CC:-gcc}
CCOPTS="-DPACKAGE_NAME=${PACKAGE_NAME} --std=c99 -g -I. -I/usr/include/libempaty -Wall -Werror" # -pedantic"
PKGS="telepathy-glib libempathy"
MODULES="tpl_observer.c tpl_headless_logger_init.c
	tpl_channel_data.c tpl_text_channel_data.c 
	tpl_contact.c
	tpl_utils.c
	test.c"
EXECUTABLE="test"


${CC} ${CCOPTS} $(pkg-config --libs --cflags ${PKGS}) ${MODULES} \
	-o ${EXECUTABLE}
RET=$?

exit $RET
