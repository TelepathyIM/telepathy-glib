#!/bin/sh

HEADERS=" \
    channel.h \
    stream.h"

srcdir=../telepathy-farsight/

output=pytpfarsight.defs
filter=pytpfarsight-filter.defs

cat ${filter} > ${output}

for h in $HEADERS; do
    python codegen/h2def.py --defsfilter=${filter} ${srcdir}/$h >> $output
done
