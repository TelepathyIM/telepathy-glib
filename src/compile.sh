#!/bin/bash

PACKAGE_NAME="TpLogger"
CC=${CC:-gcc}
CCOPTS="-D__USE_POSIX -DPACKAGE_NAME=\"${PACKAGE_NAME}\" --std=c99 -g -I../include -Wall -Werror" # -pedantic"
PKGS="telepathy-glib libxml-2.0"
MODULES="tpl_observer.c tpl_headless_logger_init.c
	tpl_channel_data.c tpl_text_channel_data.c 
	tpl_contact.c
	tpl_utils.c
	tpl-time.c
	tpl-log-manager.c
	tpl-log-store.c
	tpl-log-store-empathy.c
	tpl_log_entry_text.c
	test.c"
EXECUTABLE="telepathy-logger"


${CC} ${CCOPTS} $(pkg-config --libs --cflags ${PKGS}) ${MODULES} \
	-o ${EXECUTABLE}
RET=$?

exit $RET
