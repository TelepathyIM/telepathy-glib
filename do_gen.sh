top_srcdir=$PWD
export PYTHONPATH=$PYTHONPATH:$top_srcdir/tools

cd src

echo Generating TpMediaSessionHandler files ...
python $top_srcdir/tools/gengobject.py $top_srcdir/tp-media-session-handler.xml TpMediaSessionHandler
echo Generating TpMediaStreamHandler files ...
python $top_srcdir/tools/gengobject.py $top_srcdir/tp-media-session-handler.xml TpMediaSessionHandler
echo Generating error enums ...
python $top_srcdir/tools/generrors.py

