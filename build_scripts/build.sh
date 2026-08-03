#!/bin/bash
# build file for OmniOperatorJit
# Copyright (c) Huawei Technologies Co., Ltd. 2021-2023. All rights reserved.

set -e

source $(cd $(dirname ${BASH_SOURCE[0]}) && pwd)/env_check.sh

# if either help or --help is provided, the usage should be printed prior to exit
if [ "$1" = 'help' ] || [ "$1" = '--help' ]; then
  print_help
  exit 0
fi

# run the checks to ensure the prerequisites are ready
check_set_prerequisites

# init variables
BINDING_TARGET_EXPR='boostkit-omniop-\1-binding-2.2.0-aarch64'
CWD=$(pwd)
OPTIONS=""
TARGETS="--target all"

if [ $# = 0 ]; then
  # if no params are passed, default to Release build
  echo "-- Enable Release"
  OPTIONS="-DCMAKE_BUILD_TYPE=Release"
else
  # $1 has build type and targets to built
  target_build_type=$1
  build_type=${target_build_type%%:*}
  build_targets=$(test $(echo $target_build_type | grep ':' | wc -l) != 0  && echo "${target_build_type#*:}" || echo "")

  if [ ! -z "$build_targets" ]; then
      TARGETS+=" $(echo ":$build_targets" | sed "s@:\([^\:]\+\)@ --target ${BINDING_TARGET_EXPR} @g" )"
  fi

  if [ "$build_type" = 'debug' ]; then
      echo "-- Enable Debug"
      OPTIONS+=" -DCMAKE_BUILD_TYPE=Debug -DDEBUG=ON"
  elif [ "$build_type" = 'trace' ]; then
      echo "-- Enable Trace"
      OPTIONS+=" -DCMAKE_BUILD_TYPE=Debug -DTRACE=ON"
  elif [ "$build_type" = 'coverage' ]; then
      echo "-- Enable Coverage"
      OPTIONS+=" -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=ON"
  elif [ "$build_type" = 'release' ]; then
      echo "-- Enable Release"
      OPTIONS+=" -DCMAKE_BUILD_TYPE=Release"
  else
      exit_with_message_and_print_help "ERROR: Invalid type: $build_type"
  fi

  # $2 has build options
  if [ "$2" = "all" ]; then
    echo "-- Enable All Module Debug, Include: OPERATOR,VECTOR,LLVM"
    OPTIONS+=" -DDEBUG_OPERATOR=ON -DDEBUG_VECTOR=ON -DDEBUG_LLVM=ON"
  else
    for i in ${*:2} ; do
      if [ "$i" == 'op' ] || [ "$i" == '--enable-operator-debug' ]; then
          echo "-- Enable Operator Debug"
          OPTIONS+=" -DDEBUG_OPERATOR=ON"
      elif [ "$i" == 'vec' ] || [ "$i" == '--enable-vector-debug' ]; then
          echo "-- Enable Vector Debug"
          OPTIONS+=" -DDEBUG_VECTOR=ON"
      elif [ "$i" == 'llvm' ] || [ "$i" == '--enable-llvm-debug' ]; then
          echo "-- Enable LLVM Debug"
          OPTIONS+=" -DDEBUG_LLVM=ON"
      elif [ "$i" == '--enable-dt' ]; then
          [ "$build_type" != 'coverage' ] &&  exit_with_message "-- Please use coverage with --enable-dt"
          echo "-- Enable DT checker"
          OPTIONS+=" -DENABLE_DT=ON -DCOVERAGE=ON"
      elif [ "$i" == '--enable-benchmark' ]; then
          echo "-- Enable benchmark"
          OPTIONS+=" -DENABLE_BENCHMARK=ON"
      elif [ "$i" == '--enable-compile-time-report' ]; then
          echo " --Enable Compile Time Report"
          OPTIONS+=" -DENABLE_COMPILE_TIME_REPORT=ON"
      elif [ "$i" == '--exclude-test' ]; then
          echo "-- Exclude Test Source"
          OPTIONS+=" -DEXCLUDE_TEST=ON"
      elif [ "$i" == '--exclude-duplicate-codegen' ]; then
          echo "-- Exclude Duplicate Codegen Functions"
          OPTIONS+=" -DEXCLUDE_DUPLICATE_CODEGEN_FUNCTIONS=ON"
      else
          exit_with_message_and_print_help "ERROR: Invalid option: $i"
      fi
    done
  fi
fi
echo "-- Exclude Batch Functions"
OPTIONS+=" -DEXCLUDE_BATCH_FUNCTIONS=ON"
print_gcc_lib

# need to delete the CMakeCache.txt to refresh the options
rm -rf $CWD/build/CMakeCache.txt && cmake -S $(cd $(dirname ${BASH_SOURCE[0]})/.. && pwd)  -B $CWD/build $OPTIONS -G Ninja -DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld" -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld"
# use all available cpu cores to speed up build process
cmake --build $CWD/build --clean-first $TARGETS -j $(test -z "${OMNI_COMPILER_THREAD_COUNT}" && echo $(nproc) || echo ${OMNI_COMPILER_THREAD_COUNT})
# install requires root privilege
cmake --install $CWD/build
