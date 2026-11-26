#!/bin/bash
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# OmniOperator is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan
# PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
# KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
#
# build file for OmniOperatorJit

set -e

# print_usage
print_usage() {
  echo "
  Usage:
    build.sh [type]

  Types:
    [default]                          = Enable Default Options (equivalent to Release)
    package                            = Enable Package
    release                            = Enable Release
    test                               = Enable Build and Test
    coverage-java                      = Enable Enable coverage for Java
    coverage-c++                       = Enable Enable coverage for C++
  "
}

# if JAVA_HOME is not set,
# prompt to set to detected location before exiting
check_java_home() {
  if [ -z "$JAVA_HOME" ]; then
    local java=$(which java|xargs readlink -f)

    echo "ERROR: JAVA_HOME is not set!"
    echo "If it's ${java%/bin/java},"
    echo "you can set it as follows:"
    echo "export JAVA_HOME=${java%/bin/java}"
    echo ""
    echo "Please set JAVA_HOME and try again"
    exit 1
  fi
}

# if OMNI_HOME is not set,
# prompt to set to suggested location before exiting
check_omni_home() {
  if [ -z "$OMNI_HOME" ]; then
    echo "ERROR: OMNI_HOME is not set!"
    echo "You can set it as follows:"
    echo "for system level configuration (requiring root privilege),"
    echo "export OMNI_HOME=/opt"
    echo "or for user level configuration,"
    echo "export OMNI_HOME=$HOME/opt"
    echo ""
    echo "Please set OMNI_HOME and try again"
    exit 1
  fi
}

setup_build() {
  echo "OMNI_HOME = $OMNI_HOME"
  [ ! -d "$OMNI_HOME/lib" ] && mkdir -p $OMNI_HOME/lib

  echo "LIB_HOME = $OMNI_HOME/lib, LD_LIBRARY_PATH = $LD_LIBRARY_PATH"
  rm -rf  $OMNI_HOME/lib/libboostkit*.so $OMNI_HOME/*-binding
}

# the current solution is to build from source manually
# the future plan is to use conan to manage all the dependencies
setup_dependencies() {
  local open_source_dir="open_source"
  local workspace=$(pwd)
  mkdir -p ${workspace}/${open_source_dir}

  cp -r ${workspace}/../libboundscheck ${open_source_dir}
  cp -r ${workspace}/../json ${open_source_dir}
  if [ "$1" != "package" ] && [ "$1" != "release" ]; then
    cp -r ${workspace}/../benchmark ${open_source_dir}
    cp -r ${workspace}/../googletest ${open_source_dir}/benchmark
  fi

  echo "Start build open source code for libboundscheck, json and gtest"
  cd ${workspace}/${open_source_dir}/libboundscheck
  sudo make CC=gcc
  cd ${workspace}/${open_source_dir}
  sudo cp libboundscheck/lib/libboundscheck.so $OMNI_HOME/lib
  sudo cp -r libboundscheck/include/ $OMNI_HOME/lib

  mkdir ${workspace}/${open_source_dir}/json/build
  cd ${workspace}/${open_source_dir}/json/build && sudo cmake ../ && sudo make -j16 && sudo make install

  if [ "$1" != "package" ] && [ "$1" != "release" ]; then
    cd ${workspace}/${open_source_dir}/benchmark
    cmake -E make_directory "build"
    cmake -E chdir "build" cmake -DCMAKE_BUILD_TYPE=Release ../
    sudo cmake --build "build" --config Release --target install
  fi
}

# package build_script/build.sh functionality here
build() {
  local workspace=$(pwd)
  export OMNI_COMPILER_THREAD_COUNT=8
  sh $workspace/build_scripts/build.sh "$@"
  cd $workspace
}
