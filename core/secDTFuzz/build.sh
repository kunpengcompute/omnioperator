#!/bin/bash
# build file for OmniOperatorJit
# Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.

set -e
set -x
echo "build.sh+"

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
SRC_ROOT="$(cd $SCRIPT_DIR/../..;pwd)"

echo ${SRC_ROOT}
mkdir ${SRC_ROOT}/tools
cp -rf /SecDTFuzz/DTFrame ${SRC_ROOT}/tools/

cd ${SRC_ROOT}/tools/DTFrame
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_PRODUCT=product .. -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld" -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" -G Ninja && cmake --build . --parallel $(nproc) && cmake --install .
cp -r ${SRC_ROOT}/tools/DTFrame/dist ${SRC_ROOT}/core/test/dt/testtree

cd ${SRC_ROOT}
mkdir omni-operator
cp -r /usr1/libboundscheck/lib omni-operator
cp -r /usr1/libboundscheck/include omni-operator/lib

dos2unix core/build/build.sh
bash core/build/build.sh coverage --enable-dt

cp -r ${SRC_ROOT}/core/test/dt/testtree/dist/ /out
cp -r ${SRC_ROOT}/core/test/dt/testtree/cases/ /out
echo "build.sh-"