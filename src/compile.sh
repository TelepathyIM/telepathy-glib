#!/bin/bash

CC=${CC:-gcc}
CCOPTS="--std=c99 -g -I. -Wall -Werror" # -pedantic"
PKGS="telepathy-glib"
MODULES="tpl_observer.c tpl_headless_logger_init.c
	tpl_channel_data.c tpl_text_channel_data.c 
	tpl_utils.c
	test.c"
EXECUTABLE="test"


${CC} ${CCOPTS} $(pkg-config --libs --cflags ${PKGS}) ${MODULES} \
	-o ${EXECUTABLE}
RET=$?

exit $RET
