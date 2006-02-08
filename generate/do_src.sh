#!/bin/sh

if [ `basename $PWD` == "generate" ]; then
  TP=${TELEPATHY_PYTHON:=$PWD/../../telepathy-python}
else
  TP=${TELEPATHY_PYTHON:=$PWD/../telepathy-python}
fi

export PYTHONPATH=$TP:$PYTHONPATH

test -d generate && cd generate
cd test

echo Generating TpMediaSessionHandler files ...
python $TP/tools/gengobject.py ../xml-modified/tp-media-session-handler.xml TpMediaSessionHandler
echo Generating TpMediaStreamHandler files ...
python $TP/tools/gengobject.py ../xml-modified/tp-media-stream-handler.xml TpMediaStreamHandler
echo Generating TestStreamedMediaChannel files ...
python $TP/tools/gengobject.py ../xml-modified/test-streamed-media-channel.xml TestStreamedMediaChannel

cd ../src

echo Generating VoipEngine files ...
python $TP/tools/gengobject.py ../xml-modified/tp-voip-engine.xml TpVoipEngine

