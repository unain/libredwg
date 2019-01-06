#!/bin/sh
# test dwg2dxf via teigha roundtrips
# for f in test/test-data/20*/*.dwg; do ./dxf-roundtrip.sh $f; done
case `uname` in
    Darwin)  app=/Applications/TeighaFileConverter.app/Contents/MacOS/TeighaFileConverter ;;
    Linux)   app=/usr/bin/TeighaFileConverter ;;
    Windows) app=TeighaFileConverter ;;
esac
#full=test/test-data/2000/$f.dwg
full=$1
if [ ! -f $full ]; then
    echo $full not found
    exit
fi
f=`basename $full .dwg`
r=`basename $(dirname $full)`
case $r in
    20*)      ;;
    r9)       r=9 ;;
    r1[0234]) r=${r:1} ;;
    *)        echo wrong version $r; exit ;;
esac

echo programs/dwg2dxf -v4 -o ${f}_${r}.dxf $full
programs/dwg2dxf -v4 -o ${f}_${r}.dxf $full 2>${f}_${r}.log || \
    grep Error ${f}_${r}.log

mv ${f}_${r}.dxf test/
echo `basename $app` "test" . ACAD$r DWG 0 1 ${f}_${r}.dxf
$app "test" "." ACAD$r DWG 0 1 ${f}_${r}.dxf
mv test/${f}_${r}.dxf ./

if [ -f ${f}_${r}.dwg ]; then
    echo test ${f}_${r}.dwg created from ${f}_${r}.dxf
    programs/dwgread -v4 ${f}_${r}.dwg 2>${f}_${r}_dxf.log || \
        grep Error ${f}_${r}_dxf.log
else
    cat ${f}_${r}.dwg.err
fi
