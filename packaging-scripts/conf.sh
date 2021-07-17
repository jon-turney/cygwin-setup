#!/bin/bash -e

TARGET=${1:-upx}
ARCHES=${2:-i686 x86_64}

function arch() {
cpu=$1
rm -rf $cpu
mkdir $cpu
cd $cpu

NOCONFIGURE=1 ../bootstrap.sh
../configure --host=$cpu-w64-mingw32 --target=$cpu-w64-mingw32

make ${TARGET}

cd ..
}

for ARCH in ${ARCHES} ; do
    arch ${ARCH}
done
