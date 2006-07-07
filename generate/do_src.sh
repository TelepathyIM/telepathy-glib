#!/bin/sh

if [ `basename $PWD` == "generate" ]; then
  TP=${TELEPATHY_PYTHON:=$PWD/../../telepathy-python}
else
  TP=${TELEPATHY_PYTHON:=$PWD/../telepathy-python}
fi

export PYTHONPATH=$TP:$PYTHONPATH

test -d generate && cd generate
cd src

echo Generating MediaEngine files ...
python $TP/tools/gengobject.py ../xml-modified/tp-media-engine.xml TpMediaEngine

