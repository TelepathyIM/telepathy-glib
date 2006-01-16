top_srcdir=$PWD
export PYTHONPATH=$PYTHONPATH:$top_srcdir/tools

cd $top_srcdir/test

echo Generating TpMediaSessionHandler files ...
python $top_srcdir/tools/gengobject.py $top_srcdir/xml/tp-media-session-handler.xml TpMediaSessionHandler
echo Generating TpMediaStreamHandler files ...
python $top_srcdir/tools/gengobject.py $top_srcdir/xml/tp-media-stream-handler.xml TpMediaStreamHandler

cd $top_srcdir/common

echo Generating error enums ...
python $top_srcdir/tools/generrors.py

cd $top_srcdir/src

echo Generating VoipEngine files ...
python $top_srcdir/tools/gengobject.py $top_srcdir/xml/voip-engine.xml VoipEngine

