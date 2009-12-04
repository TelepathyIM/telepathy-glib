#!/bin/bash

PACKAGE_NAME="TpLogger"
CC=${CC:-gcc}
CCOPTS="-DPACKAGE_NAME=\"${PACKAGE_NAME}\" --std=c99 -g -I/usr/include/libempathy -I../include -Wall -Werror" # -pedantic"
PKGS="telepathy-glib libempathy"
MODULES="tpl_observer.c tpl_headless_logger_init.c
	tpl_channel_data.c tpl_text_channel_data.c 
	tpl_contact.c
	tpl_utils.c
	logstore/tpl-log-store.c
	logstore/tpl-log-store-empathy.c
	tpl_log_entry_text.c
	test.c"
EXECUTABLE="test"


${CC} ${CCOPTS} $(pkg-config --libs --cflags ${PKGS}) ${MODULES} \
	-o ${EXECUTABLE}
RET=$?

exit $RET
