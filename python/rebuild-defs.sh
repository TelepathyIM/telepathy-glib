#!/bin/sh

HEADERS=" \
    channel.h \
    content.h \
    stream.h"

srcdir=../telepathy-farstream/

output=pytpfarstream.defs
filter=pytpfarstream-filter.defs

cat ${filter} > ${output}

for h in $HEADERS; do
    python codegen/h2def.py --defsfilter=${filter} ${srcdir}/$h >> $output
done
