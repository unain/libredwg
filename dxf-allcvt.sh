#!/bin/sh
make -s
make -s -j4 -C src && \
make -s -j4 -C programs dwg2dxf && \
  programs/dwg2dxf -m -o Drawing_2000_min.dxf test/test-data/Drawing_2000.dwg

for f in test/test-data/Drawing_2*.dwg \
         test/test-data/sample_2*.dwg \
         test/test-data/example_*.dwg
do
    echo
    echo programs/dwg2dxf -v0 $f
    programs/dwg2dxf -v0 $f
done

for d in r14 2000 2004 2007 2010 2013 2018; do
    for f in test/test-data/$d/*.dwg; do
        b=`basename $f .dwg`
        echo
        echo programs/dwg2dxf -v0 -o ${b}_${d}.dxf $f
        programs/dwg2dxf -v0 -o ${b}_${d}.dxf $f
    done
done
