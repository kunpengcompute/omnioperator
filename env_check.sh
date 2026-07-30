#!/bin/bash
# build file for OmniOperatorJit
# Copyright (c) Huawei Technologies Co., Ltd. 2021-2022. All rights reserved.

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

install_rapidjson() {
  echo "Start build rapidjson"
  local rapidjson_tag="v1.1.0"
  local rapidjson_repo="https://gitee.com/Tencent/RapidJSON.git"
  local rapidjson_source_dir="${workspace}/${open_source_dir}/rapidjson"
  local rapidjson_default_home="/usr/local/"

  # Check if RAPIDJSON_HOME exists, skip build if true
  if [ -n "$RAPIDJSON_HOME" ] && [ -d "$RAPIDJSON_HOME" ]; then
    echo "RAPIDJSON_HOME=$RAPIDJSON_HOME exists, skip rapidjson build process."
  else
    echo "Start to clone rapidjson-${rapidjson_tag} source code and build..."
    rm -rf ${rapidjson_source_dir} && mkdir -p ${rapidjson_source_dir}
    git clone --branch ${rapidjson_tag} --depth=1 ${rapidjson_repo} ${rapidjson_source_dir}
    cd ${rapidjson_source_dir}
    sudo cp -r include ${rapidjson_default_home}
    echo "rapidjson-${rapidjson_tag} build and install completed successfully."
    export RAPIDJSON_HOME=${rapidjson_default_home}
    echo "Set RAPIDJSON_HOME=$RAPIDJSON_HOME automatically after rapidjson install."
    cd ${workspace}
  fi
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

  echo "Start build fmt.so"
  local fmt_tag="10.1.1"
  local fmt_repo="https://gitee.com/mirrors/fmt.git"
  local fmt_source_dir="${workspace}/${open_source_dir}/fmt"
  local fmt_default_home="/usr/local"

  if [ -n "$FMT_HOME" ] && [ -d "$FMT_HOME" ]; then
    echo "FMT_HOME=$FMT_HOME exists, skip fmt build process."
  else
    echo "FMT_HOME not set, start to clone fmt-${fmt_tag} source code and build..."
    rm -rf ${fmt_source_dir} && mkdir -p ${fmt_source_dir}
    git clone --branch ${fmt_tag} --depth=1 ${fmt_repo} ${fmt_source_dir}
    cd ${fmt_source_dir}
    mkdir -p build && cd build
    cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DFMT_TEST=OFF \
    -DFMT_DOC=OFF \
    -DFMT_INSTALL=ON \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld" \
    -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" \
    -G Ninja
    cmake --build . --parallel $(nproc)
    sudo cmake --install .
    export FMT_HOME=${fmt_default_home}
    echo "Set FMT_HOME=$FMT_HOME automatically after fmt install."
    cd ${workspace}
  fi

  echo "Start build folly"
  local folly_tag="v2024.07.01.00"
  local folly_repo="https://gitee.com/mirrors/folly.git"
  local folly_source_dir="${workspace}/${open_source_dir}/folly"
  local folly_default_home="/usr/local"

  if [ -n "$FOLLY_HOME" ] && [ -d "$FOLLY_HOME" ]; then
    echo "FOLLY_HOME=$FOLLY_HOME exists, skip folly build process."
  else
    echo "FOLLY_HOME not set, start to clone folly-${folly_tag} source code and build..."
    rm -rf ${folly_source_dir} && mkdir -p ${folly_source_dir}
    git clone --branch ${folly_tag} --depth=1 ${folly_repo} ${folly_source_dir}
    cd ${folly_source_dir}
    mkdir -p build && cd build
    cmake .. -DBUILD_TESTS=OFF -DFOLLY_HAVE_INT128_T=ON \
      -DBUILD_SHARED_LIBS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld" \
      -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" \
      -G Ninja
 	  cmake --build . --parallel $(nproc)
 	  sudo cmake --install .
    export FOLLY_HOME=${folly_default_home}
    echo "Set FOLLY_HOME=$FOLLY_HOME automatically after folly install."
    cd ${workspace}
  fi

  install_rapidjson

  echo "Start build open source code for libboundscheck, json and gtest"
  cd ${workspace}/${open_source_dir}/libboundscheck
  sudo make CC=gcc
  cd ${workspace}/${open_source_dir}
  sudo cp libboundscheck/lib/libboundscheck.so $OMNI_HOME/lib
  sudo cp -r libboundscheck/include/ $OMNI_HOME/lib

  mkdir ${workspace}/${open_source_dir}/json/build
  cd ${workspace}/${open_source_dir}/json/build && sudo cmake ../ -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld" -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" -G Ninja && sudo cmake --build . --parallel $(nproc) && sudo cmake --install .

  if [ "$1" != "package" ] && [ "$1" != "release" ]; then
    cd ${workspace}/${open_source_dir}/benchmark
    cmake -E make_directory "build"
    cmake -E chdir "build" cmake -DCMAKE_BUILD_TYPE=Release ../ -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld" -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld" -G Ninja
    sudo cmake --build "build" --config Release --target install
  fi
}

check_ninja() {
  if ! command -v ninja &> /dev/null; then
    echo "ERROR: Ninja is not installed!"
    echo "Ninja is required for optimized build performance."
    echo "Please install Ninja and try again."
    exit 1
  else
    echo "Ninja version: $(ninja --version)"
  fi
  # 检查LLD（兼容ld.lld/lld两种命令）
  if ! command -v ld.lld &> /dev/null && ! command -v lld &> /dev/null; then
    echo "WARN: LLD linker is not installed!"
  fi

  # 检查ccache
  if ! command -v ccache &> /dev/null; then
    echo "WARN: ccache is not installed!"
  fi
}

# package build_script/build.sh functionality here
build() {
  local workspace=$(pwd)
  export OMNI_COMPILER_THREAD_COUNT=${OMNI_COMPILER_THREAD_COUNT:-16}
  echo "OMNI_COMPILER_THREAD_COUNT=${OMNI_COMPILER_THREAD_COUNT}"
  sh $workspace/build_scripts/build.sh "$@"
  cd $workspace
}